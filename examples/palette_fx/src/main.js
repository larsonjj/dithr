// Palette FX — palette remapping, transparency, fill patterns, clip, custom font
//
// Demonstrates: gfx.pal, gfx.palt, gfx.fillp, gfx.color, gfx.cursor,
//   gfx.clip, gfx.font, gfx.textHeight, gfx.pget,
//   gfx.sheetCreate, gfx.sset, gfx.sget, gfx.sheetW, gfx.sheetH,
//   gfx.tri, gfx.poly, gfx.polyfill

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
// Build a tiny spritesheet with a custom font and some sprites at runtime
// -------------------------------------------------------------------------

var timer = 0;
var pal_cycle_offset = 0;
var demo_mode = 0;
var MODE_NAMES = [
    "PAL REMAP",
    "FILL PATTERNS",
    "CLIP REGION",
    "CUSTOM FONT",
    "PGET/SGET",
    "POLY SHAPES",
];
var NUM_MODES = MODE_NAMES.length;

function build_sheet() {
    gfx.sheetCreate(128, 128, 8, 8);
    var sw = gfx.sheetW();
    var sh = gfx.sheetH();
    sys.log("Sheet created: " + sw + "x" + sh);

    // Draw a simple smiley sprite at tile (0,0)
    var smiley = [
        0, 0, 10, 10, 10, 10, 0, 0, 0, 10, 10, 10, 10, 10, 10, 0, 10, 10, 0, 10, 10, 0, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 10, 10, 10, 10, 0, 10, 0,
        10, 0, 0, 0, 0, 10, 0, 0, 0, 10, 10, 10, 10, 0, 0,
    ];
    for (var i = 0; i < 64; ++i) {
        var px = i % 8,
            py = math.flr(i / 8);
        if (smiley[i] > 0) gfx.sset(px, py, smiley[i]);
    }

    // Draw a simple custom 4x6 font at row 2 (y=16) for chars A-Z + 0-9
    // Each char is 4 wide, 6 tall, packed into spritesheet starting at (0, 16)
    // For simplicity, just draw a colored block for each character
    var char_start = 0;
    for (var ch = 0; ch < 36; ++ch) {
        var cx = (ch % 32) * 4;
        var cy = 16 + math.flr(ch / 32) * 6;
        var col = 7;
        // Draw a simple pattern — letter shape approximated by a filled block
        // with a distinctive cutout
        for (var fy = 0; fy < 6; ++fy) {
            for (var fx = 0; fx < 4; ++fx) {
                // Skip one pixel to make it look like a letter
                if (fx === 0 && fy === 0) continue;
                if (fx === 3 && fy === 5) continue;
                gfx.sset(cx + fx, cy + fy, col);
            }
        }
    }
}

function _init() {
    build_sheet();
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    timer += dt;

    // Cycle demo mode with LEFT/RIGHT
    if (key.btnp(key.RIGHT) || key.btnp(key.D)) {
        demo_mode = (demo_mode + 1) % NUM_MODES;
    }
    if (key.btnp(key.LEFT) || key.btnp(key.A)) {
        demo_mode = (demo_mode + NUM_MODES - 1) % NUM_MODES;
    }

    pal_cycle_offset = math.flr(timer * 3) % 16;
}

function draw_pal_demo() {
    // Palette colour cycling — shift draw colours
    gfx.print("gfx.pal() — palette remapping", 10, 30, 7);

    // Draw 16 colour swatches before remap
    gfx.print("Original:", 10, 42, 6);
    for (var c = 0; c < 16; ++c) {
        gfx.rectfill(80 + c * 12, 40, 80 + c * 12 + 10, 50, c);
    }

    // Apply a cycled palette remap
    for (var c2 = 0; c2 < 16; ++c2) {
        gfx.pal(c2, (c2 + pal_cycle_offset) % 16);
    }

    gfx.print("Remapped:", 10, 56, 6);
    for (var c3 = 0; c3 < 16; ++c3) {
        gfx.rectfill(80 + c3 * 12, 54, 80 + c3 * 12 + 10, 64, c3);
    }

    // Draw smiley with remapped palette
    gfx.spr(0, 150, 80);
    gfx.print("Smiley with pal remap", 110, 94, 6);

    // Reset palette
    gfx.pal();

    // Demonstrate palt (transparency)
    gfx.print("gfx.palt() — transparency", 10, 110, 7);
    gfx.rectfill(150, 110, 170, 130, 12);
    gfx.palt(0, true); // Make colour 0 transparent
    gfx.spr(0, 152, 112);
    gfx.palt(); // Reset
    gfx.print("Colour 0 = transparent", 110, 134, 6);
}

function draw_fillp_demo() {
    gfx.print("gfx.fillp() — fill patterns", 10, 30, 7);

    var patterns = [0x5a5a, 0xaaaa, 0xf0f0, 0xcccc, 0xff00, 0x9669];
    var names = ["checker", "stripe", "halves", "cols", "rows", "diamond"];

    for (var i = 0; i < patterns.length; ++i) {
        var x = 10 + (i % 3) * 105;
        var y = 45 + math.flr(i / 3) * 55;

        gfx.fillp(patterns[i]);
        gfx.rectfill(x, y, x + 40, y + 30, 12);
        gfx.fillp(0);
        gfx.print(names[i], x, y + 34, 6);
        gfx.print("0x" + patterns[i].toString(16).toUpperCase(), x, y + 42, 5);
    }

    // Animated pattern
    var shift = math.flr(timer * 4) % 4;
    var animated = 0x5a5a;
    if (shift === 1) animated = 0xa5a5;
    if (shift === 2) animated = 0x5a5a;
    if (shift === 3) animated = 0xa5a5;
    gfx.fillp(animated);
    gfx.circfill(260, 120, 20, 11);
    gfx.fillp(0);
    gfx.print("animated", 242, 145, 6);
}

function draw_clip_demo() {
    gfx.print("gfx.clip() — clipping rectangle", 10, 30, 7);

    // Draw visible area indicator
    var cx = 60 + math.flr(math.sin(timer * 0.2) * 40);
    var cy = 70 + math.flr(math.cos(timer * 0.15) * 20);
    var cw = 100;
    var ch = 60;

    gfx.rect(cx - 1, cy - 1, cx + cw, cy + ch, 5);
    gfx.print("clip region", cx, cy - 10, 6);

    // Set clip and draw content that extends beyond
    gfx.clip(cx, cy, cw, ch);

    // Draw things that should be cropped
    gfx.rectfill(0, 0, 319, 179, 1);
    for (var i = 0; i < 20; ++i) {
        var r = 10 + i * 15;
        gfx.circ(160, 100, r, 7 + (i % 8));
    }
    gfx.print("CLIPPED CONTENT", 100, 95, 7);

    // Reset clip
    gfx.clip();

    gfx.print("Only content inside the box is visible", 10, 150, 6);
}

function draw_font_demo() {
    gfx.print("gfx.font() — custom font", 10, 30, 7);

    // Default font
    gfx.print("Default 4x6 font: Hello World!", 10, 45, 7);

    // Switch to custom font from spritesheet row 2
    gfx.font(0, 16, 4, 6, 65, 26);
    gfx.print("CUSTOM FONT ABCDEFG", 10, 60, 10);

    // Reset font
    gfx.font();
    gfx.print("Back to default font", 10, 75, 7);

    // textHeight demo
    var multi = "Line 1\nLine 2\nLine 3";
    var th = gfx.textHeight(multi);
    gfx.print("textHeight of 3-line string: " + th + " px", 10, 90, 6);
    gfx.print(multi, 10, 105, 12);

    // cursor demo
    gfx.cursor(10, 140);
    gfx.color(11);
    gfx.print("Printed at cursor pos (10, 140)");
    gfx.print("Next line auto-advances");
    gfx.color(7); // reset
}

function draw_pget_demo() {
    gfx.print("gfx.pget / gfx.sget — pixel readback", 10, 30, 7);

    // Draw some content
    gfx.circfill(80, 80, 30, 12);
    gfx.circfill(80, 80, 20, 11);
    gfx.circfill(80, 80, 10, 10);

    // Read pixels with pget
    gfx.print("pget results:", 140, 50, 6);
    for (var i = 0; i < 8; ++i) {
        var px2 = 50 + i * 8;
        var py2 = 80;
        var col = gfx.pget(px2, py2);
        gfx.print("(" + px2 + "," + py2 + ")=" + col, 140, 60 + i * 8, 7);
    }

    // Read spritesheet pixels with sget
    gfx.print("sget results (smiley):", 10, 120, 6);
    for (var sy = 0; sy < 8; ++sy) {
        var row = "";
        for (var sx = 0; sx < 8; ++sx) {
            var sv = gfx.sget(sx, sy);
            row += sv.toString(16);
        }
        gfx.print(row, 10, 130 + sy * 7, 5);
    }
}

function draw_poly_demo() {
    gfx.print("tri / poly / polyfill — shapes", 10, 30, 7);

    // Triangle outline
    gfx.tri(30, 100, 70, 50, 110, 100, 12);
    gfx.print("tri", 55, 105, 6);

    // Filled triangle
    gfx.trifill(130, 100, 170, 50, 210, 100, 11);
    gfx.print("trifill", 152, 105, 6);

    // Polygon outline (pentagon)
    var pts = [];
    for (var i = 0; i < 5; ++i) {
        var a = i / 5 - 0.25;
        pts.push(math.flr(60 + math.cos(a) * 25));
        pts.push(math.flr(150 + math.sin(a) * 25));
    }
    gfx.poly(pts, 9);
    gfx.print("poly", 45, 165, 6);

    // Filled polygon (hexagon)
    var pts2 = [];
    for (var j = 0; j < 6; ++j) {
        var a2 = j / 6;
        pts2.push(math.flr(170 + math.cos(a2) * 25));
        pts2.push(math.flr(150 + math.sin(a2) * 25));
    }
    gfx.polyfill(pts2, 14);
    gfx.print("polyfill", 148, 165, 6);

    // Rotating polygon
    var pts3 = [];
    for (var k = 0; k < 7; ++k) {
        var a3 = k / 7 + timer * 0.1;
        pts3.push(math.flr(270 + math.cos(a3) * 30));
        pts3.push(math.flr(100 + math.sin(a3) * 30));
    }
    gfx.polyfill(pts3, 8);
    gfx.poly(pts3, 2);
    gfx.print("rotating", 252, 135, 6);
}

function _draw() {
    gfx.cls(0);

    // Mode title bar
    gfx.rectfill(0, 0, 319, 12, 1);
    gfx.print(MODE_NAMES[demo_mode] + " (" + (demo_mode + 1) + "/" + NUM_MODES + ")", 4, 3, 7);
    gfx.print("</>  to cycle", 220, 3, 6);

    switch (demo_mode) {
        case 0:
            draw_pal_demo();
            break;
        case 1:
            draw_fillp_demo();
            break;
        case 2:
            draw_clip_demo();
            break;
        case 3:
            draw_font_demo();
            break;
        case 4:
            draw_pget_demo();
            break;
        case 5:
            draw_poly_demo();
            break;
    }

    draw_fps_widget();
}

function _save() {
    return {
        demo_mode: demo_mode,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    demo_mode = s.demo_mode;
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
}
