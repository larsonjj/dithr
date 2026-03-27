// UI & Tween Demo
//
// Showcases the ui.* layout API and tween.* animation engine:
//   - ui.anchor, ui.inset, ui.hsplit, ui.vsplit, ui.hstack, ui.vstack, ui.place
//   - tween.start (with Promise chaining), tween.tick, tween.val, tween.ease
//
// Press SPACE to replay the intro animation.
// Press Z to toggle the sidebar open/closed.

// --- Constants -------------------------------------------------------

var COL_BG = 1; // dark blue background
var COL_PANEL = 2; // dark purple panel fill
var COL_BORDER = 5; // dark grey border
var COL_TITLE = 7; // white
var COL_LABEL = 6; // light grey
var COL_BAR_BG = 0; // black
var COL_BAR_HP = 11; // green
var COL_BAR_MP = 12; // blue
var COL_BAR_XP = 10; // yellow
var COL_HIGHLIGHT = 8; // red
var COL_MENU = 13; // purple-ish

// --- State -----------------------------------------------------------

var sidebar_open = true;
var sidebar_tw = null; // tween handle for sidebar slide
var sidebar_t = 1.0; // 0 = closed, 1 = open

var hp = 0.7;
var mp = 0.4;
var xp = 0.55;

var bar_tweens = []; // intro tweens for stat bars
var bar_values = [0, 0, 0];

var menu_items = ["Inventory", "Skills", "Map", "Settings"];
var menu_tweens = []; // staggered menu slide-in tweens
var menu_offsets = []; // current x-offsets per item

var title_tw = null;
var title_alpha = 0; // 0..1 for title fade-in

var bounce_tw = null;
var bounce_y = 0;

var intro_done = false;

// --- Helpers ---------------------------------------------------------

function draw_panel(r, col) {
    gfx.rectfill(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, col);
    gfx.rect(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, COL_BORDER);
}

function draw_bar(r, fill, col) {
    gfx.rectfill(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, COL_BAR_BG);
    if (fill > 0) {
        var fw = math.flr(r.w * math.min(fill, 1));
        if (fw > 0) {
            gfx.rectfill(r.x, r.y, r.x + fw - 1, r.y + r.h - 1, col);
        }
    }
    gfx.rect(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, COL_BORDER);
}

function draw_label(text, r, col) {
    gfx.print(text, r.x + 2, r.y + 1, col);
}

// --- Animation Setup -------------------------------------------------

function start_intro() {
    intro_done = false;

    // Reset values
    sidebar_t = 0;
    bar_values = [0, 0, 0];
    menu_offsets = [80, 80, 80, 80];

    // Sidebar slides in
    sidebar_tw = tween.start(0, 1, 0.5, "easeOutCubic");
    sidebar_tw.done.then(function () {
        sidebar_t = 1;
    });

    // Stat bars fill up (staggered)
    var targets = [hp, mp, xp];
    bar_tweens = [];
    for (var i = 0; i < 3; i++) {
        bar_tweens.push(tween.start(0, targets[i], 0.6, "easeOutQuad", 0.3 + i * 0.15));
    }

    // Menu items slide in from the right (staggered)
    menu_tweens = [];
    for (var j = 0; j < menu_items.length; j++) {
        menu_tweens.push(tween.start(80, 0, 0.4, "easeOutBack", 0.2 + j * 0.1));
    }

    // Title fades in
    title_tw = tween.start(0, 1, 0.8, "easeInOut", 0.1);

    // Bouncing indicator
    bounce_tw = tween.start(0, 6, 0.6, "easeOutBounce", 0.5);
    bounce_tw.done.then(function () {
        // Chain: bounce back
        bounce_tw = tween.start(6, 0, 0.4, "easeInOut");
        bounce_tw.done.then(function () {
            bounce_y = 0;
            intro_done = true;
        });
    });
}

// --- Callbacks -------------------------------------------------------

function _init() {
    start_intro();
}

function _update(dt) {
    tween.tick(dt);

    // Read tween values
    if (sidebar_tw) {
        sidebar_t = tween.val(sidebar_tw, sidebar_t);
    }
    for (var i = 0; i < bar_tweens.length; i++) {
        if (bar_tweens[i]) {
            bar_values[i] = tween.val(bar_tweens[i], bar_values[i]);
        }
    }
    for (var j = 0; j < menu_tweens.length; j++) {
        if (menu_tweens[j]) {
            menu_offsets[j] = tween.val(menu_tweens[j], menu_offsets[j]);
        }
    }
    if (title_tw) {
        title_alpha = tween.val(title_tw, title_alpha);
    }
    if (bounce_tw) {
        bounce_y = tween.val(bounce_tw, bounce_y);
    }

    // Replay intro
    if (key.btnp(key.SPACE)) {
        tween.cancelAll();
        start_intro();
    }

    // Toggle sidebar
    if (key.btnp(key.Z)) {
        sidebar_open = !sidebar_open;
        var from = sidebar_t;
        var to = sidebar_open ? 1 : 0;
        sidebar_tw = tween.start(from, to, 0.3, "easeInOut");
        sidebar_tw.done.then(function () {
            sidebar_t = to;
        });
    }
}

function _draw() {
    gfx.cls(COL_BG);

    // Full screen rect
    var screen = ui.rect(0, 0, 320, 180);

    // --- Title bar (top 14px) ---
    var parts = ui.vsplit(screen, 14 / 180, 0);
    var title_bar = parts[0];
    var body = parts[1];

    // Draw title bar
    draw_panel(title_bar, COL_PANEL);
    if (title_alpha > 0.5) {
        gfx.print("UI & Tween Demo", title_bar.x + 4, title_bar.y + 3, COL_TITLE);
    }

    // Show bounce indicator next to title
    var indicator = ui.place(title_bar, 1.0, 0.5, 8, 8);
    indicator.x -= 12;
    gfx.rectfill(
        indicator.x,
        indicator.y - math.flr(bounce_y),
        indicator.x + 5,
        indicator.y + 5 - math.flr(bounce_y),
        COL_HIGHLIGHT,
    );

    // --- Body: sidebar + main area ---
    var sidebar_w = math.flr(70 * sidebar_t);
    var body_inset = ui.inset(body, 2);

    if (sidebar_w > 4) {
        var body_split = ui.hsplit(body_inset, sidebar_w / body_inset.w, 2);
        var sidebar = body_split[0];
        var main_area = body_split[1];

        // --- Sidebar: stat bars ---
        draw_panel(sidebar, COL_PANEL);
        var sb_inner = ui.inset(sidebar, 3);

        gfx.print("Stats", sb_inner.x, sb_inner.y, COL_TITLE);

        var bar_area = ui.rect(sb_inner.x, sb_inner.y + 10, sb_inner.w, sb_inner.h - 10);
        var rows = ui.vstack(bar_area, 3, 3);

        var labels = ["HP", "MP", "XP"];
        var colors = [COL_BAR_HP, COL_BAR_MP, COL_BAR_XP];
        for (var i = 0; i < 3; i++) {
            var label_split = ui.hsplit(rows[i], 0.25, 1);
            draw_label(labels[i], label_split[0], colors[i]);
            draw_bar(label_split[1], bar_values[i], colors[i]);
        }

        draw_main_area(main_area);
    } else {
        draw_main_area(body_inset);
    }

    // --- HUD: bottom info ---
    gfx.print("SPACE: replay  Z: toggle sidebar", 4, 172, COL_LABEL);
}

function draw_main_area(area) {
    draw_panel(area, COL_PANEL);
    var inner = ui.inset(area, 4);

    // Top section: menu items as horizontal stack
    var top_bottom = ui.vsplit(inner, 0.35, 4);
    var top_section = top_bottom[0];
    var bottom_section = top_bottom[1];

    // --- Menu buttons (hstack) ---
    gfx.print("Menu", top_section.x, top_section.y, COL_TITLE);
    var menu_area = ui.rect(top_section.x, top_section.y + 10, top_section.w, top_section.h - 10);
    var slots = ui.vstack(menu_area, menu_items.length, 2);
    for (var i = 0; i < menu_items.length; i++) {
        var slot = slots[i];
        slot.x += math.flr(menu_offsets[i]);
        draw_panel(slot, COL_MENU);
        gfx.print(menu_items[i], slot.x + 3, slot.y + 1, COL_TITLE);
    }

    // --- Bottom section: easing curve visualizer ---
    draw_easing_preview(bottom_section);
}

function draw_easing_preview(area) {
    gfx.print("Easing Curves", area.x, area.y, COL_TITLE);

    var graph_area = ui.rect(area.x, area.y + 10, area.w, area.h - 10);
    var cols = ui.hstack(graph_area, 3, 4);

    var curves = ["easeOutBounce", "easeOutElastic", "easeOutBack"];
    var time = (sys.time() % 2.0) / 2.0; // 0..1 over 2 seconds

    for (var c = 0; c < 3; c++) {
        var box = cols[c];
        draw_panel(box, COL_BAR_BG);

        var b_inner = ui.inset(box, 2);
        // Label
        var short_name = curves[c].replace("easeOut", "");
        gfx.print(short_name, b_inner.x, b_inner.y, COL_LABEL);

        // Draw curve
        var plot = ui.rect(b_inner.x, b_inner.y + 8, b_inner.w, b_inner.h - 8);
        if (plot.w > 2 && plot.h > 2) {
            for (var px = 0; px < plot.w; px++) {
                var t = px / (plot.w - 1);
                var v = tween.ease(t, curves[c]);
                var py = plot.y + plot.h - 1 - math.flr(v * (plot.h - 1));
                gfx.pset(plot.x + px, py, COL_LABEL);
            }
            // Moving dot
            var ev = tween.ease(time, curves[c]);
            var dot_x = plot.x + math.flr(time * (plot.w - 1));
            var dot_y = plot.y + plot.h - 1 - math.flr(ev * (plot.h - 1));
            gfx.circfill(dot_x, dot_y, 2, COL_HIGHLIGHT);
        }
    }
}
