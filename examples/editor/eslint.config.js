const { FlatCompat } = require('@eslint/eslintrc');
const js = require('@eslint/js');
const prettierConfig = require('eslint-config-prettier');

const compat = new FlatCompat({
    baseDirectory: __dirname,
    recommendedConfig: js.configs.recommended,
});

module.exports = [
    js.configs.recommended,
    ...compat.extends('airbnb-base'),
    prettierConfig,
    {
        files: ['src/**/*.js'],
        languageOptions: {
            ecmaVersion: 2020,
            sourceType: 'module',
            globals: {
                // dithr engine globals
                cam: 'readonly',
                cart: 'readonly',
                col: 'readonly',
                evt: 'readonly',
                gfx: 'readonly',
                input: 'readonly',
                key: 'readonly',
                map: 'readonly',
                math: 'readonly',
                mouse: 'readonly',
                mus: 'readonly',
                pad: 'readonly',
                postfx: 'readonly',
                sfx: 'readonly',
                synth: 'readonly',
                sys: 'readonly',
                tween: 'readonly',
                ui: 'readonly',
            },
        },
        rules: {
            // Engine callbacks (_init, _update, _draw) are called by the runtime
            'no-underscore-dangle': 'off',
            'no-unused-vars': ['error', { varsIgnorePattern: '^_', argsIgnorePattern: '^_|^dt$' }],

            // Function declarations hoist safely
            'no-use-before-define': ['error', { functions: false }],

            // No module system in dithr carts
            'import/no-unresolved': 'off',
            'import/extensions': 'off',
            'import/prefer-default-export': 'off',

            // Common in game code
            'no-plusplus': 'off',
            'no-continue': 'off',
            'no-param-reassign': 'off',
            'no-nested-ternary': 'off',

            // Bitwise ops are common in game/graphics code
            'no-bitwise': 'off',
        },
    },
];
