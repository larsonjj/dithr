// System Explorer — sys.* introspection, file I/O, clipboard, lifecycle
//
// Demonstrates: sys.version, sys.platform, sys.width, sys.height,
//   sys.frame, sys.delta, sys.setTargetFps, sys.targetFps,
//   sys.volume, sys.fullscreen, sys.paused, sys.pause, sys.resume,
//   sys.log, sys.warn, sys.error, sys.config, sys.limit, sys.stat,
//   sys.textInput, sys.clipboardGet, sys.clipboardSet,
//   sys.readFile, sys.writeFile, sys.listFiles, sys.listDirs,
//   key.btnr, key.name, pad.count, pad.deadzone, pad.connected, pad.name

// --- FPS widget ----------------------------------------------------------


// -------------------------------------------------------------------------

let page = 0;
const PAGE_NAMES = ['SYSTEM INFO', 'LIMITS & CONFIG', 'FILE I/O', 'GAMEPAD'];
const NUM_PAGES = PAGE_NAMES.length;

let fileContent = '';
let fileList: any[] = [];
let dirList: any[] = [];
let clipboardText = '';
let textInputEnabled = false;
let typedText = '';
let lastKeyName = '';

// Build list of all key constants for scanning
const ALL_KEYS = [];
const KEY_PROPS = [
    'UP',
    'DOWN',
    'LEFT',
    'RIGHT',
    'Z',
    'X',
    'C',
    'V',
    'SPACE',
    'ENTER',
    'ESCAPE',
    'LSHIFT',
    'RSHIFT',
    'A',
    'B',
    'D',
    'E',
    'F',
    'G',
    'H',
    'I',
    'J',
    'K',
    'L',
    'M',
    'N',
    'O',
    'P',
    'Q',
    'R',
    'S',
    'T',
    'U',
    'W',
    'Y',
    'NUM0',
    'NUM1',
    'NUM2',
    'NUM3',
    'NUM4',
    'NUM5',
    'NUM6',
    'NUM7',
    'NUM8',
    'NUM9',
    'F1',
    'F2',
    'F3',
    'F4',
    'F5',
    'F6',
    'F7',
    'F8',
    'F9',
    'F10',
    'F11',
    'F12',
    'BACKSPACE',
    'DELETE',
    'TAB',
    'HOME',
    'END',
    'PAGEUP',
    'PAGEDOWN',
    'LCTRL',
    'RCTRL',
    'LALT',
    'RALT',
    'GRAVE',
    'SLASH',
    'LGUI',
    'RGUI',
    'LBRACKET',
    'RBRACKET',
    'MINUS',
    'EQUALS',
];
for (let _ki = 0; _ki < KEY_PROPS.length; ++_ki) {
    const _kv = key[KEY_PROPS[_ki]];
    if (_kv !== undefined) ALL_KEYS.push({ code: _kv, name: KEY_PROPS[_ki] });
}

export function _init(): void {
    sys.log('System Explorer started');
    sys.warn('This is a warning message');

    // Write a test file
    sys.writeFile('test_data.txt', 'Hello from sys_explorer!\nLine 2\nLine 3');

    // Subscribe to text input events
    evt.on('text:input', (e) => {
        typedText += e;
    });
}

export function _update(_dt: number): void {

    // Page navigation with auto-repeating keys
    if (key.btnr(key.RIGHT)) {
        page = (page + 1) % NUM_PAGES;
    }
    if (key.btnr(key.LEFT)) {
        page = (page + NUM_PAGES - 1) % NUM_PAGES;
    }

    // Track last pressed key name (scan all known keys)
    for (let ki = 0; ki < ALL_KEYS.length; ++ki) {
        if (key.btnp(ALL_KEYS[ki].code)) {
            lastKeyName = `${ALL_KEYS[ki].name} (${key.name(ALL_KEYS[ki].code)})`;
        }
    }

    // Page-specific controls
    if (page === 0) {
        // +/- to change target FPS
        if (key.btnp(key.EQUALS)) {
            const fps = sys.targetFps();
            sys.setTargetFps(fps + 10);
        }
        if (key.btnp(key.MINUS)) {
            const fps2 = sys.targetFps();
            sys.setTargetFps(math.max(10, fps2 - 10));
        }
        // V to change volume
        if (key.btnp(key.V)) {
            const vol = sys.volume();
            sys.volume((vol + 0.25) % 1.25);
        }
        // F to toggle fullscreen
        if (key.btnp(key.F)) {
            sys.fullscreen(!sys.fullscreen());
        }
    }

    if (page === 2) {
        // R to read file
        if (key.btnp(key.R)) {
            fileContent = sys.readFile('test_data.txt') || '(not found)';
        }
        // L to list files
        if (key.btnp(key.L)) {
            fileList = sys.listFiles() || [];
            dirList = sys.listDirs() || [];
        }
        // W to write a new file with timestamp
        if (key.btnp(key.W)) {
            const data = `Written at frame ${math.flr(sys.frame())}`;
            sys.writeFile('save_test.txt', data);
            sys.log('Wrote save_test.txt');
        }
        // C to read clipboard
        if (key.btnp(key.C)) {
            clipboardText = sys.clipboardGet() || '(empty)';
        }
        // X to write clipboard
        if (key.btnp(key.X)) {
            sys.clipboardSet('dithr sys_explorer clipboard test');
            sys.log('Clipboard set');
        }
        // T to toggle text input
        if (key.btnp(key.T)) {
            textInputEnabled = !textInputEnabled;
            sys.textInput(textInputEnabled);
        }
    }
}

function drawSysInfo() {
    let y = 18;
    const gap = 8;

    gfx.print(`Engine: ${sys.version()}`, 4, y, 7);
    y += gap;
    gfx.print(`Platform: ${sys.platform()}`, 4, y, 7);
    y += gap;
    gfx.print(`Display: ${sys.width()}x${sys.height()}`, 4, y, 7);
    y += gap;
    gfx.print(`Frame: ${math.flr(sys.frame())}`, 4, y, 7);
    y += gap;
    gfx.print(`Delta: ${sys.delta().toFixed(4)}s`, 4, y, 7);
    y += gap;
    gfx.print(`Target FPS: ${sys.targetFps()}  (+/- to change)`, 4, y, 11);
    y += gap;
    gfx.print(`Volume: ${sys.volume().toFixed(2)}  (V to cycle)`, 4, y, 11);
    y += gap;
    gfx.print(`Fullscreen: ${sys.fullscreen()}  (F to toggle)`, 4, y, 11);
    y += gap;
    gfx.print(`Paused: ${sys.paused()}`, 4, y, 7);
    y += gap;
    gfx.print(`Time: ${sys.time().toFixed(1)}s`, 4, y, 7);
    y += gap;
    y += gap;
    gfx.print(`stat(0) mem: ${(sys.stat(0) as number).toFixed(1)}`, 4, y, 6);
    y += gap;
    gfx.print(`stat(1) cpu: ${(sys.stat(1) as number).toFixed(3)}`, 4, y, 6);
    y += gap;
    gfx.print(`stat(7) fps: ${(sys.stat(7) as number).toFixed(1)}`, 4, y, 6);
    y += gap;
    gfx.print(`Last key: ${lastKeyName} (key.name)`, 4, y, 6);
    y += gap;
}

function drawLimits() {
    let y = 18;
    const gap = 8;

    gfx.print('sys.limit() — compile-time limits:', 4, y, 7);
    y += gap + 2;

    const keys = [
        'fb_width',
        'fb_height',
        'palette_size',
        'max_sprites',
        'max_maps',
        'max_map_layers',
        'max_map_objects',
        'max_channels',
        'js_heap_mb',
        'js_stack_kb',
        'targetFps',
    ];
    for (let i = 0; i < keys.length; ++i) {
        const val = sys.limit(keys[i]);
        gfx.print(`${keys[i]}: ${val}`, 10, y, 6);
        y += gap;
    }

    y += gap;
    gfx.print('sys.config() — cart config:', 4, y, 7);
    y += gap + 2;
    gfx.print(`title: ${sys.config('meta.title')}`, 10, y, 6);
    y += gap;
    gfx.print(`display.width: ${sys.config('display.width')}`, 10, y, 6);
    y += gap;
    gfx.print(`timing.fps: ${sys.config('timing.fps')}`, 10, y, 6);
}

function drawFileIo() {
    let y = 18;
    const gap = 8;

    gfx.print('R=read file  W=write file  L=list files', 4, y, 11);
    y += gap;
    gfx.print('C=read clipboard  X=set clipboard  T=text input', 4, y, 11);
    y += gap + 4;

    gfx.print('File content:', 4, y, 7);
    y += gap;
    const lines = fileContent.split('\n');
    for (let i = 0; i < math.min(lines.length, 4); ++i) {
        gfx.print(`  ${lines[i]}`, 4, y, 6);
        y += gap;
    }
    y += 4;

    gfx.print(`Files: ${fileList.join(', ')}`, 4, y, 7);
    y += gap;
    gfx.print(`Dirs: ${dirList.join(', ')}`, 4, y, 7);
    y += gap + 4;

    gfx.print(`Clipboard: ${clipboardText.substring(0, 40)}`, 4, y, 7);
    y += gap + 4;

    gfx.print(`Text input: ${textInputEnabled ? 'ON' : 'OFF'}`, 4, y, textInputEnabled ? 11 : 6);
    y += gap;
    gfx.print(`Typed: ${typedText.substring(typedText.length - 30)}`, 4, y, 7);
}

function drawGamepad() {
    let y = 18;
    const gap = 8;

    const cnt = pad.count();
    gfx.print(`Gamepads connected: ${cnt}`, 4, y, 7);
    y += gap + 4;

    for (let i = 0; i < 4; ++i) {
        const connected = pad.connected(i);
        if (connected) {
            const name = pad.name(i);
            const dz = pad.deadzone(undefined, i);
            gfx.print(`Pad ${i}: ${name}`, 4, y, 11);
            y += gap;
            gfx.print(`  deadzone: ${dz.toFixed(2)}`, 4, y, 6);
            y += gap;
            gfx.print(
                `  LX: ${pad.axis(pad.LX, i).toFixed(2)}  LY: ${pad.axis(pad.LY, i).toFixed(2)}`,
                4,
                y,
                6,
            );
            y += gap;
            gfx.print(
                `  RX: ${pad.axis(pad.RX, i).toFixed(2)}  RY: ${pad.axis(pad.RY, i).toFixed(2)}`,
                4,
                y,
                6,
            );
            y += gap;

            // Button state
            const btns = ['A', 'B', 'X', 'Y', 'L1', 'R1', 'START', 'SELECT'];
            const bvals = [pad.A, pad.B, pad.X, pad.Y, pad.L1, pad.R1, pad.START, pad.SELECT];
            let held = '';
            for (let b = 0; b < btns.length; ++b) {
                if (pad.btn(bvals[b], i)) held += `${btns[b]} `;
            }
            gfx.print(`  Held: ${held || 'none'}`, 4, y, 6);
            y += gap + 4;
        } else {
            gfx.print(`Pad ${i}: not connected`, 4, y, 5);
            y += gap;
        }
    }
}

export function _draw(): void {
    gfx.cls(0);

    // Page header
    gfx.rectfill(0, 0, 319, 10, 1);
    gfx.print(`${PAGE_NAMES[page]} (${page + 1}/${NUM_PAGES})  </>`, 4, 2, 7);

    switch (page) {
        case 0:
            drawSysInfo();
            break;
        case 1:
            drawLimits();
            break;
        case 2:
            drawFileIo();
            break;
        case 3:
            drawGamepad();
            break;
        default:
            break;
    }

    sys.drawFps();
}

export function _save() {
    return {
        page,
    };
}

export function _restore(s: any): void {
    page = s.page;
}
