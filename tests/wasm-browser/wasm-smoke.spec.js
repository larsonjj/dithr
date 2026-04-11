/**
 * @file  wasm-smoke.spec.js
 * @brief Playwright smoke test — loads the WASM build in a headless browser
 *        and verifies the canvas renders at least one frame.
 *
 * Expects the WASM build output in build/wasm/ (served by tools/serve-wasm.js).
 */

const { test, expect } = require("@playwright/test");

const PORT = 8091;
const BASE_URL = `http://localhost:${PORT}`;

let server;

test.beforeAll(async () => {
    const { spawn } = require("node:child_process");
    const path = require("node:path");

    server = spawn("node", [path.resolve(__dirname, "../../tools/serve-wasm.js"), String(PORT)], {
        stdio: "pipe",
        cwd: path.resolve(__dirname, "../.."),
    });

    // Wait for the server to start listening
    await new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error("Server start timeout")), 10000);
        server.stdout.on("data", (data) => {
            if (data.toString().includes("listening") || data.toString().includes(String(PORT))) {
                clearTimeout(timeout);
                resolve();
            }
        });
        // Also resolve after a short delay in case there's no stdout output
        setTimeout(() => {
            clearTimeout(timeout);
            resolve();
        }, 2000);
    });
});

test.afterAll(async () => {
    if (server) {
        server.kill("SIGTERM");
    }
});

test("WASM build loads and renders a frame", async ({ page }) => {
    // Collect console errors
    const errors = [];
    page.on("pageerror", (err) => errors.push(err.message));

    await page.goto(BASE_URL, { waitUntil: "domcontentloaded", timeout: 30000 });

    // Wait for the canvas element to appear
    const canvas = page.locator("canvas#canvas");
    await expect(canvas).toBeVisible({ timeout: 15000 });

    // Wait for Emscripten module to initialise — the splash overlay hides
    // when loading completes. Give it time for WASM compilation + data fetch.
    await page.waitForFunction(
        () => {
            const splash = document.getElementById("splash");
            return splash && splash.classList.contains("hidden");
        },
        { timeout: 30000 },
    );

    // Verify no JS errors occurred during load
    expect(errors).toEqual([]);

    // Verify the canvas has non-zero dimensions (rendering initialised)
    const box = await canvas.boundingBox();
    expect(box.width).toBeGreaterThan(0);
    expect(box.height).toBeGreaterThan(0);
});
