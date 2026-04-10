// System Explorer — sys.* introspection, file I/O, clipboard, lifecycle
//
// Demonstrates: sys.version, sys.platform, sys.width, sys.height,
//   sys.frame, sys.delta, sys.setTargetFps, sys.targetFps,
//   sys.volume, sys.fullscreen, sys.paused, sys.pause, sys.resume,
//   sys.log, sys.warn, sys.error, sys.config, sys.limit, sys.stat,
//   sys.textInput, sys.clipboardGet, sys.clipboardSet,
//   sys.readFile, sys.writeFile, sys.listFiles, sys.listDirs,
//   key.btnr, key.name, pad.count, pad.deadzone, pad.connected, pad.name

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
var PAGE_NAMES = ["SYSTEM INFO", "LIMITS & CONFIG", "FILE I/O", "GAMEPAD"];
var NUM_PAGES = PAGE_NAMES.length;

var file_content = "";
var file_list = [];
var dir_list = [];
var clipboard_text = "";
var text_input_enabled = false;
var typed_text = "";
var last_key_name = "";

// Build list of all key constants for scanning
var ALL_KEYS = [];
var KEY_PROPS = [
    "UP",
    "DOWN",
    "LEFT",
    "RIGHT",
    "Z",
    "X",
    "C",
    "V",
    "SPACE",
    "ENTER",
    "ESCAPE",
    "LSHIFT",
    "RSHIFT",
    "A",
    "B",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "W",
    "Y",
    "NUM0",
    "NUM1",
    "NUM2",
    "NUM3",
    "NUM4",
    "NUM5",
    "NUM6",
    "NUM7",
    "NUM8",
    "NUM9",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "BACKSPACE",
    "DELETE",
    "TAB",
    "HOME",
    "END",
    "PAGEUP",
    "PAGEDOWN",
    "LCTRL",
    "RCTRL",
    "LALT",
    "RALT",
    "GRAVE",
    "SLASH",
    "LGUI",
    "RGUI",
    "LBRACKET",
    "RBRACKET",
    "MINUS",
    "EQUALS",
];
for (var _ki = 0; _ki < KEY_PROPS.length; ++_ki) {
    var _kv = key[KEY_PROPS[_ki]];
    if (_kv !== undefined) ALL_KEYS.push({ code: _kv, name: KEY_PROPS[_ki] });
}

function _init() {
    sys.log("System Explorer started");
    sys.warn("This is a warning message");

    // Write a test file
    sys.writeFile("test_data.txt", "Hello from sys_explorer!\nLine 2\nLine 3");

    // Subscribe to text input events
    evt.on("text:input", function (e) {
        typed_text += e;
    });
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    // Page navigation with auto-repeating keys
    if (key.btnr(key.RIGHT)) {
        page = (page + 1) % NUM_PAGES;
    }
    if (key.btnr(key.LEFT)) {
        page = (page + NUM_PAGES - 1) % NUM_PAGES;
    }

    // Track last pressed key name (scan all known keys)
    for (var ki = 0; ki < ALL_KEYS.length; ++ki) {
        if (key.btnp(ALL_KEYS[ki].code)) {
            last_key_name = ALL_KEYS[ki].name + " (" + key.name(ALL_KEYS[ki].code) + ")";
        }
    }

    // Page-specific controls
    if (page === 0) {
        // +/- to change target FPS
        if (key.btnp(key.EQUALS)) {
            var fps = sys.targetFps();
            sys.setTargetFps(fps + 10);
        }
        if (key.btnp(key.MINUS)) {
            var fps2 = sys.targetFps();
            sys.setTargetFps(math.max(10, fps2 - 10));
        }
        // V to change volume
        if (key.btnp(key.V)) {
            var vol = sys.volume();
            sys.volume((vol + 0.25) % 1.25);
        }
        // F to toggle fullscreen
        if (key.btnp(key.F)) {
            sys.fullscreen(!sys.fullscreen());
        }
    }

    if (page === 2) {
        // R to read file
        if (key.btnp(key.R)) {
            file_content = sys.readFile("test_data.txt") || "(not found)";
        }
        // L to list files
        if (key.btnp(key.L)) {
            file_list = sys.listFiles() || [];
            dir_list = sys.listDirs() || [];
        }
        // W to write a new file with timestamp
        if (key.btnp(key.W)) {
            var data = "Written at frame " + math.flr(sys.frame());
            sys.writeFile("save_test.txt", data);
            sys.log("Wrote save_test.txt");
        }
        // C to read clipboard
        if (key.btnp(key.C)) {
            clipboard_text = sys.clipboardGet() || "(empty)";
        }
        // X to write clipboard
        if (key.btnp(key.X)) {
            sys.clipboardSet("dithr sys_explorer clipboard test");
            sys.log("Clipboard set");
        }
        // T to toggle text input
        if (key.btnp(key.T)) {
            text_input_enabled = !text_input_enabled;
            sys.textInput(text_input_enabled);
        }
    }
}

function draw_sys_info() {
    var y = 18;
    var gap = 8;

    gfx.print("Engine: " + sys.version(), 4, y, 7);
    y += gap;
    gfx.print("Platform: " + sys.platform(), 4, y, 7);
    y += gap;
    gfx.print("Display: " + sys.width() + "x" + sys.height(), 4, y, 7);
    y += gap;
    gfx.print("Frame: " + math.flr(sys.frame()), 4, y, 7);
    y += gap;
    gfx.print("Delta: " + sys.delta().toFixed(4) + "s", 4, y, 7);
    y += gap;
    gfx.print("Target FPS: " + sys.targetFps() + "  (+/- to change)", 4, y, 11);
    y += gap;
    gfx.print("Volume: " + sys.volume().toFixed(2) + "  (V to cycle)", 4, y, 11);
    y += gap;
    gfx.print("Fullscreen: " + sys.fullscreen() + "  (F to toggle)", 4, y, 11);
    y += gap;
    gfx.print("Paused: " + sys.paused(), 4, y, 7);
    y += gap;
    gfx.print("Time: " + sys.time().toFixed(1) + "s", 4, y, 7);
    y += gap;
    y += gap;
    gfx.print("stat(0) mem: " + sys.stat(0).toFixed(1), 4, y, 6);
    y += gap;
    gfx.print("stat(1) cpu: " + sys.stat(1).toFixed(3), 4, y, 6);
    y += gap;
    gfx.print("stat(7) fps: " + sys.stat(7).toFixed(1), 4, y, 6);
    y += gap;
    gfx.print("Last key: " + last_key_name + " (key.name)", 4, y, 6);
    y += gap;
}

function draw_limits() {
    var y = 18;
    var gap = 8;

    gfx.print("sys.limit() — compile-time limits:", 4, y, 7);
    y += gap + 2;

    var keys = [
        "fb_width",
        "fb_height",
        "palette_size",
        "max_sprites",
        "max_maps",
        "max_map_layers",
        "max_map_objects",
        "max_channels",
        "js_heap_mb",
        "js_stack_kb",
        "targetFps",
    ];
    for (var i = 0; i < keys.length; ++i) {
        var val = sys.limit(keys[i]);
        gfx.print(keys[i] + ": " + val, 10, y, 6);
        y += gap;
    }

    y += gap;
    gfx.print("sys.config() — cart config:", 4, y, 7);
    y += gap + 2;
    gfx.print("title: " + sys.config("meta.title"), 10, y, 6);
    y += gap;
    gfx.print("display.width: " + sys.config("display.width"), 10, y, 6);
    y += gap;
    gfx.print("timing.fps: " + sys.config("timing.fps"), 10, y, 6);
}

function draw_file_io() {
    var y = 18;
    var gap = 8;

    gfx.print("R=read file  W=write file  L=list files", 4, y, 11);
    y += gap;
    gfx.print("C=read clipboard  X=set clipboard  T=text input", 4, y, 11);
    y += gap + 4;

    gfx.print("File content:", 4, y, 7);
    y += gap;
    var lines = file_content.split("\n");
    for (var i = 0; i < math.min(lines.length, 4); ++i) {
        gfx.print("  " + lines[i], 4, y, 6);
        y += gap;
    }
    y += 4;

    gfx.print("Files: " + file_list.join(", "), 4, y, 7);
    y += gap;
    gfx.print("Dirs: " + dir_list.join(", "), 4, y, 7);
    y += gap + 4;

    gfx.print("Clipboard: " + clipboard_text.substring(0, 40), 4, y, 7);
    y += gap + 4;

    gfx.print(
        "Text input: " + (text_input_enabled ? "ON" : "OFF"),
        4,
        y,
        text_input_enabled ? 11 : 6,
    );
    y += gap;
    gfx.print("Typed: " + typed_text.substring(typed_text.length - 30), 4, y, 7);
}

function draw_gamepad() {
    var y = 18;
    var gap = 8;

    var cnt = pad.count();
    gfx.print("Gamepads connected: " + cnt, 4, y, 7);
    y += gap + 4;

    for (var i = 0; i < 4; ++i) {
        var connected = pad.connected(i);
        if (connected) {
            var name = pad.name(i);
            var dz = pad.deadzone(undefined, i);
            gfx.print("Pad " + i + ": " + name, 4, y, 11);
            y += gap;
            gfx.print("  deadzone: " + dz.toFixed(2), 4, y, 6);
            y += gap;
            gfx.print(
                "  LX: " +
                    pad.axis(pad.LX, i).toFixed(2) +
                    "  LY: " +
                    pad.axis(pad.LY, i).toFixed(2),
                4,
                y,
                6,
            );
            y += gap;
            gfx.print(
                "  RX: " +
                    pad.axis(pad.RX, i).toFixed(2) +
                    "  RY: " +
                    pad.axis(pad.RY, i).toFixed(2),
                4,
                y,
                6,
            );
            y += gap;

            // Button state
            var btns = ["A", "B", "X", "Y", "L1", "R1", "START", "SELECT"];
            var bvals = [pad.A, pad.B, pad.X, pad.Y, pad.L1, pad.R1, pad.START, pad.SELECT];
            var held = "";
            for (var b = 0; b < btns.length; ++b) {
                if (pad.btn(bvals[b], i)) held += btns[b] + " ";
            }
            gfx.print("  Held: " + (held || "none"), 4, y, 6);
            y += gap + 4;
        } else {
            gfx.print("Pad " + i + ": not connected", 4, y, 5);
            y += gap;
        }
    }
}

function _draw() {
    gfx.cls(0);

    // Page header
    gfx.rectfill(0, 0, 319, 10, 1);
    gfx.print(PAGE_NAMES[page] + " (" + (page + 1) + "/" + NUM_PAGES + ")  </>", 4, 2, 7);

    switch (page) {
        case 0:
            draw_sys_info();
            break;
        case 1:
            draw_limits();
            break;
        case 2:
            draw_file_io();
            break;
        case 3:
            draw_gamepad();
            break;
    }

    draw_fps_widget();
}

function _save() {
    return {
        page: page,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    page = s.page;
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
}
