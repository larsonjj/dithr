// dithr engine type definitions
// Generated from docs/api-reference.md — keep in sync with the C API.

// ---------------------------------------------------------------------------
// Common types
// ---------------------------------------------------------------------------

interface DithrRect {
    x: number;
    y: number;
    w: number;
    h: number;
}

interface DithrMapObject {
    name: string;
    type: string;
    x: number;
    y: number;
    w: number;
    h: number;
    gid: number;
    props: Record<string, unknown>;
}

interface DithrMapLayer {
    name: string;
    type: string;
    data?: number[];
    width?: number;
    height?: number;
    objects?: DithrMapObject[];
}

interface DithrMapData {
    name: string;
    width: number;
    height: number;
    tileW: number;
    tileH: number;
    layers: DithrMapLayer[];
    tiles: number[][];
}

interface DithrSynthNote {
    pitch: number;
    waveform: number;
    volume: number;
    effect: number;
}

interface DithrSynthSfx {
    notes: DithrSynthNote[];
    speed: number;
    loopStart: number;
    loopEnd: number;
}

interface DithrTweenHandle {
    id: number;
    done: Promise<void>;
}

interface DithrPostfxChain {
    _idx: number;
    set(param: string, value: number): DithrPostfxChain;
}

interface DithrPerfResult {
    cpu: number;
    update_ms: number;
    draw_ms: number;
    fps: number;
    frame: number;
    markers: Record<string, number>;
}

// ---------------------------------------------------------------------------
// gfx — Drawing, Palette, Sprites
// ---------------------------------------------------------------------------

interface DithrGfx {
    // Drawing primitives
    cls(col?: number): void;
    pset(x?: number, y?: number, col?: number): void;
    pget(x?: number, y?: number): number;
    line(x0?: number, y0?: number, x1?: number, y1?: number, col?: number): void;
    rect(x0?: number, y0?: number, x1?: number, y1?: number, col?: number): void;
    rectfill(x0?: number, y0?: number, x1?: number, y1?: number, col?: number): void;
    circ(x?: number, y?: number, r?: number, col?: number): void;
    circfill(x?: number, y?: number, r?: number, col?: number): void;
    tri(x0?: number, y0?: number, x1?: number, y1?: number, x2?: number, y2?: number, col?: number): void;
    trifill(x0?: number, y0?: number, x1?: number, y1?: number, x2?: number, y2?: number, col?: number): void;
    poly(pts: number[], col?: number): void;
    polyfill(pts: number[], col?: number): void;

    // Text
    print(text: string | number, x?: number, y?: number, col?: number): void;
    textWidth(text: string): number;
    textHeight(text: string): number;
    font(sx?: number, sy?: number, char_w?: number, char_h?: number, first?: number, count?: number): void;

    // Sprites
    spr(idx?: number, x?: number, y?: number, w?: number, h?: number, flip_x?: boolean, flip_y?: boolean): void;
    sprBatch(data: Int32Array, count?: number): void;
    sspr(sx?: number, sy?: number, sw?: number, sh?: number, dx?: number, dy?: number, dw?: number, dh?: number): void;
    sprRot(idx?: number, x?: number, y?: number, angle?: number, cx?: number, cy?: number): void;
    sprAffine(idx?: number, x?: number, y?: number, origin_x?: number, origin_y?: number, rot_x?: number, rot_y?: number): void;

    // Indexed tilemap
    tilemap(tiles: number[], mapW: number, mapH: number, colors: number[], tileW?: number, tileH?: number): void;

    // Sprite flags
    fget(n: number): number;
    fget(n: number, bit: number): boolean;
    fset(n?: number, bit?: number, val?: number): void;

    // Spritesheet pixel access
    sget(x?: number, y?: number): number;
    sset(x?: number, y?: number, col?: number): void;
    sheetW(): number;
    sheetH(): number;
    sheetCreate(w?: number, h?: number, tileW?: number, tileH?: number): boolean;

    // Spritesheet & flag serialization
    sheetData(): string;
    sheetLoad(hex: string): boolean;
    flagsData(): string;
    flagsLoad(hex: string): boolean;
    exportPNG(path: string): boolean;

    // Raw framebuffer access
    poke(addr: number, col: number): void;
    peek(addr: number): number;

    // Palette
    pal(src?: number, dst?: number, screen?: number): void;
    palt(col?: number, transparent?: boolean): void;

    // State
    camera(x?: number, y?: number): void;
    clip(x?: number, y?: number, w?: number, h?: number): void;
    fillp(pattern?: number): void;
    color(col?: number): void;
    cursor(x?: number, y?: number): void;

    // Screen transitions
    fade(col?: number, frames?: number): void;
    wipe(dir?: number, col?: number, frames?: number): void;
    dissolve(col?: number, frames?: number): void;
    transitioning(): boolean;

    // Draw list (sprite batch)
    dlBegin(): void;
    dlEnd(): void;
    dlSpr(layer: number, idx: number, x: number, y: number, w?: number, h?: number, flip_x?: boolean, flip_y?: boolean): void;
    dlSspr(layer: number, sx: number, sy: number, sw: number, sh: number, dx: number, dy: number, dw: number, dh: number): void;
    dlSprRot(layer: number, idx: number, x: number, y: number, angle?: number, cx?: number, cy?: number): void;
    dlSprAffine(layer: number, idx: number, x: number, y: number, origin_x?: number, origin_y?: number, rot_x?: number, rot_y?: number): void;
}

// ---------------------------------------------------------------------------
// map — Tilemaps
// ---------------------------------------------------------------------------

interface DithrMap {
    get(cx: number, cy: number, layer?: number, slot?: number): number;
    set(cx: number, cy: number, tile: number, layer?: number, slot?: number): void;
    flag(cx: number, cy: number, f: number, slot?: number): boolean;
    draw(sx: number, sy: number, dx: number, dy: number, tw?: number, th?: number, layer?: number, slot?: number): void;
    width(slot?: number): number;
    height(slot?: number): number;
    layers(slot?: number): string[];
    levels(): string[];
    currentLevel(): string;
    load(name: string): boolean;
    objects(name?: string, slot?: number): DithrMapObject[];
    object(name: string, slot?: number): DithrMapObject | undefined;
    objectsIn(x: number, y: number, w: number, h: number, slot?: number): DithrMapObject[];
    objectsWith(prop: string, value?: string, slot?: number): DithrMapObject[];
    create(w: number, h: number, name?: string): boolean;
    resize(w: number, h: number, slot?: number): boolean;
    addLayer(name?: string, slot?: number): number;
    removeLayer(idx: number, slot?: number): boolean;
    renameLayer(idx: number, name: string, slot?: number): boolean;
    layerType(idx: number, slot?: number): string;
    addObjectLayer(name?: string, slot?: number): number;
    layerObjects(idx: number, slot?: number): DithrMapObject[];
    addObject(layerIdx: number, obj: Partial<DithrMapObject>, slot?: number): number;
    removeObject(layerIdx: number, objIdx: number, slot?: number): boolean;
    setObject(layerIdx: number, objIdx: number, fields: Partial<DithrMapObject>, slot?: number): boolean;
    data(slot?: number): DithrMapData | null;
}

// ---------------------------------------------------------------------------
// key — Keyboard
// ---------------------------------------------------------------------------

interface DithrKey {
    btn(keycode: number): boolean;
    btnp(keycode: number): boolean;
    btnr(keycode: number): boolean;
    name(keycode: number): string;

    // Key constants
    readonly UP: number;
    readonly DOWN: number;
    readonly LEFT: number;
    readonly RIGHT: number;
    readonly Z: number;
    readonly X: number;
    readonly C: number;
    readonly V: number;
    readonly SPACE: number;
    readonly ENTER: number;
    readonly ESCAPE: number;
    readonly LSHIFT: number;
    readonly RSHIFT: number;
    readonly A: number;
    readonly B: number;
    readonly D: number;
    readonly E: number;
    readonly F: number;
    readonly G: number;
    readonly H: number;
    readonly I: number;
    readonly J: number;
    readonly K: number;
    readonly L: number;
    readonly M: number;
    readonly N: number;
    readonly O: number;
    readonly P: number;
    readonly Q: number;
    readonly R: number;
    readonly S: number;
    readonly T: number;
    readonly U: number;
    readonly W: number;
    readonly Y: number;
    readonly NUM0: number;
    readonly NUM1: number;
    readonly NUM2: number;
    readonly NUM3: number;
    readonly NUM4: number;
    readonly NUM5: number;
    readonly NUM6: number;
    readonly NUM7: number;
    readonly NUM8: number;
    readonly NUM9: number;
    readonly F1: number;
    readonly F2: number;
    readonly F3: number;
    readonly F4: number;
    readonly F5: number;
    readonly F6: number;
    readonly F7: number;
    readonly F8: number;
    readonly F9: number;
    readonly F10: number;
    readonly F11: number;
    readonly F12: number;
    readonly BACKSPACE: number;
    readonly DELETE: number;
    readonly TAB: number;
    readonly HOME: number;
    readonly END: number;
    readonly PAGEUP: number;
    readonly PAGEDOWN: number;
    readonly LCTRL: number;
    readonly RCTRL: number;
    readonly LALT: number;
    readonly RALT: number;
    readonly LGUI: number;
    readonly RGUI: number;
    readonly LBRACKET: number;
    readonly RBRACKET: number;
    readonly MINUS: number;
    readonly EQUALS: number;
    readonly GRAVE: number;
    readonly SLASH: number;
    readonly BACKSLASH: number;
    readonly SEMICOLON: number;
    readonly APOSTROPHE: number;
    readonly COMMA: number;
    readonly PERIOD: number;
}

// ---------------------------------------------------------------------------
// mouse — Mouse
// ---------------------------------------------------------------------------

interface DithrMouse {
    x(): number;
    y(): number;
    dx(): number;
    dy(): number;
    wheel(): number;
    btn(button?: number): boolean;
    btnp(button?: number): boolean;
    show(): void;
    hide(): void;

    readonly LEFT: number;
    readonly MIDDLE: number;
    readonly RIGHT: number;
}

// ---------------------------------------------------------------------------
// touch — Touch Input
// ---------------------------------------------------------------------------

interface DithrTouch {
    count(): number;
    active(index?: number): boolean;
    pressed(index?: number): boolean;
    released(index?: number): boolean;
    x(index?: number): number;
    y(index?: number): number;
    dx(index?: number): number;
    dy(index?: number): number;
    pressure(index?: number): number;

    readonly MAX_FINGERS: number;
}

// ---------------------------------------------------------------------------
// pad — Gamepads
// ---------------------------------------------------------------------------

interface DithrPad {
    btn(button?: number, index?: number): boolean;
    btnp(button?: number, index?: number): boolean;
    axis(axis?: number, index?: number): number;
    connected(index?: number): boolean;
    count(): number;
    name(index?: number): string;
    deadzone(value?: number, index?: number): number;
    rumble(index?: number, lo?: number, hi?: number, duration?: number): void;

    // Button constants
    readonly UP: number;
    readonly DOWN: number;
    readonly LEFT: number;
    readonly RIGHT: number;
    readonly A: number;
    readonly B: number;
    readonly X: number;
    readonly Y: number;
    readonly L1: number;
    readonly R1: number;
    readonly L2: number;
    readonly R2: number;
    readonly START: number;
    readonly SELECT: number;

    // Axis constants
    readonly LX: number;
    readonly LY: number;
    readonly RX: number;
    readonly RY: number;
    readonly TL: number;
    readonly TR: number;
}

// ---------------------------------------------------------------------------
// input — Virtual Actions
// ---------------------------------------------------------------------------

interface DithrInputBinding {
    type: number;
    code: number;
    threshold?: number;
}

interface DithrInput {
    btn(action: string): boolean;
    btnp(action: string): boolean;
    axis(action: string): number;
    map(action: string, bindings: DithrInputBinding[]): void;
    clear(action?: string): void;

    readonly KEY: number;
    readonly PAD_BTN: number;
    readonly PAD_AXIS: number;
    readonly MOUSE_BTN: number;
    readonly TOUCH: number;
}

// ---------------------------------------------------------------------------
// evt — Event Bus
// ---------------------------------------------------------------------------

interface DithrEvt {
    on(name: string, callback: (payload?: any) => void): number;
    once(name: string, callback: (payload?: any) => void): number;
    off(handle: number): void;
    emit(name: string, payload?: any): void;
}

// ---------------------------------------------------------------------------
// sfx — Sound Effects
// ---------------------------------------------------------------------------

interface DithrSfx {
    play(id?: number, channel?: number, loops?: number): void;
    stop(channel?: number): void;
    volume(vol?: number, channel?: number): void;
    getVolume(channel?: number): number;
    playing(channel?: number): boolean;
}

// ---------------------------------------------------------------------------
// mus — Music
// ---------------------------------------------------------------------------

interface DithrMus {
    play(id?: number, fade_ms?: number, channel_mask?: number): void;
    stop(fade_ms?: number): void;
    volume(vol?: number): void;
    getVolume(): number;
    playing(): boolean;
}

// ---------------------------------------------------------------------------
// postfx — Post-Processing
// ---------------------------------------------------------------------------

interface DithrPostfx {
    push(effect_id?: number, intensity?: number): void;
    pop(): void;
    clear(): void;
    set(index: number, param: string, value: number): void;
    use(name: string): DithrPostfxChain;
    stack(names: string[]): void;
    available(): string[];
    save(): void;
    restore(): void;

    readonly NONE: number;
    readonly CRT: number;
    readonly SCANLINES: number;
    readonly BLOOM: number;
    readonly ABERRATION: number;
}

// ---------------------------------------------------------------------------
// math — Maths Utilities
// ---------------------------------------------------------------------------

interface DithrMath {
    // Random number generation
    rnd(max?: number): number;
    rndInt(max?: number): number;
    rndRange(lo?: number, hi?: number): number;
    seed(s?: number): void;

    // Basic maths
    flr(x: number): number;
    ceil(x: number): number;
    round(x: number): number;
    trunc(x: number): number;
    abs(x: number): number;
    sign(x: number): number;
    sqrt(x: number): number;
    pow(base: number, exp: number): number;
    min(a: number, b: number): number;
    max(a: number, b: number): number;
    mid(a: number, b: number, c: number): number;

    // Trigonometry (turn-based: 0–1 = full rotation)
    sin(t: number): number;
    cos(t: number): number;
    tan(t: number): number;
    asin(x: number): number;
    acos(x: number): number;
    atan(x: number): number;
    atan2(dy: number, dx: number): number;

    // Interpolation
    lerp(a: number, b: number, t: number): number;
    unlerp(a: number, b: number, v: number): number;
    remap(v: number, a: number, b: number, c: number, d: number): number;
    clamp(val: number, lo: number, hi: number): number;
    smoothstep(a: number, b: number, t: number): number;
    pingpong(t: number, len: number): number;

    // Distance and angle
    dist(x1: number, y1: number, x2: number, y2: number): number;
    angle(x1: number, y1: number, x2: number, y2: number): number;

    // Constants
    readonly PI: number;
    readonly TAU: number;
    readonly HUGE: number;
}

// ---------------------------------------------------------------------------
// cart — Persistence and Metadata
// ---------------------------------------------------------------------------

interface DithrCart {
    // Key-value persistence
    save(key: string, value: string): void;
    load(key: string): string | undefined;
    has(key: string): boolean;
    delete(key: string): void;

    // Numeric data slots
    dset(slot: number, value: number): void;
    dget(slot: number): number;

    // Metadata
    title(): string;
    author(): string;
    version(): string;

    // Asset management
    reloadAssets(): boolean;
}

// ---------------------------------------------------------------------------
// sys — System
// ---------------------------------------------------------------------------

interface DithrSys {
    // Timing
    time(): number;
    delta(): number;
    frame(): number;
    fps(): number;
    targetFps(): number;
    setTargetFps(fps: number): void;
    fixedDelta(): number;
    targetUps(): number;
    setTargetUps(ups: number): void;

    // Audio
    volume(vol?: number): number;

    // Display
    width(): number;
    height(): number;

    // Identity
    version(): string;
    platform(): string;

    // Logging
    log(msg: string): void;
    warn(msg: string): void;
    error(msg: string): void;

    // Lifecycle
    pause(): void;
    resume(): void;
    quit(): void;
    restart(): void;
    paused(): boolean;
    fullscreen(enabled?: boolean): boolean;
    setFullscreen(enabled: boolean): void;

    // Text Input
    textInput(enabled?: boolean): void;
    clipboardGet(): string;
    clipboardSet(text: string): void;

    // File I/O (sandboxed to cart directory)
    readFile(path: string): string | undefined;
    writeFile(path: string, content: string): boolean;
    listFiles(dir?: string): string[];
    listDirs(dir?: string): string[];

    // Configuration
    config(path?: string): unknown;
    limit(key: string): number;
    stat(n: number): number | string | boolean;

    // Profiling
    perf(): DithrPerfResult;
    perfBegin(label: string): void;
    perfEnd(label: string): number | undefined;

    // HUD
    drawFps(): void;
}

// ---------------------------------------------------------------------------
// col — Collision Helpers
// ---------------------------------------------------------------------------

interface DithrCol {
    rect(x1: number, y1: number, w1: number, h1: number, x2: number, y2: number, w2: number, h2: number): boolean;
    circ(x1: number, y1: number, r1: number, x2: number, y2: number, r2: number): boolean;
    pointRect(px: number, py: number, rx: number, ry: number, rw: number, rh: number): boolean;
    pointCirc(px: number, py: number, cx: number, cy: number, r: number): boolean;
    circRect(cx: number, cy: number, cr: number, rx: number, ry: number, rw: number, rh: number): boolean;
}

// ---------------------------------------------------------------------------
// ui — UI Layout Helpers
// ---------------------------------------------------------------------------

interface DithrUi {
    rect(x: number, y: number, w: number, h: number): DithrRect;
    inset(rect: DithrRect, n: number): DithrRect;
    anchor(ax: number, ay: number, w: number, h: number): DithrRect;
    hsplit(rect: DithrRect, t?: number, gap?: number): [DithrRect, DithrRect];
    vsplit(rect: DithrRect, t?: number, gap?: number): [DithrRect, DithrRect];
    hstack(rect: DithrRect, n: number, gap?: number): DithrRect[];
    vstack(rect: DithrRect, n: number, gap?: number): DithrRect[];
    place(parent: DithrRect, ax: number, ay: number, w: number, h: number): DithrRect;
}

// ---------------------------------------------------------------------------
// tween — Tweening Engine
// ---------------------------------------------------------------------------

interface DithrTween {
    start(from: number, to: number, dur: number, ease?: string, delay?: number): DithrTweenHandle;
    tick(dt: number): void;
    val(handle_or_id: DithrTweenHandle | number, defaultVal?: number): number;
    done(handle_or_id: DithrTweenHandle | number): boolean;
    cancel(handle_or_id: DithrTweenHandle | number): void;
    cancelAll(): void;
    ease(t: number, name: string): number;
}

// ---------------------------------------------------------------------------
// cam — Camera Helpers
// ---------------------------------------------------------------------------

interface DithrCam {
    set(x: number, y: number): void;
    get(): { x: number; y: number };
    reset(): void;
    follow(tx: number, ty: number, speed?: number): void;
}

// ---------------------------------------------------------------------------
// synth — Procedural Sound Synthesis
// ---------------------------------------------------------------------------

interface DithrSynth {
    // Definitions
    set(idx: number, notes: DithrSynthNote[], speed?: number, loopStart?: number, loopEnd?: number): boolean;
    get(idx: number): DithrSynthSfx | undefined;
    count(): number;

    // Playback
    play(idx: number, channel?: number, sampleOffset?: number): void;
    playNote(pitch: number, waveform: number, volume?: number, effect?: number, speed?: number, channel?: number): void;
    stop(channel?: number): void;
    playing(channel?: number): boolean;
    noteIdx(channel?: number): number;

    // Rendering
    render(idx: number): ArrayBuffer;
    exportWav(idx: number, path: string): boolean;

    // Utilities
    noteName(pitch: number): string;
    pitchFreq(pitch: number): number;
    waveNames(): string[];
    fxNames(): string[];
}

// ---------------------------------------------------------------------------
// Global declarations
// ---------------------------------------------------------------------------

declare const gfx: DithrGfx;
declare const map: DithrMap;
declare const key: DithrKey;
declare const mouse: DithrMouse;
declare const touch: DithrTouch;
declare const pad: DithrPad;
declare const input: DithrInput;
declare const evt: DithrEvt;
declare const sfx: DithrSfx;
declare const mus: DithrMus;
declare const postfx: DithrPostfx;
declare const math: DithrMath;
declare const cart: DithrCart;
declare const sys: DithrSys;
declare const col: DithrCol;
declare const ui: DithrUi;
declare const tween: DithrTween;
declare const cam: DithrCam;
declare const synth: DithrSynth;

// ---------------------------------------------------------------------------
// Global callbacks (implement these in your cart)
// ---------------------------------------------------------------------------

declare function _init(): void;
declare function _update(dt: number): void;
declare function _draw(): void;
declare function _fixedUpdate(dt: number): void;
declare function _save(): unknown;
declare function _restore(state: unknown): void;
