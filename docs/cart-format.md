# Cart Format

A **cart** is a game project consisting of a `cart.json` manifest, a
JavaScript entry point, and any asset files the game needs.

## Directory layout

```
my-game/
├── cart.json               manifest
├── src/
│   └── main.js             entry-point JS file
└── assets/
    ├── sprites/
    │   └── spritesheet.png indexed sprite sheet
    ├── sfx/
    │   ├── jump.wav
    │   └── coin.ogg
    └── music/
        └── theme.mp3
```

## `cart.json` schema

```jsonc
{
    // --- Metadata (optional) ---
    "title": "My Game", // shown in window title
    "author": "Your Name",
    "version": "1.0.0",
    "description": "A short description",
    "id": "com.example.my-game", // SDL app identifier (optional)

    // --- Display ---
    "display": {
        "width": 320, // framebuffer width  (default 320)
        "height": 180, // framebuffer height (default 180)
        "scale": 3, // initial window scale multiplier
        "scaleMode": "integer", // "integer", "letterbox", "stretch", or "overscan"
    },

    // --- Timing ---
    "timing": {
        "fps": 60, // target frames per second (default 60)
        "ups": 60, // fixed updates per second (default 60)
    },

    // --- Sprites ---
    // Default tile size for spr*/map* when no setActiveSheet({tileW,tileH}) is set.
    // Sheets themselves are loaded at runtime via res.loadSprite(name, path).
    "sprites": {
        "tileW": 8, // tile width in pixels  (default 8)
        "tileH": 8, // tile height in pixels (default 8)
    },

    // --- Audio ---
    "audio": {
        "channels": 8, // mixer channel count (default 8, max 16)
    },

    // --- Input mappings ---
    "input": {
        "default_mappings": {
            "left": ["KEY_LEFT", "KEY_A", "PAD_LEFT"],
            "right": ["KEY_RIGHT", "KEY_D", "PAD_RIGHT"],
            "up": ["KEY_UP", "KEY_W", "PAD_UP"],
            "down": ["KEY_DOWN", "KEY_S", "PAD_DOWN"],
            "jump": ["KEY_Z", "KEY_SPACE", "PAD_A"],
            "action": ["KEY_X", "PAD_B"],
        },
    },

    // --- Entry point ---
    "code": "src/main.js",

    // --- Asset packaging ---
    "assets": {
        "include": ["**/*"], // glob(s) of files to ship; default = everything
        "exclude": ["**/*.psd", "**/*.aseprite"], // working files to omit
        "dynamic": ["assets/dlc/**"], // bundled on native, HTTP-fetched on web
        "compress": false, // false | "br" | "gzip" — pre-compress dynamic text files
    },
}
```

### `assets` block

The `assets` block declares which files in the cart directory are
distributed. The same manifest drives the WASM preload bundle, the
desktop export, and which paths are reachable via the [`res.*` API](api-reference.md#res).

#### Resolution rules

For each file under the cart directory:

1. Files matched by **`exclude`** are omitted entirely (from every export).
2. Files not matched by **`include`** are omitted entirely.
3. Files matched by **`dynamic`** are:
    - Bundled into the cart folder on **desktop** export (read via `SDL_LoadFile`).
    - Copied to a separate `dynamic/` directory on **web** export and
      fetched at runtime via `res.load*` (HTTP).
4. Everything else is **bundled** — preloaded into the WASM virtual FS at
   `/cart/`, or copied into `cart/` on desktop.

`exclude` always wins over `dynamic` and `include`. `cart.json` itself is
always bundled regardless of the rules above.

The following paths are **always excluded** (not configurable):
`node_modules/**`, `.git/**`, `build/**`, `.DS_Store`.

#### Glob syntax

Standard minimatch patterns:

| Pattern | Matches                                              |
| ------- | ---------------------------------------------------- |
| `**`    | Any number of path segments (including zero)         |
| `*`     | Any characters except `/`                            |
| `?`     | Any single character except `/`                      |
| Literal | Exact match (regex special chars escaped internally) |

#### `compress`

Controls pre-compression of files in `dynamic` before they ship in a web
export. Has no effect on native exports (desktop reads raw files from disk).

| Value    | Behavior                                                    |
| -------- | ----------------------------------------------------------- |
| `false`  | No pre-compression. Rely on host's `Content-Encoding` only. |
| `"br"`   | Generate `.br` siblings via brotli. Recommended.            |
| `"gzip"` | Generate `.gz` siblings. Wider browser baseline.            |

When enabled, files with already-compressed payloads
(`.png`, `.ogg`, `.mp3`, `.wav`) are skipped. The browser inflates `.br`
or `.gz` siblings via the native `DecompressionStream` API. Brotli
requires Chrome 117+, Firefox 130+, Safari 17.6+ (mid-2024); use
`"gzip"` for older targets.

### Field reference

| Field                    | Type   | Default     | Description                                              |
| ------------------------ | ------ | ----------- | -------------------------------------------------------- |
| `title`                  | string | `""`        | Window title and `cart.title()` return value             |
| `author`                 | string | `""`        | `cart.author()` return value                             |
| `version`                | string | `""`        | `cart.version()` return value                            |
| `description`            | string | `""`        | Not used at runtime                                      |
| `id`                     | string | `""`        | SDL app identifier (reverse-DNS, e.g. `com.example.app`) |
| `display.width`          | int    | 320         | Framebuffer width in pixels                              |
| `display.height`         | int    | 180         | Framebuffer height in pixels                             |
| `display.scale`          | int    | 3           | Initial window = framebuffer × scale                     |
| `display.scaleMode`      | string | `"integer"` | `"integer"`, `"letterbox"`, `"stretch"`, or `"overscan"` |
| `timing.fps`             | int    | 60          | Target frames per second                                 |
| `timing.ups`             | int    | 60          | Fixed updates per second (`_fixedUpdate` tick rate)      |
| `sprites.sheet`          | string | —           | Path to sprite sheet PNG (relative to cart.json)         |
| `sprites.tileW`          | int    | 8           | Sprite tile width                                        |
| `sprites.tileH`          | int    | 8           | Sprite tile height                                       |
| `audio.channels`         | int    | 8           | Mixer channels (max 16)                                  |
| `input.default_mappings` | object | `{}`        | Action name → array of binding strings                   |
| `code`                   | string | —           | Path to the main JS file (relative to cart.json)         |
| `assets.include`         | array  | `["**/*"]`  | Globs of files to ship in exports                        |
| `assets.exclude`         | array  | `[]`        | Globs of files to omit from all exports                  |
| `assets.dynamic`         | array  | `[]`        | Globs of files HTTP-fetched on web (bundled on native)   |
| `assets.compress`        | enum   | `false`     | `false`, `"br"`, or `"gzip"` — pre-compress dynamic text |
| `maps`                   | array  | `[]`        | Tiled map data (see below)                               |

### Binding string format

Each binding string in `default_mappings` has one of these forms:

| Format            | Examples                         | Description    |
| ----------------- | -------------------------------- | -------------- |
| `KEY_<name>`      | `KEY_LEFT`, `KEY_SPACE`, `KEY_Z` | Keyboard key   |
| `PAD_<button>`    | `PAD_A`, `PAD_L1`, `PAD_START`   | Gamepad button |
| `PAD_AXIS_<axis>` | `PAD_AXIS_LX`, `PAD_AXIS_RY`     | Gamepad axis   |
| `MOUSE_<button>`  | `MOUSE_LEFT`, `MOUSE_RIGHT`      | Mouse button   |

## Sprite sheet

The sprite sheet is a standard PNG image. Sprites are indexed left-to-right,
top-to-bottom in tile-sized cells. For example, with 8×8 tiles on a 128×128
sheet, sprite index 0 is the top-left 8×8 tile, index 1 is the next tile to
the right, and so on.

The image is decoded into an indexed pixel buffer using the engine's palette.

## Persistence

Carts can store key-value data via `cart.save()` / `cart.load()` and numeric
slots via `cart.dset()` / `cart.dget()`. Saved data persists across sessions
on native builds.

## Scaffolding a new cart

```bash
npm create @dithrkit my-game
```

Templates: `blank`, `platformer` (in JS or TS). The tool creates the
directory structure, a starter `cart.json`, and a template `src/main.js`
(or `src/main.ts`).
