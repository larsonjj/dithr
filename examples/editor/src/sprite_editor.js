// ─── Sprite editor ───────────────────────────────────────────────────────────
//
// Layout (640×360):
//   Row 0        : tab bar (shared drawTabBar)
//   Left panel   : sprite sheet grid (scrollable)
//   Center panel : zoomed pixel canvas for selected sprite
//   Bottom strip : palette picker, flags, tool selector
//

import { st } from './state.js';
import {
    FB_W, FB_H, CW, CH, TAB_H, TAB_CODE, TAB_SPRITES, TAB_MAP,
    FG, GUTBG, GUTFG, FOOTBG, FOOTFG, GRIDC
} from './config.js';
import { clamp } from './helpers.js';

// ─── Constants ───────────────────────────────────────────────────────────────

const SPR_GRID_X = 0;
const SPR_GRID_Y = TAB_H; // below tab bar
const SPR_GRID_COLS = 16; // sprites per row in sheet overview
const SPR_GRID_CELL = 10; // pixels per cell in the overview
const SPR_GRID_W = SPR_GRID_COLS * SPR_GRID_CELL; // 160
const SPR_GRID_H = FB_H - TAB_H - CH; // full height minus tab bar and footer

const SPR_CANVAS_X = SPR_GRID_W + 2;
const SPR_CANVAS_Y = TAB_H;
const SPR_PAL_H = 34; // palette strip height at bottom
const SPR_CANVAS_H = FB_H - TAB_H - SPR_PAL_H - CH; // canvas area height
const SPR_CANVAS_W = FB_W - SPR_GRID_W - 2; // canvas area width

const SPR_PAL_Y = FB_H - SPR_PAL_H - CH; // palette Y
const SPR_PAL_X = SPR_CANVAS_X;

const SPR_PAL_CELL = 10; // palette swatch size
const SPR_PAL_COLS = 16;

const SPR_TOOLS = ["PEN", "ERA"];

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateSpriteEditor() {
    let ctrl = key.btn(key.LCTRL) || key.btn(key.RCTRL);

    // Tab switching
    if (ctrl) {
        if (key.btnp(key.NUM1)) {
            st.activeTab = TAB_CODE;
            return;
        }
        if (key.btnp(key.NUM2)) {
            st.activeTab = TAB_SPRITES;
            return;
        }
        if (key.btnp(key.NUM3)) {
            st.activeTab = TAB_MAP;
            return;
        }
    }

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
    let zoom = Math.floor(canvasSize / tileW);
    let canvasPixW = zoom * tileW;
    let canvasPixH = zoom * tileH;
    let canvasOx = SPR_CANVAS_X + Math.floor((SPR_CANVAS_W - canvasPixW) / 2);
    let canvasOy = SPR_CANVAS_Y + Math.floor((SPR_CANVAS_H - canvasPixH) / 2);

    if (
        mBtn &&
        mx >= canvasOx &&
        mx < canvasOx + canvasPixW &&
        my >= canvasOy &&
        my < canvasOy + canvasPixH
    ) {
        let px = Math.floor((mx - canvasOx) / zoom);
        let py = Math.floor((my - canvasOy) / zoom);
        let selRow = Math.floor(st.sprSel / sheetCols);
        let selCol = st.sprSel % sheetCols;
        let sx = selCol * tileW + px;
        let sy = selRow * tileH + py;
        let newCol = st.sprTool === 0 ? st.sprCol : 0;
        let prev = gfx.sget(sx, sy);
        if (prev !== newCol) {
            st.sprUndoPending.push({ x: sx, y: sy, prev: prev });
            gfx.sset(sx, sy, newCol);
        }
    }

    // Commit stroke on mouse release
    if (!mBtn && st.sprUndoPending.length > 0) {
        st.sprUndoStack.push(st.sprUndoPending);
        st.sprUndoPending = [];
        if (st.sprUndoStack.length > st.SPR_MAX_UNDO) st.sprUndoStack.shift();
    }

    // Undo (Ctrl+Z)
    if (ctrl && key.btnp(key.Z) && st.sprUndoStack.length > 0) {
        let stroke = st.sprUndoStack.pop();
        for (let i = stroke.length - 1; i >= 0; i--) {
            gfx.sset(stroke[i].x, stroke[i].y, stroke[i].prev);
        }
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
    if (key.btnp(key.E)) st.sprTool = 1;
    if (key.btnp(key.B)) st.sprTool = 0;

    // Pick colour from canvas (right-click)
    if (
        mouse.btn(1) &&
        mx >= canvasOx &&
        mx < canvasOx + canvasPixW &&
        my >= canvasOy &&
        my < canvasOy + canvasPixH
    ) {
        let px = Math.floor((mx - canvasOx) / zoom);
        let py = Math.floor((my - canvasOy) / zoom);
        let selRow = Math.floor(st.sprSel / sheetCols);
        let selCol = st.sprSel % sheetCols;
        let sx = selCol * tileW + px;
        let sy = selRow * tileH + py;
        st.sprCol = gfx.sget(sx, sy);
        st.sprTool = 0;
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

    // ── Sheet grid panel background ──
    gfx.rectfill(
        SPR_GRID_X,
        SPR_GRID_Y,
        SPR_GRID_X + SPR_GRID_W - 1,
        SPR_GRID_Y + SPR_GRID_H - 1,
        GUTBG,
    );

    // Draw sprite thumbnails
    let visRows = Math.floor(SPR_GRID_H / SPR_GRID_CELL);
    for (let r = 0; r < visRows; r++) {
        for (let c = 0; c < SPR_GRID_COLS; c++) {
            let idx = (r + st.sprScrollY) * SPR_GRID_COLS + c;
            if (idx >= sprCount) break;
            let dx = SPR_GRID_X + c * SPR_GRID_CELL;
            let dy = SPR_GRID_Y + r * SPR_GRID_CELL;
            // Draw mini sprite using sspr
            let srcX = (idx % sheetCols) * tileW;
            let srcY = Math.floor(idx / sheetCols) * tileH;
            gfx.sspr(srcX, srcY, tileW, tileH, dx, dy, SPR_GRID_CELL, SPR_GRID_CELL);
            // Highlight selected
            if (idx === st.sprSel) {
                gfx.rect(dx, dy, dx + SPR_GRID_CELL - 1, dy + SPR_GRID_CELL - 1, FG);
            }
        }
    }

    // ── Zoomed canvas ──
    let canvasSize = Math.min(SPR_CANVAS_W, SPR_CANVAS_H);
    let zoom = Math.floor(canvasSize / tileW);
    let canvasPixW = zoom * tileW;
    let canvasPixH = zoom * tileH;
    let canvasOx = SPR_CANVAS_X + Math.floor((SPR_CANVAS_W - canvasPixW) / 2);
    let canvasOy = SPR_CANVAS_Y + Math.floor((SPR_CANVAS_H - canvasPixH) / 2);

    // Background checkerboard
    gfx.rectfill(canvasOx - 1, canvasOy - 1, canvasOx + canvasPixW, canvasOy + canvasPixH, GRIDC);

    let selRow = Math.floor(st.sprSel / sheetCols);
    let selCol = st.sprSel % sheetCols;
    for (let py = 0; py < tileH; py++) {
        for (let px = 0; px < tileW; px++) {
            let sx = selCol * tileW + px;
            let sy = selRow * tileH + py;
            let c = gfx.sget(sx, sy);
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
    for (let px = 0; px <= tileW; px++) {
        let x = canvasOx + px * zoom;
        gfx.line(x, canvasOy, x, canvasOy + canvasPixH - 1, GRIDC);
    }
    for (let py = 0; py <= tileH; py++) {
        let y = canvasOy + py * zoom;
        gfx.line(canvasOx, y, canvasOx + canvasPixW - 1, y, GRIDC);
    }

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

    // ── Tool / info ──
    let infoY = flagsY + CH + 4;
    gfx.print("TOOL:" + SPR_TOOLS[st.sprTool], SPR_PAL_X, infoY, GUTFG);
    gfx.print("COL:" + st.sprCol, SPR_PAL_X + 14 * CW, infoY, st.sprCol);
    gfx.print("#" + st.sprSel, SPR_PAL_X + 22 * CW, infoY, FG);

    // ── Footer ──
    let footY = FB_H - CH;
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, FOOTBG);
    gfx.print("B:pen E:eraser RClick:pick  Ctrl+1/2/3:tabs", 1 * CW, footY, FOOTFG);
}
