"use strict";

const js = require("@eslint/js");
const prettierConfig = require("eslint-config-prettier");

const DITHR_GLOBALS = {
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
};

const BASE_RULES = {
    "no-var": "error",
    "prefer-const": "error",
    "prefer-template": "warn",
    camelcase: ["error", { properties: "never" }],
    eqeqeq: ["error", "always"],
    "no-underscore-dangle": "off",
    "no-plusplus": "off",
    "no-bitwise": "off",
};

// JavaScript config
const jsConfig = [
    js.configs.recommended,
    prettierConfig,
    {
        files: ["src/**/*.js"],
        languageOptions: {
            ecmaVersion: 2020,
            sourceType: "script",
            globals: DITHR_GLOBALS,
        },
        rules: {
            ...BASE_RULES,
            "no-plusplus": ["error", { allowForLoopAfterthoughts: true }],
        },
    },
];

// TypeScript config — lazily loads typescript-eslint
let _tsConfig = null;
Object.defineProperty(module.exports, "ts", {
    get() {
        if (_tsConfig) return _tsConfig;
        let tseslint;
        try {
            tseslint = require("typescript-eslint");
        } catch {
            throw new Error(
                "typescript-eslint is required for TypeScript linting. " +
                    "Install it: npm install -D typescript-eslint",
            );
        }
        _tsConfig = [
            js.configs.recommended,
            ...tseslint.configs.recommended,
            prettierConfig,
            {
                files: ["src/**/*.ts"],
                languageOptions: {
                    ecmaVersion: 2020,
                    sourceType: "module",
                    globals: DITHR_GLOBALS,
                },
                rules: {
                    ...BASE_RULES,
                    "@typescript-eslint/no-unused-vars": [
                        "error",
                        { varsIgnorePattern: "^_", argsIgnorePattern: "^_" },
                    ],
                },
            },
        ];
        return _tsConfig;
    },
    enumerable: true,
});

module.exports.js = jsConfig;
