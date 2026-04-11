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

const SPAWN_COUNT = 200; // sprites added per click/press
const GRAVITY = 0.5; // downward acceleration per frame
const SCREEN_W = 320;
const SCREEN_H = 180;
const TILE_SIZE = 16;

// Sprite indices in the sheet (first row of tiles)
const SPR_CHARS = [0, 1, 2];

// --- State (structure-of-arrays for cache-friendly iteration) --------

const spX = [];
const spY = [];
const spVx = [];
const spVy = [];
const spSpr = []; // resolved tile index (skip lookup at draw time)
const spFlip = [];
let total = 0;

// --- Helpers ---------------------------------------------------------

function spawn(count) {
    for (let i = 0; i < count; ++i) {
        spX.push(math.rnd(SCREEN_W - TILE_SIZE));
        spY.push(math.rnd(SCREEN_H / 2));
        spVx.push(math.rndRange(-3, 3));
        spVy.push(math.rndRange(-2, 0));
        spSpr.push(SPR_CHARS[math.rndInt(SPR_CHARS.length)]);
        spFlip.push(math.rnd() > 0.5);
    }
    total = spX.length;
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

    // Physics (SOA — flat array access, no property hash lookups)
    const right = SCREEN_W - TILE_SIZE;
    const bottom = SCREEN_H - TILE_SIZE;
    const numChars = SPR_CHARS.length;
    for (let i = 0; i < total; ++i) {
        spVy[i] += GRAVITY;
        spX[i] += spVx[i];
        spY[i] += spVy[i];

        // Bounce off edges
        if (spX[i] < 0) {
            spX[i] = 0;
            spVx[i] = -spVx[i];
            spFlip[i] = !spFlip[i];
        } else if (spX[i] > right) {
            spX[i] = right;
            spVx[i] = -spVx[i];
            spFlip[i] = !spFlip[i];
        }

        if (spY[i] > bottom) {
            spY[i] = bottom;
            spVy[i] = -spVy[i] * 0.85;
            // Cycle animation frame on each bounce
            spSpr[i] = (spSpr[i] + 1) % numChars;
        } else if (spY[i] < 0) {
            spY[i] = 0;
            spVy[i] = -spVy[i];
        }
    }
}

function _draw() {
    gfx.cls(1);

    // Draw all sprites (pre-resolved tile index, no object access)
    for (let i = 0; i < total; ++i) {
        gfx.spr(spSpr[i], spX[i], spY[i], 1, 1, spFlip[i], false);
    }

    // HUD background
    gfx.rectfill(0, 0, 120, 18, 0);

    // Sprite count
    gfx.print(`sprites: ${total}`, 2, 2, 7);

    // FPS widget
    sys.drawFps();
}
