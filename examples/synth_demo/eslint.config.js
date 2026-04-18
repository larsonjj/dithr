const js = require("@eslint/js");
const tseslint = require("typescript-eslint");
const prettierConfig = require("eslint-config-prettier");

module.exports = [
    js.configs.recommended,
    ...tseslint.configs.recommended,
    prettierConfig,
    {
        files: ["src/**/*.ts"],
        languageOptions: {
            ecmaVersion: 2020,
            sourceType: "module",
            globals: {
                // dithr engine globals (typed via types/dithr.d.ts)
                cam: "readonly",
                cart: "readonly",
                col: "readonly",
                evt: "readonly",
                gfx: "readonly",
                input: "readonly",
                key: "readonly",
                map: "readonly",
                math: "readonly",
                mouse: "readonly",
                mus: "readonly",
                pad: "readonly",
                postfx: "readonly",
                sfx: "readonly",
                synth: "readonly",
                sys: "readonly",
                touch: "readonly",
                tween: "readonly",
                ui: "readonly",
            },
        },
        rules: {

            // Quality rules
            "no-var": "error",
            "prefer-const": "error",
            "prefer-template": "warn",
            "camelcase": ["error", { "properties": "never" }],
            "eqeqeq": ["error", "always"],

            // Engine callbacks use underscore prefix (_init, _update, _draw)
            "no-underscore-dangle": "off",

            // Allow ++ in for loops (common in game code)
            "no-plusplus": "off",

            // Bitwise ops are common in game/graphics code
            "no-bitwise": "off",

            // Converted code uses `any` for engine compatibility
            "@typescript-eslint/no-explicit-any": "off",

            // Engine callbacks (_init, _update, _draw) and unused params prefixed with _
            "@typescript-eslint/no-unused-vars": ["error", { "varsIgnorePattern": "^_", "argsIgnorePattern": "^_" }],
        },
    },
];
