// PICO-8 port template — compatibility shims for porting PICO-8 games
//
// Provides thin wrappers that map common PICO-8 API names to mvngin
// equivalents. Paste your PICO-8 code below the shim section.

// --- PICO-8 compatibility shims ---

function cls(c) {
    gfx.cls(c || 0);
}
function pset(x, y, c) {
    gfx.pset(x, y, c);
}
function pget(x, y) {
    return gfx.pget(x, y);
}
function line(x0, y0, x1, y1, c) {
    gfx.line(x0, y0, x1, y1, c);
}
function rect(x0, y0, x1, y1, c) {
    gfx.rect(x0, y0, x1, y1, c);
}
function rectfill(x0, y0, x1, y1, c) {
    gfx.rectfill(x0, y0, x1, y1, c);
}
function circ(x, y, r, c) {
    gfx.circ(x, y, r, c);
}
function circfill(x, y, r, c) {
    gfx.circfill(x, y, r, c);
}
function print(s, x, y, c) {
    gfx.print(s, x, y, c);
}
function spr(n, x, y, w, h, fx, fy) {
    gfx.spr(n, x, y, w, h, fx, fy);
}
function sspr(sx, sy, sw, sh, dx, dy, dw, dh, fx, fy) {
    gfx.sspr(sx, sy, sw, sh, dx, dy, dw, dh, fx, fy);
}
function fget(n, f) {
    return gfx.fget(n, f);
}
function fset(n, f, v) {
    gfx.fset(n, f, v);
}
function pal(a, b, p) {
    gfx.pal(a, b, p);
}
function palt(c, t) {
    gfx.palt(c, t);
}
function camera(x, y) {
    gfx.camera(x, y);
}
function clip(x, y, w, h) {
    gfx.clip(x, y, w, h);
}
function fillp(p) {
    gfx.fillp(p);
}
function color(c) {
    gfx.color(c);
}
function cursor(x, y) {
    gfx.cursor(x, y);
}

function mget(x, y) {
    return map.mget(x, y);
}
function mset(x, y, v) {
    map.mset(x, y, v);
}

function btn(i, p) {
    return key.btn(i);
}
function btnp(i, p) {
    return key.btnp(i);
}

function sfx(n, ch, off, len) {
    sfx_api.play(n, ch);
}
function music(n, fade, mask) {
    mus.play(n);
}

function rnd(x) {
    return math.rnd(x);
}
function flr(x) {
    return math.flr(x);
}
function ceil(x) {
    return math.ceil(x);
}
function mid(a, b, c) {
    return math.mid(a, b, c);
}
function abs(x) {
    return math.abs(x);
}
function min(a, b) {
    return math.min(a, b);
}
function max(a, b) {
    return math.max(a, b);
}
function sin(x) {
    return math.sin(x);
}
function cos(x) {
    return math.cos(x);
}
function atan2(dx, dy) {
    return math.atan2(dx, dy);
}
function sgn(x) {
    return math.sign(x);
}
function sqrt(x) {
    return Math.sqrt(x);
}

function t() {
    return sys.time();
}
function time() {
    return sys.time();
}

// --- Paste your PICO-8 code below ---

function _init() {
    cls(0);
    print("pico-8 port ready", 4, 4, 7);
}

function _update(dt) {
    // Your _update code here
}

function _draw() {
    cls(0);
    print("pico-8 port ready", 4, 4, 7);
}
