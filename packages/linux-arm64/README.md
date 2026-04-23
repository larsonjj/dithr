# @dithrkit/linux-arm64

Linux ARM64 (`aarch64`) runtime binary for [dithr](https://github.com/dithrkit/dithr), a small fantasy console.

This package ships the prebuilt `dithr` executable for Linux on 64-bit ARM CPUs (e.g. Raspberry Pi 4/5 running 64-bit Linux, AWS Graviton, Ampere). It is not meant to be installed directly.

## Usage

Install the SDK, which will pull in the correct platform binary as an optional dependency:

```sh
npm install @dithrkit/sdk
```

Or scaffold a new project:

```sh
npm create dithr@latest
```

The SDK resolves the binary at runtime via [`@dithrkit/sdk/lib/binary.js`](../sdk/lib/binary.js).

## License

MIT — see [LICENSE](../../LICENSE).
