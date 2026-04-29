// Sprite & Animation demo
//
// Shows how to:
//   - Draw sprites with gfx.spr()
//   - Animate by cycling sprite indices
//   - Flip sprites horizontally/vertically
//   - Rotate sprites with gfx.sprRot()
//
// Spritesheet: 320x320, 16x16 tiles (20 cols x 20 rows = 400 tiles)

// --- Tile indices ----------------------------------------------------

const SPR_IDLE = 280;
const SPR_WALK = [281, 282, 283];
const SPR_JUMP = 284;

const SPR_ENEMY_IDLE = 320;
const SPR_ENEMY_WALK = [321, 322, 323];

const SPR_GROUND_L = 270; // ground left wall
const SPR_GROUND_M = [271, 271, 271]; // ground top variants
const SPR_GROUND_R = 272; // ground right wall

const SPR_COIN = 1;
const SPR_COIN_BIG = 2;
const SPR_HEART_SM = 40;
const SPR_HEART = 41;
const SPR_HEART_BIG = 42;

// --- State -----------------------------------------------------------

let animT = 0;
const animSpeed = 0.15; // seconds per frame
let walkFrame = 0;
let enemyFrame = 0;

let facingRight = true;
let dirTimer = 0;
let rotation = 0;

// Player X for the walk demo
let playerX = 0;
const playerVx = 30; // pixels per second


// --- Callbacks -------------------------------------------------------

export async function _init(): Promise<void> {
    await res.loadSprite('sheet', 'assets/sprites/spritesheet.png');
    res.setActiveSheet('sheet', { tileW: 8, tileH: 8 });
    gfx.cls(0);
}

export function _update(dt: number): void {

    // Advance animation timer
    animT += dt;
    if (animT >= animSpeed) {
        animT -= animSpeed;
        walkFrame = (walkFrame + 1) % SPR_WALK.length;
        enemyFrame = (enemyFrame + 1) % SPR_ENEMY_WALK.length;
    }

    // Move player back and forth
    dirTimer += dt;
    if (dirTimer >= 1) {
        dirTimer -= 1;
        facingRight = !facingRight;
    }
    playerX += (facingRight ? playerVx : -playerVx) * dt;

    // Spin the rotation demo
    rotation += dt * 1.5;
}

export function _draw(): void {
    gfx.cls(1);

    const T = 16; // tile size shorthand
    const col2 = 170; // x offset for right column

    // =====================================================
    // LEFT COLUMN
    // =====================================================

    // --- Section 1: Idle sprite ---
    gfx.print('idle', 4, 2, 7);
    gfx.spr(SPR_IDLE, 8, 12);

    // --- Section 2: Animated walk cycle ---
    gfx.print('walk cycle', 40, 2, 7);
    // Show all 3 frames side by side, highlight current
    for (let i = 0; i < SPR_WALK.length; ++i) {
        const fx = 44 + i * (T + 4);
        gfx.spr(SPR_WALK[i], fx, 12);
        if (i === walkFrame) {
            gfx.rect(fx - 1, 11, fx + T, 12 + T, 10);
        }
    }

    // --- Section 3: Walking character (moving + flipping) ---
    gfx.print('flip_x walk', 4, 34, 7);
    const sprIdx = SPR_WALK[walkFrame];
    gfx.spr(sprIdx, playerX, 44, 1, 1, !facingRight, false);

    // --- Section 4: Jump sprite ---
    gfx.print('jump', 120, 34, 7);
    gfx.spr(SPR_JUMP, 124, 44);

    // --- Section 5: Flip showcase ---
    gfx.print('flip combos', 4, 66, 7);
    gfx.spr(SPR_IDLE, 8, 78, 1, 1, false, false); // normal
    gfx.spr(SPR_IDLE, 28, 78, 1, 1, true, false); // flip X
    gfx.spr(SPR_IDLE, 48, 78, 1, 1, false, true); // flip Y
    gfx.spr(SPR_IDLE, 68, 78, 1, 1, true, true); // both
    gfx.print('N', 12, 96, 6);
    gfx.print('X', 32, 96, 6);
    gfx.print('Y', 52, 96, 6);
    gfx.print('XY', 70, 96, 6);

    // --- Section 6: Rotation ---
    gfx.print('sprRot()', 4, 108, 7);
    gfx.sprRot(SPR_IDLE, 20, 126, rotation, -1, -1);
    gfx.print(math.flr(rotation * 100) / 100, 44, 130, 6);

    // --- Section 7: Ground tiles ---
    gfx.print('ground tiles', 4, 150, 7);
    gfx.spr(SPR_GROUND_L, 8, 162);
    for (let g = 0; g < 3; ++g) {
        gfx.spr(SPR_GROUND_M[g], 24 + g * T, 162);
    }
    gfx.spr(SPR_GROUND_R, 24 + 3 * T, 162);

    // =====================================================
    // RIGHT COLUMN
    // =====================================================

    // --- Enemy ---
    gfx.print('enemy idle', col2, 2, 7);
    gfx.spr(SPR_ENEMY_IDLE, col2 + 4, 12);

    gfx.print('enemy walk', col2 + 60, 2, 7);
    for (let e = 0; e < SPR_ENEMY_WALK.length; ++e) {
        const ex = col2 + 64 + e * (T + 4);
        gfx.spr(SPR_ENEMY_WALK[e], ex, 12);
        if (e === enemyFrame) {
            gfx.rect(ex - 1, 11, ex + T, 12 + T, 8);
        }
    }

    // --- Collectibles ---
    gfx.print('collectibles', col2, 34, 7);
    gfx.spr(SPR_COIN, col2 + 2, 46);
    gfx.spr(SPR_COIN_BIG, col2 + 24, 46);
    gfx.print('coin', col2, 64, 10);
    gfx.print('big', col2 + 26, 64, 10);

    // --- Hearts ---
    gfx.print('hearts', col2, 78, 7);
    gfx.spr(SPR_HEART_SM, col2 + 4, 90);
    gfx.spr(SPR_HEART, col2 + 24, 90);
    gfx.spr(SPR_HEART_BIG, col2 + 44, 90);
    gfx.print('sm', col2 + 6, 108, 8);
    gfx.print('med', col2 + 24, 108, 8);
    gfx.print('lg', col2 + 48, 108, 8);

    // --- Stretch blit ---
    gfx.print('sspr() stretch', col2, 122, 7);
    // Stretch the idle sprite (16x16 region) to 48x48
    const sx = (SPR_IDLE % 20) * T;
    const sy = math.flr(SPR_IDLE / 20) * T;
    gfx.sspr(sx, sy, T, T, col2 + 4, 120, 48, 48);

    // FPS widget
    sys.drawFps();
}
