#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");

const TEMPLATES_DIR = path.join(__dirname, "..", "..", "templates");

const TEMPLATES = {
    blank: { js: "blank.js", ts: "blank.ts" },
    platformer: { js: "platformer.js", ts: "platformer.ts" },
};

function usage() {
    console.log("Usage: dithrkit create <name> [options]");
    console.log("");
    console.log("Options:");
    console.log("  --template, -t <name>  Template to use (blank, platformer)");
    console.log("  --typescript, --ts     Scaffold a TypeScript project");
    console.log("  --help, -h             Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.length === 0 || args[0] === "--help" || args[0] === "-h") usage();

let name = null;
let template = "blank";
let useTypescript = false;

for (let i = 0; i < args.length; i++) {
    if (args[i] === "--template" || args[i] === "-t") {
        template = args[++i];
        if (!TEMPLATES[template]) {
            console.error(`Unknown template: ${template}`);
            usage();
        }
    } else if (args[i] === "--typescript" || args[i] === "--ts") {
        useTypescript = true;
    } else if (!name) {
        name = args[i];
    }
}

if (!name) usage();

const safeName = name.replace(/[^a-zA-Z0-9_-]/g, "_");
const dir = path.resolve(safeName);

if (fs.existsSync(dir)) {
    console.error(`Directory already exists: ${dir}`);
    process.exit(1);
}

// Create directory structure
fs.mkdirSync(dir, { recursive: true });
fs.mkdirSync(path.join(dir, "src"), { recursive: true });
fs.mkdirSync(path.join(dir, "assets", "sprites"), { recursive: true });
fs.mkdirSync(path.join(dir, "assets", "maps"), { recursive: true });
fs.mkdirSync(path.join(dir, "assets", "sfx"), { recursive: true });
fs.mkdirSync(path.join(dir, "assets", "music"), { recursive: true });

const srcExt = useTypescript ? "ts" : "js";
const codeEntry = useTypescript ? "dist/main.js" : "src/main.js";

// cart.json
const cart = {
    title: name,
    author: "",
    version: "1.0.0",
    description: "",
    display: { width: 320, height: 180, scale: 3 },
    timing: { fps: 60 },
    sprites: { tileW: 8, tileH: 8 },
    audio: { channels: 8 },
    input: {
        default_mappings: {
            left: ["KEY_LEFT", "KEY_A", "PAD_LEFT"],
            right: ["KEY_RIGHT", "KEY_D", "PAD_RIGHT"],
            up: ["KEY_UP", "KEY_W", "PAD_UP"],
            down: ["KEY_DOWN", "KEY_S", "PAD_DOWN"],
            jump: ["KEY_Z", "KEY_SPACE", "PAD_A"],
            action: ["KEY_X", "PAD_B"],
        },
    },
    code: codeEntry,
    maps: [],
};
fs.writeFileSync(path.join(dir, "cart.json"), JSON.stringify(cart, null, 4) + "\n");

// src/main.{js,ts} from template
const tplFile = useTypescript ? TEMPLATES[template].ts : TEMPLATES[template].js;
const tplSrc = fs.readFileSync(path.join(TEMPLATES_DIR, tplFile), "utf-8");
fs.writeFileSync(path.join(dir, "src", `main.${srcExt}`), tplSrc);

// TypeScript-specific files
if (useTypescript) {
    // tsconfig.json — extends the shared SDK config
    const tsconfig = {
        extends: "@dithrkit/sdk/configs/tsconfig.base.json",
        include: ["src/**/*.ts"],
    };
    fs.writeFileSync(path.join(dir, "tsconfig.json"), JSON.stringify(tsconfig, null, 4) + "\n");
}

// .gitignore
const gitignoreLines = ["node_modules/", "build/", "*.baked.json", ".DS_Store"];
if (useTypescript) gitignoreLines.push("dist/");
gitignoreLines.push("");
fs.writeFileSync(path.join(dir, ".gitignore"), gitignoreLines.join("\n"));

// README.md
fs.writeFileSync(
    path.join(dir, "README.md"),
    [
        `# ${name}`,
        "",
        `A game made with [dithr](https://github.com/dithrkit/dithr).`,
        "",
        "## Getting Started",
        "",
        "```bash",
        "npm install",
        useTypescript ? "dithrkit build" : "",
        "dithrkit run",
        "```",
        "",
    ]
        .filter(Boolean)
        .join("\n"),
);

// package.json
const pkg = {
    name: safeName,
    version: "1.0.0",
    private: true,
    description: `${name} — dithr cart`,
    scripts: {
        start: "dithrkit run",
        build: useTypescript ? "dithrkit build" : undefined,
        serve: "dithrkit serve",
        watch: "dithrkit watch",
    },
    devDependencies: {
        "@dithrkit/sdk": "^0.1.0",
    },
};
// Remove undefined scripts
Object.keys(pkg.scripts).forEach((k) => {
    if (pkg.scripts[k] === undefined) delete pkg.scripts[k];
});
fs.writeFileSync(path.join(dir, "package.json"), JSON.stringify(pkg, null, 4) + "\n");

// eslint.config.js — imports shared config from SDK
if (useTypescript) {
    fs.writeFileSync(
        path.join(dir, "eslint.config.js"),
        [
            'const dithrConfig = require("@dithrkit/sdk/configs/eslint");',
            "",
            "module.exports = [",
            "    ...dithrConfig.ts,",
            "];",
            "",
        ].join("\n"),
    );
} else {
    fs.writeFileSync(
        path.join(dir, "eslint.config.js"),
        [
            'const dithrConfig = require("@dithrkit/sdk/configs/eslint");',
            "",
            "module.exports = [",
            "    ...dithrConfig.js,",
            "];",
            "",
        ].join("\n"),
    );
}

// .editorconfig
fs.writeFileSync(
    path.join(dir, ".editorconfig"),
    [
        "root = true",
        "",
        "[*]",
        "charset = utf-8",
        "end_of_line = lf",
        "insert_final_newline = true",
        "trim_trailing_whitespace = true",
        "indent_style = space",
        "indent_size = 4",
        "",
        "[*.json]",
        "indent_size = 2",
        "",
        "[*.md]",
        "trim_trailing_whitespace = false",
        "",
    ].join("\n"),
);

// .prettierrc
const prettierrc = {
    printWidth: 100,
    tabWidth: 4,
    useTabs: false,
    semi: true,
    singleQuote: true,
    trailingComma: "all",
    bracketSpacing: true,
};
fs.writeFileSync(path.join(dir, ".prettierrc"), JSON.stringify(prettierrc, null, 4) + "\n");

console.log(`Created dithr project "${name}" in ${dir}/`);
console.log("");
console.log("Next steps:");
console.log(`  cd ${safeName}`);
console.log("  npm install");
if (useTypescript) {
    console.log("  dithrkit build");
}
console.log("  dithrkit run");
