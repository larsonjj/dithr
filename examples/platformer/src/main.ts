// Platformer Example
//
// An original side-scrolling platformer demonstrating:
//   - Hand-crafted scrolling level with varied terrain
//   - Breakable brick blocks and item blocks that spawn coins
//   - Pipe decorations and underground sections
//   - Collectible coins with floating score popups
//   - Patrolling enemies (walkers and flyers) with stomp mechanics
//   - Flagpole goal with victory fanfare
//   - Procedural sound effects via the synth engine
//   - Variable-height jumping and coyote time
//   - Camera follow with smooth scrolling
//   - High score saved via cart.dset/dget
//   - Animated tiles (coins, item blocks) and particles
//
// All art is drawn with primitives (rectfill/circfill/trifill).
// Replace with gfx.spr() calls once you add a spritesheet.

// =====================================================================
// Constants
// =====================================================================

const TILE = 8;
const MAP_W = 120; // level width in tiles (960px — wide scrolling level)
const MAP_H = 23; // level height in tiles (184px, fits 180px screen)
const SCREEN_W = 320;
const SCREEN_H = 180;

// Physics — acceleration-based movement
const WALK_ACCEL = 0.046875;
const RUN_ACCEL = 0.046875;
const RELEASE_DECEL = 0.046875;
const SKID_DECEL = 0.15;
const WALK_MAX = 0.75;
const RUN_MAX = 1.3;
const AIR_ACCEL = 0.046875;

// Variable-height jump
const JUMP_VEL_WALK = -2.6;
const JUMP_VEL_RUN = -2.9;
const GRAV_HOLD = 0.11;
const GRAV_RELEASE = 0.375;
const MAX_FALL = 2.25;
const COYOTE_TICKS = 4;
const STOMP_BOUNCE = -2.0;

// Tile types
const T_EMPTY = 0;
const T_GROUND = 1; // solid ground / floor
const T_BRICK = 2; // breakable brick
const T_ITEM = 3; // item block (gives coin when hit from below)
const T_ITEM_USED = 4; // spent item block
const T_COIN = 5; // floating collectible coin
const T_PIPE_TL = 6; // pipe top-left (solid)
const T_PIPE_TR = 7; // pipe top-right (solid)
const T_PIPE_BL = 8; // pipe body-left (solid)
const T_PIPE_BR = 9; // pipe body-right (solid)
const T_FLAG_POLE = 10; // flag pole (non-solid, goal trigger)
const T_FLAG_TOP = 11; // flag pole top ornament
const T_CLOUD = 12; // background cloud (non-solid)
const T_BUSH = 13; // background bush (non-solid)
const T_HILL = 14; // background hill (non-solid)
const T_UNDERGROUND = 15; // underground solid block
const T_SPIKE = 16; // hazard spike

// Colours (palette indices)
const COL_SKY = 12; // light blue sky
const _COL_UNDERGROUND_BG = 0; // black for underground
const COL_GROUND = 4; // brown ground
const COL_GROUND_ACCENT = 15; // tan/beige ground surface
const COL_BRICK = 4; // brown brick
const COL_BRICK_LINE = 2; // dark red brick mortar
const COL_ITEM = 10; // yellow item block
const COL_ITEM_USED = 5; // dark grey spent block
const COL_COIN = 10; // yellow coin
const COL_COIN_SHINE = 7; // white coin shine
const COL_PIPE = 11; // green pipe
const COL_PIPE_DARK = 3; // dark green pipe shadow
const COL_PLAYER = 8; // red player body
const COL_PLAYER_FACE = 15; // peach skin
const COL_ENEMY_WALK = 6; // brown walker enemy
const COL_ENEMY_FLY = 5; // grey flyer enemy
const COL_FLAG = 9; // orange flag
const COL_POLE = 5; // dark grey pole
const COL_HILL_COL = 3; // dark green hill
const COL_BUSH_COL = 11; // green bush
const COL_CLOUD_COL = 7; // white cloud
const COL_SPIKE_COL = 8; // red spike

// SFX slot indices (synth)
const SFX_JUMP = 0;
const SFX_COIN = 1;
const SFX_STOMP = 2;
const SFX_BRICK = 3;
const SFX_ITEM_HIT = 4;
const SFX_DIE = 5;
const SFX_FLAG = 6;

// =====================================================================
// Level data
// =====================================================================

let tiles: number[] = [];
let coinsLeft = 0;
let totalCoins = 0;

function isSolid(t: number): boolean {
    return (
        t === T_GROUND ||
        t === T_BRICK ||
        t === T_ITEM ||
        t === T_ITEM_USED ||
        t === T_PIPE_TL ||
        t === T_PIPE_TR ||
        t === T_PIPE_BL ||
        t === T_PIPE_BR ||
        t === T_UNDERGROUND
    );
}

function solidAt(cx: number, cy: number): boolean {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return true;
    return isSolid(tiles[cy * MAP_W + cx]);
}

function tileAt(cx: number, cy: number): number {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return T_GROUND;
    return tiles[cy * MAP_W + cx];
}

function setTile(cx: number, cy: number, v: number): void {
    if (cx >= 0 && cy >= 0 && cx < MAP_W && cy < MAP_H) {
        tiles[cy * MAP_W + cx] = v;
    }
}

// Place a horizontal run of ground tiles
function groundRun(x1: number, x2: number, surfaceY: number): void {
    for (let x = x1; x <= x2 && x < MAP_W; ++x) {
        for (let y = surfaceY; y < MAP_H; ++y) {
            setTile(x, y, T_GROUND);
        }
    }
}

// Place a pipe at (px, surfaceY-height) .. (px+1, surfaceY-1)
function placePipe(px: number, surfaceY: number, height: number): void {
    const topY = surfaceY - height;
    setTile(px, topY, T_PIPE_TL);
    setTile(px + 1, topY, T_PIPE_TR);
    for (let y = topY + 1; y < surfaceY; ++y) {
        setTile(px, y, T_PIPE_BL);
        setTile(px + 1, y, T_PIPE_BR);
    }
}

// Place a row of bricks with optional item blocks mixed in
function brickRow(x1: number, x2: number, y: number, itemPositions: number[]): void {
    for (let x = x1; x <= x2; ++x) {
        if (itemPositions.indexOf(x) >= 0) {
            setTile(x, y, T_ITEM);
        } else {
            setTile(x, y, T_BRICK);
        }
    }
}

function placeCoin(cx: number, cy: number): void {
    if (tileAt(cx, cy) === T_EMPTY) {
        setTile(cx, cy, T_COIN);
        ++coinsLeft;
        ++totalCoins;
    }
}

function placeFlag(cx: number, surfaceY: number): void {
    setTile(cx, surfaceY - 1, T_FLAG_POLE);
    setTile(cx, surfaceY - 2, T_FLAG_POLE);
    setTile(cx, surfaceY - 3, T_FLAG_POLE);
    setTile(cx, surfaceY - 4, T_FLAG_POLE);
    setTile(cx, surfaceY - 5, T_FLAG_POLE);
    setTile(cx, surfaceY - 6, T_FLAG_POLE);
    setTile(cx, surfaceY - 7, T_FLAG_POLE);
    setTile(cx, surfaceY - 8, T_FLAG_TOP);
}

function buildLevel(): void {
    tiles = [];
    coinsLeft = 0;
    totalCoins = 0;

    // Fill with empty
    for (let i = 0; i < MAP_W * MAP_H; ++i) tiles.push(T_EMPTY);

    const FLOOR = MAP_H - 2; // surface row (row 21 of 0..22)

    // === Section 1: Flat start area (cols 0-14) ===
    groundRun(0, 14, FLOOR);

    // A few coins above start
    placeCoin(5, FLOOR - 3);
    placeCoin(6, FLOOR - 3);
    placeCoin(7, FLOOR - 3);

    // === Section 2: First blocks and pipe (cols 10-25) ===
    brickRow(10, 14, FLOOR - 5, [12]); // brick row with item block at col 12
    groundRun(15, 25, FLOOR);
    placePipe(18, FLOOR, 3);
    placeCoin(16, FLOOR - 2);
    placeCoin(17, FLOOR - 2);

    // === Section 3: Small gap and staircase (cols 26-38) ===
    // Gap at cols 26-27
    groundRun(28, 38, FLOOR);
    // Staircase going up (3 steps)
    for (let s = 0; s < 4; ++s) {
        for (let x = 30 + s; x <= 33; ++x) {
            setTile(x, FLOOR - 1 - s, T_GROUND);
        }
    }
    // Coins arching over the gap
    placeCoin(25, FLOOR - 3);
    placeCoin(26, FLOOR - 5);
    placeCoin(27, FLOOR - 5);
    placeCoin(28, FLOOR - 3);

    // === Section 4: Elevated brick row and underground dip (cols 35-55) ===
    brickRow(36, 42, FLOOR - 5, [38, 40]); // two item blocks
    groundRun(39, 55, FLOOR);
    placePipe(44, FLOOR, 4); // tall pipe

    // Coins on bricks
    placeCoin(37, FLOOR - 6);
    placeCoin(39, FLOOR - 6);
    placeCoin(41, FLOOR - 6);

    // Underground pocket (cols 48-54, below floor)
    for (let x = 48; x <= 54; ++x) {
        setTile(x, FLOOR, T_EMPTY); // remove surface
        setTile(x, FLOOR + 1, T_EMPTY); // remove sub-surface
    }
    setTile(47, FLOOR, T_GROUND); // left wall
    setTile(55, FLOOR, T_GROUND); // right wall
    for (let x = 48; x <= 54; ++x) {
        placeCoin(x, FLOOR);
    }

    // === Section 5: Pit with platforms (cols 56-72) ===
    groundRun(56, 59, FLOOR);
    // Pit at cols 60-64
    groundRun(65, 72, FLOOR);

    // Floating platforms over the pit
    setTile(61, FLOOR - 3, T_BRICK);
    setTile(62, FLOOR - 3, T_ITEM);
    setTile(63, FLOOR - 3, T_BRICK);
    placeCoin(62, FLOOR - 4);

    // Coins leading over the pit
    placeCoin(60, FLOOR - 2);
    placeCoin(61, FLOOR - 5);
    placeCoin(62, FLOOR - 5);
    placeCoin(63, FLOOR - 5);
    placeCoin(64, FLOOR - 2);

    // === Section 6: Enemy gauntlet area (cols 68-85) ===
    groundRun(68, 85, FLOOR);
    placePipe(70, FLOOR, 2);
    placePipe(78, FLOOR, 3);

    // High brick row
    brickRow(73, 77, FLOOR - 6, [75]);
    placeCoin(74, FLOOR - 7);
    placeCoin(75, FLOOR - 7);
    placeCoin(76, FLOOR - 7);

    // Low item row
    brickRow(80, 84, FLOOR - 4, [81, 83]);

    // === Section 7: Staircase to flagpole (cols 86-110) ===
    groundRun(86, 110, FLOOR);

    // Descending block row
    brickRow(88, 92, FLOOR - 5, [90]);

    // More coins
    placeCoin(89, FLOOR - 6);
    placeCoin(91, FLOOR - 6);

    // Large staircase leading to the goal
    for (let s = 0; s < 8; ++s) {
        for (let x = 100 + s; x <= 107; ++x) {
            setTile(x, FLOOR - 1 - s, T_GROUND);
        }
    }

    // Flag at top of the staircase area
    placeFlag(109, FLOOR);

    // Fill remaining ground to the end
    groundRun(108, MAP_W - 1, FLOOR);

    // Spikes in a few tricky spots
    setTile(35, FLOOR - 1, T_SPIKE);
    setTile(67, FLOOR - 1, T_SPIKE);
    setTile(87, FLOOR - 1, T_SPIKE);

    // === Background decoration ===
    // Hills (non-solid background)
    for (let x = 2; x <= 5; ++x) setTile(x, FLOOR - 1, T_HILL);
    for (let x = 40; x <= 43; ++x) setTile(x, FLOOR - 1, T_HILL);
    for (let x = 90; x <= 93; ++x) setTile(x, FLOOR - 1, T_HILL);

    // Bushes
    setTile(8, FLOOR - 1, T_BUSH);
    setTile(9, FLOOR - 1, T_BUSH);
    setTile(22, FLOOR - 1, T_BUSH);
    setTile(68, FLOOR - 1, T_BUSH);
    setTile(69, FLOOR - 1, T_BUSH);

    // Clouds
    setTile(6, 4, T_CLOUD);
    setTile(7, 4, T_CLOUD);
    setTile(20, 3, T_CLOUD);
    setTile(21, 3, T_CLOUD);
    setTile(22, 3, T_CLOUD);
    setTile(45, 5, T_CLOUD);
    setTile(46, 5, T_CLOUD);
    setTile(65, 3, T_CLOUD);
    setTile(66, 3, T_CLOUD);
    setTile(85, 4, T_CLOUD);
    setTile(86, 4, T_CLOUD);
    setTile(105, 3, T_CLOUD);
    setTile(106, 3, T_CLOUD);
}

// =====================================================================
// Particles (floating score text, brick debris)
// =====================================================================

interface Particle {
    x: number;
    y: number;
    vx: number;
    vy: number;
    life: number;
    maxLife: number;
    kind: 'score' | 'debris';
    text: string;
    col: number;
}

let particles: Particle[] = [];

function spawnScorePopup(x: number, y: number, value: number): void {
    particles.push({
        x,
        y,
        vx: 0,
        vy: -0.8,
        life: 40,
        maxLife: 40,
        kind: 'score',
        text: `+${value}`,
        col: COL_COIN,
    });
}

function spawnDebris(x: number, y: number, col: number): void {
    for (let i = 0; i < 4; ++i) {
        const angle = 0.5 + math.rnd() * 2.2;
        const spd = 1.2 + math.rnd() * 0.8;
        particles.push({
            x: x + (i % 2) * 4,
            y: y + math.flr(i / 2) * 4,
            vx: (i % 2 === 0 ? -1 : 1) * spd,
            vy: -angle,
            life: 25,
            maxLife: 25,
            kind: 'debris',
            text: '',
            col,
        });
    }
}

function updateParticles(): void {
    for (let i = particles.length - 1; i >= 0; --i) {
        const p = particles[i];
        p.x += p.vx;
        p.y += p.vy;
        if (p.kind === 'debris') p.vy += 0.12;
        p.life--;
        if (p.life <= 0) {
            particles.splice(i, 1);
        }
    }
}

function drawParticles(): void {
    for (let i = 0; i < particles.length; ++i) {
        const p = particles[i];
        const px = math.flr(p.x);
        const py = math.flr(p.y);
        if (p.kind === 'score') {
            gfx.print(p.text, px, py, p.col);
        } else {
            gfx.rectfill(px, py, px + 2, py + 2, p.col);
        }
    }
}

// =====================================================================
// Block bump animation
// =====================================================================

interface BumpAnim {
    cx: number;
    cy: number;
    offset: number; // current y pixel offset (negative = up)
    returning: boolean;
}

let bumps: BumpAnim[] = [];

function startBump(cx: number, cy: number): void {
    bumps.push({ cx, cy, offset: 0, returning: false });
}

function updateBumps(): void {
    for (let i = bumps.length - 1; i >= 0; --i) {
        const b = bumps[i];
        if (!b.returning) {
            b.offset -= 1;
            if (b.offset <= -4) b.returning = true;
        } else {
            b.offset += 1;
            if (b.offset >= 0) {
                bumps.splice(i, 1);
            }
        }
    }
}

function getBumpOffset(cx: number, cy: number): number {
    for (let i = 0; i < bumps.length; ++i) {
        if (bumps[i].cx === cx && bumps[i].cy === cy) return bumps[i].offset;
    }
    return 0;
}

// =====================================================================
// Player
// =====================================================================

interface Player {
    x: number;
    y: number;
    vx: number;
    vy: number;
    w: number;
    h: number;
    grounded: boolean;
    facing: number;
    alive: boolean;
    score: number;
    coins: number;
    jumping: boolean;
    coyote: number;
    won: boolean;
    winTimer: number;
    animFrame: number;
    animTimer: number;
    invincible: number;
}

let player: Player = {
    x: 24,
    y: (MAP_H - 4) * TILE,
    vx: 0,
    vy: 0,
    w: 6,
    h: 7,
    grounded: false,
    facing: 1,
    alive: true,
    score: 0,
    coins: 0,
    jumping: false,
    coyote: 0,
    won: false,
    winTimer: 0,
    animFrame: 0,
    animTimer: 0,
    invincible: 0,
};

let highScore = 0;
let camX = 0;
let camY = 0;
let lives = 3;
let gameState: 'title' | 'playing' | 'dead' | 'win' = 'title';
let stateTimer = 0;
let levelTime = 300; // countdown timer in seconds
let timeAccum = 0;

// =====================================================================
// Enemies
// =====================================================================

interface Enemy {
    x: number;
    y: number;
    vx: number;
    vy: number;
    w: number;
    h: number;
    alive: boolean;
    kind: 'walker' | 'flyer';
    animTimer: number;
    grounded: boolean;
    squashTimer: number; // brief squash animation on death
}

let enemies: Enemy[] = [];

function spawnEnemies(): void {
    enemies = [];

    // Walkers — patrol on ground
    const walkerPositions = [15, 24, 34, 50, 53, 72, 74, 82, 84, 95, 98];
    for (let i = 0; i < walkerPositions.length; ++i) {
        const ex = walkerPositions[i] * TILE;
        const ey = findGround(walkerPositions[i]);
        enemies.push({
            x: ex,
            y: ey,
            vx: -0.3,
            vy: 0,
            w: 6,
            h: 7,
            alive: true,
            kind: 'walker',
            animTimer: 0,
            grounded: true,
            squashTimer: 0,
        });
    }

    // Flyers — bob up and down in the air
    const flyerPositions = [
        [30, 12],
        [62, 10],
        [76, 8],
        [96, 10],
    ];
    for (let i = 0; i < flyerPositions.length; ++i) {
        const fx = flyerPositions[i][0] * TILE;
        const fy = flyerPositions[i][1] * TILE;
        enemies.push({
            x: fx,
            y: fy,
            vx: -0.4,
            vy: 0,
            w: 7,
            h: 6,
            alive: true,
            kind: 'flyer',
            animTimer: 0,
            grounded: false,
            squashTimer: 0,
        });
    }
}

function findGround(cx: number): number {
    for (let y = 0; y < MAP_H - 1; ++y) {
        if (solidAt(cx, y + 1)) {
            return (y + 1) * TILE - 7;
        }
    }
    return (MAP_H - 3) * TILE;
}

// =====================================================================
// Collision helpers
// =====================================================================

function collideX(obj: { x: number; y: number; w: number; h: number; vx: number }): void {
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

function collideY(obj: {
    x: number;
    y: number;
    w: number;
    h: number;
    vy: number;
    grounded: boolean;
}): void {
    const left = math.flr(obj.x / TILE);
    const right = math.flr((obj.x + obj.w - 1) / TILE);
    const top = math.flr(obj.y / TILE);
    const bot = math.flr((obj.y + obj.h - 1) / TILE);

    obj.grounded = false;

    // Head collision (going up)
    if (obj.vy < 0) {
        for (let tx = left; tx <= right; ++tx) {
            if (solidAt(tx, top)) {
                obj.y = (top + 1) * TILE;
                obj.vy = 0;
                // Check for block interaction (player only)
                if (obj === player) hitBlockFromBelow(tx, top);
                return;
            }
        }
    }

    // Foot collision (going down)
    for (let tx = left; tx <= right; ++tx) {
        if (solidAt(tx, bot)) {
            obj.y = bot * TILE - obj.h;
            obj.vy = 0;
            obj.grounded = true;
            return;
        }
    }
}

function hitBlockFromBelow(cx: number, cy: number): void {
    const t = tileAt(cx, cy);
    if (t === T_BRICK) {
        setTile(cx, cy, T_EMPTY);
        spawnDebris(cx * TILE, cy * TILE, COL_BRICK);
        synth.play(SFX_BRICK, 1);
        player.score += 5;
    } else if (t === T_ITEM) {
        setTile(cx, cy, T_ITEM_USED);
        startBump(cx, cy);
        // Spawn a coin above the block
        spawnScorePopup(cx * TILE + 2, cy * TILE - 8, 50);
        player.score += 50;
        player.coins += 1;
        synth.play(SFX_ITEM_HIT, 1);
    }
}

function boxesOverlap(
    a: { x: number; y: number; w: number; h: number },
    b: { x: number; y: number; w: number; h: number },
): boolean {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

// =====================================================================
// Sound effects (procedural via synth)
// =====================================================================

function initSounds(): void {
    // SFX_JUMP — short rising chirp
    synth.set(
        SFX_JUMP,
        [
            { pitch: 50, waveform: 3, volume: 5, effect: 1 },
            { pitch: 58, waveform: 3, volume: 4, effect: 0 },
            { pitch: 62, waveform: 3, volume: 3, effect: 0 },
            { pitch: 64, waveform: 3, volume: 2, effect: 0 },
        ],
        16,
    );

    // SFX_COIN — bright two-tone ding
    synth.set(
        SFX_COIN,
        [
            { pitch: 72, waveform: 0, volume: 6, effect: 0 },
            { pitch: 76, waveform: 0, volume: 5, effect: 0 },
            { pitch: 79, waveform: 0, volume: 4, effect: 5 },
        ],
        24,
    );

    // SFX_STOMP — thump
    synth.set(
        SFX_STOMP,
        [
            { pitch: 40, waveform: 6, volume: 5, effect: 3 },
            { pitch: 30, waveform: 6, volume: 3, effect: 0 },
        ],
        20,
    );

    // SFX_BRICK — breaking crunch
    synth.set(
        SFX_BRICK,
        [
            { pitch: 20, waveform: 6, volume: 6, effect: 0 },
            { pitch: 15, waveform: 6, volume: 5, effect: 3 },
            { pitch: 10, waveform: 6, volume: 3, effect: 0 },
        ],
        16,
    );

    // SFX_ITEM_HIT — item block pop
    synth.set(
        SFX_ITEM_HIT,
        [
            { pitch: 60, waveform: 0, volume: 6, effect: 1 },
            { pitch: 67, waveform: 0, volume: 5, effect: 0 },
            { pitch: 72, waveform: 0, volume: 4, effect: 5 },
        ],
        20,
    );

    // SFX_DIE — descending wah
    synth.set(
        SFX_DIE,
        [
            { pitch: 55, waveform: 3, volume: 6, effect: 3 },
            { pitch: 48, waveform: 3, volume: 5, effect: 3 },
            { pitch: 40, waveform: 3, volume: 4, effect: 3 },
            { pitch: 30, waveform: 3, volume: 3, effect: 5 },
        ],
        10,
    );

    // SFX_FLAG — victory fanfare arpeggio
    synth.set(
        SFX_FLAG,
        [
            { pitch: 60, waveform: 0, volume: 6, effect: 0 },
            { pitch: 64, waveform: 0, volume: 6, effect: 0 },
            { pitch: 67, waveform: 0, volume: 6, effect: 0 },
            { pitch: 72, waveform: 0, volume: 6, effect: 0 },
            { pitch: 67, waveform: 0, volume: 5, effect: 0 },
            { pitch: 72, waveform: 0, volume: 5, effect: 0 },
            { pitch: 76, waveform: 0, volume: 6, effect: 0 },
            { pitch: 76, waveform: 0, volume: 5, effect: 5 },
        ],
        12,
    );
}

// =====================================================================
// Game logic
// =====================================================================

function resetPlayer(): void {
    player.x = 24;
    player.y = (MAP_H - 4) * TILE;
    player.vx = 0;
    player.vy = 0;
    player.alive = true;
    player.jumping = false;
    player.coyote = 0;
    player.won = false;
    player.winTimer = 0;
    player.animFrame = 0;
    player.animTimer = 0;
    player.invincible = 0;
    camX = 0;
    camY = 0;
}

function startGame(): void {
    buildLevel();
    spawnEnemies();
    player.score = 0;
    player.coins = 0;
    lives = 3;
    levelTime = 300;
    timeAccum = 0;
    particles = [];
    bumps = [];
    resetPlayer();
    gameState = 'playing';
}

// =====================================================================
// Callbacks
// =====================================================================

export function _init(): void {
    initSounds();
    highScore = cart.dget(0) || 0;
    buildLevel();
    gameState = 'title';
    stateTimer = 0;
}

export function _fixedUpdate(_dt: number): void {
    if (gameState !== 'playing') return;
    if (!player.alive || player.won) return;

    const running = input.btn('run');
    const maxSpd = running ? RUN_MAX : WALK_MAX;
    const accel = player.grounded ? (running ? RUN_ACCEL : WALK_ACCEL) : AIR_ACCEL;
    const dirL = input.btn('left');
    const dirR = input.btn('right');

    // --- Horizontal movement ---
    if (dirL && !dirR) {
        player.facing = -1;
        if (player.vx > 0) {
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
        if (player.vx > 0) {
            player.vx -= RELEASE_DECEL;
            if (player.vx < 0) player.vx = 0;
        } else if (player.vx < 0) {
            player.vx += RELEASE_DECEL;
            if (player.vx > 0) player.vx = 0;
        }
    }

    // Clamp to max speed
    if (player.vx > maxSpd) {
        player.vx -= RELEASE_DECEL;
        if (player.vx < maxSpd) player.vx = maxSpd;
    } else if (player.vx < -maxSpd) {
        player.vx += RELEASE_DECEL;
        if (player.vx > -maxSpd) player.vx = -maxSpd;
    }

    // --- Variable-height gravity ---
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

    // Clamp to level bounds (can't go left of start)
    if (player.x < 0) {
        player.x = 0;
        player.vx = 0;
    }

    // --- Coyote time ---
    if (player.grounded) {
        player.coyote = COYOTE_TICKS;
    } else if (player.coyote > 0) {
        player.coyote--;
    }

    // --- Jump ---
    if (player.coyote > 0 && input.btnp('jump')) {
        const spd = math.abs(player.vx);
        const t = math.min(spd / RUN_MAX, 1);
        player.vy = JUMP_VEL_WALK + (JUMP_VEL_RUN - JUMP_VEL_WALK) * t;
        player.jumping = true;
        player.coyote = 0;
        player.grounded = false;
        synth.play(SFX_JUMP, 0);
    }

    // --- Walk animation ---
    if (player.grounded && math.abs(player.vx) > 0.1) {
        player.animTimer++;
        if (player.animTimer > 6) {
            player.animTimer = 0;
            player.animFrame = (player.animFrame + 1) % 3;
        }
    } else if (!player.grounded) {
        player.animFrame = 1; // airborne frame
    } else {
        player.animFrame = 0;
        player.animTimer = 0;
    }

    // Invincibility countdown
    if (player.invincible > 0) player.invincible--;

    // --- Collect coins ---
    const pcx1 = math.flr(player.x / TILE);
    const pcx2 = math.flr((player.x + player.w - 1) / TILE);
    const pcy1 = math.flr(player.y / TILE);
    const pcy2 = math.flr((player.y + player.h - 1) / TILE);
    for (let ty = pcy1; ty <= pcy2; ++ty) {
        for (let tx = pcx1; tx <= pcx2; ++tx) {
            const t = tileAt(tx, ty);
            if (t === T_COIN) {
                setTile(tx, ty, T_EMPTY);
                player.score += 10;
                player.coins += 1;
                --coinsLeft;
                spawnScorePopup(tx * TILE + 2, ty * TILE - 4, 10);
                synth.play(SFX_COIN, 2);
            }
            if (t === T_SPIKE && player.invincible <= 0) {
                killPlayer();
            }
            // Flag pole — victory!
            if (t === T_FLAG_POLE || t === T_FLAG_TOP) {
                player.won = true;
                player.winTimer = 180;
                synth.play(SFX_FLAG, 0);
                // Time bonus
                player.score += levelTime * 5;
            }
        }
    }

    // --- Update enemies ---
    for (let i = 0; i < enemies.length; ++i) {
        const e = enemies[i];
        if (!e.alive) {
            if (e.squashTimer > 0) e.squashTimer--;
            continue;
        }
        e.animTimer++;

        if (e.kind === 'walker') {
            e.x += e.vx;
            // Reverse at walls or ledge edges
            const ecx = math.flr((e.x + (e.vx > 0 ? e.w : 0)) / TILE);
            const ecy = math.flr((e.y + e.h - 1) / TILE);
            if (solidAt(ecx, ecy) || !solidAt(ecx, ecy + 1)) {
                e.vx = -e.vx;
                e.x += e.vx;
            }
        } else {
            // Flyer — horizontal patrol + sine bob
            e.x += e.vx;
            e.y += math.sin(e.animTimer * 0.06) * 0.4;
            // Reverse at walls
            const ecx = math.flr((e.x + (e.vx > 0 ? e.w : 0)) / TILE);
            const ecy = math.flr((e.y + e.h / 2) / TILE);
            if (solidAt(ecx, ecy)) {
                e.vx = -e.vx;
                e.x += e.vx;
            }
        }

        // Player collision
        if (player.alive && !player.won && player.invincible <= 0 && boxesOverlap(player, e)) {
            if (player.vy > 0 && player.y + player.h - 4 < e.y + 2) {
                // Stomp
                e.alive = false;
                e.squashTimer = 15;
                player.vy = STOMP_BOUNCE;
                player.jumping = true;
                player.score += 100;
                spawnScorePopup(e.x, e.y - 8, 100);
                synth.play(SFX_STOMP, 3);
            } else {
                killPlayer();
            }
        }
    }

    // Fell off bottom
    if (player.y > MAP_H * TILE + 16) {
        killPlayer();
    }

    // Update particles and bumps
    updateParticles();
    updateBumps();
}

function killPlayer(): void {
    if (!player.alive) return;
    player.alive = false;
    player.vy = -3;
    synth.play(SFX_DIE, 0);
}

export function _update(dt: number): void {
    stateTimer++;

    if (gameState === 'title') {
        if (input.btnp('jump') || input.btnp('run')) {
            startGame();
        }
        return;
    }

    if (gameState === 'dead') {
        if (stateTimer > 90 && input.btnp('jump')) {
            if (lives > 0) {
                resetPlayer();
                gameState = 'playing';
            } else {
                if (player.score > highScore) {
                    highScore = player.score;
                    cart.dset(0, highScore);
                }
                gameState = 'title';
                stateTimer = 0;
            }
        }
        return;
    }

    if (gameState === 'win') {
        if (stateTimer > 120 && input.btnp('jump')) {
            if (player.score > highScore) {
                highScore = player.score;
                cart.dset(0, highScore);
            }
            gameState = 'title';
            stateTimer = 0;
        }
        return;
    }

    // --- Playing state ---

    if (!player.alive) {
        // Death animation — player floats up then falls
        player.vy += GRAV_RELEASE;
        player.y += player.vy;
        if (player.y > MAP_H * TILE + 32) {
            lives--;
            gameState = 'dead';
            stateTimer = 0;
        }
        return;
    }

    if (player.won) {
        player.winTimer--;
        if (player.winTimer <= 0) {
            gameState = 'win';
            stateTimer = 0;
        }
        return;
    }

    // Level timer countdown
    timeAccum += dt;
    if (timeAccum >= 1) {
        timeAccum -= 1;
        levelTime--;
        if (levelTime <= 0) {
            killPlayer();
        }
    }

    // Camera follow — right-biased (classic side-scroller feel)
    const targetX = player.x - 80;
    const targetY = player.y - SCREEN_H / 2;
    camX += (targetX - camX) * 0.08;
    camY += (targetY - camY) * 0.08;

    // Clamp camera
    const maxCx = MAP_W * TILE - SCREEN_W;
    const maxCy = MAP_H * TILE - SCREEN_H;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > maxCx) camX = maxCx;
    if (camY > maxCy) camY = maxCy;
}

// =====================================================================
// Drawing
// =====================================================================

function drawTile(t: number, px: number, py: number, time: number): void {
    if (t === T_GROUND) {
        gfx.rectfill(px, py, px + 7, py + 7, COL_GROUND);
        // Surface highlight on top row
        gfx.line(px, py, px + 7, py, COL_GROUND_ACCENT);
    } else if (t === T_BRICK) {
        const bo = getBumpOffset(math.flr(px / TILE), math.flr(py / TILE));
        const by = py + bo;
        gfx.rectfill(px, by, px + 7, by + 7, COL_BRICK);
        // Mortar lines
        gfx.line(px, by + 3, px + 7, by + 3, COL_BRICK_LINE);
        gfx.line(px + 3, by, px + 3, by + 3, COL_BRICK_LINE);
        gfx.line(px + 5, by + 3, px + 5, by + 7, COL_BRICK_LINE);
    } else if (t === T_ITEM) {
        const bo = getBumpOffset(math.flr(px / TILE), math.flr(py / TILE));
        const by = py + bo;
        gfx.rectfill(px, by, px + 7, by + 7, COL_ITEM);
        // Animated question mark shimmer
        const blink = math.flr(time * 3) % 2;
        const qCol = blink ? 7 : COL_ITEM;
        gfx.print('?', px + 2, by + 1, qCol);
    } else if (t === T_ITEM_USED) {
        gfx.rectfill(px, py, px + 7, py + 7, COL_ITEM_USED);
    } else if (t === T_COIN) {
        // Animated spinning coin
        const stretch = math.abs(math.sin(time * 4));
        const hw = math.flr(stretch * 3);
        if (hw > 0) {
            gfx.rectfill(px + 4 - hw, py + 1, px + 4 + hw, py + 6, COL_COIN);
            gfx.pset(px + 4 - hw + 1, py + 2, COL_COIN_SHINE);
        }
    } else if (t === T_PIPE_TL) {
        gfx.rectfill(px, py, px + 7, py + 7, COL_PIPE);
        gfx.line(px, py, px, py + 7, COL_PIPE_DARK); // left edge
        gfx.line(px, py, px + 7, py, COL_PIPE_DARK); // top edge
        gfx.line(px + 6, py + 1, px + 6, py + 7, 7); // highlight
    } else if (t === T_PIPE_TR) {
        gfx.rectfill(px, py, px + 7, py + 7, COL_PIPE);
        gfx.line(px + 7, py, px + 7, py + 7, COL_PIPE_DARK);
        gfx.line(px, py, px + 7, py, COL_PIPE_DARK);
    } else if (t === T_PIPE_BL) {
        gfx.rectfill(px + 1, py, px + 7, py + 7, COL_PIPE);
        gfx.line(px + 1, py, px + 1, py + 7, COL_PIPE_DARK);
        gfx.line(px + 6, py, px + 6, py + 7, 7);
    } else if (t === T_PIPE_BR) {
        gfx.rectfill(px, py, px + 6, py + 7, COL_PIPE);
        gfx.line(px + 6, py, px + 6, py + 7, COL_PIPE_DARK);
    } else if (t === T_FLAG_POLE) {
        gfx.line(px + 3, py, px + 3, py + 7, COL_POLE);
        gfx.line(px + 4, py, px + 4, py + 7, COL_POLE);
    } else if (t === T_FLAG_TOP) {
        gfx.line(px + 3, py, px + 3, py + 7, COL_POLE);
        gfx.line(px + 4, py, px + 4, py + 7, COL_POLE);
        // Flag pennant
        gfx.trifill(px + 4, py + 1, px + 7, py + 3, px + 4, py + 5, COL_FLAG);
        // Ball on top
        gfx.circfill(px + 3, py, 1, COL_COIN);
    } else if (t === T_CLOUD) {
        gfx.circfill(px + 2, py + 4, 3, COL_CLOUD_COL);
        gfx.circfill(px + 5, py + 4, 3, COL_CLOUD_COL);
        gfx.circfill(px + 4, py + 2, 3, COL_CLOUD_COL);
    } else if (t === T_BUSH) {
        gfx.circfill(px + 4, py + 5, 3, COL_BUSH_COL);
        gfx.circfill(px + 2, py + 5, 2, COL_BUSH_COL);
        gfx.circfill(px + 6, py + 5, 2, COL_BUSH_COL);
    } else if (t === T_HILL) {
        gfx.trifill(px, py + 7, px + 7, py + 7, px + 4, py + 1, COL_HILL_COL);
    } else if (t === T_UNDERGROUND) {
        gfx.rectfill(px, py, px + 7, py + 7, 1);
        gfx.rect(px, py, px + 7, py + 7, 2);
    } else if (t === T_SPIKE) {
        gfx.trifill(px, py + 7, px + 7, py + 7, px + 3, py + 1, COL_SPIKE_COL);
        gfx.trifill(px + 4, py + 7, px + 7, py + 7, px + 6, py + 3, COL_SPIKE_COL);
    }
}

function drawPlayer(): void {
    if (!player.alive && gameState === 'playing') {
        // Death — spinning in the air
        const blink = math.flr(sys.time() * 10) % 2;
        if (blink) {
            gfx.rectfill(player.x, player.y, player.x + 5, player.y + 6, COL_PLAYER);
        }
        return;
    }
    if (!player.alive) return;

    // Invincibility blink
    if (player.invincible > 0 && math.flr(player.invincible / 2) % 2 === 0) return;

    const px = math.flr(player.x);
    const py = math.flr(player.y);
    const f = player.facing > 0;

    // Body
    gfx.rectfill(px, py + 2, px + 5, py + 6, COL_PLAYER);

    // Head
    gfx.rectfill(px + 1, py, px + 4, py + 2, COL_PLAYER_FACE);

    // Hat brim
    gfx.line(px, py, px + 5, py, COL_PLAYER);

    // Eye
    const eyeX = f ? px + 3 : px + 2;
    gfx.pset(eyeX, py + 1, 0);

    // Legs animation
    if (player.grounded && math.abs(player.vx) > 0.1) {
        const legPhase = player.animFrame;
        if (legPhase === 0) {
            gfx.pset(px + 1, py + 7, COL_PLAYER);
            gfx.pset(px + 4, py + 7, COL_PLAYER);
        } else if (legPhase === 1) {
            gfx.pset(px + 2, py + 7, COL_PLAYER);
            gfx.pset(px + 3, py + 7, COL_PLAYER);
        } else {
            gfx.pset(px + 1, py + 7, COL_PLAYER);
            gfx.pset(px + 4, py + 7, COL_PLAYER);
        }
    } else if (!player.grounded) {
        // Airborne — spread legs
        gfx.pset(px, py + 7, COL_PLAYER);
        gfx.pset(px + 5, py + 7, COL_PLAYER);
    } else {
        gfx.pset(px + 1, py + 7, COL_PLAYER);
        gfx.pset(px + 4, py + 7, COL_PLAYER);
    }
}

function drawEnemy(e: Enemy): void {
    if (!e.alive) {
        if (e.squashTimer > 0) {
            // Squash death animation — flat
            const ex = math.flr(e.x);
            const ey = math.flr(e.y) + 5;
            gfx.rectfill(ex - 1, ey, ex + e.w, ey + 2, e.kind === 'walker' ? COL_ENEMY_WALK : COL_ENEMY_FLY);
        }
        return;
    }

    const ex = math.flr(e.x);
    const ey = math.flr(e.y);

    if (e.kind === 'walker') {
        // Mushroom-shaped walker
        gfx.rectfill(ex, ey + 3, ex + 5, ey + 6, COL_ENEMY_WALK);
        gfx.circfill(ex + 3, ey + 2, 3, COL_ENEMY_WALK);
        // Feet animation
        const step = math.flr(e.animTimer / 8) % 2;
        if (step === 0) {
            gfx.pset(ex + 1, ey + 7, COL_ENEMY_WALK);
            gfx.pset(ex + 4, ey + 7, COL_ENEMY_WALK);
        } else {
            gfx.pset(ex, ey + 7, COL_ENEMY_WALK);
            gfx.pset(ex + 5, ey + 7, COL_ENEMY_WALK);
        }
        // Eyes
        const look = e.vx > 0 ? 1 : -1;
        gfx.pset(ex + 2 + look, ey + 1, 0);
        gfx.pset(ex + 4 + look, ey + 1, 0);
    } else {
        // Flyer — wing-flapping creature
        gfx.rectfill(ex + 1, ey + 1, ex + 5, ey + 4, COL_ENEMY_FLY);
        // Wings
        const wingUp = math.flr(e.animTimer / 5) % 2;
        if (wingUp) {
            gfx.line(ex, ey, ex + 1, ey + 1, COL_ENEMY_FLY);
            gfx.line(ex + 6, ey, ex + 5, ey + 1, COL_ENEMY_FLY);
        } else {
            gfx.line(ex, ey + 3, ex + 1, ey + 2, COL_ENEMY_FLY);
            gfx.line(ex + 6, ey + 3, ex + 5, ey + 2, COL_ENEMY_FLY);
        }
        // Eyes
        gfx.pset(ex + 2, ey + 2, 0);
        gfx.pset(ex + 4, ey + 2, 0);
    }
}

export function _draw(): void {
    const time = sys.time();

    // --- Title screen ---
    if (gameState === 'title') {
        gfx.cls(COL_SKY);
        gfx.rectfill(0, 130, 319, 179, COL_GROUND);
        gfx.line(0, 130, 319, 130, COL_GROUND_ACCENT);

        // Decorative hills
        gfx.trifill(40, 130, 100, 130, 70, 110, COL_HILL_COL);
        gfx.trifill(200, 130, 280, 130, 240, 105, COL_HILL_COL);

        // Clouds
        gfx.circfill(60, 40, 8, COL_CLOUD_COL);
        gfx.circfill(72, 38, 10, COL_CLOUD_COL);
        gfx.circfill(84, 40, 8, COL_CLOUD_COL);
        gfx.circfill(220, 55, 6, COL_CLOUD_COL);
        gfx.circfill(230, 53, 8, COL_CLOUD_COL);

        // Title
        gfx.print('ADVENTURE LAND', 105, 50, COL_PLAYER);
        gfx.print('A platformer demo', 107, 62, 7);

        gfx.print('press jump to start', 103, 100, 7);
        gfx.print(`high score: ${highScore}`, 115, 115, COL_COIN);

        // Controls help
        gfx.print('arrows/wasd: move', 30, 150, 5);
        gfx.print('z/space: jump', 30, 160, 5);
        gfx.print('x/shift: run', 200, 150, 5);
        gfx.print('run + jump = go far!', 200, 160, 5);
        return;
    }

    // --- Dead screen ---
    if (gameState === 'dead') {
        gfx.cls(0);
        if (lives > 0) {
            gfx.print('oops!', 142, 70, COL_PLAYER);
            gfx.print(`lives: ${lives}`, 135, 85, 7);
            if (stateTimer > 90) {
                gfx.print('press jump to continue', 95, 105, 5);
            }
        } else {
            gfx.print('game over', 130, 70, COL_PLAYER);
            gfx.print(`final score: ${player.score}`, 115, 85, 7);
            if (player.score >= highScore && player.score > 0) {
                gfx.print('new high score!', 115, 100, COL_COIN);
            }
            if (stateTimer > 90) {
                gfx.print('press jump for title', 100, 120, 5);
            }
        }
        return;
    }

    // --- Win screen ---
    if (gameState === 'win') {
        gfx.cls(COL_SKY);
        gfx.print('stage clear!', 120, 50, COL_FLAG);
        gfx.print(`score: ${player.score}`, 125, 70, 7);
        gfx.print(`coins: ${player.coins}`, 125, 82, COL_COIN);
        gfx.print(`time bonus: ${levelTime * 5}`, 110, 94, 7);
        if (player.score >= highScore && player.score > 0) {
            gfx.print('new high score!', 115, 112, COL_COIN);
        }
        if (stateTimer > 120) {
            gfx.print('press jump for title', 100, 135, 5);
        }
        return;
    }

    // --- Playing state ---
    gfx.cls(COL_SKY);

    const ox = math.flr(camX);
    const oy = math.flr(camY);
    gfx.camera(ox, oy);

    // Draw visible tiles
    const sx = math.flr(camX / TILE);
    const sy = math.flr(camY / TILE);
    let endX = sx + 42;
    let endY = sy + 24;
    if (endX > MAP_W) endX = MAP_W;
    if (endY > MAP_H) endY = MAP_H;

    // Background tiles first (non-solid decorations)
    for (let ty = sy; ty < endY; ++ty) {
        for (let tx = sx; tx < endX; ++tx) {
            const t = tileAt(tx, ty);
            if (t === T_CLOUD || t === T_BUSH || t === T_HILL) {
                drawTile(t, tx * TILE, ty * TILE, time);
            }
        }
    }

    // Foreground tiles
    for (let ty = sy; ty < endY; ++ty) {
        for (let tx = sx; tx < endX; ++tx) {
            const t = tileAt(tx, ty);
            if (t !== T_EMPTY && t !== T_CLOUD && t !== T_BUSH && t !== T_HILL) {
                drawTile(t, tx * TILE, ty * TILE, time);
            }
        }
    }

    // Draw enemies
    for (let i = 0; i < enemies.length; ++i) {
        drawEnemy(enemies[i]);
    }

    // Draw player
    drawPlayer();

    // Draw particles
    drawParticles();

    // --- HUD (screen-fixed) ---
    gfx.camera(0, 0);

    // HUD background
    gfx.rectfill(0, 0, SCREEN_W - 1, 11, 0);

    // Score
    gfx.print(`score ${player.score}`, 4, 3, 7);

    // Coins
    gfx.rectfill(108, 3, 110, 7, COL_COIN); // tiny coin icon
    gfx.print(`x${player.coins}`, 112, 3, 7);

    // Level name
    gfx.print('1-1', 155, 3, 7);

    // Time
    gfx.print(`time ${levelTime}`, 190, 3, levelTime <= 30 ? COL_PLAYER : 7);

    // Lives
    gfx.print(`lives ${lives}`, 265, 3, 7);
}

// =====================================================================
// Hot reload state preservation
// =====================================================================

export function _save() {
    return {
        tiles,
        coinsLeft,
        totalCoins,
        player,
        highScore,
        camX,
        camY,
        enemies,
        lives,
        gameState,
        stateTimer,
        levelTime,
        timeAccum,
        particles,
        bumps,
    };
}

export function _restore(state: any): void {
    tiles = state.tiles;
    coinsLeft = state.coinsLeft;
    totalCoins = state.totalCoins;
    player = state.player;
    highScore = state.highScore;
    camX = state.camX;
    camY = state.camY;
    enemies = state.enemies;
    lives = state.lives;
    gameState = state.gameState;
    stateTimer = state.stateTimer;
    levelTime = state.levelTime;
    timeAccum = state.timeAccum;
    particles = state.particles || [];
    bumps = state.bumps || [];
}
