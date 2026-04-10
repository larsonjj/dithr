// Gauntlet Example
//
// A top-down dungeon crawler inspired by NES Gauntlet:
//   - Four character classes (Warrior, Valkyrie, Wizard, Elf)
//   - Procedurally generated dungeon rooms with walls and corridors
//   - Enemy spawners (generators) that produce ghosts and demons
//   - Melee and ranged (projectile) attacks
//   - Collectible food (health), keys, treasure, and potions
//   - Locked doors requiring keys
//   - Health drains slowly over time (classic Gauntlet mechanic)
//   - Exit staircase to advance to the next level
//   - HUD with health, score, keys, and level number
//
// All art is drawn with primitives (rectfill/circfill/trifill).

// --- Constants -------------------------------------------------------

var TILE = 8;
var MAP_W = 50;
var MAP_H = 40;
var SCREEN_W = 320;
var SCREEN_H = 180;
var HUD_H = 14;

// Tile types
var T_EMPTY = 0;
var T_WALL = 1;
var T_EXIT = 2;
var T_DOOR = 3;
var T_FOOD = 4;
var T_KEY = 5;
var T_TREASURE = 6;
var T_POTION = 7;
var T_SPAWNER = 8;

// Character classes
var CLASS_WARRIOR = 0;
var CLASS_VALKYRIE = 1;
var CLASS_WIZARD = 2;
var CLASS_ELF = 3;

var CLASS_NAMES = ["WARRIOR", "VALKYRIE", "WIZARD", "ELF"];
var CLASS_COLORS = [8, 12, 13, 11]; // red, blue, purple, green
var CLASS_SPEEDS = [0.28, 0.28, 0.25, 0.32];
var CLASS_SHOT_DAMAGE = [8, 6, 12, 10];
var CLASS_MELEE_DAMAGE = [15, 12, 5, 8];
var CLASS_MAX_HP = [800, 900, 600, 700];

// --- Game state ------------------------------------------------------

var state = "select"; // "select", "play", "dead", "level_clear"

// Player
var player = {};
var player_class = CLASS_WARRIOR;

// Projectiles
var shots = [];
var SHOT_SPEED = 0.8;
var SHOT_LIFETIME = 40;

// Enemies
var enemies = [];
var spawners = [];
var ENEMY_SPEED = 0.2;
var GHOST_HP = 10;
var DEMON_HP = 20;

// Map
var tiles = [];
var level = 1;
var cam_x = 0;
var cam_y = 0;

// Attack cooldown
var attack_timer = 0;
var ATTACK_COOLDOWN = 10;

// Health drain timer
var drain_timer = 0;
var DRAIN_INTERVAL = 60; // frames between health drain ticks
var DRAIN_AMOUNT = 1;

// Potion
var potion_active = false;
var potion_timer = 0;
var POTION_DURATION = 180; // 3 seconds at 60fps

// Level transition
var transition_timer = 0;
var TRANSITION_FRAMES = 60;

// Select screen
var select_cursor = 0;

// Spawn timer for generators
var spawn_timer = 0;
var SPAWN_INTERVAL = 180;
var MAX_ENEMIES = 40;

// Flashing timer for pickups
var flash_tick = 0;

// Smoothed FPS (exponential moving average)
var smooth_fps = 60;
var fps_history = [];
var fps_hist_idx = 0;
var FPS_HIST_LEN = 50;
for (var _i = 0; _i < FPS_HIST_LEN; ++_i) fps_history.push(60);

function draw_fps_widget() {
    var wx = SCREEN_W - FPS_HIST_LEN - 4;
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

// --- Tile helpers ----------------------------------------------------

function tile_at(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) {
        return T_WALL;
    }
    return tiles[cy * MAP_W + cx];
}

function set_tile(cx, cy, val) {
    if (cx >= 0 && cy >= 0 && cx < MAP_W && cy < MAP_H) {
        tiles[cy * MAP_W + cx] = val;
    }
}

function is_solid(cx, cy) {
    var tile = tile_at(cx, cy);
    return tile === T_WALL || tile === T_DOOR;
}

function world_solid(px, py, wid, hgt) {
    var cx0 = math.flr(px / TILE);
    var cy0 = math.flr(py / TILE);
    var cx1 = math.flr((px + wid - 1) / TILE);
    var cy1 = math.flr((py + hgt - 1) / TILE);
    for (var ty = cy0; ty <= cy1; ++ty) {
        for (var tx = cx0; tx <= cx1; ++tx) {
            if (is_solid(tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

// --- Level generation ------------------------------------------------

function generate_level() {
    tiles = [];
    enemies = [];
    shots = [];
    spawners = [];

    // Fill with walls
    for (var idx = 0; idx < MAP_W * MAP_H; ++idx) {
        tiles.push(T_WALL);
    }

    math.seed(level * 7 + 31);

    // Carve rooms
    var rooms = [];
    var attempts = 0;
    while (rooms.length < 8 + level && attempts < 200) {
        var rw = 4 + math.rndInt(6);
        var rh = 4 + math.rndInt(6);
        var rx = 2 + math.rndInt(MAP_W - rw - 4);
        var ry = 2 + math.rndInt(MAP_H - rh - 4);

        // Check overlap with existing rooms (with margin)
        var overlap = false;
        for (var ri = 0; ri < rooms.length; ++ri) {
            var other = rooms[ri];
            if (
                rx - 1 < other.x + other.w &&
                rx + rw + 1 > other.x &&
                ry - 1 < other.y + other.h &&
                ry + rh + 1 > other.y
            ) {
                overlap = true;
                break;
            }
        }
        if (!overlap) {
            rooms.push({ x: rx, y: ry, w: rw, h: rh });
            // Carve room
            for (var yy = ry; yy < ry + rh; ++yy) {
                for (var xx = rx; xx < rx + rw; ++xx) {
                    set_tile(xx, yy, T_EMPTY);
                }
            }
        }
        ++attempts;
    }

    // Connect rooms with corridors
    for (var idx = 1; idx < rooms.length; ++idx) {
        var ra = rooms[idx - 1];
        var rb = rooms[idx];
        var ax = math.flr(ra.x + ra.w / 2);
        var ay = math.flr(ra.y + ra.h / 2);
        var bx = math.flr(rb.x + rb.w / 2);
        var by = math.flr(rb.y + rb.h / 2);

        // Horizontal then vertical
        var sx = ax < bx ? ax : bx;
        var ex = ax < bx ? bx : ax;
        for (var xx = sx; xx <= ex; ++xx) {
            set_tile(xx, ay, T_EMPTY);
            // Make corridors 2-wide for easier navigation
            if (ay + 1 < MAP_H - 1) {
                set_tile(xx, ay + 1, T_EMPTY);
            }
        }
        var sy = ay < by ? ay : by;
        var ey = ay < by ? by : ay;
        for (var yy = sy; yy <= ey; ++yy) {
            set_tile(bx, yy, T_EMPTY);
            if (bx + 1 < MAP_W - 1) {
                set_tile(bx + 1, yy, T_EMPTY);
            }
        }
    }

    // Place player spawn in first room
    var spawn_room = rooms[0];
    player.x = (spawn_room.x + math.flr(spawn_room.w / 2)) * TILE;
    player.y = (spawn_room.y + math.flr(spawn_room.h / 2)) * TILE;

    // Place exit in last room
    var exit_room = rooms[rooms.length - 1];
    var exit_cx = exit_room.x + math.flr(exit_room.w / 2);
    var exit_cy = exit_room.y + math.flr(exit_room.h / 2);
    set_tile(exit_cx, exit_cy, T_EXIT);

    // Place items in rooms
    for (var ri = 1; ri < rooms.length - 1; ++ri) {
        var room = rooms[ri];
        var center_x = room.x + math.flr(room.w / 2);
        var center_y = room.y + math.flr(room.h / 2);

        // Food
        if (math.rnd() < 0.5) {
            var fx = room.x + 1 + math.rndInt(room.w - 2);
            var fy = room.y + 1 + math.rndInt(room.h - 2);
            if (tile_at(fx, fy) === T_EMPTY) {
                set_tile(fx, fy, T_FOOD);
            }
        }

        // Treasure
        if (math.rnd() < 0.6) {
            var tx = room.x + 1 + math.rndInt(room.w - 2);
            var ty = room.y + 1 + math.rndInt(room.h - 2);
            if (tile_at(tx, ty) === T_EMPTY) {
                set_tile(tx, ty, T_TREASURE);
            }
        }

        // Key (less common)
        if (math.rnd() < 0.3) {
            var kx = room.x + 1 + math.rndInt(room.w - 2);
            var ky = room.y + 1 + math.rndInt(room.h - 2);
            if (tile_at(kx, ky) === T_EMPTY) {
                set_tile(kx, ky, T_KEY);
            }
        }

        // Potion (rare)
        if (math.rnd() < 0.15) {
            var ppx = room.x + 1 + math.rndInt(room.w - 2);
            var ppy = room.y + 1 + math.rndInt(room.h - 2);
            if (tile_at(ppx, ppy) === T_EMPTY) {
                set_tile(ppx, ppy, T_POTION);
            }
        }

        // Enemy spawner
        if (math.rnd() < 0.5 + level * 0.05) {
            if (tile_at(center_x, center_y) === T_EMPTY) {
                set_tile(center_x, center_y, T_SPAWNER);
                spawners.push({
                    cx: center_x,
                    cy: center_y,
                    hp: 20 + level * 5,
                    alive: true,
                });
            }
        }
    }

    // Place doors at corridor/room transitions
    var door_count = 0;
    var max_doors = 2 + math.flr(level / 2);
    for (var ri = 1; ri < rooms.length && door_count < max_doors; ++ri) {
        var room = rooms[ri];
        // Check edges of room for corridor entry points
        for (var xx = room.x; xx < room.x + room.w && door_count < max_doors; ++xx) {
            if (tile_at(xx, room.y - 1) === T_EMPTY && math.rnd() < 0.15) {
                set_tile(xx, room.y, T_DOOR);
                ++door_count;
            }
        }
        for (var yy = room.y; yy < room.y + room.h && door_count < max_doors; ++yy) {
            if (tile_at(room.x - 1, yy) === T_EMPTY && math.rnd() < 0.15) {
                set_tile(room.x, yy, T_DOOR);
                ++door_count;
            }
        }
    }

    // Spawn initial enemies near spawners
    for (var si = 0; si < spawners.length; ++si) {
        var spn = spawners[si];
        for (var ei = 0; ei < 2 + math.flr(level / 2); ++ei) {
            spawn_enemy_near(spn.cx, spn.cy);
        }
    }
}

function spawn_enemy_near(cx, cy) {
    if (enemies.length >= MAX_ENEMIES) {
        return;
    }
    // Try random nearby tiles
    for (var att = 0; att < 20; ++att) {
        var ex = cx - 2 + math.rndInt(5);
        var ey = cy - 2 + math.rndInt(5);
        if (tile_at(ex, ey) === T_EMPTY) {
            var is_demon = math.rnd() < 0.3 + level * 0.05;
            enemies.push({
                x: ex * TILE,
                y: ey * TILE,
                w: 6,
                h: 6,
                hp: is_demon ? DEMON_HP : GHOST_HP,
                max_hp: is_demon ? DEMON_HP : GHOST_HP,
                type: is_demon ? 1 : 0, // 0=ghost, 1=demon
                speed: (is_demon ? 0.08 : 0.12) + level * 0.005,
                damage: is_demon ? 3 : 1,
                alive: true,
                hit_flash: 0,
            });
            return;
        }
    }
}

// --- Player init -----------------------------------------------------

function init_player() {
    player = {
        x: 0,
        y: 0,
        w: 6,
        h: 6,
        vx: 0,
        vy: 0,
        hp: CLASS_MAX_HP[player_class],
        max_hp: CLASS_MAX_HP[player_class],
        score: 0,
        keys: 0,
        dir_x: 0,
        dir_y: 1, // facing down
        alive: true,
        hit_flash: 0,
        melee_timer: 0,
    };
}

// --- Projectile management -------------------------------------------

function fire_shot() {
    if (attack_timer > 0) {
        return;
    }
    if (player.dir_x === 0 && player.dir_y === 0) {
        return;
    }

    var len = math.sqrt(player.dir_x * player.dir_x + player.dir_y * player.dir_y);
    if (len === 0) {
        return;
    }
    var ndx = player.dir_x / len;
    var ndy = player.dir_y / len;

    shots.push({
        x: player.x + 2,
        y: player.y + 2,
        vx: ndx * SHOT_SPEED,
        vy: ndy * SHOT_SPEED,
        life: SHOT_LIFETIME,
        damage: CLASS_SHOT_DAMAGE[player_class],
    });
    attack_timer = ATTACK_COOLDOWN;
}

function melee_attack() {
    if (player.melee_timer > 0) {
        return;
    }
    player.melee_timer = 15;
    var mx = player.x + player.dir_x * 8;
    var my = player.y + player.dir_y * 8;
    var mw = 10;
    var mh = 10;
    var dmg = CLASS_MELEE_DAMAGE[player_class];

    for (var idx = 0; idx < enemies.length; ++idx) {
        var ene = enemies[idx];
        if (!ene.alive) {
            continue;
        }
        if (col.rect(mx - mw / 2, my - mh / 2, mw, mh, ene.x, ene.y, ene.w, ene.h)) {
            ene.hp -= dmg;
            ene.hit_flash = 6;
            if (ene.hp <= 0) {
                ene.alive = false;
                player.score += ene.type === 1 ? 20 : 10;
            }
        }
    }

    // Melee can also destroy spawners
    for (var si = 0; si < spawners.length; ++si) {
        var spn = spawners[si];
        if (!spn.alive) {
            continue;
        }
        var spx = spn.cx * TILE;
        var spy = spn.cy * TILE;
        if (col.rect(mx - mw / 2, my - mh / 2, mw, mh, spx, spy, TILE, TILE)) {
            spn.hp -= dmg;
            if (spn.hp <= 0) {
                spn.alive = false;
                set_tile(spn.cx, spn.cy, T_EMPTY);
                player.score += 100;
            }
        }
    }
}

// --- Callbacks -------------------------------------------------------

function _init() {
    state = "select";
    select_cursor = 0;
}

function _update(dt) {
    flash_tick += 1;
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    if (state === "select") {
        update_select();
    } else if (state === "play") {
        update_play();
    } else if (state === "dead") {
        update_dead();
    } else if (state === "level_clear") {
        update_level_clear();
    }
}

function _draw() {
    if (state === "select") {
        draw_select();
    } else if (state === "play" || state === "dead") {
        draw_play();
    } else if (state === "level_clear") {
        draw_level_clear();
    }
}

// --- Character select ------------------------------------------------

function update_select() {
    if (input.btnp("up")) {
        select_cursor = (select_cursor + 3) % 4;
    }
    if (input.btnp("down")) {
        select_cursor = (select_cursor + 1) % 4;
    }
    if (input.btnp("attack") || input.btnp("use")) {
        player_class = select_cursor;
        init_player();
        level = 1;
        generate_level();
        state = "play";
    }
}

function draw_select() {
    gfx.cls(0);
    var title = "GAUNTLET";
    var tw = gfx.textWidth(title);
    gfx.print(title, (SCREEN_W - tw) / 2, 20, 10);

    var sub = "Choose your hero";
    var sw = gfx.textWidth(sub);
    gfx.print(sub, (SCREEN_W - sw) / 2, 35, 7);

    for (var idx = 0; idx < 4; ++idx) {
        var yy = 55 + idx * 28;
        var clr = CLASS_COLORS[idx];
        var is_sel = idx === select_cursor;

        // Selection arrow
        if (is_sel) {
            gfx.print(">", 70, yy + 2, 10);
        }

        // Character preview box
        var bx = 85;
        gfx.rectfill(bx, yy, bx + 8, yy + 8, clr);
        // Eyes
        gfx.pset(bx + 2, yy + 3, 7);
        gfx.pset(bx + 5, yy + 3, 7);

        // Name and stats
        gfx.print(CLASS_NAMES[idx], 100, yy, is_sel ? 7 : 5);
        gfx.print(
            "HP:" +
                CLASS_MAX_HP[idx] +
                " ATK:" +
                CLASS_MELEE_DAMAGE[idx] +
                " MAG:" +
                CLASS_SHOT_DAMAGE[idx] +
                " SPD:" +
                math.flr(CLASS_SPEEDS[idx] * 10),
            100,
            yy + 8,
            is_sel ? 6 : 5,
        );
    }

    var hint = "Press ATTACK to start";
    var hw = gfx.textWidth(hint);
    gfx.print(hint, (SCREEN_W - hw) / 2, 170, math.flr(flash_tick / 30) % 2 === 0 ? 7 : 5);
}

// --- Main gameplay ---------------------------------------------------

function update_play() {
    // Attack cooldowns
    if (attack_timer > 0) {
        --attack_timer;
    }
    if (player.melee_timer > 0) {
        --player.melee_timer;
    }
    if (player.hit_flash > 0) {
        --player.hit_flash;
    }

    // Health drain (classic Gauntlet mechanic)
    ++drain_timer;
    if (drain_timer >= DRAIN_INTERVAL) {
        drain_timer = 0;
        if (player.hp > 0) {
            player.hp -= DRAIN_AMOUNT;
        }
    }

    // Potion timer
    if (potion_active) {
        --potion_timer;
        if (potion_timer <= 0) {
            potion_active = false;
        }
    }

    // Movement
    var spd = CLASS_SPEEDS[player_class];
    var dx = 0;
    var dy = 0;
    if (input.btn("left")) {
        dx = -spd;
    }
    if (input.btn("right")) {
        dx = spd;
    }
    if (input.btn("up")) {
        dy = -spd;
    }
    if (input.btn("down")) {
        dy = spd;
    }

    // Normalize diagonal movement
    if (dx !== 0 && dy !== 0) {
        var dlen = math.sqrt(dx * dx + dy * dy);
        dx = (dx / dlen) * spd;
        dy = (dy / dlen) * spd;
    }

    // Update facing direction
    if (dx !== 0 || dy !== 0) {
        player.dir_x = dx > 0 ? 1 : dx < 0 ? -1 : 0;
        player.dir_y = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    }

    // Apply movement with collision
    if (dx !== 0) {
        var nx = player.x + dx;
        if (!world_solid(nx, player.y, player.w, player.h)) {
            player.x = nx;
        } else {
            // Try sliding along wall
            player.x = player.x;
        }
    }
    if (dy !== 0) {
        var ny = player.y + dy;
        if (!world_solid(player.x, ny, player.w, player.h)) {
            player.y = ny;
        }
    }

    // Attack
    if (input.btnp("attack")) {
        if (player_class === CLASS_WIZARD || player_class === CLASS_ELF) {
            fire_shot();
        } else {
            melee_attack();
        }
    }

    // Use (secondary) — warriors/valkyries can shoot, wizards/elves can melee
    if (input.btnp("use")) {
        if (player_class === CLASS_WIZARD || player_class === CLASS_ELF) {
            melee_attack();
        } else {
            fire_shot();
        }
    }

    // Tile pickups
    var pcx0 = math.flr(player.x / TILE);
    var pcy0 = math.flr(player.y / TILE);
    var pcx1 = math.flr((player.x + player.w - 1) / TILE);
    var pcy1 = math.flr((player.y + player.h - 1) / TILE);

    for (var ty = pcy0; ty <= pcy1; ++ty) {
        for (var tx = pcx0; tx <= pcx1; ++tx) {
            var tile = tile_at(tx, ty);

            if (tile === T_FOOD) {
                set_tile(tx, ty, T_EMPTY);
                player.hp = math.min(player.hp + 100, player.max_hp);
                player.score += 5;
            } else if (tile === T_KEY) {
                set_tile(tx, ty, T_EMPTY);
                player.keys += 1;
                player.score += 10;
            } else if (tile === T_TREASURE) {
                set_tile(tx, ty, T_EMPTY);
                player.score += 100;
            } else if (tile === T_POTION) {
                set_tile(tx, ty, T_EMPTY);
                potion_active = true;
                potion_timer = POTION_DURATION;
                player.score += 50;
                // Kill all visible enemies (screen nuke)
                for (var ei = 0; ei < enemies.length; ++ei) {
                    if (enemies[ei].alive) {
                        var edx = enemies[ei].x - player.x;
                        var edy = enemies[ei].y - player.y;
                        if (math.abs(edx) < SCREEN_W / 2 && math.abs(edy) < SCREEN_H / 2) {
                            enemies[ei].alive = false;
                            player.score += 5;
                        }
                    }
                }
            } else if (tile === T_DOOR) {
                if (player.keys > 0) {
                    set_tile(tx, ty, T_EMPTY);
                    player.keys -= 1;
                }
            } else if (tile === T_EXIT) {
                state = "level_clear";
                transition_timer = TRANSITION_FRAMES;
                player.score += 200;
                return;
            }
        }
    }

    // Update projectiles
    for (var si = shots.length - 1; si >= 0; --si) {
        var shot = shots[si];
        shot.x += shot.vx;
        shot.y += shot.vy;
        --shot.life;

        // Wall collision
        var scx = math.flr((shot.x + 1) / TILE);
        var scy = math.flr((shot.y + 1) / TILE);
        if (is_solid(scx, scy) || shot.life <= 0) {
            // Shots can destroy spawners
            for (var spi = 0; spi < spawners.length; ++spi) {
                var spn = spawners[spi];
                if (spn.alive && spn.cx === scx && spn.cy === scy) {
                    spn.hp -= shot.damage;
                    if (spn.hp <= 0) {
                        spn.alive = false;
                        set_tile(spn.cx, spn.cy, T_EMPTY);
                        player.score += 100;
                    }
                }
            }
            shots.splice(si, 1);
            continue;
        }

        // Enemy collision
        var hit = false;
        for (var ei = 0; ei < enemies.length; ++ei) {
            var ene = enemies[ei];
            if (!ene.alive) {
                continue;
            }
            if (col.rect(shot.x, shot.y, 3, 3, ene.x, ene.y, ene.w, ene.h)) {
                ene.hp -= shot.damage;
                ene.hit_flash = 6;
                if (ene.hp <= 0) {
                    ene.alive = false;
                    player.score += ene.type === 1 ? 20 : 10;
                }
                hit = true;
                break;
            }
        }
        if (hit) {
            shots.splice(si, 1);
        }
    }

    // Update enemies (chase player)
    for (var ei = 0; ei < enemies.length; ++ei) {
        var ene = enemies[ei];
        if (!ene.alive) {
            continue;
        }
        if (ene.hit_flash > 0) {
            --ene.hit_flash;
        }

        // Simple chase AI
        var edx = player.x - ene.x;
        var edy = player.y - ene.y;
        var edist = math.sqrt(edx * edx + edy * edy);

        if (edist > 1 && edist < 200) {
            var enx = (edx / edist) * ene.speed;
            var eny = (edy / edist) * ene.speed;

            // Try X movement
            var nnx = ene.x + enx;
            if (!world_solid(nnx, ene.y, ene.w, ene.h)) {
                ene.x = nnx;
            }
            // Try Y movement
            var nny = ene.y + eny;
            if (!world_solid(ene.x, nny, ene.w, ene.h)) {
                ene.y = nny;
            }
        }

        // Damage player on contact
        if (
            player.alive &&
            col.rect(player.x, player.y, player.w, player.h, ene.x, ene.y, ene.w, ene.h)
        ) {
            if (!potion_active) {
                player.hp -= ene.damage;
                player.hit_flash = 6;
            } else {
                // Potion makes player invulnerable and kills on contact
                ene.alive = false;
                player.score += 5;
            }
        }
    }

    // Spawner enemy generation
    ++spawn_timer;
    if (spawn_timer >= SPAWN_INTERVAL) {
        spawn_timer = 0;
        for (var si = 0; si < spawners.length; ++si) {
            var spn = spawners[si];
            if (spn.alive) {
                spawn_enemy_near(spn.cx, spn.cy);
            }
        }
    }

    // Clean up dead enemies
    for (var ei = enemies.length - 1; ei >= 0; --ei) {
        if (!enemies[ei].alive) {
            enemies.splice(ei, 1);
        }
    }

    // Camera follow
    var target_x = player.x - SCREEN_W / 2 + player.w / 2;
    var target_y = player.y - (SCREEN_H - HUD_H) / 2 + player.h / 2;
    cam_x += (target_x - cam_x) * 0.1;
    cam_y += (target_y - cam_y) * 0.1;

    var max_cx = MAP_W * TILE - SCREEN_W;
    var max_cy = MAP_H * TILE - (SCREEN_H - HUD_H);
    if (cam_x < 0) {
        cam_x = 0;
    }
    if (cam_y < 0) {
        cam_y = 0;
    }
    if (cam_x > max_cx) {
        cam_x = max_cx;
    }
    if (cam_y > max_cy) {
        cam_y = max_cy;
    }

    // Death check
    if (player.hp <= 0) {
        player.alive = false;
        state = "dead";
    }
}

// --- Drawing ---------------------------------------------------------

function draw_play() {
    gfx.cls(0);

    var ox = math.flr(cam_x);
    var oy = math.flr(cam_y);
    gfx.camera(ox, oy - HUD_H);

    // Visible tile range
    var sx = math.flr(cam_x / TILE) - 1;
    var sy = math.flr(cam_y / TILE) - 1;
    var ex = sx + math.flr(SCREEN_W / TILE) + 3;
    var ey = sy + math.flr(SCREEN_H / TILE) + 3;
    if (sx < 0) {
        sx = 0;
    }
    if (sy < 0) {
        sy = 0;
    }
    if (ex > MAP_W) {
        ex = MAP_W;
    }
    if (ey > MAP_H) {
        ey = MAP_H;
    }

    // Draw tiles
    for (var ty = sy; ty < ey; ++ty) {
        for (var tx = sx; tx < ex; ++tx) {
            var tile = tile_at(tx, ty);
            var px = tx * TILE;
            var py = ty * TILE;

            if (tile === T_WALL) {
                // Dark stone wall
                gfx.rectfill(px, py, px + 7, py + 7, 5);
                // Brick pattern
                if ((tx + ty) % 2 === 0) {
                    gfx.line(px, py + 3, px + 7, py + 3, 1);
                    gfx.line(px + 3, py, px + 3, py + 3, 1);
                } else {
                    gfx.line(px, py + 4, px + 7, py + 4, 1);
                    gfx.line(px + 5, py + 4, px + 5, py + 7, 1);
                }
            } else if (tile === T_EMPTY) {
                // Floor
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                if ((tx + ty) % 4 === 0) {
                    gfx.pset(px + 3, py + 3, 2);
                }
            } else if (tile === T_EXIT) {
                // Staircase exit
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                var blink = math.flr(flash_tick / 15) % 2;
                gfx.rectfill(px + 1, py + 1, px + 6, py + 6, blink === 0 ? 10 : 9);
                gfx.rect(px + 1, py + 1, px + 6, py + 6, 7);
                // Stair lines
                gfx.line(px + 2, py + 3, px + 5, py + 3, 7);
                gfx.line(px + 2, py + 5, px + 5, py + 5, 7);
            } else if (tile === T_DOOR) {
                // Locked door
                gfx.rectfill(px, py, px + 7, py + 7, 4);
                gfx.rect(px, py, px + 7, py + 7, 2);
                // Lock symbol
                gfx.circfill(px + 3, py + 3, 1, 10);
                gfx.line(px + 3, py + 4, px + 3, py + 6, 10);
            } else if (tile === T_FOOD) {
                // Floor + food (turkey leg)
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                gfx.circfill(px + 4, py + 3, 2, 4);
                gfx.line(px + 2, py + 3, px + 1, py + 6, 4);
            } else if (tile === T_KEY) {
                // Floor + key
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                var kblink = math.flr(flash_tick / 20) % 2;
                gfx.circfill(px + 3, py + 2, 1, kblink === 0 ? 10 : 9);
                gfx.line(px + 3, py + 3, px + 3, py + 6, kblink === 0 ? 10 : 9);
                gfx.pset(px + 4, py + 5, kblink === 0 ? 10 : 9);
            } else if (tile === T_TREASURE) {
                // Floor + treasure chest
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                gfx.rectfill(px + 1, py + 3, px + 6, py + 6, 9);
                gfx.rect(px + 1, py + 3, px + 6, py + 6, 4);
                gfx.pset(px + 3, py + 4, 10);
            } else if (tile === T_POTION) {
                // Floor + potion bottle
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                var pblink = math.flr(flash_tick / 10) % 2;
                gfx.rectfill(px + 2, py + 3, px + 5, py + 6, pblink === 0 ? 12 : 13);
                gfx.rectfill(px + 3, py + 1, px + 4, py + 3, 6);
            } else if (tile === T_SPAWNER) {
                // Spawner (generator)
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                var sblink = math.flr(flash_tick / 8) % 2;
                gfx.rectfill(px + 1, py + 1, px + 6, py + 6, sblink === 0 ? 2 : 8);
                gfx.rect(px + 1, py + 1, px + 6, py + 6, 5);
            }
        }
    }

    // Draw enemies
    for (var ei = 0; ei < enemies.length; ++ei) {
        var ene = enemies[ei];
        if (!ene.alive) {
            continue;
        }
        var ecol;
        if (ene.hit_flash > 0) {
            ecol = 7; // white flash
        } else if (ene.type === 0) {
            ecol = 6; // ghost = light grey
        } else {
            ecol = 8; // demon = red
        }

        if (ene.type === 0) {
            // Ghost — round shape
            gfx.circfill(ene.x + 3, ene.y + 3, 3, ecol);
            gfx.pset(ene.x + 2, ene.y + 2, 0);
            gfx.pset(ene.x + 4, ene.y + 2, 0);
        } else {
            // Demon — box with horns
            gfx.rectfill(ene.x, ene.y + 1, ene.x + ene.w, ene.y + ene.h, ecol);
            gfx.pset(ene.x, ene.y, ecol);
            gfx.pset(ene.x + ene.w, ene.y, ecol);
            gfx.pset(ene.x + 2, ene.y + 2, 0);
            gfx.pset(ene.x + 4, ene.y + 2, 0);
        }
    }

    // Draw spawners (overlay HP bars)
    for (var si = 0; si < spawners.length; ++si) {
        var spn = spawners[si];
        if (!spn.alive) {
            continue;
        }
        var spx = spn.cx * TILE;
        var spy = spn.cy * TILE;
        var hp_pct = spn.hp / (20 + level * 5);
        var bar_w = math.flr(6 * hp_pct);
        if (bar_w > 0) {
            gfx.rectfill(spx + 1, spy - 2, spx + 1 + bar_w, spy - 1, 8);
        }
    }

    // Draw projectiles
    for (var si = 0; si < shots.length; ++si) {
        var shot = shots[si];
        var shot_col = 10;
        if (player_class === CLASS_WIZARD) {
            shot_col = 13;
        } else if (player_class === CLASS_ELF) {
            shot_col = 11;
        }
        gfx.circfill(shot.x + 1, shot.y + 1, 1, shot_col);
    }

    // Draw player
    if (player.alive) {
        var pcol = CLASS_COLORS[player_class];
        if (player.hit_flash > 0) {
            pcol = 7;
        }
        if (potion_active && math.flr(flash_tick / 4) % 2 === 0) {
            pcol = 7;
        }

        gfx.rectfill(player.x, player.y, player.x + player.w, player.y + player.h, pcol);
        // Eyes based on facing direction
        var eye_x1 = player.x + 1 + (player.dir_x > 0 ? 2 : 0);
        var eye_x2 = eye_x1 + 2;
        var eye_y = player.y + 2 + (player.dir_y > 0 ? 1 : 0);
        gfx.pset(eye_x1, eye_y, 7);
        gfx.pset(eye_x2, eye_y, 7);

        // Melee attack indicator
        if (player.melee_timer > 10) {
            var mx = player.x + player.dir_x * 8;
            var my = player.y + player.dir_y * 8;
            gfx.circ(mx + 3, my + 3, 4, 7);
        }
    }

    // --- HUD ---
    gfx.camera(0, 0);

    // HUD background
    gfx.rectfill(0, 0, SCREEN_W - 1, HUD_H - 1, 0);
    gfx.line(0, HUD_H - 1, SCREEN_W - 1, HUD_H - 1, 5);

    // Class name
    gfx.print(CLASS_NAMES[player_class], 2, 2, CLASS_COLORS[player_class]);

    // HP bar
    var hp_bar_x = 50;
    var hp_bar_w = 50;
    var hp_pct = player.hp / player.max_hp;
    if (hp_pct < 0) {
        hp_pct = 0;
    }
    gfx.rectfill(hp_bar_x, 2, hp_bar_x + hp_bar_w, 8, 2);
    var fill_w = math.flr(hp_bar_w * hp_pct);
    if (fill_w > 0) {
        var hp_col = hp_pct > 0.5 ? 11 : hp_pct > 0.25 ? 9 : 8;
        gfx.rectfill(hp_bar_x, 2, hp_bar_x + fill_w, 8, hp_col);
    }
    gfx.rect(hp_bar_x, 2, hp_bar_x + hp_bar_w, 8, 5);
    gfx.print(player.hp, hp_bar_x + 2, 3, 7);

    // Keys
    gfx.print("KEY:" + player.keys, 110, 4, 10);

    // Score
    gfx.print("SCORE:" + player.score, 155, 4, 7);

    // Level
    gfx.print("LV:" + level, 230, 4, 6);

    // Potion indicator
    if (potion_active) {
        gfx.print("POTION!", 270, 4, 13);
    }

    // FPS widget
    draw_fps_widget();

    // Death overlay
    if (state === "dead") {
        draw_death_overlay();
    }
}

function draw_death_overlay() {
    gfx.rectfill(60, 50, 260, 130, 0);
    gfx.rect(60, 50, 260, 130, 8);

    var title = "YOU HAVE DIED";
    var tw = gfx.textWidth(title);
    gfx.print(title, (SCREEN_W - tw) / 2, 60, 8);

    gfx.print("Class: " + CLASS_NAMES[player_class], 80, 78, CLASS_COLORS[player_class]);
    gfx.print("Score: " + player.score, 80, 90, 7);
    gfx.print("Level: " + level, 80, 100, 6);

    var hint = "Press ATTACK to retry";
    var hw = gfx.textWidth(hint);
    gfx.print(hint, (SCREEN_W - hw) / 2, 118, math.flr(flash_tick / 30) % 2 === 0 ? 7 : 5);
}

function update_dead() {
    if (input.btnp("attack")) {
        state = "select";
    }
}

// --- Level transition ------------------------------------------------

function update_level_clear() {
    --transition_timer;
    if (transition_timer <= 0) {
        ++level;
        generate_level();
        state = "play";
    }
}

function draw_level_clear() {
    gfx.cls(0);

    var pct = 1.0 - transition_timer / TRANSITION_FRAMES;
    var msg = "LEVEL " + level + " COMPLETE!";
    var mw = gfx.textWidth(msg);
    gfx.print(msg, (SCREEN_W - mw) / 2, 70, 10);

    var msg2 = "Entering level " + (level + 1) + "...";
    var m2w = gfx.textWidth(msg2);
    gfx.print(msg2, (SCREEN_W - m2w) / 2, 90, 7);

    // Progress bar
    var bar_x = 100;
    var bar_w = 120;
    gfx.rect(bar_x, 110, bar_x + bar_w, 116, 5);
    var prog = math.flr(bar_w * pct);
    if (prog > 0) {
        gfx.rectfill(bar_x + 1, 111, bar_x + prog, 115, 11);
    }
}

// --- Hot reload state preservation -----------------------------------

function _save() {
    return {
        state: state,
        player: player,
        player_class: player_class,
        tiles: tiles,
        enemies: enemies,
        spawners: spawners,
        shots: shots,
        level: level,
        cam_x: cam_x,
        cam_y: cam_y,
        attack_timer: attack_timer,
        drain_timer: drain_timer,
        potion_active: potion_active,
        potion_timer: potion_timer,
        select_cursor: select_cursor,
        spawn_timer: spawn_timer,
        flash_tick: flash_tick,
        smooth_fps: smooth_fps,
        fps_history: fps_history.slice(),
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(sav) {
    state = sav.state;
    player = sav.player;
    player_class = sav.player_class;
    tiles = sav.tiles;
    enemies = sav.enemies;
    spawners = sav.spawners;
    shots = sav.shots;
    level = sav.level;
    cam_x = sav.cam_x;
    cam_y = sav.cam_y;
    attack_timer = sav.attack_timer;
    drain_timer = sav.drain_timer;
    potion_active = sav.potion_active;
    potion_timer = sav.potion_timer;
    select_cursor = sav.select_cursor;
    spawn_timer = sav.spawn_timer;
    flash_tick = sav.flash_tick;
    smooth_fps = sav.smooth_fps;
    if (sav.fps_history) {
        fps_history = sav.fps_history;
        fps_hist_idx = sav.fps_hist_idx;
    }
}
