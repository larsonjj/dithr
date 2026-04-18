"use strict";

const fs = require("node:fs");
const path = require("node:path");

/**
 * Pack a directory tree into the Emscripten preload data format.
 *
 * Emscripten's file_packager.py produces two files:
 *   - .data  — concatenated raw file contents
 *   - .js    — metadata (Remote package, file list with offsets/sizes)
 *
 * This module produces a compatible .data file and returns metadata
 * that the export command uses to patch the Emscripten glue JS.
 *
 * The .data file is simply all files concatenated. Metadata records
 * each file's virtual path, byte offset, and byte length.
 *
 * @param {string} dir    Directory to pack (e.g. the cart directory)
 * @param {string} mount  Virtual mount point (e.g. "/cart")
 * @returns {{ dataBuffer: Buffer, files: Array<{filename: string, start: number, end: number}> }}
 */
function packDirectory(dir, mount) {
    const files = [];
    const chunks = [];
    let offset = 0;

    function walk(currentDir, relPrefix) {
        const entries = fs.readdirSync(currentDir, { withFileTypes: true });
        // Sort for deterministic output
        entries.sort((a, b) => a.name.localeCompare(b.name));

        for (const entry of entries) {
            const fullPath = path.join(currentDir, entry.name);
            const relPath = relPrefix ? `${relPrefix}/${entry.name}` : entry.name;

            if (entry.isDirectory()) {
                walk(fullPath, relPath);
            } else if (entry.isFile()) {
                const data = fs.readFileSync(fullPath);
                const virtualPath = `${mount}/${relPath}`;

                files.push({
                    filename: virtualPath,
                    start: offset,
                    end: offset + data.length,
                });

                chunks.push(data);
                offset += data.length;
            }
        }
    }

    walk(dir, "");

    const dataBuffer = Buffer.concat(chunks);
    return { dataBuffer, files };
}

/**
 * Generate the Emscripten-compatible data package loader JS.
 *
 * This produces a self-contained script that:
 * 1. Fetches the .data file
 * 2. Registers each file into the Emscripten VFS at the correct path
 *
 * @param {string} dataFilename  Name of the .data file (e.g. "dithr.data")
 * @param {Array<{filename: string, start: number, end: number}>} files
 * @returns {string}  JavaScript source
 */
function generatePackageLoader(dataFilename, files) {
    // Build the directory creation list
    const dirs = new Set();
    for (const f of files) {
        const parts = f.filename.split("/");
        for (let i = 1; i < parts.length; i++) {
            dirs.add(parts.slice(0, i).join("/"));
        }
    }

    const dirList = [...dirs].sort();
    const fileEntries = files.map(
        (f) => `{filename:"${f.filename}",start:${f.start},end:${f.end}}`,
    );

    return [
        "var Module = typeof Module != 'undefined' ? Module : {};",
        "",
        "(function() {",
        `  var PACKAGE_PATH = '';`,
        `  var PACKAGE_NAME = '${dataFilename}';`,
        `  var REMOTE_PACKAGE_BASE = '${dataFilename}';`,
        "",
        "  var PACKAGE_INFO = {",
        `    files: [${fileEntries.join(",")}]`,
        "  };",
        "",
        "  function assert(check, msg) {",
        "    if (!check) throw msg + new Error().stack;",
        "  }",
        "",
        "  function processPackageData(arrayBuffer) {",
        "    assert(arrayBuffer, 'Loading data file failed.');",
        "    assert(arrayBuffer.constructor.name === ArrayBuffer.name, 'bad input');",
        "    var byteArray = new Uint8Array(arrayBuffer);",
        "    var curr;",
        "",
        "    // Create directories",
        ...dirList.map(
            (d) =>
                `    Module['FS_createPath']('${path.dirname("/" + d).replace(/\\/g, "/")}', '${path.basename(d)}', true, true);`,
        ),
        "",
        "    // Create files",
        "    PACKAGE_INFO.files.forEach(function(fileInfo) {",
        "      var data = byteArray.subarray(fileInfo.start, fileInfo.end);",
        "      var dir = fileInfo.filename.substring(0, fileInfo.filename.lastIndexOf('/'));",
        "      Module['FS_createDataFile'](dir, fileInfo.filename.split('/').pop(), data, true, true, true);",
        "    });",
        "  }",
        "",
        "  var fetchedCallback = null;",
        "  var fetched = Module['getPreloadedPackage'] ? Module['getPreloadedPackage'](REMOTE_PACKAGE_BASE, " +
            (files.length > 0 ? files[files.length - 1].end : 0) +
            ") : null;",
        "",
        "  if (!fetched) {",
        "    fetchedCallback = function(data) { fetched = data; };",
        "    var url = PACKAGE_PATH + REMOTE_PACKAGE_BASE;",
        "    fetch(url).then(function(response) {",
        "      if (!response.ok) throw 'failed to load ' + url;",
        "      return response.arrayBuffer();",
        "    }).then(function(data) {",
        "      if (fetchedCallback) { fetchedCallback(data); fetchedCallback = null; }",
        "      else { fetched = data; }",
        "    });",
        "  }",
        "",
        "  Module['calledRun'] = false;",
        "  function runWithFS() {",
        "    processPackageData(fetched);",
        "  }",
        "",
        "  if (Module['calledRun']) {",
        "    runWithFS();",
        "  } else {",
        "    var old = Module['onRuntimeInitialized'];",
        "    Module['onRuntimeInitialized'] = function() {",
        "      if (old) old();",
        "      runWithFS();",
        "    };",
        "  }",
        "})();",
        "",
    ].join("\n");
}

module.exports = { packDirectory, generatePackageLoader };
