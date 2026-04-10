#!/usr/bin/env node
// tools/cart-export.js — Export a cart to a standalone HTML file
// Usage: node cart-export.js <cart.json> [--out output.html]
//
// NOTE: This tool is a work-in-progress. The generated HTML embeds the cart
// data but requires a standalone dithr WASM runtime to actually run. Use the
// normal WASM build workflow (cmake --preset wasm) for playable web exports.

"use strict";

const fs = require("fs");
const path = require("path");

function usage() {
    console.log("Usage: node cart-export.js <cart.json> [--out output.html]");
    process.exit(1);
}

function readCartFile(cartPath) {
    const raw = fs.readFileSync(cartPath, "utf-8");
    return JSON.parse(raw);
}

function resolveAsset(basePath, relPath) {
    const full = path.resolve(basePath, relPath);
    if (!fs.existsSync(full)) {
        console.warn(`Warning: asset not found: ${full}`);
        return null;
    }
    return full;
}

function fileToBase64(filePath) {
    const buf = fs.readFileSync(filePath);
    return buf.toString("base64");
}

function main() {
    const args = process.argv.slice(2);
    if (args.length === 0) usage();

    let cartFile = null;
    let outFile = null;

    for (let i = 0; i < args.length; i++) {
        if (args[i] === "--out" || args[i] === "-o") {
            outFile = args[++i];
        } else if (!cartFile) {
            cartFile = args[i];
        }
    }

    if (!cartFile) usage();

    const cartPath = path.resolve(cartFile);
    if (!fs.existsSync(cartPath)) {
        console.error(`Cart file not found: ${cartPath}`);
        process.exit(1);
    }

    const basePath = path.dirname(cartPath);
    const cart = readCartFile(cartPath);
    const title = cart.title || "dithr cart";

    if (!outFile) {
        outFile = path.join(basePath, `${title.replace(/[^a-zA-Z0-9_-]/g, "_")}.html`);
    }

    // Read JS source
    let jsSource = "";
    if (cart.code) {
        const codePath = resolveAsset(basePath, cart.code);
        if (codePath) {
            jsSource = fs.readFileSync(codePath, "utf-8");
        }
    }

    // Read sprite sheet as base64
    let spriteDataUrl = "";
    if (cart.sprites && cart.sprites.sheet) {
        const sheetPath = resolveAsset(basePath, cart.sprites.sheet);
        if (sheetPath) {
            const ext = path.extname(sheetPath).toLowerCase();
            const mime = ext === ".png" ? "image/png" : "image/bmp";
            spriteDataUrl = `data:${mime};base64,${fileToBase64(sheetPath)}`;
        }
    }

    // Build embedded cart JSON (inline assets)
    const exportCart = JSON.parse(JSON.stringify(cart));
    exportCart._embedded = {
        code: jsSource,
        sprites: spriteDataUrl,
    };

    const html = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>${escapeHtml(title)}</title>
<style>
html, body { margin: 0; padding: 0; background: #000; overflow: hidden; }
canvas { display: block; margin: auto; image-rendering: pixelated; }
#splash {
  position: fixed; inset: 0; display: flex; align-items: center;
  justify-content: center; background: #111; color: #ccc;
  font: 1.4rem sans-serif; cursor: pointer; z-index: 10;
}
#splash.hidden { display: none; }
</style>
</head>
<body>
<div id="splash">Tap or click to start</div>
<canvas id="screen"></canvas>
<script>
// Audio context unlock — required for browsers to allow playback
(function() {
  var splash = document.getElementById("splash");
  function unlock() {
    splash.classList.add("hidden");
    var AudioCtx = window.AudioContext || window.webkitAudioContext;
    if (AudioCtx) {
      var ctx = new AudioCtx();
      if (ctx.state === "suspended") ctx.resume();
      window._dtrAudioCtx = ctx;
    }
    document.removeEventListener("click", unlock);
    document.removeEventListener("touchend", unlock);
    if (window._dithrStart) window._dithrStart();
  }
  document.addEventListener("click", unlock);
  document.addEventListener("touchend", unlock);
})();
</script>
<script>
// dithr embedded cart — requires dithr WASM runtime
window.DITHR_CART = ${JSON.stringify(exportCart)};
</script>
<script>
// Placeholder: load dithr WASM runtime here
console.log("dithr cart loaded. WASM runtime required to run.");
console.log("Cart:", window.DITHR_CART.meta);
</script>
</body>
</html>`;

    fs.writeFileSync(outFile, html);
    console.log(`Exported: ${outFile}`);
}

function escapeHtml(str) {
    return str
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

main();
