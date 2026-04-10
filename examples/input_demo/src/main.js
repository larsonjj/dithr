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
var wheel_display = 0; // accumulated wheel for display
var wheel_decay = 0; // timer to fade wheel display

function prv_log(msg) {
    log_lines.push(msg);
    if (log_lines.length > 8) {
        log_lines.shift();
    }
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
    mouse.show();
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;
    // Virtual action movement (keyboard/gamepad mapped)
    if (input.btn("left")) px -= speed * dt;
    if (input.btn("right")) px += speed * dt;
    if (input.btn("up")) py -= speed * dt;
    if (input.btn("down")) py += speed * dt;

    // Left stick direct movement
    if (pad.connected(0)) {
        var slx = pad.axis(pad.LX, 0);
        var sly = pad.axis(pad.LY, 0);
        px += slx * speed * dt;
        py += sly * speed * dt;
    }

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
    if (mouse.btnp(mouse.MIDDLE)) prv_log("mouse: MIDDLE click");
    if (mouse.btnp(mouse.RIGHT)) prv_log("mouse: RIGHT click");

    // Log scroll wheel
    var w = mouse.wheel();
    if (w !== 0) {
        wheel_display += w;
        wheel_decay = 1.0;
        prv_log("mouse: wheel " + (w > 0 ? "up" : "down"));
    }
    if (wheel_decay > 0) {
        wheel_decay -= dt;
        if (wheel_decay <= 0) wheel_display = 0;
    }

    // Log gamepad presses
    if (pad.btnp(pad.A, 0)) prv_log("pad: A");
    if (pad.btnp(pad.B, 0)) prv_log("pad: B");
    if (pad.btnp(pad.X, 0)) prv_log("pad: X");
    if (pad.btnp(pad.Y, 0)) prv_log("pad: Y");
    if (pad.btnp(pad.L1, 0)) prv_log("pad: L1");
    if (pad.btnp(pad.R1, 0)) prv_log("pad: R1");
    if (pad.btnp(pad.L2, 0)) prv_log("pad: L2");
    if (pad.btnp(pad.R2, 0)) prv_log("pad: R2");
    if (pad.btnp(pad.START, 0)) prv_log("pad: START");
    if (pad.btnp(pad.SELECT, 0)) prv_log("pad: SELECT");
    if (pad.btnp(pad.UP, 0)) prv_log("pad: DPAD UP");
    if (pad.btnp(pad.DOWN, 0)) prv_log("pad: DPAD DN");
    if (pad.btnp(pad.LEFT, 0)) prv_log("pad: DPAD L");
    if (pad.btnp(pad.RIGHT, 0)) prv_log("pad: DPAD R");
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
    if (key.btn(key.W)) arrows += "W ";
    if (key.btn(key.A)) arrows += "A ";
    if (key.btn(key.S)) arrows += "S ";
    if (key.btn(key.D)) arrows += "D ";
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
    gfx.print("wheel: " + wheel_display, 4, 48, 7);

    // Crosshair
    var mx = mouse.x();
    var my = mouse.y();
    gfx.line(mx - 4, my, mx + 4, my, 8);
    gfx.line(mx, my - 4, mx, my + 4, 8);

    // --- Gamepad ---
    gfx.print("-- gamepad 0 --", 4, 60, 6);
    if (pad.connected(0)) {
        gfx.print(pad.name(0), 4, 68, 7);

        // Left stick
        var lx = math.flr(pad.axis(pad.LX, 0) * 100) / 100;
        var ly = math.flr(pad.axis(pad.LY, 0) * 100) / 100;
        var ls_cx = 24;
        var ls_cy = 90;
        gfx.circ(ls_cx, ls_cy, 10, 5);
        gfx.circfill(ls_cx + lx * 10, ls_cy + ly * 10, 2, 11);
        gfx.print("L", ls_cx - 2, ls_cy + 12, 6);

        // Right stick
        var rx = math.flr(pad.axis(pad.RX, 0) * 100) / 100;
        var ry = math.flr(pad.axis(pad.RY, 0) * 100) / 100;
        var rs_cx = 64;
        var rs_cy = 90;
        gfx.circ(rs_cx, rs_cy, 10, 5);
        gfx.circfill(rs_cx + rx * 10, rs_cy + ry * 10, 2, 8);
        gfx.print("R", rs_cx - 2, rs_cy + 12, 6);

        // Triggers
        var l2 = math.flr(pad.axis(pad.TL, 0) * 100);
        var r2 = math.flr(pad.axis(pad.TR, 0) * 100);
        gfx.print("L2:" + l2 + "% R2:" + r2 + "%", 4, 108, 7);

        // Button grid — show held state for face + shoulder + dpad
        var btn_names = ["A", "B", "X", "Y", "L1", "R1", "ST", "SL"];
        var btn_ids = [pad.A, pad.B, pad.X, pad.Y, pad.L1, pad.R1, pad.START, pad.SELECT];
        var bx = 4;
        for (var i = 0; i < btn_names.length; ++i) {
            var held = pad.btn(btn_ids[i], 0);
            var bc = held ? 11 : 5;
            gfx.print(btn_names[i], bx, 118, bc);
            bx += (btn_names[i].length + 1) * 6;
        }

        // D-pad visual
        var dp_cx = 110;
        var dp_cy = 90;
        gfx.print("D", dp_cx - 2, dp_cy + 12, 6);
        gfx.rectfill(dp_cx - 2, dp_cy - 8, dp_cx + 2, dp_cy + 8, 5); // vert
        gfx.rectfill(dp_cx - 8, dp_cy - 2, dp_cx + 8, dp_cy + 2, 5); // horiz
        if (pad.btn(pad.UP, 0)) gfx.rectfill(dp_cx - 2, dp_cy - 8, dp_cx + 2, dp_cy - 3, 11);
        if (pad.btn(pad.DOWN, 0)) gfx.rectfill(dp_cx - 2, dp_cy + 3, dp_cx + 2, dp_cy + 8, 11);
        if (pad.btn(pad.LEFT, 0)) gfx.rectfill(dp_cx - 8, dp_cy - 2, dp_cx - 3, dp_cy + 2, 11);
        if (pad.btn(pad.RIGHT, 0)) gfx.rectfill(dp_cx + 3, dp_cy - 2, dp_cx + 8, dp_cy + 2, 11);
    } else {
        gfx.print("(not connected)", 4, 68, 5);
    }

    // --- Virtual actions ---
    gfx.print("-- actions --", 160, 4, 6);
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
    gfx.print("arrows/wasd/lstick=move z/spc=flash", 4, 170, 5);

    // FPS widget
    draw_fps_widget();
}
