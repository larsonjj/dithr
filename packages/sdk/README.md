# @dithrkit/sdk

SDK and CLI for the [dithr](https://github.com/dithrkit/dithr) fantasy console.

## Installation

```bash
npm install @dithrkit/sdk
```

Or scaffold a new project (installs the SDK automatically):

```bash
npm create @dithrkit my-game
```

## CLI Commands

| Command           | Description                                    |
| ----------------- | ---------------------------------------------- |
| `dithrkit run`    | Run natively with hot-reload                   |
| `dithrkit dev`    | Build TS in watch mode + run (native dev loop) |
| `dithrkit build`  | Bundle JS/TS source via esbuild                |
| `dithrkit serve`  | Build and serve in the browser                 |
| `dithrkit watch`  | Build, serve, and live-reload on file changes  |
| `dithrkit export` | Export for web or desktop distribution         |

## TypeScript Support

The SDK ships type definitions for the dithr API. Scaffolded TypeScript
projects automatically extend the shared tsconfig:

```json
{
    "extends": "@dithrkit/sdk/configs/tsconfig.base.json",
    "include": ["src/**/*.ts"]
}
```

## ESLint Config

A shared ESLint config is available for both JS and TS projects:

```js
const dithrConfig = require("@dithrkit/sdk/configs/eslint");

module.exports = [
    ...dithrConfig.ts, // or dithrConfig.js
];
```

## Documentation

- [Getting Started](https://github.com/dithrkit/dithr/blob/main/docs/getting-started.md)
- [API Reference](https://github.com/dithrkit/dithr/blob/main/docs/api-reference.md)
- [Cart Format](https://github.com/dithrkit/dithr/blob/main/docs/cart-format.md)
- [Architecture](https://github.com/dithrkit/dithr/blob/main/docs/architecture.md)
- [Building](https://github.com/dithrkit/dithr/blob/main/docs/building.md)

## License

MIT
