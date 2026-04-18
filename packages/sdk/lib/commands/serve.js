#!/usr/bin/env node
"use strict";

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const zlib = require("node:zlib");
const { execSync } = require("node:child_process");

function usage() {
    console.log("Usage: dithrkit serve [port] [options]");
    console.log("");
    console.log("Start a local WASM dev server for the current cart.");
    console.log("Serves from the build/web/ directory (created by dithrkit export --web).");
    console.log("");
    console.log("Options:");
    console.log("  --dir <path>  Directory to serve (default: build/web)");
    console.log("  --help, -h    Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h")) usage();

let port = 8080;
let serveDir = path.resolve("build", "web");

for (let i = 0; i < args.length; i++) {
    if (args[i] === "--dir") {
        serveDir = path.resolve(args[++i]);
    } else if (!isNaN(parseInt(args[i], 10))) {
        port = parseInt(args[i], 10);
    }
}

const MIME_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".wasm": "application/wasm",
    ".data": "application/octet-stream",
    ".json": "application/json; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".svg": "image/svg+xml",
    ".txt": "text/plain; charset=utf-8",
};

function getMime(filePath) {
    return MIME_TYPES[path.extname(filePath).toLowerCase()] || "application/octet-stream";
}

function pickEncoding(acceptEncoding) {
    if (acceptEncoding.includes("br")) return "br";
    if (acceptEncoding.includes("gzip")) return "gzip";
    return null;
}

function compressStream(encoding) {
    if (encoding === "br") return zlib.createBrotliCompress();
    if (encoding === "gzip") return zlib.createGzip();
    return null;
}

const server = http.createServer((req, res) => {
    const url = new URL(req.url, `http://localhost:${port}`);
    let relPath = decodeURIComponent(url.pathname);

    if (relPath === "/") relPath = "/index.html";

    // Prevent path traversal
    const filePath = path.join(serveDir, relPath);
    if (!filePath.startsWith(serveDir)) {
        res.writeHead(403);
        res.end("Forbidden");
        return;
    }

    fs.stat(filePath, (err, stat) => {
        if (err || !stat.isFile()) {
            res.writeHead(404);
            res.end("Not Found");
            return;
        }

        const mime = getMime(filePath);
        const accept = req.headers["accept-encoding"] || "";
        const encoding = pickEncoding(accept);

        res.setHeader("Content-Type", mime);
        res.setHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        res.setHeader("Pragma", "no-cache");
        res.setHeader("Expires", "0");
        const stream = fs.createReadStream(filePath);
        const comp = compressStream(encoding);

        if (comp) {
            res.setHeader("Content-Encoding", encoding);
            res.writeHead(200);
            stream.pipe(comp).pipe(res);
        } else {
            res.setHeader("Content-Length", stat.size);
            res.writeHead(200);
            stream.pipe(res);
        }
    });
});

function startServer() {
    server.listen(port, () => {
        const url = `http://localhost:${port}`;
        console.log(`Serving ${serveDir}`);
        console.log(`  → ${url}`);
        console.log("Press Ctrl+C to stop.\n");

        try {
            const cmd =
                process.platform === "win32"
                    ? `start ${url}`
                    : process.platform === "darwin"
                      ? `open ${url}`
                      : `xdg-open ${url}`;
            execSync(cmd, { stdio: "ignore" });
        } catch {
            /* ignore */
        }
    });

    server.on("error", (err) => {
        if (err.code === "EADDRINUSE") {
            console.log(`Port ${port} in use — killing previous process…`);
            try {
                if (process.platform === "win32") {
                    const out = execSync(`netstat -ano | findstr :${port} | findstr LISTENING`, {
                        encoding: "utf8",
                    });
                    const pids = new Set(
                        out
                            .trim()
                            .split(/\r?\n/)
                            .map((l) => l.trim().split(/\s+/).pop()),
                    );
                    for (const pid of pids) {
                        execSync(`taskkill /F /PID ${pid}`, { stdio: "ignore" });
                    }
                } else {
                    execSync(`fuser -k ${port}/tcp`, { stdio: "ignore" });
                }
            } catch {
                /* ignore */
            }
            setTimeout(startServer, 500);
        } else {
            throw err;
        }
    });
}

if (!fs.existsSync(serveDir)) {
    console.error(`Serve directory not found: ${serveDir}`);
    console.error('Run "dithrkit export --web" first to create a web build.');
    process.exit(1);
}

startServer();
