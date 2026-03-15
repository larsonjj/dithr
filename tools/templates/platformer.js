// Platformer template — simple gravity + tile collision

var px = 64;
var py = 64;
var vx = 0;
var vy = 0;
var grounded = false;

var GRAVITY = 0.4;
var JUMP_VEL = -5.0;
var MOVE_SPD = 2.0;
var FRICTION = 0.8;

function _init() {
    gfx.cls(0);
}

function _update(dt) {
    // Horizontal movement
    if (key.btn(key.LEFT)) vx -= MOVE_SPD;
    if (key.btn(key.RIGHT)) vx += MOVE_SPD;
    vx *= FRICTION;

    // Jump
    if (grounded && key.btnp(key.Z)) {
        vy = JUMP_VEL;
        grounded = false;
    }

    // Gravity
    vy += GRAVITY;

    // Simple floor collision at y = 140
    px += vx;
    py += vy;
    if (py >= 140) {
        py = 140;
        vy = 0;
        grounded = true;
    }
}

function _draw() {
    gfx.cls(1);

    // Draw ground
    gfx.rectfill(0, 148, gfx.screenW ? gfx.screenW : 320, 180, 3);

    // Draw player
    gfx.rectfill(math.flr(px), math.flr(py), math.flr(px) + 8, math.flr(py) + 8, 11);

    gfx.print("arrows: move  z: jump", 4, 4, 7);
}
