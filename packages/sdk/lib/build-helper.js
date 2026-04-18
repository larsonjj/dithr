"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");

/**
 * Detect whether a cart project needs a build step (TS → dist/).
 * Returns { srcEntry, outFile } if a build is needed, or null otherwise.
 */
function detectBuild(cartDir) {
    const cartPath = path.resolve(cartDir, "cart.json");
    if (!fs.existsSync(cartPath)) return null;

    const cart = JSON.parse(fs.readFileSync(cartPath, "utf-8"));
    const codeEntry = cart.code || "dist/main.js";

    if (!codeEntry.startsWith("dist/")) return null;

    const base = path.basename(codeEntry, ".js");
    const tsFile = path.resolve(cartDir, "src", `${base}.ts`);
    const jsFile = path.resolve(cartDir, "src", `${base}.js`);

    let srcEntry;
    if (fs.existsSync(tsFile)) {
        srcEntry = tsFile;
    } else if (fs.existsSync(jsFile)) {
        srcEntry = jsFile;
    } else {
        return null;
    }

    return { srcEntry, outFile: path.resolve(cartDir, codeEntry) };
}

/**
 * Find the esbuild binary.
 * Returns the path to esbuild/bin/esbuild or null.
 */
function findEsbuild() {
    try {
        return require.resolve("esbuild/bin/esbuild");
    } catch {
        const localBin = path.resolve("node_modules", ".bin", "esbuild");
        if (fs.existsSync(localBin)) return localBin;
        return null;
    }
}

/**
 * Run a synchronous esbuild build.
 * Returns true on success, false on failure.
 */
function buildSync(cartDir) {
    const info = detectBuild(cartDir);
    if (!info) return true; // no build needed

    const esbuildBin = findEsbuild();
    if (!esbuildBin) {
        console.error("esbuild not found. Install @dithrkit/sdk or run npm install.");
        return false;
    }

    fs.mkdirSync(path.dirname(info.outFile), { recursive: true });

    const esbuildArgs = [
        info.srcEntry,
        "--bundle",
        "--format=esm",
        "--target=es2020",
        `--outfile=${info.outFile}`,
    ];

    const result = spawnSync(process.execPath, [esbuildBin, ...esbuildArgs], {
        stdio: "inherit",
        cwd: cartDir,
    });

    return result.status === 0;
}

module.exports = { detectBuild, findEsbuild, buildSync };
