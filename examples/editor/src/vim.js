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
