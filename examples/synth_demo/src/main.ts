// Synth Demo
//
// Demonstrates the procedural synth API:
//   - Define SFX patterns with synth.set()
//   - Play patterns with synth.play() on channels 0-3
//   - Preview individual notes with synth.playNote()
//   - Cycle waveforms and effects
//   - Visual piano keyboard and waveform display

// --- Constants -------------------------------------------------------

const WAVE_NAMES = synth.waveNames();
const FX_NAMES = synth.fxNames();
const _NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const OCTAVE_NOTES = 12;
const PIANO_START = 37; // C-3
const PIANO_KEYS = 24; // two octaves

// --- SFX presets -----------------------------------------------------

const PRESETS = [
    {
        name: 'Laser',
        notes: [
            { pitch: 72, waveform: 2, volume: 7, effect: 3 },
            { pitch: 60, waveform: 2, volume: 5, effect: 3 },
            { pitch: 48, waveform: 2, volume: 3, effect: 0 },
        ],
        speed: 4,
    },
    {
        name: 'Jump',
        notes: [
            { pitch: 49, waveform: 0, volume: 7, effect: 1 },
            { pitch: 61, waveform: 0, volume: 6, effect: 0 },
            { pitch: 68, waveform: 0, volume: 4, effect: 5 },
        ],
        speed: 6,
    },
    {
        name: 'Coin',
        notes: [
            { pitch: 73, waveform: 3, volume: 7, effect: 0 },
            { pitch: 80, waveform: 3, volume: 6, effect: 0 },
            { pitch: 80, waveform: 3, volume: 4, effect: 5 },
        ],
        speed: 3,
    },
    {
        name: 'Hit',
        notes: [
            { pitch: 30, waveform: 6, volume: 7, effect: 3 },
            { pitch: 20, waveform: 6, volume: 5, effect: 0 },
            { pitch: 10, waveform: 6, volume: 2, effect: 5 },
        ],
        speed: 3,
    },
    {
        name: 'Powerup',
        notes: [
            { pitch: 49, waveform: 5, volume: 7, effect: 6 },
            { pitch: 56, waveform: 5, volume: 7, effect: 6 },
            { pitch: 61, waveform: 5, volume: 6, effect: 6 },
            { pitch: 68, waveform: 5, volume: 5, effect: 5 },
        ],
        speed: 5,
    },
    {
        name: 'Explosion',
        notes: [
            { pitch: 10, waveform: 6, volume: 7, effect: 0 },
            { pitch: 8, waveform: 6, volume: 7, effect: 2 },
            { pitch: 5, waveform: 6, volume: 5, effect: 0 },
            { pitch: 3, waveform: 6, volume: 3, effect: 5 },
            { pitch: 1, waveform: 6, volume: 1, effect: 5 },
        ],
        speed: 8,
    },
];

// --- State -----------------------------------------------------------

let curPreset = 0;
let curWave = 0;
let curFx = 0;
let curPitch = 49; // C-4
let curVol = 7;
let curChannel = 0;
let playingFlash = 0;
let noteFlash = 0;
let status = '';
let statusTimer = 0;

// --- Helpers ---------------------------------------------------------

function setStatus(msg) {
    status = msg;
    statusTimer = 90; // 1.5s at 60fps
}

function prvShiftPresetPitch(delta) {
    const p = PRESETS[curPreset];
    for (let ni = 0; ni < p.notes.length; ++ni) {
        const np = p.notes[ni].pitch + delta;
        p.notes[ni].pitch = math.clamp(np, 1, 96);
    }
    synth.set(curPreset, p.notes, p.speed);
}

export function _drawLabel(x, y, label, value, col) {
    gfx.print(label, x, y, 6);
    gfx.print(value, x + gfx.textWidth(label) + 2, y, col || 7);
}

function drawPiano(x, y) {
    const keyW = 5;
    const keyH = 20;
    const blackH = 12;

    // White keys first
    let wx = x;
    for (let idx = 0; idx < PIANO_KEYS; ++idx) {
        const pitch = PIANO_START + idx;
        const note = pitch % OCTAVE_NOTES;
        const isBlack = [1, 3, 6, 8, 10].indexOf(note) >= 0;
        if (isBlack) continue;

        const active = pitch === curPitch;
        const col = active ? 11 : 7;
        gfx.rectfill(wx, y, wx + keyW - 1, y + keyH - 1, col);
        gfx.rect(wx, y, wx + keyW - 1, y + keyH - 1, 5);
        wx += keyW;
    }

    // Black keys on top
    wx = x;
    for (let idx = 0; idx < PIANO_KEYS; ++idx) {
        const pitch = PIANO_START + idx;
        const note = pitch % OCTAVE_NOTES;
        const isBlack = [1, 3, 6, 8, 10].indexOf(note) >= 0;
        if (isBlack) {
            const active = pitch === curPitch;
            const col = active ? 3 : 0;
            gfx.rectfill(wx - 2, y, wx + 1, y + blackH - 1, col);
        } else {
            wx += keyW;
        }
    }
}

function drawWaveform(x, y, w, h, wave) {
    // Simple waveform preview
    gfx.rect(x, y, x + w - 1, y + h - 1, 1);
    const mid = y + math.flr(h / 2);
    const amp = math.flr(h / 2) - 1;
    let prevY = mid;

    for (let px = 0; px < w - 2; ++px) {
        const t = px / (w - 2);
        let val = 0;

        if (wave === 0) {
            // triangle
            val = math.abs(((t * 2) % 2) - 1) * 2 - 1;
        } else if (wave === 1) {
            // tiltsaw
            val = ((t * 2) % 1) * 1.5 - 0.75;
            val = math.clamp(val, -1, 1);
        } else if (wave === 2) {
            // saw
            val = ((t * 4) % 1) * 2 - 1;
        } else if (wave === 3) {
            // square
            val = (t * 4) % 1 < 0.5 ? 1 : -1;
        } else if (wave === 4) {
            // pulse
            val = (t * 4) % 1 < 0.25 ? 1 : -1;
        } else if (wave === 5) {
            // organ
            val = math.sin(t * 4 * 6.283) * 0.5 + math.sin(t * 8 * 6.283) * 0.5;
        } else if (wave === 6) {
            // noise
            val = math.rnd(2) - 1;
        } else if (wave === 7) {
            // phaser
            val = math.sin(t * 4 * 6.283 + math.sin(t * 8 * 6.283) * 2);
        }

        const py = mid - math.flr(val * amp);
        gfx.line(x + 1 + px, prevY, x + 1 + px, py, 11);
        prevY = py;
    }
}

function drawChannelIndicators(x, y) {
    for (let ch = 0; ch < 4; ++ch) {
        const active = synth.playing(ch);
        const selected = ch === curChannel;
        const col = active ? 11 : selected ? 6 : 1;
        gfx.rectfill(x + ch * 14, y, x + ch * 14 + 10, y + 6, col);
        gfx.print(`${ch}`, x + ch * 14 + 3, y + 1, active ? 0 : 7);
    }
}

function drawNotePattern(x, y, sfxIdx) {
    const def = synth.get(sfxIdx);
    if (!def) return;

    const _noteH = 3;
    const noteW = 3;
    const maxNotes = 32;

    for (let idx = 0; idx < maxNotes; ++idx) {
        const note = def.notes[idx];
        if (!note || note.pitch === 0) {
            gfx.pset(x + idx * noteW + 1, y + 15, 1);
            continue;
        }

        const barH = math.flr((note.volume / 7) * 12) + 1;
        const col = note.waveform + 8;
        const py = y + 15 - barH;
        gfx.rectfill(x + idx * noteW, py, x + idx * noteW + noteW - 2, y + 15, col);

        // Highlight current playing note
        const curNoteIdx = synth.noteIdx(curChannel);
        if (curNoteIdx === idx && synth.playing(curChannel)) {
            gfx.rect(x + idx * noteW - 1, py - 1, x + idx * noteW + noteW - 1, y + 16, 7);
        }
    }
}

// --- Callbacks -------------------------------------------------------

export function _init(): void {
    // Load all presets into synth slots
    for (let idx = 0; idx < PRESETS.length; ++idx) {
        const p = PRESETS[idx];
        synth.set(idx, p.notes, p.speed);
    }
    setStatus('arrows: navigate  Z: play  X: note preview');
}

export function _update(_dt) {
    if (statusTimer > 0) statusTimer--;

    if (playingFlash > 0) playingFlash--;
    if (noteFlash > 0) noteFlash--;

    // Preset selection with up/down
    if (key.btnp(key.UP)) {
        curPreset = (curPreset - 1 + PRESETS.length) % PRESETS.length;
        curWave = PRESETS[curPreset].notes[0].waveform;
        curFx = PRESETS[curPreset].notes[0].effect;
    }
    if (key.btnp(key.DOWN)) {
        curPreset = (curPreset + 1) % PRESETS.length;
        curWave = PRESETS[curPreset].notes[0].waveform;
        curFx = PRESETS[curPreset].notes[0].effect;
    }

    // Pitch with left/right — shifts all notes in current preset
    if (key.btnp(key.LEFT)) {
        curPitch = math.max(1, curPitch - 1);
        prvShiftPresetPitch(-1);
    }
    if (key.btnp(key.RIGHT)) {
        curPitch = math.min(96, curPitch + 1);
        prvShiftPresetPitch(1);
    }

    // Play preset with Z / jump
    if (key.btnp(key.Z) || input.btnp('jump')) {
        synth.play(curPreset, curChannel);
        playingFlash = 15;
        setStatus(`play SFX ${curPreset} (${PRESETS[curPreset].name}) -> ch ${curChannel}`);
    }

    // Preview note with X / action
    if (key.btnp(key.X) || input.btnp('action')) {
        synth.playNote(curPitch, curWave, curVol, curFx);
        noteFlash = 10;
        setStatus(
            `note ${synth.noteName(curPitch)} wave:${WAVE_NAMES[curWave]} fx:${FX_NAMES[curFx]}`,
        );
    }

    // Cycle waveform with W — updates note preview AND current preset
    if (key.btnp(key.W)) {
        curWave = (curWave + 1) % WAVE_NAMES.length;
        const p = PRESETS[curPreset];
        for (let ni = 0; ni < p.notes.length; ++ni) {
            p.notes[ni].waveform = curWave;
        }
        synth.set(curPreset, p.notes, p.speed);
        setStatus(`waveform: ${WAVE_NAMES[curWave]}`);
    }

    // Cycle effect with E — updates note preview AND current preset
    if (key.btnp(key.E)) {
        curFx = (curFx + 1) % FX_NAMES.length;
        const p = PRESETS[curPreset];
        for (let ni = 0; ni < p.notes.length; ++ni) {
            p.notes[ni].effect = curFx;
        }
        synth.set(curPreset, p.notes, p.speed);
        setStatus(`effect: ${FX_NAMES[curFx]}`);
    }

    // Cycle channel with C
    if (key.btnp(key.C)) {
        curChannel = (curChannel + 1) % 4;
        setStatus(`channel: ${curChannel}`);
    }

    // Volume with V — updates note preview AND current preset
    if (key.btnp(key.V)) {
        curVol = (curVol % 7) + 1;
        const p = PRESETS[curPreset];
        for (let ni = 0; ni < p.notes.length; ++ni) {
            p.notes[ni].volume = curVol;
        }
        synth.set(curPreset, p.notes, p.speed);
        setStatus(`volume: ${curVol}`);
    }

    // Stop with S
    if (key.btnp(key.S)) {
        synth.stop();
        setStatus('stopped all channels');
    }
}

export function _draw(): void {
    gfx.cls(0);

    // Title
    gfx.print('SYNTH DEMO', 4, 2, 7);
    gfx.print(`${synth.count()} SFX defined`, 80, 2, 6);

    // Preset list
    gfx.print('PRESETS', 4, 14, 6);
    for (let idx = 0; idx < PRESETS.length; ++idx) {
        const y = 22 + idx * 8;
        const selected = idx === curPreset;
        const playing = synth.playing(curChannel) && idx === curPreset;

        if (selected) gfx.rectfill(2, y - 1, 90, y + 6, 1);
        const arrow = selected ? '> ' : '  ';
        const col = playing && playingFlash % 4 < 2 ? 11 : selected ? 7 : 5;
        gfx.print(`${arrow}${idx}: ${PRESETS[idx].name}`, 4, y, col);
    }

    // Note pattern for current preset
    gfx.print('PATTERN', 4, 74, 6);
    drawNotePattern(4, 82, curPreset);

    // Waveform display
    gfx.print(`WAVE: ${WAVE_NAMES[curWave]} (W)`, 110, 14, 6);
    drawWaveform(110, 22, 80, 30, curWave);

    // Effect
    gfx.print(`FX: ${FX_NAMES[curFx]} (E)`, 110, 56, 6);

    // Channel indicators
    gfx.print('CH (C)', 110, 68, 6);
    drawChannelIndicators(140, 68);

    // Volume
    gfx.print(`VOL: ${curVol} (V)`, 200, 68, 6);

    // Piano keyboard
    gfx.print('PIANO', 110, 80, 6);
    gfx.print(synth.noteName(curPitch), 140, 80, 11);
    gfx.print(`${math.flr(synth.pitchFreq(curPitch))} Hz`, 164, 80, 5);
    drawPiano(110, 88);

    // Controls help
    const cy = 116;
    gfx.print('CONTROLS', 110, cy, 6);
    gfx.print('up/dn  select preset', 110, cy + 8, 5);
    gfx.print('lt/rt  change pitch', 110, cy + 15, 5);
    gfx.print('Z      play preset', 110, cy + 22, 5);
    gfx.print('X      preview note', 110, cy + 29, 5);
    gfx.print('S      stop all', 110, cy + 36, 5);

    // Frequency reference
    const refY = 158;
    gfx.print(
        `pitch ${curPitch}  ${synth.noteName(curPitch)}  ${math.flr(synth.pitchFreq(curPitch))} Hz`,
        4,
        refY,
        5,
    );

    // Status bar
    if (statusTimer > 0) {
        gfx.rectfill(0, 172, 319, 179, 1);
        gfx.print(status, 4, 173, noteFlash > 0 ? 11 : 7);
    }
}
