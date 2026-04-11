// Draw List Demo — depth-sorted sprite rendering
//
// Demonstrates: gfx.dlBegin, gfx.dlEnd, gfx.dlSpr, gfx.dlSspr,
//   gfx.dlSprRot, gfx.dlSprAffine, gfx.sprAffine,
//   gfx.sheetCreate, gfx.sset

// --- FPS widget ----------------------------------------------------------


// -------------------------------------------------------------------------
// Spritesheet: 4 simple 8x8 sprites (tree, rock, player, shadow)
//   tile 0 = tree (green triangle)
//   tile 1 = rock (grey circle)
//   tile 2 = player (red square)
//   tile 3 = shadow (dark oval)
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
    const tree = [
        0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 3, 11, 11, 3, 0, 0, 0, 3, 11, 11, 11, 11, 0, 0, 3, 11, 11, 11,
        11, 11, 3, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0,
        0, 4, 4, 0, 0, 0,
    ];
    paintTile(0, 0, tree);

    // tile 1: rock (grey)
    const rock = [
        0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 5, 6, 6, 5, 0, 0, 0, 5, 6, 6, 6, 6, 5, 0, 5, 6, 6, 7, 6, 6, 6,
        5, 5, 6, 6, 6, 6, 6, 6, 5, 0, 5, 6, 6, 6, 6, 5, 0, 0, 0, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0,
    ];
    paintTile(1, 0, rock);

    // tile 2: player (red)
    const player = [
        0, 0, 8, 8, 8, 8, 0, 0, 0, 8, 8, 7, 7, 8, 8, 0, 0, 8, 8, 7, 7, 8, 8, 0, 0, 0, 8, 8, 8, 8, 0,
        0, 0, 0, 2, 8, 8, 2, 0, 0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 2,
        0, 0,
    ];
    paintTile(2, 0, player);

    // tile 3: shadow (dark ellipse)
    const shadow = [
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1,
        0, 0,
    ];
    paintTile(3, 0, shadow);
}

// -------------------------------------------------------------------------
// Objects in the scene
// -------------------------------------------------------------------------

const objects = [];
let player = { x: 160, y: 100 };
const SPEED = 60;
let timer = 0;

function _init() {
    buildSheet();

    // Place trees and rocks
    math.seed(42);
    for (let i = 0; i < 20; ++i) {
        objects.push({
            type: math.rnd(1) > 0.5 ? 0 : 1, // tree or rock
            x: math.rndInt(300) + 10,
            y: math.rndInt(140) + 20,
            rot: 0,
        });
    }
}

function _update(dt) {

    timer += dt;

    // Player movement
    if (input.btn('left')) player.x -= SPEED * dt;
    if (input.btn('right')) player.x += SPEED * dt;
    if (input.btn('up')) player.y -= SPEED * dt;
    if (input.btn('down')) player.y += SPEED * dt;
    player.x = math.clamp(player.x, 4, 312);
    player.y = math.clamp(player.y, 10, 172);
}

function _draw() {
    gfx.cls(3); // dark green ground

    // Checkerboard ground pattern
    for (let gy = 0; gy < 180; gy += 16) {
        for (let gx = 0; gx < 320; gx += 16) {
            if ((gx + gy) % 32 === 0) {
                gfx.rectfill(gx, gy, gx + 15, gy + 15, 3);
            }
        }
    }

    // --- Draw list: all objects + player sorted by Y (depth) ---
    gfx.dlBegin();

    // Draw shadows first at a low layer
    for (let i = 0; i < objects.length; ++i) {
        const o = objects[i];
        gfx.dlSpr(o.y - 1, 3, o.x, o.y + 4);
    }
    // Player shadow
    gfx.dlSpr(player.y - 1, 3, player.x, player.y + 4);

    // Objects
    for (let j = 0; j < objects.length; ++j) {
        const ob = objects[j];
        if (ob.type === 0) {
            // Trees use dlSpr
            gfx.dlSpr(ob.y, ob.type, ob.x, ob.y);
        } else {
            // Rocks use dlSprRot with a gentle wobble
            const wobble = math.sin(timer * 0.3 + j * 0.7) * 0.05;
            gfx.dlSprRot(ob.y, ob.type, ob.x, ob.y, wobble);
        }
    }

    // Player via dlSpr
    gfx.dlSpr(player.y, 2, player.x, player.y);

    // A couple of stretch-blitted decorations via dlSspr
    gfx.dlSspr(200, 0, 0, 8, 8, 280, 10, 16, 16);

    // An affine-transformed sprite spinning in the corner
    const angle = timer * 0.4;
    const cosA = Math.cos(angle);
    const sinA = Math.sin(angle);
    gfx.dlSprAffine(200, 0, 20, 20, 4, 4, cosA, sinA);

    gfx.dlEnd();

    // Also demonstrate standalone sprAffine (non-draw-list) for label
    const sa = timer * 0.2;
    const sc = Math.cos(sa);
    const ss = Math.sin(sa);
    gfx.sprAffine(1, 290, 160, 4, 4, sc * 1.5, ss * 1.5);

    // HUD
    gfx.rectfill(0, 0, 180, 9, 0);
    gfx.print(`DRAW LIST DEMO  objects:${objects.length}`, 2, 1, 7);
    gfx.print('Arrow keys to move player', 2, 172, 5);

    sys.drawFps();
}

function _save() {
    return {
        player,
    };
}

function _restore(s) {
    player = s.player;
}
