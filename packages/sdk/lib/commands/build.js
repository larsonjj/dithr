#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { execSync } = require("node:child_process");

function usage() {
    console.log("Usage: dithrkit build [options]");
    console.log("");
    console.log("Bundle cart TypeScript/JavaScript source via esbuild.");
    console.log("Reads cart.json in the current directory to determine the entry point.");
    console.log("");
    console.log("Options:");
    console.log("  --watch, -w   Watch for changes and rebuild");
    console.log("  --help, -h    Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h")) usage();

const watchMode = args.includes("--watch") || args.includes("-w");
const cartPath = path.resolve("cart.json");

if (!fs.existsSync(cartPath)) {
    console.error("No cart.json found in current directory.");
    console.error("Run this command from the root of a dithr project.");
    process.exit(1);
}

const cart = JSON.parse(fs.readFileSync(cartPath, "utf-8"));
const codeEntry = cart.code || "dist/main.js";

// Determine source file — if code points to dist/, source is in src/
let srcEntry;
if (codeEntry.startsWith("dist/")) {
    const base = path.basename(codeEntry, ".js");
    const tsFile = path.resolve("src", `${base}.ts`);
    const jsFile = path.resolve("src", `${base}.js`);
    if (fs.existsSync(tsFile)) {
        srcEntry = tsFile;
    } else if (fs.existsSync(jsFile)) {
        srcEntry = jsFile;
    } else {
        console.error(`Cannot find source file: src/${base}.ts or src/${base}.js`);
        process.exit(1);
    }
} else {
    // Code entry points directly to source — no build needed
    console.log("Cart code entry does not use dist/. No build required.");
    process.exit(0);
}

const outFile = path.resolve(codeEntry);

// Ensure output directory exists
fs.mkdirSync(path.dirname(outFile), { recursive: true });

const esbuildArgs = [
    srcEntry,
    "--bundle",
    "--format=esm",
    "--target=es2020",
    `--outfile=${outFile}`,
];

if (watchMode) {
    esbuildArgs.push("--watch");
}

// Find esbuild binary
let esbuildBin;
try {
    esbuildBin = require.resolve("esbuild/bin/esbuild");
} catch {
    // Fallback: try to find in node_modules/.bin
    const localBin = path.resolve("node_modules", ".bin", "esbuild");
    if (fs.existsSync(localBin)) {
        esbuildBin = localBin;
    } else {
        console.error("esbuild not found. It should be installed as part of @dithrkit/sdk.");
        process.exit(1);
    }
}

try {
    execSync(`node ${JSON.stringify(esbuildBin)} ${esbuildArgs.join(" ")}`, {
        stdio: "inherit",
        cwd: process.cwd(),
    });
} catch (err) {
    process.exit(err.status || 1);
}
