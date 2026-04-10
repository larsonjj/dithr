// Map Demo — tilemap API, camera helpers, map objects, multi-layer, levels
//
// Demonstrates: map.create, map.draw, map.get, map.set, map.flag,
//   map.width, map.height, map.layers, map.levels, map.load,
//   map.current_level, map.objects, map.object, map.objects_in,
//   map.objects_with, map.add_layer, map.remove_layer, map.resize, map.data,
//   cam.set, cam.get, cam.reset, cam.follow,
//   gfx.fget, gfx.fset

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
// Tile legend (drawn directly into spritesheet at runtime)
//   0 = empty (transparent)
//   1 = grass (green)
//   2 = wall  (brown) — flagged as solid
//   3 = water (blue)  — flagged as solid
//   4 = path  (beige)
//   5 = spawn (yellow marker)
//   6 = chest (orange marker)
// -------------------------------------------------------------------------

var TILE_COLS = [0, 11, 4, 12, 15, 10, 9];

var MAP_W = 40;
var MAP_H = 23;
var TILE = 8;

var px = 0;
var py = 0;
var SPEED = 80; // pixels per second

var current_level_name = "";
var level_names = [];
var level_idx = 0;
var objects_near = [];
var info_msg = "";
var info_timer = 0;

function build_sheet() {
    gfx.sheetCreate(128, 128, 8, 8);
    for (var t = 0; t < TILE_COLS.length; ++t) {
        var sx = (t % 16) * 8;
        var sy = math.flr(t / 16) * 8;
        for (var py2 = 0; py2 < 8; ++py2) {
            for (var px2 = 0; px2 < 8; ++px2) {
                if (t === 0) continue; // keep transparent
                gfx.sset(sx + px2, sy + py2, TILE_COLS[t]);
            }
        }
    }
    // Mark wall (2) and water (3) as solid via flag bit 0
    gfx.fset(2, 0, 1);
    gfx.fset(3, 0, 1);
}

function build_level(name) {
    map.create(MAP_W, MAP_H, name);

    // Fill with grass
    for (var cy = 0; cy < MAP_H; ++cy) {
        for (var cx = 0; cx < MAP_W; ++cx) {
            map.set(cx, cy, 1);
        }
    }

    // Border walls
    for (var cx2 = 0; cx2 < MAP_W; ++cx2) {
        map.set(cx2, 0, 2);
        map.set(cx2, MAP_H - 1, 2);
    }
    for (var cy2 = 0; cy2 < MAP_H; ++cy2) {
        map.set(0, cy2, 2);
        map.set(MAP_W - 1, cy2, 2);
    }

    // Scattered features based on level name
    var seed = 0;
    for (var ch = 0; ch < name.length; ++ch) seed += name.charCodeAt(ch);
    math.seed(seed);

    // Random water pools
    for (var w = 0; w < 4; ++w) {
        var wx2 = math.rnd_int(MAP_W - 8) + 3;
        var wy2 = math.rnd_int(MAP_H - 6) + 3;
        for (var dy = 0; dy < 3; ++dy) {
            for (var dx = 0; dx < 4; ++dx) {
                map.set(wx2 + dx, wy2 + dy, 3);
            }
        }
    }

    // Random walls
    for (var wb = 0; wb < 8; ++wb) {
        var bx = math.rnd_int(MAP_W - 6) + 3;
        var by2 = math.rnd_int(MAP_H - 4) + 2;
        var horiz = math.rnd(1) > 0.5;
        for (var s = 0; s < 5; ++s) {
            if (horiz) map.set(bx + s, by2, 2);
            else map.set(bx, by2 + s, 2);
        }
    }

    // Path
    for (var px3 = 2; px3 < MAP_W - 2; ++px3) {
        map.set(px3, math.flr(MAP_H / 2), 4);
    }

    // Ensure spawn area is clear
    map.set(2, 2, 4);
    map.set(3, 2, 4);
    map.set(2, 3, 4);
    map.set(3, 3, 4);

    // Add a second layer for decoration
    map.add_layer("overlay");
    // Place spawn marker on overlay layer
    map.set(2, 2, 5, 1);

    // Place chest objects via overlay
    for (var c = 0; c < 3; ++c) {
        var cx3 = math.rnd_int(MAP_W - 6) + 3;
        var cy3 = math.rnd_int(MAP_H - 6) + 3;
        map.set(cx3, cy3, 6, 1);
    }
}

function show_info(msg) {
    info_msg = msg;
    info_timer = 3.0;
}

function _init() {
    build_sheet();

    // Create two levels
    build_level("overworld");
    build_level("dungeon");

    level_names = map.levels();
    level_idx = 0;
    map.load(level_names[level_idx]);
    current_level_name = map.current_level();

    // Display map metadata
    var w = map.width();
    var h = map.height();
    var layers = map.layers();
    sys.log("Level: " + current_level_name + " (" + w + "x" + h + ") layers: " + layers.join(", "));

    // Get full map data for logging
    var d = map.data();
    if (d) sys.log("Map data name: " + d.name);

    // Spawn at the spawn marker
    px = 2 * TILE;
    py = 2 * TILE;
    cam.reset();

    show_info("Level: " + current_level_name + " | TAB=switch | Arrow/WASD=move");
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    if (info_timer > 0) info_timer -= dt;

    // Switch level with TAB
    if (key.btnp(key.TAB)) {
        level_idx = (level_idx + 1) % level_names.length;
        map.load(level_names[level_idx]);
        current_level_name = map.current_level();
        px = 2 * TILE;
        py = 2 * TILE;
        show_info("Switched to: " + current_level_name);
    }

    // Movement with collision via map.flag
    var dx = 0;
    var dy = 0;
    if (input.btn("left")) dx = -1;
    if (input.btn("right")) dx = 1;
    if (input.btn("up")) dy = -1;
    if (input.btn("down")) dy = 1;

    var nx = px + dx * SPEED * dt;
    var ny = py + dy * SPEED * dt;

    // Check map.flag for solid tiles (bit 0)
    var cx_new = math.flr(nx / TILE);
    var cy_new = math.flr(ny / TILE);
    var cx_old_x = math.flr(px / TILE);
    var cy_old_y = math.flr(py / TILE);

    // Horizontal
    if (dx !== 0) {
        var check_cx = math.flr((nx + (dx > 0 ? 7 : 0)) / TILE);
        if (
            !map.flag(check_cx, math.flr(py / TILE), 0) &&
            !map.flag(check_cx, math.flr((py + 7) / TILE), 0)
        ) {
            px = nx;
        }
    }

    // Vertical
    if (dy !== 0) {
        var check_cy = math.flr((ny + (dy > 0 ? 7 : 0)) / TILE);
        if (
            !map.flag(math.flr(px / TILE), check_cy, 0) &&
            !map.flag(math.flr((px + 7) / TILE), check_cy, 0)
        ) {
            py = ny;
        }
    }

    // Query objects near the player via map.objects_in (overlay layer)
    objects_near = map.objects_in(px - 16, py - 16, 40, 40);

    // Action button — pick up chests nearby
    if (input.btnp("action")) {
        var pcx = math.flr(px / TILE);
        var pcy = math.flr(py / TILE);
        var tile_overlay = map.get(pcx, pcy, 1);
        if (tile_overlay === 6) {
            map.set(pcx, pcy, 0, 1); // clear chest
            show_info("Found a treasure chest!");
        }
    }

    // Smooth camera follow
    cam.follow(px - 160 + 4, py - 90 + 4, 0.08);
}

function _draw() {
    gfx.cls(0);

    // Draw both layers
    map.draw(0, 0, 0, 0); // base layer
    map.draw(0, 0, 0, 0, undefined, undefined, 1); // overlay layer

    // Draw player (simple coloured rect in world coords)
    var cam_pos = cam.get();
    gfx.rectfill(px - cam_pos.x, py - cam_pos.y, px - cam_pos.x + 7, py - cam_pos.y + 7, 8);

    // HUD (screen-space — temporarily reset camera)
    var saved_cam = cam.get();
    cam.set(0, 0);

    // Info message
    if (info_timer > 0) {
        gfx.rectfill(0, 170, 319, 179, 1);
        gfx.print(info_msg, 2, 172, 7);
    }

    // Level / tile info
    var pcx = math.flr(px / TILE);
    var pcy = math.flr(py / TILE);
    var tile_id = map.get(pcx, pcy);
    var is_solid = map.flag(pcx, pcy, 0);
    gfx.rectfill(0, 0, 180, 8, 0);
    gfx.print(
        current_level_name +
            " tile:" +
            tile_id +
            (is_solid ? " SOLID" : "") +
            " (" +
            pcx +
            "," +
            pcy +
            ")",
        1,
        1,
        6,
    );

    draw_fps_widget();

    cam.set(saved_cam.x, saved_cam.y);
}

function _save() {
    return {
        px: px,
        py: py,
        level_idx: level_idx,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    px = s.px;
    py = s.py;
    level_idx = s.level_idx;
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
    map.load(level_names[level_idx]);
    current_level_name = map.current_level();
}
