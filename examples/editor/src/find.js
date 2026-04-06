// ─── Find & Replace ──────────────────────────────────────────────────────────

import { st } from "./state.js";
import { FB_W, FB_H, CH, GUTTER, CW, FOOT_Y, FOOT_H, FG, GUTFG, HEADBG } from "./config.js";
import { clamp, ensureVisible, resetBlink, status, modKey, MOD_NAME } from "./helpers.js";
import { selOrdered, pushUndo } from "./buffer.js";

export function updateFind() {
    let ctrl = modKey();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    if (key.btnp(key.ESCAPE)) {
        if (st.findText) st.lastFindText = st.findText;
        st.findMode = false;
        return;
    }

    if (key.btnr(key.BACKSPACE)) {
        if (st.findField === 0 && st.findText.length) st.findText = st.findText.slice(0, -1);
        if (st.findField === 1 && st.replaceText.length)
            st.replaceText = st.replaceText.slice(0, -1);
        return;
    }

    if (key.btnp(key.TAB) && st.findReplace) {
        st.findField = 1 - st.findField;
        return;
    }

    if (key.btnp(key.ENTER)) {
        if (st.findField === 0) {
            findNext(shift ? -1 : 1);
        } else {
            if (ctrl) {
                replaceAll();
            } else {
                replaceCurrent();
                findNext(1);
            }
        }
        return;
    }
}

export function findNext(dir) {
    if (!st.findText) return;
    let total = st.buf.length;
    let startCol = st.cx + (dir > 0 ? 1 : 0);

    for (let i = 0; i < total; i++) {
        let li = (st.cy + i * dir + total) % total;
        let line = st.buf[li];
        let col;

        if (i === 0) {
            col =
                dir > 0
                    ? line.indexOf(st.findText, startCol)
                    : line.lastIndexOf(st.findText, Math.max(0, startCol - 1));
        } else {
            col = dir > 0 ? line.indexOf(st.findText) : line.lastIndexOf(st.findText);
        }

        if (col >= 0) {
            st.cy = li;
            st.cx = col;
            st.anchor = { x: col, y: li };
            st.cx = col + st.findText.length;
            ensureVisible();
            resetBlink();
            return;
        }
    }
    status("Not found: " + st.findText);
}

export function replaceCurrent() {
    if (!st.findText) return;
    let s = selOrdered();
    if (s && s.a.y === s.b.y) {
        let sel = st.buf[s.a.y].slice(s.a.x, s.b.x);
        if (sel === st.findText) {
            pushUndo();
            st.buf[s.a.y] =
                st.buf[s.a.y].slice(0, s.a.x) + st.replaceText + st.buf[s.a.y].slice(s.b.x);
            st.cx = s.a.x + st.replaceText.length;
            st.cy = s.a.y;
            st.anchor = null;
            st.dirty = true;
        }
    }
}

export function replaceAll() {
    if (!st.findText) return;
    pushUndo();
    let count = 0;
    for (let i = 0; i < st.buf.length; i++) {
        let parts = st.buf[i].split(st.findText);
        if (parts.length > 1) {
            count += parts.length - 1;
            st.buf[i] = parts.join(st.replaceText);
        }
    }
    st.anchor = null;
    if (count) {
        st.dirty = true;
        status("Replaced " + count + " occurrences");
    } else {
        status("No occurrences found");
    }
}

export function drawFind() {
    let footY = FOOT_Y;
    let gutterPx = GUTTER * CW;

    // Find field on footer row
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, HEADBG);
    let footTextY = footY + Math.floor((FOOT_H - CH) / 2);
    let fl = "Find: ";
    gfx.print(fl, gutterPx, footTextY, st.findField === 0 ? FG : GUTFG);
    gfx.print(
        st.findText + (st.findField === 0 ? "_" : ""),
        gutterPx + fl.length * CW,
        footTextY,
        FG,
    );

    // Hint on right
    let hint = st.findReplace
        ? "Tab:switch  " + MOD_NAME + "+Ent:all"
        : "Enter:next  Shift+Ent:prev";
    let hw = gfx.textWidth(hint);
    gfx.print(hint, FB_W - hw - CW, footTextY, GUTFG);

    // Replace field one row above footer
    if (st.findReplace) {
        let replY = footY - FOOT_H;
        gfx.rectfill(0, replY, FB_W - 1, replY + FOOT_H - 1, HEADBG);
        let replTextY = replY + Math.floor((FOOT_H - CH) / 2);
        let rl = "Repl: ";
        gfx.print(rl, gutterPx, replTextY, st.findField === 1 ? FG : GUTFG);
        gfx.print(
            st.replaceText + (st.findField === 1 ? "_" : ""),
            gutterPx + rl.length * CW,
            replTextY,
            FG,
        );
    }
}

// ─── Go to line ──────────────────────────────────────────────────────────────

export function updateGoto() {
    if (key.btnp(key.ESCAPE)) {
        st.gotoMode = false;
        return;
    }
    if (key.btnr(key.BACKSPACE)) {
        if (st.gotoText.length) st.gotoText = st.gotoText.slice(0, -1);
        else st.gotoMode = false;
        return;
    }
    if (key.btnp(key.ENTER)) {
        let line = parseInt(st.gotoText);
        if (line > 0) {
            st.cy = clamp(line - 1, 0, st.buf.length - 1);
            st.cx = 0;
            st.anchor = null;
            ensureVisible();
            resetBlink();
        }
        st.gotoMode = false;
        return;
    }
}

export function drawGoto() {
    let footY = FOOT_Y;
    let gutterPx = GUTTER * CW;
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, HEADBG);
    let footTextY = footY + Math.floor((FOOT_H - CH) / 2);
    let gl = "Go to line: ";
    gfx.print(gl, gutterPx, footTextY, FG);
    gfx.print(st.gotoText + "_", gutterPx + gl.length * CW, footTextY, FG);
}
