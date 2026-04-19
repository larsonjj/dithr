# @dithrkit/wasm

Prebuilt dithr WASM runtime for web exports.

This package provides the compiled WebAssembly binary and JavaScript glue code
needed to run dithr carts in the browser. It is used internally by
`@dithrkit/sdk` when exporting carts with `dithrkit export --web`.

## Usage

```js
const { jsPath, wasmPath } = require("@dithrkit/wasm");
```

Both paths point to the prebuilt files in the `bin/` directory:

- **`dithr.js`** — Emscripten-generated JavaScript loader
- **`dithr.wasm`** — Compiled WebAssembly module

## Building from source

To rebuild the WASM runtime from source you need
[Emscripten](https://emscripten.org/) (>= 3.x) and Ninja:

```bash
cmake --preset wasm
cmake --build build/wasm
```

See [docs/building.md](https://github.com/dithrkit/dithr/blob/main/docs/building.md)
for full build instructions.

## License

MIT
