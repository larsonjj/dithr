#!/usr/bin/env node
// tools/prepare-dynamic.js — Stage `assets.dynamic` files for a WASM build.
//
// Reads the cart manifest, copies each dynamic file into <outDir>/<rel>, and
// (if assets.compress is "br" or "gzip") writes a compressed sibling next to
// each text-like file. Skips already-compressed payload formats.
//
// Usage:
//   node tools/prepare-dynamic.js <cart-dir> <out-dir>
//
// Designed to be invoked by CMake at build time (or by watch-wasm.js).

"use strict";

const fs = require("node:fs");
const path = require("node:path");
const zlib = require("node:zlib");

const { resolveAssets } = require("./resolve-assets.js");

// Extensions whose payload is already compressed and would not benefit
// from an extra brotli/gzip pass.
const ALREADY_COMPRESSED = new Set([
    ".png",
    ".jpg",
    ".jpeg",
    ".gif",
    ".webp",
    ".ogg",
    ".mp3",
    ".m4a",
    ".aac",
    ".flac",
    ".opus",
    ".zip",
    ".gz",
    ".br",
    ".woff",
    ".woff2",
]);

function shouldCompress(rel) {
    return !ALREADY_COMPRESSED.has(path.extname(rel).toLowerCase());
}

function copyIfChanged(src, dest) {
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    try {
        const s = fs.statSync(src);
        const d = fs.statSync(dest);
        if (d.size === s.size && d.mtimeMs >= s.mtimeMs) {
            return false;
        }
    } catch {
        // dest missing — fall through
    }
    fs.copyFileSync(src, dest);
    return true;
}

function writeCompressed(srcAbs, destAbs, format) {
    const data = fs.readFileSync(srcAbs);
    let compressed;
    let suffix;
    if (format === "br") {
        compressed = zlib.brotliCompressSync(data, {
            params: {
                [zlib.constants.BROTLI_PARAM_QUALITY]: 11,
                [zlib.constants.BROTLI_PARAM_SIZE_HINT]: data.length,
            },
        });
        suffix = ".br";
    } else if (format === "gzip") {
        compressed = zlib.gzipSync(data, { level: 9 });
        suffix = ".gz";
    } else {
        return false;
    }

    // Only write if smaller than original — otherwise serving the compressed
    // sibling would cost bytes, not save them.
    if (compressed.length >= data.length) {
        try {
            fs.unlinkSync(destAbs + suffix);
        } catch {
            /* ignore */
        }
        return false;
    }

    fs.mkdirSync(path.dirname(destAbs + suffix), { recursive: true });
    fs.writeFileSync(destAbs + suffix, compressed);
    return true;
}

function main() {
    const args = process.argv.slice(2);
    if (args.length < 2) {
        console.error("Usage: node tools/prepare-dynamic.js <cart-dir> <out-dir>");
        process.exit(1);
    }
    const cartDir = path.resolve(args[0]);
    const outDir = path.resolve(args[1]);

    const { dynamic, compress } = resolveAssets(cartDir);

    if (dynamic.length === 0) {
        // Clean any stale dynamic dir contents to avoid serving removed files.
        if (fs.existsSync(outDir)) {
            fs.rmSync(outDir, { recursive: true, force: true });
        }
        return;
    }

    fs.mkdirSync(outDir, { recursive: true });

    let copied = 0;
    let compressed = 0;
    for (const rel of dynamic) {
        const src = path.join(cartDir, rel);
        const dest = path.join(outDir, rel);
        if (copyIfChanged(src, dest)) {
            copied++;
        }
        if (compress && shouldCompress(rel)) {
            if (writeCompressed(src, dest, compress)) {
                compressed++;
            }
        }
    }

    if (copied > 0 || compressed > 0) {
        console.log(
            `prepare-dynamic: ${copied} copied, ${compressed} compressed (${compress || "off"}) → ${outDir}`,
        );
    }
}

if (require.main === module) {
    main();
}

module.exports = { shouldCompress, writeCompressed };
