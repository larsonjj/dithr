// ─── Music Editor ────────────────────────────────────────────────────────────
//
// Pattern-based music sequencer. 64 patterns, each with 4 channel slots
// that reference SFX indices (0-63). Patterns play sequentially and can
// have flow flags: loop-start, loop-back, stop.
//
// NOTE: The C synth engine currently supports only one playback voice,
// so only channel 0 plays during preview. Multi-channel playback would
// require C-side expansion of the synth voice pool.

import { st } from "./state.js";
import {
    FB_W,
    FB_H,
    CW,
    CH,
    TAB_H,
    FG,
    BG,
    PANELBG,
    SEPC,
    SELBG,
    GUTFG,
    GUTBG,
    FOOTBG,
    FOOTFG,
} from "./config.js";
import { clamp, modKey, status } from "./helpers.js";

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

// ─── Playback ────────────────────────────────────────────────────────────────

function startMusic() {
    ensurePatterns();
    let pat = st.musPatterns[st.musSel];
    let sfxIdx = pat.ch[0];
    if (sfxIdx < 0) {
        status("Channel 0 empty");
        return;
    }
    synth.play(sfxIdx, 0);
    st.musPlaying = true;
    st.musPlayRow = st.musSel;
    st.musPlayStart = sys.time();
    status("Playing pattern " + st.musSel);
}

function stopMusic() {
    synth.stop(0);
    st.musPlaying = false;
    status("Stopped");
}

function advancePattern() {
    // Called when current SFX finishes — advance to next pattern
    let next = st.musPlayRow + 1;
    if (next >= 64) {
        stopMusic();
        return;
    }

    let pat = st.musPatterns[next];

    // Check flags
    if (pat.flags === FLAG_STOP) {
        stopMusic();
        return;
    }
    if (pat.flags === FLAG_LOOP_BACK) {
        // Find the loop start
        for (let i = next - 1; i >= 0; i--) {
            if (st.musPatterns[i].flags === FLAG_LOOP_START) {
                next = i;
                pat = st.musPatterns[next];
                break;
            }
        }
    }

    let sfxIdx = pat.ch[0];
    if (sfxIdx < 0) {
        stopMusic();
        return;
    }
    synth.play(sfxIdx, 0);
    st.musPlayRow = next;
    st.musPlayStart = sys.time();
}

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateMusicEditor(dt) {
    ensurePatterns();
    let ctrl = modKey();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    // Track playback advancement
    if (st.musPlaying) {
        if (!synth.playing()) {
            advancePattern();
        }
    }

    // Space: play/stop
    if (key.btnp(key.SPACE)) {
        if (st.musPlaying) stopMusic();
        else startMusic();
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
        st.musPatterns[st.musSel].ch[st.musCol] = -1;
        return;
    }

    // +/-: increment/decrement SFX index
    if (key.btnp(key.EQUALS)) {
        let cur = st.musPatterns[st.musSel].ch[st.musCol];
        st.musPatterns[st.musSel].ch[st.musCol] = clamp(cur + 1, 0, 63);
        return;
    }
    if (key.btnp(key.MINUS)) {
        let cur = st.musPatterns[st.musSel].ch[st.musCol];
        if (cur < 0) cur = 0;
        st.musPatterns[st.musSel].ch[st.musCol] = clamp(cur - 1, 0, 63);
        return;
    }

    // Number keys: quick SFX assignment (0-9 set tens digit, then units)
    for (let i = 0; i <= 9; i++) {
        let k = i === 0 ? key.NUM0 : key.NUM1 + (i - 1);
        if (key.btnp(k)) {
            let cur = st.musPatterns[st.musSel].ch[st.musCol];
            if (cur < 0) cur = 0;
            // Shift current value left and add digit: e.g. 1 -> 12 -> 23
            let next = (cur % 10) * 10 + i;
            if (next > 63) next = i;
            st.musPatterns[st.musSel].ch[st.musCol] = next;
            return;
        }
    }

    // F key: cycle flow flags
    if (key.btnp(key.F)) {
        let pat = st.musPatterns[st.musSel];
        pat.flags = (pat.flags + 1) % 4;
        let names = ["none", "loop start", "loop back", "stop"];
        status("Flag: " + names[pat.flags]);
        return;
    }

    // Mouse
    handleMusMouse();
}

function handleMusMouse() {
    let mx = mouse.x();
    let my = mouse.y();

    if (mouse.btnp(0) && my >= GRID_Y && my < FB_H - FOOT_H) {
        let row = Math.floor((my - GRID_Y) / ROW_H) + st.musScrollY;
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

// ─── Draw ────────────────────────────────────────────────────────────────────

export function drawMusicEditor() {
    ensurePatterns();

    // Toolbar
    let tbY = TAB_H;
    gfx.rectfill(0, tbY, FB_W - 1, tbY + TOOLBAR_H - 1, FOOTBG);
    gfx.line(0, tbY + TOOLBAR_H - 1, FB_W - 1, tbY + TOOLBAR_H - 1, SEPC);
    gfx.print("MUSIC PATTERNS", 4, tbY + 3, FG);
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
        gfx.print("CH" + c, cx + 8, GRID_Y + 2, isSelCol ? FG : CH_COLS[c]);
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
        } else if (isSel) {
            gfx.rectfill(0, yy, FB_W - 1, yy + ROW_H - 2, 17);
        }

        // Pattern number
        let label = (row < 10 ? "0" : "") + row;
        let hasData = patternHasData(pat);
        gfx.print(label, 4, yy + 2, isSel ? FG : hasData ? GUTFG : 17);

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
