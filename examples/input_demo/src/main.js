// Input Demo
//
// Shows how to read from all input sources:
//   - key.btn / key.btnp  — raw keyboard
//   - mouse.x/y, mouse.btn — mouse position and buttons
//   - pad.btn, pad.axis   — gamepad buttons and sticks
//   - input.btn / input.btnp — virtual action mappings
//
// Move the box with arrow keys / WASD / gamepad / virtual actions.
// Press SPACE/Z to flash.  Mouse cursor draws a crosshair.

// --- State -----------------------------------------------------------

var px = 160;
var py = 90;
var speed = 80; // pixels per second
var flash = 0;
var log_lines = []; // recent input events

function prv_log(msg) {
    log_lines.push(msg);
    if (log_lines.length > 6) {
        log_lines.shift();
    }
}

// --- Callbacks -------------------------------------------------------

function _init() {
    mouse.show();
}

function _update(dt) {
    // Virtual action movement
    if (input.btn("left")) px -= speed * dt;
    if (input.btn("right")) px += speed * dt;
    if (input.btn("up")) py -= speed * dt;
    if (input.btn("down")) py += speed * dt;

    // Jump / action flash
    if (input.btnp("jump")) {
        flash = 10;
        prv_log("jump pressed");
    }
    if (input.btnp("action")) {
        flash = 10;
        prv_log("action pressed");
    }

    // Clamp to screen
    if (px < 4) px = 4;
    if (px > 312) px = 312;
    if (py < 4) py = 4;
    if (py > 172) py = 172;

    if (flash > 0) --flash;

    // Log raw key presses
    if (key.btnp(key.ENTER)) prv_log("key: ENTER");
    if (key.btnp(key.ESCAPE)) prv_log("key: ESCAPE");

    // Log mouse clicks
    if (mouse.btnp(mouse.LEFT)) prv_log("mouse: LEFT click");
    if (mouse.btnp(mouse.RIGHT)) prv_log("mouse: RIGHT click");

    // Log gamepad presses
    if (pad.btnp(pad.A, 0)) prv_log("pad0: A");
    if (pad.btnp(pad.B, 0)) prv_log("pad0: B");
}

function _draw() {
    gfx.cls(1);

    // --- Player box ---
    var col = flash > 0 ? 10 : 7;
    gfx.rectfill(px - 4, py - 4, px + 4, py + 4, col);

    // --- Key state display ---
    gfx.print("-- keyboard --", 4, 4, 6);
    var arrows = "";
    if (key.btn(key.LEFT)) arrows += "L ";
    if (key.btn(key.RIGHT)) arrows += "R ";
    if (key.btn(key.UP)) arrows += "U ";
    if (key.btn(key.DOWN)) arrows += "D ";
    if (key.btn(key.Z)) arrows += "Z ";
    if (key.btn(key.X)) arrows += "X ";
    if (key.btn(key.SPACE)) arrows += "SPC ";
    gfx.print(arrows.length > 0 ? arrows : "(none held)", 4, 12, 7);

    // --- Mouse ---
    gfx.print("-- mouse --", 4, 24, 6);
    gfx.print("pos: " + mouse.x() + "," + mouse.y(), 4, 32, 7);
    var btns = "";
    if (mouse.btn(mouse.LEFT)) btns += "L ";
    if (mouse.btn(mouse.MIDDLE)) btns += "M ";
    if (mouse.btn(mouse.RIGHT)) btns += "R ";
    gfx.print("btn: " + (btns.length > 0 ? btns : "-"), 4, 40, 7);
    gfx.print("wheel: " + mouse.wheel(), 4, 48, 7);

    // Crosshair
    var mx = mouse.x();
    var my = mouse.y();
    gfx.line(mx - 4, my, mx + 4, my, 8);
    gfx.line(mx, my - 4, mx, my + 4, 8);

    // --- Gamepad ---
    gfx.print("-- gamepad 0 --", 4, 60, 6);
    if (pad.connected(0)) {
        gfx.print("name: " + pad.name(0), 4, 68, 7);
        var lx = math.flr(pad.axis(pad.LX, 0) * 100) / 100;
        var ly = math.flr(pad.axis(pad.LY, 0) * 100) / 100;
        gfx.print("L stick: " + lx + "," + ly, 4, 76, 7);

        // Visual stick indicator
        var stick_cx = 40;
        var stick_cy = 96;
        gfx.circ(stick_cx, stick_cy, 10, 5);
        gfx.circfill(stick_cx + lx * 10, stick_cy + ly * 10, 2, 11);
    } else {
        gfx.print("(not connected)", 4, 68, 5);
    }

    // --- Virtual actions ---
    gfx.print("-- input actions --", 160, 4, 6);
    var actions = ["left", "right", "up", "down", "jump", "action"];
    for (var i = 0; i < actions.length; ++i) {
        var name = actions[i];
        var held = input.btn(name);
        var c = held ? 11 : 5;
        gfx.print(name + ": " + (held ? "HELD" : "-"), 160, 12 + i * 8, c);
    }

    // --- Event log ---
    gfx.print("-- event log --", 160, 68, 6);
    for (var i = 0; i < log_lines.length; ++i) {
        gfx.print(log_lines[i], 160, 76 + i * 8, 7);
    }

    // --- Instructions ---
    gfx.print("arrows/wasd=move z/spc=flash", 4, 170, 5);

    // FPS counter
    var fps_str = "FPS:" + math.flr(sys.fps());
    var fps_w = fps_str.length * 6 + 2;
    gfx.rectfill(320 - fps_w, 0, 319, 8, 0);
    gfx.print(fps_str, 321 - fps_w, 1, 7);
}
