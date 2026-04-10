// Draw List Demo — depth-sorted sprite rendering
//
// Demonstrates: gfx.dlBegin, gfx.dlEnd, gfx.dlSpr, gfx.dlSspr,
//   gfx.dlSprRot, gfx.dlSprAffine, gfx.sprAffine,
//   gfx.sheetCreate, gfx.sset

// --- FPS widget ----------------------------------------------------------
var smooth_fps = 60;
var fps_history = [];
var fps_hist_idx = 0;
var FPS_HIST_LEN = 50;
for (var _i = 0; _i < FPS_HIST_LEN; ++_i) fps_history.push(60);

function draw_fps_widget() {
    var wx = 320 - FPS_HIST_LEN - 4,
        wy = 0;
    var ww = FPS_HIST_LEN + 4,
        gh = 16;
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

// -------------------------------------------------------------------------
// Spritesheet: 4 simple 8x8 sprites (tree, rock, player, shadow)
//   tile 0 = tree (green triangle)
//   tile 1 = rock (grey circle)
//   tile 2 = player (red square)
//   tile 3 = shadow (dark oval)
// -------------------------------------------------------------------------

function paint_tile(tx, ty, pattern) {
    for (var py = 0; py < 8; ++py) {
        for (var px = 0; px < 8; ++px) {
            var col = pattern[py * 8 + px];
            if (col > 0) gfx.sset(tx * 8 + px, ty * 8 + py, col);
        }
    }
}

function build_sheet() {
    gfx.sheetCreate(128, 128, 8, 8);

    // tile 0: tree (green)
    var tree = [
        0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 3, 11, 11, 3, 0, 0, 0, 3, 11, 11, 11, 11, 0, 0, 3, 11, 11, 11,
        11, 11, 3, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0,
        0, 4, 4, 0, 0, 0,
    ];
    paint_tile(0, 0, tree);

    // tile 1: rock (grey)
    var rock = [
        0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 5, 6, 6, 5, 0, 0, 0, 5, 6, 6, 6, 6, 5, 0, 5, 6, 6, 7, 6, 6, 6,
        5, 5, 6, 6, 6, 6, 6, 6, 5, 0, 5, 6, 6, 6, 6, 5, 0, 0, 0, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0,
    ];
    paint_tile(1, 0, rock);

    // tile 2: player (red)
    var player = [
        0, 0, 8, 8, 8, 8, 0, 0, 0, 8, 8, 7, 7, 8, 8, 0, 0, 8, 8, 7, 7, 8, 8, 0, 0, 0, 8, 8, 8, 8, 0,
        0, 0, 0, 2, 8, 8, 2, 0, 0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 2,
        0, 0,
    ];
    paint_tile(2, 0, player);

    // tile 3: shadow (dark ellipse)
    var shadow = [
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1,
        0, 0,
    ];
    paint_tile(3, 0, shadow);
}

// -------------------------------------------------------------------------
// Objects in the scene
// -------------------------------------------------------------------------

var objects = [];
var player = { x: 160, y: 100 };
var SPEED = 60;
var timer = 0;

function _init() {
    build_sheet();

    // Place trees and rocks
    math.seed(42);
    for (var i = 0; i < 20; ++i) {
        objects.push({
            type: math.rnd(1) > 0.5 ? 0 : 1, // tree or rock
            x: math.rndInt(300) + 10,
            y: math.rndInt(140) + 20,
            rot: 0,
        });
    }
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    timer += dt;

    // Player movement
    if (input.btn("left")) player.x -= SPEED * dt;
    if (input.btn("right")) player.x += SPEED * dt;
    if (input.btn("up")) player.y -= SPEED * dt;
    if (input.btn("down")) player.y += SPEED * dt;
    player.x = math.clamp(player.x, 4, 312);
    player.y = math.clamp(player.y, 10, 172);
}

function _draw() {
    gfx.cls(3); // dark green ground

    // Checkerboard ground pattern
    for (var gy = 0; gy < 180; gy += 16) {
        for (var gx = 0; gx < 320; gx += 16) {
            if ((gx + gy) % 32 === 0) {
                gfx.rectfill(gx, gy, gx + 15, gy + 15, 3);
            }
        }
    }

    // --- Draw list: all objects + player sorted by Y (depth) ---
    gfx.dlBegin();

    // Draw shadows first at a low layer
    for (var i = 0; i < objects.length; ++i) {
        var o = objects[i];
        gfx.dlSpr(o.y - 1, 3, o.x, o.y + 4);
    }
    // Player shadow
    gfx.dlSpr(player.y - 1, 3, player.x, player.y + 4);

    // Objects
    for (var j = 0; j < objects.length; ++j) {
        var ob = objects[j];
        if (ob.type === 0) {
            // Trees use dlSpr
            gfx.dlSpr(ob.y, ob.type, ob.x, ob.y);
        } else {
            // Rocks use dlSprRot with a gentle wobble
            var wobble = math.sin(timer * 0.3 + j * 0.7) * 0.05;
            gfx.dlSprRot(ob.y, ob.type, ob.x, ob.y, wobble);
        }
    }

    // Player via dlSpr
    gfx.dlSpr(player.y, 2, player.x, player.y);

    // A couple of stretch-blitted decorations via dlSspr
    gfx.dlSspr(200, 0, 0, 8, 8, 280, 10, 16, 16);

    // An affine-transformed sprite spinning in the corner
    var angle = timer * 0.4;
    var cos_a = Math.cos(angle);
    var sin_a = Math.sin(angle);
    gfx.dlSprAffine(200, 0, 20, 20, 4, 4, cos_a, sin_a);

    gfx.dlEnd();

    // Also demonstrate standalone sprAffine (non-draw-list) for label
    var sa = timer * 0.2;
    var sc = Math.cos(sa);
    var ss = Math.sin(sa);
    gfx.sprAffine(1, 290, 160, 4, 4, sc * 1.5, ss * 1.5);

    // HUD
    gfx.rectfill(0, 0, 180, 9, 0);
    gfx.print("DRAW LIST DEMO  objects:" + objects.length, 2, 1, 7);
    gfx.print("Arrow keys to move player", 2, 172, 5);

    draw_fps_widget();
}

function _save() {
    return {
        player: player,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    player = s.player;
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
}
