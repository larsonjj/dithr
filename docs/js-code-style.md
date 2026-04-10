# JavaScript Code Style

This document describes the JavaScript coding conventions for dithr cart source
files (`src/**/*.js`). The style is enforced by **ESLint** (Airbnb base) and
**Prettier**, both of which are scaffolded automatically by
`tools/create-cart.js`.

## Table of Contents

- [JavaScript Code Style](#javascript-code-style)
    - [Table of Contents](#table-of-contents)
    - [Quick Start](#quick-start)
    - [Prettier](#prettier)
    - [EditorConfig](#editorconfig)
    - [ESLint](#eslint)
        - [Base configuration](#base-configuration)
        - [Engine globals](#engine-globals)
        - [Rule overrides](#rule-overrides)
        - [Editor / multi-file carts](#editor--multi-file-carts)
    - [General Rules](#general-rules)
    - [Variables](#variables)
    - [Functions](#functions)
    - [Strings](#strings)
    - [Naming](#naming)
    - [Common Patterns](#common-patterns)
    - [Running the Tools](#running-the-tools)

## Quick Start

Every cart scaffolded with `create-cart.js` ships with a ready-to-use linting
and formatting setup. Install dev-dependencies then run:

```bash
npm install --legacy-peer-deps
npm run lint        # check for errors
npm run lint:fix    # auto-fix what ESLint can
npm run format      # format with Prettier
```

> **Note:** `--legacy-peer-deps` is needed because `eslint-config-airbnb-base`
> v15 declares a peer dependency on ESLint 7–8, while the project uses
> ESLint 9.

## Prettier

Formatting is handled entirely by Prettier so that ESLint only reports logical
errors, never whitespace or style issues. The `.prettierrc` shipped with every
cart:

```json
{
    "printWidth": 100,
    "tabWidth": 4,
    "useTabs": false,
    "semi": true,
    "singleQuote": true,
    "trailingComma": "all",
    "bracketSpacing": true
}
```

| Option          | Value  | Rationale                                        |
| --------------- | ------ | ------------------------------------------------ |
| `printWidth`    | `100`  | Matches the C code-style column limit            |
| `tabWidth`      | `4`    | Matches the C code-style indent width            |
| `singleQuote`   | `true` | Shorter, consistent with engine identifier style |
| `trailingComma` | `all`  | Cleaner diffs when adding items to lists         |
| `semi`          | `true` | Explicit statement terminators                   |

## EditorConfig

Each cart includes an `.editorconfig` that mirrors the Prettier settings so
editors without Prettier integration still produce consistent files:

```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
indent_style = space
indent_size = 4
```

## ESLint

### Base configuration

Carts use an ESLint v9 **flat config** (`eslint.config.js`) built from three
layers:

1. `@eslint/js` recommended rules
2. `airbnb-base` (via `@eslint/eslintrc` FlatCompat)
3. `eslint-config-prettier` (disables formatting rules that conflict with
   Prettier)

The language level is **ES2020** (`ecmaVersion: 2020`), matching the
QuickJS-NG runtime.

### Engine globals

All 18 dithr API namespaces are declared as `"readonly"` globals so ESLint
does not flag them as undefined:

```
cam  cart  col  evt  gfx  input  key  map  math  mouse
mus  pad  postfx  sfx  synth  sys  tween  ui
```

### Rule overrides

The following Airbnb rules are relaxed or customised for game code:

| Rule                           | Setting                                                                 | Why                                                                                      |
| ------------------------------ | ----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `no-underscore-dangle`         | `off`                                                                   | Engine callbacks are `_init`, `_update`, `_draw`, etc.                                   |
| `no-unused-vars`               | `error` with `varsIgnorePattern: "^_"`, `argsIgnorePattern: "^_\|^dt$"` | Prefixing with `_` marks intentionally unused; `dt` is the standard delta-time parameter |
| `no-use-before-define`         | `error` with `functions: false`                                         | Function declarations hoist safely; allows top-down reading                              |
| `no-plusplus`                  | `off`                                                                   | `++`/`--` are idiomatic in loops and counters                                            |
| `no-continue`                  | `off`                                                                   | `continue` is common in update/draw loops                                                |
| `no-param-reassign`            | `off`                                                                   | Reassigning parameters is common in game math helpers                                    |
| `no-nested-ternary`            | `off`                                                                   | Useful for compact conditional expressions                                               |
| `no-bitwise`                   | `off`                                                                   | Bitwise ops are fundamental in graphics/palette code                                     |
| `import/no-unresolved`         | `off`                                                                   | ESLint uses Node resolution; dithr uses QuickJS at runtime                               |
| `import/extensions`            | `off`                                                                   | Same reason as above                                                                     |
| `import/prefer-default-export` | `off`                                                                   | Named exports are preferred for clarity                                                  |

All other Airbnb rules remain **enabled**, including:

- `no-var` — use `let` or `const`
- `prefer-const` — use `const` when the binding is never reassigned
- `prefer-template` — use template literals instead of string concatenation
- `camelcase` — use camelCase for variables, functions, and parameters

### Editor / multi-file carts

Carts that use ES module `import`/`export` (like the built-in editor) set
`sourceType: "module"` instead of `"script"`. All other rules remain the same.

## General Rules

- Use **4 spaces** per indent level — never tabs.
- Keep lines to **100 characters** or fewer.
- End every file with a newline.
- Use **LF** line endings.

## Variables

- Use `const` for bindings that are never reassigned:

```js
const TILE_SIZE = 8;
const enemies = [];
```

- Use `let` for bindings that change:

```js
let score = 0;
let frame = 0;
```

- **Never** use `var`.
- Declare variables close to where they are first used.

## Functions

- Prefer **function declarations** for top-level game functions — they are
  hoisted, which allows a natural top-down reading order:

```js
function _init() {
    resetLevel();
}

function _update(dt) {
    movePlayer(dt);
    updateEnemies(dt);
}

function _draw() {
    drawBackground();
    drawPlayer();
}
```

- Use `const` + arrow functions for short callbacks and inline helpers:

```js
const sorted = items.sort((a, b) => a.y - b.y);
```

## Strings

- Use **single quotes** for string literals:

```js
const name = "dithr";
```

- Use **template literals** when embedding expressions:

```js
gfx.print(`Score: ${score}`, 4, 4, 7);
```

- Do **not** use string concatenation (`+`) to build strings with variables.

## Naming

- `camelCase` for variables, functions, and parameters:

```js
let playerSpeed = 2;
function updatePlayer(dt) {
    /* ... */
}
```

- `UPPER_SNAKE_CASE` for module-level constants:

```js
const MAX_ENEMIES = 16;
const TILE_SIZE = 8;
```

- Prefix intentionally unused variables or parameters with `_`:

```js
function _update(_dt) {
    // time-independent logic
}
```

## Common Patterns

**Game loop callbacks** — the engine calls these if they exist:

| Callback       | Purpose                                |
| -------------- | -------------------------------------- |
| `_init()`      | Called once on cart load               |
| `_update(dt)`  | Called every frame before `_draw`      |
| `_draw()`      | Called every frame for rendering       |
| `_save()`      | Return state to preserve across reload |
| `_restore(st)` | Receive saved state after reload       |

**Delta time** — always use the `dt` parameter for time-dependent logic:

```js
function _update(dt) {
    x += speed * dt;
}
```

**Bitwise palette tricks** — bitwise operators are enabled:

```js
const r = (color >> 16) & 0xff;
const g = (color >> 8) & 0xff;
const b = color & 0xff;
```

## Running the Tools

Each cart's `package.json` includes convenience scripts:

| Script                 | Command                 | Description                      |
| ---------------------- | ----------------------- | -------------------------------- |
| `npm run lint`         | `eslint src/`           | Report all ESLint errors         |
| `npm run lint:fix`     | `eslint --fix src/`     | Auto-fix what ESLint can handle  |
| `npm run format`       | `prettier --write src/` | Format all source files          |
| `npm run format:check` | `prettier --check src/` | Check formatting without writing |
