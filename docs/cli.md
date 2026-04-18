# CLI Reference

The `@dithrkit/sdk` package provides the `dithrkit` command-line tool for
creating, building, running, and exporting dithr carts.

## Installation

```bash
npm install @dithrkit/sdk
```

Or scaffold a project which includes the SDK as a dev dependency:

```bash
npm create @dithrkit my-game
```

## Commands

### `dithrkit create <name>`

Scaffold a new dithr project.

```bash
dithrkit create my-game
dithrkit create my-game --ts                      # TypeScript project
dithrkit create my-game --ts --template platformer # with template
```

| Option               | Description                         |
| -------------------- | ----------------------------------- |
| `--template, -t`     | Template to use (blank, platformer) |
| `--typescript, --ts` | Scaffold a TypeScript project       |

### `dithrkit run [cart-dir]`

Launch the native dithr runtime with the cart in the current (or specified)
directory. TypeScript projects are automatically built before launching.

```bash
dithrkit run
dithrkit run path/to/cart
dithrkit run --no-build   # skip auto-build
```

| Option       | Description                     |
| ------------ | ------------------------------- |
| `--no-build` | Skip automatic TypeScript build |

### `dithrkit dev [cart-dir]`

Build TypeScript in watch mode and launch the native runtime. The runtime's
built-in hot-reload picks up changes automatically (300 ms poll). For plain
JavaScript projects this is equivalent to `dithrkit run`.

```bash
dithrkit dev
```

### `dithrkit build`

Bundle cart TypeScript/JavaScript source via esbuild. Reads `cart.json` in the
current directory to determine the entry point. If `cart.json`'s `code` field
does not start with `dist/`, no build is needed and the command exits cleanly.

```bash
dithrkit build
dithrkit build --watch   # rebuild on changes
```

| Option        | Description                   |
| ------------- | ----------------------------- |
| `--watch, -w` | Watch for changes and rebuild |

### `dithrkit serve [port]`

Build and serve the cart in the browser. Exports the cart for WASM and starts a
local HTTP server.

```bash
dithrkit serve
dithrkit serve 3000
```

| Option       | Description                      |
| ------------ | -------------------------------- |
| `--port, -p` | Port to serve on (default: 8080) |

### `dithrkit watch [port]`

Watch cart source directory, auto-rebuild, and serve with live reload. Supports
hot-reload for JS entry changes, asset-reload for sprite/map/audio changes, and
full rebuild for structural changes.

```bash
dithrkit watch
dithrkit watch 3000
```

| Option       | Description                      |
| ------------ | -------------------------------- |
| `--port, -p` | Port to serve on (default: 8080) |

### `dithrkit export`

Export the cart for distribution.

```bash
dithrkit export --web              # browser-ready build
dithrkit export --web --out dist   # custom output directory
dithrkit export --desktop          # standalone executable
```

| Option      | Description                             |
| ----------- | --------------------------------------- |
| `--web`     | Export for web (WASM)                   |
| `--desktop` | Export as standalone desktop executable |
| `--out`     | Output directory (default: `build/web`) |

## Typical workflows

### JavaScript project (native)

```bash
npm create @dithrkit my-game
cd my-game && npm install
npx dithrkit run          # edit src/main.js, save → hot-reload
```

### TypeScript project (native)

```bash
npm create @dithrkit my-game --ts
cd my-game && npm install
npx dithrkit dev          # builds TS in watch mode + runs
```

### Browser development

```bash
npx dithrkit watch        # auto-rebuild + live-reload in browser
```

### Export for distribution

```bash
npx dithrkit export --web       # → build/web/
npx dithrkit export --desktop   # → build/desktop/
```
