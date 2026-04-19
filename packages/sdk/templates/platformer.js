// Platformer template — tile-based collision with acceleration physics
//
// Demonstrates:
//   - Acceleration-based horizontal movement with run button
//   - Variable-height jump (hold jump to go higher)
//   - Tile-based collision with a simple level
//   - Coyote time (jump grace period after leaving a ledge)
//   - Collectible coins
//
// Extend this by adding enemies, more tile types, scrolling, etc.

const TILE = 8;
const MAP_W = 40; // level width in tiles
const MAP_H = 23; // level height in tiles (fits 180px)

// Physics
const WALK_ACCEL = 0.046875;
const SKID_DECEL = 0.15;
const RELEASE_DECEL = 0.046875;
const WALK_MAX = 0.75;
const RUN_MAX = 1.3;
const JUMP_VEL = -2.6;
const GRAV_HOLD = 0.11;
const GRAV_RELEASE = 0.375;
const MAX_FALL = 2.25;
const COYOTE_TICKS = 4;

// Tile types
const T_EMPTY = 0;
const T_SOLID = 1;
const T_COIN = 2;

let tiles = [];
let px = 16;
let py = 0;
let vx = 0;
let vy = 0;
let grounded = false;
let facing = 1;
let jumping = false;
let coyote = 0;
let score = 0;

function solidAt(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return true;
    return tiles[cy * MAP_W + cx] === T_SOLID;
}

function tileAt(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return T_SOLID;
    return tiles[cy * MAP_W + cx];
}

function setTile(cx, cy, v) {
    if (cx >= 0 && cy >= 0 && cx < MAP_W && cy < MAP_H)
        tiles[cy * MAP_W + cx] = v;
}

function buildLevel() {
    tiles = [];
    for (let i = 0; i < MAP_W * MAP_H; ++i) tiles.push(T_EMPTY);

    const floor = MAP_H - 2;

    // Ground with a couple of gaps
    for (let x = 0; x < MAP_W; ++x) {
        if ((x >= 12 && x <= 14) || (x >= 26 && x <= 28)) continue;
        for (let y = floor; y < MAP_H; ++y) setTile(x, y, T_SOLID);
    }

    // Platforms
    for (let x = 8; x <= 12; ++x) setTile(x, floor - 4, T_SOLID);
    for (let x = 18; x <= 22; ++x) setTile(x, floor - 5, T_SOLID);
    for (let x = 30; x <= 35; ++x) setTile(x, floor - 4, T_SOLID);

    // Coins
    for (let x = 9; x <= 11; ++x) setTile(x, floor - 5, T_COIN);
    for (let x = 19; x <= 21; ++x) setTile(x, floor - 6, T_COIN);
    for (let x = 31; x <= 34; ++x) setTile(x, floor - 5, T_COIN);
}

function collideX() {
    const left = math.flr(px / TILE);
    const right = math.flr((px + 5) / TILE);
    const top = math.flr(py / TILE);
    const bot = math.flr((py + 6) / TILE);
    for (let ty = top; ty <= bot; ++ty) {
        if (solidAt(left, ty)) { px = (left + 1) * TILE; vx = 0; return; }
        if (solidAt(right, ty)) { px = right * TILE - 6; vx = 0; return; }
    }
}

function collideY() {
    const left = math.flr(px / TILE);
    const right = math.flr((px + 5) / TILE);
    const top = math.flr(py / TILE);
    const bot = math.flr((py + 6) / TILE);
    grounded = false;
    if (vy < 0) {
        for (let tx = left; tx <= right; ++tx) {
            if (solidAt(tx, top)) { py = (top + 1) * TILE; vy = 0; return; }
        }
    }
    for (let tx = left; tx <= right; ++tx) {
        if (solidAt(tx, bot)) { py = bot * TILE - 7; vy = 0; grounded = true; return; }
    }
}

function _init() {
    buildLevel();
    py = (MAP_H - 4) * TILE;
}

function _fixedUpdate(_dt) {
    const running = input.btn("run");
    const maxSpd = running ? RUN_MAX : WALK_MAX;
    const accel = WALK_ACCEL;

    if (input.btn("left") && !input.btn("right")) {
        facing = -1;
        vx = vx > 0 ? vx - SKID_DECEL : math.max(vx - accel, -maxSpd);
    } else if (input.btn("right") && !input.btn("left")) {
        facing = 1;
        vx = vx < 0 ? vx + SKID_DECEL : math.min(vx + accel, maxSpd);
    } else {
        if (vx > 0) { vx -= RELEASE_DECEL; if (vx < 0) vx = 0; }
        else if (vx < 0) { vx += RELEASE_DECEL; if (vx > 0) vx = 0; }
    }

    // Variable-height gravity
    if (jumping && input.btn("jump") && vy < 0) vy += GRAV_HOLD;
    else { vy += GRAV_RELEASE; jumping = false; }
    if (vy > MAX_FALL) vy = MAX_FALL;

    px += vx; collideX();
    py += vy; collideY();
    if (px < 0) { px = 0; vx = 0; }

    if (grounded) coyote = COYOTE_TICKS;
    else if (coyote > 0) coyote--;

    if (coyote > 0 && input.btnp("jump")) {
        vy = JUMP_VEL;
        jumping = true;
        coyote = 0;
        grounded = false;
    }

    // Collect coins
    const cx1 = math.flr(px / TILE);
    const cx2 = math.flr((px + 5) / TILE);
    const cy1 = math.flr(py / TILE);
    const cy2 = math.flr((py + 6) / TILE);
    for (let ty = cy1; ty <= cy2; ++ty) {
        for (let tx = cx1; tx <= cx2; ++tx) {
            if (tileAt(tx, ty) === T_COIN) {
                setTile(tx, ty, T_EMPTY);
                score += 10;
            }
        }
    }

    // Respawn if fallen
    if (py > MAP_H * TILE + 16) {
        px = 16; py = (MAP_H - 4) * TILE;
        vx = 0; vy = 0;
    }
}

function _draw() {
    gfx.cls(12);

    // Draw tiles
    for (let ty = 0; ty < MAP_H; ++ty) {
        for (let tx = 0; tx < MAP_W; ++tx) {
            const t = tileAt(tx, ty);
            const x = tx * TILE;
            const y = ty * TILE;
            if (t === T_SOLID) {
                gfx.rectfill(x, y, x + 7, y + 7, 4);
                gfx.line(x, y, x + 7, y, 15);
            } else if (t === T_COIN) {
                const r = 2 + math.abs(math.sin(sys.time() * 4));
                gfx.circfill(x + 4, y + 4, r, 10);
            }
        }
    }

    // Draw player
    const ipx = math.flr(px);
    const ipy = math.flr(py);
    gfx.rectfill(ipx, ipy + 2, ipx + 5, ipy + 6, 8);
    gfx.rectfill(ipx + 1, ipy, ipx + 4, ipy + 2, 15);
    gfx.line(ipx, ipy, ipx + 5, ipy, 8);
    gfx.pset(facing > 0 ? ipx + 3 : ipx + 2, ipy + 1, 0);

    // HUD
    gfx.print("score: " + score, 4, 4, 7);
    gfx.print("arrows: move  z: jump  x: run", 4, 170, 5);
}
