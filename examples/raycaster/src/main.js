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
var SHADE_NS = [12, 13, 5, 1];
var SHADE_EW = [6, 13, 5, 1];

// --- Player state ----------------------------------------------------

var px = 2.5;
var py = 2.5;
var pa = 0; // angle in radians

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

    // --- Cast rays ---------------------------------------------------

    for (var i = 0; i < NUM_RAYS; i++) {
        var ray_angle = pa - HALF_FOV + (i / NUM_RAYS) * FOV;
        var ray_cos = Math.cos(ray_angle);
        var ray_sin = Math.sin(ray_angle);

        // DDA setup
        var map_x = math.flr(px);
        var map_y = math.flr(py);

        // Step direction
        var step_x = ray_cos >= 0 ? 1 : -1;
        var step_y = ray_sin >= 0 ? 1 : -1;

        // Distance between consecutive X / Y grid crossings
        var delta_x = ray_cos === 0 ? 1e30 : math.abs(1 / ray_cos);
        var delta_y = ray_sin === 0 ? 1e30 : math.abs(1 / ray_sin);

        // Distance to first X / Y crossing
        var side_x, side_y;
        if (ray_cos < 0) {
            side_x = (px - map_x) * delta_x;
        } else {
            side_x = (map_x + 1 - px) * delta_x;
        }
        if (ray_sin < 0) {
            side_y = (py - map_y) * delta_y;
        } else {
            side_y = (map_y + 1 - py) * delta_y;
        }

        // DDA traversal
        var hit = 0;
        var side = 0; // 0 = NS wall (vertical), 1 = EW wall (horizontal)
        while (hit === 0) {
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = 0;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = 1;
            }
            if (map_at(map_x, map_y) !== 0) {
                hit = 1;
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
        var draw_start = math.flr(HALF_H - line_h / 2);
        var draw_end = draw_start + line_h - 1;

        // Clamp to screen
        if (draw_start < 0) draw_start = 0;
        if (draw_end >= SCREEN_H) draw_end = SCREEN_H - 1;

        // Shade by distance + side
        var col = shade_color(perp_dist, side === 0);

        // Draw vertical wall strip
        gfx.line(i, draw_start, i, draw_end, col);
    }

    // --- Minimap overlay (top-left) ----------------------------------

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

    // --- Left stick debug overlay ------------------------------------

    if (pad.connected(0)) {
        var ls_cx = 20;
        var ls_cy = SCREEN_H - 20;
        var lx = pad.axis(pad.LX, 0);
        var ly = pad.axis(pad.LY, 0);
        gfx.circ(ls_cx, ls_cy, 10, 5);
        gfx.circfill(ls_cx + lx * 10, ls_cy + ly * 10, 2, 11);
        gfx.print("L", ls_cx - 2, ls_cy + 12, 6);
    }

    // --- HUD ---------------------------------------------------------

    gfx.print("WASD/Arrows: Move & Turn", 76, 2, 7);

    var fps_str = "FPS:" + math.flr(sys.fps());
    var fps_w = fps_str.length * 6 + 2;
    gfx.rectfill(SCREEN_W - fps_w, SCREEN_H - 10, SCREEN_W - 1, SCREEN_H - 1, 0);
    gfx.print(fps_str, SCREEN_W + 1 - fps_w, SCREEN_H - 9, 7);
}
