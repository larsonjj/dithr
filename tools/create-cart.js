#!/usr/bin/env node
// tools/create-cart.js — Project scaffolding tool for dithr carts
// Usage: node create-cart.js <name> [--template blank|platformer] [--typescript]

"use strict";

const fs = require("fs");
const path = require("path");

const TEMPLATES_DIR = path.join(__dirname, "templates");

const TEMPLATES = {
    blank: { js: "blank.js", ts: "blank.ts" },
    platformer: { js: "platformer.js", ts: "platformer.ts" },
};

function usage() {
    console.log("Usage: node create-cart.js <name> [--template <template>] [--typescript]");
    console.log("");
    console.log("Templates:");
    for (const key of Object.keys(TEMPLATES)) {
        console.log(`  ${key}`);
    }
    console.log("");
    console.log("Options:");
    console.log("  --typescript, --ts   Scaffold a TypeScript project");
    process.exit(1);
}

function main() {
    const args = process.argv.slice(2);
    if (args.length === 0) usage();

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

    // Sanitise project name for directory
    const safeName = name.replace(/[^a-zA-Z0-9_-]/g, "_");
    const dir = path.resolve(safeName);

    if (fs.existsSync(dir)) {
        console.error(`Directory already exists: ${dir}`);
        process.exit(1);
    }

    fs.mkdirSync(dir, { recursive: true });
    fs.mkdirSync(path.join(dir, "src"), { recursive: true });
    fs.mkdirSync(path.join(dir, "assets"), { recursive: true });
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
        display: {
            width: 320,
            height: 180,
            scale: 3,
        },
        timing: {
            fps: 60,
        },
        sprites: {
            tileW: 8,
            tileH: 8,
        },
        audio: {
            channels: 8,
        },
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
    };
    fs.writeFileSync(path.join(dir, "cart.json"), JSON.stringify(cart, null, 4) + "\n");

    // src/main.{js,ts} from template
    const tplFile = useTypescript ? TEMPLATES[template].ts : TEMPLATES[template].js;
    const tplPath = path.join(TEMPLATES_DIR, tplFile);
    const tplSrc = fs.readFileSync(tplPath, "utf-8");
    fs.writeFileSync(path.join(dir, "src", `main.${srcExt}`), tplSrc);

    // TypeScript-specific files
    if (useTypescript) {
        fs.mkdirSync(path.join(dir, "types"), { recursive: true });

        // Copy dithr.d.ts
        const dtsPath = path.join(TEMPLATES_DIR, "dithr.d.ts");
        fs.copyFileSync(dtsPath, path.join(dir, "types", "dithr.d.ts"));

        // tsconfig.json
        const tsconfig = {
            compilerOptions: {
                target: "ES2020",
                module: "nodenext",
                moduleResolution: "nodenext",
                rootDir: "src",
                strict: true,
                noEmit: true,
                esModuleInterop: true,
                skipLibCheck: true,
            },
            include: ["src/**/*.ts", "types/**/*.d.ts"],
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
            `A game made with [dithr](https://github.com/example/dithr).`,
            "",
            "## Run",
            "",
            "```bash",
            `dithr cart.json`,
            "```",
            "",
        ].join("\n"),
    );

    // package.json
    const pkg = {
        name: safeName,
        version: "1.0.0",
        private: true,
        description: `${name} — dithr cart`,
        scripts: {
            start: "dithr cart.json",
            export: "node ../tools/cart-export.js cart.json",
            lint: "eslint src/",
            "lint:fix": "eslint --fix src/",
            format: "prettier --write src/",
            "format:check": "prettier --check src/",
        },
        devDependencies: {
            "@eslint/js": "^9.0.0",
            eslint: "^9.0.0",
            "eslint-config-prettier": "^9.0.0",
            prettier: "^3.0.0",
        },
    };
    if (useTypescript) {
        pkg.scripts.build =
            "esbuild src/main.ts --bundle --format=esm --target=es2020 --outfile=dist/main.js";
        pkg.scripts.typecheck = "tsc --noEmit";
        pkg.devDependencies.esbuild = "^0.24.0";
        pkg.devDependencies.typescript = "^5.0.0";
        pkg.devDependencies["typescript-eslint"] = "^8.0.0";
    }
    fs.writeFileSync(path.join(dir, "package.json"), JSON.stringify(pkg, null, 4) + "\n");

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

    // eslint.config.js (flat config — @eslint/js recommended + Prettier)
    if (useTypescript) {
        fs.writeFileSync(
            path.join(dir, "eslint.config.js"),
            [
                'const js = require("@eslint/js");',
                'const tseslint = require("typescript-eslint");',
                'const prettierConfig = require("eslint-config-prettier");',
                "",
                "module.exports = [",
                "    js.configs.recommended,",
                "    ...tseslint.configs.recommended,",
                "    prettierConfig,",
                "    {",
                '        files: ["src/**/*.ts"],',
                "        languageOptions: {",
                "            ecmaVersion: 2020,",
                '            sourceType: "module",',
                "            globals: {",
                "                // dithr engine globals (typed via types/dithr.d.ts)",
                '                cam: "readonly",',
                '                cart: "readonly",',
                '                col: "readonly",',
                '                evt: "readonly",',
                '                gfx: "readonly",',
                '                input: "readonly",',
                '                key: "readonly",',
                '                map: "readonly",',
                '                math: "readonly",',
                '                mouse: "readonly",',
                '                mus: "readonly",',
                '                pad: "readonly",',
                '                postfx: "readonly",',
                '                sfx: "readonly",',
                '                synth: "readonly",',
                '                sys: "readonly",',
                '                touch: "readonly",',
                '                tween: "readonly",',
                '                ui: "readonly",',
                "            },",
                "        },",
                "        rules: {",
                "",
                "            // Quality rules",
                '            "no-var": "error",',
                '            "prefer-const": "error",',
                '            "prefer-template": "warn",',
                '            "camelcase": ["error", { "properties": "never" }],',
                '            "eqeqeq": ["error", "always"],',
                "",
                "            // Engine callbacks use underscore prefix (_init, _update, _draw)",
                '            "no-underscore-dangle": "off",',
                "",
                "            // Allow ++ in for loops (common in game code)",
                '            "no-plusplus": "off",',
                "",
                "            // Bitwise ops are common in game/graphics code",
                '            "no-bitwise": "off",',
                "",
                "            // Engine callbacks (_init, _update, _draw) and unused params prefixed with _",
                '            "@typescript-eslint/no-unused-vars": ["error", { "varsIgnorePattern": "^_", "argsIgnorePattern": "^_" }],',
                "        },",
                "    },",
                "];",
                "",
            ].join("\n"),
        );
    } else {
        fs.writeFileSync(
            path.join(dir, "eslint.config.js"),
            [
                'const js = require("@eslint/js");',
                'const prettierConfig = require("eslint-config-prettier");',
                "",
                "module.exports = [",
                "    js.configs.recommended,",
                "    prettierConfig,",
                "    {",
                '        files: ["src/**/*.js"],',
                "        languageOptions: {",
                "            ecmaVersion: 2020,",
                '            sourceType: "script",',
                "            globals: {",
                "                // dithr engine globals",
                '                cam: "readonly",',
                '                cart: "readonly",',
                '                col: "readonly",',
                '                evt: "readonly",',
                '                gfx: "readonly",',
                '                input: "readonly",',
                '                key: "readonly",',
                '                map: "readonly",',
                '                math: "readonly",',
                '                mouse: "readonly",',
                '                mus: "readonly",',
                '                pad: "readonly",',
                '                postfx: "readonly",',
                '                sfx: "readonly",',
                '                synth: "readonly",',
                '                sys: "readonly",',
                '                touch: "readonly",',
                '                tween: "readonly",',
                '                ui: "readonly",',
                "            },",
                "        },",
                "        rules: {",
                "",
                "            // Quality rules",
                '            "no-var": "error",',
                '            "prefer-const": "error",',
                '            "prefer-template": "warn",',
                '            "camelcase": ["error", { "properties": "never" }],',
                '            "eqeqeq": ["error", "always"],',
                "",
                "            // Engine callbacks use underscore prefix (_init, _update, _draw)",
                '            "no-underscore-dangle": "off",',
                "",
                "            // Allow ++ in for loops (common in game code)",
                '            "no-plusplus": ["error", { "allowForLoopAfterthoughts": true }],',
                "",
                "            // Bitwise ops are common in game/graphics code",
                '            "no-bitwise": "off",',
                "        },",
                "    },",
                "];",
                "",
            ].join("\n"),
        );
    }

    console.log(`Created cart "${name}" in ${dir}/`);
    console.log(`  Template: ${template}${useTypescript ? " (TypeScript)" : ""}`);
    console.log(`  Run: dithr ${dir}/cart.json`);
    if (useTypescript) {
        console.log(`  Build: cd ${dir} && npm ci && npm run build`);
    }
}

main();
