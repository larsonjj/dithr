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
    gfx.cls(0);
    // Start with an initial batch
    spawn(SPAWN_COUNT);
}

function _update(dt) {
    smoothFps = math.lerp(smoothFps, sys.fps(), 0.05);
    fpsHistory[fpsHistIdx] = smoothFps;
    fpsHistIdx = (fpsHistIdx + 1) % FPS_HIST_LEN;
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
    drawFpsWidget();
}
