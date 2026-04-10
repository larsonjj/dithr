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

// --- State -----------------------------------------------------------

var sprites = [];
var total = 0;

// --- Helpers ---------------------------------------------------------

function spawn(count) {
    for (var i = 0; i < count; ++i) {
        sprites.push({
            x: math.rnd(SCREEN_W - TILE_SIZE),
            y: math.rnd(SCREEN_H / 2),
            vx: math.rnd_range(-3, 3),
            vy: math.rnd_range(-2, 0),
            frame: math.rnd_int(SPR_CHARS.length),
            flip: math.rnd() > 0.5,
        });
    }
    total = sprites.length;
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

    // Physics
    for (var i = 0; i < sprites.length; ++i) {
        var s = sprites[i];

        s.vy += GRAVITY;
        s.x += s.vx;
        s.y += s.vy;

        // Bounce off edges
        if (s.x < 0) {
            s.x = 0;
            s.vx = -s.vx;
            s.flip = !s.flip;
        } else if (s.x > SCREEN_W - TILE_SIZE) {
            s.x = SCREEN_W - TILE_SIZE;
            s.vx = -s.vx;
            s.flip = !s.flip;
        }

        if (s.y > SCREEN_H - TILE_SIZE) {
            s.y = SCREEN_H - TILE_SIZE;
            s.vy = -s.vy * 0.85;
            // Cycle animation frame on each bounce
            s.frame = (s.frame + 1) % SPR_CHARS.length;
        } else if (s.y < 0) {
            s.y = 0;
            s.vy = -s.vy;
        }
    }
}

function _draw() {
    gfx.cls(1);

    // Draw all sprites
    for (var i = 0; i < sprites.length; ++i) {
        var s = sprites[i];
        gfx.spr(SPR_CHARS[s.frame], s.x, s.y, 1, 1, s.flip, false);
    }

    // HUD background
    gfx.rectfill(0, 0, 120, 18, 0);

    // Sprite count
    gfx.print("sprites: " + total, 2, 2, 7);

    // FPS widget
    draw_fps_widget();
}
