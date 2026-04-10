// Map Demo — tilemap API, camera helpers, map objects, multi-layer, levels
//
// Demonstrates: map.create, map.draw, map.get, map.set, map.flag,
//   map.width, map.height, map.layers, map.levels, map.load,
//   map.currentLevel, map.objects, map.object, map.objectsIn,
//   map.objectsWith, map.addLayer, map.removeLayer, map.resize, map.data,
//   cam.set, cam.get, cam.reset, cam.follow,
//   gfx.fget, gfx.fset

// --- FPS widget ----------------------------------------------------------
let smoothFps = 60;
let fpsHistory = [];
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

// -------------------------------------------------------------------------
// Tile legend (drawn directly into spritesheet at runtime)
//   0 = empty (transparent)
//   1 = grass (green)
//   2 = wall  (brown)  — solid
//   3 = water (blue)   — solid
//   4 = path  (beige)
//   5 = spawn (yellow marker)
//   6 = chest (orange marker)
// -------------------------------------------------------------------------

const TILE_COLS = [0, 11, 4, 12, 15, 10, 9];

const MAP_W = 80;
const MAP_H = 45;
const TILE = 8;

let px = 0;
let py = 0;
const SPEED = 80; // pixels per second

let currentLevelName = '';
let levelNames = [];
let levelIdx = 0;

let infoMsg = '';
let infoTimer = 0;
const PLAYER_SPRITE = 7; // sprite index for player tile

function buildSheet() {
    gfx.sheetCreate(128, 128, 8, 8);
    for (let t = 0; t < TILE_COLS.length; ++t) {
        const sx = (t % 16) * 8;
        const sy = math.flr(t / 16) * 8;
        for (let py2 = 0; py2 < 8; ++py2) {
            for (let px2 = 0; px2 < 8; ++px2) {
                if (t === 0) continue; // keep transparent
                gfx.sset(sx + px2, sy + py2, TILE_COLS[t]);
            }
        }
    }
    // Player sprite (index 7) — red square
    const psx = (PLAYER_SPRITE % 16) * 8;
    const psy = math.flr(PLAYER_SPRITE / 16) * 8;
    for (let py3 = 0; py3 < 8; ++py3) {
        for (let px3 = 0; px3 < 8; ++px3) {
            gfx.sset(psx + px3, psy + py3, 8);
        }
    }
    // Mark solid tiles via flag bit 0
    // map.flag uses tile-1 as sprite index, so tile N → sprite N-1
    gfx.fset(1, 0, 1); // wall (tile 2)  → sprite 1
    gfx.fset(2, 0, 1); // water (tile 3) → sprite 2
}

function buildLevel(name) {
    map.create(MAP_W, MAP_H, name);

    // Fill with grass
    for (let cy = 0; cy < MAP_H; ++cy) {
        for (let cx = 0; cx < MAP_W; ++cx) {
            map.set(cx, cy, 1);
        }
    }

    // Border walls
    for (let cx2 = 0; cx2 < MAP_W; ++cx2) {
        map.set(cx2, 0, 2);
        map.set(cx2, MAP_H - 1, 2);
    }
    for (let cy2 = 0; cy2 < MAP_H; ++cy2) {
        map.set(0, cy2, 2);
        map.set(MAP_W - 1, cy2, 2);
    }

    // Seeded random
    let seed = 0;
    for (let ch = 0; ch < name.length; ++ch) seed += name.charCodeAt(ch);
    math.seed(seed);

    // Water pools
    for (let w = 0; w < 10; ++w) {
        const wx2 = math.rndInt(MAP_W - 10) + 3;
        const wy2 = math.rndInt(MAP_H - 8) + 3;
        const pw = math.rndInt(3) + 3;
        const ph = math.rndInt(2) + 2;
        for (let dy = 0; dy < ph; ++dy) {
            for (let dx = 0; dx < pw; ++dx) {
                map.set(wx2 + dx, wy2 + dy, 3);
            }
        }
    }

    // Interior walls
    for (let wb = 0; wb < 20; ++wb) {
        const bx = math.rndInt(MAP_W - 10) + 3;
        const by2 = math.rndInt(MAP_H - 8) + 2;
        const horiz = math.rnd(1) > 0.5;
        const wlen = math.rndInt(4) + 3;
        for (let s = 0; s < wlen; ++s) {
            if (horiz) map.set(bx + s, by2, 2);
            else map.set(bx, by2 + s, 2);
        }
    }

    // Paths
    for (let px3 = 2; px3 < MAP_W - 2; ++px3) {
        map.set(px3, math.flr(MAP_H / 2), 4);
    }

    // Ensure spawn area is clear (4x4 tiles around spawn)
    for (let sy = 1; sy <= 4; ++sy) {
        for (let sx = 1; sx <= 4; ++sx) {
            map.set(sx, sy, 1);
        }
    }
    map.set(2, 2, 4);
    map.set(3, 2, 4);

    // Add a second layer for decoration
    map.addLayer('overlay');
    // Place spawn marker on overlay layer
    map.set(2, 2, 5, 1);

    // Place chest objects via overlay
    for (let c = 0; c < 8; ++c) {
        const cx3 = math.rndInt(MAP_W - 6) + 3;
        const cy3 = math.rndInt(MAP_H - 6) + 3;
        map.set(cx3, cy3, 6, 1);
    }
}

function showInfo(msg) {
    infoMsg = msg;
    infoTimer = 3.0;
}

function _init() {
    buildSheet();

    // Create overworld
    buildLevel('overworld');

    levelNames = map.levels();
    levelIdx = 0;
    map.load(levelNames[levelIdx]);
    currentLevelName = map.currentLevel();

    // Display map metadata
    const w = map.width();
    const h = map.height();
    const layers = map.layers();
    sys.log(`Level: ${currentLevelName} (${w}x${h}) layers: ${layers.join(', ')}`);

    // Get full map data for logging
    const d = map.data();
    if (d) sys.log(`Map data name: ${d.name}`);

    // Spawn at the spawn marker
    px = 2 * TILE;
    py = 2 * TILE;
    cam.reset();

    showInfo('Arrow/WASD=move | Action=pick up chests');
}

function _update(dt) {
    smoothFps = math.lerp(smoothFps, sys.fps(), 0.05);
    fpsHistory[fpsHistIdx] = smoothFps;
    fpsHistIdx = (fpsHistIdx + 1) % FPS_HIST_LEN;

    if (infoTimer > 0) infoTimer -= dt;

    // Movement with collision via map.flag
    let dx = 0;
    let dy = 0;
    if (input.btn('left')) dx = -1;
    if (input.btn('right')) dx = 1;
    if (input.btn('up')) dy = -1;
    if (input.btn('down')) dy = 1;

    const nx = px + dx * SPEED * dt;
    const ny = py + dy * SPEED * dt;

    // Horizontal
    if (dx !== 0) {
        const checkCx = math.flr((nx + (dx > 0 ? 7 : 0)) / TILE);
        if (
            !map.flag(checkCx, math.flr(py / TILE), 0) &&
            !map.flag(checkCx, math.flr((py + 7) / TILE), 0)
        ) {
            px = nx;
        }
    }

    // Vertical
    if (dy !== 0) {
        const checkCy = math.flr((ny + (dy > 0 ? 7 : 0)) / TILE);
        if (
            !map.flag(math.flr(px / TILE), checkCy, 0) &&
            !map.flag(math.flr((px + 7) / TILE), checkCy, 0)
        ) {
            py = ny;
        }
    }

    // Query objects near the player via map.objectsIn (overlay layer)
    const _objectsNear = map.objectsIn(px - 16, py - 16, 40, 40);

    // Action button — pick up chests nearby
    if (input.btnp('action')) {
        const pcx = math.flr(px / TILE);
        const pcy = math.flr(py / TILE);
        const tileOverlay = map.get(pcx, pcy, 1);
        if (tileOverlay === 6) {
            map.set(pcx, pcy, 0, 1); // clear chest
            showInfo('Found a treasure chest!');
        }
    }

    // Camera follow — track player, centered on screen
    cam.follow(px - 160 + 4, py - 90 + 4, 0.1);

    // Clamp engine camera to map bounds
    let c = cam.get();
    const maxCx = MAP_W * TILE - 320;
    const maxCy = MAP_H * TILE - 180;
    if (c.x < 0) cam.set(0, c.y);
    if (c.y < 0) cam.set(cam.get().x, 0);
    c = cam.get();
    if (c.x > maxCx) cam.set(maxCx, c.y);
    if (c.y > maxCy) cam.set(cam.get().x, maxCy);
}

function _draw() {
    gfx.cls(0);

    // Draw both layers
    map.draw(0, 0, 0, 0); // base layer
    map.draw(0, 0, 0, 0, undefined, undefined, 1); // overlay layer

    // Draw player as sprite (respects engine camera automatically)
    gfx.spr(PLAYER_SPRITE, px, py);

    // HUD (screen-space — temporarily reset camera)
    const savedCam = cam.get();
    cam.set(0, 0);

    // Info message
    if (infoTimer > 0) {
        gfx.rectfill(0, 170, 319, 179, 1);
        gfx.print(infoMsg, 2, 172, 7);
    }

    // Level / tile info
    const pcx = math.flr(px / TILE);
    const pcy = math.flr(py / TILE);
    const tileId = map.get(pcx, pcy);
    const isSolid = map.flag(pcx, pcy, 0);
    gfx.rectfill(0, 0, 180, 8, 0);
    gfx.print(
        `${currentLevelName} tile:${tileId}${isSolid ? ' SOLID' : ''} (${pcx},${pcy})`,
        1,
        1,
        6,
    );

    drawFpsWidget();

    cam.set(savedCam.x, savedCam.y);
}

function _save() {
    return {
        px,
        py,
        levelIdx,
        smoothFps,
        fpsHistory,
        fpsHistIdx,
    };
}

function _restore(s) {
    px = s.px;
    py = s.py;
    levelIdx = s.levelIdx;
    smoothFps = s.smoothFps;
    fpsHistory = s.fpsHistory;
    fpsHistIdx = s.fpsHistIdx;
    map.load(levelNames[levelIdx]);
    currentLevelName = map.currentLevel();
}
