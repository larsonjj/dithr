// ─── Map editor ──────────────────────────────────────────────────────────────
//
// Layout (640×360):
//   Row 0         : tab bar (shared drawTabBar)
//   Left panel    : map viewport with tile painting
//   Right panel   : tile picker (spritesheet as palette)
//   Bottom strip  : layer selector, info, grid toggle
//

import { st } from "./state.js";
import { FB_W, FB_H, CW, CH, TAB_H, FG, GUTBG, FOOTBG, FOOTFG, GRIDC } from "./config.js";
import { clamp, modKey } from "./helpers.js";

// ─── Constants ───────────────────────────────────────────────────────────────

const MAP_TILE_PX = 8; // default tile size in pixels
const MAP_PICKER_W = 130; // right panel width
const MAP_VP_X = 0;
const MAP_VP_Y = TAB_H; // below tab bar
const MAP_VP_W = FB_W - MAP_PICKER_W;
const MAP_VP_H = FB_H - TAB_H - CH; // minus tab bar and footer
const MAP_PICK_X = MAP_VP_W + 1;
const MAP_PICK_Y = TAB_H;
const MAP_PICK_CELL = 10; // tile picker cell size
const MAP_PICK_COLS = Math.floor(MAP_PICKER_W / MAP_PICK_CELL);

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateMapEditor() {
    let ctrl = modKey();

    let mx = mouse.x();
    let my = mouse.y();
    let mBtn = mouse.btn(0);
    let mPress = mouse.btnp(0);
    let wheel = mouse.wheel();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    let mw = map.width();
    let mh = map.height();
    if (mw === 0 || mh === 0) return; // no map loaded

    let sw = gfx.sheetW();
    let sh = gfx.sheetH();
    let tileW = MAP_TILE_PX;
    let tileH = MAP_TILE_PX;
    let sheetCols = sw > 0 ? Math.floor(sw / tileW) : 0;
    let sheetCount = sheetCols > 0 ? sheetCols * Math.floor(sh / tileH) : 0;

    // ── Map viewport panning (arrow keys / WASD) ──
    let speed = shift ? 4 : 1;
    if (key.btn(key.LEFT) || key.btn(key.A)) st.mapCamX -= speed;
    if (key.btn(key.RIGHT) || key.btn(key.D)) st.mapCamX += speed;
    if (key.btn(key.UP) || key.btn(key.W)) st.mapCamY -= speed;
    if (key.btn(key.DOWN) || key.btn(key.S)) st.mapCamY += speed;
    st.mapCamX = clamp(st.mapCamX, 0, Math.max(0, mw - Math.floor(MAP_VP_W / tileW)));
    st.mapCamY = clamp(st.mapCamY, 0, Math.max(0, mh - Math.floor(MAP_VP_H / tileH)));

    // ── Paint tile on map viewport ──
    if (
        mBtn &&
        mx >= MAP_VP_X &&
        mx < MAP_VP_X + MAP_VP_W &&
        my >= MAP_VP_Y &&
        my < MAP_VP_Y + MAP_VP_H
    ) {
        let tx = Math.floor((mx - MAP_VP_X) / tileW) + st.mapCamX;
        let ty = Math.floor((my - MAP_VP_Y) / tileH) + st.mapCamY;
        if (tx >= 0 && tx < mw && ty >= 0 && ty < mh) {
            let newTile = st.mapTool === 0 ? st.mapTile : 0;
            let prev = map.get(tx, ty, st.mapLayer);
            if (prev !== newTile) {
                st.mapUndoPending.push({ x: tx, y: ty, layer: st.mapLayer, prev: prev });
                map.set(tx, ty, st.mapLayer, newTile);
            }
        }
    }

    // Pick tile from map (right-click)
    if (
        mouse.btn(1) &&
        mx >= MAP_VP_X &&
        mx < MAP_VP_X + MAP_VP_W &&
        my >= MAP_VP_Y &&
        my < MAP_VP_Y + MAP_VP_H
    ) {
        let tx = Math.floor((mx - MAP_VP_X) / tileW) + st.mapCamX;
        let ty = Math.floor((my - MAP_VP_Y) / tileH) + st.mapCamY;
        if (tx >= 0 && tx < mw && ty >= 0 && ty < mh) {
            st.mapTile = map.get(tx, ty, st.mapLayer);
            st.mapTool = 0;
        }
    }

    // ── Tile picker interaction ──
    if (
        mPress &&
        mx >= MAP_PICK_X &&
        mx < MAP_PICK_X + MAP_PICK_COLS * MAP_PICK_CELL &&
        my >= MAP_PICK_Y
    ) {
        let pc = Math.floor((mx - MAP_PICK_X) / MAP_PICK_CELL);
        let pr = Math.floor((my - MAP_PICK_Y) / MAP_PICK_CELL);
        let idx = pr * MAP_PICK_COLS + pc;
        if (idx >= 0 && idx < sheetCount) st.mapTile = idx;
    }

    // ── Layer switching (keyboard) ──
    let layers = map.layers().length;
    if (st.mapLayer >= layers) st.mapLayer = Math.max(0, layers - 1);
    if (key.btnp(key.LBRACKET) && st.mapLayer > 0) st.mapLayer--;
    if (key.btnp(key.RBRACKET) && st.mapLayer < layers - 1) st.mapLayer++;

    // Commit stroke on mouse release
    if (!mBtn && st.mapUndoPending.length > 0) {
        st.mapUndoStack.push(st.mapUndoPending);
        st.mapUndoPending = [];
        if (st.mapUndoStack.length > st.MAP_MAX_UNDO) st.mapUndoStack.shift();
    }

    // Undo (Ctrl+Z)
    if (ctrl && key.btnp(key.Z) && st.mapUndoStack.length > 0) {
        let stroke = st.mapUndoStack.pop();
        for (let i = stroke.length - 1; i >= 0; i--) {
            map.set(stroke[i].x, stroke[i].y, stroke[i].layer, stroke[i].prev);
        }
    }

    // ── Grid toggle ──
    if (key.btnp(key.G) && !ctrl) st.mapGridOn = !st.mapGridOn;

    // ── Tool switch ──
    if (key.btnp(key.E)) st.mapTool = 1;
    if (key.btnp(key.B)) st.mapTool = 0;
}

// ─── Draw ────────────────────────────────────────────────────────────────────

export function drawMapEditor() {
    let mw = map.width();
    let mh = map.height();
    if (mw === 0 || mh === 0) {
        gfx.print("No map loaded", 10, FB_H / 2, FG);
        return;
    }

    let sw = gfx.sheetW();
    let sh = gfx.sheetH();
    let tileW = MAP_TILE_PX;
    let tileH = MAP_TILE_PX;
    let sheetCols = sw > 0 ? Math.floor(sw / tileW) : 0;
    let sheetRows = sw > 0 ? Math.floor(sh / tileH) : 0;
    let sheetCount = sheetCols * sheetRows;

    // ── Map viewport ──
    gfx.clip(MAP_VP_X, MAP_VP_Y, MAP_VP_W, MAP_VP_H);

    // Draw the map using map.draw — it draws all visible tiles
    let drawX = MAP_VP_X - ((st.mapCamX * tileW) % tileW);
    let drawY = MAP_VP_Y - ((st.mapCamY * tileH) % tileH);
    let visCols = Math.ceil(MAP_VP_W / tileW) + 1;
    let visRows = Math.ceil(MAP_VP_H / tileH) + 1;

    // Draw each visible tile manually so we control the layer
    for (let r = 0; r < visRows; r++) {
        for (let c = 0; c < visCols; c++) {
            let tx = st.mapCamX + c;
            let ty = st.mapCamY + r;
            if (tx >= mw || ty >= mh) continue;
            let tile = map.get(tx, ty, st.mapLayer);
            if (tile > 0 && sheetCols > 0) {
                let srcX = (tile % sheetCols) * tileW;
                let srcY = Math.floor(tile / sheetCols) * tileH;
                let dx = MAP_VP_X + c * tileW;
                let dy = MAP_VP_Y + r * tileH;
                gfx.sspr(srcX, srcY, tileW, tileH, dx, dy, tileW, tileH);
            }
        }
    }

    // Grid overlay
    if (st.mapGridOn) {
        for (let c = 0; c <= visCols; c++) {
            let x = MAP_VP_X + c * tileW;
            gfx.line(x, MAP_VP_Y, x, MAP_VP_Y + MAP_VP_H - 1, GRIDC);
        }
        for (let r = 0; r <= visRows; r++) {
            let y = MAP_VP_Y + r * tileH;
            gfx.line(MAP_VP_X, y, MAP_VP_X + MAP_VP_W - 1, y, GRIDC);
        }
    }

    gfx.clip(); // reset clip

    // ── Tile picker panel ──
    gfx.rectfill(MAP_PICK_X, MAP_PICK_Y, FB_W - 1, FB_H - CH - 1, GUTBG);

    if (sheetCols > 0) {
        let pickRows = Math.ceil(sheetCount / MAP_PICK_COLS);
        let visPickRows = Math.floor((FB_H - CH * 2) / MAP_PICK_CELL);
        for (let r = 0; r < visPickRows && r < pickRows; r++) {
            for (let c = 0; c < MAP_PICK_COLS; c++) {
                let idx = r * MAP_PICK_COLS + c;
                if (idx >= sheetCount) break;
                let dx = MAP_PICK_X + c * MAP_PICK_CELL;
                let dy = MAP_PICK_Y + r * MAP_PICK_CELL;
                let srcX = (idx % sheetCols) * tileW;
                let srcY = Math.floor(idx / sheetCols) * tileH;
                gfx.sspr(srcX, srcY, tileW, tileH, dx, dy, MAP_PICK_CELL, MAP_PICK_CELL);
                if (idx === st.mapTile) {
                    gfx.rect(dx, dy, dx + MAP_PICK_CELL - 1, dy + MAP_PICK_CELL - 1, FG);
                }
            }
        }
    }

    // ── Footer ──
    let footY = FB_H - CH;
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, FOOTBG);
    let layers = map.layers().length;
    let info =
        "L:" +
        st.mapLayer +
        "/" +
        (layers - 1) +
        " T:" +
        st.mapTile +
        " G:" +
        (st.mapGridOn ? "on" : "off") +
        " (" +
        st.mapCamX +
        "," +
        st.mapCamY +
        ")";
    gfx.print(info, 1 * CW, footY, FOOTFG);
    gfx.print("[/]:layer B:pen E:era G:grid", FB_W - 30 * CW, footY, FOOTFG);
}
