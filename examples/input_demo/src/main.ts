// Input Demo
//
// Shows how to read from all input sources:
//   - key.btn / key.btnp  — raw keyboard
//   - mouse.x/y, mouse.btn — mouse position and buttons
//   - pad.btn, pad.axis   — gamepad buttons and sticks
//   - input.btn / input.btnp — virtual action mappings
//
// Move the box with arrow keys / WASD / gamepad / virtual actions.
// Press SPACE/Z to flash.  Mouse cursor draws a crosshair.

// --- State -----------------------------------------------------------

let px = 160;
let py = 90;
const speed = 80; // pixels per second
let flash = 0;
const logLines: string[] = []; // recent input events
let wheelDisplay = 0; // accumulated wheel for display
let wheelDecay = 0; // timer to fade wheel display

function prvLog(msg: string) {
    logLines.push(msg);
    if (logLines.length > 8) {
        logLines.shift();
    }
}



// --- Callbacks -------------------------------------------------------

export function _init(): void {
    mouse.show();
}

export function _update(dt: number): void {
    // Virtual action movement (keyboard/gamepad mapped)
    if (input.btn('left')) px -= speed * dt;
    if (input.btn('right')) px += speed * dt;
    if (input.btn('up')) py -= speed * dt;
    if (input.btn('down')) py += speed * dt;

    // Left stick direct movement
    if (pad.connected(0)) {
        const slx = pad.axis(pad.LX, 0);
        const sly = pad.axis(pad.LY, 0);
        px += slx * speed * dt;
        py += sly * speed * dt;
    }

    // Jump / action flash
    if (input.btnp('jump')) {
        flash = 10;
        prvLog('jump pressed');
    }
    if (input.btnp('action')) {
        flash = 10;
        prvLog('action pressed');
    }

    // Clamp to screen
    if (px < 4) px = 4;
    if (px > 312) px = 312;
    if (py < 4) py = 4;
    if (py > 172) py = 172;

    if (flash > 0) --flash;

    // Log raw key presses
    if (key.btnp(key.ENTER)) prvLog('key: ENTER');
    if (key.btnp(key.ESCAPE)) prvLog('key: ESCAPE');

    // Log mouse clicks
    if (mouse.btnp(mouse.LEFT)) prvLog('mouse: LEFT click');
    if (mouse.btnp(mouse.MIDDLE)) prvLog('mouse: MIDDLE click');
    if (mouse.btnp(mouse.RIGHT)) prvLog('mouse: RIGHT click');

    // Log scroll wheel
    const w = mouse.wheel();
    if (w !== 0) {
        wheelDisplay += w;
        wheelDecay = 1.0;
        prvLog(`mouse: wheel ${w > 0 ? 'up' : 'down'}`);
    }
    if (wheelDecay > 0) {
        wheelDecay -= dt;
        if (wheelDecay <= 0) wheelDisplay = 0;
    }

    // Log gamepad presses
    if (pad.btnp(pad.A, 0)) prvLog('pad: A');
    if (pad.btnp(pad.B, 0)) prvLog('pad: B');
    if (pad.btnp(pad.X, 0)) prvLog('pad: X');
    if (pad.btnp(pad.Y, 0)) prvLog('pad: Y');
    if (pad.btnp(pad.L1, 0)) prvLog('pad: L1');
    if (pad.btnp(pad.R1, 0)) prvLog('pad: R1');
    if (pad.btnp(pad.L2, 0)) prvLog('pad: L2');
    if (pad.btnp(pad.R2, 0)) prvLog('pad: R2');
    if (pad.btnp(pad.START, 0)) prvLog('pad: START');
    if (pad.btnp(pad.SELECT, 0)) prvLog('pad: SELECT');
    if (pad.btnp(pad.UP, 0)) prvLog('pad: DPAD UP');
    if (pad.btnp(pad.DOWN, 0)) prvLog('pad: DPAD DN');
    if (pad.btnp(pad.LEFT, 0)) prvLog('pad: DPAD L');
    if (pad.btnp(pad.RIGHT, 0)) prvLog('pad: DPAD R');

    // Log touch events
    for (let ti = 0; ti < touch.MAX_FINGERS; ++ti) {
        if (touch.pressed(ti)) prvLog(`touch: finger ${ti} down`);
        if (touch.released(ti)) prvLog(`touch: finger ${ti} up`);
    }
}

export function _draw(): void {
    gfx.cls(1);

    // --- Player box ---
    const col = flash > 0 ? 10 : 7;
    gfx.rectfill(px - 4, py - 4, px + 4, py + 4, col);

    // --- Key state display ---
    gfx.print('-- keyboard --', 4, 4, 6);
    let arrows = '';
    if (key.btn(key.LEFT)) arrows += 'L ';
    if (key.btn(key.RIGHT)) arrows += 'R ';
    if (key.btn(key.UP)) arrows += 'U ';
    if (key.btn(key.DOWN)) arrows += 'D ';
    if (key.btn(key.W)) arrows += 'W ';
    if (key.btn(key.A)) arrows += 'A ';
    if (key.btn(key.S)) arrows += 'S ';
    if (key.btn(key.D)) arrows += 'D ';
    if (key.btn(key.Z)) arrows += 'Z ';
    if (key.btn(key.X)) arrows += 'X ';
    if (key.btn(key.SPACE)) arrows += 'SPC ';
    gfx.print(arrows.length > 0 ? arrows : '(none held)', 4, 12, 7);

    // --- Mouse ---
    gfx.print('-- mouse --', 4, 24, 6);
    gfx.print(`pos: ${mouse.x()},${mouse.y()}`, 4, 32, 7);
    let btns = '';
    if (mouse.btn(mouse.LEFT)) btns += 'L ';
    if (mouse.btn(mouse.MIDDLE)) btns += 'M ';
    if (mouse.btn(mouse.RIGHT)) btns += 'R ';
    gfx.print(`btn: ${btns.length > 0 ? btns : '-'}`, 4, 40, 7);
    gfx.print(`wheel: ${wheelDisplay}`, 4, 48, 7);

    // Crosshair
    const mx = mouse.x();
    const my = mouse.y();
    gfx.line(mx - 4, my, mx + 4, my, 8);
    gfx.line(mx, my - 4, mx, my + 4, 8);

    // --- Gamepad ---
    gfx.print('-- gamepad 0 --', 4, 60, 6);
    if (pad.connected(0)) {
        gfx.print(pad.name(0), 4, 68, 7);

        // Left stick
        const lx = math.flr(pad.axis(pad.LX, 0) * 100) / 100;
        const ly = math.flr(pad.axis(pad.LY, 0) * 100) / 100;
        const lsCx = 24;
        const lsCy = 90;
        gfx.circ(lsCx, lsCy, 10, 5);
        gfx.circfill(lsCx + lx * 10, lsCy + ly * 10, 2, 11);
        gfx.print('L', lsCx - 2, lsCy + 12, 6);

        // Right stick
        const rx = math.flr(pad.axis(pad.RX, 0) * 100) / 100;
        const ry = math.flr(pad.axis(pad.RY, 0) * 100) / 100;
        const rsCx = 64;
        const rsCy = 90;
        gfx.circ(rsCx, rsCy, 10, 5);
        gfx.circfill(rsCx + rx * 10, rsCy + ry * 10, 2, 8);
        gfx.print('R', rsCx - 2, rsCy + 12, 6);

        // Triggers
        const l2 = math.flr(pad.axis(pad.TL, 0) * 100);
        const r2 = math.flr(pad.axis(pad.TR, 0) * 100);
        gfx.print(`L2:${l2}% R2:${r2}%`, 4, 108, 7);

        // Button grid — show held state for face + shoulder + dpad
        const btnNames = ['A', 'B', 'X', 'Y', 'L1', 'R1', 'ST', 'SL'];
        const btnIds = [pad.A, pad.B, pad.X, pad.Y, pad.L1, pad.R1, pad.START, pad.SELECT];
        let bx = 4;
        for (let i = 0; i < btnNames.length; ++i) {
            const held = pad.btn(btnIds[i], 0);
            const bc = held ? 11 : 5;
            gfx.print(btnNames[i], bx, 118, bc);
            bx += (btnNames[i].length + 1) * 6;
        }

        // D-pad visual
        const dpCx = 110;
        const dpCy = 90;
        gfx.print('D', dpCx - 2, dpCy + 12, 6);
        gfx.rectfill(dpCx - 2, dpCy - 8, dpCx + 2, dpCy + 8, 5); // vert
        gfx.rectfill(dpCx - 8, dpCy - 2, dpCx + 8, dpCy + 2, 5); // horiz
        if (pad.btn(pad.UP, 0)) gfx.rectfill(dpCx - 2, dpCy - 8, dpCx + 2, dpCy - 3, 11);
        if (pad.btn(pad.DOWN, 0)) gfx.rectfill(dpCx - 2, dpCy + 3, dpCx + 2, dpCy + 8, 11);
        if (pad.btn(pad.LEFT, 0)) gfx.rectfill(dpCx - 8, dpCy - 2, dpCx - 3, dpCy + 2, 11);
        if (pad.btn(pad.RIGHT, 0)) gfx.rectfill(dpCx + 3, dpCy - 2, dpCx + 8, dpCy + 2, 11);
    } else {
        gfx.print('(not connected)', 4, 68, 5);
    }

    // --- Virtual actions ---
    gfx.print('-- actions --', 160, 4, 6);
    const actions = ['left', 'right', 'up', 'down', 'jump', 'action'];
    for (let i = 0; i < actions.length; ++i) {
        const name = actions[i];
        const held = input.btn(name);
        const c = held ? 11 : 5;
        gfx.print(`${name}: ${held ? 'HELD' : '-'}`, 160, 12 + i * 8, c);
    }

    // --- Touch ---
    gfx.print('-- touch --', 160, 62, 6);
    const tc = touch.count();
    if (tc > 0) {
        gfx.print(`fingers: ${tc}`, 160, 70, 7);
        for (let ti = 0; ti < touch.MAX_FINGERS; ++ti) {
            if (!touch.active(ti)) continue;
            const tx = math.flr(touch.x(ti));
            const ty = math.flr(touch.y(ti));
            // Draw filled circle at touch point
            gfx.circfill(tx, ty, 4, 12);
            gfx.print(`${ti}`, tx + 6, ty - 3, 12);
        }
    } else {
        gfx.print('(no touches)', 160, 70, 5);
    }

    // --- Event log ---
    gfx.print('-- event log --', 160, 82, 6);
    for (let i = 0; i < logLines.length; ++i) {
        gfx.print(logLines[i], 160, 90 + i * 8, 7);
    }

    // --- Instructions ---
    gfx.print('arrows/wasd/lstick=move z/spc=flash', 4, 170, 5);

    // FPS widget
    sys.drawFps();
}
