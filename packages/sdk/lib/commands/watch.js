#!/usr/bin/env node
"use strict";

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const zlib = require("node:zlib");
const { execSync, spawn } = require("node:child_process");

function usage() {
    console.log("Usage: dithrkit watch [port] [options]");
    console.log("");
    console.log("Watch cart source directory, auto-rebuild, and serve with live reload.");
    console.log("");
    console.log("Options:");
    console.log("  --port, -p <port>  Port to serve on (default: 8080)");
    console.log("  --help, -h         Show help");
    process.exit(1);
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h")) usage();

let port = 8080;
for (let i = 0; i < args.length; i++) {
    if (args[i] === "--port" || args[i] === "-p") {
        port = parseInt(args[++i], 10);
    } else if (!isNaN(parseInt(args[i], 10))) {
        port = parseInt(args[i], 10);
    }
}

const cartJson = path.resolve("cart.json");
if (!fs.existsSync(cartJson)) {
    console.error("No cart.json found in current directory.");
    process.exit(1);
}

const cart = JSON.parse(fs.readFileSync(cartJson, "utf-8"));
const cartDir = path.dirname(cartJson);
const buildDir = path.resolve("build", "web");

// Determine JS entry point for hot-reload
let entryJs = null;
if (cart.code) {
    entryJs = cart.code.replace(/\\/g, "/");
}

/* ------------------------------------------------------------------ */
/*  SSE live-reload                                                    */
/* ------------------------------------------------------------------ */

/** @type {Set<import("node:http").ServerResponse>} */
const sseClients = new Set();

const RELOAD_SNIPPET = `
<script>
(function(){
  var es = new EventSource("/__livereload");
  es.addEventListener("reload", function(){ location.reload(); });
  es.addEventListener("hotreload", function(e){
    var jsPath = e.data;
    if (!Module || !Module.FS) {
      console.warn("[hot-reload] Module.FS not available, full reload");
      location.reload();
      return;
    }
    if (typeof Module._dtr_wasm_reload !== "function") {
      console.warn("[hot-reload] _dtr_wasm_reload not exported, full reload");
      location.reload();
      return;
    }
    fetch("/__cart/" + jsPath + "?t=" + Date.now())
      .then(function(r){ return r.text(); })
      .then(function(src){
        var enc = new TextEncoder();
        Module.FS.writeFile("/cart/" + jsPath, enc.encode(src));
        Module._dtr_wasm_reload();
        console.log("[hot-reload] reloaded " + jsPath);
      })
      .catch(function(err){ console.warn("[hot-reload] failed, full reload", err); location.reload(); });
  });
  es.addEventListener("assetreload", function(e){
    var assetPath = e.data;
    if (!Module || !Module.FS) {
      console.warn("[asset-reload] Module.FS not available, full reload");
      location.reload();
      return;
    }
    if (typeof Module._dtr_wasm_reload_assets !== "function") {
      console.warn("[asset-reload] _dtr_wasm_reload_assets not exported, full reload");
      location.reload();
      return;
    }
    fetch("/__cart/" + assetPath + "?t=" + Date.now())
      .then(function(r){ return r.arrayBuffer(); })
      .then(function(buf){
        Module.FS.writeFile("/cart/" + assetPath, new Uint8Array(buf));
        Module._dtr_wasm_reload_assets();
        console.log("[asset-reload] reloaded " + assetPath);
      })
      .catch(function(err){ console.warn("[asset-reload] failed, full reload", err); location.reload(); });
  });
  es.onerror = function(){ es.close(); setTimeout(function(){ location.reload(); }, 2000); };
})();
</script>
</head>`;

/* ------------------------------------------------------------------ */
/*  Static file server                                                 */
/* ------------------------------------------------------------------ */

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

function getMime(fp) {
    return MIME_TYPES[path.extname(fp).toLowerCase()] || "application/octet-stream";
}

function pickEncoding(accept) {
    if (accept.includes("br")) return "br";
    if (accept.includes("gzip")) return "gzip";
    return null;
}

function compressStream(enc) {
    if (enc === "br") return zlib.createBrotliCompress();
    if (enc === "gzip") return zlib.createGzip();
    return null;
}

const server = http.createServer((req, res) => {
    const url = new URL(req.url, `http://localhost:${port}`);
    let relPath = decodeURIComponent(url.pathname);

    /* SSE endpoint for live reload */
    if (relPath === "/__livereload") {
        res.writeHead(200, {
            "Content-Type": "text/event-stream",
            "Cache-Control": "no-cache",
            Connection: "keep-alive",
        });
        res.write(":\n\n");
        sseClients.add(res);
        req.on("close", () => sseClients.delete(res));
        return;
    }

    /* Serve raw cart files for hot reload */
    if (relPath.startsWith("/__cart/")) {
        const cartRel = relPath.slice("/__cart/".length);
        const cartFile = path.resolve(cartDir, cartRel);
        if (!cartFile.startsWith(cartDir + path.sep) && cartFile !== cartDir) {
            res.writeHead(403);
            res.end("Forbidden");
            return;
        }
        fs.stat(cartFile, (err, stat) => {
            if (err || !stat.isFile()) {
                res.writeHead(404);
                res.end("Not Found");
                return;
            }
            res.setHeader("Content-Type", getMime(cartFile));
            res.setHeader("Cache-Control", "no-store");
            res.setHeader("Content-Length", stat.size);
            res.writeHead(200);
            fs.createReadStream(cartFile).pipe(res);
        });
        return;
    }

    if (relPath === "/") relPath = "/index.html";

    // Prevent path traversal
    const filePath = path.resolve(buildDir, relPath.slice(1));
    if (!filePath.startsWith(buildDir + path.sep) && filePath !== buildDir) {
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
        /* Inject live-reload snippet into HTML */
        if (mime.startsWith("text/html")) {
            fs.readFile(filePath, "utf8", (readErr, html) => {
                if (readErr) {
                    res.writeHead(500);
                    res.end("Read error");
                    return;
                }
                html = html.replace("</head>", RELOAD_SNIPPET);

                const buf = Buffer.from(html, "utf8");
                const comp = compressStream(encoding);
                if (comp) {
                    res.setHeader("Content-Encoding", encoding);
                    res.writeHead(200);
                    comp.end(buf);
                    comp.pipe(res);
                } else {
                    res.setHeader("Content-Length", buf.length);
                    res.writeHead(200);
                    res.end(buf);
                }
            });
            return;
        }

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

/* ------------------------------------------------------------------ */
/*  Auto-rebuild on cart file changes                                  */
/* ------------------------------------------------------------------ */

let building = false;
let pendingRebuild = false;

function rebuild(openBrowser) {
    if (building) {
        pendingRebuild = true;
        return;
    }
    building = true;
    console.log("\n[watch] Rebuilding web export…");

    // Run dithrkit export --web
    const dithrkit = process.argv[0];
    const cliPath = path.join(__dirname, "..", "cli.js");
    const proc = spawn(dithrkit, [cliPath, "export", "--web", "--out", buildDir], {
        cwd: cartDir,
        stdio: "inherit",
        shell: false,
    });

    proc.on("close", (code) => {
        building = false;
        if (code === 0) {
            console.log("[watch] Build succeeded");
            if (openBrowser) {
                startServer();
            } else {
                console.log("[watch] Reloading browser");
                for (const client of sseClients) {
                    client.write("event: reload\ndata: rebuild\n\n");
                }
            }
        } else {
            console.log(`[watch] Build failed (exit ${code})`);
            if (openBrowser) {
                startServer();
            }
        }

        if (pendingRebuild) {
            pendingRebuild = false;
            rebuild(false);
        }
    });
}

/* Debounce */
let debounceTimer = null;
let pendingFiles = new Set();

function isIgnoredFile(filename) {
    if (!filename) return true;
    if (filename.endsWith("~") || filename.startsWith(".")) return true;
    const ext = path.extname(filename).toLowerCase();
    if (ext === ".tmp" || ext === ".swp" || ext === ".bak" || ext === ".crswap") return true;
    return false;
}

function onCartChange(eventType, filename) {
    if (isIgnoredFile(filename)) return;
    if (debounceTimer) clearTimeout(debounceTimer);

    pendingFiles.add(filename.replace(/\\/g, "/"));

    debounceTimer = setTimeout(() => {
        const files = [...pendingFiles];
        pendingFiles.clear();

        const known = files.filter((f) => path.extname(f) !== "");
        if (known.length === 0) return;

        const jsOnly = known.every((f) => f.endsWith(".js") || f.endsWith(".mjs"));
        const isEntryChange =
            jsOnly && entryJs != null && known.length === 1 && known[0] === entryJs;

        const ASSET_EXTS = [".png", ".tmj", ".ldtk", ".wav", ".ogg", ".mp3"];
        const assetOnly =
            !jsOnly &&
            known.every((f) => {
                const ext = path.extname(f).toLowerCase();
                return ASSET_EXTS.includes(ext);
            });

        if (isEntryChange) {
            console.log(`[watch] JS entry changed: ${known.join(", ")} — hot-reloading`);
            for (const client of sseClients) {
                for (const f of known) {
                    client.write(`event: hotreload\ndata: ${f}\n\n`);
                }
            }
        } else if (assetOnly) {
            console.log(`[watch] Assets changed: ${known.join(", ")} — asset-reloading`);
            for (const client of sseClients) {
                for (const f of known) {
                    client.write(`event: assetreload\ndata: ${f}\n\n`);
                }
            }
        } else {
            rebuild(false);
        }
    }, 300);
}

const watcher = fs.watch(cartDir, { recursive: true }, onCartChange);
console.log(`[watch] Watching ${cartDir} for changes`);

/* ------------------------------------------------------------------ */
/*  Graceful shutdown                                                  */
/* ------------------------------------------------------------------ */

function cleanup() {
    watcher.close();
    for (const client of sseClients) {
        client.end();
    }
    sseClients.clear();
    server.close();
    process.exit(0);
}

process.on("SIGINT", cleanup);
process.on("SIGTERM", cleanup);

/* ------------------------------------------------------------------ */
/*  Start server                                                       */
/* ------------------------------------------------------------------ */

function startServer() {
    server.listen(port, () => {
        const url = `http://localhost:${port}`;
        console.log(`[watch] Serving ${buildDir}`);
        console.log(`[watch]   → ${url}`);
        console.log("[watch] Press Ctrl+C to stop.\n");

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
            console.log(`[watch] Port ${port} in use — killing previous process…`);
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

// Initial build + serve
rebuild(true);
