// ─── Map editor ──────────────────────────────────────────────────────────────
//
// Layout (640×360):
//   Row 0         : tab bar (shared drawTabBar)
//   Left panel    : map viewport with tile painting
//   Right panel   : tile picker (spritesheet as palette)
//   Bottom strip  : layer selector, info, grid toggle
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
    FOOTBG,
    FOOTFG,
    GRIDC,
    FOOT_H,
    PANELBG,
    SEPC,
    SELBG,
} from "./config.js";
import { clamp, modKey, MOD_NAME, status } from "./helpers.js";
import { createHistory, record, commit, undo, redo } from "./stroke_history.js";

// ─── Constants ───────────────────────────────────────────────────────────────

const TOOL_PEN = 0;
const TOOL_ERA = 1;
const TOOL_FILL = 2;
const TOOL_RECT = 3;
const TOOL_SEL = 4;
const TOOL_OBJ = 5;

const MAP_TILE_PX = 8; // default tile size in pixels
const MAP_PICKER_W = 130; // right panel width
const MAP_VP_X = 0;
const MAP_VP_Y = TAB_H; // below tab bar
const MAP_VP_W = FB_W - MAP_PICKER_W;
const MAP_VP_H = FB_H - TAB_H - FOOT_H; // minus tab bar and footer
const MAP_PICK_X = MAP_VP_W + 1;
const MAP_PICK_Y = TAB_H;
const MAP_PICK_CELL = 10; // tile picker cell size
const MAP_PICK_COLS = Math.floor(MAP_PICKER_W / MAP_PICK_CELL);
const LAYER_ROW_H = 10;

const ZOOM_LEVELS = [1, 2, 3, 4];
const GHOST_COLOR = 1; // dim palette index for ghost layer tint

let mapHist = createHistory(100);

// ─── Helpers ─────────────────────────────────────────────────────────────────

function tileSize() {
    return MAP_TILE_PX * st.mapZoom;
}

/** Commit wrapper that also marks dirty state. */
function mapCommit() {
    commit(mapHist);
    st.mapDirty = true;
}

/** Paint a single tile, recording undo. Tile is a 1-based map ID (0=empty). */
function paintTile(tx, ty, newTile) {
    let mw = map.width();
    let mh = map.height();
    if (tx < 0 || tx >= mw || ty < 0 || ty >= mh) return;
    let prev = map.get(tx, ty, st.mapLayer);
    if (prev !== newTile) {
        record(mapHist, { x: tx, y: ty, layer: st.mapLayer, prev: prev, next: newTile });
        map.set(tx, ty, newTile, st.mapLayer);
    }
}

/** Convert 0-based sprite index to 1-based map tile ID. */
function sprToTile(sprIdx) {
    return sprIdx + 1;
}

/** Convert 1-based map tile ID to 0-based sprite index (-1 if empty). */
function tileToSpr(tileId) {
    return tileId > 0 ? tileId - 1 : -1;
}

/** Bresenham line between two tile coords, calling paintTile for each. */
function bresenhamLine(x0, y0, x1, y1, tile) {
    let ddx = Math.abs(x1 - x0);
    let ddy = Math.abs(y1 - y0);
    let sx = x0 < x1 ? 1 : -1;
    let sy = y0 < y1 ? 1 : -1;
    let err = ddx - ddy;
    while (true) {
        paintTile(x0, y0, tile);
        if (x0 === x1 && y0 === y1) break;
        let e2 = 2 * err;
        if (e2 > -ddy) {
            err -= ddy;
            x0 += sx;
        }
        if (e2 < ddx) {
            err += ddx;
            y0 += sy;
        }
    }
}

/** Flood fill from (sx, sy) replacing target tile with replacement. */
function floodFill(sx, sy, replacement) {
    let mw = map.width();
    let mh = map.height();
    if (sx < 0 || sx >= mw || sy < 0 || sy >= mh) return;
    let target = map.get(sx, sy, st.mapLayer);
    if (target === replacement) return;

    let stack = [[sx, sy]];
    let visited = new Set();
    while (stack.length > 0) {
        let pt = stack.pop();
        let px = pt[0];
        let py = pt[1];
        if (px < 0 || px >= mw || py < 0 || py >= mh) continue;
        let kk = py * mw + px;
        if (visited.has(kk)) continue;
        visited.add(kk);
        if (map.get(px, py, st.mapLayer) !== target) continue;
        paintTile(px, py, replacement);
        stack.push([px - 1, py]);
        stack.push([px + 1, py]);
        stack.push([px, py - 1]);
        stack.push([px, py + 1]);
    }
}

/** Convert mouse screen coords to tile coords. Returns null if outside viewport. */
function screenToTile(mx, my) {
    let ts = tileSize();
    if (mx < MAP_VP_X || mx >= MAP_VP_X + MAP_VP_W || my < MAP_VP_Y || my >= MAP_VP_Y + MAP_VP_H)
        return null;
    let tx = Math.floor((mx - MAP_VP_X) / ts) + st.mapCamX;
    let ty = Math.floor((my - MAP_VP_Y) / ts) + st.mapCamY;
    return { x: tx, y: ty };
}

/** Paint stamp pattern at tile position (tx,ty). Uses mapStampRect for multi-tile. */
function paintStamp(tx, ty) {
    let sr = st.mapStampRect;
    if (!sr) {
        paintTile(tx, ty, sprToTile(st.mapTile));
        return;
    }
    let sw = gfx.sheetW();
    let sheetCols = sw > 0 ? Math.floor(sw / MAP_TILE_PX) : 0;
    if (sheetCols <= 0) return;
    let stampW = sr.x1 - sr.x0 + 1;
    let stampH = sr.y1 - sr.y0 + 1;
    for (let dy = 0; dy < stampH; dy++) {
        for (let dx = 0; dx < stampW; dx++) {
            let sprIdx = (sr.y0 + dy) * sheetCols + (sr.x0 + dx);
            paintTile(tx + dx, ty + dy, sprToTile(sprIdx));
        }
    }
}

/** Lift tiles from selection rect into a floating selection. */
function liftSelection() {
    let r = st.mapSelRect;
    if (!r) return;
    let w = r.x1 - r.x0 + 1;
    let h = r.y1 - r.y0 + 1;
    let tiles = [];
    for (let ty = r.y0; ty <= r.y1; ty++) {
        for (let tx = r.x0; tx <= r.x1; tx++) {
            let t = map.get(tx, ty, st.mapLayer);
            tiles.push(t);
            paintTile(tx, ty, 0);
        }
    }
    if (mapHist.pending.length > 0) mapCommit();
    st.mapSelFloat = { x: r.x0, y: r.y0, w: w, h: h, tiles: tiles };
    st.mapSelRect = null;
}

/** Drop floating selection back onto the map. */
function dropSelection() {
    let f = st.mapSelFloat;
    if (!f) return;
    for (let dy = 0; dy < f.h; dy++) {
        for (let dx = 0; dx < f.w; dx++) {
            let t = f.tiles[dy * f.w + dx];
            if (t > 0) paintTile(f.x + dx, f.y + dy, t);
        }
    }
    if (mapHist.pending.length > 0) mapCommit();
    st.mapSelFloat = null;
    st.mapSelDrag = null;
    st.mapSelRect = null;
}

// ─── Auto-tile helpers ───────────────────────────────────────────────────────

/** Return the auto-tile group base for a sprite index, or -1 if not in any group. */
function getAutoGroup(sprIdx) {
    for (let i = 0; i < st.mapAutoGroups.length; i++) {
        let base = st.mapAutoGroups[i];
        if (sprIdx >= base && sprIdx < base + 16) return base;
    }
    return -1;
}

/** Compute 4-bit bitmask for a tile at (tx,ty) based on NESW same-group neighbors. */
function autoTileMask(tx, ty, base) {
    let mw = map.width();
    let mh = map.height();
    let mask = 0;
    // N
    if (ty > 0) {
        let t = tileToSpr(map.get(tx, ty - 1, st.mapLayer));
        if (t >= base && t < base + 16) mask |= 1;
    }
    // E
    if (tx < mw - 1) {
        let t = tileToSpr(map.get(tx + 1, ty, st.mapLayer));
        if (t >= base && t < base + 16) mask |= 2;
    }
    // S
    if (ty < mh - 1) {
        let t = tileToSpr(map.get(tx, ty + 1, st.mapLayer));
        if (t >= base && t < base + 16) mask |= 4;
    }
    // W
    if (tx > 0) {
        let t = tileToSpr(map.get(tx - 1, ty, st.mapLayer));
        if (t >= base && t < base + 16) mask |= 8;
    }
    return mask;
}

/** Re-resolve a single auto-tile at (tx,ty) if it belongs to a group. */
function resolveAutoTile(tx, ty) {
    let mw = map.width();
    let mh = map.height();
    if (tx < 0 || tx >= mw || ty < 0 || ty >= mh) return;
    let cur = tileToSpr(map.get(tx, ty, st.mapLayer));
    if (cur < 0) return;
    let base = getAutoGroup(cur);
    if (base < 0) return;
    let mask = autoTileMask(tx, ty, base);
    let want = sprToTile(base + mask);
    if (map.get(tx, ty, st.mapLayer) !== want) {
        record(mapHist, {
            x: tx,
            y: ty,
            layer: st.mapLayer,
            prev: map.get(tx, ty, st.mapLayer),
            next: want,
        });
        map.set(tx, ty, want, st.mapLayer);
    }
}

/** Paint with auto-tile: place tile and update NESW neighbors. */
function paintAutoTile(tx, ty, sprIdx) {
    let base = getAutoGroup(sprIdx);
    if (base < 0) {
        paintTile(tx, ty, sprToTile(sprIdx));
        return;
    }
    let prev = map.get(tx, ty, st.mapLayer);
    // Place tile so neighbor lookups include it
    let mask = autoTileMask(tx, ty, base);
    map.set(tx, ty, sprToTile(base + mask), st.mapLayer);
    // Recompute now that the tile is placed
    mask = autoTileMask(tx, ty, base);
    let want = sprToTile(base + mask);
    map.set(tx, ty, want, st.mapLayer);
    // Record single undo op
    if (prev !== want) {
        record(mapHist, { x: tx, y: ty, layer: st.mapLayer, prev: prev, next: want });
    }
    // Update NESW neighbors
    resolveAutoTile(tx - 1, ty);
    resolveAutoTile(tx + 1, ty);
    resolveAutoTile(tx, ty - 1);
    resolveAutoTile(tx, ty + 1);
}

/** Erase with auto-tile: clear tile and update NESW neighbors. */
function eraseAutoTile(tx, ty) {
    let cur = tileToSpr(map.get(tx, ty, st.mapLayer));
    paintTile(tx, ty, 0);
    if (cur >= 0 && getAutoGroup(cur) >= 0) {
        resolveAutoTile(tx - 1, ty);
        resolveAutoTile(tx + 1, ty);
        resolveAutoTile(tx, ty - 1);
        resolveAutoTile(tx, ty + 1);
    }
}

/** Check if current layer is an object layer. */
function isObjectLayer(idx) {
    return map.layer_type(idx) === "objectgroup";
}

/** Generate Tiled .tmj JSON string from current map data. */
function exportTiledJSON() {
    let d = map.data();
    if (!d) return null;
    let tw = d.tileW || 8;
    let th = d.tileH || 8;
    let imgW = gfx.sheetW();
    let imgH = gfx.sheetH();
    let cols = Math.floor(imgW / tw);
    let count = cols * Math.floor(imgH / th);
    let tmj = {
        compressionlevel: -1,
        height: d.height,
        infinite: false,
        layers: [],
        nextlayerid: d.layers.length + 1,
        nextobjectid: 1,
        orientation: "orthogonal",
        renderorder: "right-down",
        tiledversion: "1.11",
        tileheight: th,
        tilesets: [
            {
                columns: cols,
                firstgid: 1,
                image: "spritesheet.png",
                imageheight: imgH,
                imagewidth: imgW,
                margin: 0,
                name: "spritesheet",
                spacing: 0,
                tilecount: count,
                tileheight: th,
                tilewidth: tw,
            },
        ],
        tilewidth: tw,
        type: "map",
        version: "1.11",
        width: d.width,
    };
    for (let i = 0; i < d.layers.length; i++) {
        let l = d.layers[i];
        if (l.type === "objectgroup") {
            let objs = (l.objects || []).map(function (o, oi) {
                return {
                    id: oi + 1,
                    name: o.name || "",
                    type: o.type || "",
                    x: o.x,
                    y: o.y,
                    width: o.w,
                    height: o.h,
                    gid: o.gid || 0,
                    visible: true,
                };
            });
            tmj.layers.push({
                id: i + 1,
                name: l.name || "Objects " + i,
                type: "objectgroup",
                objects: objs,
                opacity: 1,
                visible: true,
                x: 0,
                y: 0,
            });
        } else {
            tmj.layers.push({
                data: l.data,
                height: l.height,
                id: i + 1,
                name: l.name || "Layer " + i,
                opacity: 1,
                type: "tilelayer",
                visible: true,
                width: l.width,
                x: 0,
                y: 0,
            });
        }
    }
    return JSON.stringify(tmj);
}

/** Ensure mapLayerVis array has enough slots. */
function ensureLayerVis(count) {
    while (st.mapLayerVis.length < count) st.mapLayerVis.push(true);
}

/** Check if layer is visible. */
function isLayerVisible(idx) {
    if (idx >= st.mapLayerVis.length) return true;
    return st.mapLayerVis[idx];
}

/** Apply a single undo/redo op — handles tile ops and layer structural ops. */
function applyMapOp(op) {
    if (op.type === "addLayer") {
        let idx = op.idx;
        let mw = map.width();
        let mh = map.height();
        let tiles = [];
        for (let ty = 0; ty < mh; ty++)
            for (let tx = 0; tx < mw; tx++) tiles.push(map.get(tx, ty, idx));
        let lrs = map.layers();
        let name = lrs[idx] || "Layer";
        map.remove_layer(idx);
        if (idx < st.mapLayerVis.length) st.mapLayerVis.splice(idx, 1);
        if (st.mapLayer >= map.layers().length) st.mapLayer = Math.max(0, map.layers().length - 1);
        return { type: "removeLayer", idx: idx, name: name, tiles: tiles };
    }
    if (op.type === "removeLayer") {
        let newIdx = map.add_layer(op.name);
        if (newIdx >= 0) {
            let mw = map.width();
            let mh = map.height();
            for (let ty = 0; ty < mh; ty++)
                for (let tx = 0; tx < mw; tx++) {
                    let t = op.tiles[ty * mw + tx];
                    if (t !== 0) map.set(tx, ty, t, newIdx);
                }
            ensureLayerVis(map.layers().length);
            st.mapLayer = newIdx;
        }
        return { type: "addLayer", idx: newIdx >= 0 ? newIdx : 0, name: op.name };
    }
    if (op.type === "addObj") {
        map.remove_object(op.layer, op.idx);
        st.mapObjSel = -1;
        return { type: "removeObj", layer: op.layer, idx: op.idx, obj: op.obj };
    }
    if (op.type === "removeObj") {
        let idx = map.add_object(op.layer, op.obj);
        return { type: "addObj", layer: op.layer, idx: idx >= 0 ? idx : 0, obj: op.obj };
    }
    if (op.type === "moveObj") {
        map.set_object(op.layer, op.idx, { x: op.prevX, y: op.prevY });
        return {
            type: "moveObj",
            layer: op.layer,
            idx: op.idx,
            prevX: op.nextX,
            prevY: op.nextY,
            nextX: op.prevX,
            nextY: op.prevY,
        };
    }
    if (op.type === "resizeObj") {
        map.set_object(op.layer, op.idx, { w: op.prevW, h: op.prevH });
        return {
            type: "resizeObj",
            layer: op.layer,
            idx: op.idx,
            prevW: op.nextW,
            prevH: op.nextH,
            nextW: op.prevW,
            nextH: op.prevH,
        };
    }
    map.set(op.x, op.y, op.prev, op.layer);
    return { x: op.x, y: op.y, layer: op.layer, prev: op.next, next: op.prev };
}

/** Drop selection and clear selection state. */
function clearSel() {
    if (st.mapSelFloat) dropSelection();
    st.mapSelRect = null;
    st.mapSelAnchor = null;
}

/** Handle text input forwarded from main.js for resize/rename dialog. */
export function mapTextInput(ch) {
    if (st.mapResizeMode && ch >= "0" && ch <= "9") {
        if (st.mapResizeField === 0 && st.mapResizeW.length < 4) st.mapResizeW += ch;
        else if (st.mapResizeField === 1 && st.mapResizeH.length < 4) st.mapResizeH += ch;
    }
    if (st.mapRenameMode && ch.length === 1 && ch >= " " && st.mapRenameTxt.length < 31) {
        st.mapRenameTxt += ch;
    }
}

/** Save map data to disk as JSON. */
export function saveMapToDisk() {
    let data = map.data();
    if (data) {
        let json = JSON.stringify(data);
        let ok = sys.writeFile("map.json", json);
        status(ok ? "Map saved" : "Map save failed");
        if (ok) st.mapDirty = false;
    }
}

// ─── Update ──────────────────────────────────────────────────────────────────

export function updateMapEditor() {
    let ctrl = modKey();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    let mx = mouse.x();
    let my = mouse.y();
    let mBtn = mouse.btn(0);
    let mPress = mouse.btnp(0);
    let mRelease =
        !mBtn && (st.mapLastPenX >= 0 || mapHist.pending.length > 0 || st.mapSelAnchor !== null);
    let wheel = mouse.wheel();

    let mw = map.width();
    let mh = map.height();

    // ── Auto-create map if none exists ──
    if (mw === 0 || mh === 0) {
        map.create(64, 64, "default");
        return;
    }

    // ── Resize dialog mode ──
    if (st.mapResizeMode) {
        updateResizeDialog();
        return;
    }

    // ── Layer rename dialog mode ──
    if (st.mapRenameMode) {
        updateRenameDialog();
        return;
    }

    // ── Level picker mode ──
    if (st.mapLevelMode) {
        updateLevelPicker();
        return;
    }

    let sw = gfx.sheetW();
    let sh = gfx.sheetH();
    let sheetCols = sw > 0 ? Math.floor(sw / MAP_TILE_PX) : 0;
    let sheetCount = sheetCols > 0 ? sheetCols * Math.floor(sh / MAP_TILE_PX) : 0;
    let ts = tileSize();
    let layers = map.layers();

    // ── Zoom (Ctrl+wheel or +/-) ──
    if (ctrl && wheel !== 0) {
        let zi = ZOOM_LEVELS.indexOf(st.mapZoom);
        if (zi < 0) zi = 0;
        zi = clamp(zi + (wheel > 0 ? 1 : -1), 0, ZOOM_LEVELS.length - 1);
        st.mapZoom = ZOOM_LEVELS[zi];
        clampCam();
        return;
    }
    if (key.btnp(key.MINUS) && !ctrl) {
        let zi = ZOOM_LEVELS.indexOf(st.mapZoom);
        st.mapZoom = ZOOM_LEVELS[clamp((zi < 0 ? 0 : zi) - 1, 0, ZOOM_LEVELS.length - 1)];
        clampCam();
    }
    if (key.btnp(key.EQUALS) && !ctrl) {
        let zi = ZOOM_LEVELS.indexOf(st.mapZoom);
        st.mapZoom = ZOOM_LEVELS[clamp((zi < 0 ? 0 : zi) + 1, 0, ZOOM_LEVELS.length - 1)];
        clampCam();
    }

    // ── Map viewport panning (arrow keys / WASD) ──
    let speed = shift ? 4 : 1;
    if (key.btn(key.LEFT) || key.btn(key.A)) st.mapCamX -= speed;
    if (key.btn(key.RIGHT) || key.btn(key.D)) st.mapCamX += speed;
    if (key.btn(key.UP) || key.btn(key.W)) st.mapCamY -= speed;
    if (key.btn(key.DOWN) || (key.btn(key.S) && !ctrl)) st.mapCamY += speed;
    clampCam();

    // ── Middle-click pan ──
    if (mouse.btn(2)) {
        st.mapCamX -= Math.round(mouse.dx() / ts);
        st.mapCamY -= Math.round(mouse.dy() / ts);
        clampCam();
    }

    // ── Spacebar+drag pan ──
    if (key.btn(key.SPACE) && mBtn) {
        if (!st.mapSpacePan) st.mapSpacePan = true;
        st.mapCamX -= Math.round(mouse.dx() / ts);
        st.mapCamY -= Math.round(mouse.dy() / ts);
        clampCam();
        return;
    }
    if (st.mapSpacePan && !key.btn(key.SPACE)) st.mapSpacePan = false;

    // ── Hover tracking ──
    let hover = screenToTile(mx, my);
    st.mapHoverX = hover ? hover.x : -1;
    st.mapHoverY = hover ? hover.y : -1;

    // ── Minimap click-to-navigate ──
    if (st.mapMinimap && mPress && mw > 0 && mh > 0) {
        let mmMaxW = 64;
        let mmMaxH = 48;
        let mmScale2 = Math.min(mmMaxW / mw, mmMaxH / mh, 1);
        let mmW = Math.max(Math.floor(mw * mmScale2), 8);
        let mmH = Math.max(Math.floor(mh * mmScale2), 6);
        let mmX = MAP_VP_X + 4;
        let mmY = MAP_VP_Y + MAP_VP_H - mmH - 4;
        if (mx >= mmX && mx < mmX + mmW && my >= mmY && my < mmY + mmH) {
            let visCols2 = Math.floor(MAP_VP_W / ts);
            let visRows2 = Math.floor(MAP_VP_H / ts);
            st.mapCamX = Math.floor(((mx - mmX) / mmW) * mw - visCols2 / 2);
            st.mapCamY = Math.floor(((my - mmY) / mmH) * mh - visRows2 / 2);
            clampCam();
            return;
        }
    }

    // ── Escape — cancel active operations ──
    if (key.btnp(key.ESCAPE)) {
        if (st.mapSelFloat) {
            dropSelection();
        } else if (st.mapSelRect) {
            st.mapSelRect = null;
            st.mapSelAnchor = null;
        } else {
            st.mapRectAnchor = null;
            st.mapStampRect = null;
            st.mapStampAnchor = null;
            if (mapHist.pending.length > 0) mapHist.pending = [];
        }
    }

    // ── Layer panel click handling ──
    let layerPanelH = layers.length * LAYER_ROW_H + LAYER_ROW_H + 4;
    let layerPanelY = FB_H - FOOT_H - layerPanelH;
    if (mPress && mx >= MAP_PICK_X && mx < FB_W && my >= layerPanelY && my < FB_H - FOOT_H) {
        let relY = my - layerPanelY;
        let layerIdx = Math.floor(relY / LAYER_ROW_H);
        if (layerIdx >= 0 && layerIdx < layers.length) {
            let eyeX = MAP_PICK_X + 2;
            if (mx >= eyeX && mx < eyeX + CW * 2) {
                ensureLayerVis(layers.length);
                st.mapLayerVis[layerIdx] = !st.mapLayerVis[layerIdx];
            } else if (layerIdx === st.mapLayer) {
                // Click on active layer name to rename
                st.mapRenameMode = true;
                st.mapRenameTxt = layers[layerIdx] || "Layer " + layerIdx;
            } else {
                st.mapLayer = layerIdx;
                st.mapObjSel = -1;
                if (st.mapTool === TOOL_OBJ && !isObjectLayer(st.mapLayer)) st.mapTool = TOOL_PEN;
            }
        }
        let btnY = layerPanelY + layers.length * LAYER_ROW_H + 2;
        if (my >= btnY && my < btnY + LAYER_ROW_H) {
            if (mx >= MAP_PICK_X + 2 && mx < MAP_PICK_X + 2 + CW * 3 && layers.length < 8) {
                let name = "Layer " + layers.length;
                let newIdx = map.add_layer(name);
                if (newIdx >= 0) {
                    record(mapHist, { type: "addLayer", idx: newIdx, name: name });
                    mapCommit();
                    ensureLayerVis(layers.length + 1);
                    status("Layer added");
                }
            } else if (
                mx >= MAP_PICK_X + 2 + CW * 4 &&
                mx < MAP_PICK_X + 2 + CW * 7 &&
                layers.length > 1
            ) {
                let ri = st.mapLayer;
                let mw2 = map.width();
                let mh2 = map.height();
                let tiles = [];
                for (let ty = 0; ty < mh2; ty++)
                    for (let tx = 0; tx < mw2; tx++) tiles.push(map.get(tx, ty, ri));
                let lname = layers[ri] || "Layer " + ri;
                map.remove_layer(ri);
                record(mapHist, { type: "removeLayer", idx: ri, name: lname, tiles: tiles });
                mapCommit();
                if (ri < st.mapLayerVis.length) st.mapLayerVis.splice(ri, 1);
                if (st.mapLayer >= layers.length - 1) st.mapLayer = Math.max(0, layers.length - 2);
                status("Layer removed");
            }
        }
        return;
    }

    // ── Tool dispatch ──
    if (isObjectLayer(st.mapLayer)) {
        if (st.mapTool !== TOOL_OBJ) st.mapTool = TOOL_OBJ;
        handleObjectTool(mBtn, mPress, mRelease);
    } else if (st.mapTool === TOOL_PEN || st.mapTool === TOOL_ERA) {
        handlePenEraser(mBtn, mPress, mRelease, ts);
    } else if (st.mapTool === TOOL_FILL) {
        handleFill(mPress);
    } else if (st.mapTool === TOOL_RECT) {
        handleRect(mBtn, mPress, mRelease);
    } else if (st.mapTool === TOOL_SEL) {
        handleSelection(mBtn, mPress, mRelease);
    }

    // Pick tile from map (right-click)
    if (
        mouse.btnp(1) &&
        hover &&
        !isObjectLayer(st.mapLayer) &&
        hover.x >= 0 &&
        hover.x < mw &&
        hover.y >= 0 &&
        hover.y < mh
    ) {
        let raw = map.get(hover.x, hover.y, st.mapLayer);
        if (raw > 0) {
            st.mapTile = tileToSpr(raw);
            st.mapTool = TOOL_PEN;
            st.mapStampRect = null;
            status("Picked sprite " + st.mapTile);
        }
    }

    // ── Tile picker interaction ──
    let pickH = layerPanelY - MAP_PICK_Y;
    let visPickRows = Math.floor(pickH / MAP_PICK_CELL);
    let totalPickRows = sheetCount > 0 ? Math.ceil(sheetCount / MAP_PICK_COLS) : 0;
    let maxPickScroll = Math.max(0, totalPickRows - visPickRows);

    if (
        mx >= MAP_PICK_X &&
        mx < MAP_PICK_X + MAP_PICK_COLS * MAP_PICK_CELL &&
        my >= MAP_PICK_Y &&
        my < MAP_PICK_Y + pickH
    ) {
        if (!ctrl) st.mapPickScrollY = clamp(st.mapPickScrollY - wheel, 0, maxPickScroll);
    }

    if (
        mPress &&
        mx >= MAP_PICK_X &&
        mx < MAP_PICK_X + MAP_PICK_COLS * MAP_PICK_CELL &&
        my >= MAP_PICK_Y &&
        my < MAP_PICK_Y + pickH
    ) {
        let pc = Math.floor((mx - MAP_PICK_X) / MAP_PICK_CELL);
        let pr = Math.floor((my - MAP_PICK_Y) / MAP_PICK_CELL) + st.mapPickScrollY;
        let idx = pr * MAP_PICK_COLS + pc;
        if (idx >= 0 && idx < sheetCount) {
            if (shift && sheetCols > 0) {
                let col = idx % sheetCols;
                let row = Math.floor(idx / sheetCols);
                st.mapStampAnchor = { x: col, y: row };
            } else {
                st.mapTile = idx;
                st.mapStampRect = null;
                st.mapStampAnchor = null;
            }
        }
    }

    // Stamp drag
    if (
        mBtn &&
        st.mapStampAnchor &&
        sheetCols > 0 &&
        mx >= MAP_PICK_X &&
        mx < MAP_PICK_X + MAP_PICK_COLS * MAP_PICK_CELL &&
        my >= MAP_PICK_Y &&
        my < MAP_PICK_Y + pickH
    ) {
        let pc = Math.floor((mx - MAP_PICK_X) / MAP_PICK_CELL);
        let pr = Math.floor((my - MAP_PICK_Y) / MAP_PICK_CELL) + st.mapPickScrollY;
        let idx = pr * MAP_PICK_COLS + pc;
        if (idx >= 0 && idx < sheetCount) {
            let col = idx % sheetCols;
            let row = Math.floor(idx / sheetCols);
            st.mapStampRect = {
                x0: Math.min(st.mapStampAnchor.x, col),
                y0: Math.min(st.mapStampAnchor.y, row),
                x1: Math.max(st.mapStampAnchor.x, col),
                y1: Math.max(st.mapStampAnchor.y, row),
            };
        }
    }
    if (!mBtn && st.mapStampAnchor) {
        if (st.mapStampRect && sheetCols > 0) {
            st.mapTile = st.mapStampRect.y0 * sheetCols + st.mapStampRect.x0;
            let stampW = st.mapStampRect.x1 - st.mapStampRect.x0 + 1;
            let stampH = st.mapStampRect.y1 - st.mapStampRect.y0 + 1;
            status("Stamp " + stampW + "x" + stampH);
        }
        st.mapStampAnchor = null;
    }

    // ── Viewport scroll with wheel (no ctrl) ──
    if (
        !ctrl &&
        wheel !== 0 &&
        mx >= MAP_VP_X &&
        mx < MAP_VP_X + MAP_VP_W &&
        my >= MAP_VP_Y &&
        my < MAP_VP_Y + MAP_VP_H
    ) {
        if (shift) {
            st.mapCamX = clamp(st.mapCamX - wheel * 3, 0, maxCamX());
        } else {
            st.mapCamY = clamp(st.mapCamY - wheel * 3, 0, maxCamY());
        }
    }

    // ── Layer switching (keyboard) ──
    if (st.mapLayer >= layers.length) st.mapLayer = Math.max(0, layers.length - 1);
    if (key.btnp(key.LBRACKET) && st.mapLayer > 0) {
        st.mapLayer--;
        st.mapObjSel = -1;
    }
    if (key.btnp(key.RBRACKET) && st.mapLayer < layers.length - 1) {
        st.mapLayer++;
        st.mapObjSel = -1;
    }
    if (st.mapTool === TOOL_OBJ && !isObjectLayer(st.mapLayer)) st.mapTool = TOOL_PEN;

    // Commit stroke on mouse release
    if (mRelease && st.mapTool !== TOOL_SEL) {
        if (mapHist.pending.length > 0) mapCommit();
        st.mapLastPenX = -1;
        st.mapLastPenY = -1;
    }

    // Undo (Ctrl+Z)
    if (ctrl && key.btnp(key.Z) && !shift) {
        undo(mapHist, applyMapOp);
        st.mapDirty = true;
    }

    // Redo (Ctrl+Y or Ctrl+Shift+Z)
    if (ctrl && (key.btnp(key.Y) || (shift && key.btnp(key.Z)))) {
        redo(mapHist, applyMapOp);
        st.mapDirty = true;
    }

    // ── Grid toggle ──
    if (key.btnp(key.G) && !ctrl) st.mapGridOn = !st.mapGridOn;

    // ── Ghost layers toggle ──
    if (key.btnp(key.H) && !ctrl) st.mapGhostLayers = !st.mapGhostLayers;

    // ── Copy selection (Ctrl+C) ──
    if (ctrl && key.btnp(key.C) && (st.mapSelRect || st.mapSelFloat)) {
        let src = st.mapSelFloat;
        if (!src && st.mapSelRect) {
            let r = st.mapSelRect;
            let w = r.x1 - r.x0 + 1;
            let h = r.y1 - r.y0 + 1;
            let tiles = [];
            for (let ty = r.y0; ty <= r.y1; ty++) {
                for (let tx = r.x0; tx <= r.x1; tx++) {
                    tiles.push(map.get(tx, ty, st.mapLayer));
                }
            }
            src = { x: r.x0, y: r.y0, w: w, h: h, tiles: tiles };
        }
        if (src) {
            st.mapClipboard = { w: src.w, h: src.h, tiles: src.tiles.slice() };
            status("Copied " + src.w + "x" + src.h);
        }
    }

    // ── Paste (Ctrl+V) ──
    if (ctrl && key.btnp(key.V) && st.mapClipboard) {
        if (st.mapSelFloat) dropSelection();
        let c = st.mapClipboard;
        let px = st.mapHoverX >= 0 ? st.mapHoverX : st.mapCamX;
        let py = st.mapHoverY >= 0 ? st.mapHoverY : st.mapCamY;
        st.mapSelFloat = { x: px, y: py, w: c.w, h: c.h, tiles: c.tiles.slice() };
        st.mapSelRect = null;
        st.mapTool = TOOL_SEL;
        status("Pasted " + c.w + "x" + c.h);
    }

    // ── Delete selection ──
    if (key.btnp(key.DELETE) && st.mapSelRect && !isObjectLayer(st.mapLayer)) {
        let r = st.mapSelRect;
        for (let ty = r.y0; ty <= r.y1; ty++) {
            for (let tx = r.x0; tx <= r.x1; tx++) {
                paintTile(tx, ty, 0);
            }
        }
        if (mapHist.pending.length > 0) mapCommit();
        st.mapSelRect = null;
        status("Cleared selection");
    }

    // ── Tool switch ──
    if (key.btnp(key.B) && !ctrl) {
        st.mapTool = TOOL_PEN;
        clearSel();
    }
    if (key.btnp(key.E) && !ctrl) {
        st.mapTool = TOOL_ERA;
        clearSel();
    }
    if (key.btnp(key.F) && !ctrl) {
        st.mapTool = TOOL_FILL;
        clearSel();
    }
    if (key.btnp(key.R) && !ctrl && !shift) {
        st.mapTool = TOOL_RECT;
        clearSel();
    }
    if (key.btnp(key.V) && !ctrl) st.mapTool = TOOL_SEL;
    if (key.btnp(key.O) && !ctrl) {
        st.mapTool = TOOL_OBJ;
        clearSel();
    }

    // ── Minimap toggle (M) ──
    if (key.btnp(key.M) && !ctrl) st.mapMinimap = !st.mapMinimap;

    // ── Auto-tile toggle (Ctrl+A) ──
    if (ctrl && key.btnp(key.A) && !shift) {
        st.mapAutoTile = !st.mapAutoTile;
        status("Auto-tile " + (st.mapAutoTile ? "ON" : "OFF"));
    }

    // ── Define auto-tile group (Ctrl+Shift+A) ──
    if (ctrl && shift && key.btnp(key.A)) {
        let base = Math.floor(st.mapTile / 16) * 16;
        let exists = false;
        for (let i = 0; i < st.mapAutoGroups.length; i++) {
            if (st.mapAutoGroups[i] === base) {
                st.mapAutoGroups.splice(i, 1);
                exists = true;
                break;
            }
        }
        if (exists) {
            status("Removed auto-tile group " + base + "-" + (base + 15));
        } else {
            st.mapAutoGroups.push(base);
            status("Added auto-tile group " + base + "-" + (base + 15));
        }
    }

    // ── Add object layer (Ctrl+Shift+O) ──
    if (ctrl && shift && key.btnp(key.O) && layers.length < 8) {
        let name = "Objects " + layers.length;
        let newIdx = map.add_object_layer(name);
        if (newIdx >= 0) {
            ensureLayerVis(layers.length + 1);
            st.mapLayer = newIdx;
            st.mapTool = TOOL_OBJ;
            st.mapDirty = true;
            status("Object layer added");
        }
    }

    // ── Resize dialog (Ctrl+Shift+R) ──
    if (ctrl && shift && key.btnp(key.R)) {
        st.mapResizeMode = true;
        st.mapResizeW = "" + map.width();
        st.mapResizeH = "" + map.height();
        st.mapResizeField = 0;
    }

    // ── Rename layer (F2) ──
    if (key.btnp(key.F2)) {
        let layers2 = map.layers();
        st.mapRenameMode = true;
        st.mapRenameTxt = layers2[st.mapLayer] || "Layer " + st.mapLayer;
    }

    // ── Level picker (Ctrl+L) ──
    if (ctrl && key.btnp(key.L)) {
        st.mapLevelMode = true;
        st.mapLevelIdx = 0;
    }

    // ── Export Tiled JSON (Ctrl+Shift+E) ──
    if (ctrl && shift && key.btnp(key.E)) {
        let tmj = exportTiledJSON();
        if (tmj) {
            let ok = sys.writeFile("map.tmj", tmj);
            status(ok ? "Exported map.tmj" : "Export failed");
        }
    }

    // ── Home — reset view ──
    if (key.btnp(key.HOME)) {
        st.mapCamX = 0;
        st.mapCamY = 0;
        st.mapZoom = 1;
    }

    // ── Save map (Ctrl+S) ──
    if (ctrl && key.btnp(key.S) && !shift) {
        if (st.mapSelFloat) dropSelection();
        saveMapToDisk();
    }
}

// ─── Tool handlers ───────────────────────────────────────────────────────────

function handlePenEraser(mBtn, mPress, mRelease, ts) {
    let hover = screenToTile(mouse.x(), mouse.y());
    if (!hover) return;

    if (mBtn) {
        let mw = map.width();
        let mh = map.height();
        let tx = hover.x;
        let ty = hover.y;
        if (tx < 0 || tx >= mw || ty < 0 || ty >= mh) return;

        if (st.mapTool === TOOL_ERA) {
            if (st.mapAutoTile) {
                if (st.mapLastPenX >= 0 && (st.mapLastPenX !== tx || st.mapLastPenY !== ty)) {
                    let ddx = Math.abs(tx - st.mapLastPenX);
                    let ddy = Math.abs(ty - st.mapLastPenY);
                    let bsx = st.mapLastPenX < tx ? 1 : -1;
                    let bsy = st.mapLastPenY < ty ? 1 : -1;
                    let berr = ddx - ddy;
                    let cx = st.mapLastPenX;
                    let cy = st.mapLastPenY;
                    while (true) {
                        eraseAutoTile(cx, cy);
                        if (cx === tx && cy === ty) break;
                        let e2 = 2 * berr;
                        if (e2 > -ddy) {
                            berr -= ddy;
                            cx += bsx;
                        }
                        if (e2 < ddx) {
                            berr += ddx;
                            cy += bsy;
                        }
                    }
                } else {
                    eraseAutoTile(tx, ty);
                }
            } else if (st.mapLastPenX >= 0 && (st.mapLastPenX !== tx || st.mapLastPenY !== ty)) {
                bresenhamLine(st.mapLastPenX, st.mapLastPenY, tx, ty, 0);
            } else {
                paintTile(tx, ty, 0);
            }
        } else if (st.mapStampRect) {
            // Stamp brush — paint only when position changes
            if (st.mapLastPenX !== tx || st.mapLastPenY !== ty || st.mapLastPenX < 0) {
                paintStamp(tx, ty);
            }
        } else {
            if (st.mapAutoTile && getAutoGroup(st.mapTile) >= 0) {
                if (st.mapLastPenX !== tx || st.mapLastPenY !== ty || st.mapLastPenX < 0) {
                    paintAutoTile(tx, ty, st.mapTile);
                }
            } else {
                let newTile = sprToTile(st.mapTile);
                if (st.mapLastPenX >= 0 && (st.mapLastPenX !== tx || st.mapLastPenY !== ty)) {
                    bresenhamLine(st.mapLastPenX, st.mapLastPenY, tx, ty, newTile);
                } else {
                    paintTile(tx, ty, newTile);
                }
            }
        }
        st.mapLastPenX = tx;
        st.mapLastPenY = ty;
    }
}

function handleFill(mPress) {
    if (!mPress) return;
    let hover = screenToTile(mouse.x(), mouse.y());
    if (!hover) return;
    let mw = map.width();
    let mh = map.height();
    if (hover.x < 0 || hover.x >= mw || hover.y < 0 || hover.y >= mh) return;
    floodFill(hover.x, hover.y, sprToTile(st.mapTile));
    if (mapHist.pending.length > 0) mapCommit();
}

function handleRect(mBtn, mPress, mRelease) {
    let hover = screenToTile(mouse.x(), mouse.y());
    if (!hover) return;

    if (mPress && st.mapRectAnchor === null) {
        st.mapRectAnchor = { x: hover.x, y: hover.y };
        return;
    }

    if (!mBtn && st.mapRectAnchor !== null) {
        // Commit the rectangle
        let ax = st.mapRectAnchor.x;
        let ay = st.mapRectAnchor.y;
        let bx = hover.x;
        let by = hover.y;
        let x0 = Math.min(ax, bx);
        let y0 = Math.min(ay, by);
        let x1 = Math.max(ax, bx);
        let y1 = Math.max(ay, by);
        for (let ry = y0; ry <= y1; ry++) {
            for (let rx = x0; rx <= x1; rx++) {
                paintTile(rx, ry, sprToTile(st.mapTile));
            }
        }
        if (mapHist.pending.length > 0) mapCommit();
        st.mapRectAnchor = null;
    }
}

function handleSelection(mBtn, mPress, mRelease) {
    let hover = screenToTile(mouse.x(), mouse.y());
    if (!hover) return;

    // Click while float exists — move or drop
    if (mPress && st.mapSelFloat) {
        let f = st.mapSelFloat;
        if (hover.x >= f.x && hover.x < f.x + f.w && hover.y >= f.y && hover.y < f.y + f.h) {
            st.mapSelDrag = { ox: hover.x - f.x, oy: hover.y - f.y };
            return;
        } else {
            dropSelection();
            return;
        }
    }

    // Drag float
    if (mBtn && st.mapSelDrag && st.mapSelFloat) {
        st.mapSelFloat.x = hover.x - st.mapSelDrag.ox;
        st.mapSelFloat.y = hover.y - st.mapSelDrag.oy;
        return;
    }
    if (!mBtn && st.mapSelDrag) {
        st.mapSelDrag = null;
        return;
    }

    // Click inside existing selection — lift it
    if (mPress && st.mapSelRect) {
        let r = st.mapSelRect;
        if (hover.x >= r.x0 && hover.x <= r.x1 && hover.y >= r.y0 && hover.y <= r.y1) {
            liftSelection();
            let f = st.mapSelFloat;
            st.mapSelDrag = { ox: hover.x - f.x, oy: hover.y - f.y };
            return;
        }
        st.mapSelRect = null;
    }

    // Start new selection marquee
    if (mPress && !st.mapSelAnchor) {
        st.mapSelAnchor = { x: hover.x, y: hover.y };
        return;
    }

    // Drag — update selection rect
    if (mBtn && st.mapSelAnchor) {
        st.mapSelRect = {
            x0: Math.min(st.mapSelAnchor.x, hover.x),
            y0: Math.min(st.mapSelAnchor.y, hover.y),
            x1: Math.max(st.mapSelAnchor.x, hover.x),
            y1: Math.max(st.mapSelAnchor.y, hover.y),
        };
        return;
    }

    // Release — finalize selection
    if (!mBtn && st.mapSelAnchor) {
        if (st.mapSelRect) {
            let w = st.mapSelRect.x1 - st.mapSelRect.x0 + 1;
            let h = st.mapSelRect.y1 - st.mapSelRect.y0 + 1;
            status("Selected " + w + "x" + h);
        }
        st.mapSelAnchor = null;
    }
}

function handleObjectTool(mBtn, mPress, mRelease) {
    let mx = mouse.x();
    let my = mouse.y();
    let ts = tileSize();
    let shift = key.btn(key.LSHIFT) || key.btn(key.RSHIFT);

    if (mx < MAP_VP_X || mx >= MAP_VP_X + MAP_VP_W || my < MAP_VP_Y || my >= MAP_VP_Y + MAP_VP_H)
        return;

    // Convert mouse to pixel coords in map space
    let mapPx = ((mx - MAP_VP_X) / ts) * MAP_TILE_PX + st.mapCamX * MAP_TILE_PX;
    let mapPy = ((my - MAP_VP_Y) / ts) * MAP_TILE_PX + st.mapCamY * MAP_TILE_PX;

    let objs = map.layer_objects(st.mapLayer);

    // Drag in progress
    if (mBtn && st.mapObjDrag && st.mapObjSel >= 0 && st.mapObjSel < objs.length) {
        if (st.mapObjDrag.resize) {
            let nw = Math.max(
                MAP_TILE_PX,
                Math.round(st.mapObjDrag.startW + (mapPx - st.mapObjDrag.anchorX)),
            );
            let nh = Math.max(
                MAP_TILE_PX,
                Math.round(st.mapObjDrag.startH + (mapPy - st.mapObjDrag.anchorY)),
            );
            map.set_object(st.mapLayer, st.mapObjSel, { w: nw, h: nh });
        } else {
            let nx = Math.round(mapPx - st.mapObjDrag.ox);
            let ny = Math.round(mapPy - st.mapObjDrag.oy);
            map.set_object(st.mapLayer, st.mapObjSel, { x: nx, y: ny });
        }
        st.mapDirty = true;
        return;
    }
    if (!mBtn && st.mapObjDrag) {
        if (st.mapObjSel >= 0 && st.mapObjSel < objs.length) {
            let o = objs[st.mapObjSel];
            if (st.mapObjDrag.resize) {
                if (o.w !== st.mapObjDrag.startW || o.h !== st.mapObjDrag.startH) {
                    record(mapHist, {
                        type: "resizeObj",
                        layer: st.mapObjDrag.layer,
                        idx: st.mapObjSel,
                        prevW: st.mapObjDrag.startW,
                        prevH: st.mapObjDrag.startH,
                        nextW: o.w,
                        nextH: o.h,
                    });
                    mapCommit();
                }
            } else if (o.x !== st.mapObjDrag.startX || o.y !== st.mapObjDrag.startY) {
                record(mapHist, {
                    type: "moveObj",
                    layer: st.mapObjDrag.layer,
                    idx: st.mapObjSel,
                    prevX: st.mapObjDrag.startX,
                    prevY: st.mapObjDrag.startY,
                    nextX: o.x,
                    nextY: o.y,
                });
                mapCommit();
            }
        }
        st.mapObjDrag = null;
        return;
    }

    if (mPress) {
        // Hit test: find object under cursor
        let hit = -1;
        for (let i = objs.length - 1; i >= 0; i--) {
            let o = objs[i];
            let ow = o.w > 0 ? o.w : MAP_TILE_PX;
            let oh = o.h > 0 ? o.h : MAP_TILE_PX;
            if (mapPx >= o.x && mapPx < o.x + ow && mapPy >= o.y && mapPy < o.y + oh) {
                hit = i;
                break;
            }
        }

        if (hit >= 0) {
            st.mapObjSel = hit;
            let o = objs[hit];
            let ow = o.w > 0 ? o.w : MAP_TILE_PX;
            let oh = o.h > 0 ? o.h : MAP_TILE_PX;
            // Check if near bottom-right corner for resize
            let hDist = MAP_TILE_PX * 0.6;
            if (Math.abs(mapPx - (o.x + ow)) < hDist && Math.abs(mapPy - (o.y + oh)) < hDist) {
                st.mapObjDrag = {
                    resize: true,
                    startW: ow,
                    startH: oh,
                    startX: o.x,
                    startY: o.y,
                    anchorX: mapPx,
                    anchorY: mapPy,
                    layer: st.mapLayer,
                };
            } else {
                st.mapObjDrag = {
                    ox: mapPx - o.x,
                    oy: mapPy - o.y,
                    startX: o.x,
                    startY: o.y,
                    layer: st.mapLayer,
                };
            }
        } else if (shift) {
            // Shift+click: add new object
            let name = "obj_" + st.mapObjCounter;
            st.mapObjCounter++;
            let snapX = Math.floor(mapPx / MAP_TILE_PX) * MAP_TILE_PX;
            let snapY = Math.floor(mapPy / MAP_TILE_PX) * MAP_TILE_PX;
            let obj = {
                name: name,
                type: "",
                x: snapX,
                y: snapY,
                w: MAP_TILE_PX * 2,
                h: MAP_TILE_PX * 2,
            };
            let idx = map.add_object(st.mapLayer, obj);
            if (idx >= 0) {
                record(mapHist, { type: "addObj", layer: st.mapLayer, idx: idx, obj: obj });
                mapCommit();
                st.mapObjSel = idx;
                st.mapDirty = true;
                status("Added " + name);
            }
        } else {
            st.mapObjSel = -1;
        }
    }

    // Delete selected object
    if (key.btnp(key.DELETE) && st.mapObjSel >= 0 && st.mapObjSel < objs.length) {
        let o = objs[st.mapObjSel];
        let obj = { name: o.name, type: o.type, x: o.x, y: o.y, w: o.w, h: o.h, gid: o.gid };
        map.remove_object(st.mapLayer, st.mapObjSel);
        record(mapHist, {
            type: "removeObj",
            layer: st.mapLayer,
            idx: st.mapObjSel,
            obj: obj,
        });
        mapCommit();
        status("Removed object");
        st.mapObjSel = -1;
        st.mapDirty = true;
    }
}

function updateResizeDialog() {
    if (key.btnp(key.TAB)) {
        st.mapResizeField = st.mapResizeField === 0 ? 1 : 0;
        return;
    }
    if (key.btnp(key.ESCAPE)) {
        st.mapResizeMode = false;
        return;
    }
    if (key.btnp(key.ENTER)) {
        let w = parseInt(st.mapResizeW) || 0;
        let h = parseInt(st.mapResizeH) || 0;
        if (w >= 1 && w <= 4096 && h >= 1 && h <= 4096) {
            map.resize(w, h);
            st.mapDirty = true;
            status("Resized to " + w + "x" + h);
        } else {
            status("Invalid size (1-4096)");
        }
        st.mapResizeMode = false;
        return;
    }
    if (key.btnp(key.BACKSPACE)) {
        if (st.mapResizeField === 0) st.mapResizeW = st.mapResizeW.slice(0, -1);
        else st.mapResizeH = st.mapResizeH.slice(0, -1);
    }
}

function updateRenameDialog() {
    if (key.btnp(key.ESCAPE)) {
        st.mapRenameMode = false;
        return;
    }
    if (key.btnp(key.ENTER)) {
        let name = st.mapRenameTxt.trim();
        if (name.length > 0) {
            map.rename_layer(st.mapLayer, name);
            st.mapDirty = true;
            status("Renamed layer");
        }
        st.mapRenameMode = false;
        return;
    }
    if (key.btnp(key.BACKSPACE)) {
        st.mapRenameTxt = st.mapRenameTxt.slice(0, -1);
    }
}

function updateLevelPicker() {
    let levels = map.levels();
    if (key.btnp(key.ESCAPE)) {
        st.mapLevelMode = false;
        return;
    }
    if (key.btnp(key.UP) && st.mapLevelIdx > 0) st.mapLevelIdx--;
    if (key.btnp(key.DOWN) && st.mapLevelIdx < levels.length) st.mapLevelIdx++;
    if (key.btnp(key.ENTER)) {
        if (st.mapLevelIdx < levels.length) {
            map.load(levels[st.mapLevelIdx]);
            status("Loaded " + levels[st.mapLevelIdx]);
        } else {
            let name = "level_" + levels.length;
            map.create(64, 64, name);
            status("Created " + name);
        }
        st.mapLevelMode = false;
        st.mapDirty = true;
        return;
    }
}

// ─── Camera helpers ──────────────────────────────────────────────────────────

function maxCamX() {
    let ts = tileSize();
    return Math.max(0, map.width() - Math.floor(MAP_VP_W / ts));
}

function maxCamY() {
    let ts = tileSize();
    return Math.max(0, map.height() - Math.floor(MAP_VP_H / ts));
}

function clampCam() {
    st.mapCamX = clamp(st.mapCamX, 0, maxCamX());
    st.mapCamY = clamp(st.mapCamY, 0, maxCamY());
}

// ─── Draw ────────────────────────────────────────────────────────────────────

export function drawMapEditor() {
    let mw = map.width();
    let mh = map.height();
    if (mw === 0 || mh === 0) {
        return;
    }

    let sw = gfx.sheetW();
    let sh = gfx.sheetH();
    let sheetCols = sw > 0 ? Math.floor(sw / MAP_TILE_PX) : 0;
    let sheetRows = sh > 0 ? Math.floor(sh / MAP_TILE_PX) : 0;
    let sheetCount = sheetCols * sheetRows;
    let ts = tileSize();
    let visCols = Math.ceil(MAP_VP_W / ts) + 1;
    let visRows = Math.ceil(MAP_VP_H / ts) + 1;
    let layers = map.layers();

    // ── Map viewport ──
    gfx.clip(MAP_VP_X, MAP_VP_Y, MAP_VP_W, MAP_VP_H);

    // Draw all visible layers bottom-to-top (proper layer stacking)
    // Pass 1: non-active layers
    for (let li = 0; li < layers.length; li++) {
        if (li === st.mapLayer) continue;
        if (!isLayerVisible(li)) continue;
        if (isObjectLayer(li)) {
            let objs = map.layer_objects(li);
            let camPx = st.mapCamX * MAP_TILE_PX;
            let camPy = st.mapCamY * MAP_TILE_PX;
            let ghostScale = ts / MAP_TILE_PX;
            for (let oi = 0; oi < objs.length; oi++) {
                let o = objs[oi];
                let ox = MAP_VP_X + (o.x - camPx) * ghostScale;
                let oy = MAP_VP_Y + (o.y - camPy) * ghostScale;
                let ow2 = Math.max(o.w * ghostScale, 4);
                let oh2 = Math.max(o.h * ghostScale, 4);
                gfx.rect(
                    Math.floor(ox),
                    Math.floor(oy),
                    Math.floor(ox + ow2 - 1),
                    Math.floor(oy + oh2 - 1),
                    st.mapGhostLayers ? 5 : 11,
                );
            }
        } else {
            for (let r = 0; r < visRows; r++) {
                for (let c = 0; c < visCols; c++) {
                    let tx = st.mapCamX + c;
                    let ty = st.mapCamY + r;
                    if (tx >= mw || ty >= mh) continue;
                    let tile = map.get(tx, ty, li);
                    if (tile > 0 && sheetCols > 0) {
                        let spr = tileToSpr(tile);
                        let srcX = (spr % sheetCols) * MAP_TILE_PX;
                        let srcY = Math.floor(spr / sheetCols) * MAP_TILE_PX;
                        let dx = MAP_VP_X + c * ts;
                        let dy = MAP_VP_Y + r * ts;
                        gfx.sspr(srcX, srcY, MAP_TILE_PX, MAP_TILE_PX, dx, dy, ts, ts);
                    }
                }
            }
        }
        // Dim overlay only when ghost mode is on (makes non-active layers translucent)
        if (st.mapGhostLayers)
            gfx.rectfill(MAP_VP_X, MAP_VP_Y, MAP_VP_X + MAP_VP_W - 1, MAP_VP_Y + MAP_VP_H - 1, 0);
    }
    // Pass 2: active layer on top (always full brightness)
    if (isLayerVisible(st.mapLayer) && !isObjectLayer(st.mapLayer)) {
        for (let r = 0; r < visRows; r++) {
            for (let c = 0; c < visCols; c++) {
                let tx = st.mapCamX + c;
                let ty = st.mapCamY + r;
                if (tx >= mw || ty >= mh) continue;
                let tile = map.get(tx, ty, st.mapLayer);
                if (tile > 0 && sheetCols > 0) {
                    let spr = tileToSpr(tile);
                    let srcX = (spr % sheetCols) * MAP_TILE_PX;
                    let srcY = Math.floor(spr / sheetCols) * MAP_TILE_PX;
                    let dx = MAP_VP_X + c * ts;
                    let dy = MAP_VP_Y + r * ts;
                    gfx.sspr(srcX, srcY, MAP_TILE_PX, MAP_TILE_PX, dx, dy, ts, ts);
                }
            }
        }
    }

    // Rect tool preview
    if (st.mapRectAnchor !== null && st.mapHoverX >= 0) {
        let ax = st.mapRectAnchor.x;
        let ay = st.mapRectAnchor.y;
        let bx = st.mapHoverX;
        let by = st.mapHoverY;
        let x0 = Math.min(ax, bx);
        let y0 = Math.min(ay, by);
        let x1 = Math.max(ax, bx);
        let y1 = Math.max(ay, by);
        let px0 = MAP_VP_X + (x0 - st.mapCamX) * ts;
        let py0 = MAP_VP_Y + (y0 - st.mapCamY) * ts;
        let px1 = MAP_VP_X + (x1 - st.mapCamX + 1) * ts - 1;
        let py1 = MAP_VP_Y + (y1 - st.mapCamY + 1) * ts - 1;
        gfx.rect(px0, py0, px1, py1, FG);
    }

    // Selection rectangle
    if (st.mapSelRect) {
        let r = st.mapSelRect;
        let px0 = MAP_VP_X + (r.x0 - st.mapCamX) * ts;
        let py0 = MAP_VP_Y + (r.y0 - st.mapCamY) * ts;
        let px1 = MAP_VP_X + (r.x1 - st.mapCamX + 1) * ts - 1;
        let py1 = MAP_VP_Y + (r.y1 - st.mapCamY + 1) * ts - 1;
        gfx.rect(px0, py0, px1, py1, 12);
    }

    // Floating selection
    if (st.mapSelFloat && sheetCols > 0) {
        let f = st.mapSelFloat;
        for (let dy = 0; dy < f.h; dy++) {
            for (let dx = 0; dx < f.w; dx++) {
                let t = f.tiles[dy * f.w + dx];
                if (t > 0) {
                    let spr = tileToSpr(t);
                    let srcX = (spr % sheetCols) * MAP_TILE_PX;
                    let srcY = Math.floor(spr / sheetCols) * MAP_TILE_PX;
                    let px = MAP_VP_X + (f.x + dx - st.mapCamX) * ts;
                    let py = MAP_VP_Y + (f.y + dy - st.mapCamY) * ts;
                    gfx.sspr(srcX, srcY, MAP_TILE_PX, MAP_TILE_PX, px, py, ts, ts);
                }
            }
        }
        let fx0 = MAP_VP_X + (f.x - st.mapCamX) * ts;
        let fy0 = MAP_VP_Y + (f.y - st.mapCamY) * ts;
        gfx.rect(fx0, fy0, fx0 + f.w * ts - 1, fy0 + f.h * ts - 1, 12);
    }

    // Grid overlay
    if (st.mapGridOn) {
        for (let c = 0; c <= visCols; c++) {
            let x = MAP_VP_X + c * ts;
            gfx.line(x, MAP_VP_Y, x, MAP_VP_Y + MAP_VP_H - 1, GRIDC);
        }
        for (let r = 0; r <= visRows; r++) {
            let y = MAP_VP_Y + r * ts;
            gfx.line(MAP_VP_X, y, MAP_VP_X + MAP_VP_W - 1, y, GRIDC);
        }
    }

    // Tile preview on cursor (ghost of selected tile/stamp)
    if (
        st.mapHoverX >= 0 &&
        st.mapHoverY >= 0 &&
        st.mapTool === TOOL_PEN &&
        sheetCols > 0 &&
        !st.mapSelFloat
    ) {
        let hx = MAP_VP_X + (st.mapHoverX - st.mapCamX) * ts;
        let hy = MAP_VP_Y + (st.mapHoverY - st.mapCamY) * ts;
        if (
            hx >= MAP_VP_X &&
            hx < MAP_VP_X + MAP_VP_W &&
            hy >= MAP_VP_Y &&
            hy < MAP_VP_Y + MAP_VP_H
        ) {
            if (st.mapStampRect) {
                let sr = st.mapStampRect;
                for (let dy = sr.y0; dy <= sr.y1; dy++) {
                    for (let dx = sr.x0; dx <= sr.x1; dx++) {
                        let sprIdx = dy * sheetCols + dx;
                        let srcX = (sprIdx % sheetCols) * MAP_TILE_PX;
                        let srcY = Math.floor(sprIdx / sheetCols) * MAP_TILE_PX;
                        let px = hx + (dx - sr.x0) * ts;
                        let py = hy + (dy - sr.y0) * ts;
                        gfx.sspr(srcX, srcY, MAP_TILE_PX, MAP_TILE_PX, px, py, ts, ts);
                    }
                }
            } else {
                let srcX = (st.mapTile % sheetCols) * MAP_TILE_PX;
                let srcY = Math.floor(st.mapTile / sheetCols) * MAP_TILE_PX;
                gfx.sspr(srcX, srcY, MAP_TILE_PX, MAP_TILE_PX, hx, hy, ts, ts);
            }
        }
    }

    // Hover cursor (outline)
    if (st.mapHoverX >= 0 && st.mapHoverY >= 0) {
        let hx = MAP_VP_X + (st.mapHoverX - st.mapCamX) * ts;
        let hy = MAP_VP_Y + (st.mapHoverY - st.mapCamY) * ts;
        if (
            hx >= MAP_VP_X &&
            hx < MAP_VP_X + MAP_VP_W &&
            hy >= MAP_VP_Y &&
            hy < MAP_VP_Y + MAP_VP_H
        ) {
            gfx.rect(hx, hy, hx + ts - 1, hy + ts - 1, FG);
        }
    }

    // ── Object layer drawing ──
    if (isObjectLayer(st.mapLayer)) {
        let objs = map.layer_objects(st.mapLayer);
        let camPx = st.mapCamX * MAP_TILE_PX;
        let camPy = st.mapCamY * MAP_TILE_PX;
        let scale = ts / MAP_TILE_PX;
        for (let i = 0; i < objs.length; i++) {
            let o = objs[i];
            let ox = MAP_VP_X + (o.x - camPx) * scale;
            let oy = MAP_VP_Y + (o.y - camPy) * scale;
            let ow = Math.max(o.w * scale, 4);
            let oh = Math.max(o.h * scale, 4);
            let col = i === st.mapObjSel ? 12 : 11;
            gfx.rect(
                Math.floor(ox),
                Math.floor(oy),
                Math.floor(ox + ow - 1),
                Math.floor(oy + oh - 1),
                col,
            );
            if (ow > CW * 3) {
                let label = o.name || "obj";
                if (label.length * CW > ow) label = label.slice(0, Math.floor(ow / CW));
                gfx.print(label, Math.floor(ox + 1), Math.floor(oy + 1), col);
            }
        }
        // Resize handle on selected object
        if (st.mapObjSel >= 0 && st.mapObjSel < objs.length) {
            let so = objs[st.mapObjSel];
            let sox = MAP_VP_X + (so.x - camPx) * scale;
            let soy = MAP_VP_Y + (so.y - camPy) * scale;
            let sow = Math.max(so.w * scale, 4);
            let soh = Math.max(so.h * scale, 4);
            let hx = Math.floor(sox + sow - 3);
            let hy = Math.floor(soy + soh - 3);
            gfx.rectfill(hx, hy, hx + 4, hy + 4, 12);
        }
    }

    gfx.clip(); // reset clip

    // ── Minimap overlay ──
    if (st.mapMinimap && mw > 0 && mh > 0) {
        let mmMaxW = 64;
        let mmMaxH = 48;
        let mmScale = Math.min(mmMaxW / mw, mmMaxH / mh, 1);
        let mmW = Math.max(Math.floor(mw * mmScale), 8);
        let mmH = Math.max(Math.floor(mh * mmScale), 6);
        let mmX = MAP_VP_X + 4;
        let mmY = MAP_VP_Y + MAP_VP_H - mmH - 4;
        // Background
        gfx.rectfill(mmX - 1, mmY - 1, mmX + mmW, mmY + mmH, 0);
        // Tiles — sample active layer
        for (let py = 0; py < mmH; py++) {
            for (let px = 0; px < mmW; px++) {
                let tx = Math.floor((px / mmW) * mw);
                let ty = Math.floor((py / mmH) * mh);
                let tile = map.get(tx, ty, st.mapLayer);
                if (tile > 0) {
                    gfx.pset(mmX + px, mmY + py, 6);
                }
            }
        }
        // Viewport indicator
        let visCols2 = Math.floor(MAP_VP_W / ts);
        let visRows2 = Math.floor(MAP_VP_H / ts);
        let vx0 = mmX + Math.floor((st.mapCamX / mw) * mmW);
        let vy0 = mmY + Math.floor((st.mapCamY / mh) * mmH);
        let vx1 = vx0 + Math.max(Math.floor((visCols2 / mw) * mmW), 1);
        let vy1 = vy0 + Math.max(Math.floor((visRows2 / mh) * mmH), 1);
        gfx.rect(vx0, vy0, Math.min(vx1, mmX + mmW - 1), Math.min(vy1, mmY + mmH - 1), FG);
    }

    // ── Right panel: tile picker + layer panel ──
    let layerPanelH = layers.length * LAYER_ROW_H + LAYER_ROW_H + 4;
    let layerPanelY = FB_H - FOOT_H - layerPanelH;
    let pickH = layerPanelY - MAP_PICK_Y;

    // Tile picker
    gfx.rectfill(MAP_PICK_X, MAP_PICK_Y, FB_W - 1, layerPanelY - 1, GUTBG);

    if (sheetCols > 0) {
        let pickRows = Math.ceil(sheetCount / MAP_PICK_COLS);
        let visPickRows = Math.floor(pickH / MAP_PICK_CELL);
        for (let r = 0; r < visPickRows && r + st.mapPickScrollY < pickRows; r++) {
            for (let c = 0; c < MAP_PICK_COLS; c++) {
                let idx = (r + st.mapPickScrollY) * MAP_PICK_COLS + c;
                if (idx >= sheetCount) break;
                let dx = MAP_PICK_X + c * MAP_PICK_CELL;
                let dy = MAP_PICK_Y + r * MAP_PICK_CELL;
                let srcX = (idx % sheetCols) * MAP_TILE_PX;
                let srcY = Math.floor(idx / sheetCols) * MAP_TILE_PX;
                gfx.sspr(
                    srcX,
                    srcY,
                    MAP_TILE_PX,
                    MAP_TILE_PX,
                    dx,
                    dy,
                    MAP_PICK_CELL,
                    MAP_PICK_CELL,
                );
                // Auto-tile group indicator
                if (st.mapAutoTile && getAutoGroup(idx) >= 0) {
                    gfx.pset(dx + MAP_PICK_CELL - 2, dy + 1, 11);
                }
                // Highlight current tile or stamp selection
                let inStamp = false;
                if (st.mapStampRect && sheetCols > 0) {
                    let col = idx % sheetCols;
                    let row = Math.floor(idx / sheetCols);
                    inStamp =
                        col >= st.mapStampRect.x0 &&
                        col <= st.mapStampRect.x1 &&
                        row >= st.mapStampRect.y0 &&
                        row <= st.mapStampRect.y1;
                }
                if (idx === st.mapTile || inStamp) {
                    gfx.rect(
                        dx,
                        dy,
                        dx + MAP_PICK_CELL - 1,
                        dy + MAP_PICK_CELL - 1,
                        inStamp ? 12 : FG,
                    );
                }
            }
        }
    }

    // Separator
    gfx.line(MAP_PICK_X, layerPanelY, FB_W - 1, layerPanelY, SEPC);

    // Layer panel
    gfx.rectfill(MAP_PICK_X, layerPanelY + 1, FB_W - 1, FB_H - FOOT_H - 1, PANELBG);
    ensureLayerVis(layers.length);
    for (let i = 0; i < layers.length; i++) {
        let ly = layerPanelY + 1 + i * LAYER_ROW_H;
        let isActive = i === st.mapLayer;
        if (isActive) gfx.rectfill(MAP_PICK_X, ly, FB_W - 1, ly + LAYER_ROW_H - 1, SELBG);
        let vis = st.mapLayerVis[i];
        gfx.print(vis ? "*" : "-", MAP_PICK_X + 2, ly + 1, vis ? FG : 5);
        let name = layers[i] || "Layer " + i;
        let isObj = isObjectLayer(i);
        if (isObj) name = "\u25A1" + name;
        if (name.length > 14) name = name.slice(0, 13) + "\u2026";
        gfx.print(name, MAP_PICK_X + 2 + CW * 2, ly + 1, isActive ? FG : 6);
    }

    // Add/remove layer buttons
    let btnY = layerPanelY + 1 + layers.length * LAYER_ROW_H + 1;
    gfx.print("[+]", MAP_PICK_X + 2, btnY, layers.length < 8 ? FG : 5);
    gfx.print("[-]", MAP_PICK_X + 2 + CW * 4, btnY, layers.length > 1 ? FG : 5);

    // ── Footer ──
    let footY = FB_H - FOOT_H;
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, FOOTBG);
    let footTextY = footY + Math.floor((FOOT_H - CH) / 2);
    let toolNames = ["pen", "era", "fill", "rect", "sel", "obj"];
    let info =
        "L:" +
        st.mapLayer +
        "/" +
        (layers.length - 1) +
        " Z:" +
        st.mapZoom +
        "x " +
        toolNames[st.mapTool];
    if (st.mapAutoTile) info += " AT";
    if (st.mapHoverX >= 0) info += " (" + st.mapHoverX + "," + st.mapHoverY + ")";
    else info += " (" + st.mapCamX + "," + st.mapCamY + ")";
    if (st.mapStampRect) {
        let sw2 = st.mapStampRect.x1 - st.mapStampRect.x0 + 1;
        let sh2 = st.mapStampRect.y1 - st.mapStampRect.y0 + 1;
        info += " stamp:" + sw2 + "x" + sh2;
    }
    if (st.mapObjSel >= 0 && isObjectLayer(st.mapLayer)) {
        let objs = map.layer_objects(st.mapLayer);
        if (st.mapObjSel < objs.length) {
            let o = objs[st.mapObjSel];
            info += " [" + (o.name || "obj") + "]";
        }
    }
    if (st.mapDirty) info = "* " + info;
    gfx.print(info, CW, footTextY, FOOTFG);
    // Level name on right
    let lvl = map.current_level();
    if (lvl) gfx.print(lvl, FB_W - (lvl.length + 1) * CW, footTextY, FOOTFG);

    // ── Resize dialog overlay ──
    if (st.mapResizeMode) drawResizeDialog();

    // ── Rename dialog overlay ──
    if (st.mapRenameMode) drawRenameDialog();

    // ── Level picker overlay ──
    if (st.mapLevelMode) drawLevelPicker();
}

function drawResizeDialog() {
    let dw = 180;
    let dh = 60;
    let dx = Math.floor((FB_W - dw) / 2);
    let dy = Math.floor((FB_H - dh) / 2);
    gfx.rectfill(dx, dy, dx + dw - 1, dy + dh - 1, PANELBG);
    gfx.rect(dx, dy, dx + dw - 1, dy + dh - 1, FG);
    gfx.print("Resize Map", dx + 4, dy + 4, FG);

    let wVal = parseInt(st.mapResizeW) || 0;
    let hVal = parseInt(st.mapResizeH) || 0;
    let wOk = wVal >= 1 && wVal <= 4096;
    let hOk = hVal >= 1 && hVal <= 4096;

    let fy = dy + 18;
    gfx.print("W:", dx + 4, fy, st.mapResizeField === 0 ? FG : 6);
    gfx.print(
        st.mapResizeW + (st.mapResizeField === 0 ? "_" : ""),
        dx + 4 + CW * 3,
        fy,
        wOk ? FG : 8,
    );
    fy += CH + 4;
    gfx.print("H:", dx + 4, fy, st.mapResizeField === 1 ? FG : 6);
    gfx.print(
        st.mapResizeH + (st.mapResizeField === 1 ? "_" : ""),
        dx + 4 + CW * 3,
        fy,
        hOk ? FG : 8,
    );
    gfx.print("Tab:switch Enter:ok Esc:cancel", dx + 4, dy + dh - CH - 2, 6);
}

function drawRenameDialog() {
    let dw = 200;
    let dh = 40;
    let dx = Math.floor((FB_W - dw) / 2);
    let dy = Math.floor((FB_H - dh) / 2);
    gfx.rectfill(dx, dy, dx + dw - 1, dy + dh - 1, PANELBG);
    gfx.rect(dx, dy, dx + dw - 1, dy + dh - 1, FG);
    gfx.print("Rename Layer", dx + 4, dy + 4, FG);
    let fy = dy + 18;
    let txt = st.mapRenameTxt;
    let maxChars = Math.floor((dw - 12) / CW) - 1;
    if (txt.length > maxChars) txt = txt.slice(txt.length - maxChars);
    gfx.print(txt + "_", dx + 4, fy, FG);
    gfx.print("Enter:ok Esc:cancel", dx + 4, dy + dh - CH - 2, 6);
}

function drawLevelPicker() {
    let levels = map.levels();
    let count = levels.length + 1;
    let dw = 180;
    let dh = count * (CH + 2) + 30;
    let dx = Math.floor((FB_W - dw) / 2);
    let dy = Math.floor((FB_H - dh) / 2);
    gfx.rectfill(dx, dy, dx + dw - 1, dy + dh - 1, PANELBG);
    gfx.rect(dx, dy, dx + dw - 1, dy + dh - 1, FG);
    gfx.print("Levels", dx + 4, dy + 4, FG);
    let ly = dy + 18;
    for (let i = 0; i < levels.length; i++) {
        let isCur = levels[i] === map.current_level();
        let isSel = i === st.mapLevelIdx;
        if (isSel) gfx.rectfill(dx + 2, ly, dx + dw - 3, ly + CH + 1, SELBG);
        gfx.print((isCur ? "> " : "  ") + levels[i], dx + 4, ly, FG);
        ly += CH + 2;
    }
    let isSel = st.mapLevelIdx === levels.length;
    if (isSel) gfx.rectfill(dx + 2, ly, dx + dw - 3, ly + CH + 1, SELBG);
    gfx.print("  + New level", dx + 4, ly, 11);
    gfx.print("Enter:select Esc:close", dx + 4, dy + dh - CH - 2, 6);
}
