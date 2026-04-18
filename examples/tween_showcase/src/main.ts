// Tween Showcase — easing functions, advanced math, and runtime input remapping
//
// Demonstrates: tween.start, tween.tick, tween.val, tween.done,
//   tween.cancel, tween.cancelAll, tween.ease,
//   math.smoothstep, math.pingpong, math.unlerp, math.remap,
//   math.mid, math.sign, math.ceil, math.round, math.trunc, math.pow,
//   math.rndRange, math.sin, math.cos, math.atan2,
//   input.map, input.axis, input.clear

// --- FPS widget ----------------------------------------------------------


// -------------------------------------------------------------------------

let page = 0;
const PAGE_NAMES = ['EASING CURVES', 'TWEEN POOL', 'MATH UTILS', 'INPUT REMAP'];
const NUM_PAGES = PAGE_NAMES.length;

// --- Easing curves page ---
const EASING_NAMES = [
    'linear',
    'easeIn',
    'easeOut',
    'easeInOut',
    'easeInQuad',
    'easeOutQuad',
    'easeInOutQuad',
    'easeInCubic',
    'easeOutCubic',
    'easeInOutCubic',
    'easeOutBack',
    'easeOutElastic',
    'easeOutBounce',
];
let curveT = 0;

// --- Tween pool page ---
const _tweens: number[] = [];
let balls: any[] = [];
const MAX_BALLS = 8;

// --- Math page ---
let mathTimer = 0;

// --- Input remap page ---
let remapObjX = 160;
let inputScheme = 0;
const SCHEME_NAMES = ['WASD', 'Arrow keys', 'IJKL'];
const SCHEMES = [
    { name: 'WASD', left: key.A, right: key.D },
    { name: 'Arrow keys', left: key.LEFT, right: key.RIGHT },
    { name: 'IJKL', left: key.J, right: key.L },
];

function applyInputScheme(idx: number) {
    input.clear('move_left');
    input.clear('move_right');
    input.map('move_left', [{ type: 0, code: SCHEMES[idx].left }]);
    input.map('move_right', [{ type: 0, code: SCHEMES[idx].right }]);
}

export function _init(): void {
    applyInputScheme(0);
}

export function _update(dt: number): void {

    // Page navigation (disabled on input remap page to avoid arrow key conflict)
    if (page !== 3) {
        if (key.btnp(key.RIGHT) && !key.btn(key.LSHIFT)) {
            page = (page + 1) % NUM_PAGES;
        }
        if (key.btnp(key.LEFT) && !key.btn(key.LSHIFT)) {
            page = (page + NUM_PAGES - 1) % NUM_PAGES;
        }
    } else {
        // On input page, use Shift+Arrow to switch pages
        if (key.btnp(key.RIGHT) && key.btn(key.LSHIFT)) {
            page = (page + 1) % NUM_PAGES;
        }
        if (key.btnp(key.LEFT) && key.btn(key.LSHIFT)) {
            page = (page + NUM_PAGES - 1) % NUM_PAGES;
        }
    }

    tween.tick(dt);

    if (page === 0) {
        // Animate the curve preview
        curveT += dt * 0.3;
        if (curveT > 1) curveT -= 1;
    }

    if (page === 1) {
        // Space spawns a new tween ball
        if (key.btnp(key.SPACE) && balls.length < MAX_BALLS) {
            const easeName = EASING_NAMES[math.rndInt(EASING_NAMES.length)];
            const dur = math.rndRange(2.0, 5.0);
            const startY = math.rndRange(30, 140);
            const tw = tween.start(10, 300, dur, easeName);
            balls.push({
                tw,
                ease: easeName,
                y: startY,
                col: math.rndInt(15) + 1,
            });
        }
        // C cancels all
        if (key.btnp(key.C)) {
            tween.cancelAll();
            balls = [];
        }
        // Remove done tweens
        for (let i = balls.length - 1; i >= 0; --i) {
            if (tween.done(balls[i].tw)) {
                balls.splice(i, 1);
            }
        }
    }

    if (page === 2) {
        mathTimer += dt;
    }

    if (page === 3) {
        // Tab cycles input scheme
        if (key.btnp(key.TAB)) {
            inputScheme = (inputScheme + 1) % SCHEMES.length;
            applyInputScheme(inputScheme);
        }
        // Move object with virtual input
        if (input.btn('move_left')) remapObjX -= 100 * dt;
        if (input.btn('move_right')) remapObjX += 100 * dt;
        remapObjX = math.clamp(remapObjX, 10, 310);

        // Also demonstrate input.axis if available
        const _ax = input.axis('move_right');
    }
}

function drawEasingPage() {
    const cols = 4;
    const _rows = math.ceil(EASING_NAMES.length / cols);
    const cw = 75;
    const ch = 36;
    const gap = 4;
    const ox = 6;
    const oy = 16;

    for (let i = 0; i < EASING_NAMES.length; ++i) {
        const cx = ox + (i % cols) * (cw + gap);
        const cy = oy + math.flr(i / cols) * (ch + gap + 8);

        // Draw curve background
        gfx.rect(cx, cy, cx + cw - 1, cy + ch - 1, 5);

        // Draw curve
        for (let px = 1; px < cw - 1; ++px) {
            const t = px / (cw - 2);
            const v = tween.ease(t, EASING_NAMES[i]);
            let py2 = cy + ch - 2 - math.flr(v * (ch - 4));
            py2 = math.clamp(py2, cy + 1, cy + ch - 2);
            gfx.pset(cx + px, py2, 11);
        }

        // Animated dot showing current position
        const cv = tween.ease(curveT, EASING_NAMES[i]);
        const dotX = cx + math.flr(curveT * (cw - 2));
        let dotY = cy + ch - 2 - math.flr(cv * (ch - 4));
        dotY = math.clamp(dotY, cy + 1, cy + ch - 2);
        gfx.circfill(dotX, dotY, 2, 8);

        // Label
        gfx.print(EASING_NAMES[i], cx, cy + ch + 1, 6);
    }
}

function drawTweenPage() {
    gfx.print(`SPACE=spawn ball  C=cancel all  pool:${balls.length}/${MAX_BALLS}`, 4, 16, 11);

    for (let i = 0; i < balls.length; ++i) {
        const b = balls[i];
        const x = tween.val(b.tw, 10);
        const done = tween.done(b.tw);
        gfx.circfill(math.flr(x), math.flr(b.y), 4, done ? 5 : b.col);
        gfx.print(b.ease, math.flr(x) + 6, math.flr(b.y) - 3, 6);
    }

    // Trail line
    gfx.line(10, 155, 300, 155, 5);
    gfx.print('start', 4, 158, 5);
    gfx.print('end', 290, 158, 5);
}

function drawMathPage() {
    let y = 16;
    const gap = 9;
    const t = (mathTimer * 0.3) % 1.0;

    gfx.print('Advanced math interpolation:', 4, y, 7);
    y += gap + 2;

    // smoothstep
    const ss = math.smoothstep(0, 1, t);
    gfx.print(`smoothstep(0,1,${t.toFixed(2)}) = ${ss.toFixed(3)}`, 4, y, 6);
    y += gap;

    // pingpong
    const pp = math.pingpong(mathTimer, 2.0);
    gfx.print(`pingpong(t, 2.0) = ${pp.toFixed(3)}`, 4, y, 6);
    y += gap;

    // unlerp
    const ul = math.unlerp(10, 50, 30);
    gfx.print(`unlerp(10, 50, 30) = ${ul.toFixed(3)}`, 4, y, 6);
    y += gap;

    // remap
    const rm = math.remap(t, 0, 1, -100, 100);
    gfx.print(`remap(${t.toFixed(2)}, 0,1, -100,100) = ${rm.toFixed(1)}`, 4, y, 6);
    y += gap;

    // mid
    const md = math.mid(3, 7, 5);
    gfx.print(`mid(3, 7, 5) = ${md}`, 4, y, 6);
    y += gap;

    // sign
    gfx.print(
        `sign(-42)=${math.sign(-42)}  sign(0)=${math.sign(0)}  sign(7)=${math.sign(7)}`,
        4,
        y,
        6,
    );
    y += gap;

    // ceil / round / trunc
    gfx.print(
        `ceil(3.2)=${math.ceil(3.2)}  round(3.5)=${math.round(3.5)}  trunc(3.9)=${math.trunc(3.9)}`,
        4,
        y,
        6,
    );
    y += gap;

    // pow
    gfx.print(`pow(2, 10) = ${math.pow(2, 10)}`, 4, y, 6);
    y += gap;

    // rndRange
    gfx.print(`rndRange(10, 20) = ${math.rndRange(10, 20).toFixed(2)}`, 4, y, 6);
    y += gap;

    // sin / cos (0-1 = full rotation)
    const angle = t;
    gfx.print(
        `sin(${angle.toFixed(2)})=${math.sin(angle).toFixed(3)}  cos=${math.cos(angle).toFixed(3)}`,
        4,
        y,
        6,
    );
    y += gap;

    // atan2
    const at = math.atan2(1, 1);
    gfx.print(`atan2(1, 1) = ${at.toFixed(4)}`, 4, y, 6);
    y += gap;

    // Visual: smoothstep vs linear bar
    y += 4;
    gfx.print('Linear vs Smoothstep:', 4, y, 7);
    y += gap;
    const barW = 200;
    gfx.rect(4, y, 4 + barW, y + 6, 5);
    gfx.rectfill(4, y, 4 + math.flr(t * barW), y + 6, 12);
    gfx.print('linear', 210, y, 6);
    y += 10;
    gfx.rect(4, y, 4 + barW, y + 6, 5);
    gfx.rectfill(4, y, 4 + math.flr(ss * barW), y + 6, 11);
    gfx.print('smooth', 210, y, 6);
}

function drawInputPage() {
    let y = 16;
    const gap = 9;

    gfx.print('input.map / input.clear — runtime remapping', 4, y, 7);
    y += gap + 2;
    gfx.print(`Current scheme: ${SCHEME_NAMES[inputScheme]}`, 4, y, 11);
    y += gap;
    gfx.print('Press TAB to cycle schemes  (Shift+</>  switch page)', 4, y, 6);
    y += gap + 4;

    for (let i = 0; i < SCHEMES.length; ++i) {
        const col = i === inputScheme ? 11 : 5;
        gfx.print((i === inputScheme ? '> ' : '  ') + SCHEME_NAMES[i], 10, y, col);
        y += gap;
    }

    y += gap;

    // Moving object
    gfx.rectfill(remapObjX - 8, 120, remapObjX + 8, 136, 12);
    gfx.rect(remapObjX - 8, 120, remapObjX + 8, 136, 7);
    gfx.print('<  MOVE  >', remapObjX - 20, 140, 6);

    // input.btn state
    const heldL = input.btn('move_left');
    const heldR = input.btn('move_right');
    gfx.print(`move_left: ${heldL ? 'HELD' : '---'}`, 4, 155, heldL ? 8 : 5);
    gfx.print(`move_right: ${heldR ? 'HELD' : '---'}`, 120, 155, heldR ? 8 : 5);
}

export function _draw(): void {
    gfx.cls(0);

    gfx.rectfill(0, 0, 319, 10, 1);
    gfx.print(`${PAGE_NAMES[page]} (${page + 1}/${NUM_PAGES})  </>`, 4, 2, 7);

    switch (page) {
        case 0:
            drawEasingPage();
            break;
        case 1:
            drawTweenPage();
            break;
        case 2:
            drawMathPage();
            break;
        case 3:
            drawInputPage();
            break;
        default:
            break;
    }

    sys.drawFps();
}

export function _save() {
    return {
        page,
        remapObjX,
        inputScheme,
    };
}

export function _restore(s: any): void {
    page = s.page;
    remapObjX = s.remapObjX;
    inputScheme = s.inputScheme;
    applyInputScheme(inputScheme);
}
