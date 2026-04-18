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

function prvStatus(msg: string) {
    statusMsg = msg;
    statusTimer = 120; // 2 seconds at 60fps
}

// --- Callbacks -------------------------------------------------------

export function _init(): void {
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



export function _update(_dt: number): void {
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

export function _draw(): void {
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
    sys.drawFps();
}
