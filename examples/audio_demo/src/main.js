// Audio Demo
//
// Shows how to:
//   - Play sound effects with sfx.play()
//   - Control per-channel volume with sfx.volume()
//   - Play / stop music with mus.play() / mus.stop()
//   - Fade music in/out
//   - Query playback state
//
// PLACEHOLDER: No audio files are loaded yet.  Add .wav/.ogg files to
// assets/sfx/ and .ogg/.mp3 to assets/music/, then list them in cart.json.
// Until then, the UI is fully functional but silent.

// --- State -----------------------------------------------------------

let sfxVol = 1.0;
let musVol = 1.0;
let musPlaying = false;
let flashCh = -1; // channel that last played (for visual feedback)
let flashTimer = 0;

let STATUS = '';

// --- Helpers ---------------------------------------------------------

function prvStatus(msg) {
    STATUS = msg;
}

let smoothFps = 60;
const fpsHistory = [];
let fpsHistIdx = 0;
const FPS_HIST_LEN = 50;
for (let _i = 0; _i < FPS_HIST_LEN; ++_i) fpsHistory.push(60);

function drawFpsWidget() {
    const wx = 320 - FPS_HIST_LEN - 4;
    const wy = 0;
    const ww = FPS_HIST_LEN + 4;
    const gh = 16;
    const target = sys.targetFps();
    gfx.rectfill(wx, wy, wx + ww - 1, wy + 8 + gh + 1, 0);
    gfx.print(`${math.flr(smoothFps)} FPS`, wx + 2, wy + 1, 7);
    gfx.rect(wx + 1, wy + 8, wx + ww - 2, wy + 8 + gh, 5);
    for (let idx = 1; idx < FPS_HIST_LEN; ++idx) {
        const i0 = (fpsHistIdx + idx - 1) % FPS_HIST_LEN;
        const i1 = (fpsHistIdx + idx) % FPS_HIST_LEN;
        const v0 = math.clamp(fpsHistory[i0] / target, 0, 1);
        const v1 = math.clamp(fpsHistory[i1] / target, 0, 1);
        const y0 = wy + 8 + gh - 1 - math.flr(v0 * (gh - 2));
        const y1 = wy + 8 + gh - 1 - math.flr(v1 * (gh - 2));
        const clr = v1 > 0.9 ? 11 : v1 > 0.5 ? 9 : 8;
        gfx.line(wx + 2 + idx - 1, y0, wx + 2 + idx, y1, clr);
    }
}

// --- Callbacks -------------------------------------------------------

function _init() {
    prvStatus('press 1-4 to play SFX, M to play music');
}

function _update(dt) {
    smoothFps = math.lerp(smoothFps, sys.fps(), 0.05);
    fpsHistory[fpsHistIdx] = smoothFps;
    fpsHistIdx = (fpsHistIdx + 1) % FPS_HIST_LEN;
    // Play SFX on channels 0-3 via number keys
    if (key.btnp(key.A)) {
        // '1' key — using A as proxy since key.1 may not exist
        sfx.play(0, 0);
        flashCh = 0;
        flashTimer = 15;
        prvStatus('sfx 0 -> ch 0');
    }
    if (key.btnp(key.B)) {
        sfx.play(1, 1);
        flashCh = 1;
        flashTimer = 15;
        prvStatus('sfx 1 -> ch 1');
    }
    if (key.btnp(key.C)) {
        sfx.play(2, 2);
        flashCh = 2;
        flashTimer = 15;
        prvStatus('sfx 2 -> ch 2');
    }
    if (key.btnp(key.D)) {
        sfx.play(3, 3);
        flashCh = 3;
        flashTimer = 15;
        prvStatus('sfx 3 -> ch 3');
    }

    // Music controls
    if (key.btnp(key.W)) {
        // M = play music
        if (!mus.playing()) {
            mus.play(0, 0, 500);
            musPlaying = true;
            prvStatus('music: play (500ms fade in)');
        } else {
            mus.stop(500);
            musPlaying = false;
            prvStatus('music: stop (500ms fade out)');
        }
    }

    // Volume up/down with UP/DOWN arrows
    if (key.btnp(key.UP)) {
        sfxVol = math.clamp(sfxVol + 0.1, 0.0, 1.0);
        for (let i = 0; i < 4; ++i) sfx.volume(sfxVol, i);
        prvStatus(`sfx vol: ${math.flr(sfxVol * 100)}%`);
    }
    if (key.btnp(key.DOWN)) {
        sfxVol = math.clamp(sfxVol - 0.1, 0.0, 1.0);
        for (let i = 0; i < 4; ++i) sfx.volume(sfxVol, i);
        prvStatus(`sfx vol: ${math.flr(sfxVol * 100)}%`);
    }

    // Music volume with LEFT/RIGHT
    if (key.btnp(key.LEFT)) {
        musVol = math.clamp(musVol - 0.1, 0.0, 1.0);
        mus.volume(musVol);
        prvStatus(`mus vol: ${math.flr(musVol * 100)}%`);
    }
    if (key.btnp(key.RIGHT)) {
        musVol = math.clamp(musVol + 0.1, 0.0, 1.0);
        mus.volume(musVol);
        prvStatus(`mus vol: ${math.flr(musVol * 100)}%`);
    }

    if (flashTimer > 0) --flashTimer;

    musPlaying = mus.playing();
}

function _draw() {
    gfx.cls(1);

    gfx.print('=== audio demo ===', 100, 4, 7);

    // --- SFX channels ---
    gfx.print('sfx channels', 4, 20, 6);
    for (let ch = 0; ch < 4; ++ch) {
        const x = 4 + ch * 60;
        const y = 30;
        const playing = sfx.playing(ch);
        let col = playing ? 11 : 5;

        if (ch === flashCh && flashTimer > 0) col = 10;

        gfx.rectfill(x, y, x + 50, y + 20, col);
        gfx.rect(x, y, x + 50, y + 20, 7);
        gfx.print(`ch ${ch}`, x + 4, y + 2, 0);
        gfx.print(playing ? 'PLAY' : 'idle', x + 4, y + 10, 0);
    }

    // SFX volume bar
    gfx.print('sfx vol', 4, 58, 6);
    gfx.rect(50, 58, 200, 64, 5);
    gfx.rectfill(50, 58, 50 + math.flr(sfxVol * 150), 64, 11);
    gfx.print(`${math.flr(sfxVol * 100)}%`, 206, 58, 7);

    // --- Music ---
    gfx.print('music', 4, 78, 6);
    const musCol = musPlaying ? 11 : 5;
    gfx.rectfill(4, 88, 240, 108, musCol);
    gfx.rect(4, 88, 240, 108, 7);
    gfx.print(musPlaying ? 'PLAYING' : 'STOPPED', 8, 92, 0);

    // Music volume bar
    gfx.print('mus vol', 4, 116, 6);
    gfx.rect(50, 116, 200, 122, 5);
    gfx.rectfill(50, 116, 50 + math.flr(musVol * 150), 122, 12);
    gfx.print(`${math.flr(musVol * 100)}%`, 206, 116, 7);

    // --- Status ---
    gfx.print(STATUS, 4, 136, 10);

    // --- Controls ---
    gfx.print('a/b/c/d = play sfx 0-3', 4, 152, 5);
    gfx.print('w = toggle music        up/dn = sfx vol', 4, 160, 5);
    gfx.print('left/right = music vol', 4, 168, 5);

    // FPS widget
    drawFpsWidget();
}
