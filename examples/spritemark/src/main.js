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

// --- Callbacks -------------------------------------------------------

function _init() {
    gfx.cls(0);
    // Start with an initial batch
    spawn(SPAWN_COUNT);
}

function _update(dt) {
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

    // FPS
    gfx.print("fps: " + math.flr(sys.fps()), 2, 10, 7);
}
