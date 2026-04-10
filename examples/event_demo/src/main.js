// Event Demo — event bus and system events
//
// Demonstrates: evt.on, evt.once, evt.off, evt.emit,
//   system events (sys:pause, sys:resume, sys:focus_lost, sys:focus_gained)

// --- FPS widget ----------------------------------------------------------
let smoothFps = 60;
let fpsHistory = [];
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
const MAX_LOG = 12;
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
        combo++;
        logEvent(`coin:collect  score=${score} combo=${combo}`);
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
    smoothFps = math.lerp(smoothFps, sys.fps(), 0.05);
    fpsHistory[fpsHistIdx] = smoothFps;
    fpsHistIdx = (fpsHistIdx + 1) % FPS_HIST_LEN;

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
                combo++;
                logEvent(`coin:collect  score=${score} combo=${combo}`);
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
    gfx.rectfill(0, ly - 1, 200, 179, 0);
    gfx.print('-- event log --', lx, ly - 8, 6);
    for (let i = 0; i < eventLog.length; ++i) {
        const entry = eventLog[i];
        const c2 = entry.age < 0.5 ? 7 : entry.age < 2 ? 6 : 5;
        gfx.print(entry.text, lx, ly + i * 7, c2);
    }

    // Controls hint
    gfx.print('O=off score  R=re-register', 2, 172, 5);

    drawFpsWidget();
}

function _save() {
    return {
        score,
        playerX,
        playerY,
        coins,
        combo,
        smoothFps,
        fpsHistory,
        fpsHistIdx,
    };
}

function _restore(s) {
    score = s.score;
    playerX = s.playerX;
    playerY = s.playerY;
    coins = s.coins;
    combo = s.combo;
    smoothFps = s.smoothFps;
    fpsHistory = s.fpsHistory;
    fpsHistIdx = s.fpsHistIdx;
}
