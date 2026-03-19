#!/usr/bin/env node
/**
 * @file  serve-wasm.js
 * @brief Zero-dependency static file server for the WASM build output.
 *        Supports gzip and Brotli compression via Accept-Encoding.
 *
 * Usage:  node tools/serve-wasm.js [port]
 *         Default port is 8080.
 */

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const zlib = require("node:zlib");
const { execSync } = require("node:child_process");

const PORT = parseInt(process.argv[2], 10) || 8080;
const SERVE_DIR = path.resolve(__dirname, "..", "build", "wasm");

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
    const url = new URL(req.url, `http://localhost:${PORT}`);
    let relPath = decodeURIComponent(url.pathname);

    // Default to index → dithr.html
    if (relPath === "/") relPath = "/dithr.html";

    // Prevent path traversal
    const filePath = path.join(SERVE_DIR, relPath);
    if (!filePath.startsWith(SERVE_DIR)) {
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
        res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
        res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");

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
    server.listen(PORT, () => {
        const url = `http://localhost:${PORT}`;
        console.log(`Serving ${SERVE_DIR}`);
        console.log(`  → ${url}`);
        console.log("Press Ctrl+C to stop.\n");

        // Open browser
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
            console.log(`Port ${PORT} in use — killing previous process…`);
            try {
                if (process.platform === "win32") {
                    const out = execSync(`netstat -ano | findstr :${PORT} | findstr LISTENING`, {
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
                    execSync(`fuser -k ${PORT}/tcp`, { stdio: "ignore" });
                }
            } catch {
                /* ignore — process may have already exited */
            }
            setTimeout(startServer, 500);
        } else {
            throw err;
        }
    });
}

startServer();
