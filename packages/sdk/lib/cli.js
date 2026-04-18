"use strict";

const COMMANDS = {
    create: "./commands/create.js",
    build: "./commands/build.js",
    run: "./commands/run.js",
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
    console.error('Run "dithrkit --help" for usage.');
    process.exit(1);
}

// Remove command name from argv so sub-commands see their own args
process.argv.splice(2, 1);

require(handler);
