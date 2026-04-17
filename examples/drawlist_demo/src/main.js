// Draw List Demo — top-down arena shooter with depth-sorted rendering
//
// Demonstrates: gfx.dlBegin, gfx.dlEnd, gfx.dlSpr, gfx.dlSspr,
//   gfx.dlSprRot, gfx.dlSprAffine, gfx.sprAffine,
//   gfx.sheetCreate, gfx.sset, synth.set, synth.play, col.circ

// --- FPS widget ----------------------------------------------------------

// -------------------------------------------------------------------------
// Spritesheet: 8x8 tiles
//   0 = tree        1 = rock       2 = player
//   3 = shadow      4 = enemy      5 = bullet
//   6 = explosion   7 = pickup
// -------------------------------------------------------------------------

function paintTile(tx, ty, pattern) {
    for (let py = 0; py < 8; ++py) {
        for (let px = 0; px < 8; ++px) {
            const col = pattern[py * 8 + px];
            if (col > 0) gfx.sset(tx * 8 + px, ty * 8 + py, col);
        }
    }
}

function buildSheet() {
    gfx.sheetCreate(128, 128, 8, 8);

    // tile 0: tree (green)
    // prettier-ignore
    paintTile(0, 0, [
        0, 0, 0, 3, 3, 0, 0, 0,
        0, 0, 3,11,11, 3, 0, 0,
        0, 3,11,11,11,11, 0, 0,
        3,11,11,11,11,11, 3, 0,
        0, 0, 0, 4, 4, 0, 0, 0,
        0, 0, 0, 4, 4, 0, 0, 0,
        0, 0, 0, 4, 4, 0, 0, 0,
        0, 0, 0, 4, 4, 0, 0, 0,
    ]);

    // tile 1: rock (grey)
    // prettier-ignore
    paintTile(1, 0, [
        0, 0, 0, 5, 5, 0, 0, 0,
        0, 0, 5, 6, 6, 5, 0, 0,
        0, 5, 6, 6, 6, 6, 5, 0,
        5, 6, 6, 7, 6, 6, 6, 5,
        5, 6, 6, 6, 6, 6, 6, 5,
        0, 5, 6, 6, 6, 6, 5, 0,
        0, 0, 5, 5, 5, 5, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    ]);

    // tile 2: player (blue guy)
    // prettier-ignore
    paintTile(2, 0, [
        0, 0,12,12,12,12, 0, 0,
        0,12,12, 7, 7,12,12, 0,
        0,12,12, 7, 7,12,12, 0,
        0, 0,12,12,12,12, 0, 0,
        0, 0, 1,12,12, 1, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 0, 1, 0, 0, 1, 0, 0,
        0, 0, 1, 0, 0, 1, 0, 0,
    ]);

    // tile 3: shadow (dark ellipse)
    // prettier-ignore
    paintTile(3, 0, [
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    ]);

    // tile 4: enemy (red skull)
    // prettier-ignore
    paintTile(4, 0, [
        0, 0, 8, 8, 8, 8, 0, 0,
        0, 8, 2, 8, 8, 2, 8, 0,
        0, 8, 0, 8, 8, 0, 8, 0,
        0, 0, 8, 8, 8, 8, 0, 0,
        0, 0, 8, 0, 0, 8, 0, 0,
        0, 0, 2, 8, 8, 2, 0, 0,
        0, 0, 2, 0, 0, 2, 0, 0,
        0, 0, 2, 0, 0, 2, 0, 0,
    ]);

    // tile 5: bullet (yellow dot)
    // prettier-ignore
    paintTile(5, 0, [
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0,10,10, 0, 0, 0,
        0, 0,10, 7, 7,10, 0, 0,
        0, 0,10, 7, 7,10, 0, 0,
        0, 0, 0,10,10, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    ]);

    // tile 6: explosion (orange burst)
    // prettier-ignore
    paintTile(6, 0, [
        0, 0, 9, 0, 0, 9, 0, 0,
        0, 9,10, 9, 9,10, 9, 0,
        9,10,10, 7, 7,10,10, 9,
        0, 9, 7, 7, 7, 7, 9, 0,
        0, 9, 7, 7, 7, 7, 9, 0,
        9,10,10, 7, 7,10,10, 9,
        0, 9,10, 9, 9,10, 9, 0,
        0, 0, 9, 0, 0, 9, 0, 0,
    ]);

    // tile 7: pickup star (yellow)
    // prettier-ignore
    paintTile(7, 0, [
        0, 0, 0,10, 0, 0, 0, 0,
        0, 0,10,10,10, 0, 0, 0,
        0, 0, 0,10, 0, 0, 0, 0,
        10,10,10,10,10,10,10, 0,
        0, 0,10,10,10, 0, 0, 0,
        0,10,10, 0,10,10, 0, 0,
        0,10, 0, 0, 0,10, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    ]);
}

// -------------------------------------------------------------------------
// Sound effects
// -------------------------------------------------------------------------

function buildSounds() {
    // 0 = shoot
    synth.set(0, 'square', 'C5', 0.06, 'none');
    // 1 = enemy hit
    synth.set(1, 'noise', 'C3', 0.15, 'none');
    // 2 = pickup
    synth.set(2, 'sine', 'E5', 0.12, 'none');
    // 3 = player hit
    synth.set(3, 'noise', 'C2', 0.3, 'none');
}

// -------------------------------------------------------------------------
// Game constants
// -------------------------------------------------------------------------

const SPEED = 70;
const BULLET_SPEED = 200;
const ENEMY_SPEED = 30;
const SHOOT_COOLDOWN = 0.15;
const SPAWN_INTERVAL = 2.0;
const MAX_ENEMIES = 20;
const PICKUP_INTERVAL = 8.0;
const HIT_FLASH_TIME = 0.3;

// -------------------------------------------------------------------------
// Game state
// -------------------------------------------------------------------------

let player = { x: 160, y: 90, dirX: 0, dirY: 1, hp: 5, maxHp: 5 };
let score = 0;
let combo = 0;
let comboTimer = 0;
let timer = 0;
let shootCooldown = 0;
let spawnTimer = 0;
let pickupTimer = 0;
let hitFlash = 0;
let gameOver = false;
let shakeTimer = 0;
let wave = 1;

const bullets = [];
const enemies = [];
const trees = [];
const pickups = [];
const explosions = [];

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

function spawnEnemy() {
    if (enemies.length >= MAX_ENEMIES) return;
    // Spawn from edges
    let x, y;
    const side = math.rndInt(4);
    if (side === 0) {
        x = -8;
        y = math.rndInt(180);
    } else if (side === 1) {
        x = 328;
        y = math.rndInt(180);
    } else if (side === 2) {
        x = math.rndInt(320);
        y = -8;
    } else {
        x = math.rndInt(320);
        y = 188;
    }

    // Faster enemies in later waves
    const speedMul = 1.0 + (wave - 1) * 0.15;
    enemies.push({ x, y, hp: 1 + math.flr(wave / 4), speedMul });
}

function spawnPickup() {
    pickups.push({
        x: math.rndInt(280) + 20,
        y: math.rndInt(140) + 20,
        age: 0,
    });
}

function addExplosion(x, y) {
    explosions.push({ x, y, age: 0 });
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

function _init() {
    buildSheet();
    buildSounds();

    // Scatter trees and rocks for scenery
    math.seed(42);
    for (let i = 0; i < 14; ++i) {
        trees.push({
            type: math.rnd(1) > 0.5 ? 0 : 1,
            x: math.rndInt(300) + 10,
            y: math.rndInt(140) + 20,
        });
    }

    // Initial enemies
    for (let i = 0; i < 3; ++i) spawnEnemy();
}

function _update(dt) {
    if (gameOver) {
        if (input.btnp('action')) {
            // Restart
            score = 0;
            combo = 0;
            comboTimer = 0;
            wave = 1;
            hitFlash = 0;
            shakeTimer = 0;
            player.x = 160;
            player.y = 90;
            player.hp = player.maxHp;
            player.dirX = 0;
            player.dirY = 1;
            bullets.length = 0;
            enemies.length = 0;
            pickups.length = 0;
            explosions.length = 0;
            gameOver = false;
            spawnTimer = 0;
            shootCooldown = 0;
            for (let i = 0; i < 3; ++i) spawnEnemy();
        }
        return;
    }

    timer += dt;

    // --- Player movement ---
    let dx = 0;
    let dy = 0;
    if (input.btn('left')) dx -= 1;
    if (input.btn('right')) dx += 1;
    if (input.btn('up')) dy -= 1;
    if (input.btn('down')) dy += 1;

    if (dx !== 0 || dy !== 0) {
        const len = Math.sqrt(dx * dx + dy * dy);
        dx /= len;
        dy /= len;
        player.dirX = dx;
        player.dirY = dy;
        player.x += dx * SPEED * dt;
        player.y += dy * SPEED * dt;
    }
    player.x = math.clamp(player.x, 4, 312);
    player.y = math.clamp(player.y, 14, 168);

    // --- Shooting ---
    shootCooldown -= dt;
    if (input.btn('action') && shootCooldown <= 0) {
        shootCooldown = SHOOT_COOLDOWN;
        bullets.push({
            x: player.x + 2,
            y: player.y + 2,
            dx: player.dirX,
            dy: player.dirY,
        });
        synth.play(0);
    }

    // --- Update bullets ---
    for (let i = bullets.length - 1; i >= 0; --i) {
        const b = bullets[i];
        b.x += b.dx * BULLET_SPEED * dt;
        b.y += b.dy * BULLET_SPEED * dt;
        if (b.x < -8 || b.x > 328 || b.y < -8 || b.y > 188) {
            bullets.splice(i, 1);
            continue;
        }

        // Bullet-enemy collision
        let hit = false;
        for (let e = enemies.length - 1; e >= 0; --e) {
            if (col.circ(b.x + 3, b.y + 3, 2, enemies[e].x + 4, enemies[e].y + 4, 4)) {
                enemies[e].hp--;
                if (enemies[e].hp <= 0) {
                    addExplosion(enemies[e].x, enemies[e].y);
                    enemies.splice(e, 1);
                    score += 10 * (combo + 1);
                    combo++;
                    comboTimer = 2.0;
                    synth.play(1);
                    shakeTimer = 0.08;
                }
                hit = true;
                break;
            }
        }
        if (hit) {
            bullets.splice(i, 1);
        }
    }

    // --- Update enemies ---
    for (let e = 0; e < enemies.length; ++e) {
        const en = enemies[e];
        const edx = player.x - en.x;
        const edy = player.y - en.y;
        const elen = Math.sqrt(edx * edx + edy * edy);
        if (elen > 1) {
            en.x += (edx / elen) * ENEMY_SPEED * en.speedMul * dt;
            en.y += (edy / elen) * ENEMY_SPEED * en.speedMul * dt;
        }

        // Enemy-player collision
        if (col.circ(player.x + 4, player.y + 4, 3, en.x + 4, en.y + 4, 3)) {
            player.hp--;
            hitFlash = HIT_FLASH_TIME;
            shakeTimer = 0.15;
            addExplosion(en.x, en.y);
            enemies.splice(e, 1);
            e--;
            synth.play(3);
            if (player.hp <= 0) {
                gameOver = true;
                return;
            }
        }
    }

    // --- Combo timer ---
    if (combo > 0) {
        comboTimer -= dt;
        if (comboTimer <= 0) {
            combo = 0;
        }
    }

    // --- Pickups ---
    pickupTimer -= dt;
    if (pickupTimer <= 0 && pickups.length < 2) {
        spawnPickup();
        pickupTimer = PICKUP_INTERVAL;
    }
    for (let p = pickups.length - 1; p >= 0; --p) {
        pickups[p].age += dt;
        if (col.circ(player.x + 4, player.y + 4, 4, pickups[p].x + 4, pickups[p].y + 4, 4)) {
            player.hp = math.min(player.hp + 1, player.maxHp);
            pickups.splice(p, 1);
            synth.play(2);
        }
    }

    // --- Hit flash ---
    if (hitFlash > 0) hitFlash -= dt;

    // --- Screen shake ---
    if (shakeTimer > 0) shakeTimer -= dt;

    // --- Explosions ---
    for (let x = explosions.length - 1; x >= 0; --x) {
        explosions[x].age += dt;
        if (explosions[x].age > 0.3) explosions.splice(x, 1);
    }

    // --- Enemy spawning / waves ---
    spawnTimer -= dt;
    if (spawnTimer <= 0) {
        spawnTimer = math.max(SPAWN_INTERVAL - wave * 0.1, 0.5);
        spawnEnemy();
        if (enemies.length === 0) {
            wave++;
        }
    }
    // Wave advances every ~30 seconds
    if (math.flr(timer / 30) + 1 > wave) {
        wave = math.flr(timer / 30) + 1;
    }
}

function _draw() {
    gfx.cls(3);

    // Screen shake offset
    let sx = 0;
    let sy = 0;
    if (shakeTimer > 0) {
        sx = math.rndInt(5) - 2;
        sy = math.rndInt(5) - 2;
    }
    gfx.camera(-sx, -sy);

    // Checkerboard ground
    for (let gy = 0; gy < 180; gy += 16) {
        for (let gx = 0; gx < 320; gx += 16) {
            if ((gx + gy) % 32 === 0) {
                gfx.rectfill(gx, gy, gx + 15, gy + 15, 3);
            }
        }
    }

    // ---- Draw list: everything depth-sorted by Y ----
    gfx.dlBegin();

    // Shadows (layer = y - 1 so they render behind feet)
    for (let i = 0; i < trees.length; ++i) {
        gfx.dlSpr(trees[i].y - 1, 3, trees[i].x, trees[i].y + 4);
    }
    for (let i = 0; i < enemies.length; ++i) {
        gfx.dlSpr(enemies[i].y - 1, 3, enemies[i].x, enemies[i].y + 4);
    }
    gfx.dlSpr(player.y - 1, 3, player.x, player.y + 4);

    // Trees and rocks (dlSpr for static, dlSprRot for rocks with wobble)
    for (let i = 0; i < trees.length; ++i) {
        const t = trees[i];
        if (t.type === 0) {
            gfx.dlSpr(t.y, t.type, t.x, t.y);
        } else {
            const wobble = math.sin(timer * 0.3 + i * 0.7) * 0.05;
            gfx.dlSprRot(t.y, t.type, t.x, t.y, wobble);
        }
    }

    // Enemies (dlSprRot — they wobble as they chase)
    for (let i = 0; i < enemies.length; ++i) {
        const en = enemies[i];
        const wobble = math.sin(timer * 6 + i * 1.3) * 0.15;
        gfx.dlSprRot(en.y, 4, en.x, en.y, wobble);
    }

    // Bullets (dlSpr)
    for (let i = 0; i < bullets.length; ++i) {
        gfx.dlSpr(bullets[i].y, 5, bullets[i].x, bullets[i].y);
    }

    // Pickups (dlSprRot — gentle spin)
    for (let i = 0; i < pickups.length; ++i) {
        const p = pickups[i];
        gfx.dlSprRot(p.y, 7, p.x, p.y, math.sin(timer * 3) * 0.2);
    }

    // Player (dlSpr — flashes when hit)
    if (hitFlash <= 0 || math.flr(hitFlash * 20) % 2 === 0) {
        gfx.dlSpr(player.y, 2, player.x, player.y);
    }

    // Explosions (dlSprAffine — scale up as they age)
    for (let i = 0; i < explosions.length; ++i) {
        const ex = explosions[i];
        const t = ex.age / 0.3;
        const scale = 1.0 + t * 1.5;
        const angle = t * 1.5;
        const cs = Math.cos(angle) * scale;
        const sn = Math.sin(angle) * scale;
        gfx.dlSprAffine(ex.y, 6, ex.x, ex.y, 4, 4, cs, sn);
    }

    // Decorative scaled sprite in corner (dlSspr)
    gfx.dlSspr(200, 0, 0, 8, 8, 296, 164, 16, 16);

    gfx.dlEnd();

    // Non-draw-list sprAffine: spinning pickup indicator near HUD
    if (pickups.length > 0) {
        const sa = timer * 2.5;
        gfx.sprAffine(7, 300, 2, 4, 4, Math.cos(sa), Math.sin(sa));
    }

    // Reset camera for HUD
    gfx.camera(0, 0);

    // --- HUD ---
    gfx.rectfill(0, 0, 319, 11, 0);

    // Health bar
    for (let h = 0; h < player.maxHp; ++h) {
        const hx = 2 + h * 7;
        gfx.rectfill(hx, 2, hx + 5, 8, h < player.hp ? 8 : 5);
    }

    // Score + combo
    const comboStr = combo > 1 ? `  x${combo}` : '';
    gfx.print(`SCORE:${score}${comboStr}`, 42, 2, 7);

    // Wave indicator
    gfx.print(`W${wave}`, 148, 2, 6);

    // Controls hint
    gfx.print('ARROWS=move  Z/X=shoot', 2, 172, 5);

    sys.drawFps();

    // Game over overlay
    if (gameOver) {
        gfx.rectfill(80, 70, 240, 110, 0);
        gfx.rect(80, 70, 240, 110, 8);
        gfx.print('GAME OVER', 130, 78, 8);
        gfx.print(`SCORE: ${score}`, 130, 90, 7);
        gfx.print('PRESS Z TO RESTART', 108, 100, 6);
    }
}

function _save() {
    return { player, score, combo, wave, timer };
}

function _restore(s) {
    player = s.player;
    score = s.score;
    combo = s.combo;
    wave = s.wave;
    timer = s.timer;
}
