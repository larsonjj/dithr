// Touch Demo — multi-finger input visualization
//
// On touch devices, tap and drag to see finger positions, pressure,
// and delta movement. On desktop, use mouse clicks as single-finger touch.

const COLORS = [7, 8, 9, 10, 11, 12, 13, 14];
const RADIUS = 6;

function _init() {
    gfx.cls(0);
}

function _update() {
    // No state to update — touch is read directly in _draw
}

function _draw() {
    gfx.cls(0);

    const count = touch.count();

    gfx.print('Touch Demo', 4, 4, 7);
    gfx.print(`Active fingers: ${count}`, 4, 14, 6);

    if (count === 0) {
        gfx.print('Tap or click to begin', 4, 30, 5);
        gfx.print('', 4, 40, 5);
        gfx.print('touch.count()     - active fingers', 4, 50, 5);
        gfx.print('touch.x/y(i)      - finger position', 4, 60, 5);
        gfx.print('touch.dx/dy(i)    - delta movement', 4, 70, 5);
        gfx.print('touch.pressure(i) - finger pressure', 4, 80, 5);
        gfx.print('touch.pressed(i)  - just pressed?', 4, 90, 5);
        gfx.print('touch.released(i) - just released?', 4, 100, 5);
        gfx.print(`touch.MAX_FINGERS = ${touch.MAX_FINGERS}`, 4, 116, 6);
        return;
    }

    for (let i = 0; i < count; i += 1) {
        const tx = touch.x(i);
        const ty = touch.y(i);
        const dx = touch.dx(i);
        const dy = touch.dy(i);
        const pr = touch.pressure(i);
        const col = COLORS[i % COLORS.length];
        const r = RADIUS + Math.floor(pr * 10);

        // Draw finger circle
        gfx.circfill(tx, ty, r, col);
        gfx.circ(tx, ty, r, 7);

        // Draw delta vector
        if (dx !== 0 || dy !== 0) {
            gfx.line(tx, ty, tx + dx * 4, ty + dy * 4, 7);
        }

        // Label
        const label = `#${i} (${Math.floor(tx)},${Math.floor(ty)}) p=${pr.toFixed(2)}`;
        gfx.print(label, 4, 28 + i * 10, col);

        // Flash on press/release
        if (touch.pressed(i)) {
            gfx.circfill(tx, ty, r + 4, 7);
        }
        if (touch.released(i)) {
            gfx.circ(tx, ty, r + 6, 8);
        }
    }

    sys.drawFps();
}
