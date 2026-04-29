# API Reference

dithr exposes **19 namespaces** to cart JavaScript code. All functions are
available as globals on their namespace object (e.g. `gfx.cls()`).

## Quick Start

A minimum viable cart is two files: `cart.json` (manifest) and a JS
entry point.

**`cart.json`**

```jsonc
{
    "title": "My Cart",

    /* Path to the cart's JS entry point, relative to cart.json */
    "code": { "path": "main.js" },

    /* Logical resolution and integer scale for the window */
    "display": { "width": 320, "height": 180, "scale": 3 },
}
```

**`main.js`**

```js
let x = 160;
let y = 90;

function _init() {
    gfx.color(7); /* default to white text */
}

function _update(dt) {
    if (key.btn(key.LEFT)) x -= 60 * dt;
    if (key.btn(key.RIGHT)) x += 60 * dt;
    if (key.btn(key.UP)) y -= 60 * dt;
    if (key.btn(key.DOWN)) y += 60 * dt;
}

function _draw() {
    gfx.cls(0);
    gfx.print("Hello, dithr!", 8, 8);
    gfx.circfill(x | 0, y | 0, 4, 8);
}
```

Run it:

```bash
build/release/Release/dithr path/to/cart.json
```

`examples/hello_world` is the same cart with a `package.json` for
TypeScript builds. Larger examples in `examples/` show audio
(`audio_demo`), tilemaps (`tilemap`), input (`input_demo`), and
post-processing (`postfx_demo`).

---

## Lifecycle Callbacks

The engine calls three optional global callbacks each frame:

| Callback           | Signature                        | Description                                             |
| ------------------ | -------------------------------- | ------------------------------------------------------- |
| `_init()`          | —                                | Called once after the cart is loaded                    |
| `_fixedUpdate(dt)` | `dt`: fixed step size in seconds | Called 0–N times per frame at a fixed tick rate (`ups`) |
| `_update(dt)`      | `dt`: seconds since last frame   | Called every frame before draw                          |
| `_draw()`          | —                                | Called every frame for rendering                        |

`_fixedUpdate(dt)` runs in an accumulator loop. Each frame, the elapsed
time is added to an internal accumulator and the callback fires
repeatedly with a constant `dt` (= 1 / `timing.ups`) until the
accumulator is drained. The accumulator is capped at 8× the step size
to prevent a spiral of death after long pauses. Input state is **not**
updated between fixed ticks; read input in `_update` and apply forces
in `_fixedUpdate`.

### Hot-reload callbacks (DEV_BUILD only)

Two additional optional callbacks support state preservation across hot
reloads:

| Callback          | Signature              | Description                                                                         |
| ----------------- | ---------------------- | ----------------------------------------------------------------------------------- |
| `_save()`         | — → `object` or `void` | Called before teardown. Return a JSON-serializable object to preserve game state    |
| `_restore(state)` | `state`: saved object  | Called after the new code is evaluated with the object returned by the last `_save` |

If `_save()` returns `undefined` (or is not defined), `_restore()` is not
called. The engine also auto-preserves camera, palette, draw state, custom
font, PostFX stack, and draw list across reloads.

```js
function _save() {
    return { px: px, py: py, score: score };
}
function _restore(s) {
    px = s.px;
    py = s.py;
    score = s.score;
}
```

---

## `gfx` — Drawing, Palette, Sprites

### Drawing primitives

| Function   | Parameters                           | Returns | Description                                           |
| ---------- | ------------------------------------ | ------- | ----------------------------------------------------- |
| `cls`      | `col?`                               | —       | Clear the screen. Default colour 0                    |
| `pset`     | `x?, y?, col?`                       | —       | Set a pixel                                           |
| `pget`     | `x?, y?`                             | `int`   | Get the palette index at a pixel                      |
| `line`     | `x0?, y0?, x1?, y1?, col?`           | —       | Draw a line                                           |
| `rect`     | `x0?, y0?, x1?, y1?, col?`           | —       | Draw a rectangle outline                              |
| `rectfill` | `x0?, y0?, x1?, y1?, col?`           | —       | Draw a filled rectangle                               |
| `circ`     | `x?, y?, r?, col?`                   | —       | Circle outline. `r` defaults to 4                     |
| `circfill` | `x?, y?, r?, col?`                   | —       | Filled circle                                         |
| `tri`      | `x0?, y0?, x1?, y1?, x2?, y2?, col?` | —       | Triangle outline                                      |
| `trifill`  | `x0?, y0?, x1?, y1?, x2?, y2?, col?` | —       | Filled triangle                                       |
| `poly`     | `pts, col?`                          | —       | Polygon outline from flat `[x0,y0, x1,y1, ...]` array |
| `polyfill` | `pts, col?`                          | —       | Filled convex polygon (fan-of-triangles)              |

All colour parameters default to the current draw colour (set with
`gfx.color()`). Omitting a colour passes `-1` internally, which resolves to
the active colour.

```js
/* Draw a HUD panel with a border */
gfx.rectfill(0, 0, 80, 16, 1); /* dark-blue fill */
gfx.rect(0, 0, 80, 16, 7); /* white border   */
gfx.print("HP: 100", 2, 5, 7);
```

### Text

| Function     | Parameters                                   | Returns | Description                                                                                                                                                                                                                                                                                        |
| ------------ | -------------------------------------------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `print`      | `text, x?, y?, col?`                         | —       | Print a string. If `x`,`y` are omitted the cursor position is used and advances down by one line                                                                                                                                                                                                   |
| `textWidth`  | `text`                                       | `int`   | Pixel width of a string (widest line if multi-line)                                                                                                                                                                                                                                                |
| `textHeight` | `text`                                       | `int`   | Pixel height of a string (all lines)                                                                                                                                                                                                                                                               |
| `font`       | `sx?, sy?, char_w?, char_h?, first?, count?` | —       | Set a custom monospaced font from a sprite sheet region. Characters are laid out left-to-right in the sheet, wrapping to the next row after `(sheet_width - sx) / char_w` columns. `first` is the ASCII code of the first glyph (default 32 = space). No arguments resets to the built-in 4×6 font |

```js
/* Centre a label on screen */
const msg = "GAME OVER";
const w = gfx.textWidth(msg);
gfx.print(msg, (320 - w) / 2, 87, 7);
```

### Sprites

| Function    | Parameters                                           | Returns | Description                                                                               |
| ----------- | ---------------------------------------------------- | ------- | ----------------------------------------------------------------------------------------- |
| `spr`       | `idx?, x?, y?, w?, h?, flip_x?, flip_y?`             | —       | Draw sprite. `w` and `h` are in tiles (default 1). `flip_x`/`flip_y` mirror the sprite    |
| `sspr`      | `sx?, sy?, sw?, sh?, dx?, dy?, dw?, dh?`             | —       | Stretch-blit a region of the spritesheet. `sw`/`sh` default to 8                          |
| `sprRot`    | `idx?, x?, y?, angle?, cx?, cy?`                     | —       | Draw sprite rotated by `angle` (radians) around pivot (`cx`, `cy`). Pivot −1 = center     |
| `sprAffine` | `idx?, x?, y?, origin_x?, origin_y?, rot_x?, rot_y?` | —       | Draw sprite with 2×2 affine transform. `rot_x`/`rot_y` = basis vectors (default identity) |

```js
/* Draw a 2×1 tile sprite, flipped horizontally */
gfx.spr(8, player.x, player.y, 2, 1, player.facing < 0, false);
```

### Indexed tilemap

| Function  | Parameters                                  | Returns | Description                                                                                       |
| --------- | ------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------- |
| `tilemap` | `tiles, mapW, mapH, colors, tileW?, tileH?` | —       | Render an indexed-colour tilemap. `tiles` and `colors` are flat arrays; tile size defaults to 8×8 |

`tiles` is a flat array of tile indices (one per cell, row-major). `colors`
maps each tile index to a palette colour. Tiles outside the palette range are
skipped.

```js
/* 4×2 tilemap with two tile types */
var tiles = [0, 1, 0, 1, 1, 0, 1, 0];
var colors = [7, 8]; // tile 0 → white, tile 1 → red
gfx.tilemap(tiles, 4, 2, colors);
```

### Sprite flags

| Function | Parameters       | Returns         | Description                                                                                            |
| -------- | ---------------- | --------------- | ------------------------------------------------------------------------------------------------------ |
| `fget`   | `n?, bit?`       | `int` or `bool` | Get sprite flags. Without `bit` returns the full bitmask; with `bit` returns a single bit as a boolean |
| `fset`   | `n?, bit?, val?` | —               | Set a sprite flag bit. `val` defaults to 1                                                             |

### Spritesheet pixel access

| Function      | Parameters               | Returns | Description                              |
| ------------- | ------------------------ | ------- | ---------------------------------------- |
| `sget`        | `x?, y?`                 | `int`   | Get palette index at spritesheet pixel   |
| `sset`        | `x?, y?, col?`           | —       | Set palette index at spritesheet pixel   |
| `sheetW`      |                          | `int`   | Spritesheet width in pixels              |
| `sheetH`      |                          | `int`   | Spritesheet height in pixels             |
| `sheetCreate` | `w?, h?, tileW?, tileH?` | `bool`  | Create empty sheet (default 128×128 8×8) |

```js
/* Create a sheet at runtime if none was loaded */
if (gfx.sheetW() === 0) gfx.sheetCreate(128, 128, 8, 8);

/* Read and modify a pixel on the spritesheet */
var old = gfx.sget(16, 8); // get colour at (16, 8) on sheet
gfx.sset(16, 8, 7); // set it to white
```

### Spritesheet & flag serialization

| Function    | Parameters | Returns  | Description                                                     |
| ----------- | ---------- | -------- | --------------------------------------------------------------- |
| `sheetData` |            | `string` | Hex-encoded string of all spritesheet pixel data                |
| `sheetLoad` | `hex`      | `bool`   | Decode a hex string into the spritesheet pixel buffer           |
| `flagsData` |            | `string` | Hex-encoded string of all sprite flags                          |
| `flagsLoad` | `hex`      | `bool`   | Decode a hex string into the sprite flag array                  |
| `exportPNG` | `path`     | `bool`   | Save the spritesheet as a PNG file (relative to cart directory) |

```js
/* Round-trip the spritesheet through hex */
var hex = gfx.sheetData();
gfx.sheetLoad(hex); // restore

/* Export the sheet to disk */
gfx.exportPNG("sprites.png");
```

### Raw framebuffer access

Direct pixel read/write that bypasses the camera, clipping rectangle and
palette remapping. Addresses run from 0 to `screenW * screenH - 1`, in
row-major order.

| Function | Parameters  | Returns | Description                                       |
| -------- | ----------- | ------- | ------------------------------------------------- |
| `poke`   | `addr, col` | —       | Write palette colour `col` at framebuffer address |
| `peek`   | `addr`      | `int`   | Read palette index at framebuffer address         |

### Palette

| Function | Parameters            | Returns | Description                                                                                       |
| -------- | --------------------- | ------- | ------------------------------------------------------------------------------------------------- |
| `pal`    | `src?, dst?, screen?` | —       | Remap a colour. No arguments resets all remappings. `screen` 0 = draw palette, 1 = screen palette |
| `palt`   | `col?, transparent?`  | —       | Mark a colour as transparent. No arguments resets all                                             |

### State

| Function | Parameters       | Returns | Description                                                             |
| -------- | ---------------- | ------- | ----------------------------------------------------------------------- |
| `camera` | `x?, y?`         | —       | Set the camera offset. Default (0, 0)                                   |
| `clip`   | `x?, y?, w?, h?` | —       | Set the clipping rectangle. No arguments resets to the full framebuffer |
| `fillp`  | `pattern?`       | —       | Set a 4×4 fill pattern as a 16-bit bitmask (default 0 = solid)          |
| `color`  | `col?`           | —       | Set the current draw colour (default 7)                                 |
| `cursor` | `x?, y?`         | —       | Set the text cursor position                                            |

### Screen transitions

Transitions overlay the framebuffer after flip and run for a fixed number of
frames. Only one transition is active at a time — starting a new one replaces
the current.

| Function        | Parameters            | Returns | Description                                                         |
| --------------- | --------------------- | ------- | ------------------------------------------------------------------- |
| `fade`          | `col?, frames?`       | —       | Fade the screen towards palette colour `col` over `frames` (def 30) |
| `wipe`          | `dir?, col?, frames?` | —       | Wipe from one edge. Directions: 0=left, 1=right, 2=up, 3=down       |
| `dissolve`      | `col?, frames?`       | —       | Random pixel dissolve towards `col` over `frames` (def 30)          |
| `transitioning` |                       | `bool`  | Is a transition currently playing?                                  |

### Draw list (sprite batch)

Queue sprite draws and flush them sorted by layer. Lower layers draw first.
Use this when you need depth-sorted rendering (e.g. isometric or top-down
games).

> **Capacity:** Up to 1 024 draw commands per frame. Commands beyond this
> limit are silently dropped.

| Function      | Parameters                                               | Returns | Description                               |
| ------------- | -------------------------------------------------------- | ------- | ----------------------------------------- |
| `dlBegin`     |                                                          | —       | Start recording draw commands             |
| `dlEnd`       |                                                          | —       | Sort by layer, flush all draws, and clear |
| `dlSpr`       | `layer, idx, x, y, w?, h?, flip_x?, flip_y?`             | —       | Queue a sprite draw at the given layer    |
| `dlSspr`      | `layer, sx, sy, sw, sh, dx, dy, dw, dh`                  | —       | Queue a stretch-blit at the given layer   |
| `dlSprRot`    | `layer, idx, x, y, angle?, cx?, cy?`                     | —       | Queue a rotated sprite draw               |
| `dlSprAffine` | `layer, idx, x, y, origin_x?, origin_y?, rot_x?, rot_y?` | —       | Queue an affine sprite draw               |

---

## `res` — Resource Loader

Asynchronous, Promise-based loader for sprites, maps, audio, JSON, and raw
files. All `load*` calls take a string `name` (your handle for the asset)
and a `path` relative to the cart directory. They return a Promise that
settles when decoding completes — typically on the next frame for bundled
assets, or when an HTTP fetch finishes for `dynamic` assets on web.

| Function           | Parameters                 | Returns               | Description                                                   |
| ------------------ | -------------------------- | --------------------- | ------------------------------------------------------------- |
| `loadSprite`       | `name, path`               | `Promise<void>`       | Load a sprite sheet (PNG)                                     |
| `loadMap`          | `name, path`               | `Promise<void>`       | Load a Tiled `.tmj` or LDtk `.ldtk` map                       |
| `loadSfx`          | `name, path`               | `Promise<void>`       | Load a sound effect (wav/ogg)                                 |
| `loadMusic`        | `name, path`               | `Promise<void>`       | Load a music track (ogg/mp3)                                  |
| `loadJson`         | `name, path`               | `Promise<any>`        | Load and parse a JSON file                                    |
| `loadRaw`          | `name, path`               | `Promise<Uint8Array>` | Load arbitrary bytes                                          |
| `loadHex`          | `name, path`               | `Promise<Uint8Array>` | Load a `.hex` palette/flag dump                               |
| `has`              | `name`                     | `bool`                | Is the name registered (may still be loading)?                |
| `isLoaded`         | `name`                     | `bool`                | Sync probe — is decoding complete?                            |
| `handle`           | `name`                     | `int`                 | Stable integer slot for hot-loop fast-path; throws if missing |
| `unload`           | `name`                     | `bool`                | Free a loaded asset                                           |
| `list`             | `kind?`                    | `string[]`            | List loaded names, optionally filtered by kind                |
| `setActiveSheet`   | `name, { tileW?, tileH? }` | —                     | Set the spritesheet used by `gfx.spr*` calls                  |
| `setActiveFlags`   | `name`                     | —                     | Set the active sprite-flag table (loaded via `loadHex`)       |
| `setActivePalette` | `name`                     | —                     | Set the active palette (loaded via `loadHex`)                 |
| `preload`          | `names[]`                  | `Promise<void[]>`     | `Promise.all` shortcut over many loads                        |

`load*` rejects if the file is missing or fails to decode. `_init` may be
declared `async` and `await` these promises — the engine defers
`_update`/`_draw` ticks until the returned promise settles (with a 30 s
slow-load warning).

```js
async function _init() {
    await Promise.all([
        res.loadSprite("sheet", "assets/sprites/sheet.png"),
        res.loadHex("flags", "flags.hex"),
        res.loadMap("level1", "assets/maps/level1.tmj"),
        res.loadSfx("jump", "assets/sfx/jump.wav"),
        res.loadMusic("theme", "assets/music/theme.mp3"),
    ]);
    res.setActiveSheet("sheet", { tileW: 8, tileH: 8 });
    res.setActiveFlags("flags");
    map.use("level1");
    mus.play("theme");
}
```

---

## `map` — Tilemaps

| Function         | Parameters                                | Returns                 | Description                                                                              |
| ---------------- | ----------------------------------------- | ----------------------- | ---------------------------------------------------------------------------------------- |
| `get`            | `cx, cy, layer?, slot?`                   | `int`                   | Tile ID at cell position                                                                 |
| `set`            | `cx, cy, tile, layer?, slot?`             | —                       | Set a tile ID                                                                            |
| `flag`           | `cx, cy, f, slot?`                        | `bool`                  | Test sprite flag `f` on tile at (cx, cy)                                                 |
| `draw`           | `sx, sy, dx, dy, tw?, th?, layer?, slot?` | —                       | Draw a map region. `tw`/`th` default to the full level size                              |
| `width`          | `slot?`                                   | `int`                   | Level width in tiles                                                                     |
| `height`         | `slot?`                                   | `int`                   | Level height in tiles                                                                    |
| `layers`         | `slot?`                                   | `string[]`              | Array of layer names                                                                     |
| `levels`         |                                           | `string[]`              | Array of all loaded level names                                                          |
| `currentLevel`   |                                           | `string`                | Name of the active level                                                                 |
| `load`           | `name`                                    | `bool`                  | Switch active level by name (loaded `res` map or built-in)                               |
| `use`            | `nameOrHandle`                            | `bool`                  | Select active map registered via `res.loadMap()` (string name or `res.handle()` integer) |
| `objects`        | `name?, slot?`                            | `object[]`              | Get map objects, optionally filtered by name                                             |
| `object`         | `name, slot?`                             | `object` or `undefined` | First object matching name                                                               |
| `objectsIn`      | `x, y, w, h, slot?`                       | `object[]`              | AABB query for objects overlapping a rectangle                                           |
| `objectsWith`    | `prop, value?, slot?`                     | `object[]`              | Filter objects by type or custom property                                                |
| `create`         | `w, h, name?`                             | `bool`                  | Create a blank map with one tile layer                                                   |
| `resize`         | `w, h, slot?`                             | `bool`                  | Resize all tile layers (preserves existing data)                                         |
| `addLayer`       | `name?, slot?`                            | `int`                   | Add a tile layer, returns index (-1 on error)                                            |
| `removeLayer`    | `idx, slot?`                              | `bool`                  | Remove a layer (must keep at least one)                                                  |
| `renameLayer`    | `idx, name, slot?`                        | `bool`                  | Rename a layer at the given index                                                        |
| `layerType`      | `idx, slot?`                              | `string`                | Layer type: `"tilelayer"` or `"objectgroup"`                                             |
| `addObjectLayer` | `name?, slot?`                            | `int`                   | Add an object layer, returns index (-1 on error)                                         |
| `layerObjects`   | `idx, slot?`                              | `object[]`              | All objects in a specific layer                                                          |
| `addObject`      | `layerIdx, obj, slot?`                    | `int`                   | Add an object to a layer, returns object index (-1 on error)                             |
| `removeObject`   | `layerIdx, objIdx, slot?`                 | `bool`                  | Remove an object from a layer (shifts remaining objects)                                 |
| `setObject`      | `layerIdx, objIdx, fields, slot?`         | `bool`                  | Update object fields (name, type, x, y, w, h, gid)                                       |
| `data`           | `slot?`                                   | `object` or `null`      | Full map data for serialization (name, layers, tiles)                                    |

**Map object shape:**

```js
{
    name: string,
    type: string,
    x: number, y: number,
    w: number, h: number,
    gid: number,
    props: object
}
```

```js
/* Draw a scrollable map region */
map.draw(cam.get().x, cam.get().y, 0, 0);

/* Read and modify tiles */
const id = map.get(10, 5); // tile at column 10, row 5
map.set(10, 5, 0); // clear it

/* Query objects placed in Tiled */
const spawn = map.object("player_spawn");
if (spawn) {
    player.x = spawn.x;
    player.y = spawn.y;
}
const coins = map.objectsWith("coin");

/* Add an object layer and place objects at runtime */
const li = map.addObjectLayer("runtime_objs");
map.addObject(li, { name: "chest", type: "loot", x: 64, y: 32, w: 16, h: 16 });
const objs = map.layerObjects(li);

/* Update and remove objects */
map.setObject(li, 0, { x: 80, y: 48 });
map.removeObject(li, 0);
```

---

## `key` — Keyboard

| Function | Parameters | Returns  | Description                                                       |
| -------- | ---------- | -------- | ----------------------------------------------------------------- |
| `btn`    | `keycode`  | `bool`   | Is the key currently held?                                        |
| `btnp`   | `keycode`  | `bool`   | Was the key just pressed this frame?                              |
| `btnr`   | `keycode`  | `bool`   | Just pressed **or** auto-repeating (350 ms delay, 40 ms interval) |
| `name`   | `keycode`  | `string` | Human-readable key name                                           |

### Constants

`key.UP` `key.DOWN` `key.LEFT` `key.RIGHT` `key.Z` `key.X` `key.C` `key.V`
`key.SPACE` `key.ENTER` `key.ESCAPE` `key.LSHIFT` `key.RSHIFT`
`key.A` `key.B` `key.D` `key.E` `key.F` `key.G` `key.H` `key.I` `key.J`
`key.K` `key.L` `key.M` `key.N` `key.O` `key.P` `key.Q` `key.R` `key.S`
`key.T` `key.U` `key.W` `key.Y`
`key.NUM0` … `key.NUM9`
`key.F1` … `key.F12`
`key.BACKSPACE` `key.DELETE` `key.TAB` `key.HOME` `key.END`
`key.PAGEUP` `key.PAGEDOWN` `key.LCTRL` `key.RCTRL` `key.LALT` `key.RALT`
`key.LGUI` `key.RGUI` `key.LBRACKET` `key.RBRACKET` `key.MINUS` `key.EQUALS`

```js
/* Move a character with arrow keys */
if (key.btn(key.LEFT)) player.x -= speed;
if (key.btn(key.RIGHT)) player.x += speed;

/* Jump on first press only */
if (key.btnp(key.Z)) player.vy = -4;

/* Text-style repeat for menu navigation */
if (key.btnr(key.DOWN)) menu.next();
```

---

## `mouse` — Mouse

| Function | Parameters | Returns | Description                          |
| -------- | ---------- | ------- | ------------------------------------ |
| `x`      |            | `int`   | Mouse X in framebuffer coordinates   |
| `y`      |            | `int`   | Mouse Y in framebuffer coordinates   |
| `dx`     |            | `int`   | X movement since last frame          |
| `dy`     |            | `int`   | Y movement since last frame          |
| `wheel`  |            | `int`   | Scroll wheel delta                   |
| `btn`    | `button?`  | `bool`  | Is button held? Default `mouse.LEFT` |
| `btnp`   | `button?`  | `bool`  | Was button just pressed?             |
| `show`   |            | —       | Show the system cursor               |
| `hide`   |            | —       | Hide the system cursor               |

### Constants

`mouse.LEFT` (0) &ensp; `mouse.MIDDLE` (1) &ensp; `mouse.RIGHT` (2)

```js
/* Draw a crosshair at the mouse position */
gfx.circ(mouse.x(), mouse.y(), 3, 7);

/* Detect click and drag */
if (mouse.btnp()) {
    drag.startX = mouse.x();
    drag.startY = mouse.y();
}
if (mouse.btn()) {
    gfx.line(drag.startX, drag.startY, mouse.x(), mouse.y(), 12);
}
```

---

## `touch` — Touch Input

Up to 10 simultaneous fingers are tracked. The `index` parameter (0–9)
selects which finger slot to query.

| Function   | Parameters | Returns | Description                                      |
| ---------- | ---------- | ------- | ------------------------------------------------ |
| `count`    |            | `int`   | Number of fingers currently touching             |
| `active`   | `index?`   | `bool`  | Is finger slot active (touching)?                |
| `pressed`  | `index?`   | `bool`  | Did finger touch down this frame?                |
| `released` | `index?`   | `bool`  | Did finger lift this frame?                      |
| `x`        | `index?`   | `float` | Finger X in framebuffer coordinates              |
| `y`        | `index?`   | `float` | Finger Y in framebuffer coordinates              |
| `dx`       | `index?`   | `float` | X movement since last frame (framebuffer pixels) |
| `dy`       | `index?`   | `float` | Y movement since last frame (framebuffer pixels) |
| `pressure` | `index?`   | `float` | Finger pressure 0.0–1.0 (device dependent)       |

### Constants

`touch.MAX_FINGERS` (10)

```js
/* Simple touch-drag movement */
if (touch.active(0)) {
    player.x += touch.dx(0);
    player.y += touch.dy(0);
}
if (touch.pressed(0)) {
    sys.log("finger down at " + touch.x(0) + "," + touch.y(0));
}
```

---

## `pad` — Gamepads

Up to 4 gamepads are supported. The `index` parameter (default 0) selects
which pad to query.

| Function    | Parameters                    | Returns  | Description                                                                      |
| ----------- | ----------------------------- | -------- | -------------------------------------------------------------------------------- |
| `btn`       | `button?, index?`             | `bool`   | Is button held?                                                                  |
| `btnp`      | `button?, index?`             | `bool`   | Was button just pressed?                                                         |
| `axis`      | `axis?, index?`               | `float`  | Axis value from −1.0 to 1.0                                                      |
| `connected` | `index?`                      | `bool`   | Is a gamepad connected at this index?                                            |
| `count`     |                               | `int`    | Number of connected gamepads                                                     |
| `name`      | `index?`                      | `string` | Controller name                                                                  |
| `deadzone`  | `value?, index?`              | `float`  | Get or set the deadzone. If `value` is given, sets it. Returns the current value |
| `rumble`    | `index?, lo?, hi?, duration?` | —        | Trigger rumble. `lo`/`hi` 0–65535, `duration` in ms (default 200)                |

### Button constants

`pad.UP` `pad.DOWN` `pad.LEFT` `pad.RIGHT` `pad.A` `pad.B` `pad.X` `pad.Y`
`pad.L1` `pad.R1` `pad.L2` `pad.R2` `pad.START` `pad.SELECT`

### Axis constants

`pad.LX` `pad.LY` `pad.RX` `pad.RY`

```js
/* Move with the left stick, jump with A */
const lx = pad.axis(pad.LX);
const ly = pad.axis(pad.LY);
player.x += lx * speed;
player.y += ly * speed;

if (pad.btnp(pad.A)) player.vy = -4;

/* Rumble on hit */
pad.rumble(0, 30000, 60000, 150);
```

---

## `input` — Virtual Actions

Map abstract action names to physical bindings. Bindings can come from
`cart.json` or be set at runtime with `input.map()`.

| Function | Parameters         | Returns | Description                                                                    |
| -------- | ------------------ | ------- | ------------------------------------------------------------------------------ |
| `btn`    | `action`           | `bool`  | Is the named action held?                                                      |
| `btnp`   | `action`           | `bool`  | Was the named action just pressed?                                             |
| `axis`   | `action`           | `float` | Analog axis value for the named action                                         |
| `map`    | `action, bindings` | —       | Map an action to an array of binding strings (e.g. `["KEY_LEFT", "PAD_LEFT"]`) |
| `clear`  | `action?`          | —       | Clear one action or all if no argument                                         |

```js
/* Set up bindings and check them */
input.map("jump", ["KEY_Z", "PAD_A"]);
if (input.btnp("jump")) {
    player.vy = -4;
}
```

### Binding type constants

`input.KEY` `input.PAD_BTN` `input.PAD_AXIS` `input.MOUSE_BTN` `input.TOUCH`

---

## `evt` — Event Bus

| Function | Parameters       | Returns | Description                                |
| -------- | ---------------- | ------- | ------------------------------------------ |
| `on`     | `name, callback` | `int`   | Subscribe to an event. Returns a handle    |
| `once`   | `name, callback` | `int`   | Subscribe for one firing, then auto-remove |
| `off`    | `handle`         | —       | Unsubscribe by handle                      |
| `emit`   | `name, payload?` | —       | Queue an event for dispatch at frame end   |

Events are queued by `emit()` and dispatched during the engine's flush at the
end of each frame.

> **Capacity:** Up to 128 handler registrations and 64 queued events per frame.
> Excess handlers return −1 from `on()`/`once()`; excess events are silently
> dropped.

```js
/* Simple event-driven scoring */
evt.on("coin_collected", (e) => {
    score += e.value;
});

evt.emit("coin_collected", { value: 10 });
```

---

## `sfx` — Sound Effects

| Function    | Parameters             | Returns | Description                                                                                                    |
| ----------- | ---------------------- | ------- | -------------------------------------------------------------------------------------------------------------- |
| `play`      | `id, channel?, loops?` | —       | Play a sound. `id` is a `res`-registered name or `res.handle()` integer. `channel` −1 = auto. `loops` 0 = once |
| `stop`      | `channel?`             | —       | Stop a channel (−1 = all)                                                                                      |
| `volume`    | `vol?, channel?`       | —       | Set volume 0.0 – 1.0. `channel` −1 = all                                                                       |
| `getVolume` | `channel?`             | `float` | Get channel volume                                                                                             |
| `playing`   | `channel?`             | `bool`  | Is the channel playing? (−1 = any)                                                                             |

```js
/* Play a jump sound on channel 0 */
sfx.play("jump", 0);

/* Loop an engine hum on channel 3, half volume */
sfx.volume(0.5, 3);
sfx.play("engine", 3, -1);

/* Hot-loop fast path: hoist the lookup out of the inner loop */
const H_BLIP = res.handle("blip");
for (let i = 0; i < 64; ++i) sfx.play(H_BLIP, i & 7);

/* Stop all channels */
sfx.stop(-1);
```

---

## `mus` — Music

| Function    | Parameters                    | Returns | Description                                                                                                                                                      |
| ----------- | ----------------------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `play`      | `id, fade_ms?, channel_mask?` | —       | Play a music track. `id` is a `res`-registered name or `res.handle()` integer. `fade_ms` for crossfade. `channel_mask` selects output channels (default 0 = all) |
| `stop`      | `fade_ms?`                    | —       | Stop music with optional fade-out (default 0 = immediate)                                                                                                        |
| `volume`    | `vol?`                        | —       | Set music volume 0.0 – 1.0                                                                                                                                       |
| `getVolume` |                               | `float` | Get current music volume                                                                                                                                         |
| `playing`   |                               | `bool`  | Is music playing?                                                                                                                                                |

```js
/* Start looping the title theme, then crossfade to gameplay music */
mus.play("title");
mus.volume(0.8);

/* Later, crossfade to another track */
mus.play("gameplay", 500); // 500 ms fade

/* Fade out */
mus.stop(1000);
```

---

## `postfx` — Post-Processing

Effects are managed as a stack applied after `_draw()`.

| Function    | Parameters               | Returns      | Description                                                                    |
| ----------- | ------------------------ | ------------ | ------------------------------------------------------------------------------ |
| `push`      | `effect_id?, intensity?` | —            | Push an effect. `intensity` defaults to 1.0                                    |
| `pop`       |                          | —            | Pop the top effect                                                             |
| `clear`     |                          | —            | Remove all effects                                                             |
| `set`       | `index, param, value`    | —            | Set a parameter on the effect at stack index                                   |
| `use`       | `name`                   | chain object | Activate an effect by name. Returns `{ _idx, set(param, value) }` for chaining |
| `stack`     | `names[]`                | —            | Clear and push an array of effects by name                                     |
| `available` |                          | `string[]`   | List built-in effect names                                                     |
| `save`      |                          | —            | Snapshot the current effect stack                                              |
| `restore`   |                          | —            | Restore a saved snapshot                                                       |

### Effect constants

`postfx.NONE` `postfx.CRT` `postfx.SCANLINES` `postfx.BLOOM`
`postfx.ABERRATION`

### Effect parameters

All effects accept a `strength` parameter (0.0–1.0) controlling intensity.

| Effect       | Behaviour                                                                                           |
| ------------ | --------------------------------------------------------------------------------------------------- |
| `CRT`        | Vignette darkening + colour-channel horizontal shift. Higher strength = more shift and darker edges |
| `SCANLINES`  | Darkens every other row. Factor = 1 − strength × 0.4                                                |
| `BLOOM`      | Brightens luminant pixels. Threshold = 200 − strength × 100; boost = 1 + strength × 0.3             |
| `ABERRATION` | Chromatic aberration. Offset = floor(strength × 2 + 0.5) pixels; R shifts left, B shifts right      |

```js
/* Apply a CRT + scanline stack */
postfx.stack(["CRT", "SCANLINES"]);

/* Fine-tune parameters */
postfx.use("CRT").set("strength", 0.6);

/* Flash aberration on damage */
postfx.push(postfx.ABERRATION, 1.0);
// ... after a few frames:
postfx.pop();
```

---

## `math` — Maths Utilities

### Random number generation (xorshift64)

| Function   | Parameters | Returns | Description                           |
| ---------- | ---------- | ------- | ------------------------------------- |
| `rnd`      | `max?`     | `float` | Random float in [0, max). Default 1.0 |
| `rndInt`   | `max?`     | `int`   | Random int in [0, max)                |
| `rndRange` | `lo?, hi?` | `float` | Random float in [lo, hi)              |
| `seed`     | `s?`       | —       | Seed the PRNG (default 1)             |

### Basic maths

| Function | Parameters  | Returns | Description            |
| -------- | ----------- | ------- | ---------------------- |
| `flr`    | `x`         | `float` | Floor                  |
| `ceil`   | `x`         | `float` | Ceiling                |
| `round`  | `x`         | `float` | Round to nearest       |
| `trunc`  | `x`         | `float` | Truncate toward zero   |
| `abs`    | `x`         | `float` | Absolute value         |
| `sign`   | `x`         | `float` | −1, 0, or 1            |
| `sqrt`   | `x`         | `float` | Square root            |
| `pow`    | `base, exp` | `float` | Exponentiation         |
| `min`    | `a, b`      | `float` | Minimum                |
| `max`    | `a, b`      | `float` | Maximum                |
| `mid`    | `a, b, c`   | `float` | Median of three values |

### Trigonometry

Trig functions use **turn-based conventions**: angles are in the range 0–1 where 1
equals a full rotation. `sin` is **inverted** (returns −sin) so that positive
values move downward in screen coordinates (Y-down).

| Function | Parameters | Returns | Description                      |
| -------- | ---------- | ------- | -------------------------------- |
| `sin`    | `t`        | `float` | Sine (inverted, 0–1 = full turn) |
| `cos`    | `t`        | `float` | Cosine (0–1 = full turn)         |
| `tan`    | `t`        | `float` | Tangent (0–1 = full turn)        |
| `asin`   | `x`        | `float` | Arc sine (radians)               |
| `acos`   | `x`        | `float` | Arc cosine (radians)             |
| `atan`   | `x`        | `float` | Arc tangent (radians)            |
| `atan2`  | `dy, dx`   | `float` | Angle in 0–1 range               |

### Interpolation

| Function     | Parameters      | Returns | Description                    |
| ------------ | --------------- | ------- | ------------------------------ |
| `lerp`       | `a, b, t`       | `float` | Linear interpolation           |
| `unlerp`     | `a, b, v`       | `float` | Inverse lerp: (v−a) / (b−a)    |
| `remap`      | `v, a, b, c, d` | `float` | Remap from [a,b] to [c,d]      |
| `clamp`      | `val, lo, hi`   | `float` | Clamp to [lo, hi]              |
| `smoothstep` | `a, b, t`       | `float` | Hermite smoothstep             |
| `pingpong`   | `t, len`        | `float` | Triangle wave bouncing 0–len–0 |

### Distance and angle

| Function | Parameters       | Returns | Description        |
| -------- | ---------------- | ------- | ------------------ |
| `dist`   | `x1, y1, x2, y2` | `float` | Euclidean distance |
| `angle`  | `x1, y1, x2, y2` | `float` | Angle in radians   |

### Constants

`math.PI` (3.14159…) &ensp; `math.TAU` (6.28318…) &ensp; `math.HUGE`
(1e308)

```js
/* Random spawn position */
const rx = math.rndInt(sys.width());
const ry = math.rndInt(sys.height());

/* Smooth camera follow */
camX = math.lerp(camX, player.x, 0.1);

/* Circular motion (turns-based trig, Y-down) */
const t = sys.time() * 0.5; // half turn per second
const ox = math.cos(t) * 40;
const oy = math.sin(t) * 40; // positive = downward

/* Clamp health */
hp = math.clamp(hp - dmg, 0, maxHp);
```

---

## `cart` — Persistence and Metadata

### Key-value persistence

| Function | Parameters   | Returns                 | Description                  |
| -------- | ------------ | ----------------------- | ---------------------------- |
| `save`   | `key, value` | —                       | Save a string key-value pair |
| `load`   | `key`        | `string` or `undefined` | Load a value by key          |
| `has`    | `key`        | `bool`                  | Check whether a key exists   |
| `delete` | `key`        | —                       | Delete a key                 |

### Numeric data slots

| Function | Parameters    | Returns | Description                    |
| -------- | ------------- | ------- | ------------------------------ |
| `dset`   | `slot, value` | —       | Store a number at a slot index |
| `dget`   | `slot`        | `float` | Read the number from a slot    |

```js
/* Save and restore a high score */
let hi = cart.dget(0) || 0;
if (score > hi) {
    hi = score;
    cart.dset(0, hi);
}
```

### Metadata

| Function  | Parameters | Returns  | Description                 |
| --------- | ---------- | -------- | --------------------------- |
| `title`   |            | `string` | Cart title from `cart.json` |
| `author`  |            | `string` | Cart author                 |
| `version` |            | `string` | Cart version                |

### Asset management

| Function       | Parameters | Returns | Description                                                                                |
| -------------- | ---------- | ------- | ------------------------------------------------------------------------------------------ |
| `reloadAssets` |            | `bool`  | Reload cart assets from disk. Returns `true` on success. Fires `sys:assets_reloaded` event |

---

## `sys` — System

### Timing

| Function       | Parameters | Returns | Description                    |
| -------------- | ---------- | ------- | ------------------------------ |
| `time`         |            | `float` | Seconds since start            |
| `delta`        |            | `float` | Last frame delta in seconds    |
| `frame`        |            | `float` | Total frame count              |
| `fps`          |            | `float` | Current FPS (1 / delta)        |
| `targetFps`    |            | `int`   | Configured target FPS          |
| `setTargetFps` | `fps`      | —       | Set target FPS (clamped 1–240) |
| `fixedDelta`   |            | `float` | Fixed-update step size (1/ups) |
| `targetUps`    |            | `int`   | Configured updates per second  |
| `setTargetUps` | `ups`      | —       | Set UPS (clamped 15–240)       |

### Audio

| Function | Parameters | Returns | Description                                                       |
| -------- | ---------- | ------- | ----------------------------------------------------------------- |
| `volume` | `vol?`     | `float` | Get or set master volume (0.0–1.0). Scales all sfx and music gain |

### Display

| Function | Parameters | Returns | Description        |
| -------- | ---------- | ------- | ------------------ |
| `width`  |            | `int`   | Framebuffer width  |
| `height` |            | `int`   | Framebuffer height |

### Identity

| Function   | Parameters | Returns  | Description                                                |
| ---------- | ---------- | -------- | ---------------------------------------------------------- |
| `version`  |            | `string` | Engine version                                             |
| `platform` |            | `string` | `"web"`, `"windows"`, `"macos"`, `"linux"`, or `"unknown"` |

### Logging

| Function | Parameters | Returns | Description         |
| -------- | ---------- | ------- | ------------------- |
| `log`    | `msg`      | —       | Log an info message |
| `warn`   | `msg`      | —       | Log a warning       |
| `error`  | `msg`      | —       | Log an error        |

### Lifecycle

| Function        | Parameters | Returns | Description                                  |
| --------------- | ---------- | ------- | -------------------------------------------- |
| `pause`         |            | —       | Pause the game loop                          |
| `resume`        |            | —       | Resume from pause                            |
| `quit`          |            | —       | Exit the engine                              |
| `restart`       |            | —       | Restart the cart                             |
| `paused`        |            | `bool`  | Is the game paused?                          |
| `fullscreen`    | `enabled?` | `bool`  | Get or set fullscreen. Returns current state |
| `setFullscreen` | `enabled`  | —       | Set fullscreen mode                          |

### Text Input

| Function       | Parameters | Returns  | Description                                                 |
| -------------- | ---------- | -------- | ----------------------------------------------------------- |
| `textInput`    | `enabled?` | —        | Enable/disable OS text input (triggers `text:input` events) |
| `clipboardGet` |            | `string` | Read the system clipboard                                   |
| `clipboardSet` | `text`     | —        | Write to the system clipboard                               |

### File I/O (sandboxed to cart directory)

| Function    | Parameters      | Returns    | Description                                                    |
| ----------- | --------------- | ---------- | -------------------------------------------------------------- |
| `readFile`  | `path`          | `string`   | Read a text file. Returns `undefined` if not found             |
| `writeFile` | `path, content` | `bool`     | Write a text file. Returns `true` on success                   |
| `listFiles` | `dir?`          | `string[]` | List files in a directory (relative to cart). Defaults to root |
| `listDirs`  | `dir?`          | `string[]` | List subdirectories in a directory (relative to cart)          |

All paths are relative to the cart directory. Absolute paths and `..`
traversal are rejected with a `RangeError`.

### Configuration

| Function | Parameters | Returns                      | Description                                                                                                                                                                                                       |
| -------- | ---------- | ---------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `config` | `path?`    | `any`                        | Read cart config. Dot-separated path (e.g. `"display.width"`). No argument returns the full config object                                                                                                         |
| `limit`  | `key`      | `int`                        | Compile-time limit. Keys: `"fb_width"`, `"fb_height"`, `"palette_size"`, `"max_sprites"`, `"max_maps"`, `"max_map_layers"`, `"max_map_objects"`, `"max_channels"`, `"js_heap_mb"`, `"js_stack_kb"`, `"targetFps"` |
| `stat`   | `n`        | `float`, `string`, or `bool` | System stat query. `0` = memory, `1` = CPU, `7` = FPS, `32` = mouse X, `33` = mouse Y, `34` = mouse button mask, `36` = wheel, `90` = ticks                                                                       |

### Profiling

| Function    | Parameters | Returns                | Description                                                                                                 |
| ----------- | ---------- | ---------------------- | ----------------------------------------------------------------------------------------------------------- |
| `perf`      |            | `object`               | Returns `{cpu, update_ms, draw_ms, fps, frame, markers}`. `markers` is a label→ms map of user perf markers  |
| `perfBegin` | `label`    | —                      | Start timing a named section (up to 16 markers per frame)                                                   |
| `perfEnd`   | `label`    | `float` or `undefined` | Stop timing, returns elapsed ms. Appears in `perf().markers`                                                |
| `drawFps`   |            | —                      | Draw a colour-coded FPS counter with graph in the top-right corner. Green ≥ 90 %, yellow ≥ 50 %, red < 50 % |

```js
/* Show FPS and frame budget */\nconst p = sys.perf();
sys.log("FPS: " + math.flr(p.fps) + "  CPU: " + math.flr(p.cpu * 100) + "%");

/* Measure a custom section */
sys.perfBegin("physics");
updatePhysics();
sys.perfEnd("physics");

sys.perfBegin("ai");
updateEnemies();
const ai_ms = sys.perfEnd("ai");

/* Read all markers */
const m = sys.perf().markers;
sys.log("physics=" + m.physics + " ai=" + m.ai);

/* Toggle fullscreen */
if (key.btnp(key.F)) sys.fullscreen(!sys.fullscreen());

/* Save and load files */
sys.writeFile("save.json", JSON.stringify(state));
const data = sys.readFile("save.json");
```

---

## `col` — Collision Helpers

All functions return a boolean: `true` if the shapes overlap, `false`
otherwise.

| Function    | Parameters                       | Returns | Description                                  |
| ----------- | -------------------------------- | ------- | -------------------------------------------- |
| `rect`      | `x1, y1, w1, h1, x2, y2, w2, h2` | `bool`  | AABB rectangle vs rectangle                  |
| `circ`      | `x1, y1, r1, x2, y2, r2`         | `bool`  | Circle vs circle                             |
| `pointRect` | `px, py, rx, ry, rw, rh`         | `bool`  | Point inside rectangle                       |
| `pointCirc` | `px, py, cx, cy, r`              | `bool`  | Point inside circle                          |
| `circRect`  | `cx, cy, cr, rx, ry, rw, rh`     | `bool`  | Circle vs rectangle (nearest-point clamping) |

```js
/* Check player vs coin overlap */
if (col.rect(player.x, player.y, 8, 8, coin.x, coin.y, 8, 8)) {
    evt.emit("coin_collected", { value: 10 });
}
```

---

## `ui` — UI Layout Helpers

Stateless rectangle-based layout utilities for building menus, HUDs, and
dialogue boxes. Every function takes and returns `{x, y, w, h}` rect objects.

| Function | Parameters             | Returns            | Description                                             |
| -------- | ---------------------- | ------------------ | ------------------------------------------------------- |
| `rect`   | `x, y, w, h`           | `{x, y, w, h}`     | Create a rect from position and size                    |
| `inset`  | `rect, n`              | `{x, y, w, h}`     | Shrink a rect on all four sides by `n` pixels           |
| `anchor` | `ax, ay, w, h`         | `{x, y, w, h}`     | Position a rect using normalised 0–1 screen anchors     |
| `hsplit` | `rect, t?, gap?`       | `[left, right]`    | Split a rect horizontally at fraction `t` (default 0.5) |
| `vsplit` | `rect, t?, gap?`       | `[top, bottom]`    | Split a rect vertically at fraction `t` (default 0.5)   |
| `hstack` | `rect, n, gap?`        | `[{x,y,w,h}, ...]` | Divide a rect into `n` equal columns (max 64)           |
| `vstack` | `rect, n, gap?`        | `[{x,y,w,h}, ...]` | Divide a rect into `n` equal rows (max 64)              |
| `place`  | `parent, ax, ay, w, h` | `{x, y, w, h}`     | Place a child rect within a parent using 0–1 anchors    |

```js
/* Three-column HUD bar along the bottom of the screen */
const bar = ui.anchor(0.5, 1.0, 300, 20);
const cols = ui.hstack(ui.inset(bar, 2), 3, 4);
gfx.rectfill(bar.x, bar.y, bar.x + bar.w, bar.y + bar.h, 1);
gfx.print("HP: 100", cols[0].x, cols[0].y, 7);
gfx.print("MP: 50", cols[1].x, cols[1].y, 12);
gfx.print("LV: 3", cols[2].x, cols[2].y, 10);
```

---

## `tween` — Tweening Engine

Managed value interpolation with easing functions and Promise-based
completion. Call `tween.tick(dt)` once per frame in `_update()` to advance
all active tweens.

### Managed tweens

| Function    | Parameters                     | Returns                    | Description                                                        |
| ----------- | ------------------------------ | -------------------------- | ------------------------------------------------------------------ |
| `start`     | `from, to, dur, ease?, delay?` | `{id: int, done: Promise}` | Start a tween. Returns a handle with `id` and a `done` Promise     |
| `tick`      | `dt`                           | —                          | Advance all tweens by `dt` seconds. Resolves finished Promises     |
| `val`       | `handle_or_id, default?`       | `float`                    | Current interpolated value (or `default` if the tween is inactive) |
| `done`      | `handle_or_id`                 | `bool`                     | `true` if the tween has finished                                   |
| `cancel`    | `handle_or_id`                 | —                          | Cancel a tween. Rejects its Promise with `"tween cancelled"`       |
| `cancelAll` |                                | —                          | Cancel all active tweens                                           |

### Standalone easing

| Function | Parameters | Returns | Description                                |
| -------- | ---------- | ------- | ------------------------------------------ |
| `ease`   | `t, name`  | `float` | Apply a named easing function to `t` (0–1) |

### Easing names

`"linear"`, `"easeIn"`, `"easeOut"`, `"easeInOut"`, `"easeInQuad"`,
`"easeOutQuad"`, `"easeInOutQuad"`, `"easeInCubic"`, `"easeOutCubic"`,
`"easeInOutCubic"`, `"easeOutBack"`, `"easeOutElastic"`, `"easeOutBounce"`

The pool supports up to **64** concurrent tweens.

```js
let fadeIn;

function _init() {
    fadeIn = tween.start(0, 255, 1.0, "outQuad");
    fadeIn.done.then(() => {
        /* animation complete */
    });
}

function _update(dt) {
    tween.tick(dt);
}

function _draw() {
    const alpha = tween.val(fadeIn, 0);
    gfx.cls(0);
    gfx.print("Hello!", 140, 87, 7);
}
```

---

## `cam` — Camera Helpers

Convenience wrappers around `gfx.camera()`. Call `cam.follow()` each frame
in `_update()` for smooth tracking.

| Function | Parameters       | Returns  | Description                                              |
| -------- | ---------------- | -------- | -------------------------------------------------------- |
| `set`    | `x, y`           | —        | Set the camera position (same as `gfx.camera`)           |
| `get`    |                  | `{x, y}` | Return the current camera position as an object          |
| `reset`  |                  | —        | Reset camera to (0, 0)                                   |
| `follow` | `tx, ty, speed?` | —        | Lerp camera toward target. `speed` defaults to 0.1 (0–1) |

```js
function _update(dt) {
    /* Smoothly follow the player, centred on screen */
    cam.follow(player.x - 160, player.y - 90, 0.08);
}
```

---

## `synth` — Procedural Sound Synthesis

Create, edit and play back procedural sound effects at runtime. Each SFX
slot holds a 32-note pattern with per-note waveform, volume and effect
settings. Up to 64 SFX slots and 4 simultaneous playback channels are
supported.

### Definitions

| Function | Parameters                                 | Returns                                             | Description                                                                                                               |
| -------- | ------------------------------------------ | --------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| `set`    | `idx, notes, speed?, loopStart?, loopEnd?` | `bool`                                              | Define an SFX. `notes` is an array of `{pitch, waveform, volume, effect}` objects (max 32). `speed` defaults to 8 (1–255) |
| `get`    | `idx`                                      | `{notes, speed, loopStart, loopEnd}` or `undefined` | Read back the full SFX definition at slot `idx`                                                                           |
| `count`  |                                            | `int`                                               | Number of defined SFX slots (highest index + 1)                                                                           |

### Playback

| Function   | Parameters                                            | Returns | Description                                                                                                                                                                                        |
| ---------- | ----------------------------------------------------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `play`     | `idx, channel?, sampleOffset?`                        | —       | Play SFX `idx` on `channel` (0–3, default 0). If `sampleOffset` > 0 and the same SFX is already playing on that channel, the call is ignored (useful for live-editing without restarting playback) |
| `playNote` | `pitch, waveform, volume?, effect?, speed?, channel?` | —       | Preview a single note. `volume` default 5, `speed` default 8                                                                                                                                       |
| `stop`     | `channel?`                                            | —       | Stop playback. Omit `channel` to stop all channels and note preview                                                                                                                                |
| `playing`  | `channel?`                                            | `bool`  | Is the given channel playing? (default channel 0)                                                                                                                                                  |
| `noteIdx`  | `channel?`                                            | `int`   | Current note index (0–31) or −1 if not playing                                                                                                                                                     |

### Rendering

| Function    | Parameters  | Returns       | Description                                   |
| ----------- | ----------- | ------------- | --------------------------------------------- |
| `render`    | `idx`       | `ArrayBuffer` | Render the full SFX to PCM S16 mono 44 100 Hz |
| `exportWav` | `idx, path` | `bool`        | Write a WAV file (relative to cart directory) |

### Utilities

| Function    | Parameters | Returns    | Description                          |
| ----------- | ---------- | ---------- | ------------------------------------ |
| `noteName`  | `pitch`    | `string`   | Human-readable name, e.g. `"C-4"`    |
| `pitchFreq` | `pitch`    | `number`   | Frequency in Hz for pitch index 0–96 |
| `waveNames` |            | `string[]` | Waveform names (indices 0–7)         |
| `fxNames`   |            | `string[]` | Effect names (indices 0–7)           |

### Waveforms (0–7)

| Index | Name     |
| ----- | -------- |
| 0     | triangle |
| 1     | tiltsaw  |
| 2     | saw      |
| 3     | square   |
| 4     | pulse    |
| 5     | organ    |
| 6     | noise    |
| 7     | phaser   |

### Effects (0–7)

| Index | Name     |
| ----- | -------- |
| 0     | none     |
| 1     | slide    |
| 2     | vibrato  |
| 3     | drop     |
| 4     | fade in  |
| 5     | fade out |
| 6     | arp fast |
| 7     | arp slow |

```js
/* Define a simple laser SFX and play it */
synth.set(0, [
    { pitch: 72, waveform: 2, volume: 7, effect: 3 },
    { pitch: 60, waveform: 2, volume: 5, effect: 0 },
]);
synth.play(0);

/* Preview a single note */
synth.playNote(49, 0, 5); // C-4 triangle
```

---

## System Events

The engine emits the following events on the `evt` bus. Subscribe with
`evt.on("sys:event_name", callback)`.

| Event                 | Payload | Description                                        |
| --------------------- | ------- | -------------------------------------------------- |
| `sys:cart_load`       | —       | Cart has been loaded (or reloaded)                 |
| `sys:cart_unload`     | —       | Cart is about to be torn down                      |
| `sys:quit`            | —       | Engine is shutting down                            |
| `sys:pause`           | —       | Game paused (Escape pressed)                       |
| `sys:resume`          | —       | Game resumed                                       |
| `sys:fullscreen`      | —       | Fullscreen toggled (F11)                           |
| `sys:focus_lost`      | —       | Window lost focus                                  |
| `sys:focus_gained`    | —       | Window gained focus                                |
| `sys:resize`          | —       | Window was resized                                 |
| `sys:assets_reloaded` | —       | Assets were reloaded (F6 or `cart.reloadAssets()`) |
| `text:input`          | string  | Text input character (from `SDL_EVENT_TEXT_INPUT`) |

---

## Dev Keybindings (DEV_BUILD only)

These keybindings are available in development builds:

| Key | Action                                                    |
| --- | --------------------------------------------------------- |
| F4  | Undo last hot reload (revert to previous JS code)         |
| F5  | Force hot reload (re-evaluate JS source from disk)        |
| F6  | Reload assets only (sprites, maps, SFX, music) without JS |
| F11 | Toggle fullscreen                                         |
| Esc | Toggle pause                                              |
