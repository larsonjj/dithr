// ─── Editor entry point (ES module) ──────────────────────────────────────────

import { st } from "./state.js";
import { TAB_CODE, TAB_SPRITES, TAB_MAP, BG, AUTO_CLOSE } from "./config.js";
import { ensureVisible, resetBlink, status, handleTabSwitch, modKey } from "./helpers.js";
import { deleteSel, pushUndo, openFile, storeFileState, loadFileState } from "./buffer.js";
import { vimNormal } from "./vim.js";
import { updateEdit } from "./edit.js";
import { drawTabBar, drawFileTabs, drawEditor } from "./draw.js";
import { updateFind, drawFind, updateGoto, drawGoto } from "./find.js";
import { updateBrowser, drawBrowser } from "./browser.js";
import { updateSpriteEditor, drawSpriteEditor } from "./sprite_editor.js";
import { updateMapEditor, drawMapEditor } from "./map_editor.js";

// ─── Text input event handler ────────────────────────────────────────────────

function onTextInput(ch) {
    if (st.activeTab !== TAB_CODE) return;
    if (st.brMode) return;
    if (modKey()) return;

    if (st.findMode) {
        if (st.findField === 0) st.findText += ch;
        else st.replaceText += ch;
        return;
    }

    if (st.gotoMode) {
        if (ch >= "0" && ch <= "9") st.gotoText += ch;
        return;
    }

    if (st.vimEnabled && st.vim !== "insert") {
        if (st.vim === "command") {
            st.vimCmd += ch;
        } else {
            vimNormal(ch);
        }
        return;
    }

    // Skip over matching closing bracket already at cursor
    if ((ch === ")" || ch === "]" || ch === "}") && st.buf[st.cy][st.cx] === ch) {
        st.cx++;
        ensureVisible();
        resetBlink();
        return;
    }

    deleteSel();
    pushUndo();
    let closer = AUTO_CLOSE[ch];
    if (closer) {
        st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + ch + closer + st.buf[st.cy].slice(st.cx);
        st.cx += 1;
    } else {
        st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + ch + st.buf[st.cy].slice(st.cx);
        st.cx += ch.length;
    }
    st.dirty = true;
    ensureVisible();
    resetBlink();
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

export function _init() {
    sys.textInput(true);
    evt.on("text:input", onTextInput);
    if (!st.restored) {
        openFile("src/main.js");
    }
    st.restored = false;
}

export function _save() {
    storeFileState();
    return {
        openFiles: st.openFiles,
        fileIdx: st.fileIdx,
        brDir: st.brDir,
        vimEnabled: st.vimEnabled,
        lastFindText: st.lastFindText,
        activeTab: st.activeTab,
        sprSel: st.sprSel,
        sprCol: st.sprCol,
        sprTool: st.sprTool,
        sprScrollY: st.sprScrollY,
        mapCamX: st.mapCamX,
        mapCamY: st.mapCamY,
        mapLayer: st.mapLayer,
        mapTile: st.mapTile,
        mapTool: st.mapTool,
        mapGridOn: st.mapGridOn,
    };
}

export function _restore(s) {
    if (s.openFiles) {
        st.openFiles = s.openFiles;
        st.fileIdx = s.fileIdx;
        if (st.fileIdx >= 0 && st.fileIdx < st.openFiles.length) {
            loadFileState(st.fileIdx);
        }
    } else {
        // Legacy single-file format
        st.buf = s.buf;
        st.cx = s.cx;
        st.cy = s.cy;
        st.ox = s.ox;
        st.oy = s.oy;
        st.fname = s.fname;
        st.dirty = s.dirty;
        st.undoStack = s.undoStack || [];
        st.redoStack = s.redoStack || [];
        st.anchor = s.anchor || null;
        st.savedBuf = s.savedBuf || [];
        if (st.fname) {
            st.openFiles = [
                {
                    path: st.fname,
                    buf: st.buf,
                    cx: st.cx,
                    cy: st.cy,
                    ox: st.ox,
                    oy: st.oy,
                    targetOy: st.oy,
                    anchor: st.anchor,
                    undoStack: st.undoStack,
                    redoStack: st.redoStack,
                    dirty: st.dirty,
                    savedBuf: st.savedBuf,
                },
            ];
            st.fileIdx = 0;
        }
    }
    st.brDir = s.brDir || "";
    st.vimEnabled = s.vimEnabled || false;
    st.lastFindText = s.lastFindText || "";
    st.activeTab = s.activeTab || TAB_CODE;
    st.sprSel = s.sprSel || 0;
    st.sprCol = s.sprCol || 7;
    st.sprTool = s.sprTool || 0;
    st.sprScrollY = s.sprScrollY || 0;
    st.mapCamX = s.mapCamX || 0;
    st.mapCamY = s.mapCamY || 0;
    st.mapLayer = s.mapLayer || 0;
    st.mapTile = s.mapTile || 0;
    st.mapTool = s.mapTool || 0;
    st.mapGridOn = s.mapGridOn !== undefined ? s.mapGridOn : true;
    st.targetOy = st.oy;
    st.vim = "normal";
    st.vimCount = "";
    st.vimPending = "";
    st.vimCmd = "";
    st.restored = true;
    st.invalidateCaches();
}

export function _update(dt) {
    // Cursor blink
    st.blink += dt;
    if (st.blink >= 0.5) {
        st.blink -= 0.5;
        st.curOn = !st.curOn;
    }

    // Status decay
    if (st.msgT > 0) {
        st.msgT -= dt;
        if (st.msgT <= 0) st.msg = "";
    }

    // Smooth scroll interpolation
    if (st.oy !== st.targetOy) {
        let diff = st.targetOy - st.oy;
        if (Math.abs(diff) < 0.5) {
            st.oy = st.targetOy;
        } else {
            st.oy = st.oy + diff * 0.3;
            st.oy = Math.round(st.oy);
        }
    }

    // Tab switching (Ctrl+1/2/3) — handled once here for all tabs
    if (handleTabSwitch()) return;

    if (st.activeTab === TAB_SPRITES) {
        updateSpriteEditor();
        return;
    }
    if (st.activeTab === TAB_MAP) {
        updateMapEditor();
        return;
    }

    if (st.brMode) updateBrowser();
    else if (st.findMode) updateFind();
    else if (st.gotoMode) updateGoto();
    else updateEdit();
}

export function _draw() {
    gfx.cls(BG);
    drawTabBar();
    if (st.activeTab === TAB_SPRITES) {
        drawSpriteEditor();
        return;
    }
    if (st.activeTab === TAB_MAP) {
        drawMapEditor();
        return;
    }

    drawFileTabs();

    if (st.brMode) drawBrowser();
    else {
        drawEditor();
        if (st.findMode) drawFind();
        if (st.gotoMode) drawGoto();
    }
}
