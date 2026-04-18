"use strict";

function levenshtein(a, b) {
    const m = a.length;
    const n = b.length;
    const dp = Array.from({ length: m + 1 }, () => new Array(n + 1).fill(0));
    for (let i = 0; i <= m; i++) dp[i][0] = i;
    for (let j = 0; j <= n; j++) dp[0][j] = j;
    for (let i = 1; i <= m; i++) {
        for (let j = 1; j <= n; j++) {
            dp[i][j] = Math.min(
                dp[i - 1][j] + 1,
                dp[i][j - 1] + 1,
                dp[i - 1][j - 1] + (a[i - 1] !== b[j - 1] ? 1 : 0),
            );
        }
    }
    return dp[m][n];
}

function didYouMean(input, candidates) {
    let best = null;
    let bestDist = Infinity;
    for (const c of candidates) {
        const d = levenshtein(input.toLowerCase(), c.toLowerCase());
        if (d < bestDist) {
            bestDist = d;
            best = c;
        }
    }
    // Only suggest if edit distance is at most 3
    return bestDist <= 3 ? best : null;
}

const COMMANDS = {
    create: "./commands/create.js",
    build: "./commands/build.js",
    run: "./commands/run.js",
    dev: "./commands/dev.js",
    serve: "./commands/serve.js",
    watch: "./commands/watch.js",
    export: "./commands/export.js",
};

function printUsage() {
    console.log("Usage: dithrkit <command> [options]");
    console.log("");
    console.log("Commands:");
    console.log("  create <name>     Scaffold a new dithr project");
    console.log("  build             Bundle cart source via esbuild");
    console.log("  run               Launch the native dithr runtime");
    console.log("  dev               Build --watch + run (native dev loop)");
    console.log("  serve [port]      Start a WASM dev server");
    console.log("  watch [port]      Watch + live-reload WASM dev server");
    console.log("  export            Export cart for distribution");
    console.log("");
    console.log("Options:");
    console.log("  --help, -h        Show help");
    console.log("  --version, -v     Show version");
    process.exit(1);
}

const args = process.argv.slice(2);

if (args.length === 0 || args[0] === "--help" || args[0] === "-h") {
    printUsage();
}

if (args[0] === "--version" || args[0] === "-v") {
    const pkg = require("../package.json");
    console.log(pkg.version);
    process.exit(0);
}

const command = args[0];
const handler = COMMANDS[command];

if (!handler) {
    console.error(`Unknown command: ${command}`);
    const suggestion = didYouMean(command, Object.keys(COMMANDS));
    if (suggestion) {
        console.error(`Did you mean "${suggestion}"?`);
    }
    console.error('Run "dithrkit --help" for usage.');
    process.exit(1);
}

// Remove command name from argv so sub-commands see their own args
process.argv.splice(2, 1);

require(handler);
