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

var sfx_vol = 1.0;
var mus_vol = 1.0;
var mus_playing = false;
var flash_ch = -1; // channel that last played (for visual feedback)
var flash_timer = 0;

var STATUS = "";

// --- Helpers ---------------------------------------------------------

function prv_status(msg) {
    STATUS = msg;
}

// --- Callbacks -------------------------------------------------------

function _init() {
    prv_status("press 1-4 to play SFX, M to play music");
}

function _update(dt) {
    // Play SFX on channels 0-3 via number keys
    if (key.btnp(key.A)) {
        // '1' key — using A as proxy since key.1 may not exist
        sfx.play(0, 0);
        flash_ch = 0;
        flash_timer = 15;
        prv_status("sfx 0 -> ch 0");
    }
    if (key.btnp(key.B)) {
        sfx.play(1, 1);
        flash_ch = 1;
        flash_timer = 15;
        prv_status("sfx 1 -> ch 1");
    }
    if (key.btnp(key.C)) {
        sfx.play(2, 2);
        flash_ch = 2;
        flash_timer = 15;
        prv_status("sfx 2 -> ch 2");
    }
    if (key.btnp(key.D)) {
        sfx.play(3, 3);
        flash_ch = 3;
        flash_timer = 15;
        prv_status("sfx 3 -> ch 3");
    }

    // Music controls
    if (key.btnp(key.W)) {
        // M = play music
        if (!mus.playing()) {
            mus.play(0, 0, 500);
            mus_playing = true;
            prv_status("music: play (500ms fade in)");
        } else {
            mus.stop(500);
            mus_playing = false;
            prv_status("music: stop (500ms fade out)");
        }
    }

    // Volume up/down with UP/DOWN arrows
    if (key.btnp(key.UP)) {
        sfx_vol = math.clamp(sfx_vol + 0.1, 0.0, 1.0);
        for (var i = 0; i < 4; ++i) sfx.volume(sfx_vol, i);
        prv_status("sfx vol: " + math.flr(sfx_vol * 100) + "%");
    }
    if (key.btnp(key.DOWN)) {
        sfx_vol = math.clamp(sfx_vol - 0.1, 0.0, 1.0);
        for (var i = 0; i < 4; ++i) sfx.volume(sfx_vol, i);
        prv_status("sfx vol: " + math.flr(sfx_vol * 100) + "%");
    }

    // Music volume with LEFT/RIGHT
    if (key.btnp(key.LEFT)) {
        mus_vol = math.clamp(mus_vol - 0.1, 0.0, 1.0);
        mus.volume(mus_vol);
        prv_status("mus vol: " + math.flr(mus_vol * 100) + "%");
    }
    if (key.btnp(key.RIGHT)) {
        mus_vol = math.clamp(mus_vol + 0.1, 0.0, 1.0);
        mus.volume(mus_vol);
        prv_status("mus vol: " + math.flr(mus_vol * 100) + "%");
    }

    if (flash_timer > 0) --flash_timer;

    mus_playing = mus.playing();
}

function _draw() {
    gfx.cls(1);

    gfx.print("=== audio demo ===", 100, 4, 7);

    // --- SFX channels ---
    gfx.print("sfx channels", 4, 20, 6);
    for (var ch = 0; ch < 4; ++ch) {
        var x = 4 + ch * 60;
        var y = 30;
        var playing = sfx.playing(ch);
        var col = playing ? 11 : 5;

        if (ch === flash_ch && flash_timer > 0) col = 10;

        gfx.rectfill(x, y, x + 50, y + 20, col);
        gfx.rect(x, y, x + 50, y + 20, 7);
        gfx.print("ch " + ch, x + 4, y + 2, 0);
        gfx.print(playing ? "PLAY" : "idle", x + 4, y + 10, 0);
    }

    // SFX volume bar
    gfx.print("sfx vol", 4, 58, 6);
    gfx.rect(50, 58, 200, 64, 5);
    gfx.rectfill(50, 58, 50 + math.flr(sfx_vol * 150), 64, 11);
    gfx.print(math.flr(sfx_vol * 100) + "%", 206, 58, 7);

    // --- Music ---
    gfx.print("music", 4, 78, 6);
    var mus_col = mus_playing ? 11 : 5;
    gfx.rectfill(4, 88, 240, 108, mus_col);
    gfx.rect(4, 88, 240, 108, 7);
    gfx.print(mus_playing ? "PLAYING" : "STOPPED", 8, 92, 0);

    // Music volume bar
    gfx.print("mus vol", 4, 116, 6);
    gfx.rect(50, 116, 200, 122, 5);
    gfx.rectfill(50, 116, 50 + math.flr(mus_vol * 150), 122, 12);
    gfx.print(math.flr(mus_vol * 100) + "%", 206, 116, 7);

    // --- Status ---
    gfx.print(STATUS, 4, 136, 10);

    // --- Controls ---
    gfx.print("a/b/c/d = play sfx 0-3", 4, 152, 5);
    gfx.print("w = toggle music        up/dn = sfx vol", 4, 160, 5);
    gfx.print("left/right = music vol", 4, 168, 5);
}
