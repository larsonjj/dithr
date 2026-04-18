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

// --- Constants (NES Mario-style, scaled to 8px tiles) ---------------

const TILE = 8;
const MAP_W = 60; // level width in tiles
const MAP_H = 23; // level height in tiles (fits 180px)

// Horizontal movement — acceleration based, not friction
const WALK_ACCEL = 0.046875; // ground acceleration
const RUN_ACCEL = 0.046875;
const RELEASE_DECEL = 0.046875; // deceleration when no input
const SKID_DECEL = 0.15; // deceleration when reversing direction
const WALK_MAX = 0.75; // walk top speed
const RUN_MAX = 1.3; // run top speed (holding run button)
const AIR_ACCEL = 0.046875; // air control (same as ground in NES)

// Vertical movement — variable jump via two gravity values
const JUMP_VEL_WALK = -2.6; // jump impulse at walk speed
const JUMP_VEL_RUN = -2.9; // jump impulse at run speed
const GRAV_HOLD = 0.11; // gravity while holding jump (floaty rise)
const GRAV_RELEASE = 0.375; // gravity when jump released (fast fall)
const MAX_FALL = 2.25; // terminal fall speed
const COYOTE_TICKS = 4; // frames of grace after leaving ground
const STOMP_BOUNCE = -2.0; // bounce velocity on enemy stomp

// Tile types
const T_EMPTY = 0;
const T_SOLID = 1;
const T_COIN = 2;
const T_SPIKE = 3;

// --- Level data (procedurally generated) -----------------------------

let tiles: any[] = [];
let coinsLeft = 0;

function solidAt(cx: number, cy: number) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return true;
    return tiles[cy * MAP_W + cx] === T_SOLID;
}

function tileAt(cx: number, cy: number) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return T_SOLID;
    return tiles[cy * MAP_W + cx];
}

function setTile(cx: number, cy: number, v: number) {
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
    jumping: false, // true while jump button is held after launch
    coyote: 0, // ticks since last grounded
};

let highScore = 0;
let camX = 0;
let camY = 0;

// --- Enemies ---------------------------------------------------------

let enemies: any[] = [];

function spawnEnemies() {
    enemies = [];
    for (let i = 0; i < 6; ++i) {
        const ex = 40 + i * 70;
        let ey = 0;
        // Find ground under spawn X
        for (let y = 0; y < MAP_H - 1; ++y) {
            if (solidAt(math.flr(ex / TILE), y + 1)) {
                ey = (y + 1) * TILE - 7; // align bottom of hitbox with top of ground
                break;
            }
        }
        enemies.push({
            x: ex,
            y: ey,
            vx: 0.3 * (math.rnd() > 0.5 ? 1 : -1),
            w: 6,
            h: 7,
            alive: true,
        });
    }
}

// --- Collision helpers ------------------------------------------------

function collideX(obj: { x: number; y: number; w: number; h: number; vx: number }) {
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

function collideY(obj: { x: number; y: number; w: number; h: number; vy: number; grounded: boolean }) {
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

function boxesOverlap(a: { x: number; y: number; w: number; h: number }, b: { x: number; y: number; w: number; h: number }) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

// --- Game logic ------------------------------------------------------

function resetPlayer() {
    player.x = 24;
    player.y = 100;
    player.vx = 0;
    player.vy = 0;
    player.alive = true;
    player.jumping = false;
    player.coyote = 0;
}

// --- Callbacks -------------------------------------------------------

export function _init(): void {
    generateLevel();
    spawnEnemies();
    highScore = cart.dget(0) || 0;
}

export function _fixedUpdate(_dt: number): void {
    if (!player.alive) return;

    const running = input.btn('run');
    const maxSpd = running ? RUN_MAX : WALK_MAX;
    const accel = player.grounded ? (running ? RUN_ACCEL : WALK_ACCEL) : AIR_ACCEL;
    const dirL = input.btn('left');
    const dirR = input.btn('right');

    // --- Horizontal movement (acceleration / deceleration) ---
    if (dirL && !dirR) {
        player.facing = -1;
        if (player.vx > 0) {
            // Skidding — reversing direction
            player.vx -= SKID_DECEL;
            if (player.vx < 0) player.vx = 0;
        } else {
            player.vx -= accel;
            if (player.vx < -maxSpd) player.vx = -maxSpd;
        }
    } else if (dirR && !dirL) {
        player.facing = 1;
        if (player.vx < 0) {
            player.vx += SKID_DECEL;
            if (player.vx > 0) player.vx = 0;
        } else {
            player.vx += accel;
            if (player.vx > maxSpd) player.vx = maxSpd;
        }
    } else {
        // No input — decelerate toward zero
        if (player.vx > 0) {
            player.vx -= RELEASE_DECEL;
            if (player.vx < 0) player.vx = 0;
        } else if (player.vx < 0) {
            player.vx += RELEASE_DECEL;
            if (player.vx > 0) player.vx = 0;
        }
    }

    // Clamp to current max (lets go of run button mid-air → slow down gently)
    if (player.vx > maxSpd) {
        player.vx -= RELEASE_DECEL;
        if (player.vx < maxSpd) player.vx = maxSpd;
    } else if (player.vx < -maxSpd) {
        player.vx += RELEASE_DECEL;
        if (player.vx > -maxSpd) player.vx = -maxSpd;
    }

    // --- Variable-height gravity ---
    // Holding jump while rising → floaty; released or falling → fast
    if (player.jumping && input.btn('jump') && player.vy < 0) {
        player.vy += GRAV_HOLD;
    } else {
        player.vy += GRAV_RELEASE;
        player.jumping = false;
    }
    if (player.vy > MAX_FALL) player.vy = MAX_FALL;

    // --- Move + collide ---
    player.x += player.vx;
    collideX(player);
    player.y += player.vy;
    collideY(player);

    // --- Coyote time ---
    if (player.grounded) {
        player.coyote = COYOTE_TICKS;
    } else if (player.coyote > 0) {
        player.coyote--;
    }

    // --- Jump ---
    if (player.coyote > 0 && input.btnp('jump')) {
        // Stronger impulse when moving fast (like NES Mario)
        const spd = math.abs(player.vx);
        const t = math.min(spd / RUN_MAX, 1);
        player.vy = JUMP_VEL_WALK + (JUMP_VEL_RUN - JUMP_VEL_WALK) * t;
        player.jumping = true;
        player.coyote = 0;
        player.grounded = false;
    }

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
                player.vy = STOMP_BOUNCE;
                player.jumping = true;
                player.score += 50;
                // sfx.play(2); // uncomment when SFX loaded
            } else {
                player.alive = false;
            }
        }
    }

    // Fell off bottom
    if (player.y > MAP_H * TILE + 16) {
        player.alive = false;
    }
}

export function _update(_dt: number): void {
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
}

export function _draw(): void {
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
        gfx.rectfill(e.x, e.y, e.x + e.w - 1, e.y + e.h - 1, 8);
        // Eyes
        const ex1 = e.vx > 0 ? e.x + 4 : e.x + 1;
        gfx.pset(ex1, e.y + 2, 7);
        gfx.pset(ex1 + 2, e.y + 2, 7);
    }

    // --- Draw player ---
    if (player.alive) {
        // PLACEHOLDER: coloured box (replace with gfx.spr())
        gfx.rectfill(player.x, player.y, player.x + player.w - 1, player.y + player.h - 1, 11);
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

export function _save() {
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

export function _restore(state: any): void {
    tiles = state.tiles;
    coinsLeft = state.coinsLeft;
    player = state.player;
    highScore = state.highScore;
    camX = state.camX;
    camY = state.camY;
    enemies = state.enemies;
}
