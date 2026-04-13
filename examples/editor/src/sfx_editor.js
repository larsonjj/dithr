// ─── SFX Editor ──────────────────────────────────────────────────────────────
//
// Chip-tune synthesizer editor. Each SFX is 32 notes with pitch, waveform,
// volume, and effect per note. Uses the C-side synth engine for PCM rendering
// and preview playback via the synth.* JS API.

import { st } from './state.js';
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
    GUTFG,
    FOOTBG,
    FOOTFG,
} from './config.js';
import { clamp, modKey, status } from './helpers.js';

// ─── Layout constants ────────────────────────────────────────────────────────

const LIST_W = 70; // SFX list panel width
const FOOT_H = 14; // footer height
const TOOLBAR_H = 14; // SFX toolbar height (nav + play buttons)
const GRID_X = LIST_W + 1; // note grid left edge
const GRID_Y = TAB_H; // note grid top edge
const GRID_W = FB_W - GRID_X; // note grid total width
const NOTE_W = 18; // column width per note
const ROW_H = 14; // row height for each field
const HEADER_H = 12; // column header height
const FIELD_H = ROW_H * 5; // total height of the 5 field rows
const LABEL_W = 30; // row label width
const VISIBLE_NOTES = Math.floor((GRID_W - LABEL_W) / NOTE_W); // notes visible

// Octave mapping — display octave = internal octave - OCT_OFFSET
// Display 1-4 corresponds to internal octaves 3-6
const OCT_OFFSET = 2;
const MIN_INT_OCT = 3; // internal octave for display octave 1
const MAX_INT_OCT = 6; // internal octave for display octave 4
const MIN_PITCH = MIN_INT_OCT * 12 + 1; // 37 = C at internal octave 3
const MAX_PITCH = (MAX_INT_OCT + 1) * 12; // 84 = B at internal octave 6

// Colors
const NOTE_COL = 12; // cyan for note names
const WAVE_COL = 11; // green for waveform
const VOL_COL = 9; // orange for volume
const FX_COL = 14; // pink for effects
const REST_COL = 17; // dim for rests
const SEL_COL = 2; // selection highlight
const LOOP_COL = 3; // loop marker color
const PLAY_COL = 11; // playback cursor color

// Waveform colors for pitch graph caps
const WAVE_COLORS = [
    9, // triangle = orange
    15, // tiltsaw = peach
    8, // saw = red
    11, // square = green
    14, // pulse = pink
    13, // organ = lavender
    6, // noise = light gray
    10, // phaser = yellow
];

// ─── SFX list hasData cache ──────────────────────────────────────────────────

let sfxHasData = null;
let sfxSparkCache = null; // cached pitch arrays per SFX (null = needs rebuild)
let cachedWaveNames = null;
let cachedFxNames = null;

// Per-SFX names (editor-only, stored in sfx.json)
export const sfxNames = new Array(64).fill('');

function getWaveNames() {
    if (!cachedWaveNames) cachedWaveNames = synth.waveNames();
    return cachedWaveNames;
}

function getFxNames() {
    if (!cachedFxNames) cachedFxNames = synth.fxNames();
    return cachedFxNames;
}

function initHasDataCache() {
    sfxHasData = new Array(64);
    sfxSparkCache = new Array(64);
    for (let i = 0; i < 64; i++) {
        const data = synth.get(i);
        sfxHasData[i] = false;
        sfxSparkCache[i] = null;
        if (data) {
            for (let n = 0; n < 32; n++) {
                if (data.notes[n].pitch > 0) {
                    sfxHasData[i] = true;
                    break;
                }
            }
        }
    }
}

function markSfxHasData(idx, knownTrue) {
    if (!sfxHasData) return;
    if (sfxSparkCache) sfxSparkCache[idx] = null; // invalidate sparkline cache
    if (knownTrue) {
        sfxHasData[idx] = true;
        return;
    }
    const data = synth.get(idx);
    sfxHasData[idx] = false;
    if (data) {
        for (let n = 0; n < 32; n++) {
            if (data.notes[n].pitch > 0) {
                sfxHasData[idx] = true;
                break;
            }
        }
    }
}

// ─── Data helpers ────────────────────────────────────────────────────────────

/** Load SFX data from C into a JS array of note objects. */
function loadSfx(idx) {
    const data = synth.get(idx);
    if (data) {
        // Clamp speed to valid range (C struct may be zero-initialized)
        data.speed = clamp(data.speed || 16, 1, 32);
        return data;
    }
    // Return default empty SFX
    const notes = [];
    for (let i = 0; i < 32; i++) notes.push({ pitch: 0, waveform: 0, volume: 0, effect: 0 });
    return { notes, speed: 16, loopStart: 0, loopEnd: 0 };
}

/** Push current SFX data to the C synth engine. */
function saveSfx(idx, data) {
    synth.set(idx, data.notes, data.speed, data.loopStart, data.loopEnd);
}

/** Save all SFX data to disk as JSON. */
export function saveSfxToDisk() {
    const all = [];
    for (let i = 0; i < 64; i++) {
        const data = synth.get(i);
        if (!data) continue;
        let hasNotes = false;
        for (let n = 0; n < 32; n++) {
            if (data.notes[n].pitch > 0) {
                hasNotes = true;
                break;
            }
        }
        if (!hasNotes && !data.speed && !data.loopStart && !data.loopEnd) continue;
        const notes = [];
        for (let n = 0; n < 32; n++) {
            const nt = data.notes[n];
            notes.push([nt.pitch, nt.waveform, nt.volume, nt.effect]);
        }
        const entry = { i, n: notes, s: data.speed, ls: data.loopStart, le: data.loopEnd };
        if (sfxNames[i]) entry.nm = sfxNames[i];
        all.push(entry);
    }
    const json = JSON.stringify(all);
    const ok = sys.writeFile('sfx.json', json);
    if (ok) st.sfxDirty = false;
    status(ok ? 'SFX saved' : 'SFX save failed');
}

/** Load SFX data from disk. Called once during _init. */
export function loadSfxFromDisk() {
    const json = sys.readFile('sfx.json');
    if (!json) return;
    const all = JSON.parse(json);
    if (!all || !all.length) return;
    for (let si = 0; si < all.length; si++) {
        const entry = all[si];
        const notes = [];
        for (let n = 0; n < 32; n++) {
            const src = entry.n[n];
            notes.push({ pitch: src[0], waveform: src[1], volume: src[2], effect: src[3] });
        }
        synth.set(entry.i, notes, entry.s, entry.ls, entry.le);
        if (entry.nm) sfxNames[entry.i] = entry.nm;
    }
    sfxHasData = null; // force cache rebuild
}

/** Get the currently loaded SFX data. */
function curSfx() {
    return loadSfx(st.sfxSel);
}

/** Modify a note field and push to C. Accepts optional pre-loaded data
 *  to avoid redundant JS→C roundtrips when caller already has it. */
function setNoteField(noteIdx, field, value, data) {
    if (!data) data = curSfx();
    data.notes[noteIdx][field] = value;
    saveSfx(st.sfxSel, data);
    markSfxHasData(st.sfxSel);
    st.sfxDirty = true;
}

/** Generate a pitch value from note name index (0-11) and internal octave. */
function makePitch(noteInOctave, octave) {
    return clamp(octave, MIN_INT_OCT, MAX_INT_OCT) * 12 + noteInOctave + 1;
}

/** Nudge the current field value up or down by `dir` (+1 or -1). */
function nudgeField(dir) {
    pushSfxUndo();
    const data = curSfx();
    const note = data.notes[st.sfxNote];
    if (st.sfxField === 0) {
        let p = note.pitch + dir;
        if (p < 0) p = 0;
        if (p > 0 && p < MIN_PITCH) p = dir > 0 ? MIN_PITCH : 0;
        if (p > MAX_PITCH) p = MAX_PITCH;
        setNoteField(st.sfxNote, 'pitch', p, data);
    } else if (st.sfxField === 1) {
        setNoteField(st.sfxNote, 'waveform', clamp(note.waveform + dir, 0, 7), data);
    } else if (st.sfxField === 2) {
        setNoteField(st.sfxNote, 'volume', clamp(note.volume + dir, 0, 7), data);
    } else if (st.sfxField === 3) {
        setNoteField(st.sfxNote, 'effect', clamp(note.effect + dir, 0, 7), data);
    } else if (st.sfxField === 4) {
        if (note.pitch > 0) {
            const noteInOct = (note.pitch - 1) % 12;
            const curOct = Math.floor((note.pitch - 1) / 12);
            const newOct = clamp(curOct + dir, MIN_INT_OCT, MAX_INT_OCT);
            setNoteField(st.sfxNote, 'pitch', makePitch(noteInOct, newOct), data);
        }
    }
}

/** Get the currently playing note index, or -1 if not playing. */
function getPlaybackNote() {
    if (!st.sfxPlaying) return -1;
    return synth.noteIdx();
}

// ─── Undo / Redo ─────────────────────────────────────────────────────────────

const MAX_SFX_UNDO = 50;

function cloneSfx(data) {
    const notes = [];
    for (let i = 0; i < 32; i++) {
        const n = data.notes[i];
        notes.push({ pitch: n.pitch, waveform: n.waveform, volume: n.volume, effect: n.effect });
    }
    return { notes, speed: data.speed, loopStart: data.loopStart, loopEnd: data.loopEnd };
}

function pushSfxUndo() {
    const snap = cloneSfx(curSfx());
    st.sfxUndoStack.push(snap);
    if (st.sfxUndoStack.length > MAX_SFX_UNDO) st.sfxUndoStack.shift();
    st.sfxRedoStack = [];
}

function sfxUndo() {
    if (st.sfxUndoStack.length === 0) {
        status('Nothing to undo');
        return;
    }
    st.sfxRedoStack.push(cloneSfx(curSfx()));
    const prev = st.sfxUndoStack.pop();
    saveSfx(st.sfxSel, prev);
    st.sfxSpeed = prev.speed;
    st.sfxLoopStart = prev.loopStart;
    st.sfxLoopEnd = prev.loopEnd;
    markSfxHasData(st.sfxSel);
    st.sfxDirty = true;
    status('Undo');
}

function sfxRedo() {
    if (st.sfxRedoStack.length === 0) {
        status('Nothing to redo');
        return;
    }
    st.sfxUndoStack.push(cloneSfx(curSfx()));
    const next = st.sfxRedoStack.pop();
    saveSfx(st.sfxSel, next);
    st.sfxSpeed = next.speed;
    st.sfxLoopStart = next.loopStart;
    st.sfxLoopEnd = next.loopEnd;
    markSfxHasData(st.sfxSel);
    st.sfxDirty = true;
    status('Redo');
}

// ─── Selection helpers ───────────────────────────────────────────────────────

function hasSelection() {
    return st.sfxSelStart >= 0 && st.sfxSelEnd >= 0;
}

function selMin() {
    return Math.min(st.sfxSelStart, st.sfxSelEnd);
}
function selMax() {
    return Math.max(st.sfxSelStart, st.sfxSelEnd);
}

function clearSelection() {
    st.sfxSelStart = -1;
    st.sfxSelEnd = -1;
}

// Key-to-note mapping (piano keyboard layout)
// Z=C, S=C#, X=D, D=D#, C=E, V=F, G=F#, B=G, H=G#, N=A, J=A#, M=B
const PIANO_KEYS = [
    { key: 'Z', note: 0 },
    { key: 'S', note: 1 },
    { key: 'X', note: 2 },
    { key: 'D', note: 3 },
    { key: 'C', note: 4 },
    { key: 'V', note: 5 },
    { key: 'G', note: 6 },
    { key: 'B', note: 7 },
    { key: 'H', note: 8 },
    { key: 'N', note: 9 },
    { key: 'J', note: 10 },
    { key: 'M', note: 11 },
];

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateSfxEditor(dt) {
    const ctrl = modKey();
    const shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    // ── Play/stop preview (only on first press, ignore key repeat) ──
    if (key.btnp(key.SPACE) && !st.sfxSpaceHeld) {
        st.sfxSpaceHeld = true;
        if (st.sfxPlaying) {
            synth.stop(0);
            st.sfxPlaying = false;
        } else {
            const data = curSfx();
            saveSfx(st.sfxSel, data);
            synth.play(st.sfxSel, 0);
            st.sfxPlaying = true;
            st.sfxPlayStart = sys.time();
        }
        return;
    }
    if (!key.btn(key.SPACE)) {
        st.sfxSpaceHeld = false;
    }

    // Stop tracking when playback finishes
    if (st.sfxPlaying && !synth.playing()) {
        st.sfxPlaying = false;
    }

    // Init hasData cache on first frame
    if (!sfxHasData) initHasDataCache();

    // ── SFX rename mode ──
    if (st.sfxRenaming) {
        if (key.btnp(key.ESCAPE)) {
            st.sfxRenaming = false;
            return;
        }
        if (key.btnp(key.ENTER)) {
            sfxNames[st.sfxSel] = st.sfxRenameTxt;
            st.sfxRenaming = false;
            st.sfxDirty = true;
            return;
        }
        if (key.btnp(key.BACKSPACE) && st.sfxRenameTxt.length > 0) {
            st.sfxRenameTxt = st.sfxRenameTxt.slice(0, -1);
            return;
        }
        // Text input handled by onTextInput in main.js
        return;
    }

    // ── Save to disk (Ctrl+S) ──
    if (ctrl && !shift && key.btnp(key.S)) {
        saveSfxToDisk();
        return;
    }

    // ── Export WAV (Ctrl+Shift+E) ──
    if (ctrl && shift && key.btnp(key.E)) {
        const name = `sfx_${st.sfxSel < 10 ? '0' : ''}${st.sfxSel}.wav`;
        const ok = synth.exportWav(st.sfxSel, name);
        status(ok ? `Exported ${name}` : 'Export failed');
        return;
    }

    // ── Rename SFX (N) ──
    if (!ctrl && key.btnp(key.N)) {
        st.sfxRenaming = true;
        st.sfxRenameTxt = sfxNames[st.sfxSel] || '';
        return;
    }

    // ── Undo (Ctrl+Z) ──
    if (ctrl && !shift && key.btnp(key.Z)) {
        sfxUndo();
        return;
    }

    // ── Redo (Ctrl+Y or Ctrl+Shift+Z) ──
    if (ctrl && (key.btnp(key.Y) || (shift && key.btnp(key.Z)))) {
        sfxRedo();
        return;
    }

    // ── Copy (Ctrl+C) — range if selection, else full SFX ──
    if (ctrl && key.btnp(key.C)) {
        const data = curSfx();
        if (hasSelection()) {
            const lo = selMin();
            const hi = selMax();
            const clip = [];
            for (let i = lo; i <= hi; i++) {
                const n = data.notes[i];
                clip.push({
                    pitch: n.pitch,
                    waveform: n.waveform,
                    volume: n.volume,
                    effect: n.effect,
                });
            }
            st.sfxNoteClip = clip;
            status(`Copied notes ${lo}-${hi}`);
        } else {
            const notesCopy = [];
            for (let i = 0; i < 32; i++) {
                const n = data.notes[i];
                notesCopy.push({
                    pitch: n.pitch,
                    waveform: n.waveform,
                    volume: n.volume,
                    effect: n.effect,
                });
            }
            st.sfxClipboard = {
                notes: notesCopy,
                speed: data.speed,
                loopStart: data.loopStart,
                loopEnd: data.loopEnd,
            };
            st.sfxNoteClip = null;
            status(`Copied SFX ${st.sfxSel}`);
        }
        return;
    }

    // ── Paste (Ctrl+V) — range if note clip, else full SFX ──
    if (ctrl && key.btnp(key.V)) {
        if (st.sfxNoteClip) {
            pushSfxUndo();
            const data = curSfx();
            for (let i = 0; i < st.sfxNoteClip.length && st.sfxNote + i < 32; i++) {
                const src = st.sfxNoteClip[i];
                const dst = data.notes[st.sfxNote + i];
                dst.pitch = src.pitch;
                dst.waveform = src.waveform;
                dst.volume = src.volume;
                dst.effect = src.effect;
            }
            saveSfx(st.sfxSel, data);
            markSfxHasData(st.sfxSel);
            st.sfxDirty = true;
            status(`Pasted ${st.sfxNoteClip.length} notes at ${st.sfxNote}`);
        } else if (st.sfxClipboard) {
            pushSfxUndo();
            saveSfx(st.sfxSel, st.sfxClipboard);
            st.sfxSpeed = st.sfxClipboard.speed;
            st.sfxLoopStart = st.sfxClipboard.loopStart;
            st.sfxLoopEnd = st.sfxClipboard.loopEnd;
            markSfxHasData(st.sfxSel);
            st.sfxDirty = true;
            status(`Pasted to SFX ${st.sfxSel}`);
        } else {
            status('Nothing to paste');
        }
        return;
    }

    // ── Select all (Ctrl+A) ──
    if (ctrl && key.btnp(key.A)) {
        st.sfxSelStart = 0;
        st.sfxSelEnd = 31;
        st.sfxNote = 31;
        status('Selected all notes');
        return;
    }

    // ── Duplicate SFX to next empty slot (Ctrl+D) ──
    if (ctrl && key.btnp(key.D)) {
        const data = curSfx();
        let target = -1;
        for (let i = st.sfxSel + 1; i < 64; i++) {
            if (!sfxHasData || !sfxHasData[i]) {
                target = i;
                break;
            }
        }
        if (target < 0) {
            for (let i = 0; i < st.sfxSel; i++) {
                if (!sfxHasData || !sfxHasData[i]) {
                    target = i;
                    break;
                }
            }
        }
        if (target >= 0) {
            saveSfx(target, cloneSfx(data));
            markSfxHasData(target, true);
            st.sfxDirty = true;
            status(`Duplicated SFX ${st.sfxSel} → ${target}`);
        } else {
            status('No empty SFX slot');
        }
        return;
    }

    // ── SFX list navigation ──
    if (ctrl && key.btnp(key.UP)) {
        st.sfxSel = clamp(st.sfxSel - 1, 0, 63);
        st.sfxUndoStack = [];
        st.sfxRedoStack = [];
        clearSelection();
        ensureSfxVisible();
        return;
    }
    if (ctrl && key.btnp(key.DOWN)) {
        st.sfxSel = clamp(st.sfxSel + 1, 0, 63);
        st.sfxUndoStack = [];
        st.sfxRedoStack = [];
        clearSelection();
        ensureSfxVisible();
        return;
    }

    // ── Note cursor movement ──
    // Shift+Left/Right: extend selection
    if (!ctrl && shift && key.btnp(key.LEFT)) {
        if (!hasSelection()) {
            st.sfxSelStart = st.sfxNote;
            st.sfxSelEnd = st.sfxNote;
        }
        st.sfxNote = clamp(st.sfxNote - 1, 0, 31);
        st.sfxSelEnd = st.sfxNote;
        ensureNoteVisible();
        return;
    }
    if (!ctrl && shift && key.btnp(key.RIGHT)) {
        if (!hasSelection()) {
            st.sfxSelStart = st.sfxNote;
            st.sfxSelEnd = st.sfxNote;
        }
        st.sfxNote = clamp(st.sfxNote + 1, 0, 31);
        st.sfxSelEnd = st.sfxNote;
        ensureNoteVisible();
        return;
    }
    if (!ctrl && !shift && key.btnp(key.LEFT)) {
        clearSelection();
        st.sfxNote = clamp(st.sfxNote - 1, 0, 31);
        ensureNoteVisible();
        return;
    }
    if (!ctrl && !shift && key.btnp(key.RIGHT)) {
        clearSelection();
        st.sfxNote = clamp(st.sfxNote + 1, 0, 31);
        ensureNoteVisible();
        return;
    }

    // ── Home/End keys ──
    if (key.btnp(key.HOME)) {
        clearSelection();
        st.sfxNote = 0;
        ensureNoteVisible();
        return;
    }
    if (key.btnp(key.END)) {
        clearSelection();
        st.sfxNote = 31;
        ensureNoteVisible();
        return;
    }
    if (!ctrl && !shift && key.btnp(key.UP)) {
        st.sfxField = clamp(st.sfxField - 1, 0, 4);
        return;
    }
    if (!ctrl && !shift && key.btnp(key.DOWN)) {
        st.sfxField = clamp(st.sfxField + 1, 0, 4);
        return;
    }

    // ── Value nudging (Shift+Up/Down) ──
    if (!ctrl && shift && key.btnp(key.UP)) {
        nudgeField(1);
        return;
    }
    if (!ctrl && shift && key.btnp(key.DOWN)) {
        nudgeField(-1);
        return;
    }

    // ── Tab to cycle field ──
    if (key.btnp(key.TAB)) {
        st.sfxField = (st.sfxField + 1) % 5;
        return;
    }

    // ── Speed adjustment ──
    if (key.btnp(key.MINUS)) {
        pushSfxUndo();
        const data = curSfx();
        data.speed = clamp(data.speed - 1, 1, 32);
        st.sfxSpeed = data.speed;
        saveSfx(st.sfxSel, data);
        st.sfxDirty = true;
        return;
    }
    if (key.btnp(key.EQUALS)) {
        pushSfxUndo();
        const data = curSfx();
        data.speed = clamp(data.speed + 1, 1, 32);
        st.sfxSpeed = data.speed;
        saveSfx(st.sfxSel, data);
        st.sfxDirty = true;
        return;
    }

    // ── Octave adjustment ([ and ] change selected note's octave) ──
    if (key.btnp(key.LEFTBRACKET)) {
        const data = curSfx();
        const note = data.notes[st.sfxNote];
        if (note.pitch > 0) {
            pushSfxUndo();
            const noteInOct = (note.pitch - 1) % 12;
            const curOct = Math.floor((note.pitch - 1) / 12);
            const newOct = clamp(curOct - 1, MIN_INT_OCT, MAX_INT_OCT);
            setNoteField(st.sfxNote, 'pitch', makePitch(noteInOct, newOct), data);
            status(`Octave: ${newOct - OCT_OFFSET}`);
        }
        return;
    }
    if (key.btnp(key.RIGHTBRACKET)) {
        const data = curSfx();
        const note = data.notes[st.sfxNote];
        if (note.pitch > 0) {
            pushSfxUndo();
            const noteInOct = (note.pitch - 1) % 12;
            const curOct = Math.floor((note.pitch - 1) / 12);
            const newOct = clamp(curOct + 1, MIN_INT_OCT, MAX_INT_OCT);
            setNoteField(st.sfxNote, 'pitch', makePitch(noteInOct, newOct), data);
            status(`Octave: ${newOct - OCT_OFFSET}`);
        }
        return;
    }

    // ── Waveform cycle (W key) ──
    if (key.btnp(key.W)) {
        if (st.sfxField === 1) {
            pushSfxUndo();
            const data = curSfx();
            if (hasSelection()) {
                const lo = selMin();
                const hi = selMax();
                const nw = (data.notes[st.sfxNote].waveform + 1) % 8;
                for (let i = lo; i <= hi; i++) data.notes[i].waveform = nw;
                saveSfx(st.sfxSel, data);
                st.sfxDirty = true;
                status(`Wave ${getWaveNames()[nw]} (${lo}-${hi})`);
            } else {
                const nw = (data.notes[st.sfxNote].waveform + 1) % 8;
                setNoteField(st.sfxNote, 'waveform', nw, data);
                status(`Note wave: ${getWaveNames()[nw]}`);
            }
        } else {
            st.sfxWave = (st.sfxWave + 1) % 8;
            status(`Brush wave: ${getWaveNames()[st.sfxWave]}`);
        }
        return;
    }

    // ── Effect cycle (E key) ──
    if (key.btnp(key.E)) {
        if (st.sfxField === 3) {
            pushSfxUndo();
            const data = curSfx();
            if (hasSelection()) {
                const lo = selMin();
                const hi = selMax();
                const nf = (data.notes[st.sfxNote].effect + 1) % 8;
                for (let i = lo; i <= hi; i++) data.notes[i].effect = nf;
                saveSfx(st.sfxSel, data);
                st.sfxDirty = true;
                status(`FX ${getFxNames()[nf]} (${lo}-${hi})`);
            } else {
                const nf = (data.notes[st.sfxNote].effect + 1) % 8;
                setNoteField(st.sfxNote, 'effect', nf, data);
                status(`Note fx: ${getFxNames()[nf]}`);
            }
        } else {
            st.sfxFx = (st.sfxFx + 1) % 8;
            status(`Brush fx: ${getFxNames()[st.sfxFx]}`);
        }
        return;
    }

    // ── Volume / Octave direct set via number keys ──
    for (let i = 0; i <= 7; i++) {
        if (key.btnp(key.NUM1 + i)) {
            if (st.sfxField === 2) {
                pushSfxUndo();
                if (hasSelection()) {
                    const data = curSfx();
                    const lo = selMin();
                    const hi = selMax();
                    for (let j = lo; j <= hi; j++) data.notes[j].volume = i;
                    saveSfx(st.sfxSel, data);
                    st.sfxDirty = true;
                    status(`Vol ${i} (${lo}-${hi})`);
                } else {
                    setNoteField(st.sfxNote, 'volume', i);
                }
            } else if (st.sfxField === 4) {
                const data = curSfx();
                const note = data.notes[st.sfxNote];
                if (note.pitch > 0) {
                    pushSfxUndo();
                    const intOct = clamp(i + 1 + OCT_OFFSET, MIN_INT_OCT, MAX_INT_OCT);
                    const noteInOct = (note.pitch - 1) % 12;
                    setNoteField(st.sfxNote, 'pitch', makePitch(noteInOct, intOct), data);
                    status(`Octave: ${intOct - OCT_OFFSET}`);
                }
            } else {
                st.sfxVol = i;
                status(`Volume: ${i}`);
            }
            return;
        }
    }

    // ── Delete note (or selection range) ──
    if (key.btnp(key.DELETE) || key.btnp(key.BACKSPACE)) {
        pushSfxUndo();
        const data = curSfx();
        if (hasSelection()) {
            const lo = selMin();
            const hi = selMax();
            for (let i = lo; i <= hi; i++) {
                data.notes[i].pitch = 0;
                data.notes[i].waveform = 0;
                data.notes[i].volume = 0;
                data.notes[i].effect = 0;
            }
            saveSfx(st.sfxSel, data);
            markSfxHasData(st.sfxSel);
            st.sfxDirty = true;
            clearSelection();
            status(`Cleared notes ${lo}-${hi}`);
        } else if (st.sfxField === 0) {
            data.notes[st.sfxNote].pitch = 0;
            data.notes[st.sfxNote].volume = 0;
            saveSfx(st.sfxSel, data);
            markSfxHasData(st.sfxSel);
            st.sfxDirty = true;
        } else if (st.sfxField === 1) {
            setNoteField(st.sfxNote, 'waveform', 0, data);
        } else if (st.sfxField === 2) {
            setNoteField(st.sfxNote, 'volume', 0, data);
        } else {
            setNoteField(st.sfxNote, 'effect', 0, data);
        }
        return;
    }

    // ── Transpose selection (Ctrl+Shift+Up/Down) ──
    if (ctrl && shift && hasSelection() && (key.btnp(key.UP) || key.btnp(key.DOWN))) {
        const dir = key.btnp(key.UP) ? 1 : -1;
        pushSfxUndo();
        const data = curSfx();
        const lo = selMin();
        const hi = selMax();
        for (let i = lo; i <= hi; i++) {
            const note = data.notes[i];
            if (note.pitch > 0) {
                const p = note.pitch + dir;
                if (p >= MIN_PITCH && p <= MAX_PITCH) note.pitch = p;
            }
        }
        saveSfx(st.sfxSel, data);
        markSfxHasData(st.sfxSel);
        st.sfxDirty = true;
        status(`Transpose ${dir > 0 ? '+1' : '-1'} (${lo}-${hi})`);
        return;
    }

    // ── Loop markers (Ctrl+L for start, Ctrl+Shift+L for end) ──
    if (ctrl && key.btnp(key.L)) {
        pushSfxUndo();
        const data = curSfx();
        if (shift) {
            // Ctrl+Shift+L: set loop end (toggle-clear if on existing end)
            if (data.loopEnd === st.sfxNote + 1) {
                data.loopEnd = 0;
                data.loopStart = 0;
                st.sfxLoopEnd = 0;
                st.sfxLoopStart = 0;
                status('Loop cleared');
            } else {
                data.loopEnd = st.sfxNote + 1;
                st.sfxLoopEnd = st.sfxNote + 1;
                // Validate: swap if start > end
                if (data.loopStart > 0 && data.loopStart >= data.loopEnd) {
                    const tmp = data.loopStart;
                    data.loopStart = data.loopEnd;
                    data.loopEnd = tmp;
                    st.sfxLoopStart = data.loopStart;
                    st.sfxLoopEnd = data.loopEnd;
                }
                status(`Loop end: ${st.sfxNote + 1}`);
            }
        } else if (data.loopStart === st.sfxNote && data.loopEnd > 0) {
            // Ctrl+L: set loop start (toggle-clear if on existing start)
            data.loopEnd = 0;
            data.loopStart = 0;
            st.sfxLoopEnd = 0;
            st.sfxLoopStart = 0;
            status('Loop cleared');
        } else {
            data.loopStart = st.sfxNote;
            st.sfxLoopStart = st.sfxNote;
            // Validate: swap if start > end
            if (data.loopEnd > 0 && data.loopStart > data.loopEnd) {
                const tmp = data.loopStart;
                data.loopStart = data.loopEnd;
                data.loopEnd = tmp;
                st.sfxLoopStart = data.loopStart;
                st.sfxLoopEnd = data.loopEnd;
            }
            status(`Loop start: ${st.sfxNote}`);
        }
        saveSfx(st.sfxSel, data);
        st.sfxDirty = true;
        return;
    }

    // ── Piano keyboard input ──
    if (st.sfxField === 0 || st.sfxField === 4) {
        for (let i = 0; i < PIANO_KEYS.length; i++) {
            const pk = PIANO_KEYS[i];
            if (key.btnp(key[pk.key])) {
                pushSfxUndo();
                const data = curSfx();
                const curNote = data.notes[st.sfxNote];
                const isNew = curNote.pitch === 0;
                const oct = curNote.pitch > 0 ? Math.floor((curNote.pitch - 1) / 12) : MIN_INT_OCT;
                const pitch = makePitch(pk.note, oct);
                data.notes[st.sfxNote].pitch = pitch;
                if (isNew) {
                    data.notes[st.sfxNote].waveform = st.sfxWave;
                    data.notes[st.sfxNote].volume = st.sfxVol;
                    data.notes[st.sfxNote].effect = st.sfxFx;
                }
                saveSfx(st.sfxSel, data);
                markSfxHasData(st.sfxSel, true);
                st.sfxDirty = true;
                // Auto-advance cursor
                if (st.sfxNote < 31) {
                    st.sfxNote++;
                    ensureNoteVisible();
                }
                if (st.sfxPlaying) {
                    // Resume pattern playback with the edit applied
                    const elapsed = sys.time() - st.sfxPlayStart;
                    const sampleOff = Math.max(0, Math.floor(elapsed * 44100));
                    synth.play(st.sfxSel, 0, sampleOff);
                } else {
                    // Preview just the entered note
                    synth.playNote(pitch, st.sfxWave, st.sfxVol, st.sfxFx, data.speed, 0);
                }
                return;
            }
        }
    }

    // ── Waveform field: +/- or direct set via number when on wave row ──
    if (st.sfxField === 1) {
        if (key.btnp(key.EQUALS) || key.btnp(key.RIGHT)) {
            // already handled by cursor move; use scroll
        }
        for (let i = 0; i <= 7; i++) {
            if (key.btnp(key.NUM1 + i)) {
                pushSfxUndo();
                if (hasSelection()) {
                    const data = curSfx();
                    const lo = selMin();
                    const hi = selMax();
                    for (let j = lo; j <= hi; j++) data.notes[j].waveform = i;
                    saveSfx(st.sfxSel, data);
                    st.sfxDirty = true;
                    status(`Wave ${getWaveNames()[i]} (${lo}-${hi})`);
                } else {
                    setNoteField(st.sfxNote, 'waveform', i);
                }
                return;
            }
        }
    }

    // ── Effect field: direct set via number when on fx row ──
    if (st.sfxField === 3) {
        for (let i = 0; i <= 7; i++) {
            if (key.btnp(key.NUM1 + i)) {
                pushSfxUndo();
                if (hasSelection()) {
                    const data = curSfx();
                    const lo = selMin();
                    const hi = selMax();
                    for (let j = lo; j <= hi; j++) data.notes[j].effect = i;
                    saveSfx(st.sfxSel, data);
                    st.sfxDirty = true;
                    status(`FX ${getFxNames()[i]} (${lo}-${hi})`);
                } else {
                    setNoteField(st.sfxNote, 'effect', i);
                }
                return;
            }
        }
    }

    // ── Mouse interaction ──
    handleMouse();
}

function handleMouse() {
    const mx = mouse.x();
    const my = mouse.y();
    const listTop = TAB_H + TOOLBAR_H;

    // Click on toolbar buttons
    if (mouse.btnp(0) && mx < LIST_W && my >= TAB_H && my < listTop) {
        if (mx < 10) {
            // Prev arrow
            st.sfxSel = clamp(st.sfxSel - 1, 0, 63);
            ensureSfxVisible();
        } else if (mx >= 26 && mx < 40) {
            // Next arrow
            st.sfxSel = clamp(st.sfxSel + 1, 0, 63);
            ensureSfxVisible();
        } else if (mx >= LIST_W - 18) {
            // Play/stop button
            if (st.sfxPlaying) {
                synth.stop(0);
                st.sfxPlaying = false;
            } else {
                const data = curSfx();
                saveSfx(st.sfxSel, data);
                synth.play(st.sfxSel, 0);
                st.sfxPlaying = true;
                st.sfxPlayStart = sys.time();
            }
        }
        return;
    }

    // Click in SFX list
    if (mouse.btnp(0) && mx < LIST_W && my >= listTop && my < FB_H - FOOT_H) {
        const row = Math.floor((my - listTop) / (CH + 2)) + st.sfxListScroll;
        if (row >= 0 && row < 64) {
            st.sfxSel = row;
            st.sfxUndoStack = [];
            st.sfxRedoStack = [];
            clearSelection();
            const data = curSfx();
            st.sfxSpeed = data.speed;
            st.sfxLoopStart = data.loopStart;
            st.sfxLoopEnd = data.loopEnd;
        }
        return;
    }

    // Click/drag in note grid
    const gridContentX = GRID_X + LABEL_W;
    const gridContentY = GRID_Y + HEADER_H;
    const gridContentBot = gridContentY + FIELD_H;
    if (
        mouse.btn(0) &&
        mx >= gridContentX &&
        mx < FB_W &&
        my >= gridContentY &&
        my < gridContentBot
    ) {
        const col = Math.floor((mx - gridContentX) / NOTE_W) + st.sfxScrollX;
        const row = Math.floor((my - gridContentY) / ROW_H);
        if (col >= 0 && col < 32 && row >= 0 && row < 5) {
            if (mouse.btnp(0)) clearSelection();
            st.sfxNote = col;
            st.sfxField = row;
        }
        return;
    }

    // Mouse drag painting in pitch graph
    const previewY = GRID_Y + HEADER_H + FIELD_H + 8;
    const previewH = FB_H - FOOT_H - previewY - 4;
    const previewX = GRID_X + LABEL_W;
    const previewW = FB_W - previewX - 4;
    if (
        previewH >= 16 &&
        mx >= previewX &&
        mx < previewX + previewW &&
        my >= previewY &&
        my < previewY + previewH
    ) {
        if (mouse.btnp(0)) {
            pushSfxUndo();
            st.sfxDragging = true;
        }
        if (mouse.btn(0) && st.sfxDragging) {
            const col = Math.floor((mx - previewX) / NOTE_W) + st.sfxScrollX;
            if (col >= 0 && col < 32) {
                const range = MAX_PITCH - MIN_PITCH;
                let frac = 1.0 - (my - previewY) / (previewH - 1);
                frac = Math.max(0, Math.min(1, frac));
                let pitch = Math.round(MIN_PITCH + frac * range);
                pitch = clamp(pitch, MIN_PITCH, MAX_PITCH);
                const data = curSfx();
                const note = data.notes[col];
                note.pitch = pitch;
                if (note.volume === 0) {
                    note.waveform = st.sfxWave;
                    note.volume = st.sfxVol;
                    note.effect = st.sfxFx;
                }
                saveSfx(st.sfxSel, data);
                markSfxHasData(st.sfxSel, true);
                st.sfxDirty = true;
                st.sfxNote = col;
            }
        }
    }
    if (!mouse.btn(0)) st.sfxDragging = false;

    // Scroll SFX list
    if (mx < LIST_W && my >= TAB_H) {
        const wheel = mouse.wheel();
        if (wheel !== 0) {
            st.sfxListScroll = clamp(st.sfxListScroll - wheel, 0, Math.max(0, 64 - 20));
        }
    }

    // Scroll note grid horizontally
    if (mx >= GRID_X && my >= TAB_H && my < FB_H - FOOT_H) {
        const wheel = mouse.wheel();
        if (wheel !== 0) {
            st.sfxScrollX = clamp(st.sfxScrollX - wheel, 0, Math.max(0, 32 - VISIBLE_NOTES));
        }
    }
}

function ensureSfxVisible() {
    const listRows = Math.floor((FB_H - TAB_H - TOOLBAR_H - FOOT_H) / (CH + 2));
    if (st.sfxSel < st.sfxListScroll) st.sfxListScroll = st.sfxSel;
    if (st.sfxSel >= st.sfxListScroll + listRows) st.sfxListScroll = st.sfxSel - listRows + 1;
}

function ensureNoteVisible() {
    if (st.sfxNote < st.sfxScrollX) st.sfxScrollX = st.sfxNote;
    if (st.sfxNote >= st.sfxScrollX + VISIBLE_NOTES) st.sfxScrollX = st.sfxNote - VISIBLE_NOTES + 1;
}

// ─── Draw ────────────────────────────────────────────────────────────────────

export function drawSfxEditor() {
    const playNote = getPlaybackNote();
    const data = curSfx();
    drawSfxList();
    drawNoteGrid(playNote);
    drawFooter(data);
}

function drawSfxList() {
    const listTop = TAB_H + TOOLBAR_H;
    const listH = FB_H - listTop - FOOT_H;
    const listRows = Math.floor(listH / (CH + 2));

    // ── Toolbar (prev/next arrows + play button) ──
    const tbY = TAB_H;
    gfx.rectfill(0, tbY, LIST_W - 1, tbY + TOOLBAR_H - 1, FOOTBG);
    gfx.line(0, tbY + TOOLBAR_H - 1, LIST_W - 1, tbY + TOOLBAR_H - 1, SEPC);

    // Prev arrow
    gfx.print('\u25C0', 2, tbY + 3, st.sfxSel > 0 ? FG : GUTFG);
    // SFX index
    const label = (st.sfxSel < 10 ? '0' : '') + st.sfxSel;
    gfx.print(label, 14, tbY + 3, FG);
    // Next arrow
    gfx.print('\u25B6', 28, tbY + 3, st.sfxSel < 63 ? FG : GUTFG);
    // SFX name (after nav arrows)
    if (st.sfxRenaming) {
        gfx.print(`${st.sfxRenameTxt}_`, 40, tbY + 3, NOTE_COL);
    } else if (sfxNames[st.sfxSel]) {
        gfx.print(sfxNames[st.sfxSel], 40, tbY + 3, GUTFG);
    }
    // Play/stop button
    if (st.sfxPlaying) {
        gfx.print('\u25A0', LIST_W - 14, tbY + 3, FX_COL);
    } else {
        gfx.print('\u25B6', LIST_W - 14, tbY + 3, PLAY_COL);
    }

    // Background
    gfx.rectfill(0, listTop, LIST_W - 1, FB_H - FOOT_H - 1, PANELBG);
    // Separator
    gfx.line(LIST_W, TAB_H, LIST_W, FB_H - FOOT_H - 1, SEPC);

    for (let i = 0; i < listRows; i++) {
        const idx = i + st.sfxListScroll;
        if (idx >= 64) break;
        const yy = listTop + i * (CH + 2) + 1;
        const label2 = (idx < 10 ? '0' : '') + idx;
        const isSelected = idx === st.sfxSel;

        if (isSelected) {
            gfx.rectfill(0, yy - 1, LIST_W - 2, yy + CH, SEL_COL);
        }

        // Check if SFX has any notes (cached)
        const hd = sfxHasData ? sfxHasData[idx] : false;

        gfx.print(`#${label2}`, 2, yy, isSelected ? FG : hd ? GUTFG : 17);

        // SFX name (truncated to fit between index and sparkline)
        if (sfxNames[idx]) {
            const nameX = 24;
            const maxChars = hd ? 3 : 6;
            const nm = sfxNames[idx].slice(0, maxChars);
            gfx.print(nm, nameX, yy, isSelected ? FG : GUTFG);
        }

        // Mini pitch sparkline (32 notes compressed into ~30px)
        if (hd) {
            // Build or reuse cached pitch array
            let pitches = sfxSparkCache ? sfxSparkCache[idx] : null;
            if (!pitches) {
                const data = synth.get(idx);
                if (data) {
                    pitches = new Array(32);
                    for (let n = 0; n < 32; n++) pitches[n] = data.notes[n].pitch;
                    if (sfxSparkCache) sfxSparkCache[idx] = pitches;
                }
            }
            if (pitches) {
                const sparkX = 26;
                const sparkW = LIST_W - sparkX - 16;
                const sparkH = CH - 2;
                const sparkY = yy + 1;
                for (let n = 0; n < 32; n++) {
                    const p = pitches[n];
                    if (p < MIN_PITCH || p > MAX_PITCH) continue;
                    const frac = (p - MIN_PITCH) / (MAX_PITCH - MIN_PITCH);
                    const px = sparkX + Math.floor((n * sparkW) / 32);
                    const py = sparkY + sparkH - 1 - Math.floor(frac * (sparkH - 1));
                    gfx.pset(px, py, isSelected ? FG : GUTFG);
                }
            }
        }

        // Playing indicator
        if (isSelected && st.sfxPlaying) {
            gfx.print('\u25B6', LIST_W - 12, yy, PLAY_COL);
        }
    }
}

function drawNoteGrid(playNote) {
    const gridTop = GRID_Y;
    const gridBot = FB_H - FOOT_H;
    const contentX = GRID_X + LABEL_W;
    const contentY = gridTop + HEADER_H;

    // Background
    gfx.rectfill(GRID_X, gridTop, FB_W - 1, gridBot - 1, BG);

    const data = curSfx();

    // ── Column headers (note indices) ──
    gfx.rectfill(GRID_X, gridTop, FB_W - 1, gridTop + HEADER_H - 1, PANELBG);
    for (let i = 0; i < VISIBLE_NOTES && i + st.sfxScrollX < 32; i++) {
        const col = i + st.sfxScrollX;
        const xx = contentX + i * NOTE_W;
        const isSelected = col === st.sfxNote;

        // Column number
        const colStr = col < 10 ? `0${col}` : `${col}`;
        gfx.print(colStr, xx + 1, gridTop + 2, isSelected ? FG : GUTFG);
    }

    // ── Row labels ──
    const rowLabels = ['NOTE', 'WAVE', 'VOL', 'FX', 'OCT'];
    for (let r = 0; r < 5; r++) {
        const yy = contentY + r * ROW_H + 3;
        gfx.print(rowLabels[r], GRID_X + 1, yy, r === st.sfxField ? FG : GUTFG);
    }

    // ── Note data ──
    for (let i = 0; i < VISIBLE_NOTES && i + st.sfxScrollX < 32; i++) {
        const col = i + st.sfxScrollX;
        const xx = contentX + i * NOTE_W;
        const note = data.notes[col];
        const isSelCol = col === st.sfxNote;
        const isPlayCol = col === playNote;

        // Loop markers
        // Show loop start marker even before loop end is set
        if (data.loopEnd > 0) {
            if (col >= data.loopStart && col <= data.loopEnd) {
                gfx.rectfill(xx, contentY - 1, xx + NOTE_W - 2, contentY + FIELD_H - 2, 17);
            }
            if (col === data.loopStart) {
                gfx.line(xx, contentY - 1, xx, contentY + FIELD_H - 2, LOOP_COL);
            }
            if (col === data.loopEnd) {
                gfx.line(
                    xx + NOTE_W - 2,
                    contentY - 1,
                    xx + NOTE_W - 2,
                    contentY + FIELD_H - 2,
                    LOOP_COL,
                );
            }
        } else if (st.sfxLoopStart > 0 && col === st.sfxLoopStart) {
            // Loop start set but no end yet — show a dashed start marker
            for (let dy = 0; dy < FIELD_H; dy += 3) {
                gfx.pset(xx, contentY + dy, LOOP_COL);
            }
        }

        // Playback cursor highlight (full column)
        if (isPlayCol) {
            gfx.rectfill(xx, contentY - 1, xx + NOTE_W - 2, contentY + FIELD_H - 2, 1);
        }

        // Multi-note selection highlight
        if (hasSelection()) {
            const slo = selMin();
            const shi = selMax();
            if (col >= slo && col <= shi) {
                gfx.rectfill(xx, contentY - 1, xx + NOTE_W - 2, contentY + FIELD_H - 2, SEL_COL);
            }
        }

        // Cursor cell highlight
        if (isSelCol) {
            const selY = contentY + st.sfxField * ROW_H;
            gfx.rectfill(xx, selY, xx + NOTE_W - 2, selY + ROW_H - 2, SEL_COL);
        }

        // Pitch row (note letter only — octave shown in OCT row)
        const pitchY = contentY + 3;
        if (note.pitch > 0) {
            let name = synth.noteName(note.pitch).substring(0, 2);
            if (name[1] === '-') [name] = name;
            gfx.print(name, xx + 2, pitchY, isSelCol && st.sfxField === 0 ? FG : NOTE_COL);
        } else {
            gfx.print('--', xx + 2, pitchY, REST_COL);
        }

        // Waveform row
        const waveY = contentY + ROW_H + 3;
        if (note.pitch > 0) {
            gfx.print(
                `${note.waveform}`,
                xx + 4,
                waveY,
                isSelCol && st.sfxField === 1 ? FG : WAVE_COL,
            );
        } else {
            gfx.print('-', xx + 4, waveY, REST_COL);
        }

        // Volume row
        const volY = contentY + ROW_H * 2 + 3;
        if (note.pitch > 0) {
            gfx.print(`${note.volume}`, xx + 4, volY, isSelCol && st.sfxField === 2 ? FG : VOL_COL);
        } else {
            gfx.print('-', xx + 4, volY, REST_COL);
        }

        // Effect row
        const fxY = contentY + ROW_H * 3 + 3;
        if (note.pitch > 0) {
            gfx.print(`${note.effect}`, xx + 4, fxY, isSelCol && st.sfxField === 3 ? FG : FX_COL);
        } else {
            gfx.print('-', xx + 4, fxY, REST_COL);
        }

        // Octave row (display = internal - OCT_OFFSET)
        const octY = contentY + ROW_H * 4 + 3;
        if (note.pitch > 0) {
            const oct = Math.floor((note.pitch - 1) / 12) - OCT_OFFSET;
            gfx.print(`${oct}`, xx + 4, octY, isSelCol && st.sfxField === 4 ? FG : NOTE_COL);
        } else {
            gfx.print('-', xx + 4, octY, REST_COL);
        }

        // Column separator
        if (i > 0) {
            gfx.line(xx - 1, contentY, xx - 1, contentY + FIELD_H - 2, 17);
        }
    }

    // ── Row separators ──
    for (let r = 1; r < 5; r++) {
        const yy = contentY + r * ROW_H - 1;
        gfx.line(contentX, yy, FB_W - 1, yy, 17);
    }

    // ── Waveform mini-preview ──
    drawWavePreview(data, playNote);
}

function drawWavePreview(data, playNote) {
    // Draw a pitch graph with fixed octave 1-4 range
    const previewY = GRID_Y + HEADER_H + FIELD_H + 8;
    const previewH = FB_H - FOOT_H - previewY - 4;
    if (previewH < 16) return;

    const previewX = GRID_X + LABEL_W;
    const previewW = FB_W - previewX - 4;

    // Border
    gfx.rect(previewX - 1, previewY - 1, previewX + previewW, previewY + previewH, SEPC);

    // Fixed range: display octaves 1-4 (internal 3-6, pitch 37-84)
    const minP = MIN_PITCH;
    const maxP = MAX_PITCH;
    const range = maxP - minP;

    // Draw octave grid lines with labels
    for (let intOct = MIN_INT_OCT; intOct <= MAX_INT_OCT; intOct++) {
        const p = intOct * 12 + 1; // first note of octave
        const py = previewY + previewH - 1 - Math.floor(((p - minP) / range) * (previewH - 4));
        gfx.line(previewX, py, previewX + previewW - 1, py, 17);
        gfx.print(`${intOct - OCT_OFFSET}`, GRID_X + 1, py - 3, GUTFG);
    }

    // Draw pitch bars anchored at the bottom, aligned with tracker columns
    // Reserve 2px at top for waveform cap to avoid clipping
    const capH = 2;
    for (let i = 0; i < VISIBLE_NOTES && i + st.sfxScrollX < 32; i++) {
        const col = i + st.sfxScrollX;
        const note = data.notes[col];
        if (note.pitch === 0) continue;
        const bx = previewX + i * NOTE_W;
        // Clamp pitch to display range
        const clampedP = Math.max(minP, Math.min(maxP, note.pitch));
        const pitchFrac = (clampedP - minP) / range;
        const barTop =
            previewY + capH + (previewH - 1 - capH) - Math.floor(pitchFrac * (previewH - 2 - capH));
        const barBot = previewY + previewH - 1;
        const barCol = col === playNote ? PLAY_COL : col === st.sfxNote ? FG : NOTE_COL;
        if (note.volume === 0) continue;

        // Volume bar (thin, proportional height at bottom of column)
        const volFrac = note.volume / 7;
        const volBarH = Math.max(1, Math.floor((barBot - barTop) * volFrac));
        const volTop = barBot - volBarH + 1;
        gfx.rectfill(bx, volTop, bx + NOTE_W - 2, barBot, 17);

        // Pitch bar (full height)
        gfx.rectfill(bx, barTop, bx + NOTE_W - 2, barBot, barCol);

        // Volume overlay (darken top portion to show volume < max)
        if (note.volume < 7) {
            const darkTop = barTop;
            const darkBot = volTop - 1;
            if (darkBot >= darkTop) {
                gfx.rectfill(bx, darkTop, bx + NOTE_W - 2, darkBot, 1);
            }
        }

        // Draw a cap colored by waveform at the pitch position
        if (barBot - barTop > capH) {
            const capCol = WAVE_COLORS[note.waveform] || FG;
            gfx.rectfill(bx, barTop - 1, bx + NOTE_W - 2, barTop + capH - 2, capCol);
        }
    }

    // ── Effect visualization lines ──
    for (let i = 0; i < VISIBLE_NOTES && i + st.sfxScrollX < 32; i++) {
        const col = i + st.sfxScrollX;
        const note = data.notes[col];
        if (note.pitch === 0 || note.volume === 0 || note.effect === 0) continue;
        const bx = previewX + i * NOTE_W;
        const midX = bx + Math.floor(NOTE_W / 2) - 1;
        const clampedP = Math.max(minP, Math.min(maxP, note.pitch));
        const pitchFrac = (clampedP - minP) / range;
        const noteY =
            previewY + capH + (previewH - 1 - capH) - Math.floor(pitchFrac * (previewH - 2 - capH));

        if (note.effect === 1 && col + 1 < 32) {
            // Slide: line to next note's pitch
            const next = data.notes[col + 1];
            if (next.pitch > 0) {
                const nextP = Math.max(minP, Math.min(maxP, next.pitch));
                const nextFrac = (nextP - minP) / range;
                const nextY =
                    previewY +
                    capH +
                    (previewH - 1 - capH) -
                    Math.floor(nextFrac * (previewH - 2 - capH));
                const nextX = previewX + (i + 1) * NOTE_W + Math.floor(NOTE_W / 2) - 1;
                gfx.line(midX, noteY, nextX, nextY, 13);
            }
        } else if (note.effect === 2) {
            // Vibrato: wavy dots
            for (let dx = 0; dx < NOTE_W - 2; dx += 2) {
                const vy = noteY + Math.round(Math.sin(dx * 1.5) * 2);
                gfx.pset(bx + dx, vy, 13);
            }
        } else if (note.effect === 3) {
            // Drop: descending diagonal
            gfx.line(midX, noteY, midX, previewY + previewH - 3, 8);
        } else if (note.effect === 4) {
            // Fade in: ascending triangle
            gfx.line(bx, previewY + previewH - 3, bx + NOTE_W - 2, noteY, 11);
        } else if (note.effect === 5) {
            // Fade out: descending triangle
            gfx.line(bx, noteY, bx + NOTE_W - 2, previewY + previewH - 3, 8);
        } else if (note.effect === 6 || note.effect === 7) {
            // Arpeggio: horizontal ticks
            for (let t = 0; t < 3; t++) {
                const ty = noteY - 2 + t * 3;
                gfx.line(bx + 1, ty, bx + NOTE_W - 3, ty, 14);
            }
        }
    }

    // Label
    gfx.print('OCT', GRID_X + 1, previewY + 2, GUTFG);
}

function drawFooter(data) {
    const fy = FB_H - FOOT_H;
    gfx.rectfill(0, fy, FB_W - 1, FB_H - 1, FOOTBG);
    gfx.line(0, fy, FB_W - 1, fy, SEPC);

    let tx = 4;
    const ty = fy + 3;

    // SFX index
    gfx.print(`SFX:${st.sfxSel < 10 ? '0' : ''}${st.sfxSel}`, tx, ty, FOOTFG);
    tx += CW * 8;

    // Speed
    gfx.print(`SPD:${st.sfxSpeed}`, tx, ty, FOOTFG);
    tx += CW * 7;

    // Loop
    if (data.loopEnd > 0) {
        gfx.print(`LP:${data.loopStart}-${data.loopEnd}`, tx, ty, LOOP_COL);
    } else if (st.sfxLoopStart > 0) {
        gfx.print(`LP:${st.sfxLoopStart}...`, tx, ty, LOOP_COL);
    } else {
        gfx.print('LP:off', tx, ty, GUTFG);
    }
    tx += CW * 9;

    // Playing indicator
    if (st.sfxPlaying) {
        gfx.print('\u25B6 PLAYING', tx, ty, PLAY_COL);
        tx += CW * 10;
    }

    // Wave/effect name and frequency tooltip
    {
        const note = data.notes[st.sfxNote];
        if (st.sfxField === 1) {
            gfx.print(
                `W:${getWaveNames()[note.pitch > 0 ? note.waveform : st.sfxWave]}`,
                tx,
                ty,
                WAVE_COL,
            );
        } else if (st.sfxField === 3) {
            gfx.print(
                `FX:${getFxNames()[note.pitch > 0 ? note.effect : st.sfxFx]}`,
                tx,
                ty,
                FX_COL,
            );
        } else if (note.pitch > 0 && typeof synth.pitchFreq === 'function') {
            const hz = synth.pitchFreq(note.pitch);
            gfx.print(`${Math.round(hz)}Hz`, tx, ty, GUTFG);
        }
    }

    // Status message (right-aligned)
    if (st.msg) {
        const msgW = st.msg.length * CW;
        gfx.print(st.msg, FB_W - msgW - 4, ty, FG);
    }
}
