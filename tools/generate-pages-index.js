#!/usr/bin/env node
/**
 * @file  generate-pages-index.js
 * @brief Generates an index.html for GitHub Pages linking to every built
 *        example. Reads cart.json from each example directory in the repo
 *        and checks that the corresponding build output exists in the
 *        site directory.
 *
 * Usage:  node tools/generate-pages-index.js <siteDir>
 */

const fs = require("node:fs");
const path = require("node:path");

const siteDir = process.argv[2];
if (!siteDir) {
    console.error("Usage: generate-pages-index.js <siteDir>");
    process.exit(1);
}

const examplesDir = path.resolve(__dirname, "..", "examples");
const entries = [];

for (const name of fs.readdirSync(examplesDir, { withFileTypes: true })) {
    if (!name.isDirectory()) continue;
    const cartPath = path.join(examplesDir, name.name, "cart.json");
    if (!fs.existsSync(cartPath)) continue;

    const builtDir = path.join(siteDir, name.name);
    if (!fs.existsSync(path.join(builtDir, "index.html"))) {
        console.warn(`Skipping ${name.name} — no build output found`);
        continue;
    }

    const cart = JSON.parse(fs.readFileSync(cartPath, "utf-8"));
    const title = cart.title || (cart.meta && cart.meta.title) || name.name;
    const desc = cart.description || (cart.meta && cart.meta.description) || "";
    entries.push({ dir: name.name, title, desc });
}

entries.sort((a, b) => a.title.localeCompare(b.title));

const cards = entries
    .map(
        (e) => `      <a class="card" href="${e.dir}/">
        <span class="title">${escapeHtml(e.title)}</span>
        ${e.desc ? `<span class="desc">${escapeHtml(e.desc)}</span>` : ""}
      </a>`,
    )
    .join("\n");

const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>dithr — Examples</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: system-ui, -apple-system, sans-serif;
      background: #111; color: #eee;
      display: flex; flex-direction: column; align-items: center;
      min-height: 100vh; padding: 2rem 1rem;
    }
    h1 { font-size: 2rem; margin-bottom: .25rem; }
    .subtitle { color: #888; margin-bottom: 2rem; font-size: .95rem; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
      gap: 1rem; width: 100%; max-width: 960px;
    }
    .card {
      display: flex; flex-direction: column; gap: .25rem;
      background: #1a1a1a; border: 1px solid #333; border-radius: 8px;
      padding: 1.25rem; text-decoration: none; color: #eee;
      transition: border-color .15s, background .15s;
    }
    .card:hover { border-color: #6cf; background: #222; }
    .title { font-weight: 600; font-size: 1.05rem; }
    .desc { color: #999; font-size: .85rem; }
  </style>
</head>
<body>
  <h1>dithr</h1>
  <p class="subtitle">${entries.length} example carts</p>
  <div class="grid">
${cards}
  </div>
</body>
</html>
`;

fs.writeFileSync(path.join(siteDir, "index.html"), html);
console.log(`Generated index.html with ${entries.length} examples`);

function escapeHtml(str) {
    return str
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}
