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
const SPEED = 60; // pixels per second (scaled by dt)
const SCREEN_W = 320;
const SCREEN_H = 180;
const TILE_SIZE = 16;

// Sprite indices in the sheet (first row of tiles)
const SPR_CHARS = [0, 1, 2];

// Palette index used by the sprite foreground.  White (0xFFFFFF) from the
// 1-bit PNG quantises to index 30 (0xFEFEFEFF) in the default palette.
const REMAP_FROM = 30;

// --- State (structure-of-arrays for cache-friendly iteration) --------

const spX = [];
const spY = [];
const spVx = [];
const spVy = [];
const spSpr = []; // resolved tile index (skip lookup at draw time)
const spFlip = [];
const spCol = []; // per-sprite palette colour
let total = 0;

// Screen corners used as spawn origins
const CORNERS = [
    [0, 0],
    [SCREEN_W - TILE_SIZE, 0],
    [0, SCREEN_H - TILE_SIZE],
    [SCREEN_W - TILE_SIZE, SCREEN_H - TILE_SIZE],
];

// --- Helpers ---------------------------------------------------------

function spawn(count) {
    for (let i = 0; i < count; ++i) {
        const corner = CORNERS[math.rndInt(CORNERS.length)];
        spX.push(corner[0]);
        spY.push(corner[1]);
        // Velocity aimed away from the chosen corner towards centre
        const dirX = corner[0] === 0 ? 1 : -1;
        const dirY = corner[1] === 0 ? 1 : -1;
        spVx.push(dirX * math.rndRange(0.3, 1));
        spVy.push(dirY * math.rndRange(0.3, 1));
        spSpr.push(SPR_CHARS[math.rndInt(SPR_CHARS.length)]);
        spFlip.push(math.rnd() > 0.5);
        // Random colour from palette (skip 0 = background/transparent)
        spCol.push(1 + math.rndInt(15));
    }
    total = spX.length;
}

// --- Callbacks -------------------------------------------------------

function _init() {
    gfx.cls(0);
    // Start with an initial batch
    spawn(SPAWN_COUNT);
}

function _fixedUpdate(dt) {
    // Physics (SOA — flat array access, no property hash lookups)
    const right = SCREEN_W - TILE_SIZE;
    const bottom = SCREEN_H - TILE_SIZE;
    const numChars = SPR_CHARS.length;
    const step = dt * SPEED;
    for (let i = 0; i < total; ++i) {
        spX[i] += spVx[i] * step;
        spY[i] += spVy[i] * step;

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
            spVy[i] = -spVy[i];
            spSpr[i] = (spSpr[i] + 1) % numChars;
        } else if (spY[i] < 0) {
            spY[i] = 0;
            spVy[i] = -spVy[i];
        }
    }
}

function _update(dt) {
    // Spawn more on click or space
    if (mouse.btnp() || key.btnp(key.SPACE)) {
        spawn(SPAWN_COUNT);
    }
}

function _draw() {
    gfx.cls(1);

    // Draw all sprites (per-sprite colour via palette remap)
    for (let i = 0; i < total; ++i) {
        gfx.pal(REMAP_FROM, spCol[i]);
        gfx.spr(spSpr[i], spX[i], spY[i], 1, 1, spFlip[i], false);
    }
    gfx.pal(); // reset palette

    // HUD background
    gfx.rectfill(0, 0, 120, 18, 0);

    // Sprite count
    gfx.print(`sprites: ${total}`, 2, 2, 7);

    // FPS widget
    sys.drawFps();
}
