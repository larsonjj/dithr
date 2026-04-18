#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const { detectBuild, findEsbuild } = require("../build-helper.js");

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

const buildInfo = detectBuild(".");

if (!buildInfo) {
    console.log("Cart code entry does not use dist/. No build required.");
    process.exit(0);
}

const { srcEntry, outFile } = buildInfo;

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
const esbuildBin = findEsbuild();
if (!esbuildBin) {
    console.error("esbuild not found. Install @dithrkit/sdk or run npm install.");
    process.exit(1);
}

try {
    const result = spawnSync(process.execPath, [esbuildBin, ...esbuildArgs], {
        stdio: "inherit",
        cwd: process.cwd(),
    });
    if (result.status !== 0) {
        process.exit(result.status || 1);
    }
} catch (err) {
    process.exit(err.status || 1);
}
