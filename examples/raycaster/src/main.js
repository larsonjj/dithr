// Raycaster Demo
//
// A basic Wolfenstein 3D-style raycaster using DDA (Digital Differential
// Analyzer).  Renders flat-shaded walls with distance-based darkening,
// solid floor/ceiling, and a minimap overlay.
//
// Controls:
//   W / Up      — move forward
//   S / Down    — move backward
//   A           — strafe left
//   D           — strafe right
//   Left/Right  — turn

// --- Constants -------------------------------------------------------

var SCREEN_W = 320;
var SCREEN_H = 180;
var MAP_W = 16;
var MAP_H = 16;
var TILE_SIZE = 1; // world units per tile
var FOV = math.PI / 3; // 60 degrees
var HALF_FOV = FOV / 2;
var NUM_RAYS = SCREEN_W;
var MOVE_SPEED = 2.5;
var TURN_SPEED = 2.0;
var HALF_H = SCREEN_H / 2;

// --- World map (1 = wall, 0 = empty) --------------------------------

var world_map = [
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
];

// Wall colours per hit side (N/S vs E/W) for depth feel
var WALL_COL_NS = 12; // light blue
var WALL_COL_EW = 1; // dark blue

// Palette indices used for distance shading (light → dark)
var SHADE_NS = [12, 13, 2, 1];
var SHADE_EW = [6, 13, 2, 1];

// --- Player state ----------------------------------------------------

var px = 2.5;
var py = 2.5;
var pa = 0; // angle in radians
var show_minimap = true; // toggle with M key
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

// --- Helpers ---------------------------------------------------------

function map_at(x, y) {
    var mx = math.flr(x);
    var my = math.flr(y);
    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H) return 1;
    return world_map[my * MAP_W + mx];
}

function shade_color(dist, is_ns) {
    var shades = is_ns ? SHADE_NS : SHADE_EW;
    var idx = math.flr(dist / 3);
    if (idx >= shades.length) idx = shades.length - 1;
    return shades[idx];
}

// --- Callbacks -------------------------------------------------------

function _init() {
    // nothing needed
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    // Toggle minimap
    if (key.btnp(key.M)) show_minimap = !show_minimap;

    // Turning
    if (input.btn("turn_left")) pa -= TURN_SPEED * dt;
    if (input.btn("turn_right")) pa += TURN_SPEED * dt;

    // Gamepad right stick for turning
    if (pad.connected(0)) {
        var rx = pad.axis(pad.RX, 0);
        pa += rx * TURN_SPEED * dt;
    }

    // Movement vectors
    var cos_a = Math.cos(pa);
    var sin_a = Math.sin(pa);

    var dx = 0;
    var dy = 0;

    // Forward / back
    if (input.btn("forward")) {
        dx += cos_a * MOVE_SPEED * dt;
        dy += sin_a * MOVE_SPEED * dt;
    }
    if (input.btn("back")) {
        dx -= cos_a * MOVE_SPEED * dt;
        dy -= sin_a * MOVE_SPEED * dt;
    }

    // Strafe (left perpendicular in Y-down coords is (sin_a, -cos_a))
    if (input.btn("strafe_left")) {
        dx += sin_a * MOVE_SPEED * dt;
        dy -= cos_a * MOVE_SPEED * dt;
    }
    if (input.btn("strafe_right")) {
        dx -= sin_a * MOVE_SPEED * dt;
        dy += cos_a * MOVE_SPEED * dt;
    }

    // Gamepad left stick movement
    if (pad.connected(0)) {
        var lx = pad.axis(pad.LX, 0);
        var ly = pad.axis(pad.LY, 0);
        // Forward/back on LY
        dx += cos_a * -ly * MOVE_SPEED * dt;
        dy += sin_a * -ly * MOVE_SPEED * dt;
        // Strafe on LX (positive LX = right = (-sin_a, cos_a))
        dx -= sin_a * lx * MOVE_SPEED * dt;
        dy += cos_a * lx * MOVE_SPEED * dt;
    }

    // Slide along walls — try X and Y independently
    var margin = 0.2;
    if (map_at(px + dx + (dx > 0 ? margin : -margin), py) === 0) {
        px += dx;
    }
    if (map_at(px, py + dy + (dy > 0 ? margin : -margin)) === 0) {
        py += dy;
    }
}

function _draw() {
    // Ceiling
    gfx.rectfill(0, 0, SCREEN_W - 1, HALF_H - 1, 0);
    // Floor
    gfx.rectfill(0, HALF_H, SCREEN_W - 1, SCREEN_H - 1, 5);

    // --- Cast rays (batched drawing) ---------------------------------

    var angle_step = FOV / NUM_RAYS;
    var start_angle = pa - HALF_FOV;
    var ipx = math.flr(px);
    var ipy = math.flr(py);

    // State for batching adjacent columns with same color & height
    var batch_x0 = 0;
    var batch_start = 0;
    var batch_end = 0;
    var batch_col = -1;

    for (var i = 0; i <= NUM_RAYS; i++) {
        var col = -1;
        var draw_start = 0;
        var draw_end = 0;

        if (i < NUM_RAYS) {
            var ray_angle = start_angle + i * angle_step;
            var ray_cos = Math.cos(ray_angle);
            var ray_sin = Math.sin(ray_angle);

            // DDA setup
            var map_x = ipx;
            var map_y = ipy;

            var step_x = ray_cos >= 0 ? 1 : -1;
            var step_y = ray_sin >= 0 ? 1 : -1;

            var delta_x = ray_cos === 0 ? 1e30 : ray_cos > 0 ? 1 / ray_cos : -1 / ray_cos;
            var delta_y = ray_sin === 0 ? 1e30 : ray_sin > 0 ? 1 / ray_sin : -1 / ray_sin;

            var side_x = ray_cos < 0 ? (px - map_x) * delta_x : (map_x + 1 - px) * delta_x;
            var side_y = ray_sin < 0 ? (py - map_y) * delta_y : (map_y + 1 - py) * delta_y;

            // DDA traversal — inline map lookup
            var side = 0;
            for (;;) {
                if (side_x < side_y) {
                    side_x += delta_x;
                    map_x += step_x;
                    side = 0;
                } else {
                    side_y += delta_y;
                    map_y += step_y;
                    side = 1;
                }
                if (
                    map_x < 0 ||
                    map_x >= MAP_W ||
                    map_y < 0 ||
                    map_y >= MAP_H ||
                    world_map[map_y * MAP_W + map_x] !== 0
                ) {
                    break;
                }
            }

            // Perpendicular distance (fixes fisheye)
            var perp_dist;
            if (side === 0) {
                perp_dist = (map_x - px + (1 - step_x) / 2) / ray_cos;
            } else {
                perp_dist = (map_y - py + (1 - step_y) / 2) / ray_sin;
            }
            if (perp_dist < 0.01) perp_dist = 0.01;

            // Wall column height
            var line_h = math.flr(SCREEN_H / perp_dist);
            draw_start = math.flr(HALF_H - line_h / 2);
            draw_end = draw_start + line_h - 1;
            if (draw_start < 0) draw_start = 0;
            if (draw_end >= SCREEN_H) draw_end = SCREEN_H - 1;

            // Shade by distance + side
            col = shade_color(perp_dist, side === 0);
        }

        // Flush batch when color or height changes (or last column)
        if (col !== batch_col || draw_start !== batch_start || draw_end !== batch_end) {
            if (batch_col >= 0) {
                gfx.rectfill(batch_x0, batch_start, i - 1, batch_end, batch_col);
            }
            batch_x0 = i;
            batch_start = draw_start;
            batch_end = draw_end;
            batch_col = col;
        }
    }

    // --- Minimap overlay (top-left, toggleable with M) ---------------

    if (show_minimap) {
        var mm_scale = 4;
        var mm_x0 = 4;
        var mm_y0 = 4;

        for (var my = 0; my < MAP_H; my++) {
            for (var mx = 0; mx < MAP_W; mx++) {
                var c = world_map[my * MAP_W + mx] !== 0 ? 6 : 0;
                gfx.rectfill(
                    mm_x0 + mx * mm_scale,
                    mm_y0 + my * mm_scale,
                    mm_x0 + mx * mm_scale + mm_scale - 1,
                    mm_y0 + my * mm_scale + mm_scale - 1,
                    c,
                );
            }
        }

        // Player dot on minimap
        var pp_x = mm_x0 + math.flr(px * mm_scale);
        var pp_y = mm_y0 + math.flr(py * mm_scale);
        gfx.rectfill(pp_x - 1, pp_y - 1, pp_x + 1, pp_y + 1, 8);

        // Direction line on minimap
        var dir_len = 6;
        gfx.line(
            pp_x,
            pp_y,
            pp_x + math.flr(Math.cos(pa) * dir_len),
            pp_y + math.flr(Math.sin(pa) * dir_len),
            11,
        );
    }

    // --- HUD ---------------------------------------------------------

    gfx.print("WASD/Arrows: Move  M: Map", 72, 2, 7);

    // FPS widget
    draw_fps_widget();
}
