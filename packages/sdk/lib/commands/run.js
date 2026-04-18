#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawn } = require("node:child_process");
const { getBinaryPath } = require("../binary.js");

function usage() {
    console.log("Usage: dithrkit run [cart-dir] [options]");
    console.log("");
    console.log("Launch the native dithr runtime with the cart in the current directory");
    console.log("(or the specified directory).");
    console.log("");
    console.log("Options:");
    console.log("  --help, -h    Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h")) usage();

const cartDir = args[0] || ".";
const cartJson = path.resolve(cartDir, "cart.json");

if (!fs.existsSync(cartJson)) {
    console.error(`No cart.json found in ${path.resolve(cartDir)}`);
    process.exit(1);
}

const binary = getBinaryPath();

if (!fs.existsSync(binary)) {
    console.error(`dithr binary not found at: ${binary}`);
    console.error("The platform package may not have been built yet.");
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
