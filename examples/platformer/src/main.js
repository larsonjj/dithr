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

var TILE = 8;
var MAP_W = 60; // level width in tiles
var MAP_H = 23; // level height in tiles (fits 180px)
var GRAVITY = 0.35;
var JUMP = -5.5;
var MOVE = 1.8;
var FRICTION = 0.85;

// Tile types
var T_EMPTY = 0;
var T_SOLID = 1;
var T_COIN = 2;
var T_SPIKE = 3;

// --- Level data (procedurally generated) -----------------------------

var tiles = [];
var coins_left = 0;

function solid_at(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return true;
    return tiles[cy * MAP_W + cx] === T_SOLID;
}

function tile_at(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return T_SOLID;
    return tiles[cy * MAP_W + cx];
}

function set_tile(cx, cy, v) {
    if (cx >= 0 && cy >= 0 && cx < MAP_W && cy < MAP_H) {
        tiles[cy * MAP_W + cx] = v;
    }
}

function generate_level() {
    tiles = [];
    coins_left = 0;

    // Start with empty space + floor at bottom
    for (var y = 0; y < MAP_H; ++y) {
        for (var x = 0; x < MAP_W; ++x) {
            if (y === MAP_H - 1 || x === 0 || x === MAP_W - 1) {
                tiles.push(T_SOLID);
            } else {
                tiles.push(T_EMPTY);
            }
        }
    }

    // Generate platforms
    math.seed(7);
    for (var i = 0; i < 20; ++i) {
        var px = 3 + math.rnd_int(MAP_W - 8);
        var py = 5 + math.rnd_int(MAP_H - 8);
        var pw = 3 + math.rnd_int(5);
        for (var x = px; x < px + pw && x < MAP_W - 1; ++x) {
            set_tile(x, py, T_SOLID);
        }
    }

    // Place coins on top of platforms
    for (var y = 1; y < MAP_H - 1; ++y) {
        for (var x = 1; x < MAP_W - 1; ++x) {
            if (
                tiles[y * MAP_W + x] === T_EMPTY &&
                tiles[(y + 1) * MAP_W + x] === T_SOLID &&
                math.rnd() < 0.3
            ) {
                tiles[y * MAP_W + x] = T_COIN;
                ++coins_left;
            }
        }
    }

    // Place some spikes
    for (var x = 5; x < MAP_W - 5; x += 8 + math.rnd_int(6)) {
        var base_y = MAP_H - 2;
        if (tiles[base_y * MAP_W + x] === T_EMPTY) {
            tiles[base_y * MAP_W + x] = T_SPIKE;
        }
    }
}

// --- Player ----------------------------------------------------------

var player = {
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

var high_score = 0;
var cam_x = 0;
var cam_y = 0;

// --- Enemies ---------------------------------------------------------

var enemies = [];

function spawn_enemies() {
    enemies = [];
    for (var i = 0; i < 6; ++i) {
        var ex = 40 + i * 70;
        var ey = 0;
        // Find ground under spawn X
        for (var y = 0; y < MAP_H - 1; ++y) {
            if (solid_at(math.flr(ex / TILE), y + 1)) {
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

function collide_x(obj) {
    var left = math.flr(obj.x / TILE);
    var right = math.flr((obj.x + obj.w - 1) / TILE);
    var top = math.flr(obj.y / TILE);
    var bot = math.flr((obj.y + obj.h - 1) / TILE);

    for (var ty = top; ty <= bot; ++ty) {
        if (solid_at(left, ty)) {
            obj.x = (left + 1) * TILE;
            obj.vx = 0;
            return;
        }
        if (solid_at(right, ty)) {
            obj.x = right * TILE - obj.w;
            obj.vx = 0;
            return;
        }
    }
}

function collide_y(obj) {
    var left = math.flr(obj.x / TILE);
    var right = math.flr((obj.x + obj.w - 1) / TILE);
    var top = math.flr(obj.y / TILE);
    var bot = math.flr((obj.y + obj.h - 1) / TILE);

    obj.grounded = false;
    for (var tx = left; tx <= right; ++tx) {
        if (solid_at(tx, top)) {
            obj.y = (top + 1) * TILE;
            obj.vy = 0;
            return;
        }
        if (solid_at(tx, bot)) {
            obj.y = bot * TILE - obj.h;
            obj.vy = 0;
            obj.grounded = true;
            return;
        }
    }
}

function boxes_overlap(a, b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

// --- Game logic ------------------------------------------------------

function reset_player() {
    player.x = 24;
    player.y = 100;
    player.vx = 0;
    player.vy = 0;
    player.alive = true;
}

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
    var target = sys.target_fps();
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
    generate_level();
    spawn_enemies();
    high_score = cart.dget(0) || 0;
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;
    if (!player.alive) {
        if (input.btnp("jump")) {
            if (player.score > high_score) {
                high_score = player.score;
                cart.dset(0, high_score);
            }
            player.score = 0;
            reset_player();
        }
        return;
    }

    // Horizontal movement
    if (input.btn("left")) {
        player.vx -= MOVE;
        player.facing = -1;
    }
    if (input.btn("right")) {
        player.vx += MOVE;
        player.facing = 1;
    }
    player.vx *= FRICTION;

    // Jump
    if (player.grounded && input.btnp("jump")) {
        player.vy = JUMP;
    }

    // Gravity
    player.vy += GRAVITY;

    // Move + collide X then Y
    player.x += player.vx;
    collide_x(player);
    player.y += player.vy;
    collide_y(player);

    // Collect coins
    var pcx1 = math.flr(player.x / TILE);
    var pcx2 = math.flr((player.x + player.w - 1) / TILE);
    var pcy1 = math.flr(player.y / TILE);
    var pcy2 = math.flr((player.y + player.h - 1) / TILE);
    for (var ty = pcy1; ty <= pcy2; ++ty) {
        for (var tx = pcx1; tx <= pcx2; ++tx) {
            if (tile_at(tx, ty) === T_COIN) {
                set_tile(tx, ty, T_EMPTY);
                player.score += 10;
                --coins_left;
                // sfx.play(0); // uncomment when SFX loaded
            }
            if (tile_at(tx, ty) === T_SPIKE) {
                player.alive = false;
                // sfx.play(1); // uncomment when SFX loaded
            }
        }
    }

    // Update enemies
    for (var i = 0; i < enemies.length; ++i) {
        var e = enemies[i];
        if (!e.alive) continue;

        e.x += e.vx;
        // Reverse at walls
        var ecx = math.flr((e.x + (e.vx > 0 ? e.w : 0)) / TILE);
        var ecy = math.flr((e.y + e.h - 1) / TILE);
        if (solid_at(ecx, ecy) || !solid_at(ecx, ecy + 1)) {
            e.vx = -e.vx;
            e.x += e.vx;
        }

        // Player collision
        if (player.alive && boxes_overlap(player, e)) {
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
    var target_x = player.x - 160 + player.w / 2;
    var target_y = player.y - 90;
    cam_x += (target_x - cam_x) * 0.08;
    cam_y += (target_y - cam_y) * 0.08;

    var max_cx = MAP_W * TILE - 320;
    var max_cy = MAP_H * TILE - 180;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > max_cx) cam_x = max_cx;
    if (cam_y > max_cy) cam_y = max_cy;

    // Fell off bottom
    if (player.y > MAP_H * TILE + 16) {
        player.alive = false;
    }
}

function _draw() {
    gfx.cls(12); // sky colour

    // Camera
    var ox = math.flr(cam_x);
    var oy = math.flr(cam_y);
    gfx.camera(ox, oy);

    // --- Draw tiles ---
    var sx = math.flr(cam_x / TILE);
    var sy = math.flr(cam_y / TILE);
    var ex = sx + 41;
    var ey = sy + 24;
    if (ex > MAP_W) ex = MAP_W;
    if (ey > MAP_H) ey = MAP_H;

    for (var ty = sy; ty < ey; ++ty) {
        for (var tx = sx; tx < ex; ++tx) {
            var t = tile_at(tx, ty);
            var px = tx * TILE;
            var py = ty * TILE;

            if (t === T_SOLID) {
                // PLACEHOLDER: solid block (replace with gfx.spr())
                gfx.rectfill(px, py, px + 7, py + 7, 4);
                gfx.rect(px, py, px + 7, py + 7, 2);
            } else if (t === T_COIN) {
                // PLACEHOLDER: spinning coin
                var cr = 2 + math.abs(math.sin(sys.time() * 0.3));
                gfx.circfill(px + 4, py + 4, cr, 10);
            } else if (t === T_SPIKE) {
                // PLACEHOLDER: spike triangle
                gfx.trifill(px, py + 7, px + 7, py + 7, px + 3, py, 8);
            }
        }
    }

    // --- Draw enemies ---
    for (var i = 0; i < enemies.length; ++i) {
        var e = enemies[i];
        if (!e.alive) continue;
        // PLACEHOLDER: red box (replace with gfx.spr())
        gfx.rectfill(e.x, e.y, e.x + e.w, e.y + e.h, 8);
        // Eyes
        var ex1 = e.vx > 0 ? e.x + 4 : e.x + 1;
        gfx.pset(ex1, e.y + 2, 7);
        gfx.pset(ex1 + 2, e.y + 2, 7);
    }

    // --- Draw player ---
    if (player.alive) {
        // PLACEHOLDER: coloured box (replace with gfx.spr())
        gfx.rectfill(player.x, player.y, player.x + player.w, player.y + player.h, 11);
        // Eye
        var eye_x = player.facing > 0 ? player.x + 4 : player.x + 1;
        gfx.pset(eye_x, player.y + 2, 0);
    }

    // --- HUD (fixed position) ---
    gfx.camera(0, 0);

    gfx.rectfill(0, 0, 319, 10, 0);
    gfx.print("score:" + player.score, 4, 2, 10);
    gfx.print("high:" + high_score, 80, 2, 7);
    gfx.print("coins:" + coins_left, 160, 2, 10);

    if (!player.alive) {
        gfx.rectfill(80, 70, 240, 110, 0);
        gfx.rect(80, 70, 240, 110, 8);
        gfx.print("game over!", 130, 78, 8);
        gfx.print("score: " + player.score, 130, 90, 7);
        gfx.print("press jump to retry", 110, 100, 5);
    }

    // FPS widget
    draw_fps_widget();
}

// --- Hot reload state preservation -----------------------------------

function _save() {
    return {
        tiles: tiles,
        coins_left: coins_left,
        player: player,
        high_score: high_score,
        cam_x: cam_x,
        cam_y: cam_y,
        enemies: enemies,
    };
}

function _restore(state) {
    tiles = state.tiles;
    coins_left = state.coins_left;
    player = state.player;
    high_score = state.high_score;
    cam_x = state.cam_x;
    cam_y = state.cam_y;
    enemies = state.enemies;
}
