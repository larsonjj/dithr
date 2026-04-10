// Event Demo — event bus and system events
//
// Demonstrates: evt.on, evt.once, evt.off, evt.emit,
//   system events (sys:pause, sys:resume, sys:focus_lost, sys:focus_gained)

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
// Game state — a tiny coin-collecting game driven entirely by events
// -------------------------------------------------------------------------

var score = 0;
var coins = [];
var MAX_COINS = 8;
var player_x = 160;
var player_y = 90;
var SPEED = 80;

// Event log displayed on screen
var event_log = [];
var MAX_LOG = 12;
var handle_score = -1;
var handle_spawn = -1;
var combo = 0;
var combo_handle = -1;

function log_event(msg) {
    event_log.push({ text: msg, age: 0 });
    if (event_log.length > MAX_LOG) event_log.shift();
}

function spawn_coin() {
    if (coins.length >= MAX_COINS) return;
    coins.push({
        x: math.rnd_int(304) + 8,
        y: math.rnd_int(152) + 20,
        collected: false,
    });
}

function _init() {
    // --- Custom game events ---

    // Score handler — persistent
    handle_score = evt.on("coin:collect", function (e) {
        score += e.value;
        combo++;
        log_event("coin:collect  score=" + score + " combo=" + combo);
    });

    // Coin respawn — persistent
    handle_spawn = evt.on("coin:collect", function () {
        // Queue a new spawn event
        evt.emit("coin:spawn");
    });

    // Spawn handler
    evt.on("coin:spawn", function () {
        spawn_coin();
        log_event("coin:spawn  total=" + (coins.length + 1));
    });

    // Combo reset — one-shot, re-registered each time
    function register_combo_reset() {
        combo_handle = evt.once("combo:reset", function () {
            log_event("combo:reset  was=" + combo);
            combo = 0;
            register_combo_reset(); // re-register
        });
    }
    register_combo_reset();

    // --- System events ---
    evt.on("sys:pause", function () {
        log_event(">>> sys:pause");
    });
    evt.on("sys:resume", function () {
        log_event(">>> sys:resume");
    });
    evt.on("sys:focus_lost", function () {
        log_event(">>> sys:focus_lost");
    });
    evt.on("sys:focus_gained", function () {
        log_event(">>> sys:focus_gained");
    });

    // Spawn initial coins
    for (var i = 0; i < 5; ++i) spawn_coin();

    log_event("Game started! Collect coins with arrow keys.");
    log_event("Press O to evt.off score handler, R to re-register.");
}

var combo_timer = 0;

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    // Age log entries
    for (var i = 0; i < event_log.length; ++i) {
        event_log[i].age += dt;
    }

    // Movement
    if (input.btn("left")) player_x -= SPEED * dt;
    if (input.btn("right")) player_x += SPEED * dt;
    if (input.btn("up")) player_y -= SPEED * dt;
    if (input.btn("down")) player_y += SPEED * dt;
    player_x = math.clamp(player_x, 0, 312);
    player_y = math.clamp(player_y, 0, 172);

    // Check coin collisions
    for (var c = coins.length - 1; c >= 0; --c) {
        if (
            !coins[c].collected &&
            col.rect(player_x, player_y, 8, 8, coins[c].x, coins[c].y, 6, 6)
        ) {
            coins[c].collected = true;
            coins.splice(c, 1);
            evt.emit("coin:collect", { value: 10 });
            combo_timer = 1.5;
        }
    }

    // Combo timeout
    if (combo > 0) {
        combo_timer -= dt;
        if (combo_timer <= 0) {
            evt.emit("combo:reset");
        }
    }

    // Toggle score handler with O key
    if (key.btnp(key.O)) {
        if (handle_score >= 0) {
            evt.off(handle_score);
            handle_score = -1;
            log_event("evt.off(score handler) — coins won't add score!");
        }
    }

    // Re-register score handler with R key
    if (key.btnp(key.R)) {
        if (handle_score < 0) {
            handle_score = evt.on("coin:collect", function (e) {
                score += e.value;
                combo++;
                log_event("coin:collect  score=" + score + " combo=" + combo);
            });
            log_event("Re-registered score handler");
        }
    }
}

function _draw() {
    gfx.cls(1);

    // Draw coins
    for (var c = 0; c < coins.length; ++c) {
        gfx.circfill(coins[c].x + 3, coins[c].y + 3, 3, 10);
        gfx.circ(coins[c].x + 3, coins[c].y + 3, 3, 9);
    }

    // Draw player
    gfx.rectfill(player_x, player_y, player_x + 7, player_y + 7, 8);
    gfx.rect(player_x, player_y, player_x + 7, player_y + 7, 2);

    // Score / combo HUD
    gfx.rectfill(0, 0, 160, 9, 0);
    gfx.print("SCORE: " + score + "  COMBO: " + combo, 2, 1, 7);

    // Event log panel
    var lx = 1;
    var ly = 180 - MAX_LOG * 7 - 2;
    gfx.rectfill(0, ly - 1, 200, 179, 0);
    gfx.print("-- event log --", lx, ly - 8, 6);
    for (var i = 0; i < event_log.length; ++i) {
        var entry = event_log[i];
        var c2 = entry.age < 0.5 ? 7 : entry.age < 2 ? 6 : 5;
        gfx.print(entry.text, lx, ly + i * 7, c2);
    }

    // Controls hint
    gfx.print("O=off score  R=re-register", 2, 172, 5);

    draw_fps_widget();
}

function _save() {
    return {
        score: score,
        player_x: player_x,
        player_y: player_y,
        coins: coins,
        combo: combo,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    score = s.score;
    player_x = s.player_x;
    player_y = s.player_y;
    coins = s.coins;
    combo = s.combo;
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
}
