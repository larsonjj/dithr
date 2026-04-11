// @ts-check
const { defineConfig } = require("@playwright/test");

module.exports = defineConfig({
    testDir: ".",
    testMatch: "wasm-smoke.spec.js",
    timeout: 60000,
    retries: 1,
    use: {
        headless: true,
        browserName: "chromium",
    },
});
