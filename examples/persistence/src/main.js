// Persistence Demo
//
// Shows how to:
//   - Save / load key-value data with cart.save() / cart.load()
//   - Use numeric data slots with cart.dset() / cart.dget()
//   - Check for key existence with cart.has()
//   - Delete keys with cart.delete()
//   - Read cart metadata with cart.title() / cart.author() / cart.version()
//
// A simple click-counter and high-score that persist across restarts.

// --- State -----------------------------------------------------------

let clickCount = 0;
let highScore = 0;
let playerName = '';
let statusMsg = '';
let statusTimer = 0;

// --- Helpers ---------------------------------------------------------

function prvStatus(msg) {
    statusMsg = msg;
    statusTimer = 120; // 2 seconds at 60fps
}

// --- Callbacks -------------------------------------------------------

function _init() {
    // Restore persisted state
    if (cart.has('clickCount')) {
        clickCount = parseInt(cart.load('clickCount'), 10) || 0;
    }
    if (cart.has('playerName')) {
        playerName = cart.load('playerName');
    }

    // Numeric slot 0 stores the high score
    highScore = cart.dget(0);
    if (highScore === undefined) highScore = 0;

    prvStatus(`loaded! clicks=${clickCount} high=${highScore}`);
}

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

function _update(dt) {
    smoothFps = math.lerp(smoothFps, sys.fps(), 0.05);
    fpsHistory[fpsHistIdx] = smoothFps;
    fpsHistIdx = (fpsHistIdx + 1) % FPS_HIST_LEN;
    if (statusTimer > 0) --statusTimer;

    // Z = click (increment counter)
    if (key.btnp(key.Z)) {
        ++clickCount;
        cart.save('clickCount', `${clickCount}`);

        if (clickCount > highScore) {
            highScore = clickCount;
            cart.dset(0, highScore);
            prvStatus(`new high score: ${highScore}!`);
        } else {
            prvStatus(`click! (${clickCount})`);
        }
    }

    // X = set player name to "Player" (simulates text entry)
    if (key.btnp(key.X)) {
        playerName = `Player_${math.rndInt(1000)}`;
        cart.save('playerName', playerName);
        prvStatus(`name saved: ${playerName}`);
    }

    // C = reset (delete all saved data)
    if (key.btnp(key.C)) {
        cart.delete('clickCount');
        cart.delete('playerName');
        cart.dset(0, 0);
        clickCount = 0;
        highScore = 0;
        playerName = '';
        prvStatus('all data cleared!');
    }
}

function _draw() {
    gfx.cls(1);

    // --- Cart metadata ---
    gfx.print('=== persistence demo ===', 80, 4, 7);
    gfx.print(`cart: ${cart.title()} v${cart.version()}`, 4, 16, 6);
    gfx.print(`by: ${cart.author() || '(no author)'}`, 4, 24, 6);

    // --- Saved data display ---
    gfx.rectfill(4, 38, 220, 100, 0);
    gfx.rect(4, 38, 220, 100, 5);

    gfx.print('saved data:', 8, 42, 7);

    gfx.print(`clickCount: ${clickCount}`, 8, 54, 11);
    gfx.print(`highScore:  ${highScore}`, 8, 62, 10);
    gfx.print(`playerName: ${playerName || '(not set)'}`, 8, 70, 12);

    gfx.print(`has(clickCount): ${cart.has('clickCount')}`, 8, 82, 5);
    gfx.print(`has(playerName): ${cart.has('playerName')}`, 8, 90, 5);

    // --- Numeric slots ---
    gfx.print('numeric slots:', 4, 110, 6);
    for (let i = 0; i < 4; ++i) {
        const val = cart.dget(i);
        gfx.print(`slot[${i}] = ${val !== undefined ? val : 'empty'}`, 8, 120 + i * 8, 7);
    }

    // --- Status message ---
    if (statusTimer > 0) {
        gfx.print(statusMsg, 4, 156, 10);
    }

    // --- Controls ---
    gfx.print('z=click  x=set name  c=reset', 4, 170, 5);

    // FPS widget
    drawFpsWidget();
}
