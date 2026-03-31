// ─── dithr editor ─────────────────────────────────────────────────────────────
//
// A minimal text editor built on the dithr fantasy console engine.
// Framebuffer: 640×360, built-in 4×6 font (5×7 advance with spacing).
//
// Shortcuts:
//   Ctrl+O   Open file browser       Ctrl+S   Save file
//   Ctrl+Z   Undo                    Ctrl+A   Select all
//   Ctrl+C   Copy (selection/line)   Ctrl+V   Paste
//   Ctrl+`   Toggle vim mode         Escape   Clear selection
//
// Vim mode (toggle with Ctrl+`):
//   Normal:  h/j/k/l  w/b/e  0/$/^  gg/G  x  dd/yy/cc  d/c/y+motion
//            i/I/a/A/o/O  p/P  u  J  D/C  v/V  :  /pattern  n/N  r
//   Visual:  motions extend selection, d/c/y/x operate
//   Command: :w :q :wq :e :<line>
//
// ──────────────────────────────────────────────────────────────────────────────

// ─── Layout ──────────────────────────────────────────────────────────────────

const CW = 5; // char advance (4px glyph + 1px)
const CH = 7; // line advance (6px glyph + 1px)
const FB_W = 640,
    FB_H = 360;
const COLS = (FB_W / CW) | 0; // 128
const ROWS = (FB_H / CH) | 0; // 51

const GUTTER = 5; // chars for line numbers
const HEAD = 1; // header rows
const FOOT = 1; // status-bar rows
const EROWS = ROWS - HEAD - FOOT; // 49
const ECOLS = COLS - GUTTER; // 123

const TAB = "  "; // soft tab (2 spaces)

// ─── Palette indices ─────────────────────────────────────────────────────────

const BG = 0; // black
const FG = 7; // white (near-white #FFF1E8)
const GUTBG = 1; // dark blue
const GUTFG = 6; // light gray
const HEADBG = 1;
const HEADFG = 7;
const FOOTBG = 1;
const FOOTFG = 6;
const CURFG = 7; // cursor colour
const LINEBG = 17; // #222222 — subtle current-line highlight
const SELBG = 2; // dark purple — selection
const KWCOL = 12; // cyan — keywords
const STRCOL = 11; // green — strings
const COMCOL = 5; // dark gray — comments
const NUMCOL = 9; // orange — numbers
const DIRTCOL = 8; // red — unsaved indicator

// ─── State ───────────────────────────────────────────────────────────────────

let buf = [""]; // line buffer
let cx = 0,
    cy = 0; // cursor col / row in buffer
let ox = 0,
    oy = 0; // scroll offset col / row
let anchor = null; // selection anchor {x,y} or null

let blink = 0,
    curOn = true; // cursor blink state
let fname = ""; // current filename
let dirty = false;
let msg = "",
    msgT = 0; // status message + timer
let restored = false; // set by _restore to skip default open in _init

// undo
const MAXUNDO = 200;
let undoStack = [];

// file browser
let brMode = false;
let brEntries = []; // {name, isDir}
let brIdx = 0;
let brScroll = 0;
let brDir = ""; // current directory relative to cart root ("" = root, "src/" etc.)

// vim mode
let vimEnabled = false;
let vim = "normal"; // "normal", "insert", "visual", "vline", "command"
let vimCount = "";
let vimPending = "";
let vimOpCount = 1;
let vimCmd = "";
let vimReg = "";
let vimLinewise = false;
let vimSearch = "";
let vimVlineStart = 0;

// JS keywords for syntax highlighting
const KEYWORDS = new Set([
    "break",
    "case",
    "catch",
    "class",
    "const",
    "continue",
    "debugger",
    "default",
    "delete",
    "do",
    "else",
    "export",
    "extends",
    "false",
    "finally",
    "for",
    "function",
    "if",
    "import",
    "in",
    "instanceof",
    "let",
    "new",
    "null",
    "of",
    "return",
    "super",
    "switch",
    "this",
    "throw",
    "true",
    "try",
    "typeof",
    "undefined",
    "var",
    "void",
    "while",
    "with",
    "yield",
]);

// ─── Helpers ─────────────────────────────────────────────────────────────────

function clamp(v, lo, hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

function status(text, sec) {
    msg = text;
    msgT = sec || 3;
}

function resetBlink() {
    blink = 0;
    curOn = true;
}

function ensureVisible() {
    if (cy < oy) oy = cy;
    if (cy >= oy + EROWS) oy = cy - EROWS + 1;
    if (cx < ox) ox = cx;
    if (cx >= ox + ECOLS) ox = cx - ECOLS + 1;
}

// ─── Vim helpers ─────────────────────────────────────────────────────────────

function isWordChar(c) {
    return (
        (c >= "a" && c <= "z") ||
        (c >= "A" && c <= "Z") ||
        (c >= "0" && c <= "9") ||
        c === "_" ||
        c === "$"
    );
}

function firstNonBlank(line) {
    let s = buf[line];
    let i = 0;
    while (i < s.length && (s[i] === " " || s[i] === "\t")) i++;
    return i;
}

function getIndent(line) {
    let m = buf[line].match(/^(\s*)/);
    return m ? m[1] : "";
}

function vimWordForward() {
    let line = buf[cy];
    if (cx >= line.length) {
        if (cy < buf.length - 1) {
            cy++;
            cx = 0;
            line = buf[cy];
        }
        while (cx < line.length && line[cx] === " ") cx++;
        return;
    }
    if (isWordChar(line[cx])) {
        while (cx < line.length && isWordChar(line[cx])) cx++;
    } else if (line[cx] !== " ") {
        while (cx < line.length && !isWordChar(line[cx]) && line[cx] !== " ") cx++;
    }
    while (cx < line.length && line[cx] === " ") cx++;
    if (cx >= line.length && cy < buf.length - 1) {
        cy++;
        cx = 0;
        line = buf[cy];
        while (cx < line.length && line[cx] === " ") cx++;
    }
}

function vimWordBack() {
    if (cx === 0) {
        if (cy > 0) {
            cy--;
            cx = buf[cy].length;
        }
        return;
    }
    let line = buf[cy];
    cx--;
    while (cx > 0 && line[cx] === " ") cx--;
    if (isWordChar(line[cx])) {
        while (cx > 0 && isWordChar(line[cx - 1])) cx--;
    } else {
        while (cx > 0 && !isWordChar(line[cx - 1]) && line[cx - 1] !== " ") cx--;
    }
}

function vimWordEnd() {
    let line = buf[cy];
    if (cx >= line.length - 1) {
        if (cy < buf.length - 1) {
            cy++;
            cx = 0;
            line = buf[cy];
            while (cx < line.length && line[cx] === " ") cx++;
        }
    } else {
        cx++;
    }
    line = buf[cy];
    while (cx < line.length && line[cx] === " ") cx++;
    if (isWordChar(line[cx])) {
        while (cx < line.length - 1 && isWordChar(line[cx + 1])) cx++;
    } else {
        while (cx < line.length - 1 && !isWordChar(line[cx + 1]) && line[cx + 1] !== " ") cx++;
    }
}

// ─── Selection helpers ───────────────────────────────────────────────────────

function selOrdered() {
    if (!anchor) return null;
    let a = { x: anchor.x, y: anchor.y };
    let b = { x: cx, y: cy };
    if (a.y > b.y || (a.y === b.y && a.x > b.x)) {
        let t = a;
        a = b;
        b = t;
    }
    return { a, b };
}

function selText() {
    let s = selOrdered();
    if (!s) return "";
    if (s.a.y === s.b.y) return buf[s.a.y].slice(s.a.x, s.b.x);
    let parts = [buf[s.a.y].slice(s.a.x)];
    for (let i = s.a.y + 1; i < s.b.y; i++) parts.push(buf[i]);
    parts.push(buf[s.b.y].slice(0, s.b.x));
    return parts.join("\n");
}

function deleteSel() {
    let s = selOrdered();
    if (!s) return false;
    pushUndo();
    if (s.a.y === s.b.y) {
        buf[s.a.y] = buf[s.a.y].slice(0, s.a.x) + buf[s.a.y].slice(s.b.x);
    } else {
        buf[s.a.y] = buf[s.a.y].slice(0, s.a.x) + buf[s.b.y].slice(s.b.x);
        buf.splice(s.a.y + 1, s.b.y - s.a.y);
    }
    cx = s.a.x;
    cy = s.a.y;
    anchor = null;
    dirty = true;
    return true;
}

// ─── Undo ────────────────────────────────────────────────────────────────────

function snapshot() {
    return { buf: buf.map((l) => l), cx, cy };
}

function pushUndo() {
    undoStack.push(snapshot());
    if (undoStack.length > MAXUNDO) undoStack.shift();
}

function doUndo() {
    if (!undoStack.length) {
        status("Nothing to undo");
        return;
    }
    let s = undoStack.pop();
    buf = s.buf;
    cx = s.cx;
    cy = s.cy;
    anchor = null;
    dirty = true;
    ensureVisible();
    resetBlink();
}

// ─── File I/O ────────────────────────────────────────────────────────────────

function openFile(path) {
    let data = sys.readFile(path);
    if (data === undefined) {
        status("Cannot read: " + path);
        return;
    }
    buf = data.replace(/\r\n/g, "\n").split("\n");
    if (!buf.length) buf = [""];
    fname = path;
    cx = cy = ox = oy = 0;
    anchor = null;
    dirty = false;
    undoStack = [];
    status("Opened " + path);
}

function saveFile() {
    if (!fname) {
        status("No file — open one first (Ctrl+O)");
        return;
    }
    let ok = sys.writeFile(fname, buf.join("\n"));
    if (ok) {
        dirty = false;
        status("Saved " + fname);
    } else {
        status("Write failed: " + fname);
    }
}

function openBrowser() {
    brDir = "";
    refreshBrowser();
    brMode = true;
}

function refreshBrowser() {
    let dirs = sys.listDirs(brDir);
    let files = sys.listFiles(brDir);
    brEntries = [];
    if (brDir) {
        brEntries.push({ name: "..", isDir: true });
    }
    if (dirs) {
        for (let i = 0; i < dirs.length; i++) {
            brEntries.push({ name: dirs[i], isDir: true });
        }
    }
    if (files) {
        for (let i = 0; i < files.length; i++) {
            brEntries.push({ name: files[i], isDir: false });
        }
    }
    brIdx = 0;
    brScroll = 0;
}

// ─── Syntax highlighting ─────────────────────────────────────────────────────
// Returns array of {text, col} tokens for one source line.

function tokenize(line) {
    let toks = [];
    let i = 0,
        n = line.length;

    while (i < n) {
        // Single-line comment
        if (line[i] === "/" && line[i + 1] === "/") {
            toks.push({ text: line.slice(i), col: COMCOL });
            return toks;
        }

        // String literal
        if (line[i] === '"' || line[i] === "'" || line[i] === "`") {
            let q = line[i],
                j = i + 1;
            while (j < n && line[j] !== q) {
                if (line[j] === "\\") j++;
                j++;
            }
            if (j < n) j++;
            toks.push({ text: line.slice(i, j), col: STRCOL });
            i = j;
            continue;
        }

        // Number
        if (line[i] >= "0" && line[i] <= "9") {
            let j = i;
            while (
                j < n &&
                ((line[j] >= "0" && line[j] <= "9") ||
                    line[j] === "." ||
                    line[j] === "x" ||
                    line[j] === "X" ||
                    (line[j] >= "a" && line[j] <= "f") ||
                    (line[j] >= "A" && line[j] <= "F"))
            )
                j++;
            toks.push({ text: line.slice(i, j), col: NUMCOL });
            i = j;
            continue;
        }

        // Identifier / keyword
        let ch = line[i];
        if ((ch >= "a" && ch <= "z") || (ch >= "A" && ch <= "Z") || ch === "_" || ch === "$") {
            let j = i + 1;
            while (j < n) {
                let c = line[j];
                if (
                    (c >= "a" && c <= "z") ||
                    (c >= "A" && c <= "Z") ||
                    (c >= "0" && c <= "9") ||
                    c === "_" ||
                    c === "$"
                )
                    j++;
                else break;
            }
            let word = line.slice(i, j);
            toks.push({ text: word, col: KEYWORDS.has(word) ? KWCOL : FG });
            i = j;
            continue;
        }

        // Single character (whitespace, operator, punctuation)
        toks.push({ text: line[i], col: FG });
        i++;
    }
    return toks;
}

// ─── Text input event handler ────────────────────────────────────────────────

function onTextInput(ch) {
    if (brMode) return;
    if (key.btn(key.LCTRL) || key.btn(key.RCTRL)) return;

    if (vimEnabled && vim !== "insert") {
        if (vim === "command") {
            vimCmd += ch;
        } else {
            vimNormal(ch);
        }
        return;
    }

    deleteSel();
    pushUndo();
    buf[cy] = buf[cy].slice(0, cx) + ch + buf[cy].slice(cx);
    cx += ch.length;
    dirty = true;
    ensureVisible();
    resetBlink();
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

function _init() {
    sys.textInput(true);
    evt.on("text:input", onTextInput);
    if (!restored) {
        openFile("src/main.js");
    }
    restored = false;
}

function _save() {
    return { buf, cx, cy, ox, oy, fname, dirty, undoStack, anchor, brDir, vimEnabled };
}

function _restore(s) {
    buf = s.buf;
    cx = s.cx;
    cy = s.cy;
    ox = s.ox;
    oy = s.oy;
    fname = s.fname;
    dirty = s.dirty;
    undoStack = s.undoStack || [];
    anchor = s.anchor || null;
    brDir = s.brDir || "";
    vimEnabled = s.vimEnabled || false;
    vim = "normal";
    vimCount = "";
    vimPending = "";
    vimCmd = "";
    restored = true;
}

function _update(dt) {
    // Cursor blink
    blink += dt;
    if (blink >= 0.5) {
        blink -= 0.5;
        curOn = !curOn;
    }

    // Status decay
    if (msgT > 0) {
        msgT -= dt;
        if (msgT <= 0) msg = "";
    }

    if (brMode) updateBrowser();
    else updateEdit();
}

function _draw() {
    gfx.cls(BG);
    if (brMode) drawBrowser();
    else drawEditor();
}

// ─── Edit-mode update ────────────────────────────────────────────────────────

function updateEdit() {
    let ctrl = key.btn(key.LCTRL) || key.btn(key.RCTRL);
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    // ── Ctrl shortcuts ──
    if (ctrl) {
        if (key.btnp(key.GRAVE)) {
            vimEnabled = !vimEnabled;
            if (vimEnabled) {
                vim = "normal";
                vimCount = "";
                vimPending = "";
                anchor = null;
                status("Vim ON");
            } else {
                status("Vim OFF");
            }
            return;
        }
        if (key.btnp(key.S)) {
            saveFile();
            return;
        }
        if (key.btnp(key.O)) {
            openBrowser();
            return;
        }
        if (key.btnp(key.Z)) {
            doUndo();
            return;
        }
        if (key.btnp(key.A)) {
            anchor = { x: 0, y: 0 };
            cy = buf.length - 1;
            cx = buf[cy].length;
            return;
        }
        if (key.btnp(key.C)) {
            let t = selText();
            if (t) sys.clipboardSet(t);
            else sys.clipboardSet(buf[cy]);
            status("Copied");
            return;
        }
        if (key.btnp(key.V)) {
            let t = sys.clipboardGet();
            if (!t) return;
            deleteSel();
            pushUndo();
            let parts = t.replace(/\r\n/g, "\n").split("\n");
            if (parts.length === 1) {
                buf[cy] = buf[cy].slice(0, cx) + t + buf[cy].slice(cx);
                cx += t.length;
            } else {
                let after = buf[cy].slice(cx);
                buf[cy] = buf[cy].slice(0, cx) + parts[0];
                for (let i = 1; i < parts.length - 1; i++) {
                    buf.splice(cy + i, 0, parts[i]);
                }
                buf.splice(cy + parts.length - 1, 0, parts[parts.length - 1] + after);
                cy += parts.length - 1;
                cx = parts[parts.length - 1].length;
            }
            dirty = true;
            ensureVisible();
            resetBlink();
            return;
        }
        return;
    }

    // ── Vim non-insert modes ──
    if (vimEnabled && vim !== "insert") {
        updateVimKeys();
        return;
    }

    // ── Vim insert: Escape returns to normal ──
    if (vimEnabled && key.btnp(key.ESCAPE)) {
        vim = "normal";
        cx = Math.max(0, cx - 1);
        anchor = null;
        ensureVisible();
        resetBlink();
        return;
    }

    // ── Navigation (with key repeat) ──
    let pcx = cx,
        pcy = cy;

    if (key.btnr(key.LEFT)) {
        if (cx > 0) cx--;
        else if (cy > 0) {
            cy--;
            cx = buf[cy].length;
        }
    }
    if (key.btnr(key.RIGHT)) {
        if (cx < buf[cy].length) cx++;
        else if (cy < buf.length - 1) {
            cy++;
            cx = 0;
        }
    }
    if (key.btnr(key.UP) && cy > 0) {
        cy--;
        cx = clamp(cx, 0, buf[cy].length);
    }
    if (key.btnr(key.DOWN) && cy < buf.length - 1) {
        cy++;
        cx = clamp(cx, 0, buf[cy].length);
    }
    if (key.btnr(key.HOME)) cx = 0;
    if (key.btnr(key.END)) cx = buf[cy].length;
    if (key.btnr(key.PAGEUP)) {
        cy = Math.max(0, cy - EROWS);
        cx = clamp(cx, 0, buf[cy].length);
    }
    if (key.btnr(key.PAGEDOWN)) {
        cy = Math.min(buf.length - 1, cy + EROWS);
        cx = clamp(cx, 0, buf[cy].length);
    }

    // Update selection anchor if shifted, clear if not
    if (cx !== pcx || cy !== pcy) {
        if (shift && !anchor) anchor = { x: pcx, y: pcy };
        if (!shift) anchor = null;
        ensureVisible();
        resetBlink();
    }

    // ── Editing keys ──
    if (key.btnr(key.BACKSPACE)) {
        if (deleteSel()) {
            ensureVisible();
            resetBlink();
            return;
        }
        pushUndo();
        if (cx > 0) {
            buf[cy] = buf[cy].slice(0, cx - 1) + buf[cy].slice(cx);
            cx--;
        } else if (cy > 0) {
            cx = buf[cy - 1].length;
            buf[cy - 1] += buf[cy];
            buf.splice(cy, 1);
            cy--;
        }
        dirty = true;
        ensureVisible();
        resetBlink();
    }

    if (key.btnr(key.DELETE)) {
        if (deleteSel()) {
            ensureVisible();
            resetBlink();
            return;
        }
        pushUndo();
        if (cx < buf[cy].length) {
            buf[cy] = buf[cy].slice(0, cx) + buf[cy].slice(cx + 1);
        } else if (cy < buf.length - 1) {
            buf[cy] += buf[cy + 1];
            buf.splice(cy + 1, 1);
        }
        dirty = true;
        resetBlink();
    }

    if (key.btnp(key.ENTER)) {
        deleteSel();
        pushUndo();
        // Auto-indent: match leading whitespace of current line
        let indent = "";
        let m = buf[cy].match(/^(\s+)/);
        if (m) indent = m[1];
        let after = buf[cy].slice(cx);
        buf[cy] = buf[cy].slice(0, cx);
        cy++;
        buf.splice(cy, 0, indent + after);
        cx = indent.length;
        dirty = true;
        ensureVisible();
        resetBlink();
    }

    if (key.btnr(key.TAB)) {
        deleteSel();
        pushUndo();
        buf[cy] = buf[cy].slice(0, cx) + TAB + buf[cy].slice(cx);
        cx += TAB.length;
        dirty = true;
        ensureVisible();
        resetBlink();
    }

    if (key.btnp(key.ESCAPE)) {
        anchor = null;
    }

    // ── Mouse ──
    let mx = mouse.x();
    let my = mouse.y();
    let editY = HEAD * CH;
    let footY = (ROWS - FOOT) * CH;
    let gutterPx = GUTTER * CW;

    if (my >= editY && my < footY && mx >= gutterPx) {
        if (mouse.btnp(0)) {
            // Click to place cursor
            let row = (oy + (my - editY) / CH) | 0;
            let col = (ox + (mx - gutterPx) / CW) | 0;
            row = clamp(row, 0, buf.length - 1);
            col = clamp(col, 0, buf[row].length);
            if (shift) {
                if (!anchor) anchor = { x: cx, y: cy };
            } else {
                anchor = null;
            }
            cx = col;
            cy = row;
            ensureVisible();
            resetBlink();
        } else if (mouse.btn(0)) {
            // Drag to extend selection
            let row = (oy + (my - editY) / CH) | 0;
            let col = (ox + (mx - gutterPx) / CW) | 0;
            row = clamp(row, 0, buf.length - 1);
            col = clamp(col, 0, buf[row].length);
            if (!anchor) anchor = { x: cx, y: cy };
            cx = col;
            cy = row;
            ensureVisible();
            resetBlink();
        }
    }

    // Mouse wheel scroll
    let wheel = mouse.wheel();
    if (wheel !== 0) {
        oy = clamp(oy - wheel * 3, 0, Math.max(0, buf.length - EROWS));
    }
}

// ─── Vim mode ────────────────────────────────────────────────────────────────

function updateVimKeys() {
    // Command mode special keys
    if (vim === "command") {
        if (key.btnp(key.ESCAPE)) {
            vim = "normal";
            vimCmd = "";
            return;
        }
        if (key.btnp(key.ENTER)) {
            vimExecCmd(vimCmd);
            vim = "normal";
            vimCmd = "";
            return;
        }
        if (key.btnr(key.BACKSPACE)) {
            if (vimCmd.length > 0) {
                vimCmd = vimCmd.slice(0, -1);
            } else {
                vim = "normal";
            }
            return;
        }
        return;
    }

    // Normal/Visual: Escape
    if (key.btnp(key.ESCAPE)) {
        if (vim === "visual" || vim === "vline") {
            vim = "normal";
            anchor = null;
        }
        vimCount = "";
        vimPending = "";
        return;
    }

    // Mouse handling (works in normal/visual)
    let mx = mouse.x();
    let my = mouse.y();
    let editY = HEAD * CH;
    let footY = (ROWS - FOOT) * CH;
    let gutterPx = GUTTER * CW;

    if (my >= editY && my < footY && mx >= gutterPx) {
        if (mouse.btnp(0)) {
            let row = (oy + (my - editY) / CH) | 0;
            let col = (ox + (mx - gutterPx) / CW) | 0;
            row = clamp(row, 0, buf.length - 1);
            col = clamp(col, 0, Math.max(0, buf[row].length - 1));
            cx = col;
            cy = row;
            if (vim !== "visual" && vim !== "vline") anchor = null;
            ensureVisible();
            resetBlink();
        }
    }

    let wheel = mouse.wheel();
    if (wheel !== 0) {
        oy = clamp(oy - wheel * 3, 0, Math.max(0, buf.length - EROWS));
    }
}

// ── Vim normal mode dispatch ──

function vimNormal(ch) {
    if (vim === "visual" || vim === "vline") {
        vimVisual(ch);
        return;
    }

    // Numeric prefix
    if ((ch >= "1" && ch <= "9") || (ch === "0" && vimCount !== "")) {
        vimCount += ch;
        return;
    }

    let count = vimCount !== "" ? parseInt(vimCount) : 1;
    vimCount = "";

    // Pending operator
    if (vimPending !== "") {
        vimResolvePending(ch, count);
        return;
    }

    switch (ch) {
        // ── Mode switches ──
        case "i":
            vim = "insert";
            return;
        case "I":
            cx = firstNonBlank(cy);
            vim = "insert";
            return;
        case "a":
            cx = Math.min(cx + 1, buf[cy].length);
            vim = "insert";
            return;
        case "A":
            cx = buf[cy].length;
            vim = "insert";
            return;
        case "o": {
            pushUndo();
            let ind = getIndent(cy);
            buf.splice(cy + 1, 0, ind);
            cy++;
            cx = ind.length;
            dirty = true;
            vim = "insert";
            ensureVisible();
            resetBlink();
            return;
        }
        case "O": {
            pushUndo();
            let ind = getIndent(cy);
            buf.splice(cy, 0, ind);
            cx = ind.length;
            dirty = true;
            vim = "insert";
            ensureVisible();
            resetBlink();
            return;
        }
        case "v":
            anchor = { x: cx, y: cy };
            vim = "visual";
            return;
        case "V":
            vimVlineStart = cy;
            anchor = { x: 0, y: cy };
            cx = buf[cy].length;
            vim = "vline";
            return;
        case ":":
            vim = "command";
            vimCmd = "";
            return;
        case "/":
            vim = "command";
            vimCmd = "/";
            return;

        // ── Motions ──
        case "h":
            for (let i = 0; i < count; i++) if (cx > 0) cx--;
            break;
        case "j":
            for (let i = 0; i < count; i++) if (cy < buf.length - 1) cy++;
            cx = clamp(cx, 0, Math.max(0, buf[cy].length - 1));
            break;
        case "k":
            for (let i = 0; i < count; i++) if (cy > 0) cy--;
            cx = clamp(cx, 0, Math.max(0, buf[cy].length - 1));
            break;
        case "l":
            for (let i = 0; i < count; i++) if (cx < buf[cy].length - 1) cx++;
            break;
        case "w":
            for (let i = 0; i < count; i++) vimWordForward();
            break;
        case "b":
            for (let i = 0; i < count; i++) vimWordBack();
            break;
        case "e":
            for (let i = 0; i < count; i++) vimWordEnd();
            break;
        case "0":
            cx = 0;
            break;
        case "$":
            cx = Math.max(0, buf[cy].length - 1);
            break;
        case "^":
            cx = firstNonBlank(cy);
            break;
        case "G":
            if (count > 1) cy = clamp(count - 1, 0, buf.length - 1);
            else cy = buf.length - 1;
            cx = firstNonBlank(cy);
            break;
        case "g":
            vimPending = "g";
            vimOpCount = count;
            return;

        // ── Editing ──
        case "x":
            pushUndo();
            for (let i = 0; i < count; i++) {
                if (cx < buf[cy].length) {
                    vimReg = buf[cy][cx];
                    buf[cy] = buf[cy].slice(0, cx) + buf[cy].slice(cx + 1);
                    dirty = true;
                }
            }
            vimLinewise = false;
            if (cx >= buf[cy].length && cx > 0) cx = buf[cy].length - 1;
            break;
        case "s":
            pushUndo();
            if (cx < buf[cy].length) {
                vimReg = buf[cy][cx];
                buf[cy] = buf[cy].slice(0, cx) + buf[cy].slice(cx + 1);
                dirty = true;
            }
            vimLinewise = false;
            vim = "insert";
            break;
        case "r":
            vimPending = "r";
            return;
        case "p":
            vimPaste(false, count);
            break;
        case "P":
            vimPaste(true, count);
            break;
        case "u":
            for (let i = 0; i < count; i++) doUndo();
            break;
        case "J":
            pushUndo();
            for (let i = 0; i < count; i++) {
                if (cy < buf.length - 1) {
                    let trail = buf[cy + 1].replace(/^\s+/, "");
                    buf[cy] += (trail ? " " : "") + trail;
                    buf.splice(cy + 1, 1);
                    dirty = true;
                }
            }
            break;
        case "D":
            pushUndo();
            vimReg = buf[cy].slice(cx);
            vimLinewise = false;
            buf[cy] = buf[cy].slice(0, cx);
            if (cx > 0) cx--;
            dirty = true;
            break;
        case "C":
            pushUndo();
            vimReg = buf[cy].slice(cx);
            vimLinewise = false;
            buf[cy] = buf[cy].slice(0, cx);
            dirty = true;
            vim = "insert";
            break;

        // ── Operators ──
        case "d":
            vimPending = "d";
            vimOpCount = count;
            return;
        case "c":
            vimPending = "c";
            vimOpCount = count;
            return;
        case "y":
            vimPending = "y";
            vimOpCount = count;
            return;

        // ── Search ──
        case "n":
            for (let i = 0; i < count; i++) vimSearchNext(1);
            break;
        case "N":
            for (let i = 0; i < count; i++) vimSearchNext(-1);
            break;

        default:
            return;
    }

    ensureVisible();
    resetBlink();
}

// ── Vim visual mode ──

function vimVisual(ch) {
    if ((ch >= "1" && ch <= "9") || (ch === "0" && vimCount !== "")) {
        vimCount += ch;
        return;
    }

    let count = vimCount !== "" ? parseInt(vimCount) : 1;
    vimCount = "";

    // Operators on selection
    switch (ch) {
        case "d":
        case "x":
            vimReg = selText();
            vimLinewise = vim === "vline";
            deleteSel();
            vim = "normal";
            if (cx >= buf[cy].length && cx > 0) cx = buf[cy].length - 1;
            ensureVisible();
            resetBlink();
            return;
        case "y":
            vimReg = selText();
            vimLinewise = vim === "vline";
            anchor = null;
            vim = "normal";
            status("Yanked");
            return;
        case "c":
            vimReg = selText();
            vimLinewise = vim === "vline";
            deleteSel();
            vim = "insert";
            ensureVisible();
            resetBlink();
            return;
    }

    // Motions
    switch (ch) {
        case "h":
            for (let i = 0; i < count; i++) if (cx > 0) cx--;
            break;
        case "j":
            for (let i = 0; i < count; i++) if (cy < buf.length - 1) cy++;
            cx = clamp(cx, 0, buf[cy].length);
            break;
        case "k":
            for (let i = 0; i < count; i++) if (cy > 0) cy--;
            cx = clamp(cx, 0, buf[cy].length);
            break;
        case "l":
            for (let i = 0; i < count; i++) if (cx < buf[cy].length) cx++;
            break;
        case "w":
            for (let i = 0; i < count; i++) vimWordForward();
            break;
        case "b":
            for (let i = 0; i < count; i++) vimWordBack();
            break;
        case "e":
            for (let i = 0; i < count; i++) vimWordEnd();
            break;
        case "0":
            cx = 0;
            break;
        case "$":
            cx = buf[cy].length;
            break;
        case "^":
            cx = firstNonBlank(cy);
            break;
        case "G":
            cy = buf.length - 1;
            cx = clamp(cx, 0, buf[cy].length);
            break;
        case "g":
            vimPending = "g";
            return;
        default:
            return;
    }

    // For visual-line mode, extend to full lines
    if (vim === "vline") {
        if (cy >= vimVlineStart) {
            anchor = { x: 0, y: vimVlineStart };
            cx = buf[cy].length;
        } else {
            anchor = { x: buf[vimVlineStart].length, y: vimVlineStart };
            cx = 0;
        }
    }

    ensureVisible();
    resetBlink();
}

// ── Resolve pending operator ──

function vimResolvePending(ch, motionCount) {
    let op = vimPending;
    let opCount = vimOpCount;
    vimPending = "";
    vimOpCount = 1;

    let totalCount = opCount * motionCount;

    // Replace character
    if (op === "r") {
        pushUndo();
        if (cx < buf[cy].length) {
            buf[cy] = buf[cy].slice(0, cx) + ch + buf[cy].slice(cx + 1);
            dirty = true;
        }
        ensureVisible();
        resetBlink();
        return;
    }

    // gg — go to top or line N
    if (op === "g") {
        if (ch === "g") {
            cy = opCount > 1 ? clamp(opCount - 1, 0, buf.length - 1) : 0;
            cx = firstNonBlank(cy);
        }
        ensureVisible();
        resetBlink();
        return;
    }

    // Doubled operator (dd, yy, cc) — linewise
    if (ch === op) {
        vimLinewiseOp(op, totalCount);
        return;
    }

    // Operator + motion
    vimMotionOp(op, ch, totalCount);
}

// ── Linewise operator (dd, yy, cc) ──

function vimLinewiseOp(op, count) {
    let startLine = cy;
    let endLine = Math.min(cy + count - 1, buf.length - 1);
    let text = "";
    for (let i = startLine; i <= endLine; i++) text += buf[i] + "\n";
    vimReg = text;
    vimLinewise = true;

    if (op === "d" || op === "c") {
        pushUndo();
        buf.splice(startLine, endLine - startLine + 1);
        if (!buf.length) buf = [""];
        cy = Math.min(startLine, buf.length - 1);
        cx = firstNonBlank(cy);
        dirty = true;
    }
    if (op === "c") vim = "insert";
    if (op === "y") status("Yanked " + (endLine - startLine + 1) + " lines");

    ensureVisible();
    resetBlink();
}

// ── Operator + motion ──

function vimMotionOp(op, ch, count) {
    let ocx = cx,
        ocy = cy;

    // Execute motion
    let moved = false;
    switch (ch) {
        case "h":
            for (let i = 0; i < count; i++)
                if (cx > 0) {
                    cx--;
                    moved = true;
                }
            break;
        case "j":
            for (let i = 0; i < count; i++)
                if (cy < buf.length - 1) {
                    cy++;
                    moved = true;
                }
            break;
        case "k":
            for (let i = 0; i < count; i++)
                if (cy > 0) {
                    cy--;
                    moved = true;
                }
            break;
        case "l":
            for (let i = 0; i < count; i++)
                if (cx < buf[cy].length) {
                    cx++;
                    moved = true;
                }
            break;
        case "w":
            for (let i = 0; i < count; i++) vimWordForward();
            moved = true;
            break;
        case "b":
            for (let i = 0; i < count; i++) vimWordBack();
            moved = true;
            break;
        case "e":
            for (let i = 0; i < count; i++) {
                vimWordEnd();
                cx++;
            }
            moved = true;
            break;
        case "$":
            cx = buf[cy].length;
            moved = true;
            break;
        case "0":
            cx = 0;
            moved = true;
            break;
        case "^":
            cx = firstNonBlank(cy);
            moved = true;
            break;
        case "G":
            cy = buf.length - 1;
            moved = true;
            break;
        default:
            return;
    }

    if (!moved) return;

    // j/k/G motions are linewise
    let linewise = ch === "j" || ch === "k" || ch === "G";

    if (linewise) {
        let sl = Math.min(ocy, cy);
        let el = Math.max(ocy, cy);
        let text = "";
        for (let i = sl; i <= el; i++) text += buf[i] + "\n";
        vimReg = text;
        vimLinewise = true;

        if (op === "d" || op === "c") {
            pushUndo();
            buf.splice(sl, el - sl + 1);
            if (!buf.length) buf = [""];
            cy = Math.min(sl, buf.length - 1);
            cx = firstNonBlank(cy);
            dirty = true;
        }
        if (op === "c") vim = "insert";
        if (op === "y") {
            cx = ocx;
            cy = ocy;
            status("Yanked " + (el - sl + 1) + " lines");
        }
    } else {
        let a = { x: ocx, y: ocy };
        let b = { x: cx, y: cy };
        if (a.y > b.y || (a.y === b.y && a.x > b.x)) {
            let t = a;
            a = b;
            b = t;
        }

        let text;
        if (a.y === b.y) {
            text = buf[a.y].slice(a.x, b.x);
        } else {
            let parts = [buf[a.y].slice(a.x)];
            for (let i = a.y + 1; i < b.y; i++) parts.push(buf[i]);
            parts.push(buf[b.y].slice(0, b.x));
            text = parts.join("\n");
        }
        vimReg = text;
        vimLinewise = false;

        if (op === "d" || op === "c") {
            pushUndo();
            if (a.y === b.y) {
                buf[a.y] = buf[a.y].slice(0, a.x) + buf[a.y].slice(b.x);
            } else {
                buf[a.y] = buf[a.y].slice(0, a.x) + buf[b.y].slice(b.x);
                buf.splice(a.y + 1, b.y - a.y);
            }
            cx = a.x;
            cy = a.y;
            if (cx >= buf[cy].length && cx > 0 && op === "d") cx--;
            dirty = true;
        }
        if (op === "c") vim = "insert";
        if (op === "y") {
            cx = ocx;
            cy = ocy;
            status("Yanked");
        }
    }

    ensureVisible();
    resetBlink();
}

// ── Vim paste ──

function vimPaste(before, count) {
    if (!vimReg) return;
    pushUndo();

    if (vimLinewise) {
        let lines = vimReg.split("\n");
        if (lines.length && lines[lines.length - 1] === "") lines.pop();
        for (let c = 0; c < count; c++) {
            let insertAt = before ? cy : cy + 1;
            for (let i = 0; i < lines.length; i++) {
                buf.splice(insertAt + i, 0, lines[i]);
            }
            if (!before) cy += lines.length;
        }
        cx = firstNonBlank(cy);
    } else {
        for (let c = 0; c < count; c++) {
            let pos = before ? cx : Math.min(cx + 1, buf[cy].length);
            buf[cy] = buf[cy].slice(0, pos) + vimReg + buf[cy].slice(pos);
            cx = pos + vimReg.length - 1;
        }
    }

    dirty = true;
    ensureVisible();
    resetBlink();
}

// ── Vim command execution ──

function vimExecCmd(cmd) {
    if (cmd === "w") {
        saveFile();
    } else if (cmd === "q") {
        buf = [""];
        cx = cy = ox = oy = 0;
        fname = "";
        dirty = false;
        undoStack = [];
        anchor = null;
        status("Buffer closed");
    } else if (cmd === "wq") {
        saveFile();
        buf = [""];
        cx = cy = ox = oy = 0;
        fname = "";
        dirty = false;
        undoStack = [];
        anchor = null;
        status("Saved & closed");
    } else if (cmd === "e" || cmd === "e ") {
        openBrowser();
    } else if (/^[0-9]+$/.test(cmd)) {
        let line = parseInt(cmd);
        cy = clamp(line - 1, 0, buf.length - 1);
        cx = firstNonBlank(cy);
        ensureVisible();
        resetBlink();
    } else if (cmd.charAt(0) === "/") {
        vimSearch = cmd.slice(1);
        if (vimSearch) vimSearchNext(1);
    } else {
        status("Unknown: :" + cmd);
    }
}

// ── Vim search ──

function vimSearchNext(dir) {
    if (!vimSearch) {
        status("No search pattern");
        return;
    }

    let total = buf.length;
    let startCol = cx + dir;

    for (let i = 0; i < total; i++) {
        let li = (cy + i * dir + total) % total;
        let line = buf[li];
        let col;

        if (i === 0) {
            if (dir > 0) {
                col = line.indexOf(vimSearch, Math.max(0, startCol));
            } else {
                col = line.lastIndexOf(vimSearch, Math.max(0, startCol - 1));
            }
        } else {
            col = dir > 0 ? line.indexOf(vimSearch) : line.lastIndexOf(vimSearch);
        }

        if (col >= 0) {
            cy = li;
            cx = col;
            ensureVisible();
            resetBlink();
            status("/" + vimSearch);
            return;
        }
    }

    status("Not found: " + vimSearch);
}

// ─── Editor drawing ──────────────────────────────────────────────────────────

function drawEditor() {
    let editY = HEAD * CH;
    let footY = (ROWS - FOOT) * CH;

    // ── Header bar ──
    gfx.rectfill(0, 0, FB_W - 1, CH - 1, HEADBG);
    let title = (dirty ? "\x07 " : "  ") + (fname || "[untitled]");
    gfx.print(title, GUTTER * CW, 0, dirty ? DIRTCOL : HEADFG);
    if (vimEnabled) {
        gfx.print("[vim]", FB_W - 6 * CW, 0, GUTFG);
    }

    // ── Gutter background ──
    gfx.rectfill(0, editY, GUTTER * CW - 2, footY - 1, GUTBG);

    // ── Text area ──
    for (let r = 0; r < EROWS; r++) {
        let li = oy + r;
        if (li >= buf.length) break;
        let py = editY + r * CH;

        // Current-line highlight
        if (li === cy) {
            gfx.rectfill(GUTTER * CW, py, FB_W - 1, py + CH - 1, LINEBG);
        }

        // Selection highlight
        drawSelection(li, py);

        // Line number
        let num = String(li + 1);
        let pad = GUTTER - 1 - num.length;
        gfx.print(num, pad * CW, py, li === cy ? HEADFG : GUTFG);

        // Syntax-highlighted text
        let tokens = tokenize(buf[li]);
        let col = 0;
        let gutterPx = GUTTER * CW;
        for (let t = 0; t < tokens.length; t++) {
            let tok = tokens[t];
            for (let c = 0; c < tok.text.length; c++) {
                let vc = col - ox;
                if (vc >= 0 && vc < ECOLS) {
                    gfx.print(tok.text[c], gutterPx + vc * CW, py, tok.col);
                }
                col++;
            }
        }
    }

    // ── Cursor ──
    if (curOn) {
        let scrX = GUTTER * CW + (cx - ox) * CW;
        let scrY = editY + (cy - oy) * CH;
        if (cx >= ox && cx <= ox + ECOLS && cy >= oy && cy < oy + EROWS) {
            if (vimEnabled && vim !== "insert") {
                // Block cursor
                gfx.rectfill(scrX, scrY, scrX + CW - 2, scrY + CH - 2, CURFG);
                if (cx < buf[cy].length) {
                    gfx.print(buf[cy][cx], scrX, scrY, BG);
                }
            } else {
                // Line cursor
                gfx.rectfill(scrX, scrY, scrX, scrY + CH - 2, CURFG);
            }
        }
    }

    // ── Status bar ──
    gfx.rectfill(0, footY, FB_W - 1, footY + CH - 1, FOOTBG);
    if (vimEnabled && vim === "command") {
        gfx.print(":" + vimCmd + "_", GUTTER * CW, footY, FG);
    } else {
        let modeStr = "";
        if (vimEnabled) {
            if (vim === "insert") modeStr = "-- INSERT --  ";
            else if (vim === "visual") modeStr = "-- VISUAL --  ";
            else if (vim === "vline") modeStr = "-- V-LINE --  ";
        }
        let pos = modeStr + "Ln " + (cy + 1) + ", Col " + (cx + 1) + "  (" + buf.length + " lines)";
        gfx.print(pos, GUTTER * CW, footY, FOOTFG);
        // Pending operator / count feedback
        let right = "";
        if (vimEnabled && (vimCount || vimPending)) right = vimCount + vimPending;
        if (msg) right = msg;
        if (right) {
            let rw = gfx.textWidth(right);
            gfx.print(right, FB_W - rw - CW, footY, FOOTFG);
        }
    }
}

function drawSelection(lineIdx, py) {
    let s = selOrdered();
    if (!s) return;
    if (lineIdx < s.a.y || lineIdx > s.b.y) return;

    let sc, ec;
    if (lineIdx === s.a.y && lineIdx === s.b.y) {
        sc = s.a.x;
        ec = s.b.x;
    } else if (lineIdx === s.a.y) {
        sc = s.a.x;
        ec = buf[lineIdx].length;
    } else if (lineIdx === s.b.y) {
        sc = 0;
        ec = s.b.x;
    } else {
        sc = 0;
        ec = buf[lineIdx].length;
    }

    let gutterPx = GUTTER * CW;
    let x0 = gutterPx + (sc - ox) * CW;
    let x1 = gutterPx + (ec - ox) * CW - 1;
    x0 = clamp(x0, gutterPx, FB_W - 1);
    x1 = clamp(x1, gutterPx, FB_W - 1);
    if (x1 >= x0) {
        gfx.rectfill(x0, py, x1, py + CH - 1, SELBG);
    }
}

// ─── File browser ────────────────────────────────────────────────────────────

function updateBrowser() {
    if (key.btnr(key.UP)) brIdx = Math.max(0, brIdx - 1);
    if (key.btnr(key.DOWN) && brEntries.length) brIdx = Math.min(brEntries.length - 1, brIdx + 1);
    if (brIdx < brScroll) brScroll = brIdx;
    if (brIdx >= brScroll + EROWS) brScroll = brIdx - EROWS + 1;

    // Mouse click in browser list
    let my = mouse.y();
    let editY = HEAD * CH;
    if (mouse.btnp(0) && my >= editY && brEntries.length) {
        let row = (brScroll + (my - editY) / CH) | 0;
        if (row >= 0 && row < brEntries.length) {
            brIdx = row;
        }
    }

    // Mouse wheel scroll
    let wheel = mouse.wheel();
    if (wheel !== 0) {
        brScroll = clamp(brScroll - wheel * 3, 0, Math.max(0, brEntries.length - EROWS));
        brIdx = clamp(brIdx, brScroll, brScroll + EROWS - 1);
    }

    if (key.btnp(key.ENTER) && brEntries.length) {
        let entry = brEntries[brIdx];
        if (entry.isDir) {
            if (entry.name === "..") {
                // Go up one level
                let parts = brDir.split("/").filter(function (p) {
                    return p;
                });
                parts.pop();
                brDir = parts.length ? parts.join("/") + "/" : "";
            } else {
                brDir = brDir + entry.name + "/";
            }
            refreshBrowser();
        } else {
            openFile(brDir + entry.name);
            brMode = false;
        }
    }
    if (key.btnp(key.ESCAPE)) brMode = false;
}

function drawBrowser() {
    // Header
    gfx.rectfill(0, 0, FB_W - 1, CH - 1, HEADBG);
    let hdr = "Open: /" + brDir + "  Up/Down select  Enter open  Esc cancel";
    gfx.print(hdr, CW, 0, HEADFG);

    let editY = HEAD * CH;
    for (let i = 0; i < EROWS && brScroll + i < brEntries.length; i++) {
        let fi = brScroll + i;
        let entry = brEntries[fi];
        let py = editY + i * CH;
        if (fi === brIdx) {
            gfx.rectfill(0, py, FB_W - 1, py + CH - 1, SELBG);
        }
        let label = entry.isDir ? entry.name + "/" : entry.name;
        gfx.print(label, CW * 2, py, fi === brIdx ? FG : GUTFG);
    }

    if (!brEntries.length) {
        gfx.print("Empty directory", CW * 2, editY, GUTFG);
    }
}
