// PostFX Demo
//
// Shows how to:
//   - Push / pop effects with postfx.push() / postfx.pop()
//   - Use postfx.use() for named activation
//   - Stack multiple effects with postfx.stack()
//   - Adjust intensity and parameters
//   - Query available effects
//   - Save / restore the effect stack
//
// Number keys 1-4 toggle individual effects.
// S = stack all.  C = clear.  +/- = adjust strength.

// --- State -----------------------------------------------------------

var effects; // filled from postfx.available()
var active = {}; // name -> bool
var strength = 1.0;
var t = 0; // time accumulator for the demo scene
var debug_flash = [0, 0, 0, 0];

// --- Callbacks -------------------------------------------------------

var smooth_fps = 60;
var fps_history = [];
var fps_hist_idx = 0;
var FPS_HIST_LEN = 50;
for (var _i = 0; _i < FPS_HIST_LEN; ++_i) fps_history.push(60);

function draw_fps_widget() {
    var wx = 320 - FPS_HIST_LEN - 4;
    var wy = 0;
    var ww = FPS_HIST_LEN + 4;
    var gh = 16;
    var target = sys.targetFps();
    gfx.rectfill(wx, wy, wx + ww - 1, wy + 8 + gh + 1, 0);
    gfx.print(math.flr(smooth_fps) + " FPS", wx + 2, wy + 1, 7);
    gfx.rect(wx + 1, wy + 8, wx + ww - 2, wy + 8 + gh, 5);
    for (var idx = 1; idx < FPS_HIST_LEN; ++idx) {
        var i0 = (fps_hist_idx + idx - 1) % FPS_HIST_LEN;
        var i1 = (fps_hist_idx + idx) % FPS_HIST_LEN;
        var v0 = math.clamp(fps_history[i0] / target, 0, 1);
        var v1 = math.clamp(fps_history[i1] / target, 0, 1);
        var y0 = wy + 8 + gh - 1 - math.flr(v0 * (gh - 2));
        var y1 = wy + 8 + gh - 1 - math.flr(v1 * (gh - 2));
        var clr = v1 > 0.9 ? 11 : v1 > 0.5 ? 9 : 8;
        gfx.line(wx + 2 + idx - 1, y0, wx + 2 + idx, y1, clr);
    }
}

function _init() {
    effects = postfx.available();
    for (var i = 0; i < effects.length; ++i) {
        active[effects[i]] = false;
    }
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;
    t += dt;

    // Decay flash timers
    for (var f = 0; f < 4; ++f) {
        if (debug_flash[f] > 0) debug_flash[f] -= dt;
    }

    // Toggle effects: Z=CRT, X=scanlines, C=bloom, V=aberration
    if (key.btnp(key.Z)) {
        toggle("crt");
        debug_flash[0] = 0.3;
    }
    if (key.btnp(key.X)) {
        toggle("scanlines");
        debug_flash[1] = 0.3;
    }
    if (key.btnp(key.C)) {
        toggle("bloom");
        debug_flash[2] = 0.3;
    }
    if (key.btnp(key.V)) {
        toggle("aberration");
        debug_flash[3] = 0.3;
    }

    // S = stack all active at once
    if (key.btnp(key.SPACE)) {
        var names = [];
        for (var i = 1; i < effects.length; ++i) {
            // skip "none"
            names.push(effects[i]);
            active[effects[i]] = true;
        }
        postfx.stack(names);
        return;
    }

    // E = clear all
    if (key.btnp(key.E)) {
        postfx.clear();
        for (var k in active) active[k] = false;
    }

    // Strength control
    if (key.btnp(key.UP)) {
        strength = math.flr(math.clamp(strength + 0.1, 0.1, 2.0) * 10 + 0.5) / 10;
        rebuild_stack();
    }
    if (key.btnp(key.DOWN)) {
        strength = math.flr(math.clamp(strength - 0.1, 0.1, 2.0) * 10 + 0.5) / 10;
        rebuild_stack();
    }
}

function toggle(name) {
    active[name] = !active[name];
    rebuild_stack();
}

function rebuild_stack() {
    postfx.clear();
    for (var i = 1; i < effects.length; ++i) {
        var name = effects[i];
        if (active[name]) {
            postfx.push(i, strength);
        }
    }
}

function _draw() {
    gfx.cls(1);

    // --- Draw a colourful demo scene for the effects to act on ---
    draw_demo_scene();

    // --- HUD (drawn on top, affected by postfx too) ---
    gfx.rectfill(0, 0, 319, 40, 0);
    gfx.print("=== postfx demo ===", 100, 2, 7);

    // Show which effects are active + key debug
    var labels = ["crt", "scanlines", "bloom", "aberration"];
    var keycodes = [key.Z, key.X, key.C, key.V];
    var keynames = ["Z", "X", "C", "V"];
    for (var i = 0; i < labels.length; ++i) {
        var on = active[labels[i]];
        var held = key.btn(keycodes[i]);
        var col = on ? 11 : 5;
        // Flash yellow on press
        if (debug_flash[i] > 0) col = 10;
        var lx = 4 + i * 78;
        gfx.print(keynames[i] + ":" + labels[i], lx, 12, col);
        // Show held indicator
        gfx.rectfill(lx, 20, lx + 4, 24, held ? 11 : 2);
    }

    gfx.print("strength: " + math.flr(strength * 100) + "%", 4, 26, 7);
    gfx.print("up/dn=strength  spc=all  e=clear", 4, 34, 5);

    // FPS widget
    draw_fps_widget();
}

// A colourful test scene: gradient, shapes, text
function draw_demo_scene() {
    // Colour bars
    for (var i = 0; i < 16; ++i) {
        gfx.rectfill(i * 20, 44, i * 20 + 19, 64, i);
    }

    // Bouncing circles
    for (var i = 0; i < 5; ++i) {
        var cx = 40 + i * 60;
        var cy = 110 + math.sin(t * 0.3 + i * 0.2) * 30;
        gfx.circfill(cx, cy, 12, 8 + i);
        gfx.circ(cx, cy, 12, 7);
    }

    // Moving text
    var tx = 80 + math.sin(t * 0.15) * 60;
    gfx.print("dithr postfx", tx, 80, 7);

    // Diagonal lines
    for (var i = 0; i < 8; ++i) {
        gfx.line(0, 70 + i * 15, 319, 100 + i * 15, 2 + (i % 6));
    }

    // Checkerboard in corner
    for (var y = 150; y < 180; y += 4) {
        for (var x = 240; x < 320; x += 4) {
            var c = (((x / 4) | 0) + ((y / 4) | 0)) % 2 === 0 ? 7 : 0;
            gfx.rectfill(x, y, x + 3, y + 3, c);
        }
    }
}
