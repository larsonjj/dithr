// Transition Demo — screen transitions (fade, wipe, dissolve)
//
// Demonstrates: gfx.fade, gfx.wipe, gfx.dissolve, gfx.transitioning

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
// Scenes
// -------------------------------------------------------------------------

var SCENES = [
    { name: "TITLE", bg: 1, fg: 7, text: "TRANSITION DEMO" },
    { name: "GAMEPLAY", bg: 0, fg: 11, text: "Gameplay Scene" },
    { name: "GAMEOVER", bg: 2, fg: 8, text: "Game Over" },
    { name: "CREDITS", bg: 5, fg: 13, text: "Credits" },
];

var scene_idx = 0;
var scene_timer = 0;
var transition_type = 0; // 0=fade, 1=wipe(L), 2=wipe(R), 3=wipe(U), 4=wipe(D), 5=dissolve
var TRANSITION_NAMES = ["fade", "wipe left", "wipe right", "wipe up", "wipe down", "dissolve"];
var TRANS_FRAMES = 120;
var HOLD_TIME = 0.3; // seconds to pause on black between out and in
var phase = "idle"; // "idle", "out", "hold", "in"
var queued_scene = -1;
var in_frame = 0; // manual counter for the "in" phase
var hold_timer = 0;
var stars = [];

function build_stars() {
    stars = [];
    for (var i = 0; i < 60; ++i) {
        stars.push({
            x: math.rnd_int(320),
            y: math.rnd_int(180),
            spd: math.rnd(0.3) + 0.1,
            col: math.rnd(1) > 0.5 ? 7 : 6,
        });
    }
}

// Simple hash for dissolve pixel selection (matches engine algorithm)
function hash_pixel(x, y) {
    var h = (x * 374761393 + y * 668265263 + 42 * 2147483647) | 0;
    h = Math.imul((h ^ (h >>> 13)) | 0, 1274126177);
    h = (h ^ (h >>> 16)) >>> 0;
    return h;
}

function fire_transition_out(frames) {
    var col = 0;
    switch (transition_type) {
        case 0:
            gfx.fade(col, frames);
            break;
        case 1:
            gfx.wipe(0, col, frames);
            break;
        case 2:
            gfx.wipe(1, col, frames);
            break;
        case 3:
            gfx.wipe(2, col, frames);
            break;
        case 4:
            gfx.wipe(3, col, frames);
            break;
        case 5:
            gfx.dissolve(col, frames);
            break;
    }
}

function start_transition(target_scene) {
    if (phase !== "idle") return;
    queued_scene = target_scene;
    phase = "out";
    fire_transition_out(TRANS_FRAMES);
}

function _init() {
    build_stars();
}

function _update(dt) {
    smooth_fps = math.lerp(smooth_fps, sys.fps(), 0.05);
    fps_history[fps_hist_idx] = smooth_fps;
    fps_hist_idx = (fps_hist_idx + 1) % FPS_HIST_LEN;

    // Move stars
    for (var i = 0; i < stars.length; ++i) {
        stars[i].x -= stars[i].spd;
        if (stars[i].x < 0) {
            stars[i].x = 320;
            stars[i].y = math.rnd_int(180);
        }
    }

    // Phase machine: out → hold (black) → in → idle
    if (phase === "out" && !gfx.transitioning()) {
        scene_idx = queued_scene;
        phase = "hold";
        hold_timer = HOLD_TIME;
    } else if (phase === "hold") {
        hold_timer -= dt;
        if (hold_timer <= 0) {
            phase = "in";
            in_frame = 0;
        }
    } else if (phase === "in") {
        in_frame++;
        if (in_frame >= TRANS_FRAMES) phase = "idle";
    }

    if (phase !== "idle") return;

    // Cycle transition type: T
    if (key.btnp(key.T)) {
        transition_type = (transition_type + 1) % TRANSITION_NAMES.length;
    }

    // Number keys 1-4 select scene directly
    if (key.btnp(key.NUM1)) start_transition(0);
    if (key.btnp(key.NUM2)) start_transition(1);
    if (key.btnp(key.NUM3)) start_transition(2);
    if (key.btnp(key.NUM4)) start_transition(3);

    // Space = cycle to next scene
    if (key.btnp(key.SPACE)) {
        start_transition((scene_idx + 1) % SCENES.length);
    }

    scene_timer += dt;
}

function _draw() {
    var sc = SCENES[scene_idx];
    gfx.cls(sc.bg);

    // Stars background
    for (var i = 0; i < stars.length; ++i) {
        gfx.pset(math.flr(stars[i].x), math.flr(stars[i].y), stars[i].col);
    }

    // Scene content
    var tw = gfx.textWidth(sc.text);
    gfx.print(sc.text, math.flr((320 - tw) / 2), 60, sc.fg);

    // Scene-specific decoration
    if (scene_idx === 0) {
        gfx.print("Press SPACE to begin", 100, 80, 6);
        // Animated rectangle
        var w = 60 + math.sin(scene_timer * 0.3) * 20;
        gfx.rect(130, 100, 130 + math.flr(w), 120, 12);
    } else if (scene_idx === 1) {
        // Fake gameplay: bouncing ball
        var bx = 160 + math.cos(scene_timer * 0.2) * 60;
        var by = 100 + math.sin(scene_timer * 0.15) * 30;
        gfx.circfill(math.flr(bx), math.flr(by), 6, 11);
        gfx.circ(math.flr(bx), math.flr(by), 6, 3);
    } else if (scene_idx === 2) {
        gfx.print("Score: 12345", 130, 80, 7);
        gfx.print("Press SPACE to continue", 90, 100, 6);
    } else if (scene_idx === 3) {
        gfx.print("Made with dithr", 120, 80, 7);
        gfx.print("github.com/larsonjj/dithr", 90, 100, 6);
    }

    // HUD: transition type and status
    var status = phase !== "idle" ? phase.toUpperCase() : "ready";
    gfx.rectfill(0, 168, 319, 179, 0);
    gfx.print("T=cycle type  SPACE=next  1-4=goto scene", 2, 170, 5);
    gfx.print("[" + TRANSITION_NAMES[transition_type] + "] " + status, 2, 160, 6);

    draw_fps_widget();

    // During "hold", ensure solid black overlay so there's no flicker
    if (phase === "hold") {
        gfx.rectfill(0, 0, 319, 179, 0);
    }

    // Manual "in" overlay — draw shrinking black mask over the new scene
    if (phase === "in") {
        var t = in_frame / TRANS_FRAMES; // 0→1 = fully black → fully clear

        if (transition_type === 0) {
            // ── Fade in: dithered overlay that thins out ──
            // NOTE: 0x0000 is excluded — the engine treats fillp(0) as solid fill
            var patterns = [
                0xffff, 0x7fff, 0x7fbf, 0x7baf, 0x5aaf, 0x5aa5, 0x5a25, 0x5224, 0x5024, 0x4024,
                0x4020, 0x0020, 0x0400,
            ];
            var level = math.flr(t * (patterns.length + 1));
            if (level < patterns.length) {
                gfx.fillp(patterns[level]);
                gfx.rectfill(0, 0, 319, 179, 0);
                gfx.fillp(0);
            }
        } else if (transition_type >= 1 && transition_type <= 4) {
            // ── Wipe in: black rect receding in same direction it arrived ──
            // Out phase covered the screen; in phase uncovers from the
            // opposite edge so it looks like the black is sliding away.
            //   wipe left  out: black grows from left  → in: continues right, reveals from left
            //   wipe right out: black grows from right → in: continues left, reveals from right
            //   wipe up    out: black grows from top   → in: continues down, reveals from top
            //   wipe down  out: black grows from bot   → in: continues up, reveals from bottom
            var remain = 1.0 - t; // 1→0
            if (transition_type === 1) {
                // wipe left: black stays on right side, shrinks rightward
                var lim = math.flr(320 * remain);
                if (lim > 0) gfx.rectfill(320 - lim, 0, 319, 179, 0);
            } else if (transition_type === 2) {
                // wipe right: black stays on left side, shrinks leftward
                var lim = math.flr(320 * remain);
                if (lim > 0) gfx.rectfill(0, 0, lim - 1, 179, 0);
            } else if (transition_type === 3) {
                // wipe up: black stays on bottom, shrinks downward
                var lim = math.flr(180 * remain);
                if (lim > 0) gfx.rectfill(0, 180 - lim, 319, 179, 0);
            } else if (transition_type === 4) {
                // wipe down: black stays on top, shrinks upward
                var lim = math.flr(180 * remain);
                if (lim > 0) gfx.rectfill(0, 0, 319, lim - 1, 0);
            }
        } else if (transition_type === 5) {
            // ── Dissolve in: pixel-by-pixel reveal using same hash ──
            // At t=0 all pixels are black; at t=1 all are revealed.
            // We draw black on pixels whose hash is ABOVE the threshold.
            var threshold = (t * 4294967295) >>> 0;
            for (var py = 0; py < 180; py += 2) {
                for (var px = 0; px < 320; px += 2) {
                    if (hash_pixel(px, py) > threshold) {
                        gfx.rectfill(px, py, px + 1, py + 1, 0);
                    }
                }
            }
        }
    }
}

function _save() {
    return {
        scene_idx: scene_idx,
        transition_type: transition_type,
        phase: phase,
        in_frame: in_frame,
        hold_timer: hold_timer,
        smooth_fps: smooth_fps,
        fps_history: fps_history,
        fps_hist_idx: fps_hist_idx,
    };
}

function _restore(s) {
    scene_idx = s.scene_idx;
    transition_type = s.transition_type;
    phase = s.phase || "idle";
    in_frame = s.in_frame || 0;
    hold_timer = s.hold_timer || 0;
    smooth_fps = s.smooth_fps;
    fps_history = s.fps_history;
    fps_hist_idx = s.fps_hist_idx;
}
