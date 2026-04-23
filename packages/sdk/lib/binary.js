"use strict";

const PLATFORMS = {
    "win32-x64": "@dithrkit/win32-x64",
    "linux-x64": "@dithrkit/linux-x64",
    "linux-arm64": "@dithrkit/linux-arm64",
    "darwin-arm64": "@dithrkit/darwin-arm64",
};

/**
 * Resolve the path to the native dithr binary for the current platform.
 * @returns {string} Absolute path to the binary.
 */
function getBinaryPath() {
    const key = `${process.platform}-${process.arch}`;
    const pkg = PLATFORMS[key];

    if (!pkg) {
        const supported = Object.keys(PLATFORMS).join(", ");
        console.error(`Unsupported platform: ${key}`);
        console.error(`dithr supports: ${supported}`);
        process.exit(1);
    }

    try {
        return require(pkg);
    } catch {
        console.error(`Platform package ${pkg} is not installed.`);
        console.error(`Run: npm install ${pkg}`);
        process.exit(1);
    }
}

module.exports = { getBinaryPath, PLATFORMS };
