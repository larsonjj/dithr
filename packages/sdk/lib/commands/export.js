#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { getBinaryPath } = require("../binary.js");
const { packDirectory, generatePackageLoader } = require("../packager.js");

function usage() {
    console.log("Usage: dithrkit export <--web | --desktop> [options]");
    console.log("");
    console.log("Export the current cart for distribution.");
    console.log("");
    console.log("Targets:");
    console.log("  --web              Export as self-contained web app (WASM)");
    console.log("  --desktop          Export as standalone desktop app");
    console.log("");
    console.log("Options:");
    console.log("  --out, -o <dir>    Output directory (default: build/web or build/desktop)");
    console.log("  --help, -h         Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h") || args.length === 0) usage();

let target = null;
let outDir = null;

for (let i = 0; i < args.length; i++) {
    if (args[i] === "--web") target = "web";
    else if (args[i] === "--desktop") target = "desktop";
    else if (args[i] === "--out" || args[i] === "-o") outDir = args[++i];
}

if (!target) {
    console.error("Must specify --web or --desktop");
    process.exit(1);
}

const cartJson = path.resolve("cart.json");
if (!fs.existsSync(cartJson)) {
    console.error("No cart.json found in current directory.");
    process.exit(1);
}

const cart = JSON.parse(fs.readFileSync(cartJson, "utf-8"));

if (target === "web") {
    exportWeb(cart, outDir || path.resolve("build", "web"));
} else {
    exportDesktop(cart, outDir || path.resolve("build", "desktop"));
}

function exportWeb(cart, out) {
    // Resolve WASM runtime
    let wasmPkg;
    try {
        wasmPkg = require("@dithrkit/wasm");
    } catch {
        console.error("@dithrkit/wasm is not installed.");
        console.error("Install it: npm install @dithrkit/wasm");
        process.exit(1);
    }

    if (!fs.existsSync(wasmPkg.jsPath) || !fs.existsSync(wasmPkg.wasmPath)) {
        console.error("WASM runtime files not found in @dithrkit/wasm.");
        console.error("The package may not have been built yet.");
        process.exit(1);
    }

    fs.mkdirSync(out, { recursive: true });

    // Copy WASM runtime files
    fs.copyFileSync(wasmPkg.jsPath, path.join(out, "dithr.js"));
    fs.copyFileSync(wasmPkg.wasmPath, path.join(out, "dithr.wasm"));

    // Pack cart directory into .data file
    const cartDir = path.dirname(cartJson);
    const { dataBuffer, files } = packDirectory(cartDir, "/cart");

    fs.writeFileSync(path.join(out, "dithr.data"), dataBuffer);

    // Generate package loader
    const loaderJs = generatePackageLoader("dithr.data", files);
    fs.writeFileSync(path.join(out, "dithr.data.js"), loaderJs);

    // Generate index.html from shell template
    const shellPath = path.join(__dirname, "..", "..", "templates", "shell.html");
    let html = fs.readFileSync(shellPath, "utf-8");

    // Replace title
    const title = cart.title || "dithr";
    html = html.replace("<title>dithr</title>", `<title>${title}</title>`);

    // Replace {{{ SCRIPT }}} with script tags
    const scripts = [
        '<script src="dithr.data.js"></script>',
        '<script async src="dithr.js"></script>',
    ].join("\n");
    html = html.replace("{{{ SCRIPT }}}", scripts);

    fs.writeFileSync(path.join(out, "index.html"), html);

    console.log(`Web export created in ${out}/`);
    console.log("Files:");
    console.log("  index.html");
    console.log("  dithr.js");
    console.log("  dithr.wasm");
    console.log("  dithr.data");
    console.log("  dithr.data.js");
}

function exportDesktop(cart, out) {
    const binary = getBinaryPath();

    if (!fs.existsSync(binary)) {
        console.error(`dithr binary not found at: ${binary}`);
        process.exit(1);
    }

    fs.mkdirSync(out, { recursive: true });

    // Copy binary
    const binaryName = path.basename(binary);
    const destBinary = path.join(out, binaryName);
    fs.copyFileSync(binary, destBinary);

    // Make executable on Unix
    if (process.platform !== "win32") {
        fs.chmodSync(destBinary, 0o755);
    }

    // Copy cart directory
    const cartDir = path.dirname(cartJson);
    const destCart = path.join(out, "cart");
    copyDirSync(cartDir, destCart);

    console.log(`Desktop export created in ${out}/`);
    console.log("Files:");
    console.log(`  ${binaryName}`);
    console.log("  cart/");
}

function copyDirSync(src, dest) {
    fs.mkdirSync(dest, { recursive: true });
    const entries = fs.readdirSync(src, { withFileTypes: true });

    for (const entry of entries) {
        const srcPath = path.join(src, entry.name);
        const destPath = path.join(dest, entry.name);

        // Skip node_modules, build, .git
        if (entry.name === "node_modules" || entry.name === "build" || entry.name === ".git") {
            continue;
        }

        if (entry.isDirectory()) {
            copyDirSync(srcPath, destPath);
        } else {
            fs.copyFileSync(srcPath, destPath);
        }
    }
}
