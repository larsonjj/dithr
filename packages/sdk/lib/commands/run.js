#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawn } = require("node:child_process");
const { getBinaryPath } = require("../binary.js");
const { detectBuild, buildSync } = require("../build-helper.js");

function usage() {
    console.log("Usage: dithrkit run [cart-dir] [options]");
    console.log("");
    console.log("Launch the native dithr runtime with the cart in the current directory");
    console.log("(or the specified directory). TypeScript projects are automatically");
    console.log("built before launching.");
    console.log("");
    console.log("Options:");
    console.log("  --no-build            Skip automatic TypeScript build");
    console.log("  --help, -h            Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h")) usage();

// Parse arguments
let cartDir = ".";
let skipBuild = false;

for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "--no-build") {
        skipBuild = true;
    } else if (!arg.startsWith("-")) {
        cartDir = arg;
    }
}

const cartJson = path.resolve(cartDir, "cart.json");

if (!fs.existsSync(cartJson)) {
    console.error(`No cart.json found in ${path.resolve(cartDir)}`);
    console.error(
        "Make sure you are in a dithr project directory, or pass the path as an argument.",
    );
    process.exit(1);
}

// Auto-build TypeScript projects
if (!skipBuild && detectBuild(path.resolve(cartDir))) {
    console.log("Building TypeScript...");
    if (!buildSync(path.resolve(cartDir))) {
        console.error("Build failed. Fix errors above and try again.");
        process.exit(1);
    }
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

const child = spawn(binary, [cartJson], {
    stdio: "inherit",
    cwd: process.cwd(),
});

child.on("close", (code) => {
    process.exit(code || 0);
});

child.on("error", (err) => {
    console.error(`Failed to launch dithr: ${err.message}`);
    process.exit(1);
});
