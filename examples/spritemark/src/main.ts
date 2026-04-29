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

const spX: number[] = [];
const spY: number[] = [];
const spVx: number[] = [];
const spVy: number[] = [];
const spSpr: number[] = []; // resolved tile index (skip lookup at draw time)
const spFlip: boolean[] = [];
const spCol: number[] = []; // per-sprite palette colour
let total = 0;

// Colour buckets: colBuckets[c] is an array of sprite indices with that colour.
// Avoids calling gfx.pal() per sprite — only once per colour group.
const NUM_COLORS = 15;
const colBuckets: number[][] = [];
for (let c = 0; c < NUM_COLORS; ++c) {
    colBuckets.push([]);
}

// Screen corners used as spawn origins
const CORNERS = [
    [0, 0],
    [SCREEN_W - TILE_SIZE, 0],
    [0, SCREEN_H - TILE_SIZE],
    [SCREEN_W - TILE_SIZE, SCREEN_H - TILE_SIZE],
];

// --- Helpers ---------------------------------------------------------

function spawn(count: number) {
    for (let i = 0; i < count; ++i) {
        const corner = CORNERS[math.rndInt(CORNERS.length)];
        const idx = total + i;
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
        const col = 1 + math.rndInt(15);
        spCol.push(col);
        colBuckets[col - 1].push(idx);
    }
    total = spX.length;
}

// --- Callbacks -------------------------------------------------------

export async function _init(): Promise<void> {
    await res.loadSprite('sheet', 'assets/sprites/spritesheet.png');
    res.setActiveSheet('sheet', { tileW: 8, tileH: 8 });
    gfx.cls(0);
    // Start with an initial batch
    spawn(SPAWN_COUNT);
}

export function _fixedUpdate(dt: number): void {
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

export function _update(_dt: number): void {
    // Spawn more on click or space
    if (mouse.btnp() || key.btnp(key.SPACE)) {
        spawn(SPAWN_COUNT);
    }
}

// Reusable batch buffer — grows as needed.
// Layout: [idx, x, y, flipX, flipY, ...] (5 ints per sprite)
const STRIDE = 5;
let batchBuf = new Int32Array(SPAWN_COUNT * STRIDE);

export function _draw(): void {
    gfx.cls(1);

    // Draw sprites grouped by colour using batch API.
    // One gfx.pal() + one gfx.sprBatch() per colour (~30 bridge calls
    // instead of ~4000 individual spr+pal calls).
    for (let c = 0; c < NUM_COLORS; ++c) {
        const bucket = colBuckets[c];
        const n = bucket.length;
        if (n === 0) continue;

        // Grow buffer if needed
        const need = n * STRIDE;
        if (batchBuf.length < need) {
            batchBuf = new Int32Array(need);
        }

        // Pack sprite data
        for (let j = 0; j < n; ++j) {
            const i = bucket[j];
            const off = j * STRIDE;
            batchBuf[off] = spSpr[i];
            batchBuf[off + 1] = spX[i];
            batchBuf[off + 2] = spY[i];
            batchBuf[off + 3] = spFlip[i] ? 1 : 0;
            batchBuf[off + 4] = 0; // no flip_y
        }

        gfx.pal(REMAP_FROM, c + 1);
        gfx.sprBatch(batchBuf, n);
    }
    gfx.pal(); // reset palette

    // HUD background
    gfx.rectfill(0, 0, 120, 18, 0);

    // Sprite count
    gfx.print(`sprites: ${total}`, 2, 2, 7);

    // FPS widget
    sys.drawFps();
}
