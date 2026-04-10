// Tilemap Demo
//
// Shows how to:
//   - Draw a tilemap with map.draw()
//   - Scroll the camera to follow a cursor
//   - Query tiles with map.get() / map.set()
//   - Use map.flag() for collision detection
//
// This demo generates a procedural tile grid in _init() since no Tiled
// JSON is loaded.  Replace with a real Tiled export for production use.
//
// When a real map is loaded via cart.json "maps", map.draw() renders
// tiles from the spritesheet automatically.

// --- Constants -------------------------------------------------------

var MAP_W = 40; // tiles wide
var MAP_H = 23; // tiles tall
var TILE = 8; // tile size in pixels

// Camera position (top-left)
var cam_x = 0;
var cam_y = 0;

// Cursor position in tiles
var cur_cx = 5;
var cur_cy = 5;

// Fake tile data (since we have no Tiled map loaded)
// We'll draw our own grid using pset/rectfill as a placeholder.
var tiles = [];

// --- Helpers ---------------------------------------------------------

function tile_at(cx, cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return 0;
    return tiles[cy * MAP_W + cx];
}

function set_tile(cx, cy, t) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return;
    tiles[cy * MAP_W + cx] = t;
}

// Procedural map: border walls + random scatter
function generate_map() {
    for (var y = 0; y < MAP_H; ++y) {
        for (var x = 0; x < MAP_W; ++x) {
            if (x === 0 || y === 0 || x === MAP_W - 1 || y === MAP_H - 1) {
                tiles.push(1); // wall
            } else if (math.rnd() < 0.08) {
                tiles.push(2); // decoration
            } else {
                tiles.push(0); // floor
            }
        }
    }
}

// Tile colours for placeholder rendering
var tile_colors = [1, 5, 3]; // floor=dark blue, wall=dark grey, deco=green

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

// --- Callbacks -------------------------------------------------------

function _init() {
    math.seed(42);
    generate_map();
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;
    // Move cursor with input actions
    if (input.btnp("left") && cur_cx > 0) --cur_cx;
    if (input.btnp("right") && cur_cx < MAP_W - 1) ++cur_cx;
    if (input.btnp("up") && cur_cy > 0) --cur_cy;
    if (input.btnp("down") && cur_cy < MAP_H - 1) ++cur_cy;

    // Toggle tile under cursor with Z
    if (key.btnp(key.Z)) {
        var t = tile_at(cur_cx, cur_cy);
        set_tile(cur_cx, cur_cy, t === 0 ? 1 : 0);
    }

    // Smooth camera follow
    var target_x = cur_cx * TILE - 160 + 4;
    var target_y = cur_cy * TILE - 90 + 4;
    cam_x += (target_x - cam_x) * 0.1;
    cam_y += (target_y - cam_y) * 0.1;

    // Clamp camera
    var max_cx = MAP_W * TILE - 320;
    var max_cy = MAP_H * TILE - 180;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > max_cx) cam_x = max_cx;
    if (cam_y > max_cy) cam_y = max_cy;
}

function _draw() {
    gfx.cls(0);

    // Apply camera offset
    var ox = -math.flr(cam_x);
    var oy = -math.flr(cam_y);
    gfx.camera(-ox, -oy);

    // NOTE: With a real Tiled map, you would call:
    //   map.draw(0, 0, 0, 0);
    // and the engine draws the tilemap from the spritesheet automatically.
    // Since we have no map loaded, we draw placeholder tiles manually:

    // Batch-draw the entire visible tilemap in one native call
    gfx.tilemap(tiles, MAP_W, MAP_H, tile_colors);

    // Overlay details on special tiles (only walls/deco, not every tile)
    var start_tx = math.flr(cam_x / TILE);
    var start_ty = math.flr(cam_y / TILE);
    var end_tx = start_tx + 41;
    var end_ty = start_ty + 24;

    if (end_tx > MAP_W) end_tx = MAP_W;
    if (end_ty > MAP_H) end_ty = MAP_H;
    if (start_tx < 0) start_tx = 0;
    if (start_ty < 0) start_ty = 0;

    for (var ty = start_ty; ty < end_ty; ++ty) {
        for (var tx = start_tx; tx < end_tx; ++tx) {
            var t = tile_at(tx, ty);
            if (t === 1) {
                var px = tx * TILE;
                var py = ty * TILE;
                gfx.rect(px, py, px + TILE - 1, py + TILE - 1, 6);
            } else if (t === 2) {
                var px = tx * TILE;
                var py = ty * TILE;
                gfx.pset(px + 3, py + 3, 11);
                gfx.pset(px + 4, py + 3, 11);
                gfx.pset(px + 3, py + 4, 11);
                gfx.pset(px + 4, py + 4, 11);
            }
        }
    }

    // Draw cursor
    var cx_px = cur_cx * TILE;
    var cy_px = cur_cy * TILE;
    gfx.rect(cx_px - 1, cy_px - 1, cx_px + TILE, cy_px + TILE, 8);

    // Reset camera for HUD
    gfx.camera(0, 0);

    // HUD
    gfx.rectfill(0, 0, 319, 10, 0);
    gfx.print(
        "tilemap demo  cursor:" +
            cur_cx +
            "," +
            cur_cy +
            "  tile:" +
            tile_at(cur_cx, cur_cy) +
            "  Z=toggle",
        2,
        2,
        7,
    );

    // FPS widget
    draw_fps_widget();
}
