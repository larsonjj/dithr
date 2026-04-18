// Raycaster Demo
//
// A basic Wolfenstein 3D-style raycaster using DDA (Digital Differential
// Analyzer).  Renders flat-shaded walls with distance-based darkening,
// solid floor/ceiling, and a minimap overlay.
//
// Controls:
//   W / Up      — move forward
//   S / Down    — move backward
//   A           — strafe left
//   D           — strafe right
//   Left/Right  — turn

// --- Constants -------------------------------------------------------

const SCREEN_W = 320;
const SCREEN_H = 180;
const MAP_W = 16;
const MAP_H = 16;
const _TILE_SIZE = 1; // world units per tile
const FOV = math.PI / 3; // 60 degrees
const HALF_FOV = FOV / 2;
const NUM_RAYS = SCREEN_W;
const MOVE_SPEED = 2.5;
const TURN_SPEED = 2.0;
const HALF_H = SCREEN_H / 2;

// --- World map (1 = wall, 0 = empty) --------------------------------

const worldMap = [
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
];

// Wall colours per hit side (N/S vs E/W) for depth feel
const _WALL_COL_NS = 12; // light blue
const _WALL_COL_EW = 1; // dark blue

// Palette indices used for distance shading (light → dark)
const SHADE_NS = [12, 13, 2, 1];
const SHADE_EW = [6, 13, 2, 1];

// --- Player state ----------------------------------------------------

let px = 2.5;
let py = 2.5;
let pa = 0; // angle in radians
let showMinimap = true; // toggle with M key


// --- Helpers ---------------------------------------------------------

function mapAt(x: number, y: number) {
    const mx = math.flr(x);
    const my = math.flr(y);
    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H) return 1;
    return worldMap[my * MAP_W + mx];
}

function shadeColor(dist: number, isNs: boolean) {
    const shades = isNs ? SHADE_NS : SHADE_EW;
    let idx = math.flr(dist / 3);
    if (idx >= shades.length) idx = shades.length - 1;
    return shades[idx];
}

// --- Callbacks -------------------------------------------------------

export function _init(): void {
    // nothing needed
}

export function _update(dt: number): void {

    // Toggle minimap
    if (key.btnp(key.M)) showMinimap = !showMinimap;

    // Turning
    if (input.btn('turn_left')) pa -= TURN_SPEED * dt;
    if (input.btn('turn_right')) pa += TURN_SPEED * dt;

    // Gamepad right stick for turning
    if (pad.connected(0)) {
        const rx = pad.axis(pad.RX, 0);
        pa += rx * TURN_SPEED * dt;
    }

    // Movement vectors
    const cosA = Math.cos(pa);
    const sinA = Math.sin(pa);

    let dx = 0;
    let dy = 0;

    // Forward / back
    if (input.btn('forward')) {
        dx += cosA * MOVE_SPEED * dt;
        dy += sinA * MOVE_SPEED * dt;
    }
    if (input.btn('back')) {
        dx -= cosA * MOVE_SPEED * dt;
        dy -= sinA * MOVE_SPEED * dt;
    }

    // Strafe (left perpendicular in Y-down coords is (sinA, -cosA))
    if (input.btn('strafe_left')) {
        dx += sinA * MOVE_SPEED * dt;
        dy -= cosA * MOVE_SPEED * dt;
    }
    if (input.btn('strafe_right')) {
        dx -= sinA * MOVE_SPEED * dt;
        dy += cosA * MOVE_SPEED * dt;
    }

    // Gamepad left stick movement
    if (pad.connected(0)) {
        const lx = pad.axis(pad.LX, 0);
        const ly = pad.axis(pad.LY, 0);
        // Forward/back on LY
        dx += cosA * -ly * MOVE_SPEED * dt;
        dy += sinA * -ly * MOVE_SPEED * dt;
        // Strafe on LX (positive LX = right = (-sinA, cosA))
        dx -= sinA * lx * MOVE_SPEED * dt;
        dy += cosA * lx * MOVE_SPEED * dt;
    }

    // Slide along walls — try X and Y independently
    const margin = 0.2;
    if (mapAt(px + dx + (dx > 0 ? margin : -margin), py) === 0) {
        px += dx;
    }
    if (mapAt(px, py + dy + (dy > 0 ? margin : -margin)) === 0) {
        py += dy;
    }
}

export function _draw(): void {
    // Ceiling
    gfx.rectfill(0, 0, SCREEN_W - 1, HALF_H - 1, 0);
    // Floor
    gfx.rectfill(0, HALF_H, SCREEN_W - 1, SCREEN_H - 1, 5);

    // --- Cast rays (batched drawing) ---------------------------------

    const angleStep = FOV / NUM_RAYS;
    const startAngle = pa - HALF_FOV;
    const ipx = math.flr(px);
    const ipy = math.flr(py);

    // State for batching adjacent columns with same color & height
    let batchX0 = 0;
    let batchStart = 0;
    let batchEnd = 0;
    let batchCol = -1;

    for (let i = 0; i <= NUM_RAYS; i++) {
        let col = -1;
        let drawStart = 0;
        let drawEnd = 0;

        if (i < NUM_RAYS) {
            const rayAngle = startAngle + i * angleStep;
            const rayCos = Math.cos(rayAngle);
            const raySin = Math.sin(rayAngle);

            // DDA setup
            let mapX = ipx;
            let mapY = ipy;

            const stepX = rayCos >= 0 ? 1 : -1;
            const stepY = raySin >= 0 ? 1 : -1;

            const deltaX = rayCos === 0 ? 1e30 : rayCos > 0 ? 1 / rayCos : -1 / rayCos;
            const deltaY = raySin === 0 ? 1e30 : raySin > 0 ? 1 / raySin : -1 / raySin;

            let sideX = rayCos < 0 ? (px - mapX) * deltaX : (mapX + 1 - px) * deltaX;
            let sideY = raySin < 0 ? (py - mapY) * deltaY : (mapY + 1 - py) * deltaY;

            // DDA traversal — inline map lookup
            let side = 0;
            for (;;) {
                if (sideX < sideY) {
                    sideX += deltaX;
                    mapX += stepX;
                    side = 0;
                } else {
                    sideY += deltaY;
                    mapY += stepY;
                    side = 1;
                }
                if (
                    mapX < 0 ||
                    mapX >= MAP_W ||
                    mapY < 0 ||
                    mapY >= MAP_H ||
                    worldMap[mapY * MAP_W + mapX] !== 0
                ) {
                    break;
                }
            }

            // Perpendicular distance (fixes fisheye)
            let perpDist;
            if (side === 0) {
                perpDist = (mapX - px + (1 - stepX) / 2) / rayCos;
            } else {
                perpDist = (mapY - py + (1 - stepY) / 2) / raySin;
            }
            if (perpDist < 0.01) perpDist = 0.01;

            // Wall column height
            const lineH = math.flr(SCREEN_H / perpDist);
            drawStart = math.flr(HALF_H - lineH / 2);
            drawEnd = drawStart + lineH - 1;
            if (drawStart < 0) drawStart = 0;
            if (drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

            // Shade by distance + side
            col = shadeColor(perpDist, side === 0);
        }

        // Flush batch when color or height changes (or last column)
        if (col !== batchCol || drawStart !== batchStart || drawEnd !== batchEnd) {
            if (batchCol >= 0) {
                gfx.rectfill(batchX0, batchStart, i - 1, batchEnd, batchCol);
            }
            batchX0 = i;
            batchStart = drawStart;
            batchEnd = drawEnd;
            batchCol = col;
        }
    }

    // --- Minimap overlay (top-left, toggleable with M) ---------------

    if (showMinimap) {
        const mmScale = 4;
        const mmX0 = 4;
        const mmY0 = 4;

        for (let my = 0; my < MAP_H; my++) {
            for (let mx = 0; mx < MAP_W; mx++) {
                const c = worldMap[my * MAP_W + mx] !== 0 ? 6 : 0;
                gfx.rectfill(
                    mmX0 + mx * mmScale,
                    mmY0 + my * mmScale,
                    mmX0 + mx * mmScale + mmScale - 1,
                    mmY0 + my * mmScale + mmScale - 1,
                    c,
                );
            }
        }

        // Player dot on minimap
        const ppX = mmX0 + math.flr(px * mmScale);
        const ppY = mmY0 + math.flr(py * mmScale);
        gfx.rectfill(ppX - 1, ppY - 1, ppX + 1, ppY + 1, 8);

        // Direction line on minimap
        const dirLen = 6;
        gfx.line(
            ppX,
            ppY,
            ppX + math.flr(Math.cos(pa) * dirLen),
            ppY + math.flr(Math.sin(pa) * dirLen),
            11,
        );
    }

    // --- HUD ---------------------------------------------------------

    gfx.print('WASD/Arrows: Move  M: Map', 72, 2, 7);

    // FPS widget
    sys.drawFps();
}
