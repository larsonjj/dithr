const js = require("@eslint/js");
const prettierConfig = require("eslint-config-prettier");

module.exports = [
    js.configs.recommended,
    prettierConfig,
    {
        files: ["src/**/*.js"],
        languageOptions: {
            ecmaVersion: 2020,
            sourceType: "script",
            globals: {
                // dithr engine globals
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
                tween: "readonly",
                touch: "readonly",
                ui: "readonly",
            },
        },
        rules: {
            // Airbnb-equivalent quality rules
            "no-var": "error",
            "prefer-const": "error",
            "prefer-template": "warn",
            "camelcase": ["error", { properties: "never" }],
            "eqeqeq": ["error", "always"],

            // Engine callbacks (_init, _update, _draw) are called by the runtime
            "no-underscore-dangle": "off",
            "no-unused-vars": ["error", { "varsIgnorePattern": "^_", "argsIgnorePattern": "^_|^dt$" }],

            // Function declarations hoist safely
            "no-use-before-define": ["error", { "functions": false }],

            // Common in game code
            "no-plusplus": "off",
            "no-continue": "off",
            "no-param-reassign": "off",
            "no-nested-ternary": "off",

            // Bitwise ops are common in game/graphics code
            "no-bitwise": "off",
        },
    },
];
