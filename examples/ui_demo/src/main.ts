// UI & Tween Demo
//
// Showcases the ui.* layout API and tween.* animation engine:
//   - ui.anchor, ui.inset, ui.hsplit, ui.vsplit, ui.hstack, ui.vstack, ui.place
//   - tween.start (with Promise chaining), tween.tick, tween.val, tween.ease
//
// Press SPACE to replay the intro animation.
// Press Z to toggle the sidebar open/closed.

// --- Constants -------------------------------------------------------

const COL_BG = 1; // dark blue background
const COL_PANEL = 2; // dark purple panel fill
const COL_BORDER = 5; // dark grey border
const COL_TITLE = 7; // white
const COL_LABEL = 6; // light grey
const COL_BAR_BG = 0; // black
const COL_BAR_HP = 11; // green
const COL_BAR_MP = 12; // blue
const COL_BAR_XP = 10; // yellow
const COL_HIGHLIGHT = 8; // red
const COL_MENU = 13; // purple-ish

// --- State -----------------------------------------------------------

let sidebarOpen = true;
let sidebarTw: DithrTweenHandle | null = null; // tween handle for sidebar slide
let sidebarT = 1.0; // 0 = closed, 1 = open

let hp = 0.7;
let mp = 0.4;
let xp = 0.55;

let barTweens: DithrTweenHandle[] = []; // intro tweens for stat bars
let barValues = [0, 0, 0];

const menuItems = ['Inventory', 'Skills', 'Map', 'Settings'];
let menuTweens: DithrTweenHandle[] = []; // staggered menu slide-in tweens
let menuOffsets: number[] = []; // current x-offsets per item

let titleTw: DithrTweenHandle | null = null;
let titleAlpha = 0; // 0..1 for title fade-in

let bounceTw: DithrTweenHandle | null = null;
let bounceY = 0;

let introDone = false;

// --- Helpers ---------------------------------------------------------

function drawPanel(r: { x: number; y: number; w: number; h: number }, col: number) {
    gfx.rectfill(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, col);
    gfx.rect(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, COL_BORDER);
}

function drawBar(r: { x: number; y: number; w: number; h: number }, fill: number, col: number) {
    gfx.rectfill(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, COL_BAR_BG);
    if (fill > 0) {
        const fw = math.flr(r.w * math.min(fill, 1));
        if (fw > 0) {
            gfx.rectfill(r.x, r.y, r.x + fw - 1, r.y + r.h - 1, col);
        }
    }
    gfx.rect(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, COL_BORDER);
}

function drawLabel(text: string, r: { x: number; y: number; w: number; h: number }, col: number) {
    gfx.print(text, r.x + 2, r.y + 1, col);
}

// --- Animation Setup -------------------------------------------------

function startIntro() {
    introDone = false;

    // Reset values
    sidebarT = 0;
    barValues = [0, 0, 0];
    menuOffsets = [80, 80, 80, 80];

    // Sidebar slides in
    sidebarTw = tween.start(0, 1, 0.5, 'easeOutCubic');
    sidebarTw.done.then(() => {
        sidebarT = 1;
    });

    // Stat bars fill up (staggered)
    const targets = [hp, mp, xp];
    barTweens = [];
    for (let i = 0; i < 3; i++) {
        barTweens.push(tween.start(0, targets[i], 0.6, 'easeOutQuad', 0.3 + i * 0.15));
    }

    // Menu items slide in from the right (staggered)
    menuTweens = [];
    for (let j = 0; j < menuItems.length; j++) {
        menuTweens.push(tween.start(80, 0, 0.4, 'easeOutBack', 0.2 + j * 0.1));
    }

    // Title fades in
    titleTw = tween.start(0, 1, 0.8, 'easeInOut', 0.1);

    // Bouncing indicator
    bounceTw = tween.start(0, 6, 0.6, 'easeOutBounce', 0.5);
    bounceTw.done.then(() => {
        // Chain: bounce back
        bounceTw = tween.start(6, 0, 0.4, 'easeInOut');
        bounceTw.done.then(() => {
            bounceY = 0;
            introDone = true;
        });
    });
}

// --- Hot-reload state preservation -----------------------------------

export function _save() {
    return {
        sidebarOpen,
        sidebarT,
        hp,
        mp,
        xp,
        introDone,
    };
}

export function _restore(s: any): void {
    sidebarOpen = s.sidebarOpen;
    sidebarT = s.sidebarT;
    hp = s.hp;
    mp = s.mp;
    xp = s.xp;
    introDone = s.introDone;
    barValues = [hp, mp, xp];
    menuOffsets = [0, 0, 0, 0];
    titleAlpha = 1;
    bounceY = 0;
}

// --- Callbacks -------------------------------------------------------

export function _init(): void {
    if (!introDone) {
        startIntro();
    }
}

export function _update(dt: number): void {
    tween.tick(dt);

    // Read tween values
    if (sidebarTw) {
        sidebarT = tween.val(sidebarTw, sidebarT);
    }
    for (let i = 0; i < barTweens.length; i++) {
        if (barTweens[i]) {
            barValues[i] = tween.val(barTweens[i], barValues[i]);
        }
    }
    for (let j = 0; j < menuTweens.length; j++) {
        if (menuTweens[j]) {
            menuOffsets[j] = tween.val(menuTweens[j], menuOffsets[j]);
        }
    }
    if (titleTw) {
        titleAlpha = tween.val(titleTw, titleAlpha);
    }
    if (bounceTw) {
        bounceY = tween.val(bounceTw, bounceY);
    }

    // Replay intro
    if (key.btnp(key.SPACE)) {
        tween.cancelAll();
        startIntro();
    }

    // Toggle sidebar
    if (key.btnp(key.Z)) {
        sidebarOpen = !sidebarOpen;
        const from = sidebarT;
        const to = sidebarOpen ? 1 : 0;
        sidebarTw = tween.start(from, to, 0.3, 'easeInOut');
        sidebarTw.done.then(() => {
            sidebarT = to;
        });
    }
}

export function _draw(): void {
    gfx.cls(COL_BG);

    // Full screen rect
    const screen = ui.rect(0, 0, 320, 180);

    // --- Title bar (top 14px) ---
    const parts = ui.vsplit(screen, 14 / 180, 0);
    const titleBar = parts[0];
    const body = parts[1];

    // Draw title bar
    drawPanel(titleBar, COL_PANEL);
    if (titleAlpha > 0.5) {
        gfx.print('UI & Tween Demo', titleBar.x + 4, titleBar.y + 3, COL_TITLE);
    }

    // Show bounce indicator next to title
    const indicator = ui.place(titleBar, 1.0, 0.5, 8, 8);
    indicator.x -= 12;
    gfx.rectfill(
        indicator.x,
        indicator.y - math.flr(bounceY),
        indicator.x + 5,
        indicator.y + 5 - math.flr(bounceY),
        COL_HIGHLIGHT,
    );

    // --- Body: sidebar + main area ---
    const sidebarW = math.flr(70 * sidebarT);
    const bodyInset = ui.inset(body, 2);

    if (sidebarW > 4) {
        const bodySplit = ui.hsplit(bodyInset, sidebarW / bodyInset.w, 2);
        const sidebar = bodySplit[0];
        const mainArea = bodySplit[1];

        // --- Sidebar: stat bars ---
        drawPanel(sidebar, COL_PANEL);
        const sbInner = ui.inset(sidebar, 3);

        gfx.print('Stats', sbInner.x, sbInner.y, COL_TITLE);

        const barArea = ui.rect(sbInner.x, sbInner.y + 10, sbInner.w, sbInner.h - 10);
        const rows = ui.vstack(barArea, 3, 3);

        const labels = ['HP', 'MP', 'XP'];
        const colors = [COL_BAR_HP, COL_BAR_MP, COL_BAR_XP];
        for (let i = 0; i < 3; i++) {
            const labelSplit = ui.hsplit(rows[i], 0.25, 1);
            drawLabel(labels[i], labelSplit[0], colors[i]);
            drawBar(labelSplit[1], barValues[i], colors[i]);
        }

        drawMainArea(mainArea);
    } else {
        drawMainArea(bodyInset);
    }

    // --- HUD: bottom info ---
    gfx.print('SPACE: replay  Z: toggle sidebar', 4, 172, COL_LABEL);
}

function drawMainArea(area: { x: number; y: number; w: number; h: number }) {
    drawPanel(area, COL_PANEL);
    const inner = ui.inset(area, 4);

    // Top section: menu items as horizontal stack
    const topBottom = ui.vsplit(inner, 0.35, 4);
    const topSection = topBottom[0];
    const bottomSection = topBottom[1];

    // --- Menu buttons (hstack) ---
    gfx.print('Menu', topSection.x, topSection.y, COL_TITLE);
    const menuArea = ui.rect(topSection.x, topSection.y + 10, topSection.w, topSection.h - 10);
    const slots = ui.vstack(menuArea, menuItems.length, 2);
    for (let i = 0; i < menuItems.length; i++) {
        const slot = slots[i];
        slot.x += math.flr(menuOffsets[i]);
        drawPanel(slot, COL_MENU);
        gfx.print(menuItems[i], slot.x + 3, slot.y + 1, COL_TITLE);
    }

    // --- Bottom section: easing curve visualizer ---
    drawEasingPreview(bottomSection);
}

function drawEasingPreview(area: { x: number; y: number; w: number; h: number }) {
    gfx.print('Easing Curves', area.x, area.y, COL_TITLE);

    const graphArea = ui.rect(area.x, area.y + 10, area.w, area.h - 10);
    const cols = ui.hstack(graphArea, 3, 4);

    const curves = ['easeOutBounce', 'easeOutElastic', 'easeOutBack'];
    const time = (sys.time() % 2.0) / 2.0; // 0..1 over 2 seconds

    for (let c = 0; c < 3; c++) {
        const box = cols[c];
        drawPanel(box, COL_BAR_BG);

        const bInner = ui.inset(box, 2);
        // Label
        const shortName = curves[c].replace('easeOut', '');
        gfx.print(shortName, bInner.x, bInner.y, COL_LABEL);

        // Draw curve
        const plot = ui.rect(bInner.x, bInner.y + 8, bInner.w, bInner.h - 8);
        if (plot.w > 2 && plot.h > 2) {
            for (let px = 0; px < plot.w; px++) {
                const t = px / (plot.w - 1);
                const v = tween.ease(t, curves[c]);
                const py = plot.y + plot.h - 1 - math.flr(v * (plot.h - 1));
                gfx.pset(plot.x + px, py, COL_LABEL);
            }
            // Moving dot
            const ev = tween.ease(time, curves[c]);
            const dotX = plot.x + math.flr(time * (plot.w - 1));
            const dotY = plot.y + plot.h - 1 - math.flr(ev * (plot.h - 1));
            gfx.circfill(dotX, dotY, 2, COL_HIGHLIGHT);
        }
    }
}
