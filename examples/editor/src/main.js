// ─── Text input event handler ────────────────────────────────────────────────

function onTextInput(ch) {
    if (brMode) return;
    if (key.btn(key.LCTRL) || key.btn(key.RCTRL)) return;

    if (findMode) {
        if (findField === 0) findText += ch;
        else replaceText += ch;
        return;
    }

    if (gotoMode) {
        if (ch >= "0" && ch <= "9") gotoText += ch;
        return;
    }

    if (vimEnabled && vim !== "insert") {
        if (vim === "command") {
            vimCmd += ch;
        } else {
            vimNormal(ch);
        }
        return;
    }

    // Skip over matching closing bracket already at cursor
    if ((ch === ")" || ch === "]" || ch === "}") && buf[cy][cx] === ch) {
        cx++;
        ensureVisible();
        resetBlink();
        return;
    }

    deleteSel();
    pushUndo();
    let closer = AUTO_CLOSE[ch];
    if (closer) {
        buf[cy] = buf[cy].slice(0, cx) + ch + closer + buf[cy].slice(cx);
        cx += 1;
    } else {
        buf[cy] = buf[cy].slice(0, cx) + ch + buf[cy].slice(cx);
        cx += ch.length;
    }
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
    return {
        buf,
        cx,
        cy,
        ox,
        oy,
        fname,
        dirty,
        undoStack,
        redoStack,
        anchor,
        brDir,
        vimEnabled,
        savedBuf,
        lastFindText,
    };
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
    redoStack = s.redoStack || [];
    anchor = s.anchor || null;
    brDir = s.brDir || "";
    vimEnabled = s.vimEnabled || false;
    savedBuf = s.savedBuf || [];
    lastFindText = s.lastFindText || "";
    targetOy = oy;
    vim = "normal";
    vimCount = "";
    vimPending = "";
    vimCmd = "";
    restored = true;
    invalidateCaches();
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

    // Smooth scroll interpolation
    if (oy !== targetOy) {
        let diff = targetOy - oy;
        if (Math.abs(diff) < 0.5) {
            oy = targetOy;
        } else {
            oy = oy + diff * 0.3;
            oy = Math.round(oy);
        }
    }

    if (brMode) updateBrowser();
    else if (findMode) updateFind();
    else if (gotoMode) updateGoto();
    else updateEdit();
}

function _draw() {
    gfx.cls(BG);
    if (brMode) drawBrowser();
    else {
        drawEditor();
        if (findMode) drawFind();
        if (gotoMode) drawGoto();
    }
}
