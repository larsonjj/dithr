// Spritemark — Bunnymark-style sprite benchmark
//
// Click or press SPACE to spawn more sprites. Each sprite is a 1-bit
// platformer character that bounces around the screen with gravity.
// The counter and FPS are shown in the top-left corner.
//
// Spritesheet: 16x16 tiles. Uses tiles from Kenney's 1-Bit Platformer Pack.
//   Tile 0 = character idle
//   Tile 1 = character walk frame 1
//   Tile 2 = character walk frame 2

// --- Config ----------------------------------------------------------

var SPAWN_COUNT = 200; // sprites added per click/press
var GRAVITY = 0.5; // downward acceleration per frame
var SCREEN_W = 320;
var SCREEN_H = 180;
var TILE_SIZE = 16;

// Sprite indices in the sheet (first row of tiles)
var SPR_CHARS = [0, 1, 2];

// --- State (structure-of-arrays for cache-friendly iteration) --------

var sp_x = [];
var sp_y = [];
var sp_vx = [];
var sp_vy = [];
var sp_spr = []; // resolved tile index (skip lookup at draw time)
var sp_flip = [];
var total = 0;

// --- Helpers ---------------------------------------------------------

function spawn(count) {
    for (var i = 0; i < count; ++i) {
        sp_x.push(math.rnd(SCREEN_W - TILE_SIZE));
        sp_y.push(math.rnd(SCREEN_H / 2));
        sp_vx.push(math.rnd_range(-3, 3));
        sp_vy.push(math.rnd_range(-2, 0));
        sp_spr.push(SPR_CHARS[math.rnd_int(SPR_CHARS.length)]);
        sp_flip.push(math.rnd() > 0.5);
    }
    total = sp_x.length;
}

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
    var target = sys.target_fps();
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

// --- Callbacks -------------------------------------------------------

function _init() {
    gfx.cls(0);
    // Start with an initial batch
    spawn(SPAWN_COUNT);
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;
    // Spawn more on click or space
    if (mouse.btnp() || key.btnp(key.SPACE)) {
        spawn(SPAWN_COUNT);
    }

    // Physics (SOA — flat array access, no property hash lookups)
    var right = SCREEN_W - TILE_SIZE;
    var bottom = SCREEN_H - TILE_SIZE;
    var num_chars = SPR_CHARS.length;
    for (var i = 0; i < total; ++i) {
        sp_vy[i] += GRAVITY;
        sp_x[i] += sp_vx[i];
        sp_y[i] += sp_vy[i];

        // Bounce off edges
        if (sp_x[i] < 0) {
            sp_x[i] = 0;
            sp_vx[i] = -sp_vx[i];
            sp_flip[i] = !sp_flip[i];
        } else if (sp_x[i] > right) {
            sp_x[i] = right;
            sp_vx[i] = -sp_vx[i];
            sp_flip[i] = !sp_flip[i];
        }

        if (sp_y[i] > bottom) {
            sp_y[i] = bottom;
            sp_vy[i] = -sp_vy[i] * 0.85;
            // Cycle animation frame on each bounce
            sp_spr[i] = (sp_spr[i] + 1) % num_chars;
        } else if (sp_y[i] < 0) {
            sp_y[i] = 0;
            sp_vy[i] = -sp_vy[i];
        }
    }
}

function _draw() {
    gfx.cls(1);

    // Draw all sprites (pre-resolved tile index, no object access)
    for (var i = 0; i < total; ++i) {
        gfx.spr(sp_spr[i], sp_x[i], sp_y[i], 1, 1, sp_flip[i], false);
    }

    // HUD background
    gfx.rectfill(0, 0, 120, 18, 0);

    // Sprite count
    gfx.print("sprites: " + total, 2, 2, 7);

    // FPS widget
    draw_fps_widget();
}
