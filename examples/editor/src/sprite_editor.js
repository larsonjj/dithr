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
import { clamp, modKey, MOD_NAME, status } from "./helpers.js";
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
const SPR_CANVAS_H = FB_H - SPR_CANVAS_Y - SPR_FOOT_H; // full canvas area height
const SPR_CANVAS_W = FB_W - SPR_GRID_W - 2; // canvas area width

const SPR_PAL_CELL = 10; // palette swatch size
const SPR_PAL_COLS = 16; // colors per row

const SPR_TOOLS = ["PEN", "ERA", "FIL", "REC", "LIN", "CIR", "SEL"];

// Multi-tile size presets: [w,h] in tiles
const SIZE_PRESETS = [
    [1, 1],
    [2, 1],
    [1, 2],
    [2, 2],
    [3, 1],
    [1, 3],
    [3, 3],
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

// Filled midpoint circle
function midpointCircleFill(cx, cy, r, plot) {
    if (r <= 0) {
        plot(cx, cy);
        return;
    }
    let x = r,
        y = 0,
        d = 1 - r;
    function hline(x0, x1, yy) {
        for (let i = x0; i <= x1; i++) plot(i, yy);
    }
    while (x >= y) {
        hline(cx - x, cx + x, cy + y);
        hline(cx - x, cx + x, cy - y);
        hline(cx - y, cx + y, cy + x);
        hline(cx - y, cx + y, cy - x);
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

export function updateSpriteEditor(dt) {
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

    // ── Sprite goto input (N to open, Enter to confirm, Escape to cancel) ──
    if (st.sprGoto) {
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
        for (let i = 0; i < 10; i++) {
            if (key.btnp(numKeys[i])) st.sprGotoTxt += "" + i;
        }
        if (key.btnp(key.BACKSPACE)) {
            st.sprGotoTxt = st.sprGotoTxt.slice(0, -1);
        }
        if (key.btnp(key.ENTER)) {
            let n = parseInt(st.sprGotoTxt, 10);
            if (n >= 0 && n < sprCount) {
                st.sprSel = n;
                // Scroll grid to show selected sprite
                let row = Math.floor(n / SPR_GRID_COLS);
                let maxVis = Math.floor(SPR_GRID_H / SPR_GRID_CELL);
                if (row < st.sprScrollY || row >= st.sprScrollY + maxVis) {
                    st.sprScrollY = clamp(
                        row,
                        0,
                        Math.max(0, Math.ceil(sprCount / SPR_GRID_COLS) - maxVis),
                    );
                }
            }
            st.sprGoto = false;
            st.sprGotoTxt = "";
        }
        if (key.btnp(key.ESCAPE)) {
            st.sprGoto = false;
            st.sprGotoTxt = "";
        }
        return; // block all other sprite editor input while goto is active
    }
    if (key.btnp(key.N) && !ctrl && !shift) {
        st.sprGoto = true;
        st.sprGotoTxt = "";
    }

    // Effective canvas size in pixels (multi-tile aware)
    let { tw: cw, th: ch } = canvasTileSize();

    // ── Sheet grid interaction (click to select sprite) ──
    let totalRows = Math.ceil(sprCount / SPR_GRID_COLS);
    let maxVisRows = Math.floor(SPR_GRID_H / SPR_GRID_CELL);
    let visRows = Math.min(maxVisRows, totalRows - st.sprScrollY);
    let gridContentH = visRows * SPR_GRID_CELL;
    if (
        mPress &&
        mx >= SPR_GRID_X &&
        mx < SPR_GRID_X + SPR_GRID_W &&
        my >= SPR_GRID_Y &&
        my < SPR_GRID_Y + gridContentH
    ) {
        let gx = Math.floor((mx - SPR_GRID_X) / SPR_GRID_CELL);
        let gy = Math.floor((my - SPR_GRID_Y) / SPR_GRID_CELL) + st.sprScrollY;
        // Snap to top-left of multi-tile block
        gx = gx - (gx % st.sprSizeW);
        gy = gy - (gy % st.sprSizeH);
        let idx = gy * SPR_GRID_COLS + gx;
        if (idx >= 0 && idx < sprCount) {
            if (idx !== st.sprSel) {
                st.sprRectAnchor = null;
                st.sprLineAnchor = null;
                st.sprCircAnchor = null;
            }
            st.sprSel = idx;
        }
    }

    // Scroll sheet grid
    if (
        mx >= SPR_GRID_X &&
        mx < SPR_GRID_X + SPR_GRID_W &&
        my >= SPR_GRID_Y &&
        my < SPR_GRID_Y + gridContentH
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
    let autoZoom = Math.floor(Math.min(SPR_CANVAS_W, SPR_CANVAS_H) / Math.max(cw, ch));
    let zoom = st.sprZoom > 0 ? st.sprZoom : autoZoom;
    let canvasPixW = zoom * cw;
    let canvasPixH = zoom * ch;
    let canvasOx = SPR_CANVAS_X + Math.floor((SPR_CANVAS_W - canvasPixW) / 2) + st.sprPanX;
    let canvasOy = SPR_CANVAS_Y + Math.floor((SPR_CANVAS_H - canvasPixH) / 2) + st.sprPanY;

    let base = selBase(sheetCols);

    // ── Middle-click pan ──
    let midBtn = mouse.btn(1);
    let inCanvas =
        mx >= SPR_CANVAS_X &&
        mx < SPR_CANVAS_X + SPR_CANVAS_W &&
        my >= SPR_CANVAS_Y &&
        my < SPR_CANVAS_Y + SPR_CANVAS_H;
    if (midBtn && inCanvas) {
        if (!st.sprPanning) st.sprPanning = true;
        st.sprPanX += mouse.dx();
        st.sprPanY += mouse.dy();
    } else {
        st.sprPanning = false;
    }

    // ── Scroll-wheel zoom (over canvas area) ──
    if (inCanvas && wheel !== 0 && !st.sprPanning) {
        let oldZoom = zoom;
        let minZoom = Math.max(1, Math.floor(autoZoom / 4));
        let maxZoom = autoZoom * 4;
        let newZoom = clamp(zoom + wheel, minZoom, maxZoom);
        if (newZoom !== oldZoom) {
            // Zoom toward mouse pointer
            let pivotX = mx - canvasOx;
            let pivotY = my - canvasOy;
            st.sprPanX = Math.round(st.sprPanX + pivotX - (pivotX * newZoom) / oldZoom);
            st.sprPanY = Math.round(st.sprPanY + pivotY - (pivotY * newZoom) / oldZoom);
            st.sprZoom = newZoom;
            zoom = newZoom;
        }
    }

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

    // ── Symmetry-aware pixel setter ──
    function symSet(px, py, col) {
        let coords = [[px, py]];
        if (st.sprMirrorX) coords.push([cw - 1 - px, py]);
        if (st.sprMirrorY) coords.push([px, ch - 1 - py]);
        if (st.sprMirrorX && st.sprMirrorY) coords.push([cw - 1 - px, ch - 1 - py]);
        for (let i = 0; i < coords.length; i++) {
            let cx2 = coords[i][0],
                cy2 = coords[i][1];
            if (cx2 >= 0 && cx2 < cw && cy2 >= 0 && cy2 < ch) {
                let sx = base.x + cx2,
                    sy = base.y + cy2;
                let prev = gfx.sget(sx, sy);
                if (prev !== col) {
                    record(sprHist, { x: sx, y: sy, prev: prev, next: col });
                    gfx.sset(sx, sy, col);
                }
            }
        }
    }

    // ── Pen / Eraser (tools 0, 1) ──
    if ((st.sprTool === 0 || st.sprTool === 1) && mBtn && st.sprHoverX >= 0) {
        let newCol = st.sprTool === 0 ? st.sprCol : 0;
        symSet(st.sprHoverX, st.sprHoverY, newCol);
    }
    // Track last pen pixel to avoid duplicate records in same stroke
    if (!mBtn && sprHist.pending.length > 0) {
        // Deduplicate: keep only the last record per pixel in the pending stroke
        let seen = {};
        let deduped = [];
        for (let i = sprHist.pending.length - 1; i >= 0; i--) {
            let op = sprHist.pending[i];
            let k = op.x + "," + op.y;
            if (!seen[k]) {
                seen[k] = true;
                // Rewrite prev to be the original value (first record's prev for this pixel)
                deduped.push(op);
            } else {
                // Find original prev from first record for this pixel
                deduped[deduped.length - 1].prev = op.prev;
            }
        }
        deduped.reverse();
        // Filter out no-ops (prev === next after dedup)
        sprHist.pending = [];
        for (let i = 0; i < deduped.length; i++) {
            if (deduped[i].prev !== deduped[i].next) sprHist.pending.push(deduped[i]);
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
                    st.sprCol,
                    st.sprFilled,
                    symSet,
                );
                commit(sprHist);
            } else {
                status("Shape cancelled", 1);
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
                            symSet(px, py, st.sprCol);
                        }
                    },
                );
                commit(sprHist);
            } else {
                status("Shape cancelled", 1);
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
                let circFn = st.sprFilled ? midpointCircleFill : midpointCircle;
                circFn(st.sprCircAnchor.x, st.sprCircAnchor.y, r, function (px, py) {
                    if (px >= 0 && px < cw && py >= 0 && py < ch) {
                        symSet(px, py, st.sprCol);
                    }
                });
                commit(sprHist);
            } else {
                status("Shape cancelled", 1);
            }
            st.sprCircAnchor = null;
        }
    }

    // ── Selection/marquee (tool 6) — drag to select rect, then drag to move ──
    if (st.sprTool === 6) {
        if (mPress && st.sprHoverX >= 0) {
            // If clicking inside existing float, start moving it
            if (st.sprSelFloat) {
                let fx = st.sprHoverX - st.sprSelFloat.x;
                let fy = st.sprHoverY - st.sprSelFloat.y;
                if (fx >= 0 && fx < st.sprSelFloat.w && fy >= 0 && fy < st.sprSelFloat.h) {
                    st.sprSelDrag = {
                        ox: st.sprHoverX - st.sprSelFloat.x,
                        oy: st.sprHoverY - st.sprSelFloat.y,
                    };
                } else {
                    // Stamp the float and start new selection
                    stampFloat(base, cw, ch);
                    st.sprSelRect = null;
                    st.sprSelFloat = null;
                    st.sprRectAnchor = { x: st.sprHoverX, y: st.sprHoverY };
                }
            } else if (st.sprSelRect) {
                // Check if clicking inside selection to lift it
                let sr = st.sprSelRect;
                if (
                    st.sprHoverX >= sr.x0 &&
                    st.sprHoverX <= sr.x1 &&
                    st.sprHoverY >= sr.y0 &&
                    st.sprHoverY <= sr.y1
                ) {
                    liftSelection(base, sr, cw, ch);
                    st.sprSelDrag = { ox: st.sprHoverX - sr.x0, oy: st.sprHoverY - sr.y0 };
                } else {
                    st.sprSelRect = null;
                    st.sprRectAnchor = { x: st.sprHoverX, y: st.sprHoverY };
                }
            } else {
                st.sprRectAnchor = { x: st.sprHoverX, y: st.sprHoverY };
            }
        }
        // Drag to move floating selection
        if (mBtn && st.sprSelFloat && st.sprSelDrag && st.sprHoverX >= 0) {
            st.sprSelFloat.x = st.sprHoverX - st.sprSelDrag.ox;
            st.sprSelFloat.y = st.sprHoverY - st.sprSelDrag.oy;
        }
        // Release after dragging marquee area
        if (!mBtn && st.sprRectAnchor && !st.sprSelFloat) {
            if (st.sprHoverX >= 0) {
                let rx0 = Math.min(st.sprRectAnchor.x, st.sprHoverX);
                let ry0 = Math.min(st.sprRectAnchor.y, st.sprHoverY);
                let rx1 = Math.max(st.sprRectAnchor.x, st.sprHoverX);
                let ry1 = Math.max(st.sprRectAnchor.y, st.sprHoverY);
                if (rx1 >= rx0 || ry1 >= ry0) {
                    st.sprSelRect = { x0: rx0, y0: ry0, x1: rx1, y1: ry1 };
                }
            }
            st.sprRectAnchor = null;
        }
        if (!mBtn) st.sprSelDrag = null;
    }
    // Clear selection when switching away from sel tool
    if (st.sprTool !== 6 && (st.sprSelRect || st.sprSelFloat)) {
        if (st.sprSelFloat) stampFloat(base, cw, ch);
        st.sprSelRect = null;
        st.sprSelFloat = null;
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

    // Save sprite sheet to disk (Ctrl+S)
    if (ctrl && key.btnp(key.S)) {
        let ok1 = sys.writeFile("sprites.hex", gfx.sheetData());
        let ok2 = sys.writeFile("flags.hex", gfx.flagsData());
        status(ok1 && ok2 ? "Sprites saved" : "Sprite save failed");
    }

    // ── Palette (below sprite grid, left panel) ──
    let palX = SPR_GRID_X;
    let palY = SPR_GRID_Y + gridContentH + 2;
    let palTotalRows = Math.ceil(256 / SPR_PAL_COLS); // 16 rows for 256 colors
    let palAvailH = FB_H - SPR_FOOT_H - palY;
    let palVisRows = Math.min(palTotalRows, Math.floor(palAvailH / SPR_PAL_CELL));
    let palMaxScroll = Math.max(0, palTotalRows - palVisRows);
    st.sprPalScrollY = clamp(st.sprPalScrollY, 0, palMaxScroll);
    let palH = palVisRows * SPR_PAL_CELL;

    // Palette click
    if (
        mPress &&
        my >= palY &&
        my < palY + palH &&
        mx >= palX &&
        mx < palX + SPR_PAL_COLS * SPR_PAL_CELL
    ) {
        let pc = Math.floor((mx - palX) / SPR_PAL_CELL);
        let pr = Math.floor((my - palY) / SPR_PAL_CELL) + st.sprPalScrollY;
        let idx = pr * SPR_PAL_COLS + pc;
        if (idx >= 0 && idx < 256) st.sprCol = idx;
    }

    // Scroll palette with wheel when hovering over it
    if (
        wheel !== 0 &&
        my >= palY &&
        my < palY + palH &&
        mx >= palX &&
        mx < palX + SPR_PAL_COLS * SPR_PAL_CELL
    ) {
        st.sprPalScrollY = clamp(st.sprPalScrollY - wheel, 0, palMaxScroll);
    }

    // ── Flags toggle (below palette, left panel) ──
    let flagsY = palY + palH + 2;
    let flagsX = palX + 7 * CW; // after "FLAGS:" label
    if (mPress && my >= flagsY && my < flagsY + CH && mx >= flagsX && mx < flagsX + 8 * (CW + 2)) {
        let bit = Math.floor((mx - flagsX) / (CW + 2));
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
    if (key.btnp(key.S) && !ctrl) st.sprTool = 6; // selection tool

    // Toggle filled shapes (G key)
    if (key.btnp(key.G) && !ctrl && !shift) st.sprFilled = !st.sprFilled;

    // Toggle symmetry (Shift+X: horizontal mirror, Shift+Y: vertical mirror)
    if (shift && !ctrl && key.btnp(key.X)) st.sprMirrorX = !st.sprMirrorX;
    if (shift && !ctrl && key.btnp(key.Y)) st.sprMirrorY = !st.sprMirrorY;

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
    if (mouse.btn(2) && st.sprHoverX >= 0) {
        st.sprCol = gfx.sget(base.x + st.sprHoverX, base.y + st.sprHoverY);
        // Scroll palette to show selected color
        let colRow = Math.floor(st.sprCol / SPR_PAL_COLS);
        if (colRow < st.sprPalScrollY || colRow >= st.sprPalScrollY + palVisRows) {
            st.sprPalScrollY = clamp(colRow, 0, palMaxScroll);
        }
        st.sprTool = 0;
    }

    // ── Arrow-key sprite navigation (no modifiers) ──
    if (!ctrl && !shift) {
        let prevSel = st.sprSel;
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
        if (st.sprSel !== prevSel) {
            st.sprRectAnchor = null;
            st.sprLineAnchor = null;
            st.sprCircAnchor = null;
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
        // Clamp so multi-tile block stays within sheet
        sc = Math.min(sc, sheetCols - st.sprSizeW);
        sr = Math.min(sr, sheetRows - st.sprSizeH);
        st.sprSel = sr * sheetCols + sc;
        // Clear tool anchors for new tile size
        st.sprRectAnchor = null;
        st.sprLineAnchor = null;
        st.sprCircAnchor = null;
        // Reset zoom/pan for new tile size
        st.sprZoom = 0;
        st.sprPanX = 0;
        st.sprPanY = 0;
    }

    // ── Transforms (Shift+H: flip H, Shift+V: flip V, Shift+R: rotate 90°) ──
    if (shift && !ctrl && key.btnp(key.H)) {
        transformFlipH(base, cw, ch);
    }
    if (shift && !ctrl && key.btnp(key.V)) {
        transformFlipV(base, cw, ch);
    }
    if (shift && !ctrl && key.btnp(key.R)) {
        if (cw === ch) {
            transformRotate90(base, cw, ch);
        } else {
            status("Rotate requires square selection");
        }
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

    // ── Animation preview ──
    // P: toggle play, A: set range from→current sprite
    if (key.btnp(key.P) && !ctrl && !shift) {
        st.sprAnimPlay = !st.sprAnimPlay;
        if (st.sprAnimPlay) {
            if (st.sprAnimFrom === st.sprAnimTo) {
                st.sprAnimFrom = st.sprSel;
                st.sprAnimTo = Math.min(st.sprSel + 3, sprCount - 1);
            }
            st.sprAnimFrame = 0;
            st.sprAnimTimer = 0;
        }
    }
    // A: set animation start, Shift+A: set animation end
    if (key.btnp(key.A) && !ctrl && !st.sprAnimPlay) {
        if (shift) {
            st.sprAnimTo = st.sprSel;
        } else {
            st.sprAnimFrom = st.sprSel;
        }
        // Auto-swap so from <= to
        if (st.sprAnimFrom > st.sprAnimTo) {
            let tmp = st.sprAnimFrom;
            st.sprAnimFrom = st.sprAnimTo;
            st.sprAnimTo = tmp;
        }
        // Clamp frame to new range
        let newRange = st.sprAnimTo - st.sprAnimFrom + 1;
        if (st.sprAnimFrame >= newRange) st.sprAnimFrame = 0;
    }
    // Adjust FPS with [ and ]
    if (key.btnp(key.LBRACKET) && !ctrl) st.sprAnimFps = Math.max(1, st.sprAnimFps - 1);
    if (key.btnp(key.RBRACKET) && !ctrl) st.sprAnimFps = Math.min(60, st.sprAnimFps + 1);

    // Advance animation timer
    if (st.sprAnimPlay && st.sprAnimFrom < st.sprAnimTo) {
        st.sprAnimTimer += dt;
        let interval = 1 / st.sprAnimFps;
        if (st.sprAnimTimer >= interval) {
            st.sprAnimTimer -= interval;
            let range = st.sprAnimTo - st.sprAnimFrom + 1;
            st.sprAnimFrame = (st.sprAnimFrame + 1) % range;
        }
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
function plotRect(x0, y0, x1, y1, col, filled, plotFn) {
    let rx0 = Math.min(x0, x1);
    let ry0 = Math.min(y0, y1);
    let rx1 = Math.max(x0, x1);
    let ry1 = Math.max(y0, y1);
    for (let py = ry0; py <= ry1; py++) {
        for (let px = rx0; px <= rx1; px++) {
            if (!filled && px !== rx0 && px !== rx1 && py !== ry0 && py !== ry1) continue;
            plotFn(px, py, col);
        }
    }
}

// ─── Selection/marquee helpers ───────────────────────────────────────────────

function liftSelection(base, sr, cw, ch) {
    let w = sr.x1 - sr.x0 + 1;
    let h = sr.y1 - sr.y0 + 1;
    let pixels = [];
    for (let py = 0; py < h; py++) {
        for (let px = 0; px < w; px++) {
            let sx = base.x + sr.x0 + px;
            let sy = base.y + sr.y0 + py;
            pixels.push(gfx.sget(sx, sy));
            // Clear original area
            let prev = gfx.sget(sx, sy);
            if (prev !== 0) {
                record(sprHist, { x: sx, y: sy, prev: prev, next: 0 });
                gfx.sset(sx, sy, 0);
            }
        }
    }
    st.sprSelFloat = { x: sr.x0, y: sr.y0, w: w, h: h, pixels: pixels };
    st.sprSelRect = null;
}

function stampFloat(base, cw, ch) {
    if (!st.sprSelFloat) return;
    let f = st.sprSelFloat;
    for (let py = 0; py < f.h; py++) {
        for (let px = 0; px < f.w; px++) {
            let dx = f.x + px;
            let dy = f.y + py;
            if (dx >= 0 && dx < cw && dy >= 0 && dy < ch) {
                let col = f.pixels[py * f.w + px];
                if (col !== 0) {
                    let sx = base.x + dx;
                    let sy = base.y + dy;
                    let prev = gfx.sget(sx, sy);
                    if (prev !== col) {
                        record(sprHist, { x: sx, y: sy, prev: prev, next: col });
                        gfx.sset(sx, sy, col);
                    }
                }
            }
        }
    }
    commit(sprHist);
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
    } else if (st.sprTool === 6) {
        // Selection: dashed rect
        sp(0, 0);
        sp(2, 0);
        sp(4, 0);
        sp(0, 1);
        sp(4, 1);
        sp(0, 2);
        sp(4, 2);
        sp(0, 3);
        sp(4, 3);
        sp(0, 4);
        sp(2, 4);
        sp(4, 4);
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

    // ── Compute visible grid rows (capped to actual sheet content) ──
    let totalRows = Math.ceil(sprCount / SPR_GRID_COLS);
    let maxVisRows = Math.floor(SPR_GRID_H / SPR_GRID_CELL);
    let visRows = Math.min(maxVisRows, totalRows - st.sprScrollY);
    let gridContentH = visRows * SPR_GRID_CELL;

    // ── Panel backgrounds (distinct from header/footer) ──
    gfx.rectfill(
        SPR_GRID_X,
        SPR_GRID_Y,
        SPR_GRID_X + SPR_GRID_W - 1,
        SPR_GRID_Y + gridContentH - 1,
        PANELBG,
    );
    gfx.rectfill(SPR_CANVAS_X, SPR_CANVAS_Y, FB_W - 1, FB_H - SPR_FOOT_H - 1, PANELBG);

    // ── Separator lines ──
    gfx.line(0, TAB_H, FB_W - 1, TAB_H, SEPC); // below tab bar
    gfx.line(SPR_GRID_W, SPR_GRID_Y, SPR_GRID_W, FB_H - SPR_FOOT_H - 1, SEPC); // left/right divider
    gfx.line(0, FB_H - SPR_FOOT_H, FB_W - 1, FB_H - SPR_FOOT_H, SEPC); // above footer

    // Draw sprite thumbnails
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
    for (let gr = 1; gr <= visRows; gr++) {
        let gy = SPR_GRID_Y + gr * SPR_GRID_CELL;
        gfx.line(SPR_GRID_X, gy, SPR_GRID_X + SPR_GRID_W - 1, gy, SEPC);
    }

    // Highlight selected multi-tile region (draw if any part visible)
    let selCol = st.sprSel % sheetCols;
    let selRow = Math.floor(st.sprSel / sheetCols);
    let gridSelRow = selRow - st.sprScrollY;
    if (gridSelRow + st.sprSizeH > 0 && gridSelRow < visRows) {
        gfx.clip(SPR_GRID_X, SPR_GRID_Y, SPR_GRID_W, gridContentH);
        let hx = SPR_GRID_X + selCol * SPR_GRID_CELL;
        let hy = SPR_GRID_Y + gridSelRow * SPR_GRID_CELL;
        gfx.rect(
            hx,
            hy,
            hx + st.sprSizeW * SPR_GRID_CELL - 1,
            hy + st.sprSizeH * SPR_GRID_CELL - 1,
            FG,
        );
        gfx.clip();
    }

    // ── Zoomed canvas ──
    let autoZoom = Math.floor(Math.min(SPR_CANVAS_W, SPR_CANVAS_H) / Math.max(cw, ch));
    let zoom = st.sprZoom > 0 ? st.sprZoom : autoZoom;
    let canvasPixW = zoom * cw;
    let canvasPixH = zoom * ch;
    let canvasOx = SPR_CANVAS_X + Math.floor((SPR_CANVAS_W - canvasPixW) / 2) + st.sprPanX;
    let canvasOy = SPR_CANVAS_Y + Math.floor((SPR_CANVAS_H - canvasPixH) / 2) + st.sprPanY;

    // Clip canvas drawing to the canvas panel area
    gfx.clip(SPR_CANVAS_X, SPR_CANVAS_Y, SPR_CANVAS_W, SPR_CANVAS_H);

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

    // Selection tool marquee preview
    if (st.sprTool === 6) {
        // Selection marquee during drag
        if (st.sprRectAnchor && !st.sprSelFloat && st.sprHoverX >= 0) {
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
        // Committed selection rect (marching ants = dashed rect)
        if (st.sprSelRect) {
            let sr = st.sprSelRect;
            gfx.rect(
                canvasOx + sr.x0 * zoom,
                canvasOy + sr.y0 * zoom,
                canvasOx + (sr.x1 + 1) * zoom - 1,
                canvasOy + (sr.y1 + 1) * zoom - 1,
                FG,
            );
        }
        // Floating selection
        if (st.sprSelFloat) {
            let f = st.sprSelFloat;
            for (let py = 0; py < f.h; py++) {
                for (let px = 0; px < f.w; px++) {
                    let col = f.pixels[py * f.w + px];
                    if (col !== 0) {
                        let dx = f.x + px;
                        let dy = f.y + py;
                        if (dx >= 0 && dx < cw && dy >= 0 && dy < ch) {
                            gfx.rectfill(
                                canvasOx + dx * zoom,
                                canvasOy + dy * zoom,
                                canvasOx + (dx + 1) * zoom - 1,
                                canvasOy + (dy + 1) * zoom - 1,
                                col,
                            );
                        }
                    }
                }
            }
            gfx.rect(
                canvasOx + f.x * zoom,
                canvasOy + f.y * zoom,
                canvasOx + (f.x + f.w) * zoom - 1,
                canvasOy + (f.y + f.h) * zoom - 1,
                FG,
            );
        }
    }

    // Symmetry axis lines
    if (st.sprMirrorX) {
        let mx2 = canvasOx + Math.floor(cw / 2) * zoom;
        gfx.line(mx2, canvasOy, mx2, canvasOy + canvasPixH - 1, 8);
    }
    if (st.sprMirrorY) {
        let my2 = canvasOy + Math.floor(ch / 2) * zoom;
        gfx.line(canvasOx, my2, canvasOx + canvasPixW - 1, my2, 8);
    }

    // Reset clip after canvas drawing
    gfx.clip();

    // 2× sprite preview (fixed position, top-right of canvas panel)
    let prevScale = 2;
    let prevW = cw * prevScale;
    let prevH = ch * prevScale;
    let prevX = FB_W - prevW - 4;
    let prevY = SPR_CANVAS_Y + 2;
    gfx.rectfill(prevX - 1, prevY - 1, prevX + prevW, prevY + prevH, GRIDC);
    gfx.sspr(base.x, base.y, cw, ch, prevX, prevY, prevW, prevH);

    // ── Animation preview ──
    if (st.sprAnimPlay && st.sprAnimFrom < st.sprAnimTo) {
        let animIdx = st.sprAnimFrom + st.sprAnimFrame;
        let animBase = selBase(sheetCols);
        // Offset from sprSel to animIdx in sheet coords
        let animOffCol = (animIdx % sheetCols) - (st.sprSel % sheetCols);
        let animOffRow = Math.floor(animIdx / sheetCols) - Math.floor(st.sprSel / sheetCols);
        let animSx = animBase.x + animOffCol * tileW;
        let animSy = animBase.y + animOffRow * tileH;
        let animPrevY = prevY + prevH + 4;
        gfx.rectfill(prevX - 1, animPrevY - 1, prevX + prevW, animPrevY + prevH, GRIDC);
        gfx.sspr(animSx, animSy, cw, ch, prevX, animPrevY, prevW, prevH);
        let animTxt = "#" + animIdx + " " + st.sprAnimFps + "fps";
        gfx.print(animTxt, prevX + prevW - gfx.textWidth(animTxt), animPrevY + prevH + 2, GUTFG);
    } else if (st.sprAnimFrom !== st.sprAnimTo) {
        // Show configured range even when not playing
        let rangeTxt = "ANIM #" + st.sprAnimFrom + "-" + st.sprAnimTo + " " + st.sprAnimFps + "fps";
        gfx.print(rangeTxt, prevX + prevW - gfx.textWidth(rangeTxt), prevY + prevH + 4, GUTFG);
    }

    // ── Palette grid (below sprite grid, left panel) ──
    let palX = SPR_GRID_X;
    let palY = SPR_GRID_Y + gridContentH + 2;
    let palTotalRows = Math.ceil(256 / SPR_PAL_COLS);
    let palAvailH = FB_H - SPR_FOOT_H - palY;
    let palVisRows = Math.min(palTotalRows, Math.floor(palAvailH / SPR_PAL_CELL));
    let palH = palVisRows * SPR_PAL_CELL;

    // Separator line between grid and palette
    gfx.line(SPR_GRID_X, palY - 1, SPR_GRID_X + SPR_GRID_W - 1, palY - 1, SEPC);

    // Clip palette drawing to its area
    gfx.clip(palX, palY, SPR_GRID_W, palH);
    for (let r = 0; r < palVisRows; r++) {
        let row = r + st.sprPalScrollY;
        for (let c = 0; c < SPR_PAL_COLS; c++) {
            let colIdx = row * SPR_PAL_COLS + c;
            if (colIdx >= 256) break;
            let px = palX + c * SPR_PAL_CELL;
            let py = palY + r * SPR_PAL_CELL;
            gfx.rectfill(px, py, px + SPR_PAL_CELL - 1, py + SPR_PAL_CELL - 1, colIdx);
            if (colIdx === st.sprCol) {
                gfx.rect(px, py, px + SPR_PAL_CELL - 1, py + SPR_PAL_CELL - 1, FG);
            }
        }
    }
    gfx.clip();

    // Scroll indicator (if not all rows visible)
    if (palVisRows < palTotalRows) {
        let sbH = Math.max(4, Math.floor((palH * palVisRows) / palTotalRows));
        let sbY =
            palY +
            Math.floor(((palH - sbH) * st.sprPalScrollY) / Math.max(1, palTotalRows - palVisRows));
        gfx.rectfill(palX + SPR_GRID_W - 3, sbY, palX + SPR_GRID_W - 1, sbY + sbH - 1, GUTFG);
    }

    // ── Flags display (below palette, left panel) ──
    let flagsY = palY + palH + 2;
    let flags = gfx.fget(st.sprSel);
    gfx.print("FLAGS:", palX, flagsY, GUTFG);
    for (let b = 0; b < 8; b++) {
        let bx = palX + 7 * CW + b * (CW + 2);
        let on = (flags >> b) & 1;
        gfx.print("" + b, bx, flagsY, on ? FG : GUTFG);
        if (on) gfx.rectfill(bx, flagsY + CH, bx + CW - 1, flagsY + CH + 1, FG);
    }

    // ── Shortcuts (right panel, below canvas) ──
    let shortcutsY = FB_H - SPR_FOOT_H - (CH + 2) * 2 - 2;
    gfx.print(
        "B:pen E:era F:fill R:rect L:lin O:cir S:sel  G:fill M:size N:goto",
        SPR_CANVAS_X,
        shortcutsY,
        GUTFG,
    );
    gfx.print(
        "P:anim A/Sh+A:range [/]:fps  Sh+X/Y:mir  scroll:zoom  mid:pan",
        SPR_CANVAS_X,
        shortcutsY + CH + 2,
        GUTFG,
    );

    // ── Goto sprite overlay ──
    if (st.sprGoto) {
        let goTxt = "Go to sprite #: " + st.sprGotoTxt + "_";
        let goW = gfx.textWidth(goTxt) + CW * 2;
        let goX = SPR_CANVAS_X + Math.floor((SPR_CANVAS_W - goW) / 2);
        let goY = SPR_CANVAS_Y + Math.floor(SPR_CANVAS_H / 2) - CH;
        gfx.rectfill(goX - 2, goY - 2, goX + goW + 1, goY + CH + 3, FOOTBG);
        gfx.rect(goX - 2, goY - 2, goX + goW + 1, goY + CH + 3, FG);
        gfx.print(goTxt, goX + CW, goY + 1, FG);
    }

    // ── Footer (status bar) ──
    let footY = FB_H - SPR_FOOT_H;
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, FOOTBG);
    let footTextY = footY + Math.floor((SPR_FOOT_H - CH) / 2);
    gfx.print(
        "TOOL:" + SPR_TOOLS[st.sprTool] + (st.sprFilled ? "*" : ""),
        1 * CW,
        footTextY,
        FOOTFG,
    );
    gfx.print("COL:" + st.sprCol, 14 * CW, footTextY, FOOTFG);
    gfx.rectfill(19 * CW, footTextY, 19 * CW + CH - 1, footTextY + CH - 1, st.sprCol);
    gfx.print("#" + st.sprSel, 22 * CW, footTextY, FG);
    gfx.print(st.sprSizeW + "x" + st.sprSizeH, 28 * CW, footTextY, FOOTFG);
    if (st.sprZoom > 0) {
        gfx.print(zoom + "x", 32 * CW, footTextY, GUTFG);
    }
    // Status indicators
    let statusX = 36;
    if (st.sprMirrorX || st.sprMirrorY) {
        let mirTxt = "MIR:" + (st.sprMirrorX ? "X" : "") + (st.sprMirrorY ? "Y" : "");
        gfx.print(mirTxt, statusX * CW, footTextY, 8);
        statusX += mirTxt.length + 1;
    }
    if (st.sprAnimPlay) {
        gfx.print("ANIM", statusX * CW, footTextY, 11);
        statusX += 5;
    }
    if (st.sprHoverX >= 0) {
        gfx.print("(" + st.sprHoverX + "," + st.sprHoverY + ")", statusX * CW, footTextY, FOOTFG);
    }
    // Timed status message (right-aligned, like code editor footer)
    if (st.msg) {
        let rw = gfx.textWidth(st.msg);
        gfx.print(st.msg, FB_W - rw - CW, footTextY, FOOTFG);
    }
}
