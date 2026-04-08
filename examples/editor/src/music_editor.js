// ─── Music Editor ────────────────────────────────────────────────────────────
//
// Pattern-based music sequencer. 64 patterns, each with 4 channel slots
// that reference SFX indices (0-63). Patterns play sequentially and can
// have flow flags: loop-start, loop-back, stop.
//
// All 4 channels play simultaneously via the C synth voice pool.

import { st } from "./state.js";
import { FB_W, FB_H, CW, TAB_H, FG, BG, PANELBG, SEPC, GUTFG, FOOTBG, FOOTFG } from "./config.js";
import { clamp, modKey, status } from "./helpers.js";
import { TAB_SFX } from "./config.js";

// ─── Layout ──────────────────────────────────────────────────────────────────

const FOOT_H = 14;
const TOOLBAR_H = 14;
const ROW_H = 12;
const COL_W = 40;
const LABEL_W = 36;
const NUM_CHANNELS = 4;
const GRID_X = 0;
const GRID_Y = TAB_H + TOOLBAR_H;

// Flow flag constants
const FLAG_NONE = 0;
const FLAG_LOOP_START = 1;
const FLAG_LOOP_BACK = 2;
const FLAG_STOP = 3;
const FLAG_NAMES = ["", "\u25B6", "\u25C0", "\u25A0"]; // ▶ ◀ ■
const FLAG_COLORS = [0, 11, 9, 8]; // none, green, orange, red

// Colors
const SEL_COL = 2;
const PLAY_COL = 11;
const CH_COLS = [12, 14, 9, 13]; // cyan, pink, orange, lavender per channel

// Per-pattern names (editor-only, stored in music.json)
export let musNames = new Array(64).fill("");

// ─── Data ────────────────────────────────────────────────────────────────────

function ensurePatterns() {
    if (st.musPatterns) return;
    st.musPatterns = [];
    for (let i = 0; i < 64; i++) {
        st.musPatterns.push({ ch: [-1, -1, -1, -1], flags: FLAG_NONE });
    }
}

function patternHasData(p) {
    return p.ch[0] >= 0 || p.ch[1] >= 0 || p.ch[2] >= 0 || p.ch[3] >= 0;
}

// ─── Undo / Redo ─────────────────────────────────────────────────────────────

const MAX_MUS_UNDO = 50;

function clonePatterns() {
    let out = [];
    for (let i = 0; i < 64; i++) {
        let p = st.musPatterns[i];
        out.push({ ch: [p.ch[0], p.ch[1], p.ch[2], p.ch[3]], flags: p.flags });
    }
    return out;
}

function pushMusUndo() {
    if (!st.musUndoStack) st.musUndoStack = [];
    if (!st.musRedoStack) st.musRedoStack = [];
    st.musUndoStack.push(clonePatterns());
    if (st.musUndoStack.length > MAX_MUS_UNDO) st.musUndoStack.shift();
    st.musRedoStack = [];
}

function musUndo() {
    if (!st.musUndoStack || st.musUndoStack.length === 0) {
        status("Nothing to undo");
        return;
    }
    if (!st.musRedoStack) st.musRedoStack = [];
    st.musRedoStack.push(clonePatterns());
    st.musPatterns = st.musUndoStack.pop();
    st.musDirty = true;
    status("Undo");
}

function musRedo() {
    if (!st.musRedoStack || st.musRedoStack.length === 0) {
        status("Nothing to redo");
        return;
    }
    if (!st.musUndoStack) st.musUndoStack = [];
    st.musUndoStack.push(clonePatterns());
    st.musPatterns = st.musRedoStack.pop();
    st.musDirty = true;
    status("Redo");
}

// ─── Save / Load ─────────────────────────────────────────────────────────────

export function saveMusToDisk() {
    ensurePatterns();
    let all = [];
    for (let i = 0; i < 64; i++) {
        let p = st.musPatterns[i];
        if (!patternHasData(p) && p.flags === 0 && !musNames[i]) continue;
        let entry = { i: i, ch: [p.ch[0], p.ch[1], p.ch[2], p.ch[3]], f: p.flags };
        if (musNames[i]) entry.nm = musNames[i];
        all.push(entry);
    }
    let json = JSON.stringify(all);
    let ok = sys.writeFile("music.json", json);
    if (ok) st.musDirty = false;
    status(ok ? "Music saved" : "Music save failed");
}

export function loadMusFromDisk() {
    let json = sys.readFile("music.json");
    if (!json) return;
    let all = JSON.parse(json);
    if (!all || !all.length) return;
    ensurePatterns();
    for (let si = 0; si < all.length; si++) {
        let e = all[si];
        if (e.i >= 0 && e.i < 64) {
            st.musPatterns[e.i].ch = [e.ch[0], e.ch[1], e.ch[2], e.ch[3]];
            st.musPatterns[e.i].flags = e.f || 0;
            if (e.nm) musNames[e.i] = e.nm;
        }
    }
}

// ─── Playback ────────────────────────────────────────────────────────────────

function startMusic() {
    ensurePatterns();
    let pat = st.musPatterns[st.musSel];
    let anyPlaying = false;
    for (let c = 0; c < NUM_CHANNELS; c++) {
        synth.stop(c);
        if (pat.ch[c] >= 0 && !st.musMute[c]) {
            synth.play(pat.ch[c], c);
            anyPlaying = true;
        }
    }
    if (!anyPlaying) {
        status("Pattern empty");
        return;
    }
    st.musPlaying = true;
    st.musPlayRow = st.musSel;
    st.musPlayStart = sys.time();
    status("Playing pattern " + st.musSel);
}

function stopMusic() {
    for (let c = 0; c < NUM_CHANNELS; c++) {
        synth.stop(c);
    }
    st.musPlaying = false;
    status("Stopped");
}

function patternDuration(row) {
    // Longest SFX duration across all channels: speed / 4 seconds
    // (32 notes × speed × (44100/128) samples/note / 44100 Hz)
    let pat = st.musPatterns[row];
    let maxDur = 0;
    for (let c = 0; c < NUM_CHANNELS; c++) {
        if (pat.ch[c] >= 0) {
            let data = synth.get(pat.ch[c]);
            if (data) {
                let dur = (data.speed || 16) / 4;
                if (dur > maxDur) maxDur = dur;
            }
        }
    }
    return maxDur;
}

function playPattern(row) {
    // Start playing a pattern's channels and update play state
    let pat = st.musPatterns[row];
    if (!patternHasData(pat)) {
        stopMusic();
        return;
    }
    let anyPlaying = false;
    for (let c = 0; c < NUM_CHANNELS; c++) {
        synth.stop(c);
        if (pat.ch[c] >= 0 && !st.musMute[c]) {
            synth.play(pat.ch[c], c);
            anyPlaying = true;
        }
    }
    if (!anyPlaying) {
        // All channels muted — still advance time so pattern isn't stuck
        st.musPlayRow = row;
        st.musPlayStart = sys.time();
        ensurePlayRowVisible();
        return;
    }
    st.musPlayRow = row;
    st.musPlayStart = sys.time();
    ensurePlayRowVisible();
}

function advancePattern() {
    // Check the CURRENT pattern's flags (what to do after it finishes)
    let cur = st.musPlayRow;
    let curPat = st.musPatterns[cur];

    if (curPat.flags === FLAG_STOP) {
        stopMusic();
        return;
    }

    let next;
    if (curPat.flags === FLAG_LOOP_BACK) {
        // Find the nearest loop-start above
        next = 0; // default to top if no loop-start found
        for (let i = cur - 1; i >= 0; i--) {
            if (st.musPatterns[i].flags === FLAG_LOOP_START) {
                next = i;
                break;
            }
        }
    } else {
        next = cur + 1;
    }

    if (next >= 64) {
        stopMusic();
        return;
    }

    playPattern(next);
}

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateMusicEditor(dt) {
    ensurePatterns();
    let ctrl = modKey();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    // Track playback advancement (time-based)
    if (st.musPlaying) {
        let dur = patternDuration(st.musPlayRow);
        if (dur > 0 && sys.time() - st.musPlayStart >= dur) {
            advancePattern();
        }
    }

    // Space: play/stop
    if (key.btnp(key.SPACE)) {
        if (st.musPlaying) stopMusic();
        else startMusic();
        return;
    }

    // ── Pattern rename mode ──
    if (st.musRenaming) {
        if (key.btnp(key.ESCAPE)) {
            st.musRenaming = false;
            return;
        }
        if (key.btnp(key.ENTER)) {
            musNames[st.musSel] = st.musRenameTxt;
            st.musRenaming = false;
            st.musDirty = true;
            return;
        }
        if (key.btnp(key.BACKSPACE) && st.musRenameTxt.length > 0) {
            st.musRenameTxt = st.musRenameTxt.slice(0, -1);
            return;
        }
        // Text input handled by onTextInput in main.js
        return;
    }

    // ── Save to disk (Ctrl+S) ──
    if (ctrl && key.btnp(key.S)) {
        saveMusToDisk();
        return;
    }

    // ── Undo (Ctrl+Z) ──
    if (ctrl && !shift && key.btnp(key.Z)) {
        musUndo();
        return;
    }

    // ── Redo (Ctrl+Y or Ctrl+Shift+Z) ──
    if (ctrl && (key.btnp(key.Y) || (shift && key.btnp(key.Z)))) {
        musRedo();
        return;
    }

    // ── Copy pattern (Ctrl+C) ──
    if (ctrl && key.btnp(key.C)) {
        let p = st.musPatterns[st.musSel];
        st.musClipboard = { ch: [p.ch[0], p.ch[1], p.ch[2], p.ch[3]], flags: p.flags };
        status("Copied pattern " + st.musSel);
        return;
    }

    // ── Paste pattern (Ctrl+V) ──
    if (ctrl && key.btnp(key.V)) {
        if (st.musClipboard) {
            pushMusUndo();
            let p = st.musPatterns[st.musSel];
            p.ch = [
                st.musClipboard.ch[0],
                st.musClipboard.ch[1],
                st.musClipboard.ch[2],
                st.musClipboard.ch[3],
            ];
            p.flags = st.musClipboard.flags;
            st.musDirty = true;
            status("Pasted to pattern " + st.musSel);
        } else {
            status("Nothing to paste");
        }
        return;
    }

    // Navigation
    if (key.btnp(key.UP)) {
        if (ctrl) st.musSel = clamp(st.musSel - 8, 0, 63);
        else st.musSel = clamp(st.musSel - 1, 0, 63);
        ensureMusVisible();
        return;
    }
    if (key.btnp(key.DOWN)) {
        if (ctrl) st.musSel = clamp(st.musSel + 8, 0, 63);
        else st.musSel = clamp(st.musSel + 1, 0, 63);
        ensureMusVisible();
        return;
    }
    if (key.btnp(key.LEFT)) {
        st.musCol = clamp(st.musCol - 1, 0, NUM_CHANNELS - 1);
        return;
    }
    if (key.btnp(key.RIGHT)) {
        st.musCol = clamp(st.musCol + 1, 0, NUM_CHANNELS - 1);
        return;
    }

    // Delete: clear channel slot
    if (key.btnp(key.DELETE) || key.btnp(key.BACKSPACE)) {
        pushMusUndo();
        st.musPatterns[st.musSel].ch[st.musCol] = -1;
        st.musDirty = true;
        return;
    }

    // +/-: increment/decrement SFX index
    if (key.btnp(key.EQUALS)) {
        pushMusUndo();
        let cur = st.musPatterns[st.musSel].ch[st.musCol];
        st.musPatterns[st.musSel].ch[st.musCol] = clamp(cur + 1, 0, 63);
        st.musDirty = true;
        return;
    }
    if (key.btnp(key.MINUS)) {
        pushMusUndo();
        let cur = st.musPatterns[st.musSel].ch[st.musCol];
        if (cur < 0) cur = 0;
        st.musPatterns[st.musSel].ch[st.musCol] = clamp(cur - 1, 0, 63);
        st.musDirty = true;
        return;
    }

    // Number keys: quick SFX assignment (0-9 set tens digit, then units)
    for (let i = 0; i <= 9; i++) {
        let k = i === 0 ? key.NUM0 : key.NUM1 + (i - 1);
        if (key.btnp(k)) {
            pushMusUndo();
            let cur = st.musPatterns[st.musSel].ch[st.musCol];
            if (cur < 0) cur = 0;
            // Shift current value left and add digit: e.g. 1 -> 12 -> 23
            let next = (cur % 10) * 10 + i;
            if (next > 63) next = i;
            st.musPatterns[st.musSel].ch[st.musCol] = next;
            st.musDirty = true;
            status("CH" + st.musCol + " \u2192 SFX " + (next < 10 ? "0" : "") + next);
            return;
        }
    }

    // F key: cycle flow flags
    if (key.btnp(key.F)) {
        pushMusUndo();
        let pat = st.musPatterns[st.musSel];
        if (ctrl || shift) {
            // Ctrl+F or Shift+F: clear flag
            pat.flags = FLAG_NONE;
            status("Flag cleared");
        } else {
            pat.flags = (pat.flags + 1) % 4;
            let names = ["none", "loop start", "loop back", "stop"];
            status("Flag: " + names[pat.flags]);
        }
        st.musDirty = true;
        return;
    }

    // Enter: preview the SFX in the selected channel slot
    if (key.btnp(key.ENTER)) {
        let sfxIdx = st.musPatterns[st.musSel].ch[st.musCol];
        if (sfxIdx >= 0) {
            // Stop all voices, then play just this SFX on channel 0
            for (let c = 0; c < NUM_CHANNELS; c++) synth.stop(c);
            synth.play(sfxIdx, 0);
            status("Preview SFX " + (sfxIdx < 10 ? "0" : "") + sfxIdx);
        }
        return;
    }

    // M key: toggle mute on current channel
    if (key.btnp(key.M)) {
        st.musMute[st.musCol] = !st.musMute[st.musCol];
        if (st.musPlaying) {
            if (st.musMute[st.musCol]) {
                synth.stop(st.musCol);
            } else {
                let pat = st.musPatterns[st.musPlayRow];
                if (pat.ch[st.musCol] >= 0) synth.play(pat.ch[st.musCol], st.musCol);
            }
        }
        status("CH" + st.musCol + (st.musMute[st.musCol] ? " muted" : " unmuted"));
        return;
    }

    // S key: solo current channel (mute all others, or unsolo to unmute all)
    if (key.btnp(key.S)) {
        let allOthersMuted = true;
        for (let c = 0; c < NUM_CHANNELS; c++) {
            if (c !== st.musCol && !st.musMute[c]) {
                allOthersMuted = false;
                break;
            }
        }
        if (allOthersMuted && !st.musMute[st.musCol]) {
            // Unsolo: unmute all
            for (let c = 0; c < NUM_CHANNELS; c++) {
                st.musMute[c] = false;
                if (st.musPlaying) {
                    let pat = st.musPatterns[st.musPlayRow];
                    if (pat.ch[c] >= 0) synth.play(pat.ch[c], c);
                }
            }
            status("All channels unmuted");
        } else {
            // Solo: mute all others, unmute this
            for (let c = 0; c < NUM_CHANNELS; c++) {
                if (c === st.musCol) {
                    st.musMute[c] = false;
                    if (st.musPlaying) {
                        let pat = st.musPatterns[st.musPlayRow];
                        if (pat.ch[c] >= 0) synth.play(pat.ch[c], c);
                    }
                } else {
                    st.musMute[c] = true;
                    if (st.musPlaying) synth.stop(c);
                }
            }
            status("Solo CH" + st.musCol);
        }
        return;
    }

    // Tab: jump to SFX editor for the selected channel's SFX
    if (key.btnp(key.TAB)) {
        let sfxIdx = st.musPatterns[st.musSel].ch[st.musCol];
        if (sfxIdx >= 0) {
            st.sfxSel = sfxIdx;
            st.activeTab = TAB_SFX;
            status("SFX " + (sfxIdx < 10 ? "0" : "") + sfxIdx);
        }
        return;
    }

    // Ctrl+Shift+I: insert pattern row (shift rows down)
    if (ctrl && shift && key.btnp(key.I)) {
        pushMusUndo();
        for (let i = 63; i > st.musSel; i--) {
            st.musPatterns[i] = st.musPatterns[i - 1];
        }
        st.musPatterns[st.musSel] = { ch: [-1, -1, -1, -1], flags: FLAG_NONE };
        st.musDirty = true;
        status("Inserted row at " + st.musSel);
        return;
    }

    // Ctrl+Shift+Delete: delete pattern row (shift rows up)
    if (ctrl && shift && (key.btnp(key.DELETE) || key.btnp(key.BACKSPACE))) {
        pushMusUndo();
        for (let i = st.musSel; i < 63; i++) {
            st.musPatterns[i] = st.musPatterns[i + 1];
        }
        st.musPatterns[63] = { ch: [-1, -1, -1, -1], flags: FLAG_NONE };
        st.musDirty = true;
        status("Deleted row " + st.musSel);
        return;
    }

    // N key: rename pattern
    if (!ctrl && key.btnp(key.N)) {
        st.musRenaming = true;
        st.musRenameTxt = musNames[st.musSel] || "";
        return;
    }

    // Mouse
    handleMusMouse();
}

function handleMusMouse() {
    let mx = mouse.x();
    let my = mouse.y();

    let dataY = GRID_Y + ROW_H; // skip channel header row
    if (mouse.btnp(0) && my >= dataY && my < FB_H - FOOT_H) {
        let row = Math.floor((my - dataY) / ROW_H) + st.musScrollY;
        let col = Math.floor((mx - LABEL_W) / COL_W);
        if (row >= 0 && row < 64 && col >= 0 && col < NUM_CHANNELS && mx >= LABEL_W) {
            st.musSel = row;
            st.musCol = col;
        } else if (mx < LABEL_W && row >= 0 && row < 64) {
            st.musSel = row;
        }
        return;
    }

    // Scroll
    let wheel = mouse.wheel();
    if (wheel !== 0) {
        let maxRows = Math.floor((FB_H - GRID_Y - FOOT_H) / ROW_H);
        st.musScrollY = clamp(st.musScrollY - wheel, 0, Math.max(0, 64 - maxRows));
    }
}

function ensureMusVisible() {
    let visRows = Math.floor((FB_H - GRID_Y - FOOT_H) / ROW_H);
    if (st.musSel < st.musScrollY) st.musScrollY = st.musSel;
    if (st.musSel >= st.musScrollY + visRows) st.musScrollY = st.musSel - visRows + 1;
}

function ensurePlayRowVisible() {
    let visRows = Math.floor((FB_H - GRID_Y - FOOT_H - ROW_H) / ROW_H);
    if (st.musPlayRow < st.musScrollY) st.musScrollY = st.musPlayRow;
    if (st.musPlayRow >= st.musScrollY + visRows) st.musScrollY = st.musPlayRow - visRows + 1;
}

// ─── Draw ────────────────────────────────────────────────────────────────────

export function drawMusicEditor() {
    ensurePatterns();

    // Toolbar
    let tbY = TAB_H;
    gfx.rectfill(0, tbY, FB_W - 1, tbY + TOOLBAR_H - 1, FOOTBG);
    gfx.line(0, tbY + TOOLBAR_H - 1, FB_W - 1, tbY + TOOLBAR_H - 1, SEPC);
    let tbLabel = "MUSIC PATTERNS";
    if (st.musRenaming) {
        tbLabel = "NAME: " + st.musRenameTxt + "_";
    } else if (musNames[st.musSel]) {
        tbLabel = "PAT " + (st.musSel < 10 ? "0" : "") + st.musSel + ": " + musNames[st.musSel];
    }
    gfx.print(tbLabel, 4, tbY + 3, st.musRenaming ? 11 : FG);
    if (st.musPlaying) {
        gfx.print("\u25A0 STOP", FB_W - 40, tbY + 3, 8);
    } else {
        gfx.print("\u25B6 PLAY", FB_W - 40, tbY + 3, PLAY_COL);
    }

    // Background
    gfx.rectfill(0, GRID_Y, FB_W - 1, FB_H - FOOT_H - 1, BG);

    // Channel headers
    gfx.rectfill(0, GRID_Y, FB_W - 1, GRID_Y + ROW_H - 1, PANELBG);
    gfx.print("PAT", 4, GRID_Y + 2, GUTFG);
    for (let c = 0; c < NUM_CHANNELS; c++) {
        let cx = LABEL_W + c * COL_W;
        let isSelCol = c === st.musCol;
        let muted = st.musMute[c];
        // Check if this channel is soloed (only this one unmuted)
        let soloed = !muted;
        if (soloed) {
            for (let o = 0; o < NUM_CHANNELS; o++) {
                if (o !== c && !st.musMute[o]) {
                    soloed = false;
                    break;
                }
            }
        }
        gfx.print("CH" + c, cx + 8, GRID_Y + 2, muted ? 17 : isSelCol ? FG : CH_COLS[c]);
        if (soloed) gfx.print("S", cx + COL_W - 12, GRID_Y + 2, 11);
        else if (muted) gfx.print("M", cx + COL_W - 12, GRID_Y + 2, 8);
    }
    // Flag column header
    gfx.print("FLG", LABEL_W + NUM_CHANNELS * COL_W + 4, GRID_Y + 2, GUTFG);

    // Pattern rows
    let visRows = Math.floor((FB_H - GRID_Y - FOOT_H - ROW_H) / ROW_H);
    let dataY = GRID_Y + ROW_H;

    for (let i = 0; i < visRows; i++) {
        let row = i + st.musScrollY;
        if (row >= 64) break;
        let yy = dataY + i * ROW_H;
        let pat = st.musPatterns[row];
        let isSel = row === st.musSel;
        let isPlay = st.musPlaying && row === st.musPlayRow;

        // Row background
        if (isPlay) {
            gfx.rectfill(0, yy, FB_W - 1, yy + ROW_H - 2, 1);
            // Progress bar: filled portion of pattern duration
            let dur = patternDuration(st.musPlayRow);
            if (dur > 0) {
                let frac = Math.min((sys.time() - st.musPlayStart) / dur, 1);
                let pw = Math.floor(frac * (FB_W - 1));
                gfx.rectfill(0, yy + ROW_H - 2, pw, yy + ROW_H - 2, PLAY_COL);
            }
        } else if (isSel) {
            gfx.rectfill(0, yy, FB_W - 1, yy + ROW_H - 2, 17);
        }

        // Pattern number + name
        let label = (row < 10 ? "0" : "") + row;
        let hasData = patternHasData(pat);
        gfx.print(label, 4, yy + 2, isSel ? FG : hasData ? GUTFG : 17);
        if (musNames[row]) {
            gfx.print(musNames[row].slice(0, 3), 18, yy + 2, isSel ? GUTFG : 18);
        }

        // Channel slots
        for (let c = 0; c < NUM_CHANNELS; c++) {
            let cx = LABEL_W + c * COL_W;
            let sfxIdx = pat.ch[c];
            let isCell = isSel && c === st.musCol;

            if (isCell) {
                gfx.rectfill(cx, yy, cx + COL_W - 2, yy + ROW_H - 2, SEL_COL);
            }

            if (sfxIdx >= 0) {
                let txt = (sfxIdx < 10 ? "0" : "") + sfxIdx;
                gfx.print(txt, cx + 10, yy + 2, isCell ? FG : CH_COLS[c]);
            } else {
                gfx.print("--", cx + 10, yy + 2, isCell ? GUTFG : 17);
            }
        }

        // Flow flag
        if (pat.flags > 0) {
            let fx = LABEL_W + NUM_CHANNELS * COL_W + 8;
            gfx.print(FLAG_NAMES[pat.flags], fx, yy + 2, FLAG_COLORS[pat.flags]);
        }

        // Row separator
        gfx.line(0, yy + ROW_H - 1, FB_W - 1, yy + ROW_H - 1, 17);
    }

    // ── Loop bracket visualization ──
    // Draw a vertical bar connecting loop-start to loop-back flags
    let flagX = LABEL_W + NUM_CHANNELS * COL_W + 4;
    let bracketX = flagX + CW * 3 + 2;
    for (let i = 0; i < visRows; i++) {
        let row = i + st.musScrollY;
        if (row >= 64) break;
        if (st.musPatterns[row].flags !== FLAG_LOOP_BACK) continue;
        // Find matching loop-start above
        let startRow = -1;
        for (let j = row - 1; j >= 0; j--) {
            if (st.musPatterns[j].flags === FLAG_LOOP_START) {
                startRow = j;
                break;
            }
        }
        if (startRow < 0) continue;
        let si = startRow - st.musScrollY;
        let ei = row - st.musScrollY;
        if (si < 0) si = 0;
        if (ei >= visRows) ei = visRows - 1;
        let y0 = dataY + si * ROW_H + 2;
        let y1 = dataY + ei * ROW_H + ROW_H - 4;
        gfx.line(bracketX, y0, bracketX, y1, 11);
        gfx.line(bracketX - 2, y0, bracketX, y0, 11);
        gfx.line(bracketX - 2, y1, bracketX, y1, 11);
    }

    // Column separators
    for (let c = 0; c <= NUM_CHANNELS; c++) {
        let cx = LABEL_W + c * COL_W;
        gfx.line(cx, GRID_Y, cx, FB_H - FOOT_H - 1, SEPC);
    }

    // Footer
    let fy = FB_H - FOOT_H;
    gfx.rectfill(0, fy, FB_W - 1, FB_H - 1, FOOTBG);
    gfx.line(0, fy, FB_W - 1, fy, SEPC);
    let ty = fy + 3;
    gfx.print("PAT:" + (st.musSel < 10 ? "0" : "") + st.musSel, 4, ty, FOOTFG);
    gfx.print("CH:" + st.musCol, 56, ty, FOOTFG);
    let sfxIdx = st.musPatterns[st.musSel].ch[st.musCol];
    if (sfxIdx >= 0) {
        gfx.print("SFX:" + (sfxIdx < 10 ? "0" : "") + sfxIdx, 88, ty, FOOTFG);
    }
    if (st.musPlaying) {
        gfx.print("\u25B6 PLAYING", 140, ty, PLAY_COL);
    }
    if (st.msg) {
        let msgW = st.msg.length * CW;
        gfx.print(st.msg, FB_W - msgW - 4, ty, FG);
    }
}
