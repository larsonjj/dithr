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
        if (key.btnp(key.Y)) {
            doRedo();
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
        if (key.btnp(key.X)) {
            let t = selText();
            if (t) {
                sys.clipboardSet(t);
                deleteSel();
            } else {
                sys.clipboardSet(buf[cy] + "\n");
                pushUndo();
                buf.splice(cy, 1);
                if (!buf.length) buf = [""];
                cy = Math.min(cy, buf.length - 1);
                cx = Math.min(cx, buf[cy].length);
                dirty = true;
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
        if (key.btnp(key.D)) {
            if (shift && anchor) {
                // Ctrl+Shift+D: duplicate selection inline
                let t = selText();
                if (t) {
                    let s = selOrdered();
                    pushUndo();
                    buf[s.b.y] = buf[s.b.y].slice(0, s.b.x) + t + buf[s.b.y].slice(s.b.x);
                    dirty = true;
                }
            } else {
                // Ctrl+D: duplicate line
                pushUndo();
                buf.splice(cy + 1, 0, buf[cy]);
                cy++;
                dirty = true;
            }
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnp(key.F)) {
            findMode = true;
            findReplace = false;
            findField = 0;
            return;
        }
        if (key.btnp(key.H)) {
            findMode = true;
            findReplace = true;
            findField = 0;
            return;
        }
        if (key.btnp(key.G)) {
            gotoMode = true;
            gotoText = "";
            return;
        }
        if (shift && key.btnp(key.K)) {
            pushUndo();
            let s = selOrdered();
            if (s) {
                buf.splice(s.a.y, s.b.y - s.a.y + 1);
                anchor = null;
                cy = Math.min(s.a.y, buf.length - 1);
            } else {
                buf.splice(cy, 1);
            }
            if (!buf.length) buf = [""];
            cy = Math.min(cy, buf.length - 1);
            cx = Math.min(cx, buf[cy].length);
            dirty = true;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnr(key.LEFT)) {
            let ox0 = cx,
                oy0 = cy;
            if (cx > 0) {
                let line = buf[cy];
                cx--;
                if (isWordChar(line[cx])) {
                    while (cx > 0 && isWordChar(line[cx - 1])) cx--;
                } else {
                    while (cx > 0 && !isWordChar(line[cx - 1]) && line[cx - 1] !== " ") cx--;
                }
            } else if (cy > 0) {
                cy--;
                cx = buf[cy].length;
            }
            if (shift && !anchor) anchor = { x: ox0, y: oy0 };
            if (!shift) anchor = null;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnr(key.RIGHT)) {
            let ox0 = cx,
                oy0 = cy;
            let line = buf[cy];
            if (cx < line.length) {
                if (isWordChar(line[cx])) {
                    while (cx < line.length && isWordChar(line[cx])) cx++;
                } else if (line[cx] !== " ") {
                    while (cx < line.length && !isWordChar(line[cx]) && line[cx] !== " ") cx++;
                }
                while (cx < line.length && line[cx] === " ") cx++;
            } else if (cy < buf.length - 1) {
                cy++;
                cx = 0;
            }
            if (shift && !anchor) anchor = { x: ox0, y: oy0 };
            if (!shift) anchor = null;
            ensureVisible();
            resetBlink();
            return;
        }
        if (key.btnp(key.SLASH)) {
            // Toggle line comment
            pushUndo();
            let s = selOrdered();
            let startLine = s ? s.a.y : cy;
            let endLine = s ? s.b.y : cy;
            // Check if all lines are commented
            let allCommented = true;
            for (let i = startLine; i <= endLine; i++) {
                let stripped = buf[i].replace(/^\s*/, "");
                if (stripped.length && stripped.slice(0, 2) !== "//") {
                    allCommented = false;
                    break;
                }
            }
            for (let i = startLine; i <= endLine; i++) {
                if (allCommented) {
                    let idx = buf[i].indexOf("//");
                    if (idx >= 0) {
                        let after = buf[i][idx + 2] === " " ? idx + 3 : idx + 2;
                        buf[i] = buf[i].slice(0, idx) + buf[i].slice(after);
                    }
                } else {
                    let fnb = 0;
                    while (fnb < buf[i].length && (buf[i][fnb] === " " || buf[i][fnb] === "\t"))
                        fnb++;
                    buf[i] = buf[i].slice(0, fnb) + "// " + buf[i].slice(fnb);
                }
            }
            dirty = true;
            resetBlink();
            return;
        }
        if (shift && key.btnr(key.UP)) {
            // Move line(s) up
            let s = selOrdered();
            let startLine = s ? s.a.y : cy;
            let endLine = s ? s.b.y : cy;
            if (startLine > 0) {
                pushUndo();
                let removed = buf.splice(startLine - 1, 1)[0];
                buf.splice(endLine, 0, removed);
                cy--;
                if (s) {
                    anchor = { x: s.a.x, y: s.a.y - 1 };
                }
                dirty = true;
                ensureVisible();
                resetBlink();
            }
            return;
        }
        if (shift && key.btnr(key.DOWN)) {
            // Move line(s) down
            let s = selOrdered();
            let startLine = s ? s.a.y : cy;
            let endLine = s ? s.b.y : cy;
            if (endLine < buf.length - 1) {
                pushUndo();
                let removed = buf.splice(endLine + 1, 1)[0];
                buf.splice(startLine, 0, removed);
                cy++;
                if (s) {
                    anchor = { x: s.a.x, y: s.a.y + 1 };
                }
                dirty = true;
                ensureVisible();
                resetBlink();
            }
            return;
        }
        if (shift && key.btnp(key.ENTER)) {
            // Insert line above
            pushUndo();
            let indent = getIndent(cy);
            buf.splice(cy, 0, indent);
            cx = indent.length;
            dirty = true;
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
            if (cx > 0) {
                let nc = wordBoundaryLeft(buf[cy], cx);
                buf[cy] = buf[cy].slice(0, nc) + buf[cy].slice(cx);
                cx = nc;
            } else if (cy > 0) {
                cx = buf[cy - 1].length;
                buf[cy - 1] += buf[cy];
                buf.splice(cy, 1);
                cy--;
            }
            dirty = true;
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
            if (cx < buf[cy].length) {
                let nc = wordBoundaryRight(buf[cy], cx);
                buf[cy] = buf[cy].slice(0, cx) + buf[cy].slice(nc);
            } else if (cy < buf.length - 1) {
                buf[cy] += buf[cy + 1];
                buf.splice(cy + 1, 1);
            }
            dirty = true;
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
    if (key.btnr(key.HOME)) {
        let firstNonBlank = 0;
        while (
            firstNonBlank < buf[cy].length &&
            (buf[cy][firstNonBlank] === " " || buf[cy][firstNonBlank] === "\t")
        )
            firstNonBlank++;
        cx = cx === firstNonBlank ? 0 : firstNonBlank;
    }
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
        let charBefore = cx > 0 ? buf[cy][cx - 1] : "";
        let charAfter = cx < buf[cy].length ? buf[cy][cx] : "";
        let after = buf[cy].slice(cx);
        buf[cy] = buf[cy].slice(0, cx);
        if (charBefore === "{") {
            let newIndent = indent + TAB;
            cy++;
            if (charAfter === "}") {
                // Split braces: cursor on indented middle line, closing brace on dedented line below
                buf.splice(cy, 0, newIndent, indent + after);
            } else {
                buf.splice(cy, 0, newIndent + after);
            }
            cx = newIndent.length;
        } else {
            cy++;
            buf.splice(cy, 0, indent + after);
            cx = indent.length;
        }
        dirty = true;
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
                    if (buf[i].slice(0, TAB.length) === TAB) {
                        buf[i] = buf[i].slice(TAB.length);
                    }
                } else {
                    buf[i] = TAB + buf[i];
                }
            }
            dirty = true;
            resetBlink();
        } else if (shift) {
            // Dedent current line
            pushUndo();
            if (buf[cy].slice(0, TAB.length) === TAB) {
                buf[cy] = buf[cy].slice(TAB.length);
                cx = Math.max(0, cx - TAB.length);
                dirty = true;
            }
            resetBlink();
        } else {
            deleteSel();
            pushUndo();
            buf[cy] = buf[cy].slice(0, cx) + TAB + buf[cy].slice(cx);
            cx += TAB.length;
            dirty = true;
            ensureVisible();
            resetBlink();
        }
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
            // Detect double/triple click
            let now = Date.now();
            if (
                now - lastClickTime < 400 &&
                Math.abs(mx - lastClickX) < 4 &&
                Math.abs(my - lastClickY) < 4
            ) {
                clickCount++;
            } else {
                clickCount = 1;
            }
            lastClickTime = now;
            lastClickX = mx;
            lastClickY = my;
            if (clickCount === 2) {
                // Double-click: select word
                cy = row;
                let line = buf[cy];
                let wl = col,
                    wr = col;
                while (wl > 0 && isWordChar(line[wl - 1])) wl--;
                while (wr < line.length && isWordChar(line[wr])) wr++;
                anchor = { x: wl, y: cy };
                cx = wr;
            } else if (clickCount >= 3) {
                // Triple-click: select line
                cy = row;
                anchor = { x: 0, y: cy };
                cx = buf[cy].length;
                clickCount = 3;
            } else {
                if (shift) {
                    if (!anchor) anchor = { x: cx, y: cy };
                } else {
                    anchor = null;
                }
                cx = col;
                cy = row;
            }
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

    // Mouse wheel scroll (smooth)
    let wheel = mouse.wheel();
    if (wheel !== 0) {
        targetOy = clamp(targetOy - wheel * 3, 0, Math.max(0, buf.length - EROWS));
    }
}
