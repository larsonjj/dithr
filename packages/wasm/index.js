"use strict";

const path = require("node:path");

module.exports = {
    jsPath: path.join(__dirname, "bin", "dithr.js"),
    wasmPath: path.join(__dirname, "bin", "dithr.wasm"),
};
