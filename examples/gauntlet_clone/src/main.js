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

const TILE = 8;
const MAP_W = 50;
const MAP_H = 40;
const SCREEN_W = 320;
const SCREEN_H = 180;
const HUD_H = 14;

// Tile types
const T_EMPTY = 0;
const T_WALL = 1;
const T_EXIT = 2;
const T_DOOR = 3;
const T_FOOD = 4;
const T_KEY = 5;
const T_TREASURE = 6;
const T_POTION = 7;
const T_SPAWNER = 8;

// Character classes
const CLASS_WARRIOR = 0;
const _CLASS_VALKYRIE = 1;
const CLASS_WIZARD = 2;
const CLASS_ELF = 3;

const CLASS_NAMES = ['WARRIOR', 'VALKYRIE', 'WIZARD', 'ELF'];
const CLASS_COLORS = [8, 12, 13, 11]; // red, blue, purple, green
const CLASS_SPEEDS = [0.28, 0.28, 0.25, 0.32];
const CLASS_SHOT_DAMAGE = [8, 6, 12, 10];
const CLASS_MELEE_DAMAGE = [15, 12, 5, 8];
const CLASS_MAX_HP = [800, 900, 600, 700];

// --- Game state ------------------------------------------------------

let state = 'select'; // "select", "play", "dead", "level_clear"

// Player
let player = {};
let playerClass = CLASS_WARRIOR;

// Projectiles
let shots = [];
const SHOT_SPEED = 0.8;
const SHOT_LIFETIME = 40;

// Enemies
let enemies = [];
let spawners = [];
const _ENEMY_SPEED = 0.2;
const GHOST_HP = 10;
const DEMON_HP = 20;

// Map
let tiles = [];
let level = 1;

// Attack cooldown
let attackTimer = 0;
const ATTACK_COOLDOWN = 10;

// Health drain timer
let drainTimer = 0;
const DRAIN_INTERVAL = 60; // frames between health drain ticks
const DRAIN_AMOUNT = 1;

// Potion
let potionActive = false;
let potionTimer = 0;
const POTION_DURATION = 180; // 3 seconds at 60fps

// Level transition
let transitionTimer = 0;
const TRANSITION_FRAMES = 60;

// Select screen
let selectCursor = 0;

// Spawn timer for generators
let spawnTimer = 0;
const SPAWN_INTERVAL = 180;
const MAX_ENEMIES = 40;

// Flashing timer for pickups
let flashTick = 0;

// Smoothed FPS (exponential moving average)


// --- Tile helpers ----------------------------------------------------

function tileAt(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) {
        return T_WALL;
    }
    return tiles[cy * MAP_W + cx];
}

function setTile(cx, cy, val) {
    if (cx >= 0 && cy >= 0 && cx < MAP_W && cy < MAP_H) {
        tiles[cy * MAP_W + cx] = val;
    }
}

function isSolid(cx, cy) {
    const tile = tileAt(cx, cy);
    return tile === T_WALL || tile === T_DOOR;
}

function worldSolid(px, py, wid, hgt) {
    const cx0 = math.flr(px / TILE);
    const cy0 = math.flr(py / TILE);
    const cx1 = math.flr((px + wid - 1) / TILE);
    const cy1 = math.flr((py + hgt - 1) / TILE);
    for (let ty = cy0; ty <= cy1; ++ty) {
        for (let tx = cx0; tx <= cx1; ++tx) {
            if (isSolid(tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

// --- Level generation ------------------------------------------------

function generateLevel() {
    tiles = [];
    enemies = [];
    shots = [];
    spawners = [];

    // Fill with walls
    for (let idx = 0; idx < MAP_W * MAP_H; ++idx) {
        tiles.push(T_WALL);
    }

    math.seed(level * 7 + 31);

    // Carve rooms
    const rooms = [];
    let attempts = 0;
    while (rooms.length < 8 + level && attempts < 200) {
        const rw = 4 + math.rndInt(6);
        const rh = 4 + math.rndInt(6);
        const rx = 2 + math.rndInt(MAP_W - rw - 4);
        const ry = 2 + math.rndInt(MAP_H - rh - 4);

        // Check overlap with existing rooms (with margin)
        let overlap = false;
        for (let ri = 0; ri < rooms.length; ++ri) {
            const other = rooms[ri];
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
            for (let yy = ry; yy < ry + rh; ++yy) {
                for (let xx = rx; xx < rx + rw; ++xx) {
                    setTile(xx, yy, T_EMPTY);
                }
            }
        }
        ++attempts;
    }

    // Connect rooms with corridors
    for (let idx = 1; idx < rooms.length; ++idx) {
        const ra = rooms[idx - 1];
        const rb = rooms[idx];
        const ax = math.flr(ra.x + ra.w / 2);
        const ay = math.flr(ra.y + ra.h / 2);
        const bx = math.flr(rb.x + rb.w / 2);
        const by = math.flr(rb.y + rb.h / 2);

        // Horizontal then vertical
        const sx = ax < bx ? ax : bx;
        const ex = ax < bx ? bx : ax;
        for (let xx = sx; xx <= ex; ++xx) {
            setTile(xx, ay, T_EMPTY);
            // Make corridors 2-wide for easier navigation
            if (ay + 1 < MAP_H - 1) {
                setTile(xx, ay + 1, T_EMPTY);
            }
        }
        const sy = ay < by ? ay : by;
        const ey = ay < by ? by : ay;
        for (let yy = sy; yy <= ey; ++yy) {
            setTile(bx, yy, T_EMPTY);
            if (bx + 1 < MAP_W - 1) {
                setTile(bx + 1, yy, T_EMPTY);
            }
        }
    }

    // Place player spawn in first room
    const spawnRoom = rooms[0];
    player.x = (spawnRoom.x + math.flr(spawnRoom.w / 2)) * TILE;
    player.y = (spawnRoom.y + math.flr(spawnRoom.h / 2)) * TILE;

    // Place exit in last room
    const exitRoom = rooms[rooms.length - 1];
    const exitCx = exitRoom.x + math.flr(exitRoom.w / 2);
    const exitCy = exitRoom.y + math.flr(exitRoom.h / 2);
    setTile(exitCx, exitCy, T_EXIT);

    // Place items in rooms
    for (let ri = 1; ri < rooms.length - 1; ++ri) {
        const room = rooms[ri];
        const centerX = room.x + math.flr(room.w / 2);
        const centerY = room.y + math.flr(room.h / 2);

        // Food
        if (math.rnd() < 0.5) {
            const fx = room.x + 1 + math.rndInt(room.w - 2);
            const fy = room.y + 1 + math.rndInt(room.h - 2);
            if (tileAt(fx, fy) === T_EMPTY) {
                setTile(fx, fy, T_FOOD);
            }
        }

        // Treasure
        if (math.rnd() < 0.6) {
            const tx = room.x + 1 + math.rndInt(room.w - 2);
            const ty = room.y + 1 + math.rndInt(room.h - 2);
            if (tileAt(tx, ty) === T_EMPTY) {
                setTile(tx, ty, T_TREASURE);
            }
        }

        // Key (less common)
        if (math.rnd() < 0.3) {
            const kx = room.x + 1 + math.rndInt(room.w - 2);
            const ky = room.y + 1 + math.rndInt(room.h - 2);
            if (tileAt(kx, ky) === T_EMPTY) {
                setTile(kx, ky, T_KEY);
            }
        }

        // Potion (rare)
        if (math.rnd() < 0.15) {
            const ppx = room.x + 1 + math.rndInt(room.w - 2);
            const ppy = room.y + 1 + math.rndInt(room.h - 2);
            if (tileAt(ppx, ppy) === T_EMPTY) {
                setTile(ppx, ppy, T_POTION);
            }
        }

        // Enemy spawner
        if (math.rnd() < 0.5 + level * 0.05) {
            if (tileAt(centerX, centerY) === T_EMPTY) {
                setTile(centerX, centerY, T_SPAWNER);
                spawners.push({
                    cx: centerX,
                    cy: centerY,
                    hp: 20 + level * 5,
                    alive: true,
                });
            }
        }
    }

    // Place doors at corridor/room transitions
    let doorCount = 0;
    const maxDoors = 2 + math.flr(level / 2);
    for (let ri = 1; ri < rooms.length && doorCount < maxDoors; ++ri) {
        const room = rooms[ri];
        // Check edges of room for corridor entry points
        for (let xx = room.x; xx < room.x + room.w && doorCount < maxDoors; ++xx) {
            if (tileAt(xx, room.y - 1) === T_EMPTY && math.rnd() < 0.15) {
                setTile(xx, room.y, T_DOOR);
                ++doorCount;
            }
        }
        for (let yy = room.y; yy < room.y + room.h && doorCount < maxDoors; ++yy) {
            if (tileAt(room.x - 1, yy) === T_EMPTY && math.rnd() < 0.15) {
                setTile(room.x, yy, T_DOOR);
                ++doorCount;
            }
        }
    }

    // Spawn initial enemies near spawners
    for (let si = 0; si < spawners.length; ++si) {
        const spn = spawners[si];
        for (let ei = 0; ei < 2 + math.flr(level / 2); ++ei) {
            spawnEnemyNear(spn.cx, spn.cy);
        }
    }
}

function spawnEnemyNear(cx, cy) {
    if (enemies.length >= MAX_ENEMIES) {
        return;
    }
    // Try random nearby tiles
    for (let att = 0; att < 20; ++att) {
        const ex = cx - 2 + math.rndInt(5);
        const ey = cy - 2 + math.rndInt(5);
        if (tileAt(ex, ey) === T_EMPTY) {
            const isDemon = math.rnd() < 0.3 + level * 0.05;
            enemies.push({
                x: ex * TILE,
                y: ey * TILE,
                w: 6,
                h: 6,
                hp: isDemon ? DEMON_HP : GHOST_HP,
                max_hp: isDemon ? DEMON_HP : GHOST_HP,
                type: isDemon ? 1 : 0, // 0=ghost, 1=demon
                speed: (isDemon ? 0.08 : 0.12) + level * 0.005,
                damage: isDemon ? 2 : 1,
                alive: true,
                hit_flash: 0,
            });
            return;
        }
    }
}

// --- Player init -----------------------------------------------------

function initPlayer() {
    player = {
        x: 0,
        y: 0,
        w: 6,
        h: 6,
        vx: 0,
        vy: 0,
        hp: CLASS_MAX_HP[playerClass],
        max_hp: CLASS_MAX_HP[playerClass],
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

function fireShot() {
    if (attackTimer > 0) {
        return;
    }
    if (player.dir_x === 0 && player.dir_y === 0) {
        return;
    }

    const len = math.sqrt(player.dir_x * player.dir_x + player.dir_y * player.dir_y);
    if (len === 0) {
        return;
    }
    const ndx = player.dir_x / len;
    const ndy = player.dir_y / len;

    shots.push({
        x: player.x + 2,
        y: player.y + 2,
        vx: ndx * SHOT_SPEED,
        vy: ndy * SHOT_SPEED,
        life: SHOT_LIFETIME,
        damage: CLASS_SHOT_DAMAGE[playerClass],
    });
    attackTimer = ATTACK_COOLDOWN;
}

function meleeAttack() {
    if (player.melee_timer > 0) {
        return;
    }
    player.melee_timer = 15;
    const mx = player.x + player.dir_x * 8;
    const my = player.y + player.dir_y * 8;
    const mw = 10;
    const mh = 10;
    const dmg = CLASS_MELEE_DAMAGE[playerClass];

    for (let idx = 0; idx < enemies.length; ++idx) {
        const ene = enemies[idx];
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
    for (let si = 0; si < spawners.length; ++si) {
        const spn = spawners[si];
        if (!spn.alive) {
            continue;
        }
        const spx = spn.cx * TILE;
        const spy = spn.cy * TILE;
        if (col.rect(mx - mw / 2, my - mh / 2, mw, mh, spx, spy, TILE, TILE)) {
            spn.hp -= dmg;
            if (spn.hp <= 0) {
                spn.alive = false;
                setTile(spn.cx, spn.cy, T_EMPTY);
                player.score += 100;
            }
        }
    }
}

// --- Callbacks -------------------------------------------------------

function _init() {
    state = 'select';
    selectCursor = 0;
}

function _update(dt) {
    flashTick += 1;

    if (state === 'select') {
        updateSelect();
    } else if (state === 'play') {
        updatePlay();
    } else if (state === 'dead') {
        updateDead();
    } else if (state === 'level_clear') {
        updateLevelClear();
    }
}

function _draw() {
    if (state === 'select') {
        drawSelect();
    } else if (state === 'play' || state === 'dead') {
        drawPlay();
    } else if (state === 'level_clear') {
        drawLevelClear();
    }
}

// --- Character select ------------------------------------------------

function updateSelect() {
    if (input.btnp('up')) {
        selectCursor = (selectCursor + 3) % 4;
    }
    if (input.btnp('down')) {
        selectCursor = (selectCursor + 1) % 4;
    }
    if (input.btnp('attack') || input.btnp('use')) {
        playerClass = selectCursor;
        initPlayer();
        level = 1;
        generateLevel();
        state = 'play';
    }
}

function drawSelect() {
    gfx.cls(0);
    const title = 'GAUNTLET';
    const tw = gfx.textWidth(title);
    gfx.print(title, (SCREEN_W - tw) / 2, 20, 10);

    const sub = 'Choose your hero';
    const sw = gfx.textWidth(sub);
    gfx.print(sub, (SCREEN_W - sw) / 2, 35, 7);

    for (let idx = 0; idx < 4; ++idx) {
        const yy = 55 + idx * 28;
        const clr = CLASS_COLORS[idx];
        const isSel = idx === selectCursor;

        // Selection arrow
        if (isSel) {
            gfx.print('>', 70, yy + 2, 10);
        }

        // Character preview box
        const bx = 85;
        gfx.rectfill(bx, yy, bx + 8, yy + 8, clr);
        // Eyes
        gfx.pset(bx + 2, yy + 3, 7);
        gfx.pset(bx + 5, yy + 3, 7);

        // Name and stats
        gfx.print(CLASS_NAMES[idx], 100, yy, isSel ? 7 : 5);
        gfx.print(
            `HP:${CLASS_MAX_HP[idx]} ATK:${CLASS_MELEE_DAMAGE[idx]} MAG:${
                CLASS_SHOT_DAMAGE[idx]
            } SPD:${math.flr(CLASS_SPEEDS[idx] * 10)}`,
            100,
            yy + 8,
            isSel ? 6 : 5,
        );
    }

    const hint = 'Press ATTACK to start';
    const hw = gfx.textWidth(hint);
    gfx.print(hint, (SCREEN_W - hw) / 2, 170, math.flr(flashTick / 30) % 2 === 0 ? 7 : 5);
}

// --- Main gameplay ---------------------------------------------------

function updatePlay() {
    // Attack cooldowns
    if (attackTimer > 0) {
        --attackTimer;
    }
    if (player.melee_timer > 0) {
        --player.melee_timer;
    }
    if (player.hit_flash > 0) {
        --player.hit_flash;
    }

    // Health drain (classic Gauntlet mechanic)
    ++drainTimer;
    if (drainTimer >= DRAIN_INTERVAL) {
        drainTimer = 0;
        if (player.hp > 0) {
            player.hp -= DRAIN_AMOUNT;
        }
    }

    // Potion timer
    if (potionActive) {
        --potionTimer;
        if (potionTimer <= 0) {
            potionActive = false;
        }
    }

    // Movement
    const spd = CLASS_SPEEDS[playerClass];
    let dx = 0;
    let dy = 0;
    if (input.btn('left')) {
        dx = -spd;
    }
    if (input.btn('right')) {
        dx = spd;
    }
    if (input.btn('up')) {
        dy = -spd;
    }
    if (input.btn('down')) {
        dy = spd;
    }

    // Normalize diagonal movement
    if (dx !== 0 && dy !== 0) {
        const dlen = math.sqrt(dx * dx + dy * dy);
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
        const nx = player.x + dx;
        if (!worldSolid(nx, player.y, player.w, player.h)) {
            player.x = nx;
        }
    }
    if (dy !== 0) {
        const ny = player.y + dy;
        if (!worldSolid(player.x, ny, player.w, player.h)) {
            player.y = ny;
        }
    }

    // Attack
    if (input.btnp('attack')) {
        if (playerClass === CLASS_WIZARD || playerClass === CLASS_ELF) {
            fireShot();
        } else {
            meleeAttack();
        }
    }

    // Use (secondary) — warriors/valkyries can shoot, wizards/elves can melee
    if (input.btnp('use')) {
        if (playerClass === CLASS_WIZARD || playerClass === CLASS_ELF) {
            meleeAttack();
        } else {
            fireShot();
        }
    }

    // Tile pickups
    const pcx0 = math.flr(player.x / TILE);
    const pcy0 = math.flr(player.y / TILE);
    const pcx1 = math.flr((player.x + player.w - 1) / TILE);
    const pcy1 = math.flr((player.y + player.h - 1) / TILE);

    for (let ty = pcy0; ty <= pcy1; ++ty) {
        for (let tx = pcx0; tx <= pcx1; ++tx) {
            const tile = tileAt(tx, ty);

            if (tile === T_FOOD) {
                setTile(tx, ty, T_EMPTY);
                player.hp = math.min(player.hp + 100, player.max_hp);
                player.score += 5;
            } else if (tile === T_KEY) {
                setTile(tx, ty, T_EMPTY);
                player.keys += 1;
                player.score += 10;
            } else if (tile === T_TREASURE) {
                setTile(tx, ty, T_EMPTY);
                player.score += 100;
            } else if (tile === T_POTION) {
                setTile(tx, ty, T_EMPTY);
                potionActive = true;
                potionTimer = POTION_DURATION;
                player.score += 50;
                // Kill all visible enemies (screen nuke)
                for (let ei = 0; ei < enemies.length; ++ei) {
                    if (enemies[ei].alive) {
                        const edx = enemies[ei].x - player.x;
                        const edy = enemies[ei].y - player.y;
                        if (math.abs(edx) < SCREEN_W / 2 && math.abs(edy) < SCREEN_H / 2) {
                            enemies[ei].alive = false;
                            player.score += 5;
                        }
                    }
                }
            } else if (tile === T_DOOR) {
                if (player.keys > 0) {
                    setTile(tx, ty, T_EMPTY);
                    player.keys -= 1;
                }
            } else if (tile === T_EXIT) {
                state = 'level_clear';
                transitionTimer = TRANSITION_FRAMES;
                player.score += 200;
                return;
            }
        }
    }

    // Update projectiles
    for (let si = shots.length - 1; si >= 0; --si) {
        const shot = shots[si];
        shot.x += shot.vx;
        shot.y += shot.vy;
        --shot.life;

        // Wall collision
        const scx = math.flr((shot.x + 1) / TILE);
        const scy = math.flr((shot.y + 1) / TILE);
        if (isSolid(scx, scy) || shot.life <= 0) {
            // Shots can destroy spawners
            for (let spi = 0; spi < spawners.length; ++spi) {
                const spn = spawners[spi];
                if (spn.alive && spn.cx === scx && spn.cy === scy) {
                    spn.hp -= shot.damage;
                    if (spn.hp <= 0) {
                        spn.alive = false;
                        setTile(spn.cx, spn.cy, T_EMPTY);
                        player.score += 100;
                    }
                }
            }
            shots.splice(si, 1);
            continue;
        }

        // Enemy collision
        let hit = false;
        for (let ei = 0; ei < enemies.length; ++ei) {
            const ene = enemies[ei];
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
    for (let ei = 0; ei < enemies.length; ++ei) {
        const ene = enemies[ei];
        if (!ene.alive) {
            continue;
        }
        if (ene.hit_flash > 0) {
            --ene.hit_flash;
        }

        // Simple chase AI
        const edx = player.x - ene.x;
        const edy = player.y - ene.y;
        const edist = math.sqrt(edx * edx + edy * edy);

        if (edist > 1 && edist < 200) {
            const enx = (edx / edist) * ene.speed;
            const eny = (edy / edist) * ene.speed;

            // Try X movement
            const nnx = ene.x + enx;
            if (!worldSolid(nnx, ene.y, ene.w, ene.h)) {
                ene.x = nnx;
            }
            // Try Y movement
            const nny = ene.y + eny;
            if (!worldSolid(ene.x, nny, ene.w, ene.h)) {
                ene.y = nny;
            }
        }

        // Damage player on contact
        if (
            player.alive &&
            col.rect(player.x, player.y, player.w, player.h, ene.x, ene.y, ene.w, ene.h)
        ) {
            if (!potionActive) {
                player.hp -= ene.damage;
                player.hit_flash = 6;
            } else {
                player.score += 5;
            }
            // Enemy dies on contact with player
            ene.alive = false;
        }
    }

    // Spawner enemy generation
    ++spawnTimer;
    if (spawnTimer >= SPAWN_INTERVAL) {
        spawnTimer = 0;
        for (let si = 0; si < spawners.length; ++si) {
            const spn = spawners[si];
            if (spn.alive) {
                spawnEnemyNear(spn.cx, spn.cy);
            }
        }
    }

    // Clean up dead enemies
    for (let ei = enemies.length - 1; ei >= 0; --ei) {
        if (!enemies[ei].alive) {
            enemies.splice(ei, 1);
        }
    }

    // Camera follow
    const targetX = player.x - SCREEN_W / 2 + player.w / 2;
    const targetY = player.y - (SCREEN_H - HUD_H) / 2 + player.h / 2;
    cam.follow(targetX, targetY, 0.1);

    // Clamp camera to map bounds
    const maxCx = MAP_W * TILE - SCREEN_W;
    const maxCy = MAP_H * TILE - (SCREEN_H - HUD_H);
    let c = cam.get();
    if (c.x < 0) cam.set(0, c.y);
    if (c.y < 0) cam.set(cam.get().x, 0);
    c = cam.get();
    if (c.x > maxCx) cam.set(maxCx, c.y);
    if (c.y > maxCy) cam.set(cam.get().x, maxCy);

    // Death check
    if (player.hp <= 0) {
        player.alive = false;
        state = 'dead';
    }
}

// --- Drawing ---------------------------------------------------------

function drawPlay() {
    gfx.cls(0);

    const c = cam.get();
    cam.set(c.x, c.y - HUD_H);

    // Visible tile range
    let sx = math.flr(c.x / TILE) - 1;
    let sy = math.flr(c.y / TILE) - 1;
    let ex = sx + math.flr(SCREEN_W / TILE) + 3;
    let ey = sy + math.flr(SCREEN_H / TILE) + 3;
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
    for (let ty = sy; ty < ey; ++ty) {
        for (let tx = sx; tx < ex; ++tx) {
            const tile = tileAt(tx, ty);
            const px = tx * TILE;
            const py = ty * TILE;

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
                const blink = math.flr(flashTick / 15) % 2;
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
                const kblink = math.flr(flashTick / 20) % 2;
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
                const pblink = math.flr(flashTick / 10) % 2;
                gfx.rectfill(px + 2, py + 3, px + 5, py + 6, pblink === 0 ? 12 : 13);
                gfx.rectfill(px + 3, py + 1, px + 4, py + 3, 6);
            } else if (tile === T_SPAWNER) {
                // Spawner (generator)
                gfx.rectfill(px, py, px + 7, py + 7, 1);
                const sblink = math.flr(flashTick / 8) % 2;
                gfx.rectfill(px + 1, py + 1, px + 6, py + 6, sblink === 0 ? 2 : 8);
                gfx.rect(px + 1, py + 1, px + 6, py + 6, 5);
            }
        }
    }

    // Draw enemies
    for (let ei = 0; ei < enemies.length; ++ei) {
        const ene = enemies[ei];
        if (!ene.alive) {
            continue;
        }
        let ecol;
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
    for (let si = 0; si < spawners.length; ++si) {
        const spn = spawners[si];
        if (!spn.alive) {
            continue;
        }
        const spx = spn.cx * TILE;
        const spy = spn.cy * TILE;
        const hpPct = spn.hp / (20 + level * 5);
        const barW = math.flr(6 * hpPct);
        if (barW > 0) {
            gfx.rectfill(spx + 1, spy - 2, spx + 1 + barW, spy - 1, 8);
        }
    }

    // Draw projectiles
    for (let si = 0; si < shots.length; ++si) {
        const shot = shots[si];
        let shotCol = 10;
        if (playerClass === CLASS_WIZARD) {
            shotCol = 13;
        } else if (playerClass === CLASS_ELF) {
            shotCol = 11;
        }
        gfx.circfill(shot.x + 1, shot.y + 1, 1, shotCol);
    }

    // Draw player
    if (player.alive) {
        let pcol = CLASS_COLORS[playerClass];
        if (player.hit_flash > 0) {
            pcol = 7;
        }
        if (potionActive && math.flr(flashTick / 4) % 2 === 0) {
            pcol = 7;
        }

        gfx.rectfill(player.x, player.y, player.x + player.w, player.y + player.h, pcol);
        // Eyes based on facing direction
        const eyeX1 = player.x + 1 + (player.dir_x > 0 ? 2 : 0);
        const eyeX2 = eyeX1 + 2;
        const eyeY = player.y + 2 + (player.dir_y > 0 ? 1 : 0);
        gfx.pset(eyeX1, eyeY, 7);
        gfx.pset(eyeX2, eyeY, 7);

        // Melee attack indicator
        if (player.melee_timer > 10) {
            const mx = player.x + player.dir_x * 8;
            const my = player.y + player.dir_y * 8;
            gfx.circ(mx + 3, my + 3, 4, 7);
        }
    }

    // --- HUD ---
    cam.set(0, 0);

    // HUD background
    gfx.rectfill(0, 0, SCREEN_W - 1, HUD_H - 1, 0);
    gfx.line(0, HUD_H - 1, SCREEN_W - 1, HUD_H - 1, 5);

    // Class name
    gfx.print(CLASS_NAMES[playerClass], 2, 2, CLASS_COLORS[playerClass]);

    // HP bar
    const hpBarX = 50;
    const hpBarW = 50;
    let hpPct = player.hp / player.max_hp;
    if (hpPct < 0) {
        hpPct = 0;
    }
    gfx.rectfill(hpBarX, 2, hpBarX + hpBarW, 8, 2);
    const fillW = math.flr(hpBarW * hpPct);
    if (fillW > 0) {
        const hpCol = hpPct > 0.5 ? 11 : hpPct > 0.25 ? 9 : 8;
        gfx.rectfill(hpBarX, 2, hpBarX + fillW, 8, hpCol);
    }
    gfx.rect(hpBarX, 2, hpBarX + hpBarW, 8, 5);
    gfx.print(player.hp, hpBarX + 2, 3, 7);

    // Keys
    gfx.print(`KEY:${player.keys}`, 110, 4, 10);

    // Score
    gfx.print(`SCORE:${player.score}`, 155, 4, 7);

    // Level
    gfx.print(`LV:${level}`, 230, 4, 6);

    // Potion indicator
    if (potionActive) {
        gfx.print('POTION!', 270, 4, 13);
    }

    // FPS widget
    sys.drawFps();

    // Death overlay
    if (state === 'dead') {
        drawDeathOverlay();
    }

    // Restore camera for next frame's cam.follow()
    cam.set(c.x, c.y);
}

function drawDeathOverlay() {
    gfx.rectfill(60, 50, 260, 130, 0);
    gfx.rect(60, 50, 260, 130, 8);

    const title = 'YOU HAVE DIED';
    const tw = gfx.textWidth(title);
    gfx.print(title, (SCREEN_W - tw) / 2, 60, 8);

    gfx.print(`Class: ${CLASS_NAMES[playerClass]}`, 80, 78, CLASS_COLORS[playerClass]);
    gfx.print(`Score: ${player.score}`, 80, 90, 7);
    gfx.print(`Level: ${level}`, 80, 100, 6);

    const hint = 'Press ATTACK to retry';
    const hw = gfx.textWidth(hint);
    gfx.print(hint, (SCREEN_W - hw) / 2, 118, math.flr(flashTick / 30) % 2 === 0 ? 7 : 5);
}

function updateDead() {
    if (input.btnp('attack')) {
        state = 'select';
    }
}

// --- Level transition ------------------------------------------------

function updateLevelClear() {
    --transitionTimer;
    if (transitionTimer <= 0) {
        ++level;
        generateLevel();
        state = 'play';
    }
}

function drawLevelClear() {
    gfx.cls(0);

    const pct = 1.0 - transitionTimer / TRANSITION_FRAMES;
    const msg = `LEVEL ${level} COMPLETE!`;
    const mw = gfx.textWidth(msg);
    gfx.print(msg, (SCREEN_W - mw) / 2, 70, 10);

    const msg2 = `Entering level ${level + 1}...`;
    const m2w = gfx.textWidth(msg2);
    gfx.print(msg2, (SCREEN_W - m2w) / 2, 90, 7);

    // Progress bar
    const barX = 100;
    const barW = 120;
    gfx.rect(barX, 110, barX + barW, 116, 5);
    const prog = math.flr(barW * pct);
    if (prog > 0) {
        gfx.rectfill(barX + 1, 111, barX + prog, 115, 11);
    }
}

// --- Hot reload state preservation -----------------------------------

function _save() {
    return {
        state,
        player,
        playerClass,
        tiles,
        enemies,
        spawners,
        shots,
        level,
        cam: cam.get(),
        attackTimer,
        drainTimer,
        potionActive,
        potionTimer,
        selectCursor,
        spawnTimer,
        flashTick,
    };
}

function _restore(sav) {
    state = sav.state;
    player = sav.player;
    playerClass = sav.playerClass;
    tiles = sav.tiles;
    enemies = sav.enemies;
    spawners = sav.spawners;
    shots = sav.shots;
    level = sav.level;
    cam.set(sav.cam.x, sav.cam.y);
    attackTimer = sav.attackTimer;
    drainTimer = sav.drainTimer;
    potionActive = sav.potionActive;
    potionTimer = sav.potionTimer;
    selectCursor = sav.selectCursor;
    spawnTimer = sav.spawnTimer;
    flashTick = sav.flashTick;
}
