// Collision Demo — all col.* collision helpers with interactive shapes
//
// Demonstrates: col.rect, col.circ, col.pointRect, col.pointCirc,
//   col.circRect, math.dist, math.angle, math.atan2,
//   mouse.x, mouse.y, mouse.dx, mouse.dy, mouse.btn, mouse.btnp,
//   mouse.show, mouse.hide

// --- FPS widget ----------------------------------------------------------


// -------------------------------------------------------------------------

// Shapes draggable with mouse
let shapes: any[] = [];
let dragging = -1;
let dragOffsetX = 0;
let dragOffsetY = 0;
let cursorVisible = true;
let lastDx = 0;
let lastDy = 0;
let dxDisplayTimer = 0;

export function _init(): void {
    mouse.show();

    // Rectangle A
    shapes.push({ type: 'rect', x: 170, y: 20, w: 50, h: 35, col: 12 });
    // Rectangle B
    shapes.push({ type: 'rect', x: 265, y: 20, w: 40, h: 40, col: 14 });
    // Circle A
    shapes.push({ type: 'circ', x: 195, y: 120, r: 20, col: 11 });
    // Circle B
    shapes.push({ type: 'circ', x: 285, y: 120, r: 15, col: 9 });
}

// --- Collision helpers: test shape i vs shape j ---
function shapesCollide(a: { type: string; x: number; y: number; w?: number; h?: number; r?: number }, b: { type: string; x: number; y: number; w?: number; h?: number; r?: number }) {
    if (a.type === 'rect' && b.type === 'rect') {
        return col.rect(a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h);
    }
    if (a.type === 'circ' && b.type === 'circ') {
        return col.circ(a.x, a.y, a.r, b.x, b.y, b.r);
    }
    // circ vs rect (order doesn't matter)
    const c = a.type === 'circ' ? a : b;
    const r = a.type === 'rect' ? a : b;
    return col.circRect(c.x, c.y, c.r, r.x, r.y, r.w, r.h);
}

function pointInShape(px: number, py: number, s: { type: string; x: number; y: number; w?: number; h?: number; r?: number }) {
    if (s.type === 'rect') return col.pointRect(px, py, s.x, s.y, s.w, s.h);
    return col.pointCirc(px, py, s.x, s.y, s.r);
}

export function _update(dt: number): void {

    const mx = mouse.x();
    const my = mouse.y();
    const mdx = mouse.dx();
    const mdy = mouse.dy();

    // Keep dx/dy visible for a short time after movement
    if (mdx !== 0 || mdy !== 0) {
        lastDx = mdx;
        lastDy = mdy;
        dxDisplayTimer = 0.5;
    }
    if (dxDisplayTimer > 0) {
        dxDisplayTimer -= dt;
        if (dxDisplayTimer <= 0) {
            lastDx = 0;
            lastDy = 0;
        }
    }

    // Toggle cursor visibility with H
    if (key.btnp(key.H)) {
        cursorVisible = !cursorVisible;
        if (cursorVisible) mouse.show();
        else mouse.hide();
    }

    // Start dragging on click
    if (mouse.btnp(mouse.LEFT)) {
        for (let i = shapes.length - 1; i >= 0; --i) {
            const s = shapes[i];
            if (pointInShape(mx, my, s)) {
                dragging = i;
                dragOffsetX = s.x - mx;
                dragOffsetY = s.y - my;
                break;
            }
        }
    }

    // Drag with mouse position
    if (dragging >= 0 && mouse.btn(mouse.LEFT)) {
        shapes[dragging].x = mx + dragOffsetX;
        shapes[dragging].y = my + dragOffsetY;
    }

    // Release
    if (!mouse.btn(mouse.LEFT)) {
        dragging = -1;
    }
}

export function _draw(): void {
    gfx.cls(0);

    gfx.print('COLLISION DEMO \u2014 drag shapes with mouse', 4, 2, 7);
    gfx.print('H=toggle cursor', 4, 170, 5);

    const mx = mouse.x();
    const my = mouse.y();

    // --- Build per-shape collision flags ---
    const hit = []; // hit[i] = true if shape i collides with anything
    for (let i = 0; i < shapes.length; ++i) hit.push(false);

    // shape vs shape (all pairs)
    const pairResults = [];
    for (let i = 0; i < shapes.length; ++i) {
        for (let j = i + 1; j < shapes.length; ++j) {
            const c = shapesCollide(shapes[i], shapes[j]);
            if (c) {
                hit[i] = true;
                hit[j] = true;
            }
            pairResults.push({ a: i, b: j, hit: c });
        }
    }

    // mouse vs each shape
    const mouseHits = [];
    for (let i = 0; i < shapes.length; ++i) {
        const mh = pointInShape(mx, my, shapes[i]);
        mouseHits.push(mh);
        if (mh) hit[i] = true;
    }

    // --- Draw shapes ---
    const labels = ['A', 'B', 'C', 'D'];
    for (let i = 0; i < shapes.length; ++i) {
        const s = shapes[i];
        const c = hit[i] ? 8 : s.col;
        if (s.type === 'rect') {
            gfx.rectfill(s.x, s.y, s.x + s.w - 1, s.y + s.h - 1, c);
            gfx.rect(s.x, s.y, s.x + s.w - 1, s.y + s.h - 1, 7);
            gfx.print(labels[i], s.x + 2, s.y + 2, 0);
        } else {
            gfx.circfill(s.x, s.y, s.r, c);
            gfx.circ(s.x, s.y, s.r, 7);
            gfx.print(labels[i], s.x - 2, s.y - 3, 0);
        }
    }

    // Draw lines between colliding circle pairs
    const c0 = shapes[2];
    const c1 = shapes[3];
    if (c0.type === 'circ' && c1.type === 'circ') {
        gfx.line(c0.x, c0.y, c1.x, c1.y, 5);
    }

    // Mouse crosshair
    gfx.line(mx - 4, my, mx + 4, my, 7);
    gfx.line(mx, my - 4, mx, my + 4, 7);

    // --- Info panel: all shape-pair results ---
    const iy = 12;
    const panelW = 115;
    // Pre-calculate height: header(8) + pairs*8 + gap(2) + header(8) + mouse*8 + padding(4)
    const panelH = 8 + pairResults.length * 8 + 2 + 8 + mouseHits.length * 8 + 4;
    gfx.rectfill(0, iy, panelW, iy + panelH, 1);
    let py = iy + 2;

    gfx.print('Shape collisions:', 2, py, 7);
    py += 8;
    for (let k = 0; k < pairResults.length; ++k) {
        const p = pairResults[k];
        const la = labels[p.a];
        const lb = labels[p.b];
        const txt = `${la} - ${lb}: ${p.hit ? 'HIT' : '---'}`;
        gfx.print(txt, 4, py, p.hit ? 8 : 6);
        py += 8;
    }

    py += 2;
    gfx.print('Mouse collisions:', 2, py, 7);
    py += 8;
    for (let k = 0; k < mouseHits.length; ++k) {
        const txt = `mouse - ${labels[k]}: ${mouseHits[k] ? 'HIT' : '---'}`;
        gfx.print(txt, 4, py, mouseHits[k] ? 8 : 6);
        py += 8;
    }

    // Distance/angle between circles C and D
    const dist = math.dist(c0.x, c0.y, c1.x, c1.y);
    const angle = math.angle(c0.x, c0.y, c1.x, c1.y);
    const atan = math.atan2(c1.y - c0.y, c1.x - c0.x);

    gfx.rectfill(0, 148, 200, 168, 1);
    gfx.print(`dist C-D: ${math.flr(dist)}`, 2, 150, 6);
    gfx.print(`angle: ${angle.toFixed(2)} rad`, 2, 158, 6);
    gfx.print(`atan2: ${atan.toFixed(2)}`, 110, 158, 6);

    // Mouse dx/dy = pixels moved since last frame
    gfx.print(`mouse dx:${lastDx} dy:${lastDy}`, 170, 170, dxDisplayTimer > 0 ? 7 : 5);

    sys.drawFps();
}

export function _save() {
    return {
        shapes,
    };
}

export function _restore(s: any): void {
    shapes = s.shapes;
}
