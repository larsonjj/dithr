#!/usr/bin/env node
// tools/create-cart.js — Project scaffolding tool for mvngin carts
// Usage: node create-cart.js <name> [--template blank|platformer|pico8-port]

"use strict";

const fs = require("fs");
const path = require("path");

const TEMPLATES_DIR = path.join(__dirname, "templates");

const TEMPLATES = {
    blank: "blank.js",
    platformer: "platformer.js",
    "pico8-port": "pico8-port.js",
};

function usage() {
    console.log("Usage: node create-cart.js <name> [--template <template>]");
    console.log("");
    console.log("Templates:");
    for (const key of Object.keys(TEMPLATES)) {
        console.log(`  ${key}`);
    }
    process.exit(1);
}

function main() {
    const args = process.argv.slice(2);
    if (args.length === 0) usage();

    let name = null;
    let template = "blank";

    for (let i = 0; i < args.length; i++) {
        if (args[i] === "--template" || args[i] === "-t") {
            template = args[++i];
            if (!TEMPLATES[template]) {
                console.error(`Unknown template: ${template}`);
                usage();
            }
        } else if (!name) {
            name = args[i];
        }
    }

    if (!name) usage();

    // Sanitise project name for directory
    const safeName = name.replace(/[^a-zA-Z0-9_-]/g, "_");
    const dir = path.resolve(safeName);

    if (fs.existsSync(dir)) {
        console.error(`Directory already exists: ${dir}`);
        process.exit(1);
    }

    fs.mkdirSync(dir, { recursive: true });
    fs.mkdirSync(path.join(dir, "assets"), { recursive: true });
    fs.mkdirSync(path.join(dir, "assets", "sprites"), { recursive: true });
    fs.mkdirSync(path.join(dir, "assets", "maps"), { recursive: true });
    fs.mkdirSync(path.join(dir, "assets", "sfx"), { recursive: true });
    fs.mkdirSync(path.join(dir, "assets", "music"), { recursive: true });

    // cart.json
    const cart = {
        meta: {
            title: name,
            author: "",
            version: "1.0.0",
        },
        code: "main.js",
        sprites: {
            sheet: "assets/sprites/spritesheet.png",
        },
        maps: [],
    };
    fs.writeFileSync(path.join(dir, "cart.json"), JSON.stringify(cart, null, 4) + "\n");

    // main.js from template
    const tplPath = path.join(TEMPLATES_DIR, TEMPLATES[template]);
    const tplSrc = fs.readFileSync(tplPath, "utf-8");
    fs.writeFileSync(path.join(dir, "main.js"), tplSrc);

    console.log(`Created cart "${name}" in ${dir}/`);
    console.log(`  Template: ${template}`);
    console.log(`  Run: mvngin ${dir}/cart.json`);
}

main();
