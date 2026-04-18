#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawn } = require("node:child_process");
const { getBinaryPath } = require("../binary.js");
const { detectBuild, findEsbuild } = require("../build-helper.js");

function usage() {
    console.log("Usage: dithrkit dev [cart-dir] [options]");
    console.log("");
    console.log("Build TypeScript in watch mode and launch the native dithr runtime.");
    console.log("The runtime's built-in hot-reload picks up changes automatically.");
    console.log("");
    console.log("Options:");
    console.log("  --help, -h            Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h")) usage();

let cartDir = ".";

for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (!arg.startsWith("-")) {
        cartDir = arg;
    }
}

const resolvedCartDir = path.resolve(cartDir);
const cartJson = path.join(resolvedCartDir, "cart.json");

if (!fs.existsSync(cartJson)) {
    console.error(`No cart.json found in ${resolvedCartDir}`);
    console.error(
        "Make sure you are in a dithr project directory, or pass the path as an argument.",
    );
    process.exit(1);
}

const binary = getBinaryPath();
if (!fs.existsSync(binary)) {
    console.error(`dithr binary not found at: ${binary}`);
    console.error(
        "Install the platform package for your OS:\n" +
            "  npm install @dithrkit/win32-x64   (Windows)\n" +
            "  npm install @dithrkit/linux-x64    (Linux)\n" +
            "  npm install @dithrkit/darwin-arm64  (macOS)",
    );
    process.exit(1);
}

const buildInfo = detectBuild(resolvedCartDir);
let esbuildProc = null;

function cleanup() {
    if (esbuildProc) {
        esbuildProc.kill();
        esbuildProc = null;
    }
}

process.on("SIGINT", () => {
    cleanup();
    process.exit(0);
});
process.on("SIGTERM", () => {
    cleanup();
    process.exit(0);
});

function launchRuntime() {
    console.log("[dev] Launching dithr runtime…");
    const child = spawn(binary, [cartJson], {
        stdio: "inherit",
        cwd: process.cwd(),
    });

    child.on("close", (code) => {
        cleanup();
        process.exit(code || 0);
    });

    child.on("error", (err) => {
        console.error(`Failed to launch dithr: ${err.message}`);
        cleanup();
        process.exit(1);
    });
}

if (buildInfo) {
    // TypeScript project — start esbuild in watch mode, then launch runtime
    const esbuildBin = findEsbuild();
    if (!esbuildBin) {
        console.error("esbuild not found. Install @dithrkit/sdk or run npm install.");
        process.exit(1);
    }

    fs.mkdirSync(path.dirname(buildInfo.outFile), { recursive: true });

    const esbuildArgs = [
        esbuildBin,
        buildInfo.srcEntry,
        "--bundle",
        "--format=esm",
        "--target=es2020",
        `--outfile=${buildInfo.outFile}`,
        "--watch",
    ];

    console.log("[dev] Starting esbuild in watch mode…");
    esbuildProc = spawn(process.execPath, esbuildArgs, {
        stdio: ["ignore", "pipe", "inherit"],
        cwd: resolvedCartDir,
    });

    let launched = false;
    esbuildProc.stdout.on("data", (data) => {
        process.stdout.write(data);
        // Launch runtime after first successful build
        if (!launched) {
            launched = true;
            launchRuntime();
        }
    });

    esbuildProc.on("error", (err) => {
        console.error(`Failed to start esbuild: ${err.message}`);
        process.exit(1);
    });

    esbuildProc.on("close", (code) => {
        esbuildProc = null;
        if (!launched) {
            console.error(`esbuild exited with code ${code} before producing output.`);
            process.exit(1);
        }
    });
} else {
    // Plain JS project — just launch runtime directly
    launchRuntime();
}
