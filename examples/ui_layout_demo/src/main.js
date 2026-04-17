// UI Layout Demo — stateless layout helpers
//
// Demonstrates ui.rect(), ui.hsplit(), ui.vsplit(), ui.hstack(),
// ui.vstack(), ui.anchor(), ui.place(), and ui.inset().

const PANEL_COLORS = [1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14];

function drawRect(r, col, label) {
    gfx.rectfill(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, col);
    gfx.rect(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, 7);
    if (label && r.w > 8 && r.h > 8) {
        gfx.print(label, r.x + 2, r.y + 2, 7);
    }
}

let page = 0;
const PAGE_COUNT = 4;

function _init() {
    gfx.cls(0);
}

function _update() {
    if (key.btnp(key.RIGHT) || key.btnp(key.D)) {
        page = (page + 1) % PAGE_COUNT;
    }
    if (key.btnp(key.LEFT) || key.btnp(key.A)) {
        page = (page - 1 + PAGE_COUNT) % PAGE_COUNT;
    }
}

function _draw() {
    gfx.cls(0);

    gfx.print(`UI Layout Demo  [${page + 1}/${PAGE_COUNT}]  < >`, 4, 4, 7);

    if (page === 0) {
        drawPageSplit();
    } else if (page === 1) {
        drawPageStack();
    } else if (page === 2) {
        drawPageAnchorPlace();
    } else {
        drawPageInset();
    }

    sys.drawFps();
}

// Page 1: hsplit / vsplit
function drawPageSplit() {
    gfx.print('ui.hsplit + ui.vsplit', 4, 14, 6);

    const root = ui.rect(4, 26, 312, 148);
    const [left, right] = ui.hsplit(root, 0.4, 2);

    drawRect(left, 1, 'left (40%)');

    const [top, bot] = ui.vsplit(right, 0.5, 2);
    drawRect(top, 2, 'top-right (50%)');
    drawRect(bot, 3, 'bot-right (50%)');
}

// Page 2: hstack / vstack
function drawPageStack() {
    gfx.print('ui.hstack + ui.vstack', 4, 14, 6);

    const topArea = ui.rect(4, 26, 312, 60);
    const cols = ui.hstack(topArea, 5, 2);
    for (let i = 0; i < cols.length; i += 1) {
        drawRect(cols[i], PANEL_COLORS[i], `${i + 1}`);
    }

    const botArea = ui.rect(4, 92, 312, 82);
    const rows = ui.vstack(botArea, 4, 2);
    for (let i = 0; i < rows.length; i += 1) {
        drawRect(rows[i], PANEL_COLORS[i + 5], `row ${i + 1}`);
    }
}

// Page 3: anchor / place
function drawPageAnchorPlace() {
    gfx.print('ui.anchor + ui.place', 4, 14, 6);

    // Anchor: place a rect relative to the screen
    const centered = ui.anchor(0.5, 0.5, 100, 40);
    drawRect(centered, 9, 'anchor(0.5,0.5)');

    const topRight = ui.anchor(1.0, 0.0, 80, 24);
    drawRect(topRight, 10, 'anchor(1,0)');

    const botLeft = ui.anchor(0.0, 1.0, 80, 24);
    drawRect(botLeft, 11, 'anchor(0,1)');

    // Place: position inside a parent rect
    const parent = ui.rect(20, 30, 160, 80);
    drawRect(parent, 1, '');
    gfx.print('parent', 22, 32, 6);

    const child = ui.place(parent, 0.5, 0.5, 60, 30);
    drawRect(child, 12, 'place');
}

// Page 4: inset
function drawPageInset() {
    gfx.print('ui.inset', 4, 14, 6);

    const outer = ui.rect(20, 30, 280, 140);
    drawRect(outer, 1, 'outer');

    const mid = ui.inset(outer, 10);
    drawRect(mid, 2, 'inset(10)');

    const inner = ui.inset(mid, 15);
    drawRect(inner, 3, 'inset(15)');

    const core = ui.inset(inner, 10);
    drawRect(core, 4, 'inset(10)');
}
