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

function _update(dt) {
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
}
