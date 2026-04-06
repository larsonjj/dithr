// ─── Edit-mode update ────────────────────────────────────────────────────────

import { st } from "./state.js";
import {
    TAB_CODE,
    TAB_SPRITES,
    TAB_MAP,
    HEAD,
    CH,
    FOOT,
    ROWS,
    GUTTER,
    CW,
    EROWS,
    TAB,
} from "./config.js";
import {
    clamp,
    ensureVisible,
    resetBlink,
    status,
    isWordChar,
    wordBoundaryLeft,
    wordBoundaryRight,
    getIndent,
    firstNonBlank,
} from "./helpers.js";
import {
    selOrdered,
    selText,
    deleteSel,
    pushUndo,
    doUndo,
    doRedo,
    saveFile,
    openBrowser,
    switchToFile,
    closeFile,
} from "./buffer.js";
import { updateVimKeys, vimNormal } from "./vim.js";

export function updateEdit() {
    let ctrl = key.btn(key.LCTRL) || key.btn(key.RCTRL);
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    // ── Ctrl shortcuts ──
    if (ctrl) {
        if (key.btnp(key.NUM1)) {
            st.activeTab = TAB_CODE;
            return;
        }
        if (key.btnp(key.NUM2)) {
            st.activeTab = TAB_SPRITES;
            return;
        }
        if (key.btnp(key.NUM3)) {
            st.activeTab = TAB_MAP;
            return;
        }
        if (key.btnp(key.TAB)) {
            if (st.openFiles.length > 1) {
                let next = shift
                    ? (st.fileIdx - 1 + st.openFiles.length) % st.openFiles.length
                    : (st.fileIdx + 1) % st.openFiles.length;
                switchToFile(next);
            }
            return;
        }
        if (key.btnp(key.W)) {
            closeFile(st.fileIdx);
            return;
        }
        if (key.btnp(key.GRAVE)) {
            st.vimEnabled = !st.vimEnabled;
            if (st.vimEnabled) {
                st.vim = "normal";
                st.vimCount = "";
                st.vimPending = "";
                st.anchor = null;
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
            if (shift) {
                doRedo();
            } else {
                doUndo();
            }
            return;
        }
        if (key.btnp(key.Y)) {
            doRedo();
            return;
        }
        if (key.btnp(key.A)) {
            st.anchor = { x: 0, y: 0 };
            st.cy = st.buf.length - 1;
            st.cx = st.buf[st.cy].length;
            return;
        }
        if (key.btnp(key.C)) {
            let t = selText();
            if (t) sys.clipboardSet(t);
            else sys.clipboardSet(st.buf[st.cy]);
            status("Copied");
            return;
        }
        if (key.btnp(key.X)) {
            let t = selText();
            if (t) {
                sys.clipboardSet(t);
                deleteSel();
            } else {
                sys.clipboardSet(st.buf[st.cy] + "\n");
                pushUndo();
                st.buf.splice(st.cy, 1);
                if (!st.buf.length) st.buf = [""];
                st.cy = Math.min(st.cy, st.buf.length - 1);
                st.cx = Math.min(st.cx, st.buf[st.cy].length);
                st.dirty = true;
            }
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnp(key.V)) {
            let t = sys.clipboardGet();
            if (!t) return;
            deleteSel();
            pushUndo();
            let parts = t.replace(/\r\n/g, "\n").split("\n");
            if (parts.length === 1) {
                st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + t + st.buf[st.cy].slice(st.cx);
                st.cx += t.length;
            } else {
                let after = st.buf[st.cy].slice(st.cx);
                st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + parts[0];
                for (let i = 1; i < parts.length - 1; i++) {
                    st.buf.splice(st.cy + i, 0, parts[i]);
                }
                st.buf.splice(st.cy + parts.length - 1, 0, parts[parts.length - 1] + after);
                st.cy += parts.length - 1;
                st.cx = parts[parts.length - 1].length;
            }
            st.dirty = true;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnp(key.D)) {
            if (shift && st.anchor) {
                // Ctrl+Shift+D: duplicate selection inline
                let t = selText();
                if (t) {
                    let s = selOrdered();
                    pushUndo();
                    st.buf[s.b.y] = st.buf[s.b.y].slice(0, s.b.x) + t + st.buf[s.b.y].slice(s.b.x);
                    st.dirty = true;
                }
            } else {
                // Ctrl+D: duplicate line
                pushUndo();
                st.buf.splice(st.cy + 1, 0, st.buf[st.cy]);
                st.cy++;
                st.dirty = true;
            }
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnp(key.F)) {
            st.findMode = true;
            st.findReplace = false;
            st.findField = 0;
            return;
        }
        if (key.btnp(key.H)) {
            st.findMode = true;
            st.findReplace = true;
            st.findField = 0;
            return;
        }
        if (key.btnp(key.G)) {
            st.gotoMode = true;
            st.gotoText = "";
            return;
        }
        if (shift && key.btnp(key.K)) {
            pushUndo();
            let s = selOrdered();
            if (s) {
                st.buf.splice(s.a.y, s.b.y - s.a.y + 1);
                st.anchor = null;
                st.cy = Math.min(s.a.y, st.buf.length - 1);
            } else {
                st.buf.splice(st.cy, 1);
            }
            if (!st.buf.length) st.buf = [""];
            st.cy = Math.min(st.cy, st.buf.length - 1);
            st.cx = Math.min(st.cx, st.buf[st.cy].length);
            st.dirty = true;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnr(key.LEFT)) {
            let ox0 = st.cx,
                oy0 = st.cy;
            if (st.cx > 0) {
                let line = st.buf[st.cy];
                st.cx--;
                if (isWordChar(line[st.cx])) {
                    while (st.cx > 0 && isWordChar(line[st.cx - 1])) st.cx--;
                } else {
                    while (st.cx > 0 && !isWordChar(line[st.cx - 1]) && line[st.cx - 1] !== " ")
                        st.cx--;
                }
            } else if (st.cy > 0) {
                st.cy--;
                st.cx = st.buf[st.cy].length;
            }
            if (shift && !st.anchor) st.anchor = { x: ox0, y: oy0 };
            if (!shift) st.anchor = null;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnr(key.RIGHT)) {
            let ox0 = st.cx,
                oy0 = st.cy;
            let line = st.buf[st.cy];
            if (st.cx < line.length) {
                if (isWordChar(line[st.cx])) {
                    while (st.cx < line.length && isWordChar(line[st.cx])) st.cx++;
                } else if (line[st.cx] !== " ") {
                    while (st.cx < line.length && !isWordChar(line[st.cx]) && line[st.cx] !== " ")
                        st.cx++;
                }
                while (st.cx < line.length && line[st.cx] === " ") st.cx++;
            } else if (st.cy < st.buf.length - 1) {
                st.cy++;
                st.cx = 0;
            }
            if (shift && !st.anchor) st.anchor = { x: ox0, y: oy0 };
            if (!shift) st.anchor = null;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnp(key.SLASH)) {
            // Toggle line comment
            pushUndo();
            let s = selOrdered();
            let startLine = s ? s.a.y : st.cy;
            let endLine = s ? s.b.y : st.cy;
            // Check if all lines are commented
            let allCommented = true;
            for (let i = startLine; i <= endLine; i++) {
                let stripped = st.buf[i].replace(/^\s*/, "");
                if (stripped.length && stripped.slice(0, 2) !== "//") {
                    allCommented = false;
                    break;
                }
            }
            for (let i = startLine; i <= endLine; i++) {
                if (allCommented) {
                    let idx = st.buf[i].indexOf("//");
                    if (idx >= 0) {
                        let after = st.buf[i][idx + 2] === " " ? idx + 3 : idx + 2;
                        st.buf[i] = st.buf[i].slice(0, idx) + st.buf[i].slice(after);
                    }
                } else {
                    let fnb = 0;
                    while (
                        fnb < st.buf[i].length &&
                        (st.buf[i][fnb] === " " || st.buf[i][fnb] === "\t")
                    )
                        fnb++;
                    st.buf[i] = st.buf[i].slice(0, fnb) + "// " + st.buf[i].slice(fnb);
                }
            }
            st.dirty = true;
            resetBlink();
            return;
        }
        if (shift && key.btnr(key.UP)) {
            // Move line(s) up
            let s = selOrdered();
            let startLine = s ? s.a.y : st.cy;
            let endLine = s ? s.b.y : st.cy;
            if (startLine > 0) {
                pushUndo();
                let removed = st.buf.splice(startLine - 1, 1)[0];
                st.buf.splice(endLine, 0, removed);
                st.cy--;
                if (s) {
                    st.anchor = { x: s.a.x, y: s.a.y - 1 };
                }
                st.dirty = true;
                ensureVisible();
                resetBlink();
            }
            return;
        }
        if (shift && key.btnr(key.DOWN)) {
            // Move line(s) down
            let s = selOrdered();
            let startLine = s ? s.a.y : st.cy;
            let endLine = s ? s.b.y : st.cy;
            if (endLine < st.buf.length - 1) {
                pushUndo();
                let removed = st.buf.splice(endLine + 1, 1)[0];
                st.buf.splice(startLine, 0, removed);
                st.cy++;
                if (s) {
                    st.anchor = { x: s.a.x, y: s.a.y + 1 };
                }
                st.dirty = true;
                ensureVisible();
                resetBlink();
            }
            return;
        }
        if (shift && key.btnp(key.ENTER)) {
            // Insert line above
            pushUndo();
            let indent = getIndent(st.cy);
            st.buf.splice(st.cy, 0, indent);
            st.cx = indent.length;
            st.dirty = true;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnr(key.BACKSPACE)) {
            // Ctrl+Backspace: delete word left
            if (deleteSel()) {
                ensureVisible();
                resetBlink();
                return;
            }
            pushUndo();
            if (st.cx > 0) {
                let nc = wordBoundaryLeft(st.buf[st.cy], st.cx);
                st.buf[st.cy] = st.buf[st.cy].slice(0, nc) + st.buf[st.cy].slice(st.cx);
                st.cx = nc;
            } else if (st.cy > 0) {
                st.cx = st.buf[st.cy - 1].length;
                st.buf[st.cy - 1] += st.buf[st.cy];
                st.buf.splice(st.cy, 1);
                st.cy--;
            }
            st.dirty = true;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnr(key.DELETE)) {
            // Ctrl+Delete: delete word right
            if (deleteSel()) {
                ensureVisible();
                resetBlink();
                return;
            }
            pushUndo();
            if (st.cx < st.buf[st.cy].length) {
                let nc = wordBoundaryRight(st.buf[st.cy], st.cx);
                st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + st.buf[st.cy].slice(nc);
            } else if (st.cy < st.buf.length - 1) {
                st.buf[st.cy] += st.buf[st.cy + 1];
                st.buf.splice(st.cy + 1, 1);
            }
            st.dirty = true;
            resetBlink();
            return;
        }
        return;
    }

    // ── Mouse wheel (always active, including Vim normal) ──
    let wheel = mouse.wheel();
    if (wheel !== 0) {
        st.targetOy = clamp(st.targetOy - wheel * 3, 0, Math.max(0, st.buf.length - 1));
    }

    // ── Vim non-insert modes ──
    if (st.vimEnabled && st.vim !== "insert") {
        updateVimKeys();
        return;
    }

    // ── Vim insert: Escape returns to normal ──
    if (st.vimEnabled && key.btnp(key.ESCAPE)) {
        st.vim = "normal";
        st.cx = Math.max(0, st.cx - 1);
        st.anchor = null;
        ensureVisible();
        resetBlink();
        return;
    }

    // ── Navigation (with key repeat) ──
    let pcx = st.cx,
        pcy = st.cy;

    if (key.btnr(key.LEFT)) {
        if (st.cx > 0) st.cx--;
        else if (st.cy > 0) {
            st.cy--;
            st.cx = st.buf[st.cy].length;
        }
    }
    if (key.btnr(key.RIGHT)) {
        if (st.cx < st.buf[st.cy].length) st.cx++;
        else if (st.cy < st.buf.length - 1) {
            st.cy++;
            st.cx = 0;
        }
    }
    if (key.btnr(key.UP) && st.cy > 0) {
        st.cy--;
        st.cx = clamp(st.cx, 0, st.buf[st.cy].length);
    }
    if (key.btnr(key.DOWN) && st.cy < st.buf.length - 1) {
        st.cy++;
        st.cx = clamp(st.cx, 0, st.buf[st.cy].length);
    }
    if (key.btnr(key.HOME)) {
        let fnb = 0;
        while (
            fnb < st.buf[st.cy].length &&
            (st.buf[st.cy][fnb] === " " || st.buf[st.cy][fnb] === "\t")
        )
            fnb++;
        st.cx = st.cx === fnb ? 0 : fnb;
    }
    if (key.btnr(key.END)) st.cx = st.buf[st.cy].length;
    if (key.btnr(key.PAGEUP)) {
        st.cy = Math.max(0, st.cy - EROWS);
        st.cx = clamp(st.cx, 0, st.buf[st.cy].length);
    }
    if (key.btnr(key.PAGEDOWN)) {
        st.cy = Math.min(st.buf.length - 1, st.cy + EROWS);
        st.cx = clamp(st.cx, 0, st.buf[st.cy].length);
    }

    // Update selection anchor if shifted, clear if not
    if (st.cx !== pcx || st.cy !== pcy) {
        if (shift && !st.anchor) st.anchor = { x: pcx, y: pcy };
        if (!shift) st.anchor = null;
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
        if (st.cx > 0) {
            st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx - 1) + st.buf[st.cy].slice(st.cx);
            st.cx--;
        } else if (st.cy > 0) {
            st.cx = st.buf[st.cy - 1].length;
            st.buf[st.cy - 1] += st.buf[st.cy];
            st.buf.splice(st.cy, 1);
            st.cy--;
        }
        st.dirty = true;
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
        if (st.cx < st.buf[st.cy].length) {
            st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + st.buf[st.cy].slice(st.cx + 1);
        } else if (st.cy < st.buf.length - 1) {
            st.buf[st.cy] += st.buf[st.cy + 1];
            st.buf.splice(st.cy + 1, 1);
        }
        st.dirty = true;
        resetBlink();
    }

    if (key.btnp(key.ENTER)) {
        deleteSel();
        pushUndo();
        // Auto-indent: match leading whitespace of current line
        let indent = "";
        let m = st.buf[st.cy].match(/^(\s+)/);
        if (m) indent = m[1];
        let charBefore = st.cx > 0 ? st.buf[st.cy][st.cx - 1] : "";
        let charAfter = st.cx < st.buf[st.cy].length ? st.buf[st.cy][st.cx] : "";
        let after = st.buf[st.cy].slice(st.cx);
        st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx);
        if (charBefore === "{") {
            let newIndent = indent + TAB;
            st.cy++;
            if (charAfter === "}") {
                // Split braces: cursor on indented middle line, closing brace on dedented line below
                st.buf.splice(st.cy, 0, newIndent, indent + after);
            } else {
                st.buf.splice(st.cy, 0, newIndent + after);
            }
            st.cx = newIndent.length;
        } else {
            st.cy++;
            st.buf.splice(st.cy, 0, indent + after);
            st.cx = indent.length;
        }
        st.dirty = true;
        ensureVisible();
        resetBlink();
    }

    if (key.btnr(key.TAB)) {
        let s = selOrdered();
        if (s && s.a.y !== s.b.y) {
            // Indent / dedent selected lines
            pushUndo();
            for (let i = s.a.y; i <= s.b.y; i++) {
                if (shift) {
                    // Dedent: remove leading TAB
                    if (st.buf[i].slice(0, TAB.length) === TAB) {
                        st.buf[i] = st.buf[i].slice(TAB.length);
                    }
                } else {
                    st.buf[i] = TAB + st.buf[i];
                }
            }
            st.dirty = true;
            resetBlink();
        } else if (shift) {
            // Dedent current line
            pushUndo();
            if (st.buf[st.cy].slice(0, TAB.length) === TAB) {
                st.buf[st.cy] = st.buf[st.cy].slice(TAB.length);
                st.cx = Math.max(0, st.cx - TAB.length);
                st.dirty = true;
            }
            resetBlink();
        } else {
            deleteSel();
            pushUndo();
            st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + TAB + st.buf[st.cy].slice(st.cx);
            st.cx += TAB.length;
            st.dirty = true;
            ensureVisible();
            resetBlink();
        }
    }

    if (key.btnp(key.ESCAPE)) {
        st.anchor = null;
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
            let row = (st.oy + (my - editY) / CH) | 0;
            let col = (st.ox + (mx - gutterPx) / CW) | 0;
            row = clamp(row, 0, st.buf.length - 1);
            col = clamp(col, 0, st.buf[row].length);
            // Detect double/triple click
            let now = Date.now();
            if (
                now - st.lastClickTime < 400 &&
                Math.abs(mx - st.lastClickX) < 4 &&
                Math.abs(my - st.lastClickY) < 4
            ) {
                st.clickCount++;
            } else {
                st.clickCount = 1;
            }
            st.lastClickTime = now;
            st.lastClickX = mx;
            st.lastClickY = my;
            if (st.clickCount === 2) {
                // Double-click: select word
                st.cy = row;
                let line = st.buf[st.cy];
                let wl = col,
                    wr = col;
                while (wl > 0 && isWordChar(line[wl - 1])) wl--;
                while (wr < line.length && isWordChar(line[wr])) wr++;
                st.anchor = { x: wl, y: st.cy };
                st.cx = wr;
            } else if (st.clickCount >= 3) {
                // Triple-click: select line
                st.cy = row;
                st.anchor = { x: 0, y: st.cy };
                st.cx = st.buf[st.cy].length;
                st.clickCount = 3;
            } else {
                if (shift) {
                    if (!st.anchor) st.anchor = { x: st.cx, y: st.cy };
                } else {
                    st.anchor = null;
                }
                st.cx = col;
                st.cy = row;
            }
            ensureVisible();
            resetBlink();
        } else if (mouse.btn(0)) {
            // Drag to extend selection
            let row = (st.oy + (my - editY) / CH) | 0;
            let col = (st.ox + (mx - gutterPx) / CW) | 0;
            row = clamp(row, 0, st.buf.length - 1);
            col = clamp(col, 0, st.buf[row].length);
            if (!st.anchor) st.anchor = { x: st.cx, y: st.cy };
            st.cx = col;
            st.cy = row;
            ensureVisible();
            resetBlink();
        }
    }
}
