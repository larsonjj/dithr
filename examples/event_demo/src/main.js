// Event Demo — event bus and system events
//
// Demonstrates: evt.on, evt.once, evt.off, evt.emit,
//   system events (sys:pause, sys:resume, sys:focus_lost, sys:focus_gained)

// --- FPS widget ----------------------------------------------------------

// -------------------------------------------------------------------------
// Game state — a tiny coin-collecting game driven entirely by events
// -------------------------------------------------------------------------

let score = 0;
let coins = [];
const MAX_COINS = 8;
let playerX = 160;
let playerY = 90;
const SPEED = 80;

// Event log displayed on screen
const eventLog = [];
const MAX_LOG = 6;
let handleScore = -1;
let _handleSpawn = -1;
let combo = 0;
let _comboHandle = -1;

function logEvent(msg) {
    eventLog.push({ text: msg, age: 0 });
    if (eventLog.length > MAX_LOG) eventLog.shift();
}

function spawnCoin() {
    if (coins.length >= MAX_COINS) return;
    coins.push({
        x: math.rndInt(304) + 8,
        y: math.rndInt(152) + 20,
        collected: false,
    });
}

function _init() {
    // --- Custom game events ---

    // Score handler — persistent
    handleScore = evt.on('coin:collect', (e) => {
        score += e.value;
        logEvent(`coin:collect  score=${score}`);
    });

    // Combo handler — always active, separate from score
    evt.on('coin:collect', () => {
        combo++;
        logEvent(`combo=${combo}`);
    });

    // Coin respawn — persistent
    _handleSpawn = evt.on('coin:collect', () => {
        // Queue a new spawn event
        evt.emit('coin:spawn');
    });

    // Spawn handler
    evt.on('coin:spawn', () => {
        spawnCoin();
        logEvent(`coin:spawn  total=${coins.length + 1}`);
    });

    // Combo reset — one-shot, re-registered each time
    function registerComboReset() {
        _comboHandle = evt.once('combo:reset', () => {
            logEvent(`combo:reset  was=${combo}`);
            combo = 0;
            registerComboReset(); // re-register
        });
    }
    registerComboReset();

    // --- System events ---
    evt.on('sys:pause', () => {
        logEvent('>>> sys:pause');
    });
    evt.on('sys:resume', () => {
        logEvent('>>> sys:resume');
    });
    evt.on('sys:focus_lost', () => {
        logEvent('>>> sys:focus_lost');
    });
    evt.on('sys:focus_gained', () => {
        logEvent('>>> sys:focus_gained');
    });

    // Spawn initial coins
    for (let i = 0; i < 5; ++i) spawnCoin();

    logEvent('Game started! Collect coins with arrow keys.');
    logEvent('Press O to evt.off score handler, R to re-register.');
}

let comboTimer = 0;

function _update(dt) {
    // Age log entries
    for (let i = 0; i < eventLog.length; ++i) {
        eventLog[i].age += dt;
    }

    // Movement
    if (input.btn('left')) playerX -= SPEED * dt;
    if (input.btn('right')) playerX += SPEED * dt;
    if (input.btn('up')) playerY -= SPEED * dt;
    if (input.btn('down')) playerY += SPEED * dt;
    playerX = math.clamp(playerX, 0, 312);
    playerY = math.clamp(playerY, 0, 172);

    // Check coin collisions
    for (let c = coins.length - 1; c >= 0; --c) {
        if (!coins[c].collected && col.rect(playerX, playerY, 8, 8, coins[c].x, coins[c].y, 6, 6)) {
            coins[c].collected = true;
            coins.splice(c, 1);
            evt.emit('coin:collect', { value: 10 });
            comboTimer = 1.5;
        }
    }

    // Combo timeout
    if (combo > 0) {
        comboTimer -= dt;
        if (comboTimer <= 0) {
            evt.emit('combo:reset');
        }
    }

    // Toggle score handler with O key
    if (key.btnp(key.O)) {
        if (handleScore >= 0) {
            evt.off(handleScore);
            handleScore = -1;
            logEvent("evt.off(score handler) — coins won't add score!");
        }
    }

    // Re-register score handler with R key
    if (key.btnp(key.R)) {
        if (handleScore < 0) {
            handleScore = evt.on('coin:collect', (e) => {
                score += e.value;
                logEvent(`coin:collect  score=${score}`);
            });
            logEvent('Re-registered score handler');
        }
    }
}

function _draw() {
    gfx.cls(1);

    // Draw coins
    for (let c = 0; c < coins.length; ++c) {
        gfx.circfill(coins[c].x + 3, coins[c].y + 3, 3, 10);
        gfx.circ(coins[c].x + 3, coins[c].y + 3, 3, 9);
    }

    // Draw player
    gfx.rectfill(playerX, playerY, playerX + 7, playerY + 7, 8);
    gfx.rect(playerX, playerY, playerX + 7, playerY + 7, 2);

    // Score / combo HUD
    gfx.rectfill(0, 0, 160, 9, 0);
    gfx.print(`SCORE: ${score}  COMBO: ${combo}`, 2, 1, 7);

    // Event log panel
    const lx = 1;
    const ly = 180 - MAX_LOG * 7 - 2;
    gfx.rectfill(0, ly - 9, 200, 179, 0);
    gfx.print('-- event log --', lx, ly - 8, 6);
    for (let i = 0; i < eventLog.length; ++i) {
        const entry = eventLog[i];
        const c2 = entry.age < 0.5 ? 7 : entry.age < 2 ? 6 : 5;
        gfx.print(entry.text, lx, ly + i * 7, c2);
    }

    // Controls hint
    gfx.print('O=off score  R=re-register', 2, 172, 5);

    sys.drawFps();
}

function _save() {
    return {
        score,
        playerX,
        playerY,
        coins,
        combo,
    };
}

function _restore(s) {
    score = s.score;
    playerX = s.playerX;
    playerY = s.playerY;
    coins = s.coins;
    combo = s.combo;
}
