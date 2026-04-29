# Getting Started

This guide walks you through creating your first dithr game, from scaffolding
a project to exporting it for distribution.

## Prerequisites

- [Node.js](https://nodejs.org/) >= 18

## 1. Create a project

```bash
npm create @dithrkit my-game
cd my-game
npm install
```

This scaffolds a project with the following structure:

```
my-game/
├── cart.json           # Game manifest (display, audio, input bindings)
├── package.json
├── eslint.config.js
├── .prettierrc
├── .editorconfig
├── .gitignore
├── README.md
├── src/
│   └── main.js         # Game entry point
└── assets/
    ├── sprites/
    ├── maps/
    ├── sfx/
    └── music/
```

For TypeScript, add `--ts`:

```bash
npm create @dithrkit my-game --ts
```

You can also start from a template:

```bash
npm create @dithrkit my-game --template platformer       # JS with movement/gravity
npm create @dithrkit my-game --ts --template platformer  # TypeScript variant
```

## 2. Run your game

```bash
npm start
```

This opens a window showing `"hello, dithr!"`. The game reloads automatically
when you save changes to your source files.

For TypeScript projects, `npm start` runs `dithrkit dev` which watches your
`.ts` files, rebuilds via esbuild, and hot-reloads the running game.

### Browser development

To develop in the browser instead of a native window:

```bash
npx dithrkit watch
```

This starts a local WASM dev server at `http://localhost:8080` with live-reload
on save.

## 3. Write your game

Open `src/main.js` (or `src/main.ts`). The engine calls three functions each
frame:

```js
function _init() {
    // Called once when the cart starts
}

function _update(dt) {
    // Called every frame with delta time in seconds
}

function _draw() {
    // Called every frame — draw your game here
    gfx.cls(0);
    gfx.print("hello, dithr!", 4, 4, 7);
}
```

All engine APIs are available as globals — no imports needed:

| Namespace | Purpose                         |
| --------- | ------------------------------- |
| `gfx`     | Drawing (sprites, shapes, text) |
| `map`     | Tilemaps                        |
| `key`     | Keyboard input                  |
| `mouse`   | Mouse input                     |
| `pad`     | Gamepad input                   |
| `input`   | Virtual action bindings         |
| `cam`     | Camera                          |
| `col`     | Collision helpers               |
| `sfx`     | Sound effects                   |
| `mus`     | Music playback                  |
| `synth`   | Procedural sound synthesis      |
| `evt`     | Event bus                       |
| `tween`   | Tweening / easing               |
| `ui`      | Immediate-mode UI               |
| `postfx`  | Post-processing effects         |
| `sys`     | System (timing, logging, files) |
| `math`    | Math utilities                  |
| `cart`    | Cart metadata                   |
| `touch`   | Touch input                     |

See the full [API Reference](api-reference.md) for every function and
constant.

### A minimal example

```js
let x = 160;
let y = 90;
const speed = 60;

function _update(dt) {
    if (key.btn(key.LEFT)) x -= speed * dt;
    if (key.btn(key.RIGHT)) x += speed * dt;
    if (key.btn(key.UP)) y -= speed * dt;
    if (key.btn(key.DOWN)) y += speed * dt;
}

function _draw() {
    gfx.cls(1);
    gfx.rectfill(math.flr(x), math.flr(y), math.flr(x) + 8, math.flr(y) + 8, 11);
    gfx.print("arrow keys to move", 4, 4, 7);
}
```

## 4. Configure your game

Edit `cart.json` to change display settings, audio, and input bindings:

```json
{
    "title": "my-game",
    "display": { "width": 320, "height": 180, "scale": 3 },
    "timing": { "fps": 60 },
    "audio": { "channels": 8 },
    "input": {
        "default_mappings": {
            "left": ["KEY_LEFT", "KEY_A", "PAD_LEFT"],
            "right": ["KEY_RIGHT", "KEY_D", "PAD_RIGHT"],
            "jump": ["KEY_Z", "KEY_SPACE", "PAD_A"]
        }
    },
    "code": "src/main.js"
}
```

With input mappings defined, use `input.btn()` / `input.btnp()` in your code
instead of checking individual keys:

```js
if (input.btnp("jump")) player.vy = -4;
if (input.btn("left")) player.x -= speed * dt;
```

See [Cart Format](cart-format.md) for the full schema.

## 5. Add assets

Carts load assets explicitly via the `res` API in an async `_init`. Place
assets under `assets/` and load them by name:

### Sprites

```js
async function _init() {
    await res.loadSprite("sheet", "assets/sprites/sheet.png");
    res.setActiveSheet("sheet", { tileW: 8, tileH: 8 });
}

function _draw() {
    gfx.spr(0, 10, 10); // sprite 0 at (10, 10)
    gfx.spr(1, 20, 10, 1, 1); // sprite 1, normal size
}
```

### Tilemaps

Export maps from [Tiled](https://www.mapeditor.org/) as `.tmj` (JSON) and
load them by name:

```js
async function _init() {
    await res.loadMap("level", "assets/maps/level.tmj");
    map.use("level");
}

function _draw() {
    map.draw(cam.get().x, cam.get().y, 0, 0);
    const spawn = map.object("player_spawn");
}
```

### Sound effects and music

Place audio files in `assets/sfx/` and `assets/music/`, load them with
named ids, then play by id:

```js
async function _init() {
    await res.loadSfx("jump", "assets/sfx/jump.wav");
    await res.loadSfx("coin", "assets/sfx/coin.wav");
    await res.loadMusic("theme", "assets/music/theme.ogg");
}

function _update() {
    if (input.btnp("jump")) sfx.play("jump");
}

function _start() {
    mus.play("theme");
    mus.volume(0, 0.5); // channel 0 volume
}
```

## 6. Lint and format

The scaffolded project includes ESLint and Prettier:

```bash
npm run lint           # check for issues
npm run lint:fix       # auto-fix issues
npm run format         # auto-format code
npm run format:check   # check formatting without changing files
```

## 7. Export for distribution

### Web (browser)

```bash
npx dithrkit export --web
```

Outputs a browser-ready build to `build/web/`.

### Desktop (standalone executable)

```bash
npx dithrkit export --desktop
```

## Available scripts

| Script              | Description                              |
| ------------------- | ---------------------------------------- |
| `npm start`         | Run the game (JS: `run`, TS: `dev`)      |
| `npm run build`     | Bundle TypeScript (TS projects only)     |
| `npm run lint`      | ESLint check                             |
| `npm run lint:fix`  | ESLint auto-fix                          |
| `npm run format`    | Prettier auto-format                     |
| `npm run typecheck` | TypeScript type check (TS projects only) |

### SDK commands

| Command               | Description                                       |
| --------------------- | ------------------------------------------------- |
| `npx dithrkit run`    | Run natively with hot-reload on save              |
| `npx dithrkit dev`    | Build TS in watch mode + run (native dev loop)    |
| `npx dithrkit build`  | Bundle JS/TS source via esbuild                   |
| `npx dithrkit serve`  | Build and serve in the browser                    |
| `npx dithrkit watch`  | Build, serve, and live-reload on file changes     |
| `npx dithrkit export` | Export for web (`--web`) or desktop (`--desktop`) |

## Next steps

- Browse the [API Reference](api-reference.md) for all drawing, input, audio,
  and system functions
- Study the [examples](https://github.com/dithrkit/dithr/tree/main/examples)
  for working demos of each engine feature
- Read the [Cart Format](cart-format.md) docs for advanced configuration
- See the [CLI Reference](cli.md) for all SDK commands and options
