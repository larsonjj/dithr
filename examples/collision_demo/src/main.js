// Collision Demo — all col.* collision helpers with interactive shapes
//
// Demonstrates: col.rect, col.circ, col.pointRect, col.pointCirc,
//   col.circRect, math.dist, math.angle, math.atan2,
//   mouse.x, mouse.y, mouse.dx, mouse.dy, mouse.btn, mouse.btnp,
//   mouse.show, mouse.hide

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

// -------------------------------------------------------------------------

// Shapes draggable with mouse
var shapes = [];
var dragging = -1;
var drag_offset_x = 0;
var drag_offset_y = 0;
var cursor_visible = true;
var last_dx = 0;
var last_dy = 0;
var dx_display_timer = 0;

function _init() {
    mouse.show();

    // Rectangle A
    shapes.push({ type: "rect", x: 170, y: 20, w: 50, h: 35, col: 12 });
    // Rectangle B
    shapes.push({ type: "rect", x: 265, y: 20, w: 40, h: 40, col: 14 });
    // Circle A
    shapes.push({ type: "circ", x: 195, y: 120, r: 20, col: 11 });
    // Circle B
    shapes.push({ type: "circ", x: 285, y: 120, r: 15, col: 9 });
}

// --- Collision helpers: test shape i vs shape j ---
function shapes_collide(a, b) {
    if (a.type === "rect" && b.type === "rect") {
        return col.rect(a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h);
    }
    if (a.type === "circ" && b.type === "circ") {
        return col.circ(a.x, a.y, a.r, b.x, b.y, b.r);
    }
    // circ vs rect (order doesn't matter)
    var c = a.type === "circ" ? a : b;
    var r = a.type === "rect" ? a : b;
    return col.circRect(c.x, c.y, c.r, r.x, r.y, r.w, r.h);
}

function point_in_shape(px, py, s) {
    if (s.type === "rect") return col.pointRect(px, py, s.x, s.y, s.w, s.h);
    return col.pointCirc(px, py, s.x, s.y, s.r);
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    var mx = mouse.x();
    var my = mouse.y();
    var mdx = mouse.dx();
    var mdy = mouse.dy();

    // Keep dx/dy visible for a short time after movement
    if (mdx !== 0 || mdy !== 0) {
        last_dx = mdx;
        last_dy = mdy;
        dx_display_timer = 0.5;
    }
    if (dx_display_timer > 0) {
        dx_display_timer -= dt;
        if (dx_display_timer <= 0) {
            last_dx = 0;
            last_dy = 0;
        }
    }

    // Toggle cursor visibility with H
    if (key.btnp(key.H)) {
        cursor_visible = !cursor_visible;
        if (cursor_visible) mouse.show();
        else mouse.hide();
    }

    // Start dragging on click
    if (mouse.btnp(mouse.LEFT)) {
        for (var i = shapes.length - 1; i >= 0; --i) {
            var s = shapes[i];
            if (point_in_shape(mx, my, s)) {
                dragging = i;
                drag_offset_x = s.x - mx;
                drag_offset_y = s.y - my;
                break;
            }
        }
    }

    // Drag with mouse position
    if (dragging >= 0 && mouse.btn(mouse.LEFT)) {
        shapes[dragging].x = mx + drag_offset_x;
        shapes[dragging].y = my + drag_offset_y;
    }

    // Release
    if (!mouse.btn(mouse.LEFT)) {
        dragging = -1;
    }
}

function _draw() {
    gfx.cls(0);

    gfx.print("COLLISION DEMO \u2014 drag shapes with mouse", 4, 2, 7);
    gfx.print("H=toggle cursor", 4, 170, 5);

    var mx = mouse.x();
    var my = mouse.y();

    // --- Build per-shape collision flags ---
    var hit = []; // hit[i] = true if shape i collides with anything
    for (var i = 0; i < shapes.length; ++i) hit.push(false);

    // shape vs shape (all pairs)
    var pair_results = [];
    for (var i = 0; i < shapes.length; ++i) {
        for (var j = i + 1; j < shapes.length; ++j) {
            var c = shapes_collide(shapes[i], shapes[j]);
            if (c) {
                hit[i] = true;
                hit[j] = true;
            }
            pair_results.push({ a: i, b: j, hit: c });
        }
    }

    // mouse vs each shape
    var mouse_hits = [];
    for (var i = 0; i < shapes.length; ++i) {
        var mh = point_in_shape(mx, my, shapes[i]);
        mouse_hits.push(mh);
        if (mh) hit[i] = true;
    }

    // --- Draw shapes ---
    var labels = ["A", "B", "C", "D"];
    for (var i = 0; i < shapes.length; ++i) {
        var s = shapes[i];
        var c = hit[i] ? 8 : s.col;
        if (s.type === "rect") {
            gfx.rectfill(s.x, s.y, s.x + s.w - 1, s.y + s.h - 1, c);
            gfx.rect(s.x, s.y, s.x + s.w - 1, s.y + s.h - 1, 7);
            gfx.print(labels[i], s.x + 2, s.y + 2, 0);
        } else {
            gfx.circfill(s.x, s.y, s.r, c);
            gfx.circ(s.x, s.y, s.r, 7);
            gfx.print(labels[i], s.x - 2, s.y - 3, 0);
        }
    }

    // Draw lines between colliding circle pairs
    var c0 = shapes[2],
        c1 = shapes[3];
    if (c0.type === "circ" && c1.type === "circ") {
        gfx.line(c0.x, c0.y, c1.x, c1.y, 5);
    }

    // Mouse crosshair
    gfx.line(mx - 4, my, mx + 4, my, 7);
    gfx.line(mx, my - 4, mx, my + 4, 7);

    // --- Info panel: all shape-pair results ---
    var iy = 12;
    var panelW = 115;
    // Pre-calculate height: header(8) + pairs*8 + gap(2) + header(8) + mouse*8 + padding(4)
    var panelH = 8 + pair_results.length * 8 + 2 + 8 + mouse_hits.length * 8 + 4;
    gfx.rectfill(0, iy, panelW, iy + panelH, 1);
    var py = iy + 2;

    gfx.print("Shape collisions:", 2, py, 7);
    py += 8;
    for (var k = 0; k < pair_results.length; ++k) {
        var p = pair_results[k];
        var la = labels[p.a],
            lb = labels[p.b];
        var txt = la + " - " + lb + ": " + (p.hit ? "HIT" : "---");
        gfx.print(txt, 4, py, p.hit ? 8 : 6);
        py += 8;
    }

    py += 2;
    gfx.print("Mouse collisions:", 2, py, 7);
    py += 8;
    for (var k = 0; k < mouse_hits.length; ++k) {
        var txt = "mouse - " + labels[k] + ": " + (mouse_hits[k] ? "HIT" : "---");
        gfx.print(txt, 4, py, mouse_hits[k] ? 8 : 6);
        py += 8;
    }

    // Distance/angle between circles C and D
    var dist = math.dist(c0.x, c0.y, c1.x, c1.y);
    var angle = math.angle(c0.x, c0.y, c1.x, c1.y);
    var atan = math.atan2(c1.y - c0.y, c1.x - c0.x);

    gfx.rectfill(0, 148, 200, 168, 1);
    gfx.print("dist C-D: " + math.flr(dist), 2, 150, 6);
    gfx.print("angle: " + angle.toFixed(2) + " rad", 2, 158, 6);
    gfx.print("atan2: " + atan.toFixed(2), 110, 158, 6);

    // Mouse dx/dy = pixels moved since last frame
    gfx.print("mouse dx:" + last_dx + " dy:" + last_dy, 170, 170, dx_display_timer > 0 ? 7 : 5);

    draw_fps_widget();
}

function _save() {
    return {
        shapes: shapes,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    shapes = s.shapes;
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
}
