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

// --- Callbacks -------------------------------------------------------

function _init() {
    effects = postfx.available();
    for (var i = 0; i < effects.length; ++i) {
        active[effects[i]] = false;
    }
}

function _update(dt) {
    t += dt;

    // Toggle effects: Z=CRT, X=scanlines, C=bloom, V=aberration
    if (key.btnp(key.Z)) toggle("crt");
    if (key.btnp(key.X)) toggle("scanlines");
    if (key.btnp(key.C)) toggle("bloom");
    if (key.btnp(key.V)) toggle("aberration");

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
    if (key.btnp(key.UP)) strength = math.clamp(strength + 0.1, 0.1, 2.0);
    if (key.btnp(key.DOWN)) strength = math.clamp(strength - 0.1, 0.1, 2.0);
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

    // Show which effects are active
    var labels = ["crt", "scanlines", "bloom", "aberration"];
    var keys = ["Z", "X", "C", "V"];
    for (var i = 0; i < labels.length; ++i) {
        var on = active[labels[i]];
        var col = on ? 11 : 5;
        gfx.print(keys[i] + ":" + labels[i], 4 + i * 78, 12, col);
    }

    gfx.print("strength: " + math.flr(strength * 100) + "%", 4, 22, 7);
    gfx.print("up/dn=strength  spc=all  e=clear", 4, 32, 5);
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
    gfx.print("mvngin postfx", tx, 80, 7);

    // Diagonal lines
    for (var i = 0; i < 8; ++i) {
        gfx.line(0, 70 + i * 15, 319, 100 + i * 15, 2 + (i % 6));
    }

    // Checkerboard in corner
    for (var y = 150; y < 180; y += 4) {
        for (var x = 240; x < 320; x += 4) {
            var c = ((x + y) / 4) % 2 === 0 ? 7 : 0;
            gfx.rectfill(x, y, x + 3, y + 3, c);
        }
    }
}
