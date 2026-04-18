// PostFX Demo
//
// Shows how to:
//   - Push / pop effects with postfx.push() / postfx.pop()
//   - Use postfx.use() for named activation
//   - Stack multiple effects with postfx.stack()
//   - Adjust intensity and parameters
//   - Query available effects
//   - Save / restore the effect stack
//
// Number keys 1-4 toggle individual effects.
// S = stack all.  C = clear.  +/- = adjust strength.

// --- State -----------------------------------------------------------

let effects: string[]; // filled from postfx.available()
const active: Record<string, boolean> = {}; // name -> bool
let strength = 1.0;
let t = 0; // time accumulator for the demo scene
const debugFlash = [0, 0, 0, 0];

// --- Callbacks -------------------------------------------------------



export function _init(): void {
    effects = postfx.available();
    for (let i = 0; i < effects.length; ++i) {
        active[effects[i]] = false;
    }
}

export function _update(dt: number): void {
    t += dt;

    // Decay flash timers
    for (let f = 0; f < 4; ++f) {
        if (debugFlash[f] > 0) debugFlash[f] -= dt;
    }

    // Toggle effects: Z=CRT, X=scanlines, C=bloom, V=aberration
    if (key.btnp(key.Z)) {
        toggle('crt');
        debugFlash[0] = 0.3;
    }
    if (key.btnp(key.X)) {
        toggle('scanlines');
        debugFlash[1] = 0.3;
    }
    if (key.btnp(key.C)) {
        toggle('bloom');
        debugFlash[2] = 0.3;
    }
    if (key.btnp(key.V)) {
        toggle('aberration');
        debugFlash[3] = 0.3;
    }

    // S = stack all active at once
    if (key.btnp(key.SPACE)) {
        const names = [];
        for (let i = 1; i < effects.length; ++i) {
            // skip "none"
            names.push(effects[i]);
            active[effects[i]] = true;
        }
        postfx.stack(names);
        return;
    }

    // E = clear all
    if (key.btnp(key.E)) {
        postfx.clear();
        Object.keys(active).forEach((k) => {
            active[k] = false;
        });
    }

    // Strength control
    if (key.btnp(key.UP)) {
        strength = math.flr(math.clamp(strength + 0.1, 0.1, 2.0) * 10 + 0.5) / 10;
        rebuildStack();
    }
    if (key.btnp(key.DOWN)) {
        strength = math.flr(math.clamp(strength - 0.1, 0.1, 2.0) * 10 + 0.5) / 10;
        rebuildStack();
    }
}

function toggle(name: string) {
    active[name] = !active[name];
    rebuildStack();
}

function rebuildStack() {
    postfx.clear();
    for (let i = 1; i < effects.length; ++i) {
        const name = effects[i];
        if (active[name]) {
            postfx.push(i, strength);
        }
    }
}

export function _draw(): void {
    gfx.cls(1);

    // --- Draw a colourful demo scene for the effects to act on ---
    drawDemoScene();

    // --- HUD (drawn on top, affected by postfx too) ---
    gfx.rectfill(0, 0, 319, 40, 0);
    gfx.print('=== postfx demo ===', 100, 2, 7);

    // Show which effects are active + key debug
    const labels = ['crt', 'scanlines', 'bloom', 'aberration'];
    const keycodes = [key.Z, key.X, key.C, key.V];
    const keynames = ['Z', 'X', 'C', 'V'];
    for (let i = 0; i < labels.length; ++i) {
        const on = active[labels[i]];
        const held = key.btn(keycodes[i]);
        let col = on ? 11 : 5;
        // Flash yellow on press
        if (debugFlash[i] > 0) col = 10;
        const lx = 4 + i * 78;
        gfx.print(`${keynames[i]}:${labels[i]}`, lx, 12, col);
        // Show held indicator
        gfx.rectfill(lx, 20, lx + 4, 24, held ? 11 : 2);
    }

    gfx.print(`strength: ${math.flr(strength * 100)}%`, 4, 26, 7);
    gfx.print('up/dn=strength  spc=all  e=clear', 4, 34, 5);

    // FPS widget
    sys.drawFps();
}

// A colourful test scene: gradient, shapes, text
function drawDemoScene() {
    // Colour bars
    for (let i = 0; i < 16; ++i) {
        gfx.rectfill(i * 20, 44, i * 20 + 19, 64, i);
    }

    // Bouncing circles
    for (let i = 0; i < 5; ++i) {
        const cx = 40 + i * 60;
        const cy = 110 + math.sin(t * 0.3 + i * 0.2) * 30;
        gfx.circfill(cx, cy, 12, 8 + i);
        gfx.circ(cx, cy, 12, 7);
    }

    // Moving text
    const tx = 80 + math.sin(t * 0.15) * 60;
    gfx.print('dithr postfx', tx, 80, 7);

    // Diagonal lines
    for (let i = 0; i < 8; ++i) {
        gfx.line(0, 70 + i * 15, 319, 100 + i * 15, 2 + (i % 6));
    }

    // Checkerboard in corner
    for (let y = 150; y < 180; y += 4) {
        for (let x = 240; x < 320; x += 4) {
            const c = (((x / 4) | 0) + ((y / 4) | 0)) % 2 === 0 ? 7 : 0;
            gfx.rectfill(x, y, x + 3, y + 3, c);
        }
    }
}
