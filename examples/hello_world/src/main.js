// Hello World cart — centered text example

let smoothFps = 60;
const fpsHistory = [];
let fpsHistIdx = 0;
const FPS_HIST_LEN = 50;
for (let _i = 0; _i < FPS_HIST_LEN; ++_i) fpsHistory.push(60);

function drawFpsWidget() {
    const wx = 320 - FPS_HIST_LEN - 4;
    const wy = 0;
    const ww = FPS_HIST_LEN + 4;
    const gh = 16;
    const target = sys.targetFps();
    gfx.rectfill(wx, wy, wx + ww - 1, wy + 8 + gh + 1, 0);
    gfx.print(`${math.flr(smoothFps)} FPS`, wx + 2, wy + 1, 7);
    gfx.rect(wx + 1, wy + 8, wx + ww - 2, wy + 8 + gh, 5);
    for (let idx = 1; idx < FPS_HIST_LEN; ++idx) {
        const i0 = (fpsHistIdx + idx - 1) % FPS_HIST_LEN;
        const i1 = (fpsHistIdx + idx) % FPS_HIST_LEN;
        const v0 = math.clamp(fpsHistory[i0] / target, 0, 1);
        const v1 = math.clamp(fpsHistory[i1] / target, 0, 1);
        const y0 = wy + 8 + gh - 1 - math.flr(v0 * (gh - 2));
        const y1 = wy + 8 + gh - 1 - math.flr(v1 * (gh - 2));
        const clr = v1 > 0.9 ? 11 : v1 > 0.5 ? 9 : 8;
        gfx.line(wx + 2 + idx - 1, y0, wx + 2 + idx, y1, clr);
    }
}

function _init() {
    gfx.cls(0);
}

function _update(dt) {
    smoothFps = math.lerp(smoothFps, sys.fps(), 0.05);
    fpsHistory[fpsHistIdx] = smoothFps;
    fpsHistIdx = (fpsHistIdx + 1) % FPS_HIST_LEN;
}

function _draw() {
    gfx.cls(0);
    gfx.print('Hello World', 133, 87, 7);

    // FPS widget
    drawFpsWidget();
}
