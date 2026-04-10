// Persistence Demo
//
// Shows how to:
//   - Save / load key-value data with cart.save() / cart.load()
//   - Use numeric data slots with cart.dset() / cart.dget()
//   - Check for key existence with cart.has()
//   - Delete keys with cart.delete()
//   - Read cart metadata with cart.title() / cart.author() / cart.version()
//
// A simple click-counter and high-score that persist across restarts.

// --- State -----------------------------------------------------------

var click_count = 0;
var high_score = 0;
var player_name = "";
var status_msg = "";
var status_timer = 0;

// --- Helpers ---------------------------------------------------------

function prv_status(msg) {
    status_msg = msg;
    status_timer = 120; // 2 seconds at 60fps
}

// --- Callbacks -------------------------------------------------------

function _init() {
    // Restore persisted state
    if (cart.has("click_count")) {
        click_count = parseInt(cart.load("click_count")) || 0;
    }
    if (cart.has("player_name")) {
        player_name = cart.load("player_name");
    }

    // Numeric slot 0 stores the high score
    high_score = cart.dget(0);
    if (high_score === undefined) high_score = 0;

    prv_status("loaded! clicks=" + click_count + " high=" + high_score);
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

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;
    if (status_timer > 0) --status_timer;

    // Z = click (increment counter)
    if (key.btnp(key.Z)) {
        ++click_count;
        cart.save("click_count", "" + click_count);

        if (click_count > high_score) {
            high_score = click_count;
            cart.dset(0, high_score);
            prv_status("new high score: " + high_score + "!");
        } else {
            prv_status("click! (" + click_count + ")");
        }
    }

    // X = set player name to "Player" (simulates text entry)
    if (key.btnp(key.X)) {
        player_name = "Player_" + math.rnd_int(1000);
        cart.save("player_name", player_name);
        prv_status("name saved: " + player_name);
    }

    // C = reset (delete all saved data)
    if (key.btnp(key.C)) {
        cart.delete("click_count");
        cart.delete("player_name");
        cart.dset(0, 0);
        click_count = 0;
        high_score = 0;
        player_name = "";
        prv_status("all data cleared!");
    }
}

function _draw() {
    gfx.cls(1);

    // --- Cart metadata ---
    gfx.print("=== persistence demo ===", 80, 4, 7);
    gfx.print("cart: " + cart.title() + " v" + cart.version(), 4, 16, 6);
    gfx.print("by: " + (cart.author() || "(no author)"), 4, 24, 6);

    // --- Saved data display ---
    gfx.rectfill(4, 38, 220, 100, 0);
    gfx.rect(4, 38, 220, 100, 5);

    gfx.print("saved data:", 8, 42, 7);

    gfx.print("click_count: " + click_count, 8, 54, 11);
    gfx.print("high_score:  " + high_score, 8, 62, 10);
    gfx.print("player_name: " + (player_name || "(not set)"), 8, 70, 12);

    gfx.print("has(click_count): " + cart.has("click_count"), 8, 82, 5);
    gfx.print("has(player_name): " + cart.has("player_name"), 8, 90, 5);

    // --- Numeric slots ---
    gfx.print("numeric slots:", 4, 110, 6);
    for (var i = 0; i < 4; ++i) {
        var val = cart.dget(i);
        gfx.print("slot[" + i + "] = " + (val !== undefined ? val : "empty"), 8, 120 + i * 8, 7);
    }

    // --- Status message ---
    if (status_timer > 0) {
        gfx.print(status_msg, 4, 156, 10);
    }

    // --- Controls ---
    gfx.print("z=click  x=set name  c=reset", 4, 170, 5);

    // FPS widget
    draw_fps_widget();
}
