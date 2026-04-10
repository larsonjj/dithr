// Tween Showcase — easing functions, advanced math, and runtime input remapping
//
// Demonstrates: tween.start, tween.tick, tween.val, tween.done,
//   tween.cancel, tween.cancelAll, tween.ease,
//   math.smoothstep, math.pingpong, math.unlerp, math.remap,
//   math.mid, math.sign, math.ceil, math.round, math.trunc, math.pow,
//   math.rndRange, math.sin, math.cos, math.atan2,
//   input.map, input.axis, input.clear

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

var page = 0;
var PAGE_NAMES = ["EASING CURVES", "TWEEN POOL", "MATH UTILS", "INPUT REMAP"];
var NUM_PAGES = PAGE_NAMES.length;

// --- Easing curves page ---
var EASING_NAMES = [
    "linear",
    "easeIn",
    "easeOut",
    "easeInOut",
    "easeInQuad",
    "easeOutQuad",
    "easeInOutQuad",
    "easeInCubic",
    "easeOutCubic",
    "easeInOutCubic",
    "easeOutBack",
    "easeOutElastic",
    "easeOutBounce",
];
var curve_t = 0;

// --- Tween pool page ---
var tweens = [];
var balls = [];
var MAX_BALLS = 8;

// --- Math page ---
var math_timer = 0;

// --- Input remap page ---
var remap_obj_x = 160;
var input_scheme = 0;
var SCHEME_NAMES = ["WASD", "Arrow keys", "IJKL"];
var SCHEMES = [
    { name: "WASD", left: key.A, right: key.D },
    { name: "Arrow keys", left: key.LEFT, right: key.RIGHT },
    { name: "IJKL", left: key.J, right: key.L },
];

function apply_input_scheme(idx) {
    input.clear("move_left");
    input.clear("move_right");
    input.map("move_left", [{ type: 0, code: SCHEMES[idx].left }]);
    input.map("move_right", [{ type: 0, code: SCHEMES[idx].right }]);
}

function _init() {
    apply_input_scheme(0);
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    // Page navigation (disabled on input remap page to avoid arrow key conflict)
    if (page !== 3) {
        if (key.btnp(key.RIGHT) && !key.btn(key.LSHIFT)) {
            page = (page + 1) % NUM_PAGES;
        }
        if (key.btnp(key.LEFT) && !key.btn(key.LSHIFT)) {
            page = (page + NUM_PAGES - 1) % NUM_PAGES;
        }
    } else {
        // On input page, use Shift+Arrow to switch pages
        if (key.btnp(key.RIGHT) && key.btn(key.LSHIFT)) {
            page = (page + 1) % NUM_PAGES;
        }
        if (key.btnp(key.LEFT) && key.btn(key.LSHIFT)) {
            page = (page + NUM_PAGES - 1) % NUM_PAGES;
        }
    }

    tween.tick(dt);

    if (page === 0) {
        // Animate the curve preview
        curve_t += dt * 0.3;
        if (curve_t > 1) curve_t -= 1;
    }

    if (page === 1) {
        // Space spawns a new tween ball
        if (key.btnp(key.SPACE) && balls.length < MAX_BALLS) {
            var ease_name = EASING_NAMES[math.rndInt(EASING_NAMES.length)];
            var dur = math.rndRange(2.0, 5.0);
            var start_y = math.rndRange(30, 140);
            var tw = tween.start(10, 300, dur, ease_name);
            balls.push({
                tw: tw,
                ease: ease_name,
                y: start_y,
                col: math.rndInt(15) + 1,
            });
        }
        // C cancels all
        if (key.btnp(key.C)) {
            tween.cancelAll();
            balls = [];
        }
        // Remove done tweens
        for (var i = balls.length - 1; i >= 0; --i) {
            if (tween.done(balls[i].tw)) {
                balls.splice(i, 1);
            }
        }
    }

    if (page === 2) {
        math_timer += dt;
    }

    if (page === 3) {
        // Tab cycles input scheme
        if (key.btnp(key.TAB)) {
            input_scheme = (input_scheme + 1) % SCHEMES.length;
            apply_input_scheme(input_scheme);
        }
        // Move object with virtual input
        if (input.btn("move_left")) remap_obj_x -= 100 * dt;
        if (input.btn("move_right")) remap_obj_x += 100 * dt;
        remap_obj_x = math.clamp(remap_obj_x, 10, 310);

        // Also demonstrate input.axis if available
        var ax = input.axis("move_right");
    }
}

function draw_easing_page() {
    var cols = 4;
    var rows = math.ceil(EASING_NAMES.length / cols);
    var cw = 75;
    var ch = 36;
    var gap = 4;
    var ox = 6;
    var oy = 16;

    for (var i = 0; i < EASING_NAMES.length; ++i) {
        var cx = ox + (i % cols) * (cw + gap);
        var cy = oy + math.flr(i / cols) * (ch + gap + 8);

        // Draw curve background
        gfx.rect(cx, cy, cx + cw - 1, cy + ch - 1, 5);

        // Draw curve
        for (var px = 1; px < cw - 1; ++px) {
            var t = px / (cw - 2);
            var v = tween.ease(t, EASING_NAMES[i]);
            var py2 = cy + ch - 2 - math.flr(v * (ch - 4));
            py2 = math.clamp(py2, cy + 1, cy + ch - 2);
            gfx.pset(cx + px, py2, 11);
        }

        // Animated dot showing current position
        var cv = tween.ease(curve_t, EASING_NAMES[i]);
        var dot_x = cx + math.flr(curve_t * (cw - 2));
        var dot_y = cy + ch - 2 - math.flr(cv * (ch - 4));
        dot_y = math.clamp(dot_y, cy + 1, cy + ch - 2);
        gfx.circfill(dot_x, dot_y, 2, 8);

        // Label
        gfx.print(EASING_NAMES[i], cx, cy + ch + 1, 6);
    }
}

function draw_tween_page() {
    gfx.print("SPACE=spawn ball  C=cancel all  pool:" + balls.length + "/" + MAX_BALLS, 4, 16, 11);

    for (var i = 0; i < balls.length; ++i) {
        var b = balls[i];
        var x = tween.val(b.tw, 10);
        var done = tween.done(b.tw);
        gfx.circfill(math.flr(x), math.flr(b.y), 4, done ? 5 : b.col);
        gfx.print(b.ease, math.flr(x) + 6, math.flr(b.y) - 3, 6);
    }

    // Trail line
    gfx.line(10, 155, 300, 155, 5);
    gfx.print("start", 4, 158, 5);
    gfx.print("end", 290, 158, 5);
}

function draw_math_page() {
    var y = 16;
    var gap = 9;
    var t = (math_timer * 0.3) % 1.0;

    gfx.print("Advanced math interpolation:", 4, y, 7);
    y += gap + 2;

    // smoothstep
    var ss = math.smoothstep(0, 1, t);
    gfx.print("smoothstep(0,1," + t.toFixed(2) + ") = " + ss.toFixed(3), 4, y, 6);
    y += gap;

    // pingpong
    var pp = math.pingpong(math_timer, 2.0);
    gfx.print("pingpong(t, 2.0) = " + pp.toFixed(3), 4, y, 6);
    y += gap;

    // unlerp
    var ul = math.unlerp(10, 50, 30);
    gfx.print("unlerp(10, 50, 30) = " + ul.toFixed(3), 4, y, 6);
    y += gap;

    // remap
    var rm = math.remap(t, 0, 1, -100, 100);
    gfx.print("remap(" + t.toFixed(2) + ", 0,1, -100,100) = " + rm.toFixed(1), 4, y, 6);
    y += gap;

    // mid
    var md = math.mid(3, 7, 5);
    gfx.print("mid(3, 7, 5) = " + md, 4, y, 6);
    y += gap;

    // sign
    gfx.print(
        "sign(-42)=" + math.sign(-42) + "  sign(0)=" + math.sign(0) + "  sign(7)=" + math.sign(7),
        4,
        y,
        6,
    );
    y += gap;

    // ceil / round / trunc
    gfx.print(
        "ceil(3.2)=" +
            math.ceil(3.2) +
            "  round(3.5)=" +
            math.round(3.5) +
            "  trunc(3.9)=" +
            math.trunc(3.9),
        4,
        y,
        6,
    );
    y += gap;

    // pow
    gfx.print("pow(2, 10) = " + math.pow(2, 10), 4, y, 6);
    y += gap;

    // rndRange
    gfx.print("rndRange(10, 20) = " + math.rndRange(10, 20).toFixed(2), 4, y, 6);
    y += gap;

    // sin / cos (PICO-8 style 0-1)
    var angle = t;
    gfx.print(
        "sin(" +
            angle.toFixed(2) +
            ")=" +
            math.sin(angle).toFixed(3) +
            "  cos=" +
            math.cos(angle).toFixed(3),
        4,
        y,
        6,
    );
    y += gap;

    // atan2
    var at = math.atan2(1, 1);
    gfx.print("atan2(1, 1) = " + at.toFixed(4), 4, y, 6);
    y += gap;

    // Visual: smoothstep vs linear bar
    y += 4;
    gfx.print("Linear vs Smoothstep:", 4, y, 7);
    y += gap;
    var bar_w = 200;
    gfx.rect(4, y, 4 + bar_w, y + 6, 5);
    gfx.rectfill(4, y, 4 + math.flr(t * bar_w), y + 6, 12);
    gfx.print("linear", 210, y, 6);
    y += 10;
    gfx.rect(4, y, 4 + bar_w, y + 6, 5);
    gfx.rectfill(4, y, 4 + math.flr(ss * bar_w), y + 6, 11);
    gfx.print("smooth", 210, y, 6);
}

function draw_input_page() {
    var y = 16;
    var gap = 9;

    gfx.print("input.map / input.clear — runtime remapping", 4, y, 7);
    y += gap + 2;
    gfx.print("Current scheme: " + SCHEME_NAMES[input_scheme], 4, y, 11);
    y += gap;
    gfx.print("Press TAB to cycle schemes  (Shift+</>  switch page)", 4, y, 6);
    y += gap + 4;

    for (var i = 0; i < SCHEMES.length; ++i) {
        var col = i === input_scheme ? 11 : 5;
        gfx.print((i === input_scheme ? "> " : "  ") + SCHEME_NAMES[i], 10, y, col);
        y += gap;
    }

    y += gap;

    // Moving object
    gfx.rectfill(remap_obj_x - 8, 120, remap_obj_x + 8, 136, 12);
    gfx.rect(remap_obj_x - 8, 120, remap_obj_x + 8, 136, 7);
    gfx.print("<  MOVE  >", remap_obj_x - 20, 140, 6);

    // input.btn state
    var held_l = input.btn("move_left");
    var held_r = input.btn("move_right");
    gfx.print("move_left: " + (held_l ? "HELD" : "---"), 4, 155, held_l ? 8 : 5);
    gfx.print("move_right: " + (held_r ? "HELD" : "---"), 120, 155, held_r ? 8 : 5);
}

function _draw() {
    gfx.cls(0);

    gfx.rectfill(0, 0, 319, 10, 1);
    gfx.print(PAGE_NAMES[page] + " (" + (page + 1) + "/" + NUM_PAGES + ")  </>", 4, 2, 7);

    switch (page) {
        case 0:
            draw_easing_page();
            break;
        case 1:
            draw_tween_page();
            break;
        case 2:
            draw_math_page();
            break;
        case 3:
            draw_input_page();
            break;
    }

    draw_fps_widget();
}

function _save() {
    return {
        page: page,
        remap_obj_x: remap_obj_x,
        input_scheme: input_scheme,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    page = s.page;
    remap_obj_x = s.remap_obj_x;
    input_scheme = s.input_scheme;
    apply_input_scheme(input_scheme);
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
}
