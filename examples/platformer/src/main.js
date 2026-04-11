// Platformer Example
//
// A complete mini-platformer demonstrating:
//   - Gravity and velocity-based movement
//   - Tile-based collision with a procedural level
//   - Collectible coins with score
//   - Patrolling enemies with death/respawn
//   - Camera follow with smooth scrolling
//   - High score saved via cart.dset/dget
//
// All art is drawn with primitives (rectfill/circfill).  Replace the
// placeholder rendering with gfx.spr() calls once you add a spritesheet.

// --- Constants -------------------------------------------------------

const TILE = 8;
const MAP_W = 60; // level width in tiles
const MAP_H = 23; // level height in tiles (fits 180px)
const GRAVITY = 0.35;
const JUMP = -5.5;
const MOVE = 1.8;
const FRICTION = 0.85;

// Tile types
const T_EMPTY = 0;
const T_SOLID = 1;
const T_COIN = 2;
const T_SPIKE = 3;

// --- Level data (procedurally generated) -----------------------------

let tiles = [];
let coinsLeft = 0;

function solidAt(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return true;
    return tiles[cy * MAP_W + cx] === T_SOLID;
}

function tileAt(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return T_SOLID;
    return tiles[cy * MAP_W + cx];
}

function setTile(cx, cy, v) {
    if (cx >= 0 && cy >= 0 && cx < MAP_W && cy < MAP_H) {
        tiles[cy * MAP_W + cx] = v;
    }
}

function generateLevel() {
    tiles = [];
    coinsLeft = 0;

    // Start with empty space + floor at bottom
    for (let y = 0; y < MAP_H; ++y) {
        for (let x = 0; x < MAP_W; ++x) {
            if (y === MAP_H - 1 || x === 0 || x === MAP_W - 1) {
                tiles.push(T_SOLID);
            } else {
                tiles.push(T_EMPTY);
            }
        }
    }

    // Generate platforms
    math.seed(7);
    for (let i = 0; i < 20; ++i) {
        const px = 3 + math.rndInt(MAP_W - 8);
        const py = 5 + math.rndInt(MAP_H - 8);
        const pw = 3 + math.rndInt(5);
        for (let x = px; x < px + pw && x < MAP_W - 1; ++x) {
            setTile(x, py, T_SOLID);
        }
    }

    // Place coins on top of platforms
    for (let y = 1; y < MAP_H - 1; ++y) {
        for (let x = 1; x < MAP_W - 1; ++x) {
            if (
                tiles[y * MAP_W + x] === T_EMPTY &&
                tiles[(y + 1) * MAP_W + x] === T_SOLID &&
                math.rnd() < 0.3
            ) {
                tiles[y * MAP_W + x] = T_COIN;
                ++coinsLeft;
            }
        }
    }

    // Place some spikes
    for (let x = 5; x < MAP_W - 5; x += 8 + math.rndInt(6)) {
        const baseY = MAP_H - 2;
        if (tiles[baseY * MAP_W + x] === T_EMPTY) {
            tiles[baseY * MAP_W + x] = T_SPIKE;
        }
    }
}

// --- Player ----------------------------------------------------------

let player = {
    x: 24,
    y: 100,
    vx: 0,
    vy: 0,
    w: 6,
    h: 7, // hitbox
    grounded: false,
    facing: 1, // 1 = right, -1 = left
    alive: true,
    score: 0,
};

let highScore = 0;
let camX = 0;
let camY = 0;

// --- Enemies ---------------------------------------------------------

let enemies = [];

function spawnEnemies() {
    enemies = [];
    for (let i = 0; i < 6; ++i) {
        const ex = 40 + i * 70;
        let ey = 0;
        // Find ground under spawn X
        for (let y = 0; y < MAP_H - 1; ++y) {
            if (solidAt(math.flr(ex / TILE), y + 1)) {
                ey = y * TILE;
                break;
            }
        }
        enemies.push({
            x: ex,
            y: ey,
            vx: 0.5 * (math.rnd() > 0.5 ? 1 : -1),
            w: 6,
            h: 7,
            alive: true,
        });
    }
}

// --- Collision helpers ------------------------------------------------

function collideX(obj) {
    const left = math.flr(obj.x / TILE);
    const right = math.flr((obj.x + obj.w - 1) / TILE);
    const top = math.flr(obj.y / TILE);
    const bot = math.flr((obj.y + obj.h - 1) / TILE);

    for (let ty = top; ty <= bot; ++ty) {
        if (solidAt(left, ty)) {
            obj.x = (left + 1) * TILE;
            obj.vx = 0;
            return;
        }
        if (solidAt(right, ty)) {
            obj.x = right * TILE - obj.w;
            obj.vx = 0;
            return;
        }
    }
}

function collideY(obj) {
    const left = math.flr(obj.x / TILE);
    const right = math.flr((obj.x + obj.w - 1) / TILE);
    const top = math.flr(obj.y / TILE);
    const bot = math.flr((obj.y + obj.h - 1) / TILE);

    obj.grounded = false;
    for (let tx = left; tx <= right; ++tx) {
        if (solidAt(tx, top)) {
            obj.y = (top + 1) * TILE;
            obj.vy = 0;
            return;
        }
        if (solidAt(tx, bot)) {
            obj.y = bot * TILE - obj.h;
            obj.vy = 0;
            obj.grounded = true;
            return;
        }
    }
}

function boxesOverlap(a, b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

// --- Game logic ------------------------------------------------------

function resetPlayer() {
    player.x = 24;
    player.y = 100;
    player.vx = 0;
    player.vy = 0;
    player.alive = true;
}



// --- Callbacks -------------------------------------------------------

function _init() {
    generateLevel();
    spawnEnemies();
    highScore = cart.dget(0) || 0;
}

function _update(dt) {
    if (!player.alive) {
        if (input.btnp('jump')) {
            if (player.score > highScore) {
                highScore = player.score;
                cart.dset(0, highScore);
            }
            player.score = 0;
            resetPlayer();
        }
        return;
    }

    // Horizontal movement
    if (input.btn('left')) {
        player.vx -= MOVE;
        player.facing = -1;
    }
    if (input.btn('right')) {
        player.vx += MOVE;
        player.facing = 1;
    }
    player.vx *= FRICTION;

    // Jump
    if (player.grounded && input.btnp('jump')) {
        player.vy = JUMP;
    }

    // Gravity
    player.vy += GRAVITY;

    // Move + collide X then Y
    player.x += player.vx;
    collideX(player);
    player.y += player.vy;
    collideY(player);

    // Collect coins
    const pcx1 = math.flr(player.x / TILE);
    const pcx2 = math.flr((player.x + player.w - 1) / TILE);
    const pcy1 = math.flr(player.y / TILE);
    const pcy2 = math.flr((player.y + player.h - 1) / TILE);
    for (let ty = pcy1; ty <= pcy2; ++ty) {
        for (let tx = pcx1; tx <= pcx2; ++tx) {
            if (tileAt(tx, ty) === T_COIN) {
                setTile(tx, ty, T_EMPTY);
                player.score += 10;
                --coinsLeft;
                // sfx.play(0); // uncomment when SFX loaded
            }
            if (tileAt(tx, ty) === T_SPIKE) {
                player.alive = false;
                // sfx.play(1); // uncomment when SFX loaded
            }
        }
    }

    // Update enemies
    for (let i = 0; i < enemies.length; ++i) {
        const e = enemies[i];
        if (!e.alive) continue;

        e.x += e.vx;
        // Reverse at walls
        const ecx = math.flr((e.x + (e.vx > 0 ? e.w : 0)) / TILE);
        const ecy = math.flr((e.y + e.h - 1) / TILE);
        if (solidAt(ecx, ecy) || !solidAt(ecx, ecy + 1)) {
            e.vx = -e.vx;
            e.x += e.vx;
        }

        // Player collision
        if (player.alive && boxesOverlap(player, e)) {
            // Stomp if falling onto enemy
            if (player.vy > 0 && player.y + player.h - 4 < e.y + 2) {
                e.alive = false;
                player.vy = JUMP * 0.6;
                player.score += 50;
                // sfx.play(2); // uncomment when SFX loaded
            } else {
                player.alive = false;
            }
        }
    }

    // Camera follow
    const targetX = player.x - 160 + player.w / 2;
    const targetY = player.y - 90;
    camX += (targetX - camX) * 0.08;
    camY += (targetY - camY) * 0.08;

    const maxCx = MAP_W * TILE - 320;
    const maxCy = MAP_H * TILE - 180;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > maxCx) camX = maxCx;
    if (camY > maxCy) camY = maxCy;

    // Fell off bottom
    if (player.y > MAP_H * TILE + 16) {
        player.alive = false;
    }
}

function _draw() {
    gfx.cls(12); // sky colour

    // Camera
    const ox = math.flr(camX);
    const oy = math.flr(camY);
    gfx.camera(ox, oy);

    // --- Draw tiles ---
    const sx = math.flr(camX / TILE);
    const sy = math.flr(camY / TILE);
    let ex = sx + 41;
    let ey = sy + 24;
    if (ex > MAP_W) ex = MAP_W;
    if (ey > MAP_H) ey = MAP_H;

    for (let ty = sy; ty < ey; ++ty) {
        for (let tx = sx; tx < ex; ++tx) {
            const t = tileAt(tx, ty);
            const px = tx * TILE;
            const py = ty * TILE;

            if (t === T_SOLID) {
                // PLACEHOLDER: solid block (replace with gfx.spr())
                gfx.rectfill(px, py, px + 7, py + 7, 4);
                gfx.rect(px, py, px + 7, py + 7, 2);
            } else if (t === T_COIN) {
                // PLACEHOLDER: spinning coin
                const cr = 2 + math.abs(math.sin(sys.time() * 0.3));
                gfx.circfill(px + 4, py + 4, cr, 10);
            } else if (t === T_SPIKE) {
                // PLACEHOLDER: spike triangle
                gfx.trifill(px, py + 7, px + 7, py + 7, px + 3, py, 8);
            }
        }
    }

    // --- Draw enemies ---
    for (let i = 0; i < enemies.length; ++i) {
        const e = enemies[i];
        if (!e.alive) continue;
        // PLACEHOLDER: red box (replace with gfx.spr())
        gfx.rectfill(e.x, e.y, e.x + e.w, e.y + e.h, 8);
        // Eyes
        const ex1 = e.vx > 0 ? e.x + 4 : e.x + 1;
        gfx.pset(ex1, e.y + 2, 7);
        gfx.pset(ex1 + 2, e.y + 2, 7);
    }

    // --- Draw player ---
    if (player.alive) {
        // PLACEHOLDER: coloured box (replace with gfx.spr())
        gfx.rectfill(player.x, player.y, player.x + player.w, player.y + player.h, 11);
        // Eye
        const eyeX = player.facing > 0 ? player.x + 4 : player.x + 1;
        gfx.pset(eyeX, player.y + 2, 0);
    }

    // --- HUD (fixed position) ---
    gfx.camera(0, 0);

    gfx.rectfill(0, 0, 319, 10, 0);
    gfx.print(`score:${player.score}`, 4, 2, 10);
    gfx.print(`high:${highScore}`, 80, 2, 7);
    gfx.print(`coins:${coinsLeft}`, 160, 2, 10);

    if (!player.alive) {
        gfx.rectfill(80, 70, 240, 110, 0);
        gfx.rect(80, 70, 240, 110, 8);
        gfx.print('game over!', 130, 78, 8);
        gfx.print(`score: ${player.score}`, 130, 90, 7);
        gfx.print('press jump to retry', 110, 100, 5);
    }

    // FPS widget
    sys.drawFps();
}

// --- Hot reload state preservation -----------------------------------

function _save() {
    return {
        tiles,
        coinsLeft,
        player,
        highScore,
        camX,
        camY,
        enemies,
    };
}

function _restore(state) {
    tiles = state.tiles;
    coinsLeft = state.coinsLeft;
    player = state.player;
    highScore = state.highScore;
    camX = state.camX;
    camY = state.camY;
    enemies = state.enemies;
}
