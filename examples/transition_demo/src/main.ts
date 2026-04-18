// Transition Demo — screen transitions (fade, wipe, dissolve)
//
// Demonstrates: gfx.fade, gfx.wipe, gfx.dissolve, gfx.transitioning

// --- FPS widget ----------------------------------------------------------

// -------------------------------------------------------------------------
// Scenes
// -------------------------------------------------------------------------

const SCENES = [
    { name: 'TITLE', bg: 1, fg: 7, text: 'TRANSITION DEMO' },
    { name: 'GAMEPLAY', bg: 0, fg: 11, text: 'Gameplay Scene' },
    { name: 'GAMEOVER', bg: 2, fg: 8, text: 'Game Over' },
    { name: 'CREDITS', bg: 5, fg: 13, text: 'Credits' },
];

let sceneIdx = 0;
let sceneTimer = 0;
let transitionType = 0; // 0=fade, 1=wipe(L), 2=wipe(R), 3=wipe(U), 4=wipe(D), 5=dissolve
const TRANSITION_NAMES = ['fade', 'wipe left', 'wipe right', 'wipe up', 'wipe down', 'dissolve'];
const TRANS_DURATION = 2.0; // seconds
const HOLD_TIME = 0.3; // seconds to pause on black between out and in
let phase = 'idle'; // "idle", "out", "hold", "in"
let queuedScene = -1;
let transFrames = 0; // frame count for both out and in phases
let inFrame = 0; // render-frame counter for the "in" phase
let holdTimer = 0;
let stars: any[] = [];

function buildStars() {
    stars = [];
    for (let i = 0; i < 60; ++i) {
        stars.push({
            x: math.rndInt(320),
            y: math.rndInt(180),
            spd: math.rnd(0.3) + 0.1,
            col: math.rnd(1) > 0.5 ? 7 : 6,
        });
    }
}

function fireTransitionOut(frames: number) {
    const col = 0;
    switch (transitionType) {
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
        default:
            break;
    }
}

function startTransition(targetScene: number) {
    if (phase !== 'idle') return;
    queuedScene = targetScene;
    phase = 'out';
    transFrames = math.flr(TRANS_DURATION * sys.targetFps());
    fireTransitionOut(transFrames);
}

export function _init(): void {
    buildStars();
}

export function _update(dt: number): void {
    // Move stars
    for (let i = 0; i < stars.length; ++i) {
        stars[i].x -= stars[i].spd;
        if (stars[i].x < 0) {
            stars[i].x = 320;
            stars[i].y = math.rndInt(180);
        }
    }

    // Phase machine: out → hold (black) → in → idle
    if (phase === 'out' && !gfx.transitioning()) {
        sceneIdx = queuedScene;
        phase = 'hold';
        holdTimer = HOLD_TIME;
    } else if (phase === 'hold') {
        holdTimer -= dt;
        if (holdTimer <= 0) {
            phase = 'in';
            inFrame = 0;
        }
    }

    if (phase !== 'idle') return;

    // Cycle transition type: T
    if (key.btnp(key.T)) {
        transitionType = (transitionType + 1) % TRANSITION_NAMES.length;
    }

    // Number keys 1-4 select scene directly
    if (key.btnp(key.NUM1)) startTransition(0);
    if (key.btnp(key.NUM2)) startTransition(1);
    if (key.btnp(key.NUM3)) startTransition(2);
    if (key.btnp(key.NUM4)) startTransition(3);

    // Space = cycle to next scene
    if (key.btnp(key.SPACE)) {
        startTransition((sceneIdx + 1) % SCENES.length);
    }

    sceneTimer += dt;
}

export function _draw(): void {
    const sc = SCENES[sceneIdx];
    gfx.cls(sc.bg);

    // Stars background
    for (let i = 0; i < stars.length; ++i) {
        gfx.pset(math.flr(stars[i].x), math.flr(stars[i].y), stars[i].col);
    }

    // Scene content
    const tw = gfx.textWidth(sc.text);
    gfx.print(sc.text, math.flr((320 - tw) / 2), 60, sc.fg);

    // Scene-specific decoration
    if (sceneIdx === 0) {
        gfx.print('Press SPACE to begin', 100, 80, 6);
        // Animated rectangle
        const w = 60 + math.sin(sceneTimer * 0.3) * 20;
        gfx.rect(130, 100, 130 + math.flr(w), 120, 12);
    } else if (sceneIdx === 1) {
        // Fake gameplay: bouncing ball
        const bx = 160 + math.cos(sceneTimer * 0.2) * 60;
        const by = 100 + math.sin(sceneTimer * 0.15) * 30;
        gfx.circfill(math.flr(bx), math.flr(by), 6, 11);
        gfx.circ(math.flr(bx), math.flr(by), 6, 3);
    } else if (sceneIdx === 2) {
        gfx.print('Score: 12345', 130, 80, 7);
        gfx.print('Press SPACE to continue', 90, 100, 6);
    } else if (sceneIdx === 3) {
        gfx.print('Made with dithr', 120, 80, 7);
        gfx.print('github.com/larsonjj/dithr', 90, 100, 6);
    }

    // HUD: transition type and status
    const status = phase !== 'idle' ? phase.toUpperCase() : 'ready';
    gfx.rectfill(0, 168, 319, 179, 0);
    gfx.print('T=cycle type  SPACE=next  1-4=goto scene', 2, 170, 5);
    gfx.print(`[${TRANSITION_NAMES[transitionType]}] ${status}`, 2, 160, 6);

    sys.drawFps();

    // During "hold", ensure solid black overlay so there's no flicker
    if (phase === 'hold') {
        gfx.rectfill(0, 0, 319, 179, 0);
    }

    // Manual "in" overlay — draw shrinking black mask over the new scene
    if (phase === 'in') {
        inFrame++;
        const t = math.min(inFrame / transFrames, 1.0); // 0→1 = fully black → fully clear
        if (inFrame >= transFrames) phase = 'idle';

        // Dithered pattern table: each entry is a fill pattern with decreasing
        // coverage, used for fade-in and dissolve-in alike.
        const patterns = [
            0xffff, 0x7fff, 0x7fbf, 0x7baf, 0x5aaf, 0x5aa5, 0x5a25, 0x5224, 0x5024, 0x4024, 0x4020,
            0x0020, 0x0400,
        ];

        if (transitionType === 0 || transitionType === 5) {
            // ── Fade / Dissolve in: dithered overlay that thins out ──
            const level = math.flr(t * (patterns.length + 1));
            if (level < patterns.length) {
                gfx.fillp(patterns[level]);
                gfx.rectfill(0, 0, 319, 179, 0);
                gfx.fillp(0);
            }
        } else if (transitionType >= 1 && transitionType <= 4) {
            // ── Wipe in: black rect receding in same direction it arrived ──
            // Out phase covered the screen; in phase uncovers from the
            // opposite edge so it looks like the black is sliding away.
            //   wipe left  out: black grows from left  → in: continues right, reveals from left
            //   wipe right out: black grows from right → in: continues left, reveals from right
            //   wipe up    out: black grows from top   → in: continues down, reveals from top
            //   wipe down  out: black grows from bot   → in: continues up, reveals from bottom
            const remain = 1.0 - t; // 1→0
            if (transitionType === 1) {
                // wipe left: black stays on right side, shrinks rightward
                const lim = math.flr(320 * remain);
                if (lim > 0) gfx.rectfill(320 - lim, 0, 319, 179, 0);
            } else if (transitionType === 2) {
                // wipe right: black stays on left side, shrinks leftward
                const lim = math.flr(320 * remain);
                if (lim > 0) gfx.rectfill(0, 0, lim - 1, 179, 0);
            } else if (transitionType === 3) {
                // wipe up: black stays on bottom, shrinks downward
                const lim = math.flr(180 * remain);
                if (lim > 0) gfx.rectfill(0, 180 - lim, 319, 179, 0);
            } else if (transitionType === 4) {
                // wipe down: black stays on top, shrinks upward
                const lim = math.flr(180 * remain);
                if (lim > 0) gfx.rectfill(0, 0, 319, lim - 1, 0);
            }
        }
    }
}

export function _save() {
    return {
        sceneIdx,
        transitionType,
        phase,
        transFrames,
        inFrame,
        holdTimer,
    };
}

export function _restore(s: any): void {
    sceneIdx = s.sceneIdx;
    transitionType = s.transitionType;
    phase = s.phase || 'idle';
    transFrames = s.transFrames || 0;
    inFrame = s.inFrame || 0;
    holdTimer = s.holdTimer || 0;
}
