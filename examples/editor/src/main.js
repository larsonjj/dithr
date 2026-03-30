// ─── dithr editor ─────────────────────────────────────────────────────────────
//
// A minimal text editor built on the dithr fantasy console engine.
// Framebuffer: 640×360, built-in 4×6 font (5×7 advance with spacing).
//
// Shortcuts:
//   Ctrl+O   Open file browser       Ctrl+S   Save file
//   Ctrl+Z   Undo                    Ctrl+A   Select all
//   Ctrl+C   Copy (selection/line)   Ctrl+V   Paste
//   Escape   Clear selection / close browser
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
    return { buf, cx, cy, ox, oy, fname, dirty, undoStack, anchor, brDir };
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

// ─── Editor drawing ──────────────────────────────────────────────────────────

function drawEditor() {
    let editY = HEAD * CH;
    let footY = (ROWS - FOOT) * CH;

    // ── Header bar ──
    gfx.rectfill(0, 0, FB_W - 1, CH - 1, HEADBG);
    let title = (dirty ? "\x07 " : "  ") + (fname || "[untitled]");
    gfx.print(title, GUTTER * CW, 0, dirty ? DIRTCOL : HEADFG);

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
            gfx.rectfill(scrX, scrY, scrX, scrY + CH - 2, CURFG);
        }
    }

    // ── Status bar ──
    gfx.rectfill(0, footY, FB_W - 1, footY + CH - 1, FOOTBG);
    let pos = "Ln " + (cy + 1) + ", Col " + (cx + 1) + "  (" + buf.length + " lines)";
    gfx.print(pos, GUTTER * CW, footY, FOOTFG);
    if (msg) {
        let mw = gfx.textWidth(msg);
        gfx.print(msg, FB_W - mw - CW, footY, FOOTFG);
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
