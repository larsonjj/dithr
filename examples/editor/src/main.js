// ─── Editor entry point (ES module) ──────────────────────────────────────────

import { st } from './state.js';
import {
    TAB_CODE,
    TAB_SPRITES,
    TAB_MAP,
    TAB_SFX,
    TAB_MUSIC,
    BG,
    AUTO_CLOSE,
    FB_W,
    FB_H,
    CH,
    FG,
    GUTFG,
} from './config.js';
import { ensureVisible, resetBlink, status, handleTabSwitch, modKey, MOD_NAME } from './helpers.js';
import {
    deleteSel,
    pushUndo,
    openFile,
    storeFileState,
    loadFileState,
    saveFile,
} from './buffer.js';
import { vimNormal } from './vim.js';
import { updateEdit } from './edit.js';
import { drawTabBar, drawFileTabs, drawEditor } from './draw.js';
import { updateFind, drawFind, updateGoto, drawGoto } from './find.js';
import { updateBrowser, drawBrowser } from './browser.js';
import { updateSpriteEditor, drawSpriteEditor, saveSpritesToDisk } from './sprite_editor.js';
import {
    updateMapEditor,
    drawMapEditor,
    mapTextInput,
    saveMapToDisk,
    loadMapFromDisk,
} from './map_editor.js';
import { updateSfxEditor, drawSfxEditor, loadSfxFromDisk, saveSfxToDisk } from './sfx_editor.js';
import {
    updateMusicEditor,
    drawMusicEditor,
    loadMusFromDisk,
    saveMusToDisk,
} from './music_editor.js';

// ─── Text input event handler ────────────────────────────────────────────────

function onTextInput(ch) {
    if (st.activeTab === TAB_MAP && (st.mapResizeMode || st.mapRenameMode)) {
        mapTextInput(ch);
        return;
    }
    if (st.activeTab === TAB_SFX && st.sfxRenaming) {
        st.sfxRenameTxt = (st.sfxRenameTxt + ch).slice(0, 8);
        return;
    }
    if (st.activeTab === TAB_MUSIC && st.musRenaming) {
        st.musRenameTxt = (st.musRenameTxt + ch).slice(0, 8);
        return;
    }
    if (st.activeTab !== TAB_CODE) return;
    if (st.brMode) return;
    if (modKey()) return;

    if (st.findMode) {
        if (st.findField === 0) st.findText += ch;
        else st.replaceText += ch;
        return;
    }

    if (st.gotoMode) {
        if (ch >= '0' && ch <= '9') st.gotoText += ch;
        return;
    }

    if (st.vimEnabled && st.vim !== 'insert') {
        if (st.vim === 'command') {
            st.vimCmd += ch;
        } else {
            vimNormal(ch);
        }
        return;
    }

    // Skip over matching closing bracket already at cursor
    if ((ch === ')' || ch === ']' || ch === '}') && st.buf[st.cy][st.cx] === ch) {
        st.cx++;
        ensureVisible();
        resetBlink();
        return;
    }

    deleteSel();
    pushUndo();
    const closer = AUTO_CLOSE[ch];
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

// ─── Editor preference persistence ───────────────────────────────────────────

function loadEditorPrefs() {
    const raw = sys.readFile('editor-prefs.json');
    if (!raw) return;
    try {
        const p = JSON.parse(raw);
        if (p.vim !== undefined) st.vimEnabled = !!p.vim;
        if (p.find) st.lastFindText = p.find;
        if (p.tab !== undefined) st.activeTab = p.tab;
    } catch {
        // ignore corrupt prefs
    }
}

function saveEditorPrefs() {
    const prefs = {
        vim: st.vimEnabled,
        find: st.lastFindText || '',
        tab: st.activeTab,
    };
    sys.writeFile('editor-prefs.json', JSON.stringify(prefs));
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

export function _init() {
    sys.textInput(true);
    evt.on('text:input', onTextInput);

    // Create a default spritesheet if none was loaded from cart
    if (gfx.sheetW() === 0 || gfx.sheetH() === 0) {
        gfx.sheetCreate(128, 128, 8, 8);
    }

    // Load saved data from disk (between-run persistence)
    if (!st.restored) {
        const hex = sys.readFile('sprites.hex');
        if (hex) gfx.sheetLoad(hex);
        const flags = sys.readFile('flags.hex');
        if (flags) gfx.flagsLoad(flags);
        loadSfxFromDisk();
        loadMusFromDisk();
        loadMapFromDisk();
        loadEditorPrefs();
        openFile('src/main.js');
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
        sprSizeW: st.sprSizeW,
        sprSizeH: st.sprSizeH,
        sprFilled: st.sprFilled,
        sprMirrorX: st.sprMirrorX,
        sprMirrorY: st.sprMirrorY,
        sprPalScrollY: st.sprPalScrollY,
        sprZoom: st.sprZoom,
        sprPanX: st.sprPanX,
        sprPanY: st.sprPanY,
        sprAnimFrom: st.sprAnimFrom,
        sprAnimTo: st.sprAnimTo,
        sprAnimFps: st.sprAnimFps,
        mapCamX: st.mapCamX,
        mapCamY: st.mapCamY,
        mapLayer: st.mapLayer,
        mapTile: st.mapTile,
        mapTool: st.mapTool,
        mapGridOn: st.mapGridOn,
        mapZoom: st.mapZoom,
        mapGhostLayers: st.mapGhostLayers,
        mapLayerVis: st.mapLayerVis,
        mapMinimap: st.mapMinimap,
        mapAutoTile: st.mapAutoTile,
        mapAutoGroups: st.mapAutoGroups,
        mapObjCounter: st.mapObjCounter,
        sfxSel: st.sfxSel,
        sfxNote: st.sfxNote,
        sfxField: st.sfxField,
        sfxSpeed: st.sfxSpeed,
        sfxWave: st.sfxWave,
        sfxVol: st.sfxVol,
        sfxFx: st.sfxFx,
        sfxListScroll: st.sfxListScroll,
        sfxScrollX: st.sfxScrollX,
        musPatterns: st.musPatterns,
        musSel: st.musSel,
        musCol: st.musCol,
        musScrollY: st.musScrollY,
        sheetHex: gfx.sheetData(),
        flagsHex: gfx.flagsData(),
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
    st.brDir = s.brDir || '';
    st.vimEnabled = s.vimEnabled || false;
    st.lastFindText = s.lastFindText || '';
    st.activeTab = s.activeTab || TAB_CODE;
    st.sprSel = s.sprSel || 0;
    st.sprCol = s.sprCol || 7;
    st.sprTool = s.sprTool || 0;
    st.sprScrollY = s.sprScrollY || 0;
    st.sprSizeW = s.sprSizeW || 1;
    st.sprSizeH = s.sprSizeH || 1;
    st.sprFilled = s.sprFilled || false;
    st.sprMirrorX = s.sprMirrorX || false;
    st.sprMirrorY = s.sprMirrorY || false;
    st.sprPalScrollY = s.sprPalScrollY || 0;
    st.sprZoom = s.sprZoom || 0;
    st.sprPanX = s.sprPanX || 0;
    st.sprPanY = s.sprPanY || 0;
    st.sprAnimFrom = s.sprAnimFrom || 0;
    st.sprAnimTo = s.sprAnimTo || 0;
    st.sprAnimFps = s.sprAnimFps || 8;
    st.mapCamX = s.mapCamX || 0;
    st.mapCamY = s.mapCamY || 0;
    st.mapLayer = s.mapLayer || 0;
    st.mapTile = s.mapTile || 0;
    st.mapTool = s.mapTool || 0;
    st.mapGridOn = s.mapGridOn !== undefined ? s.mapGridOn : true;
    st.mapZoom = s.mapZoom || 1;
    st.mapGhostLayers = s.mapGhostLayers !== undefined ? s.mapGhostLayers : true;
    st.mapLayerVis = s.mapLayerVis || [];
    st.mapMinimap = s.mapMinimap !== undefined ? s.mapMinimap : true;
    st.mapAutoTile = s.mapAutoTile || false;
    st.mapAutoGroups = s.mapAutoGroups || [];
    st.mapObjCounter = s.mapObjCounter || 0;
    st.sfxSel = s.sfxSel || 0;
    st.sfxNote = s.sfxNote || 0;
    st.sfxField = s.sfxField || 0;
    st.sfxSpeed = s.sfxSpeed || 16;
    st.sfxWave = s.sfxWave || 0;
    st.sfxVol = s.sfxVol || 5;
    st.sfxFx = s.sfxFx || 0;
    st.sfxListScroll = s.sfxListScroll || 0;
    st.sfxScrollX = s.sfxScrollX || 0;
    st.musPatterns = s.musPatterns || null;
    st.musSel = s.musSel || 0;
    st.musCol = s.musCol || 0;
    st.musScrollY = s.musScrollY || 0;
    if (s.sheetHex) gfx.sheetLoad(s.sheetHex);
    if (s.flagsHex) gfx.flagsLoad(s.flagsHex);
    // Clamp sprite editor values to valid range for current sheet
    const sprMax = Math.max(0, Math.floor(gfx.sheetW() / 8) * Math.floor(gfx.sheetH() / 8) - 1);
    st.sprSel = Math.min(st.sprSel, sprMax);
    st.sprAnimFrom = Math.min(st.sprAnimFrom, sprMax);
    st.sprAnimTo = Math.min(st.sprAnimTo, sprMax);
    if (st.sprAnimFrom > st.sprAnimTo) st.sprAnimTo = st.sprAnimFrom;
    st.targetOy = st.oy;
    st.vim = 'normal';
    st.vimCount = '';
    st.vimPending = '';
    st.vimCmd = '';
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
        if (st.msgT <= 0) st.msg = '';
    }

    // Smooth scroll interpolation
    if (st.oy !== st.targetOy) {
        const diff = st.targetOy - st.oy;
        if (Math.abs(diff) <= 1) {
            st.oy = st.targetOy;
        } else {
            const step = diff * 0.3;
            let newOy = diff < 0 ? Math.floor(st.oy + step) : Math.ceil(st.oy + step);
            newOy = Math.max(0, newOy);
            if (newOy === st.oy) {
                st.oy = st.targetOy;
            } else {
                st.oy = newOy;
            }
        }
    }

    // Tab switching (Ctrl+1/2/3) — handled once here for all tabs
    if (handleTabSwitch()) {
        mouse.show(); // restore cursor when leaving sprite editor
        return;
    }

    // Save All (Ctrl+Shift+S)
    if (modKey() && (key.btn(key.LSHIFT) || key.btn(key.RSHIFT)) && key.btnp(key.S)) {
        saveFile();
        saveSpritesToDisk();
        saveMapToDisk();
        saveSfxToDisk();
        saveMusToDisk();
        saveEditorPrefs();
        status('All saved');
        return;
    }

    // Help overlay toggle (F1)
    if (key.btnp(key.F1)) {
        st.helpOverlay = !st.helpOverlay;
        return;
    }
    if (st.helpOverlay) {
        if (key.btnp(key.ESCAPE)) st.helpOverlay = false;
        return; // block all input while help is shown
    }

    if (st.activeTab === TAB_SPRITES) {
        updateSpriteEditor(dt);
        return;
    }
    if (st.activeTab === TAB_MAP) {
        updateMapEditor();
        return;
    }
    if (st.activeTab === TAB_SFX) {
        updateSfxEditor(dt);
        return;
    }
    if (st.activeTab === TAB_MUSIC) {
        updateMusicEditor(dt);
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
    } else if (st.activeTab === TAB_MAP) {
        drawMapEditor();
    } else if (st.activeTab === TAB_SFX) {
        drawSfxEditor();
    } else if (st.activeTab === TAB_MUSIC) {
        drawMusicEditor();
    } else {
        drawFileTabs();
        if (st.brMode) drawBrowser();
        else {
            drawEditor();
            if (st.findMode) drawFind();
            if (st.gotoMode) drawGoto();
        }
    }
    if (st.helpOverlay) drawHelpOverlay();
}

// ─── Help overlay ────────────────────────────────────────────────────────────

function drawHelpOverlay() {
    const MOD = MOD_NAME;
    const pad = 4;
    const col1X = pad;
    const col2X = FB_W / 2 + pad;
    const lh = CH + 2;

    // Semi-transparent background
    gfx.rectfill(0, 0, FB_W - 1, FB_H - 1, 0);

    let y = pad;
    gfx.print('KEYBOARD SHORTCUTS  (F1 to close)', pad, y, FG);
    y += lh + 4;

    if (st.activeTab === TAB_CODE) {
        const left = [
            '── Editing ──',
            `${MOD}+Z      Undo`,
            `${MOD}+Y      Redo`,
            `${MOD}+X      Cut`,
            `${MOD}+C      Copy`,
            `${MOD}+V      Paste`,
            `${MOD}+A      Select all`,
            `${MOD}+D      Duplicate line`,
            `${MOD}+Sh+D   Dup selection`,
            `${MOD}+Sh+K   Delete line`,
            `${MOD}+/      Toggle comment`,
            `${MOD}+Sh+Up  Move line up`,
            `${MOD}+Sh+Dn  Move line down`,
            `${MOD}+Sh+Ent Insert line above`,
            `${MOD}+Bksp   Delete word left`,
            `${MOD}+Del    Delete word right`,
            'Tab         Indent',
            'Shift+Tab   Dedent',
        ];
        const right = [
            '── Navigation ──',
            `${MOD}+F      Find`,
            `${MOD}+H      Find & Replace`,
            `${MOD}+G      Go to line`,
            `${MOD}+Space  Autocomplete`,
            `${MOD}+O      Open file`,
            `${MOD}+S      Save`,
            `${MOD}+Sh+S   Save all`,
            `${MOD}+W      Close file`,
            `${MOD}+Tab    Next file`,
            `${MOD}+Sh+Tab Prev file`,
            `${MOD}+'      Toggle vim mode`,
            `${MOD}+Left   Word left`,
            `${MOD}+Right  Word right`,
            'Home        Smart home',
            'DblClick    Select word',
            'TrplClick   Select line',
            '',
            '── Tabs ──',
            `${MOD}+1-5    Code/Spr/Map/SFX/Mus`,
        ];
        for (let i = 0; i < Math.max(left.length, right.length); i++) {
            if (i < left.length)
                gfx.print(left[i], col1X, y + i * lh, left[i][0] === '\u2500' ? FG : GUTFG);
            if (i < right.length)
                gfx.print(right[i], col2X, y + i * lh, right[i][0] === '\u2500' ? FG : GUTFG);
        }
    } else if (st.activeTab === TAB_SPRITES) {
        const left = [
            '── Tools ──',
            'B           Pen',
            'E           Eraser',
            'F           Flood fill',
            'R           Rectangle',
            'L           Line',
            'O           Circle',
            'S           Selection',
            'G           Toggle filled',
            'M           Cycle size',
            'P           Anim play/stop',
            'A           Anim range start',
            'Sh+A        Anim range end',
            '[/]         Anim FPS -/+',
            '',
            '── Drawing ──',
            '0-9         Palette color',
            '-/=         Prev/next color',
            'Right-click Eyedropper',
            'Sh+X        Mirror X',
            'Sh+Y        Mirror Y',
            'D           Cycle dither',
        ];
        const right = [
            '── Transforms ──',
            'Sh+H        Flip horiz',
            'Sh+V        Flip vert',
            'Sh+R        Rotate 90\xB0',
            'Sh+Arrows   Nudge',
            'Delete      Clear sprite',
            `${MOD}+C      Copy`,
            `${MOD}+V      Paste`,
            `${MOD}+Z      Undo`,
            `${MOD}+Y      Redo`,
            `${MOD}+S      Save sprites`,
            `${MOD}+Sh+S   Save all`,
            `${MOD}+Sh+E   Export PNG`,
            'Escape      Cancel shape',
            '',
            '── Navigation ──',
            'Arrows      Select sprite',
            'N           Goto sprite',
            'Home        Reset view',
            'Mouse wheel Scroll sheet',
            '',
            '── Tabs ──',
            `${MOD}+1-5    Code/Spr/Map/SFX/Mus`,
        ];
        for (let i = 0; i < Math.max(left.length, right.length); i++) {
            if (i < left.length)
                gfx.print(left[i], col1X, y + i * lh, left[i][0] === '\u2500' ? FG : GUTFG);
            if (i < right.length)
                gfx.print(right[i], col2X, y + i * lh, right[i][0] === '\u2500' ? FG : GUTFG);
        }
    } else if (st.activeTab === TAB_MAP) {
        const left = [
            '── Tools ──',
            'B           Pen',
            'E           Eraser',
            'F           Flood fill',
            'R           Rectangle',
            'V           Selection',
            'O           Object tool',
            'Right-click Pick tile',
            'Shift+click Stamp pick',
            '',
            '── Selection ──',
            `${MOD}+C      Copy`,
            `${MOD}+V      Paste`,
            'Delete      Clear',
            'Escape      Deselect',
            '',
            '── View ──',
            'WASD/Arrows Pan camera',
            'Shift+Move  Fast pan',
            'Mid-click   Drag pan',
            'Space+drag  Pan camera',
            'Scroll      Pan vertical',
            'Sh+Scroll   Pan horizontal',
            '-/=         Zoom out/in',
            `${MOD}+Wheel  Zoom`,
            'Home        Reset view',
            'M           Toggle minimap',
        ];
        const right = [
            '── Editing ──',
            '[/]         Prev/next layer',
            'G           Toggle grid',
            'H           Toggle ghost layers',
            'Escape      Cancel shape',
            `${MOD}+Z      Undo`,
            `${MOD}+Y      Redo`,
            `${MOD}+S      Save map`,
            `${MOD}+Sh+S   Save all`,
            '',
            '── Auto-tile ──',
            `${MOD}+A      Toggle auto-tile`,
            `${MOD}+Sh+A   Define/remove group`,
            '',
            '── Objects ──',
            `${MOD}+Sh+O   Add object layer`,
            'Sh+click    Place object',
            'Delete      Remove selected',
            '',
            '── Dialogs ──',
            `${MOD}+Sh+R   Resize map`,
            `${MOD}+L      Level picker`,
            `${MOD}+Sh+E   Export .tmj`,
            '',
            '── Layers ──',
            'Click eye   Toggle visibility',
            'Click name  Switch layer',
            'Click active Rename layer',
            'F2          Rename layer',
            '[+] / [-]   Add/remove layer',
            '',
            '── Tabs ──',
            `${MOD}+1-5    Code/Spr/Map/SFX/Mus`,
        ];
        for (let i = 0; i < Math.max(left.length, right.length); i++) {
            if (i < left.length)
                gfx.print(left[i], col1X, y + i * lh, left[i][0] === '\u2500' ? FG : GUTFG);
            if (i < right.length)
                gfx.print(right[i], col2X, y + i * lh, right[i][0] === '\u2500' ? FG : GUTFG);
        }
    } else if (st.activeTab === TAB_SFX) {
        const left = [
            '── Note Input ──',
            'Z-M         Piano keys',
            '             Z=C S=C# X=D ...',
            '[/]         Octave down/up',
            'W           Cycle waveform',
            'E           Cycle effect',
            '1-8         Set volume (vol row)',
            'Delete      Clear note/sel',
            'Sh+Up/Down  Nudge value +/-',
            '',
            '── Navigation ──',
            'Left/Right  Prev/next note',
            'Sh+Left/Rt  Extend selection',
            'Up/Down     Prev/next field',
            'Tab         Cycle field',
            'Home/End    First/last note',
            `${MOD}+Up/Dn  Prev/next SFX`,
            'Scroll      List / grid scroll',
        ];
        const right = [
            '── Playback ──',
            'Space       Play / stop',
            '',
            '── Edit ──',
            `${MOD}+Z      Undo`,
            `${MOD}+Y      Redo`,
            `${MOD}+A      Select all notes`,
            `${MOD}+C      Copy (sel/SFX)`,
            `${MOD}+V      Paste (sel/SFX)`,
            `${MOD}+D      Duplicate SFX`,
            'Mouse drag  Paint pitch graph',
            `${MOD}+Sh+\u2191/\u2193 Transpose sel`,
            '',
            '── File ──',
            `${MOD}+S      Save SFX to disk`,
            `${MOD}+Sh+S   Save all`,
            `${MOD}+Sh+E   Export WAV`,
            '',
            '── Settings ──',
            '-/=         Speed down/up',
            `${MOD}+L      Set loop start`,
            `${MOD}+Sh+L   Set loop end`,
            'N           Rename SFX',
        ];
        for (let i = 0; i < Math.max(left.length, right.length); i++) {
            if (i < left.length)
                gfx.print(left[i], col1X, y + i * lh, left[i][0] === '\u2500' ? FG : GUTFG);
            if (i < right.length)
                gfx.print(right[i], col2X, y + i * lh, right[i][0] === '\u2500' ? FG : GUTFG);
        }
    } else if (st.activeTab === TAB_MUSIC) {
        const left = [
            '── Navigation ──',
            'Up/Down     Prev/next pattern',
            'Left/Right  Prev/next channel',
            `${MOD}+Up/Dn  Page up/down`,
            'Scroll      Scroll list',
            'Tab         Jump to SFX editor',
            '',
            '── Editing ──',
            '0-9         Enter SFX index',
            'Delete      Clear channel',
            '+/-         Increment/decrement',
            'F           Cycle flow flag',
            'Sh+F        Clear flow flag',
            'M           Toggle mute channel',
            'S           Solo / unsolo channel',
            `${MOD}+Sh+I   Insert pattern row`,
            `${MOD}+Sh+Del Delete pattern row`,
            'N           Rename pattern',
        ];
        const right = [
            '── Playback ──',
            'Space       Play / stop',
            'Enter       Preview SFX slot',
            '',
            '── Edit ──',
            `${MOD}+Z      Undo`,
            `${MOD}+Y      Redo`,
            `${MOD}+C      Copy pattern`,
            `${MOD}+V      Paste pattern`,
            '',
            '── File ──',
            `${MOD}+S      Save music to disk`,
            `${MOD}+Sh+S   Save all`,
            '',
            '── Tabs ──',
            `${MOD}+1-5    Code/Spr/Map/SFX/Mus`,
        ];
        for (let i = 0; i < Math.max(left.length, right.length); i++) {
            if (i < left.length)
                gfx.print(left[i], col1X, y + i * lh, left[i][0] === '\u2500' ? FG : GUTFG);
            if (i < right.length)
                gfx.print(right[i], col2X, y + i * lh, right[i][0] === '\u2500' ? FG : GUTFG);
        }
    }
}
