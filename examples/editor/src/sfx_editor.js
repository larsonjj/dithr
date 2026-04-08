// ─── SFX Editor ──────────────────────────────────────────────────────────────
//
// Chip-tune synthesizer editor. Each SFX is 32 notes with pitch, waveform,
// volume, and effect per note. Uses the C-side synth engine for PCM rendering
// and preview playback via the synth.* JS API.

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

// ─── Layout constants ────────────────────────────────────────────────────────

const LIST_W = 70; // SFX list panel width
const FOOT_H = 14; // footer height
const TOOLBAR_H = 14; // SFX toolbar height (nav + play buttons)
const GRID_X = LIST_W + 1; // note grid left edge
const GRID_Y = TAB_H; // note grid top edge
const GRID_W = FB_W - GRID_X; // note grid total width
const GRID_H = FB_H - TAB_H - FOOT_H; // note grid total height
const NOTE_W = 18; // column width per note
const ROW_H = 14; // row height for each field
const HEADER_H = 12; // column header height
const FIELD_NAMES = ["pitch", "wave", "vol", "fx", "oct"];
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

// Waveform short names for grid display (ASCII only, font covers 32-126)
const WAVE_SHORT = ["/", "|", "\\", "#", "!", "@", "?", "*"];
// Effect short names (ASCII only)
const FX_SHORT = ["-", "^", "v", "\\", "<", ">", "3", "6"];

// Waveform colors for pitch graph caps
const WAVE_COLORS = [
    9, // triangle = orange
    8, // tiltsaw = red
    8, // saw = red
    11, // square = green
    14, // pulse = pink
    13, // organ = lavender
    6, // noise = light gray
    10, // phaser = yellow
];

// ─── Data helpers ────────────────────────────────────────────────────────────

/** Load SFX data from C into a JS array of note objects. */
function loadSfx(idx) {
    let data = synth.get(idx);
    if (data) {
        // Default speed to 16 (PICO-8 default) when C struct is zero-initialized
        if (!data.speed) data.speed = 16;
        if (data.speed > 32) data.speed = 32;
        return data;
    }
    // Return default empty SFX
    let notes = [];
    for (let i = 0; i < 32; i++) notes.push({ pitch: 0, waveform: 0, volume: 0, effect: 0 });
    return { notes: notes, speed: 16, loopStart: 0, loopEnd: 0 };
}

/** Push current SFX data to the C synth engine. */
function saveSfx(idx, data) {
    synth.set(idx, data.notes, data.speed, data.loopStart, data.loopEnd);
}

/** Get the currently loaded SFX data. */
function curSfx() {
    return loadSfx(st.sfxSel);
}

/** Modify a note field and push to C. The real-time synth callback reads
 *  the updated definition directly — no buffer swap needed. */
function setNoteField(noteIdx, field, value) {
    let data = curSfx();
    data.notes[noteIdx][field] = value;
    saveSfx(st.sfxSel, data);
    st.sfxDirty = true;
}

/** Generate a pitch value from note name index (0-11) and internal octave. */
function makePitch(noteInOctave, octave) {
    return clamp(octave, MIN_INT_OCT, MAX_INT_OCT) * 12 + noteInOctave + 1;
}

/** Nudge the current field value up or down by `dir` (+1 or -1). */
function nudgeField(dir) {
    let data = curSfx();
    let note = data.notes[st.sfxNote];
    if (st.sfxField === 0) {
        // Pitch: 0 = rest, MIN_PITCH-MAX_PITCH = notes
        let p = note.pitch + dir;
        if (p < 0) p = 0;
        if (p > 0 && p < MIN_PITCH) p = dir > 0 ? MIN_PITCH : 0;
        if (p > MAX_PITCH) p = MAX_PITCH;
        setNoteField(st.sfxNote, "pitch", p);
    } else if (st.sfxField === 1) {
        setNoteField(st.sfxNote, "waveform", clamp(note.waveform + dir, 0, 7));
    } else if (st.sfxField === 2) {
        setNoteField(st.sfxNote, "volume", clamp(note.volume + dir, 0, 7));
    } else if (st.sfxField === 3) {
        setNoteField(st.sfxNote, "effect", clamp(note.effect + dir, 0, 7));
    } else if (st.sfxField === 4) {
        // Octave: shift pitch to new octave keeping note-within-octave
        if (note.pitch > 0) {
            let noteInOct = (note.pitch - 1) % 12;
            let curOct = Math.floor((note.pitch - 1) / 12);
            let newOct = clamp(curOct + dir, MIN_INT_OCT, MAX_INT_OCT);
            setNoteField(st.sfxNote, "pitch", makePitch(noteInOct, newOct));
        }
    }
}

/** Get the currently playing note index, or -1 if not playing. */
function getPlaybackNote() {
    if (!st.sfxPlaying) return -1;
    return synth.noteIdx();
}

// Key-to-note mapping (piano keyboard layout)
// Z=C, S=C#, X=D, D=D#, C=E, V=F, G=F#, B=G, H=G#, N=A, J=A#, M=B
const PIANO_KEYS = [
    { key: "Z", note: 0 },
    { key: "S", note: 1 },
    { key: "X", note: 2 },
    { key: "D", note: 3 },
    { key: "C", note: 4 },
    { key: "V", note: 5 },
    { key: "G", note: 6 },
    { key: "B", note: 7 },
    { key: "H", note: 8 },
    { key: "N", note: 9 },
    { key: "J", note: 10 },
    { key: "M", note: 11 },
];

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateSfxEditor(dt) {
    let ctrl = modKey();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    // ── Play/stop preview (only on first press, ignore key repeat) ──
    if (key.btnp(key.SPACE) && !st.sfxSpaceHeld) {
        st.sfxSpaceHeld = true;
        if (st.sfxPlaying) {
            synth.stop(0);
            st.sfxPlaying = false;
        } else {
            let data = curSfx();
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

    // ── Copy SFX (Ctrl+C) ──
    if (ctrl && key.btnp(key.C)) {
        let data = curSfx();
        let notesCopy = [];
        for (let i = 0; i < 32; i++) {
            let n = data.notes[i];
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
        status("Copied SFX " + st.sfxSel);
        return;
    }

    // ── Paste SFX (Ctrl+V) ──
    if (ctrl && key.btnp(key.V)) {
        if (st.sfxClipboard) {
            saveSfx(st.sfxSel, st.sfxClipboard);
            st.sfxSpeed = st.sfxClipboard.speed;
            st.sfxLoopStart = st.sfxClipboard.loopStart;
            st.sfxLoopEnd = st.sfxClipboard.loopEnd;
            st.sfxDirty = true;
            status("Pasted to SFX " + st.sfxSel);
        } else {
            status("Nothing to paste");
        }
        return;
    }

    // ── SFX list navigation ──
    if (ctrl && key.btnp(key.UP)) {
        st.sfxSel = clamp(st.sfxSel - 1, 0, 63);
        ensureSfxVisible();
        return;
    }
    if (ctrl && key.btnp(key.DOWN)) {
        st.sfxSel = clamp(st.sfxSel + 1, 0, 63);
        ensureSfxVisible();
        return;
    }

    // ── Note cursor movement ──
    if (!ctrl && key.btnp(key.LEFT)) {
        st.sfxNote = clamp(st.sfxNote - 1, 0, 31);
        ensureNoteVisible();
        return;
    }
    if (!ctrl && key.btnp(key.RIGHT)) {
        st.sfxNote = clamp(st.sfxNote + 1, 0, 31);
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
        let data = curSfx();
        data.speed = clamp(data.speed - 1, 1, 32);
        st.sfxSpeed = data.speed;
        saveSfx(st.sfxSel, data);
        st.sfxDirty = true;
        return;
    }
    if (key.btnp(key.EQUALS)) {
        let data = curSfx();
        data.speed = clamp(data.speed + 1, 1, 32);
        st.sfxSpeed = data.speed;
        saveSfx(st.sfxSel, data);
        st.sfxDirty = true;
        return;
    }

    // ── Octave adjustment ([ and ] change selected note's octave) ──
    if (key.btnp(key.LEFTBRACKET)) {
        let data = curSfx();
        let note = data.notes[st.sfxNote];
        if (note.pitch > 0) {
            let noteInOct = (note.pitch - 1) % 12;
            let curOct = Math.floor((note.pitch - 1) / 12);
            let newOct = clamp(curOct - 1, MIN_INT_OCT, MAX_INT_OCT);
            setNoteField(st.sfxNote, "pitch", makePitch(noteInOct, newOct));
            status("Octave: " + (newOct - OCT_OFFSET));
        }
        return;
    }
    if (key.btnp(key.RIGHTBRACKET)) {
        let data = curSfx();
        let note = data.notes[st.sfxNote];
        if (note.pitch > 0) {
            let noteInOct = (note.pitch - 1) % 12;
            let curOct = Math.floor((note.pitch - 1) / 12);
            let newOct = clamp(curOct + 1, MIN_INT_OCT, MAX_INT_OCT);
            setNoteField(st.sfxNote, "pitch", makePitch(noteInOct, newOct));
            status("Octave: " + (newOct - OCT_OFFSET));
        }
        return;
    }

    // ── Waveform cycle (W key) ──
    if (key.btnp(key.W)) {
        if (st.sfxField === 1) {
            // On wave row: cycle this note's waveform
            let data = curSfx();
            let nw = (data.notes[st.sfxNote].waveform + 1) % 8;
            setNoteField(st.sfxNote, "waveform", nw);
            let names = synth.waveNames();
            status("Note wave: " + names[nw]);
        } else {
            st.sfxWave = (st.sfxWave + 1) % 8;
            let names = synth.waveNames();
            status("Brush wave: " + names[st.sfxWave]);
        }
        return;
    }

    // ── Effect cycle (E key) ──
    if (key.btnp(key.E)) {
        if (st.sfxField === 3) {
            // On fx row: cycle this note's effect
            let data = curSfx();
            let nf = (data.notes[st.sfxNote].effect + 1) % 8;
            setNoteField(st.sfxNote, "effect", nf);
            let names = synth.fxNames();
            status("Note fx: " + names[nf]);
        } else {
            st.sfxFx = (st.sfxFx + 1) % 8;
            let names = synth.fxNames();
            status("Brush fx: " + names[st.sfxFx]);
        }
        return;
    }

    // ── Volume / Octave direct set via number keys ──
    for (let i = 0; i <= 7; i++) {
        if (key.btnp(key.NUM1 + i)) {
            if (st.sfxField === 2) {
                // Direct volume set on selected note
                setNoteField(st.sfxNote, "volume", i);
            } else if (st.sfxField === 4) {
                // Direct octave set on selected note
                let data = curSfx();
                let note = data.notes[st.sfxNote];
                if (note.pitch > 0) {
                    let intOct = clamp(i + OCT_OFFSET, MIN_INT_OCT, MAX_INT_OCT);
                    let noteInOct = (note.pitch - 1) % 12;
                    setNoteField(st.sfxNote, "pitch", makePitch(noteInOct, intOct));
                    status("Octave: " + (intOct - OCT_OFFSET));
                }
            } else {
                st.sfxVol = i;
                status("Volume: " + i);
            }
            return;
        }
    }

    // ── Delete note ──
    if (key.btnp(key.DELETE) || key.btnp(key.BACKSPACE)) {
        if (st.sfxField === 0) {
            setNoteField(st.sfxNote, "pitch", 0);
            setNoteField(st.sfxNote, "volume", 0);
        } else if (st.sfxField === 1) {
            setNoteField(st.sfxNote, "waveform", 0);
        } else if (st.sfxField === 2) {
            setNoteField(st.sfxNote, "volume", 0);
        } else {
            setNoteField(st.sfxNote, "effect", 0);
        }
        return;
    }

    // ── Loop markers (Ctrl+L for start, Ctrl+Shift+L for end) ──
    // Re-pressing on the same position clears the loop.
    if (ctrl && key.btnp(key.L)) {
        let data = curSfx();
        if (shift) {
            if (data.loopEnd === st.sfxNote && data.loopEnd > 0) {
                // Toggle off: clear loop
                data.loopEnd = 0;
                data.loopStart = 0;
                st.sfxLoopEnd = 0;
                st.sfxLoopStart = 0;
                status("Loop cleared");
            } else {
                data.loopEnd = st.sfxNote;
                st.sfxLoopEnd = st.sfxNote;
                status("Loop end: " + st.sfxNote);
            }
        } else {
            if (data.loopStart === st.sfxNote && data.loopEnd > 0) {
                // Toggle off: clear loop
                data.loopEnd = 0;
                data.loopStart = 0;
                st.sfxLoopEnd = 0;
                st.sfxLoopStart = 0;
                status("Loop cleared");
            } else {
                data.loopStart = st.sfxNote;
                st.sfxLoopStart = st.sfxNote;
                status("Loop start: " + st.sfxNote);
            }
        }
        saveSfx(st.sfxSel, data);
        st.sfxDirty = true;
        return;
    }

    // ── Piano keyboard input ──
    if (st.sfxField === 0 || st.sfxField === 4) {
        for (let i = 0; i < PIANO_KEYS.length; i++) {
            let pk = PIANO_KEYS[i];
            if (key.btnp(key[pk.key])) {
                // Use the note's current octave, or default to MIN_INT_OCT for new notes
                let data = curSfx();
                let curNote = data.notes[st.sfxNote];
                let oct = curNote.pitch > 0 ? Math.floor((curNote.pitch - 1) / 12) : MIN_INT_OCT;
                let pitch = makePitch(pk.note, oct);
                data.notes[st.sfxNote].pitch = pitch;
                data.notes[st.sfxNote].waveform = st.sfxWave;
                data.notes[st.sfxNote].volume = st.sfxVol;
                data.notes[st.sfxNote].effect = st.sfxFx;
                saveSfx(st.sfxSel, data);
                st.sfxDirty = true;
                // Auto-advance cursor
                if (st.sfxNote < 31) {
                    st.sfxNote++;
                    ensureNoteVisible();
                }
                if (st.sfxPlaying) {
                    // Resume pattern playback with the edit applied
                    let elapsed = sys.time() - st.sfxPlayStart;
                    let sampleOff = Math.max(0, Math.floor(elapsed * 44100));
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
                setNoteField(st.sfxNote, "waveform", i);
                return;
            }
        }
    }

    // ── Effect field: direct set via number when on fx row ──
    if (st.sfxField === 3) {
        for (let i = 0; i <= 7; i++) {
            if (key.btnp(key.NUM1 + i)) {
                setNoteField(st.sfxNote, "effect", i);
                return;
            }
        }
    }

    // ── Mouse interaction ──
    handleMouse();
}

function handleMouse() {
    let mx = mouse.x();
    let my = mouse.y();
    let listTop = TAB_H + TOOLBAR_H;

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
                let data = curSfx();
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
        let row = Math.floor((my - listTop) / (CH + 2)) + st.sfxListScroll;
        if (row >= 0 && row < 64) {
            st.sfxSel = row;
            // Load speed/loop from selected SFX
            let data = curSfx();
            st.sfxSpeed = data.speed;
            st.sfxLoopStart = data.loopStart;
            st.sfxLoopEnd = data.loopEnd;
        }
        return;
    }

    // Click in note grid
    let gridContentX = GRID_X + LABEL_W;
    let gridContentY = GRID_Y + HEADER_H;
    if (mouse.btnp(0) && mx >= gridContentX && mx < FB_W && my >= gridContentY) {
        let col = Math.floor((mx - gridContentX) / NOTE_W) + st.sfxScrollX;
        let row = Math.floor((my - gridContentY) / ROW_H);
        if (col >= 0 && col < 32 && row >= 0 && row < 5) {
            st.sfxNote = col;
            st.sfxField = row;
        }
        return;
    }

    // Scroll SFX list
    if (mx < LIST_W && my >= TAB_H) {
        let wheel = mouse.wheel();
        if (wheel !== 0) {
            st.sfxListScroll = clamp(st.sfxListScroll - wheel, 0, Math.max(0, 64 - 20));
        }
    }

    // Scroll note grid horizontally
    if (mx >= GRID_X && my >= TAB_H && my < FB_H - FOOT_H) {
        let wheel = mouse.wheel();
        if (wheel !== 0) {
            st.sfxScrollX = clamp(st.sfxScrollX - wheel, 0, Math.max(0, 32 - VISIBLE_NOTES));
        }
    }
}

function ensureSfxVisible() {
    let listRows = Math.floor((FB_H - TAB_H - TOOLBAR_H - FOOT_H) / (CH + 2));
    if (st.sfxSel < st.sfxListScroll) st.sfxListScroll = st.sfxSel;
    if (st.sfxSel >= st.sfxListScroll + listRows) st.sfxListScroll = st.sfxSel - listRows + 1;
}

function ensureNoteVisible() {
    if (st.sfxNote < st.sfxScrollX) st.sfxScrollX = st.sfxNote;
    if (st.sfxNote >= st.sfxScrollX + VISIBLE_NOTES) st.sfxScrollX = st.sfxNote - VISIBLE_NOTES + 1;
}

// ─── Draw ────────────────────────────────────────────────────────────────────

export function drawSfxEditor() {
    let playNote = getPlaybackNote();
    drawSfxList();
    drawNoteGrid(playNote);
    drawFooter();
}

function drawSfxList() {
    let listTop = TAB_H + TOOLBAR_H;
    let listH = FB_H - listTop - FOOT_H;
    let listRows = Math.floor(listH / (CH + 2));

    // ── Toolbar (prev/next arrows + play button) ──
    let tbY = TAB_H;
    gfx.rectfill(0, tbY, LIST_W - 1, tbY + TOOLBAR_H - 1, FOOTBG);
    gfx.line(0, tbY + TOOLBAR_H - 1, LIST_W - 1, tbY + TOOLBAR_H - 1, SEPC);

    // Prev arrow
    gfx.print("\u25C0", 2, tbY + 3, st.sfxSel > 0 ? FG : GUTFG);
    // SFX index
    let label = (st.sfxSel < 10 ? "0" : "") + st.sfxSel;
    gfx.print(label, 14, tbY + 3, FG);
    // Next arrow
    gfx.print("\u25B6", 28, tbY + 3, st.sfxSel < 63 ? FG : GUTFG);
    // Play/stop button
    if (st.sfxPlaying) {
        gfx.print("\u25A0", LIST_W - 14, tbY + 3, FX_COL);
    } else {
        gfx.print("\u25B6", LIST_W - 14, tbY + 3, PLAY_COL);
    }

    // Background
    gfx.rectfill(0, listTop, LIST_W - 1, FB_H - FOOT_H - 1, PANELBG);
    // Separator
    gfx.line(LIST_W, TAB_H, LIST_W, FB_H - FOOT_H - 1, SEPC);

    for (let i = 0; i < listRows; i++) {
        let idx = i + st.sfxListScroll;
        if (idx >= 64) break;
        let yy = listTop + i * (CH + 2) + 1;
        let label2 = (idx < 10 ? "0" : "") + idx;
        let isSelected = idx === st.sfxSel;

        if (isSelected) {
            gfx.rectfill(0, yy - 1, LIST_W - 2, yy + CH, SEL_COL);
        }

        // Check if SFX has any notes
        let hasData = false;
        let data = loadSfx(idx);
        for (let n = 0; n < 32; n++) {
            if (data.notes[n].pitch > 0) {
                hasData = true;
                break;
            }
        }

        gfx.print("#" + label2, 2, yy, isSelected ? FG : hasData ? GUTFG : 17);

        // Playing indicator
        if (isSelected && st.sfxPlaying) {
            gfx.print("\u25B6", LIST_W - 12, yy, PLAY_COL);
        }
    }
}

function drawNoteGrid(playNote) {
    let gridTop = GRID_Y;
    let gridBot = FB_H - FOOT_H;
    let contentX = GRID_X + LABEL_W;
    let contentY = gridTop + HEADER_H;

    // Background
    gfx.rectfill(GRID_X, gridTop, FB_W - 1, gridBot - 1, BG);

    let data = curSfx();

    // ── Column headers (note indices) ──
    gfx.rectfill(GRID_X, gridTop, FB_W - 1, gridTop + HEADER_H - 1, PANELBG);
    for (let i = 0; i < VISIBLE_NOTES && i + st.sfxScrollX < 32; i++) {
        let col = i + st.sfxScrollX;
        let xx = contentX + i * NOTE_W;
        let isSelected = col === st.sfxNote;

        // Column number
        let colStr = col < 10 ? "0" + col : "" + col;
        gfx.print(colStr, xx + 1, gridTop + 2, isSelected ? FG : GUTFG);
    }

    // ── Row labels ──
    let rowLabels = ["NOTE", "WAVE", "VOL", "FX", "OCT"];
    let rowColors = [NOTE_COL, WAVE_COL, VOL_COL, FX_COL, NOTE_COL];
    for (let r = 0; r < 5; r++) {
        let yy = contentY + r * ROW_H + 3;
        gfx.print(rowLabels[r], GRID_X + 1, yy, r === st.sfxField ? FG : GUTFG);
    }

    // ── Note data ──
    for (let i = 0; i < VISIBLE_NOTES && i + st.sfxScrollX < 32; i++) {
        let col = i + st.sfxScrollX;
        let xx = contentX + i * NOTE_W;
        let note = data.notes[col];
        let isSelCol = col === st.sfxNote;
        let isPlayCol = col === playNote;

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

        // Selection highlight
        if (isSelCol) {
            let selY = contentY + st.sfxField * ROW_H;
            gfx.rectfill(xx, selY, xx + NOTE_W - 2, selY + ROW_H - 2, SEL_COL);
        }

        // Pitch row (note letter only — octave shown in OCT row)
        let pitchY = contentY + 3;
        if (note.pitch > 0) {
            let name = synth.noteName(note.pitch).substring(0, 2);
            if (name[1] === "-") name = name[0];
            gfx.print(name, xx + 2, pitchY, isSelCol && st.sfxField === 0 ? FG : NOTE_COL);
        } else {
            gfx.print("--", xx + 2, pitchY, REST_COL);
        }

        // Waveform row
        let waveY = contentY + ROW_H + 3;
        if (note.pitch > 0) {
            gfx.print(
                "" + note.waveform,
                xx + 4,
                waveY,
                isSelCol && st.sfxField === 1 ? FG : WAVE_COL,
            );
        } else {
            gfx.print("-", xx + 4, waveY, REST_COL);
        }

        // Volume row
        let volY = contentY + ROW_H * 2 + 3;
        if (note.pitch > 0) {
            gfx.print("" + note.volume, xx + 4, volY, isSelCol && st.sfxField === 2 ? FG : VOL_COL);
        } else {
            gfx.print("-", xx + 4, volY, REST_COL);
        }

        // Effect row
        let fxY = contentY + ROW_H * 3 + 3;
        if (note.pitch > 0) {
            let fStr = FX_SHORT[note.effect] || "?";
            gfx.print(fStr, xx + 4, fxY, isSelCol && st.sfxField === 3 ? FG : FX_COL);
        } else {
            gfx.print("-", xx + 4, fxY, REST_COL);
        }

        // Octave row (display = internal - OCT_OFFSET)
        let octY = contentY + ROW_H * 4 + 3;
        if (note.pitch > 0) {
            let oct = Math.floor((note.pitch - 1) / 12) - OCT_OFFSET;
            gfx.print("" + oct, xx + 4, octY, isSelCol && st.sfxField === 4 ? FG : NOTE_COL);
        } else {
            gfx.print("-", xx + 4, octY, REST_COL);
        }

        // Column separator
        if (i > 0) {
            gfx.line(xx - 1, contentY, xx - 1, contentY + FIELD_H - 2, 17);
        }
    }

    // ── Row separators ──
    for (let r = 1; r < 5; r++) {
        let yy = contentY + r * ROW_H - 1;
        gfx.line(contentX, yy, FB_W - 1, yy, 17);
    }

    // ── Waveform mini-preview ──
    drawWavePreview(data, playNote);
}

function drawWavePreview(data, playNote) {
    // Draw a pitch graph with fixed octave 1-4 range
    let previewY = GRID_Y + HEADER_H + FIELD_H + 8;
    let previewH = FB_H - FOOT_H - previewY - 4;
    if (previewH < 16) return;

    let previewX = GRID_X + LABEL_W;
    let previewW = FB_W - previewX - 4;

    // Border
    gfx.rect(previewX - 1, previewY - 1, previewX + previewW, previewY + previewH, SEPC);

    // Fixed range: display octaves 1-4 (internal 3-6, pitch 37-84)
    let minP = MIN_PITCH;
    let maxP = MAX_PITCH;
    let range = maxP - minP;

    // Draw octave grid lines with labels
    for (let intOct = MIN_INT_OCT; intOct <= MAX_INT_OCT; intOct++) {
        let p = intOct * 12 + 1; // first note of octave
        let py = previewY + previewH - 1 - Math.floor(((p - minP) / range) * (previewH - 4));
        gfx.line(previewX, py, previewX + previewW - 1, py, 17);
        gfx.print("" + (intOct - OCT_OFFSET), GRID_X + 1, py - 3, GUTFG);
    }

    // Draw pitch bars anchored at the bottom, aligned with tracker columns
    // Reserve 2px at top for waveform cap to avoid clipping
    let capH = 2;
    for (let i = 0; i < VISIBLE_NOTES && i + st.sfxScrollX < 32; i++) {
        let col = i + st.sfxScrollX;
        let note = data.notes[col];
        if (note.pitch === 0) continue;
        let bx = previewX + i * NOTE_W;
        // Clamp pitch to display range
        let clampedP = Math.max(minP, Math.min(maxP, note.pitch));
        let pitchFrac = (clampedP - minP) / range;
        let barTop =
            previewY + capH + (previewH - 1 - capH) - Math.floor(pitchFrac * (previewH - 2 - capH));
        let barBot = previewY + previewH - 1;
        let barCol = col === playNote ? PLAY_COL : col === st.sfxNote ? FG : NOTE_COL;
        if (note.volume === 0) continue;
        gfx.rectfill(bx, barTop, bx + NOTE_W - 2, barBot, barCol);
        // Draw a cap colored by waveform at the pitch position
        if (barBot - barTop > capH) {
            let capCol = WAVE_COLORS[note.waveform] || FG;
            gfx.rectfill(bx, barTop - 1, bx + NOTE_W - 2, barTop + capH - 2, capCol);
        }
    }

    // Label
    gfx.print("OCT", GRID_X + 1, previewY + 2, GUTFG);
}

function drawFooter() {
    let fy = FB_H - FOOT_H;
    gfx.rectfill(0, fy, FB_W - 1, FB_H - 1, FOOTBG);
    gfx.line(0, fy, FB_W - 1, fy, SEPC);

    let data = curSfx();

    let tx = 4;
    let ty = fy + 3;

    // SFX index
    gfx.print("SFX:" + (st.sfxSel < 10 ? "0" : "") + st.sfxSel, tx, ty, FOOTFG);
    tx += CW * 8;

    // Speed
    gfx.print("SPD:" + st.sfxSpeed, tx, ty, FOOTFG);
    tx += CW * 7;

    // Loop
    if (data.loopEnd > 0) {
        gfx.print("LP:" + data.loopStart + "-" + data.loopEnd, tx, ty, LOOP_COL);
    } else {
        gfx.print("LP:off", tx, ty, GUTFG);
    }
    tx += CW * 9;

    // Playing indicator
    if (st.sfxPlaying) {
        gfx.print("\u25B6 PLAYING", tx, ty, PLAY_COL);
    }

    // Status message (right-aligned)
    if (st.msg) {
        let msgW = st.msg.length * CW;
        gfx.print(st.msg, FB_W - msgW - 4, ty, FG);
    }
}
