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

var SPR_IDLE = 280;
var SPR_WALK = [281, 282, 283];
var SPR_JUMP = 284;

var SPR_ENEMY_IDLE = 320;
var SPR_ENEMY_WALK = [321, 322, 323];

var SPR_GROUND_L = 270; // ground left wall
var SPR_GROUND_M = [271, 271, 271]; // ground top variants
var SPR_GROUND_R = 272; // ground right wall

var SPR_COIN = 1;
var SPR_COIN_BIG = 2;
var SPR_HEART_SM = 40;
var SPR_HEART = 41;
var SPR_HEART_BIG = 42;

// --- State -----------------------------------------------------------

var anim_t = 0;
var anim_speed = 0.15; // seconds per frame
var walk_frame = 0;
var enemy_frame = 0;

var facing_right = true;
var dir_timer = 0;
var rotation = 0;

// Player X for the walk demo
var player_x = 0;
var player_vx = 30; // pixels per second
var smooth_fps = 60;
var fps_history = [];
var fps_hist_idx = 0;
var FPS_HIST_LEN = 50;
for (var _i = 0; _i < FPS_HIST_LEN; ++_i) fps_history.push(60);

function draw_fps_widget() {
    var wx = 320 - FPS_HIST_LEN - 4;
    var wy = 0;
    var ww = FPS_HIST_LEN + 4;
    var gh = 16;
    var target = sys.targetFps();
    gfx.rectfill(wx, wy, wx + ww - 1, wy + 8 + gh + 1, 0);
    gfx.print(math.flr(smooth_fps) + " FPS", wx + 2, wy + 1, 7);
    gfx.rect(wx + 1, wy + 8, wx + ww - 2, wy + 8 + gh, 5);
    for (var idx = 1; idx < FPS_HIST_LEN; ++idx) {
        var i0 = (fps_hist_idx + idx - 1) % FPS_HIST_LEN;
        var i1 = (fps_hist_idx + idx) % FPS_HIST_LEN;
        var v0 = math.clamp(fps_history[i0] / target, 0, 1);
        var v1 = math.clamp(fps_history[i1] / target, 0, 1);
        var y0 = wy + 8 + gh - 1 - math.flr(v0 * (gh - 2));
        var y1 = wy + 8 + gh - 1 - math.flr(v1 * (gh - 2));
        var clr = v1 > 0.9 ? 11 : v1 > 0.5 ? 9 : 8;
        gfx.line(wx + 2 + idx - 1, y0, wx + 2 + idx, y1, clr);
    }
}

// --- Callbacks -------------------------------------------------------

function _init() {
    gfx.cls(0);
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    // Advance animation timer
    anim_t += dt;
    if (anim_t >= anim_speed) {
        anim_t -= anim_speed;
        walk_frame = (walk_frame + 1) % SPR_WALK.length;
        enemy_frame = (enemy_frame + 1) % SPR_ENEMY_WALK.length;
    }

    // Move player back and forth
    dir_timer += dt;
    if (dir_timer >= 1) {
        dir_timer -= 1;
        facing_right = !facing_right;
    }
    player_x += (facing_right ? player_vx : -player_vx) * dt;

    // Spin the rotation demo
    rotation += dt * 1.5;
}

function _draw() {
    gfx.cls(1);

    var T = 16; // tile size shorthand
    var col2 = 170; // x offset for right column

    // =====================================================
    // LEFT COLUMN
    // =====================================================

    // --- Section 1: Idle sprite ---
    gfx.print("idle", 4, 2, 7);
    gfx.spr(SPR_IDLE, 8, 12);

    // --- Section 2: Animated walk cycle ---
    gfx.print("walk cycle", 40, 2, 7);
    // Show all 3 frames side by side, highlight current
    for (var i = 0; i < SPR_WALK.length; ++i) {
        var fx = 44 + i * (T + 4);
        gfx.spr(SPR_WALK[i], fx, 12);
        if (i === walk_frame) {
            gfx.rect(fx - 1, 11, fx + T, 12 + T, 10);
        }
    }

    // --- Section 3: Walking character (moving + flipping) ---
    gfx.print("flip_x walk", 4, 34, 7);
    var spr_idx = SPR_WALK[walk_frame];
    gfx.spr(spr_idx, player_x, 44, 1, 1, !facing_right, false);

    // --- Section 4: Jump sprite ---
    gfx.print("jump", 120, 34, 7);
    gfx.spr(SPR_JUMP, 124, 44);

    // --- Section 5: Flip showcase ---
    gfx.print("flip combos", 4, 66, 7);
    gfx.spr(SPR_IDLE, 8, 78, 1, 1, false, false); // normal
    gfx.spr(SPR_IDLE, 28, 78, 1, 1, true, false); // flip X
    gfx.spr(SPR_IDLE, 48, 78, 1, 1, false, true); // flip Y
    gfx.spr(SPR_IDLE, 68, 78, 1, 1, true, true); // both
    gfx.print("N", 12, 96, 6);
    gfx.print("X", 32, 96, 6);
    gfx.print("Y", 52, 96, 6);
    gfx.print("XY", 70, 96, 6);

    // --- Section 6: Rotation ---
    gfx.print("sprRot()", 4, 108, 7);
    gfx.sprRot(SPR_IDLE, 20, 126, rotation, -1, -1);
    gfx.print(math.flr(rotation * 100) / 100, 44, 130, 6);

    // --- Section 7: Ground tiles ---
    gfx.print("ground tiles", 4, 150, 7);
    gfx.spr(SPR_GROUND_L, 8, 162);
    for (var g = 0; g < 3; ++g) {
        gfx.spr(SPR_GROUND_M[g], 24 + g * T, 162);
    }
    gfx.spr(SPR_GROUND_R, 24 + 3 * T, 162);

    // =====================================================
    // RIGHT COLUMN
    // =====================================================

    // --- Enemy ---
    gfx.print("enemy idle", col2, 2, 7);
    gfx.spr(SPR_ENEMY_IDLE, col2 + 4, 12);

    gfx.print("enemy walk", col2 + 60, 2, 7);
    for (var e = 0; e < SPR_ENEMY_WALK.length; ++e) {
        var ex = col2 + 64 + e * (T + 4);
        gfx.spr(SPR_ENEMY_WALK[e], ex, 12);
        if (e === enemy_frame) {
            gfx.rect(ex - 1, 11, ex + T, 12 + T, 8);
        }
    }

    // --- Collectibles ---
    gfx.print("collectibles", col2, 34, 7);
    gfx.spr(SPR_COIN, col2 + 2, 46);
    gfx.spr(SPR_COIN_BIG, col2 + 24, 46);
    gfx.print("coin", col2, 64, 10);
    gfx.print("big", col2 + 26, 64, 10);

    // --- Hearts ---
    gfx.print("hearts", col2, 78, 7);
    gfx.spr(SPR_HEART_SM, col2 + 4, 90);
    gfx.spr(SPR_HEART, col2 + 24, 90);
    gfx.spr(SPR_HEART_BIG, col2 + 44, 90);
    gfx.print("sm", col2 + 6, 108, 8);
    gfx.print("med", col2 + 24, 108, 8);
    gfx.print("lg", col2 + 48, 108, 8);

    // --- Stretch blit ---
    gfx.print("sspr() stretch", col2, 122, 7);
    // Stretch the idle sprite (16x16 region) to 48x48
    var sx = (SPR_IDLE % 20) * T;
    var sy = math.flr(SPR_IDLE / 20) * T;
    gfx.sspr(sx, sy, T, T, col2 + 4, 120, 48, 48);

    // FPS widget
    draw_fps_widget();
}
