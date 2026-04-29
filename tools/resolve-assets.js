#!/usr/bin/env node
// tools/resolve-assets.js — Resolve cart `assets` manifest into bundled,
// dynamic, and excluded file lists.
//
// Used by:
//   - CMakeLists.txt (WASM target, configure-time)
//   - tools/serve-wasm.js, tools/watch-wasm.js
//   - packages/sdk/lib/commands/export.js
//
// Usage as a CLI:
//   node tools/resolve-assets.js <cart-dir> [--json]
//
// Usage as a library:
//   const { resolveAssets } = require('./tools/resolve-assets.js');
//   const { bundled, dynamic, excluded } = resolveAssets(cartDir);
//
// The `assets` block in cart.json:
//   {
//     "include": ["**/*"],          // default
//     "exclude": ["**/*.psd"],      // default []
//     "dynamic": ["maps/dlc-*.tmj"],// default [] — bundled on native, HTTP on web
//     "compress": false             // false | "br" | "gzip" — applies to dynamic
//   }
//
// Resolution rules:
//   - File is bundled iff matched by include AND not by exclude AND not by dynamic.
//   - File is dynamic iff matched by dynamic AND not by exclude.
//   - Dynamic files matched by exclude are still excluded (exclude wins).
//   - All paths returned are POSIX-style (forward slashes), relative to cart dir.

"use strict";

const fs = require("node:fs");
const path = require("node:path");

const DEFAULT_INCLUDE = ["**/*"];
const DEFAULT_EXCLUDE = [];
const DEFAULT_DYNAMIC = [];
const VALID_COMPRESS = [false, "br", "gzip"];

// Always-excluded paths (not configurable). These are dev/build artifacts
// that should never end up in a distributable cart.
const ALWAYS_EXCLUDE = ["node_modules/**", ".git/**", "build/**", ".DS_Store"];

/* ───── Glob → RegExp ───────────────────────────────────────────────────── */

/**
 * Convert a minimatch-style glob to a JavaScript RegExp.
 * Supports: `**` (any depth, including zero segments), `*` (any non-`/` chars),
 * `?` (single non-`/` char), and literal characters.
 */
function globToRegExp(glob) {
    // Normalize to forward slashes
    let g = glob.replace(/\\/g, "/");

    // Escape regex metacharacters EXCEPT the glob ones we handle below.
    // Process char by char so we can convert glob tokens in the same pass.
    let re = "^";
    let i = 0;
    while (i < g.length) {
        const c = g[i];

        // `**/` — any (zero+) directory segments, including the trailing slash
        if (c === "*" && g[i + 1] === "*" && g[i + 2] === "/") {
            re += "(?:.*/)?";
            i += 3;
            continue;
        }
        // Trailing `**` — match anything including slashes
        if (c === "*" && g[i + 1] === "*") {
            re += ".*";
            i += 2;
            continue;
        }
        // `*` — match any non-slash chars
        if (c === "*") {
            re += "[^/]*";
            i++;
            continue;
        }
        // `?` — match single non-slash char
        if (c === "?") {
            re += "[^/]";
            i++;
            continue;
        }
        // Literal — escape regex metacharacters
        if (/[.+^${}()|[\]\\]/.test(c)) {
            re += "\\" + c;
        } else {
            re += c;
        }
        i++;
    }
    re += "$";
    return new RegExp(re);
}

/**
 * Test whether `posixPath` matches any glob in the list.
 */
function matchesAny(posixPath, globs) {
    for (const g of globs) {
        if (globToRegExp(g).test(posixPath)) {
            return true;
        }
    }
    return false;
}

/* ───── Directory walk ──────────────────────────────────────────────────── */

/**
 * Recursively walk `rootDir`, returning a sorted list of POSIX-style
 * paths relative to root. Does not follow symlinks.
 */
function walk(rootDir) {
    const out = [];
    function recurse(absDir, relDir) {
        let entries;
        try {
            entries = fs.readdirSync(absDir, { withFileTypes: true });
        } catch (err) {
            // Unreadable directory — skip silently (mirrors fast-glob behavior)
            return;
        }
        for (const ent of entries) {
            const absPath = path.join(absDir, ent.name);
            const relPath = relDir ? `${relDir}/${ent.name}` : ent.name;
            if (ent.isSymbolicLink()) {
                continue;
            }
            if (ent.isDirectory()) {
                recurse(absPath, relPath);
            } else if (ent.isFile()) {
                out.push(relPath);
            }
        }
    }
    recurse(rootDir, "");
    out.sort();
    return out;
}

/* ───── Public API ──────────────────────────────────────────────────────── */

/**
 * Read a cart.json from `cartDir` and parse its `assets` block, falling
 * back to defaults for missing fields.
 */
function readAssetsConfig(cartDir) {
    const cartJsonPath = path.join(cartDir, "cart.json");
    if (!fs.existsSync(cartJsonPath)) {
        throw new Error(`cart.json not found in ${cartDir}`);
    }
    const cart = JSON.parse(fs.readFileSync(cartJsonPath, "utf-8"));
    const assets = cart.assets || {};

    const include = Array.isArray(assets.include) ? assets.include : DEFAULT_INCLUDE;
    const exclude = Array.isArray(assets.exclude) ? assets.exclude : DEFAULT_EXCLUDE;
    const dynamic = Array.isArray(assets.dynamic) ? assets.dynamic : DEFAULT_DYNAMIC;
    let compress = assets.compress;
    if (compress === undefined) {
        compress = false;
    }
    if (!VALID_COMPRESS.includes(compress)) {
        throw new Error(
            `cart.json: assets.compress must be false, "br", or "gzip" (got ${JSON.stringify(compress)})`,
        );
    }

    return { include, exclude, dynamic, compress };
}

/**
 * Resolve a cart directory's `assets` manifest into three file lists.
 *
 * @param {string} cartDir - Absolute path to the cart directory containing cart.json.
 * @returns {{
 *   bundled: string[],
 *   dynamic: string[],
 *   excluded: string[],
 *   compress: false | "br" | "gzip",
 * }}
 */
function resolveAssets(cartDir) {
    const cfg = readAssetsConfig(cartDir);
    const allFiles = walk(cartDir);

    const bundled = [];
    const dyn = [];
    const excluded = [];

    // ALWAYS_EXCLUDE first, then user exclude, then user dynamic.
    const effectiveExclude = ALWAYS_EXCLUDE.concat(cfg.exclude);

    for (const rel of allFiles) {
        // cart.json itself is always bundled (the runtime needs it).
        if (rel === "cart.json") {
            bundled.push(rel);
            continue;
        }

        if (matchesAny(rel, effectiveExclude)) {
            excluded.push(rel);
            continue;
        }
        if (!matchesAny(rel, cfg.include)) {
            excluded.push(rel);
            continue;
        }
        if (matchesAny(rel, cfg.dynamic)) {
            dyn.push(rel);
            continue;
        }
        bundled.push(rel);
    }

    return { bundled, dynamic: dyn, excluded, compress: cfg.compress };
}

/* ───── CLI ─────────────────────────────────────────────────────────────── */

function main() {
    const args = process.argv.slice(2);
    if (args.length === 0 || args.includes("--help") || args.includes("-h")) {
        console.log("Usage: node tools/resolve-assets.js <cart-dir> [--json]");
        console.log("");
        console.log("Resolve cart.json `assets` manifest into bundled/dynamic/excluded lists.");
        console.log("");
        console.log("Options:");
        console.log("  --json   Emit a single JSON object on stdout (for tool consumption).");
        process.exit(args.length === 0 ? 1 : 0);
    }

    const jsonOut = args.includes("--json");
    const cartDir = path.resolve(args.find((a) => !a.startsWith("--")));

    const result = resolveAssets(cartDir);

    if (jsonOut) {
        process.stdout.write(JSON.stringify(result));
        return;
    }

    console.log(`Cart: ${cartDir}`);
    console.log(`Compress: ${JSON.stringify(result.compress)}`);
    console.log(`Bundled (${result.bundled.length}):`);
    for (const f of result.bundled) console.log(`  ${f}`);
    console.log(`Dynamic (${result.dynamic.length}):`);
    for (const f of result.dynamic) console.log(`  ${f}`);
    console.log(`Excluded (${result.excluded.length}):`);
    for (const f of result.excluded) console.log(`  ${f}`);
}

if (require.main === module) {
    main();
}

module.exports = {
    resolveAssets,
    readAssetsConfig,
    globToRegExp,
    matchesAny,
    walk,
    ALWAYS_EXCLUDE,
};
