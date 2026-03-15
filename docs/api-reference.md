# API Reference

mvngin exposes **13 namespaces** to cart JavaScript code. All functions are
available as globals on their namespace object (e.g. `gfx.cls()`).

The engine calls three optional global callbacks each frame:

| Callback      | Signature                      | Description                          |
| ------------- | ------------------------------ | ------------------------------------ |
| `_init()`     | —                              | Called once after the cart is loaded |
| `_update(dt)` | `dt`: seconds since last frame | Called every frame before draw       |
| `_draw()`     | —                              | Called every frame for rendering     |

---

## `gfx` — Drawing, Palette, Sprites

### Drawing primitives

| Function   | Parameters                           | Returns | Description                        |
| ---------- | ------------------------------------ | ------- | ---------------------------------- |
| `cls`      | `col?`                               | —       | Clear the screen. Default colour 0 |
| `pset`     | `x?, y?, col?`                       | —       | Set a pixel                        |
| `pget`     | `x?, y?`                             | `int`   | Get the palette index at a pixel   |
| `line`     | `x0?, y0?, x1?, y1?, col?`           | —       | Draw a line                        |
| `rect`     | `x0?, y0?, x1?, y1?, col?`           | —       | Draw a rectangle outline           |
| `rectfill` | `x0?, y0?, x1?, y1?, col?`           | —       | Draw a filled rectangle            |
| `circ`     | `x?, y?, r?, col?`                   | —       | Circle outline. `r` defaults to 4  |
| `circfill` | `x?, y?, r?, col?`                   | —       | Filled circle                      |
| `tri`      | `x0?, y0?, x1?, y1?, x2?, y2?, col?` | —       | Triangle outline                   |
| `trifill`  | `x0?, y0?, x1?, y1?, x2?, y2?, col?` | —       | Filled triangle                    |

All colour parameters default to the current draw colour (set with
`gfx.color()`). Omitting a colour passes `-1` internally, which resolves to
the active colour.

### Text

| Function | Parameters           | Returns | Description                                                                                      |
| -------- | -------------------- | ------- | ------------------------------------------------------------------------------------------------ |
| `print`  | `text, x?, y?, col?` | —       | Print a string. If `x`,`y` are omitted the cursor position is used and advances down by one line |

### Sprites

| Function | Parameters                               | Returns | Description                                                                            |
| -------- | ---------------------------------------- | ------- | -------------------------------------------------------------------------------------- |
| `spr`    | `idx?, x?, y?, w?, h?, flip_x?, flip_y?` | —       | Draw sprite. `w` and `h` are in tiles (default 1). `flip_x`/`flip_y` mirror the sprite |
| `sspr`   | `sx?, sy?, sw?, sh?, dx?, dy?, dw?, dh?` | —       | Stretch-blit a region of the spritesheet. `sw`/`sh` default to 8                       |

### Sprite flags

| Function | Parameters       | Returns         | Description                                                                                            |
| -------- | ---------------- | --------------- | ------------------------------------------------------------------------------------------------------ |
| `fget`   | `n?, bit?`       | `int` or `bool` | Get sprite flags. Without `bit` returns the full bitmask; with `bit` returns a single bit as a boolean |
| `fset`   | `n?, bit?, val?` | —               | Set a sprite flag bit. `val` defaults to 1                                                             |

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

---

## `map` — Tilemaps

| Function        | Parameters                                | Returns                 | Description                                                 |
| --------------- | ----------------------------------------- | ----------------------- | ----------------------------------------------------------- |
| `get`           | `cx, cy, layer?, slot?`                   | `int`                   | Tile ID at cell position                                    |
| `set`           | `cx, cy, tile, layer?, slot?`             | —                       | Set a tile ID                                               |
| `flag`          | `cx, cy, f, slot?`                        | `bool`                  | Test sprite flag `f` on tile at (cx, cy)                    |
| `draw`          | `sx, sy, dx, dy, tw?, th?, layer?, slot?` | —                       | Draw a map region. `tw`/`th` default to the full level size |
| `width`         | `slot?`                                   | `int`                   | Level width in tiles                                        |
| `height`        | `slot?`                                   | `int`                   | Level height in tiles                                       |
| `layers`        | `slot?`                                   | `string[]`              | Array of layer names                                        |
| `levels`        |                                           | `string[]`              | Array of all loaded level names                             |
| `current_level` |                                           | `string`                | Name of the active level                                    |
| `load`          | `name`                                    | `bool`                  | Switch active level by name                                 |
| `objects`       | `name?, slot?`                            | `object[]`              | Get map objects, optionally filtered by name                |
| `object`        | `name, slot?`                             | `object` or `undefined` | First object matching name                                  |
| `objects_in`    | `x, y, w, h, slot?`                       | `object[]`              | AABB query for objects overlapping a rectangle              |
| `objects_with`  | `prop, value?, slot?`                     | `object[]`              | Filter objects by type or custom property                   |

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

---

## `key` — Keyboard

| Function | Parameters | Returns  | Description                          |
| -------- | ---------- | -------- | ------------------------------------ |
| `btn`    | `keycode`  | `bool`   | Is the key currently held?           |
| `btnp`   | `keycode`  | `bool`   | Was the key just pressed this frame? |
| `name`   | `keycode`  | `string` | Human-readable key name              |

### Constants

`key.UP` `key.DOWN` `key.LEFT` `key.RIGHT` `key.Z` `key.X` `key.C` `key.V`
`key.SPACE` `key.ENTER` `key.ESCAPE` `key.LSHIFT` `key.RSHIFT` `key.A`
`key.B` `key.D` `key.E` `key.F` `key.W` `key.F1` `key.F2`

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

### Binding type constants

`input.KEY` `input.PAD_BTN` `input.PAD_AXIS` `input.MOUSE_BTN`

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

---

## `sfx` — Sound Effects

| Function    | Parameters                       | Returns | Description                                         |
| ----------- | -------------------------------- | ------- | --------------------------------------------------- |
| `play`      | `id?, channel?, offset?, loops?` | —       | Play a sound. `channel` −1 = auto. `loops` 0 = once |
| `stop`      | `channel?`                       | —       | Stop a channel (−1 = all)                           |
| `volume`    | `vol?, channel?`                 | —       | Set volume 0.0 – 1.0. `channel` −1 = all            |
| `getVolume` | `channel?`                       | `float` | Get channel volume                                  |
| `playing`   | `channel?`                       | `bool`  | Is the channel playing? (−1 = any)                  |

---

## `mus` — Music

| Function    | Parameters              | Returns | Description                                                       |
| ----------- | ----------------------- | ------- | ----------------------------------------------------------------- |
| `play`      | `id?, loops?, fade_ms?` | —       | Play a music track. `loops` 0 = infinite. `fade_ms` for crossfade |
| `stop`      | `fade_ms?`              | —       | Stop music with optional fade-out                                 |
| `volume`    | `vol?`                  | —       | Set music volume 0.0 – 1.0                                        |
| `getVolume` |                         | `float` | Get current music volume                                          |
| `playing`   |                         | `bool`  | Is music playing?                                                 |

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

---

## `math` — Maths Utilities

### Random number generation (xorshift64)

| Function    | Parameters | Returns | Description                           |
| ----------- | ---------- | ------- | ------------------------------------- |
| `rnd`       | `max?`     | `float` | Random float in [0, max). Default 1.0 |
| `rnd_int`   | `max?`     | `int`   | Random int in [0, max)                |
| `rnd_range` | `lo?, hi?` | `float` | Random float in [lo, hi)              |
| `seed`      | `s?`       | —       | Seed the PRNG (default 1)             |

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

Trig functions use **PICO-8 conventions**: angles are in the range 0–1 where 1
equals a full rotation. `sin` is **inverted** (returns −sin) to match PICO-8
screen coordinates (Y-down).

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

### Metadata

| Function  | Parameters | Returns  | Description                 |
| --------- | ---------- | -------- | --------------------------- |
| `title`   |            | `string` | Cart title from `cart.json` |
| `author`  |            | `string` | Cart author                 |
| `version` |            | `string` | Cart version                |

---

## `sys` — System

### Timing

| Function         | Parameters | Returns | Description                    |
| ---------------- | ---------- | ------- | ------------------------------ |
| `time`           |            | `float` | Seconds since start            |
| `delta`          |            | `float` | Last frame delta in seconds    |
| `frame`          |            | `float` | Total frame count              |
| `fps`            |            | `float` | Current FPS (1 / delta)        |
| `target_fps`     |            | `int`   | Configured target FPS          |
| `set_target_fps` | `fps`      | —       | Set target FPS (clamped 1–240) |

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

| Function         | Parameters | Returns | Description                                  |
| ---------------- | ---------- | ------- | -------------------------------------------- |
| `pause`          |            | —       | Pause the game loop                          |
| `resume`         |            | —       | Resume from pause                            |
| `quit`           |            | —       | Exit the engine                              |
| `restart`        |            | —       | Restart the cart                             |
| `paused`         |            | `bool`  | Is the game paused?                          |
| `fullscreen`     | `enabled?` | `bool`  | Get or set fullscreen. Returns current state |
| `set_fullscreen` | `enabled`  | —       | Set fullscreen mode                          |

### Configuration

| Function | Parameters | Returns                      | Description                                                                                                                                                                                                        |
| -------- | ---------- | ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `config` | `path?`    | `any`                        | Read cart config. Dot-separated path (e.g. `"display.width"`). No argument returns the full config object                                                                                                          |
| `limit`  | `key`      | `int`                        | Compile-time limit. Keys: `"fb_width"`, `"fb_height"`, `"palette_size"`, `"max_sprites"`, `"max_maps"`, `"max_map_layers"`, `"max_map_objects"`, `"max_channels"`, `"js_heap_mb"`, `"js_stack_kb"`, `"target_fps"` |
| `stat`   | `n`        | `float`, `string`, or `bool` | PICO-8 compatible stat. `0` = memory, `1` = CPU, `7` = FPS, `32` = mouse X, `33` = mouse Y, `34` = mouse button mask, `36` = wheel, `90` = ticks                                                                   |
