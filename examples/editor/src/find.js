// ─── Find & Replace ──────────────────────────────────────────────────────────

function updateFind() {
    let ctrl = key.btn(key.LCTRL) || key.btn(key.RCTRL);
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    if (key.btnp(key.ESCAPE)) {
        if (findText) lastFindText = findText;
        findMode = false;
        return;
    }

    if (key.btnr(key.BACKSPACE)) {
        if (findField === 0 && findText.length) findText = findText.slice(0, -1);
        if (findField === 1 && replaceText.length) replaceText = replaceText.slice(0, -1);
        return;
    }

    if (key.btnp(key.TAB) && findReplace) {
        findField = 1 - findField;
        return;
    }

    if (key.btnp(key.ENTER)) {
        if (findField === 0) {
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

function findNext(dir) {
    if (!findText) return;
    let total = buf.length;
    let startCol = cx + (dir > 0 ? 1 : 0);

    for (let i = 0; i < total; i++) {
        let li = (cy + i * dir + total) % total;
        let line = buf[li];
        let col;

        if (i === 0) {
            col =
                dir > 0
                    ? line.indexOf(findText, startCol)
                    : line.lastIndexOf(findText, Math.max(0, startCol - 1));
        } else {
            col = dir > 0 ? line.indexOf(findText) : line.lastIndexOf(findText);
        }

        if (col >= 0) {
            cy = li;
            cx = col;
            anchor = { x: col, y: li };
            cx = col + findText.length;
            ensureVisible();
            resetBlink();
            return;
        }
    }
    status("Not found: " + findText);
}

function replaceCurrent() {
    if (!findText) return;
    let s = selOrdered();
    if (s && s.a.y === s.b.y) {
        let sel = buf[s.a.y].slice(s.a.x, s.b.x);
        if (sel === findText) {
            pushUndo();
            buf[s.a.y] = buf[s.a.y].slice(0, s.a.x) + replaceText + buf[s.a.y].slice(s.b.x);
            cx = s.a.x + replaceText.length;
            cy = s.a.y;
            anchor = null;
            dirty = true;
        }
    }
}

function replaceAll() {
    if (!findText) return;
    pushUndo();
    let count = 0;
    for (let i = 0; i < buf.length; i++) {
        let parts = buf[i].split(findText);
        if (parts.length > 1) {
            count += parts.length - 1;
            buf[i] = parts.join(replaceText);
        }
    }
    anchor = null;
    if (count) {
        dirty = true;
        status("Replaced " + count + " occurrences");
    } else {
        status("No occurrences found");
    }
}

function drawFind() {
    let footY = (ROWS - FOOT) * CH;
    let gutterPx = GUTTER * CW;

    // Find field on footer row
    gfx.rectfill(0, footY, FB_W - 1, footY + CH - 1, HEADBG);
    let fl = "Find: ";
    gfx.print(fl, gutterPx, footY, findField === 0 ? FG : GUTFG);
    gfx.print(findText + (findField === 0 ? "_" : ""), gutterPx + fl.length * CW, footY, FG);

    // Hint on right
    let hint = findReplace ? "Tab:switch  Ctrl+Ent:all" : "Enter:next  Shift+Ent:prev";
    let hw = gfx.textWidth(hint);
    gfx.print(hint, FB_W - hw - CW, footY, GUTFG);

    // Replace field one row above footer
    if (findReplace) {
        let replY = footY - CH;
        gfx.rectfill(0, replY, FB_W - 1, replY + CH - 1, HEADBG);
        let rl = "Repl: ";
        gfx.print(rl, gutterPx, replY, findField === 1 ? FG : GUTFG);
        gfx.print(replaceText + (findField === 1 ? "_" : ""), gutterPx + rl.length * CW, replY, FG);
    }
}

// ─── Go to line ──────────────────────────────────────────────────────────────

function updateGoto() {
    if (key.btnp(key.ESCAPE)) {
        gotoMode = false;
        return;
    }
    if (key.btnr(key.BACKSPACE)) {
        if (gotoText.length) gotoText = gotoText.slice(0, -1);
        else gotoMode = false;
        return;
    }
    if (key.btnp(key.ENTER)) {
        let line = parseInt(gotoText);
        if (line > 0) {
            cy = clamp(line - 1, 0, buf.length - 1);
            cx = 0;
            anchor = null;
            ensureVisible();
            resetBlink();
        }
        gotoMode = false;
        return;
    }
}

function drawGoto() {
    let footY = (ROWS - FOOT) * CH;
    let gutterPx = GUTTER * CW;
    gfx.rectfill(0, footY, FB_W - 1, footY + CH - 1, HEADBG);
    let gl = "Go to line: ";
    gfx.print(gl, gutterPx, footY, FG);
    gfx.print(gotoText + "_", gutterPx + gl.length * CW, footY, FG);
}
