// Tilemap Demo
//
// Shows how to:
//   - Draw a tilemap with map.draw()
//   - Scroll the camera to follow a cursor
//   - Query tiles with map.get() / map.set()
//   - Use map.flag() for collision detection
//
// This demo generates a procedural tile grid in _init() since no Tiled
// JSON is loaded.  Replace with a real Tiled export for production use.
//
// When a real map is loaded via cart.json "maps", map.draw() renders
// tiles from the spritesheet automatically.

// --- Constants -------------------------------------------------------

const MAP_W = 40; // tiles wide
const MAP_H = 23; // tiles tall
const TILE = 8; // tile size in pixels

// Camera position (top-left)
let camX = 0;
let camY = 0;

// Cursor position in tiles
let curCx = 5;
let curCy = 5;

// Fake tile data (since we have no Tiled map loaded)
// We'll draw our own grid using pset/rectfill as a placeholder.
const tiles = [];

// --- Helpers ---------------------------------------------------------

function tileAt(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return 0;
    return tiles[cy * MAP_W + cx];
}

function setTile(cx, cy, t) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return;
    tiles[cy * MAP_W + cx] = t;
}

// Procedural map: border walls + random scatter
function generateMap() {
    for (let y = 0; y < MAP_H; ++y) {
        for (let x = 0; x < MAP_W; ++x) {
            if (x === 0 || y === 0 || x === MAP_W - 1 || y === MAP_H - 1) {
                tiles.push(1); // wall
            } else if (math.rnd() < 0.08) {
                tiles.push(2); // decoration
            } else {
                tiles.push(0); // floor
            }
        }
    }
}

// Tile colours for placeholder rendering
const tileColors = [1, 5, 3]; // floor=dark blue, wall=dark grey, deco=green



// --- Callbacks -------------------------------------------------------

export function _init(): void {
    math.seed(42);
    generateMap();
}

export function _update(_dt: number): void {
    // Move cursor with input actions
    if (input.btnp('left') && curCx > 0) --curCx;
    if (input.btnp('right') && curCx < MAP_W - 1) ++curCx;
    if (input.btnp('up') && curCy > 0) --curCy;
    if (input.btnp('down') && curCy < MAP_H - 1) ++curCy;

    // Toggle tile under cursor with Z
    if (key.btnp(key.Z)) {
        const t = tileAt(curCx, curCy);
        setTile(curCx, curCy, t === 0 ? 1 : 0);
    }

    // Smooth camera follow
    const targetX = curCx * TILE - 160 + 4;
    const targetY = curCy * TILE - 90 + 4;
    camX += (targetX - camX) * 0.1;
    camY += (targetY - camY) * 0.1;

    // Clamp camera
    const maxCx = MAP_W * TILE - 320;
    const maxCy = MAP_H * TILE - 180;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > maxCx) camX = maxCx;
    if (camY > maxCy) camY = maxCy;
}

export function _draw(): void {
    gfx.cls(0);

    // Apply camera offset
    const ox = -math.flr(camX);
    const oy = -math.flr(camY);
    gfx.camera(-ox, -oy);

    // NOTE: With a real Tiled map, you would call:
    //   map.draw(0, 0, 0, 0);
    // and the engine draws the tilemap from the spritesheet automatically.
    // Since we have no map loaded, we draw placeholder tiles manually:

    // Batch-draw the entire visible tilemap in one native call
    gfx.tilemap(tiles, MAP_W, MAP_H, tileColors);

    // Overlay details on special tiles (only walls/deco, not every tile)
    let startTx = math.flr(camX / TILE);
    let startTy = math.flr(camY / TILE);
    let endTx = startTx + 41;
    let endTy = startTy + 24;

    if (endTx > MAP_W) endTx = MAP_W;
    if (endTy > MAP_H) endTy = MAP_H;
    if (startTx < 0) startTx = 0;
    if (startTy < 0) startTy = 0;

    for (let ty = startTy; ty < endTy; ++ty) {
        for (let tx = startTx; tx < endTx; ++tx) {
            const t = tileAt(tx, ty);
            if (t === 1) {
                const px = tx * TILE;
                const py = ty * TILE;
                gfx.rect(px, py, px + TILE - 1, py + TILE - 1, 6);
            } else if (t === 2) {
                const px = tx * TILE;
                const py = ty * TILE;
                gfx.pset(px + 3, py + 3, 11);
                gfx.pset(px + 4, py + 3, 11);
                gfx.pset(px + 3, py + 4, 11);
                gfx.pset(px + 4, py + 4, 11);
            }
        }
    }

    // Draw cursor
    const cxPx = curCx * TILE;
    const cyPx = curCy * TILE;
    gfx.rect(cxPx - 1, cyPx - 1, cxPx + TILE, cyPx + TILE, 8);

    // Reset camera for HUD
    gfx.camera(0, 0);

    // HUD
    gfx.rectfill(0, 0, 319, 10, 0);
    gfx.print(
        `tilemap demo  cursor:${curCx},${curCy}  tile:${tileAt(curCx, curCy)}  Z=toggle`,
        2,
        2,
        7,
    );

    // FPS widget
    sys.drawFps();
}
