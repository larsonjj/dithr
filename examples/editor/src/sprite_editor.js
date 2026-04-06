// ─── Sprite editor ───────────────────────────────────────────────────────────
//
// Layout (640×360):
//   Row 0        : tab bar (shared drawTabBar)
//   Left panel   : sprite sheet grid (scrollable)
//   Center panel : zoomed pixel canvas for selected sprite
//   Bottom strip : palette picker, flags, tool selector
//

import { st } from "./state.js";
import {
    FB_W,
    FB_H,
    CW,
    CH,
    TAB_H,
    FG,
    GUTBG,
    GUTFG,
    FOOTBG,
    FOOTFG,
    GRIDC,
    PANELBG,
    SEPC,
} from "./config.js";
import { clamp, modKey, MOD_NAME } from "./helpers.js";
import { createHistory, record, commit, undo, redo } from "./stroke_history.js";

// ─── Constants ───────────────────────────────────────────────────────────────

const SPR_FOOT_H = CH * 2 - 2; // footer height (12px)

const SPR_GRID_X = 0;
const SPR_GRID_Y = TAB_H + 1; // below tab bar + 1px separator gap
const SPR_GRID_COLS = 16; // sprites per row in sheet overview
const SPR_GRID_CELL = 10; // pixels per cell in the overview
const SPR_GRID_W = SPR_GRID_COLS * SPR_GRID_CELL; // 160
const SPR_GRID_H = FB_H - SPR_GRID_Y - SPR_FOOT_H; // full height minus header gap and footer

const SPR_CANVAS_X = SPR_GRID_W + 2;
const SPR_CANVAS_Y = TAB_H + 1;
const SPR_PAL_H = 44; // palette strip height (palette + flags + shortcuts)
const SPR_CANVAS_H = FB_H - SPR_CANVAS_Y - SPR_PAL_H - SPR_FOOT_H; // canvas area height
const SPR_CANVAS_W = FB_W - SPR_GRID_W - 2; // canvas area width

const SPR_PAL_Y = FB_H - SPR_PAL_H - SPR_FOOT_H; // palette Y
const SPR_PAL_X = SPR_CANVAS_X;

const SPR_PAL_CELL = 10; // palette swatch size
const SPR_PAL_COLS = 16;

const SPR_TOOLS = ["PEN", "ERA", "FIL", "REC", "LIN", "CIR"];

// Multi-tile size presets: [w,h] in tiles
const SIZE_PRESETS = [
    [1, 1],
    [2, 1],
    [1, 2],
    [2, 2],
];

let sprHist = createHistory(100);

// ─── Geometry helpers ────────────────────────────────────────────────────────

// Bresenham line: calls plot(x,y) for each pixel
function bresenhamLine(x0, y0, x1, y1, plot) {
    let dx = Math.abs(x1 - x0);
    let dy = Math.abs(y1 - y0);
    let sx = x0 < x1 ? 1 : -1;
    let sy = y0 < y1 ? 1 : -1;
    let err = dx - dy;
    while (true) {
        plot(x0, y0);
        if (x0 === x1 && y0 === y1) break;
        let e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Midpoint circle: calls plot(x,y) for each pixel on the outline
function midpointCircle(cx, cy, r, plot) {
    if (r <= 0) {
        plot(cx, cy);
        return;
    }
    let x = r;
    let y = 0;
    let d = 1 - r;
    while (x >= y) {
        plot(cx + x, cy + y);
        plot(cx - x, cy + y);
        plot(cx + x, cy - y);
        plot(cx - x, cy - y);
        plot(cx + y, cy + x);
        plot(cx - y, cy + x);
        plot(cx + y, cy - x);
        plot(cx - y, cy - x);
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

// Get the effective canvas dimensions in pixels (multi-tile aware)
function canvasTileSize() {
    return { tw: st.sprSizeW * 8, th: st.sprSizeH * 8 };
}

// Get base pixel coords in sheet for the current selection
function selBase(sheetCols) {
    let selCol = st.sprSel % sheetCols;
    let selRow = Math.floor(st.sprSel / sheetCols);
    return { x: selCol * 8, y: selRow * 8 };
}

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateSpriteEditor() {
    let ctrl = modKey();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    let mx = mouse.x();
    let my = mouse.y();
    let mBtn = mouse.btn(0);
    let mPress = mouse.btnp(0);
    let wheel = mouse.wheel();

    let sw = gfx.sheetW();
    let sh = gfx.sheetH();
    if (sw === 0 || sh === 0) return; // no spritesheet loaded

    let tileW = 8; // default tile size
    let tileH = 8;
    let sheetCols = Math.floor(sw / tileW);
    let sheetRows = Math.floor(sh / tileH);
    let sprCount = sheetCols * sheetRows;

    // Effective canvas size in pixels (multi-tile aware)
    let { tw: cw, th: ch } = canvasTileSize();

    // ── Sheet grid interaction (click to select sprite) ──
    if (
        mPress &&
        mx >= SPR_GRID_X &&
        mx < SPR_GRID_X + SPR_GRID_W &&
        my >= SPR_GRID_Y &&
        my < SPR_GRID_Y + SPR_GRID_H
    ) {
        let gx = Math.floor((mx - SPR_GRID_X) / SPR_GRID_CELL);
        let gy = Math.floor((my - SPR_GRID_Y) / SPR_GRID_CELL) + st.sprScrollY;
        // Snap to top-left of multi-tile block
        gx = gx - (gx % st.sprSizeW);
        gy = gy - (gy % st.sprSizeH);
        let idx = gy * SPR_GRID_COLS + gx;
        if (idx >= 0 && idx < sprCount) st.sprSel = idx;
    }

    // Scroll sheet grid
    if (
        mx >= SPR_GRID_X &&
        mx < SPR_GRID_X + SPR_GRID_W &&
        my >= SPR_GRID_Y &&
        my < SPR_GRID_Y + SPR_GRID_H
    ) {
        st.sprScrollY = clamp(
            st.sprScrollY - wheel,
            0,
            Math.max(
                0,
                Math.ceil(sprCount / SPR_GRID_COLS) - Math.floor(SPR_GRID_H / SPR_GRID_CELL),
            ),
        );
    }

    // ── Zoomed canvas interaction (paint pixels) ──
    let canvasSize = Math.min(SPR_CANVAS_W, SPR_CANVAS_H);
    let zoom = Math.floor(canvasSize / Math.max(cw, ch));
    let canvasPixW = zoom * cw;
    let canvasPixH = zoom * ch;
    let canvasOx = SPR_CANVAS_X + Math.floor((SPR_CANVAS_W - canvasPixW) / 2);
    let canvasOy = SPR_CANVAS_Y + Math.floor((SPR_CANVAS_H - canvasPixH) / 2);

    let base = selBase(sheetCols);

    // Track hover pixel and toggle system cursor
    let wasHovering = st.sprHoverX >= 0;
    st.sprHoverX = -1;
    st.sprHoverY = -1;
    if (
        mx >= canvasOx &&
        mx < canvasOx + canvasPixW &&
        my >= canvasOy &&
        my < canvasOy + canvasPixH
    ) {
        st.sprHoverX = Math.floor((mx - canvasOx) / zoom);
        st.sprHoverY = Math.floor((my - canvasOy) / zoom);
        if (!wasHovering) mouse.hide();
    } else if (wasHovering) {
        mouse.show();
    }

    // ── Pen / Eraser (tools 0, 1) ──
    if ((st.sprTool === 0 || st.sprTool === 1) && mBtn && st.sprHoverX >= 0) {
        let sx = base.x + st.sprHoverX;
        let sy = base.y + st.sprHoverY;
        let newCol = st.sprTool === 0 ? st.sprCol : 0;
        let prev = gfx.sget(sx, sy);
        if (prev !== newCol) {
            record(sprHist, { x: sx, y: sy, prev: prev, next: newCol });
            gfx.sset(sx, sy, newCol);
        }
    }

    // ── Flood fill (tool 2) ──
    if (st.sprTool === 2 && mPress && st.sprHoverX >= 0) {
        let target = gfx.sget(base.x + st.sprHoverX, base.y + st.sprHoverY);
        if (target !== st.sprCol) {
            floodFill(base.x, base.y, cw, ch, st.sprHoverX, st.sprHoverY, target, st.sprCol);
            commit(sprHist);
        }
    }

    // ── Rectangle (tool 3) — drag to define rect, commit on release ──
    if (st.sprTool === 3) {
        if (mPress && st.sprHoverX >= 0) {
            st.sprRectAnchor = { x: st.sprHoverX, y: st.sprHoverY };
        }
        if (!mBtn && st.sprRectAnchor) {
            if (st.sprHoverX >= 0) {
                plotRect(
                    st.sprRectAnchor.x,
                    st.sprRectAnchor.y,
                    st.sprHoverX,
                    st.sprHoverY,
                    base,
                    st.sprCol,
                );
                commit(sprHist);
            }
            st.sprRectAnchor = null;
        }
    }

    // ── Line (tool 4) — click start, release end ──
    if (st.sprTool === 4) {
        if (mPress && st.sprHoverX >= 0) {
            st.sprLineAnchor = { x: st.sprHoverX, y: st.sprHoverY };
        }
        if (!mBtn && st.sprLineAnchor) {
            if (st.sprHoverX >= 0) {
                bresenhamLine(
                    st.sprLineAnchor.x,
                    st.sprLineAnchor.y,
                    st.sprHoverX,
                    st.sprHoverY,
                    function (px, py) {
                        if (px >= 0 && px < cw && py >= 0 && py < ch) {
                            let sx = base.x + px;
                            let sy = base.y + py;
                            let prev = gfx.sget(sx, sy);
                            if (prev !== st.sprCol) {
                                record(sprHist, { x: sx, y: sy, prev: prev, next: st.sprCol });
                                gfx.sset(sx, sy, st.sprCol);
                            }
                        }
                    },
                );
                commit(sprHist);
            }
            st.sprLineAnchor = null;
        }
    }

    // ── Circle (tool 5) — click center, drag radius, release ──
    if (st.sprTool === 5) {
        if (mPress && st.sprHoverX >= 0) {
            st.sprCircAnchor = { x: st.sprHoverX, y: st.sprHoverY };
        }
        if (!mBtn && st.sprCircAnchor) {
            if (st.sprHoverX >= 0) {
                let cdx = st.sprHoverX - st.sprCircAnchor.x;
                let cdy = st.sprHoverY - st.sprCircAnchor.y;
                let r = Math.round(Math.sqrt(cdx * cdx + cdy * cdy));
                midpointCircle(st.sprCircAnchor.x, st.sprCircAnchor.y, r, function (px, py) {
                    if (px >= 0 && px < cw && py >= 0 && py < ch) {
                        let sx = base.x + px;
                        let sy = base.y + py;
                        let prev = gfx.sget(sx, sy);
                        if (prev !== st.sprCol) {
                            record(sprHist, { x: sx, y: sy, prev: prev, next: st.sprCol });
                            gfx.sset(sx, sy, st.sprCol);
                        }
                    }
                });
                commit(sprHist);
            }
            st.sprCircAnchor = null;
        }
    }

    // Commit stroke on mouse release
    if (!mBtn && sprHist.pending.length > 0) {
        commit(sprHist);
    }

    // Undo (Ctrl+Z)
    if (ctrl && key.btnp(key.Z) && !shift) {
        undo(sprHist, function (op) {
            gfx.sset(op.x, op.y, op.prev);
            return { x: op.x, y: op.y, prev: op.next, next: op.prev };
        });
    }

    // Redo (Ctrl+Y or Ctrl+Shift+Z)
    if (ctrl && (key.btnp(key.Y) || (shift && key.btnp(key.Z)))) {
        redo(sprHist, function (op) {
            gfx.sset(op.x, op.y, op.prev);
            return { x: op.x, y: op.y, prev: op.next, next: op.prev };
        });
    }

    // ── Palette click ──
    if (
        mPress &&
        my >= SPR_PAL_Y &&
        my < SPR_PAL_Y + SPR_PAL_CELL * 2 &&
        mx >= SPR_PAL_X &&
        mx < SPR_PAL_X + SPR_PAL_COLS * SPR_PAL_CELL
    ) {
        let pc = Math.floor((mx - SPR_PAL_X) / SPR_PAL_CELL);
        let pr = Math.floor((my - SPR_PAL_Y) / SPR_PAL_CELL);
        let idx = pr * SPR_PAL_COLS + pc;
        if (idx >= 0 && idx < 32) st.sprCol = idx;
    }

    // ── Flags toggle (8 bits below palette) ──
    let flagsY = SPR_PAL_Y + SPR_PAL_CELL * 2 + 2;
    if (
        mPress &&
        my >= flagsY &&
        my < flagsY + CH &&
        mx >= SPR_PAL_X &&
        mx < SPR_PAL_X + 8 * (CW + 2)
    ) {
        let bit = Math.floor((mx - SPR_PAL_X) / (CW + 2));
        if (bit >= 0 && bit < 8) {
            let f = gfx.fget(st.sprSel);
            gfx.fset(st.sprSel, f ^ (1 << bit));
        }
    }

    // ── Tool switch (keyboard) ──
    if (key.btnp(key.B)) st.sprTool = 0;
    if (key.btnp(key.E)) st.sprTool = 1;
    if (key.btnp(key.F) && !ctrl) st.sprTool = 2;
    if (key.btnp(key.R) && !ctrl && !shift) st.sprTool = 3;
    if (key.btnp(key.L) && !ctrl) st.sprTool = 4;
    if (key.btnp(key.O) && !ctrl) st.sprTool = 5;

    // ── Palette number shortcuts (1-9 → col 1-9, 0 → col 0) ──
    let numKeys = [
        key.NUM0,
        key.NUM1,
        key.NUM2,
        key.NUM3,
        key.NUM4,
        key.NUM5,
        key.NUM6,
        key.NUM7,
        key.NUM8,
        key.NUM9,
    ];
    if (!ctrl && !shift) {
        for (let i = 0; i < 10; i++) {
            if (key.btnp(numKeys[i])) {
                st.sprCol = i;
                break;
            }
        }
    }

    // Pick colour from canvas (right-click)
    if (mouse.btn(1) && st.sprHoverX >= 0) {
        st.sprCol = gfx.sget(base.x + st.sprHoverX, base.y + st.sprHoverY);
        st.sprTool = 0;
    }

    // ── Arrow-key sprite navigation (no modifiers) ──
    if (!ctrl && !shift) {
        if (key.btnp(key.LEFT)) {
            st.sprSel = Math.max(0, st.sprSel - st.sprSizeW);
        }
        if (key.btnp(key.RIGHT)) {
            st.sprSel = Math.min(sprCount - 1, st.sprSel + st.sprSizeW);
        }
        if (key.btnp(key.UP)) {
            let next = st.sprSel - sheetCols * st.sprSizeH;
            if (next >= 0) st.sprSel = next;
        }
        if (key.btnp(key.DOWN)) {
            let next = st.sprSel + sheetCols * st.sprSizeH;
            if (next < sprCount) st.sprSel = next;
        }
    }

    // ── Multi-tile size cycle (M key) ──
    if (key.btnp(key.M) && !ctrl) {
        let cur = -1;
        for (let i = 0; i < SIZE_PRESETS.length; i++) {
            if (SIZE_PRESETS[i][0] === st.sprSizeW && SIZE_PRESETS[i][1] === st.sprSizeH) {
                cur = i;
                break;
            }
        }
        let next = (cur + 1) % SIZE_PRESETS.length;
        st.sprSizeW = SIZE_PRESETS[next][0];
        st.sprSizeH = SIZE_PRESETS[next][1];
        // Snap selection to valid multi-tile alignment
        let sc = st.sprSel % sheetCols;
        let sr = Math.floor(st.sprSel / sheetCols);
        sc = sc - (sc % st.sprSizeW);
        sr = sr - (sr % st.sprSizeH);
        st.sprSel = sr * sheetCols + sc;
    }

    // ── Transforms (Shift+H: flip H, Shift+V: flip V, Shift+R: rotate 90°) ──
    if (shift && !ctrl && key.btnp(key.H)) {
        transformFlipH(base, cw, ch);
    }
    if (shift && !ctrl && key.btnp(key.V)) {
        transformFlipV(base, cw, ch);
    }
    if (shift && !ctrl && key.btnp(key.R)) {
        if (cw === ch) transformRotate90(base, cw, ch);
    }

    // ── Shift/nudge pixels (Shift+Arrow) ──
    if (shift && !ctrl) {
        if (key.btnp(key.LEFT)) nudgePixels(base, cw, ch, -1, 0);
        if (key.btnp(key.RIGHT)) nudgePixels(base, cw, ch, 1, 0);
        if (key.btnp(key.UP)) nudgePixels(base, cw, ch, 0, -1);
        if (key.btnp(key.DOWN)) nudgePixels(base, cw, ch, 0, 1);
    }

    // ── Clear sprite (Delete) ──
    if (key.btnp(key.DELETE) && !ctrl) {
        clearSprite(base, cw, ch);
    }

    // ── Copy sprite (Ctrl+C) ──
    if (ctrl && key.btnp(key.C)) {
        copySprite(base, cw, ch);
    }

    // ── Paste sprite (Ctrl+V) ──
    if (ctrl && key.btnp(key.V)) {
        pasteSprite(base, cw, ch);
    }
}

// ── Flood fill helper ──
function floodFill(baseX, baseY, tileW, tileH, startX, startY, target, fill) {
    let stack = [startX + startY * tileW];
    let visited = new Uint8Array(tileW * tileH);
    while (stack.length > 0) {
        let idx = stack.pop();
        if (visited[idx]) continue;
        visited[idx] = 1;
        let px = idx % tileW;
        let py = Math.floor(idx / tileW);
        let sx = baseX + px;
        let sy = baseY + py;
        if (gfx.sget(sx, sy) !== target) continue;
        record(sprHist, { x: sx, y: sy, prev: target, next: fill });
        gfx.sset(sx, sy, fill);
        if (px > 0) stack.push(px - 1 + py * tileW);
        if (px < tileW - 1) stack.push(px + 1 + py * tileW);
        if (py > 0) stack.push(px + (py - 1) * tileW);
        if (py < tileH - 1) stack.push(px + (py + 1) * tileW);
    }
}

// ── Rectangle outline helper ──
function plotRect(x0, y0, x1, y1, base, col) {
    let rx0 = Math.min(x0, x1);
    let ry0 = Math.min(y0, y1);
    let rx1 = Math.max(x0, x1);
    let ry1 = Math.max(y0, y1);
    for (let py = ry0; py <= ry1; py++) {
        for (let px = rx0; px <= rx1; px++) {
            if (px !== rx0 && px !== rx1 && py !== ry0 && py !== ry1) continue;
            let sx = base.x + px;
            let sy = base.y + py;
            let prev = gfx.sget(sx, sy);
            if (prev !== col) {
                record(sprHist, { x: sx, y: sy, prev: prev, next: col });
                gfx.sset(sx, sy, col);
            }
        }
    }
}

// ─── Transform helpers ──────────────────────────────────────────────────────

function readPixels(base, cw, ch) {
    let pixels = [];
    for (let py = 0; py < ch; py++) {
        for (let px = 0; px < cw; px++) {
            pixels.push(gfx.sget(base.x + px, base.y + py));
        }
    }
    return pixels;
}

function writePixel(sx, sy, newCol) {
    let prev = gfx.sget(sx, sy);
    if (prev !== newCol) {
        record(sprHist, { x: sx, y: sy, prev: prev, next: newCol });
        gfx.sset(sx, sy, newCol);
    }
}

function transformFlipH(base, cw, ch) {
    let pixels = readPixels(base, cw, ch);
    for (let py = 0; py < ch; py++) {
        for (let px = 0; px < cw; px++) {
            writePixel(base.x + px, base.y + py, pixels[py * cw + (cw - 1 - px)]);
        }
    }
    commit(sprHist);
}

function transformFlipV(base, cw, ch) {
    let pixels = readPixels(base, cw, ch);
    for (let py = 0; py < ch; py++) {
        for (let px = 0; px < cw; px++) {
            writePixel(base.x + px, base.y + py, pixels[(ch - 1 - py) * cw + px]);
        }
    }
    commit(sprHist);
}

function transformRotate90(base, cw, ch) {
    // Rotate 90° clockwise: new(x,y) = old(y, cw-1-x)
    let pixels = readPixels(base, cw, ch);
    for (let py = 0; py < ch; py++) {
        for (let px = 0; px < cw; px++) {
            writePixel(base.x + px, base.y + py, pixels[(cw - 1 - px) * cw + py]);
        }
    }
    commit(sprHist);
}

// ─── Shift/nudge with wrapping ──────────────────────────────────────────────

function nudgePixels(base, cw, ch, dx, dy) {
    let pixels = readPixels(base, cw, ch);
    for (let py = 0; py < ch; py++) {
        for (let px = 0; px < cw; px++) {
            let srcX = (((px - dx) % cw) + cw) % cw;
            let srcY = (((py - dy) % ch) + ch) % ch;
            writePixel(base.x + px, base.y + py, pixels[srcY * cw + srcX]);
        }
    }
    commit(sprHist);
}

// ─── Clear sprite ───────────────────────────────────────────────────────────

function clearSprite(base, cw, ch) {
    let anyChanged = false;
    for (let py = 0; py < ch; py++) {
        for (let px = 0; px < cw; px++) {
            let sx = base.x + px;
            let sy = base.y + py;
            let prev = gfx.sget(sx, sy);
            if (prev !== 0) {
                record(sprHist, { x: sx, y: sy, prev: prev, next: 0 });
                gfx.sset(sx, sy, 0);
                anyChanged = true;
            }
        }
    }
    if (anyChanged) commit(sprHist);
}

// ─── Copy / Paste ───────────────────────────────────────────────────────────

function copySprite(base, cw, ch) {
    st.sprClipboard = { w: cw, h: ch, pixels: readPixels(base, cw, ch) };
}

function pasteSprite(base, cw, ch) {
    if (!st.sprClipboard) return;
    let clip = st.sprClipboard;
    let pw = Math.min(clip.w, cw);
    let ph = Math.min(clip.h, ch);
    let anyChanged = false;
    for (let py = 0; py < ph; py++) {
        for (let px = 0; px < pw; px++) {
            let sx = base.x + px;
            let sy = base.y + py;
            let prev = gfx.sget(sx, sy);
            let newCol = clip.pixels[py * clip.w + px];
            if (prev !== newCol) {
                record(sprHist, { x: sx, y: sy, prev: prev, next: newCol });
                gfx.sset(sx, sy, newCol);
                anyChanged = true;
            }
        }
    }
    if (anyChanged) commit(sprHist);
}

// ─── Tool cursor icons ──────────────────────────────────────────────────────
// Each icon is drawn as a small pixel pattern offset from the mouse position.
// Drawn last so it appears on top of everything.

function drawToolCursor(mx, my) {
    let ox = mx + 3; // offset right of cursor
    let oy = my + 3; // offset below cursor
    let s = 2; // pixel scale factor

    // Collect icon pixel rects: {x0,y0,x1,y1}
    let rects = [];
    function sp(x, y) {
        rects.push({
            x0: ox + x * s,
            y0: oy + y * s,
            x1: ox + (x + 1) * s - 1,
            y1: oy + (y + 1) * s - 1,
        });
    }

    // Build icon shape
    if (st.sprTool === 0) {
        // Pen: diagonal pencil
        sp(1, 4);
        sp(2, 3);
        sp(3, 2);
        sp(4, 1);
        sp(5, 0);
    } else if (st.sprTool === 1) {
        // Eraser: block
        for (let ey = 0; ey <= 3; ey++) for (let ex = 0; ex <= 4; ex++) sp(ex, ey);
    } else if (st.sprTool === 2) {
        // Fill: paint bucket
        sp(2, 0);
        sp(1, 1);
        sp(3, 1);
        sp(0, 2);
        sp(1, 2);
        sp(2, 2);
        sp(3, 2);
        sp(1, 3);
        sp(2, 3);
        sp(3, 3);
        sp(4, 3);
        sp(2, 4);
        sp(3, 4);
        sp(4, 4);
        sp(3, 5);
    } else if (st.sprTool === 3) {
        // Rect: rectangle outline
        for (let i = 0; i <= 5; i++) {
            sp(i, 0);
            sp(i, 4);
        }
        for (let i = 1; i < 4; i++) {
            sp(0, i);
            sp(5, i);
        }
    } else if (st.sprTool === 4) {
        // Line: diagonal
        sp(0, 5);
        sp(1, 4);
        sp(2, 3);
        sp(3, 2);
        sp(4, 1);
        sp(5, 0);
    } else if (st.sprTool === 5) {
        // Circle
        sp(2, 0);
        sp(3, 0);
        sp(1, 1);
        sp(4, 1);
        sp(0, 2);
        sp(5, 2);
        sp(0, 3);
        sp(5, 3);
        sp(1, 4);
        sp(4, 4);
        sp(2, 5);
        sp(3, 5);
    }

    // Crosshair rects
    let cross = [
        { x0: mx - 1, y0: my, x1: mx + 1, y1: my },
        { x0: mx, y0: my - 1, x1: mx, y1: my + 1 },
    ];

    // Pass 1: black outline (expand each rect by 1px in all directions)
    for (let i = 0; i < rects.length; i++) {
        let r = rects[i];
        gfx.rectfill(r.x0 - 1, r.y0 - 1, r.x1 + 1, r.y1 + 1, 0);
    }
    for (let i = 0; i < cross.length; i++) {
        let r = cross[i];
        gfx.rectfill(r.x0 - 1, r.y0 - 1, r.x1 + 1, r.y1 + 1, 0);
    }

    // Pass 2: white foreground
    for (let i = 0; i < rects.length; i++) {
        let r = rects[i];
        gfx.rectfill(r.x0, r.y0, r.x1, r.y1, FG);
    }
    for (let i = 0; i < cross.length; i++) {
        let r = cross[i];
        gfx.rectfill(r.x0, r.y0, r.x1, r.y1, FG);
    }
}

// ─── Draw ────────────────────────────────────────────────────────────────────

export function drawSpriteEditor() {
    let sw = gfx.sheetW();
    let sh = gfx.sheetH();
    if (sw === 0 || sh === 0) {
        gfx.print("No spritesheet loaded", 10, FB_H / 2, FG);
        return;
    }

    let tileW = 8;
    let tileH = 8;
    let sheetCols = Math.floor(sw / tileW);
    let sheetRows = Math.floor(sh / tileH);
    let sprCount = sheetCols * sheetRows;

    let { tw: cw, th: ch } = canvasTileSize();

    // ── Panel backgrounds (distinct from header/footer) ──
    gfx.rectfill(
        SPR_GRID_X,
        SPR_GRID_Y,
        SPR_GRID_X + SPR_GRID_W - 1,
        SPR_GRID_Y + SPR_GRID_H - 1,
        PANELBG,
    );
    gfx.rectfill(SPR_CANVAS_X, SPR_CANVAS_Y, FB_W - 1, FB_H - SPR_FOOT_H - 1, PANELBG);

    // ── Separator lines ──
    gfx.line(0, TAB_H, FB_W - 1, TAB_H, SEPC); // below tab bar
    gfx.line(SPR_GRID_W, SPR_GRID_Y, SPR_GRID_W, FB_H - SPR_FOOT_H - 1, SEPC); // left/right divider
    gfx.line(0, FB_H - SPR_FOOT_H, FB_W - 1, FB_H - SPR_FOOT_H, SEPC); // above footer

    // Draw sprite thumbnails
    let visRows = Math.floor(SPR_GRID_H / SPR_GRID_CELL);
    for (let r = 0; r < visRows; r++) {
        for (let c = 0; c < SPR_GRID_COLS; c++) {
            let idx = (r + st.sprScrollY) * SPR_GRID_COLS + c;
            if (idx >= sprCount) break;
            let dx = SPR_GRID_X + c * SPR_GRID_CELL;
            let dy = SPR_GRID_Y + r * SPR_GRID_CELL;
            let srcX = (idx % sheetCols) * tileW;
            let srcY = Math.floor(idx / sheetCols) * tileH;
            gfx.sspr(srcX, srcY, tileW, tileH, dx, dy, SPR_GRID_CELL, SPR_GRID_CELL);
        }
    }

    // Grid lines in sheet selector
    for (let gc = 1; gc < SPR_GRID_COLS; gc++) {
        let gx = SPR_GRID_X + gc * SPR_GRID_CELL;
        gfx.line(gx, SPR_GRID_Y, gx, SPR_GRID_Y + visRows * SPR_GRID_CELL - 1, SEPC);
    }
    for (let gr = 1; gr < visRows; gr++) {
        let gy = SPR_GRID_Y + gr * SPR_GRID_CELL;
        gfx.line(SPR_GRID_X, gy, SPR_GRID_X + SPR_GRID_W - 1, gy, SEPC);
    }

    // Highlight selected multi-tile region
    let selCol = st.sprSel % sheetCols;
    let selRow = Math.floor(st.sprSel / sheetCols);
    let gridSelRow = selRow - st.sprScrollY;
    if (gridSelRow >= 0 && gridSelRow + st.sprSizeH <= visRows) {
        let hx = SPR_GRID_X + selCol * SPR_GRID_CELL;
        let hy = SPR_GRID_Y + gridSelRow * SPR_GRID_CELL;
        gfx.rect(
            hx,
            hy,
            hx + st.sprSizeW * SPR_GRID_CELL - 1,
            hy + st.sprSizeH * SPR_GRID_CELL - 1,
            FG,
        );
    }

    // ── Zoomed canvas ──
    let canvasSize = Math.min(SPR_CANVAS_W, SPR_CANVAS_H);
    let zoom = Math.floor(canvasSize / Math.max(cw, ch));
    let canvasPixW = zoom * cw;
    let canvasPixH = zoom * ch;
    let canvasOx = SPR_CANVAS_X + Math.floor((SPR_CANVAS_W - canvasPixW) / 2);
    let canvasOy = SPR_CANVAS_Y + Math.floor((SPR_CANVAS_H - canvasPixH) / 2);

    // Background
    gfx.rectfill(canvasOx - 1, canvasOy - 1, canvasOx + canvasPixW, canvasOy + canvasPixH, GRIDC);

    let base = selBase(sheetCols);
    for (let py = 0; py < ch; py++) {
        for (let px = 0; px < cw; px++) {
            let c = gfx.sget(base.x + px, base.y + py);
            gfx.rectfill(
                canvasOx + px * zoom,
                canvasOy + py * zoom,
                canvasOx + (px + 1) * zoom - 1,
                canvasOy + (py + 1) * zoom - 1,
                c,
            );
        }
    }

    // Grid lines
    for (let px = 0; px <= cw; px++) {
        let x = canvasOx + px * zoom;
        gfx.line(x, canvasOy, x, canvasOy + canvasPixH - 1, GRIDC);
    }
    for (let py = 0; py <= ch; py++) {
        let y = canvasOy + py * zoom;
        gfx.line(canvasOx, y, canvasOx + canvasPixW - 1, y, GRIDC);
    }

    // Tile boundary lines (for multi-tile)
    if (st.sprSizeW > 1 || st.sprSizeH > 1) {
        for (let tx = 1; tx < st.sprSizeW; tx++) {
            let x = canvasOx + tx * tileW * zoom;
            gfx.line(x, canvasOy, x, canvasOy + canvasPixH - 1, FG);
        }
        for (let ty = 1; ty < st.sprSizeH; ty++) {
            let y = canvasOy + ty * tileH * zoom;
            gfx.line(canvasOx, y, canvasOx + canvasPixW - 1, y, FG);
        }
    }

    // Hover highlight
    if (st.sprHoverX >= 0) {
        let hx = canvasOx + st.sprHoverX * zoom;
        let hy = canvasOy + st.sprHoverY * zoom;
        gfx.rect(hx, hy, hx + zoom - 1, hy + zoom - 1, FG);

        // Tool cursor icon (drawn at mouse position)
        drawToolCursor(mouse.x(), mouse.y());
    }

    // Rectangle preview while dragging
    if (st.sprTool === 3 && st.sprRectAnchor && st.sprHoverX >= 0) {
        let rx0 = Math.min(st.sprRectAnchor.x, st.sprHoverX);
        let ry0 = Math.min(st.sprRectAnchor.y, st.sprHoverY);
        let rx1 = Math.max(st.sprRectAnchor.x, st.sprHoverX);
        let ry1 = Math.max(st.sprRectAnchor.y, st.sprHoverY);
        gfx.rect(
            canvasOx + rx0 * zoom,
            canvasOy + ry0 * zoom,
            canvasOx + (rx1 + 1) * zoom - 1,
            canvasOy + (ry1 + 1) * zoom - 1,
            FG,
        );
    }

    // Line preview while dragging
    if (st.sprTool === 4 && st.sprLineAnchor && st.sprHoverX >= 0) {
        gfx.line(
            canvasOx + st.sprLineAnchor.x * zoom + Math.floor(zoom / 2),
            canvasOy + st.sprLineAnchor.y * zoom + Math.floor(zoom / 2),
            canvasOx + st.sprHoverX * zoom + Math.floor(zoom / 2),
            canvasOy + st.sprHoverY * zoom + Math.floor(zoom / 2),
            FG,
        );
    }

    // Circle preview while dragging
    if (st.sprTool === 5 && st.sprCircAnchor && st.sprHoverX >= 0) {
        let cdx = st.sprHoverX - st.sprCircAnchor.x;
        let cdy = st.sprHoverY - st.sprCircAnchor.y;
        let r = Math.round(Math.sqrt(cdx * cdx + cdy * cdy));
        let centerScrX = canvasOx + st.sprCircAnchor.x * zoom + Math.floor(zoom / 2);
        let centerScrY = canvasOy + st.sprCircAnchor.y * zoom + Math.floor(zoom / 2);
        gfx.circ(centerScrX, centerScrY, r * zoom, FG);
    }

    // 2× sprite preview (top-right of canvas area)
    let prevScale = 2;
    let prevW = cw * prevScale;
    let prevH = ch * prevScale;
    let prevX = canvasOx + canvasPixW + 4;
    let prevY = canvasOy;
    gfx.rectfill(prevX - 1, prevY - 1, prevX + prevW, prevY + prevH, GRIDC);
    gfx.sspr(base.x, base.y, cw, ch, prevX, prevY, prevW, prevH);

    // ── Palette strip ──
    for (let i = 0; i < 32; i++) {
        let pc = i % SPR_PAL_COLS;
        let pr = Math.floor(i / SPR_PAL_COLS);
        let px = SPR_PAL_X + pc * SPR_PAL_CELL;
        let py = SPR_PAL_Y + pr * SPR_PAL_CELL;
        gfx.rectfill(px, py, px + SPR_PAL_CELL - 1, py + SPR_PAL_CELL - 1, i);
        if (i === st.sprCol) {
            gfx.rect(px, py, px + SPR_PAL_CELL - 1, py + SPR_PAL_CELL - 1, FG);
        }
    }

    // ── Flags display ──
    let flagsY = SPR_PAL_Y + SPR_PAL_CELL * 2 + 2;
    let flags = gfx.fget(st.sprSel);
    gfx.print("FLAGS:", SPR_PAL_X, flagsY, GUTFG);
    for (let b = 0; b < 8; b++) {
        let bx = SPR_PAL_X + 7 * CW + b * (CW + 2);
        let on = (flags >> b) & 1;
        gfx.print("" + b, bx, flagsY, on ? FG : GUTFG);
        if (on) gfx.rectfill(bx, flagsY + CH, bx + CW - 1, flagsY + CH + 1, FG);
    }

    // ── Shortcuts (panel area, above footer) ──
    let shortcutsY = flagsY + CH + 4;
    gfx.print(
        "B:pen E:era F:fill R:rect L:lin O:cir M:size  " + MOD_NAME + "+Z/Y:undo",
        SPR_PAL_X,
        shortcutsY,
        GUTFG,
    );

    // ── Footer (status bar) ──
    let footY = FB_H - SPR_FOOT_H;
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, FOOTBG);
    let footTextY = footY + Math.floor((SPR_FOOT_H - CH) / 2);
    gfx.print("TOOL:" + SPR_TOOLS[st.sprTool], 1 * CW, footTextY, FOOTFG);
    gfx.print("COL:" + st.sprCol, 14 * CW, footTextY, st.sprCol);
    gfx.print("#" + st.sprSel, 22 * CW, footTextY, FG);
    gfx.print(st.sprSizeW + "x" + st.sprSizeH, 28 * CW, footTextY, FOOTFG);
    if (st.sprHoverX >= 0) {
        gfx.print("(" + st.sprHoverX + "," + st.sprHoverY + ")", 34 * CW, footTextY, FOOTFG);
    }
}
