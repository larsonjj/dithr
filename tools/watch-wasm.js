#!/usr/bin/env node
/**
 * @file  watch-wasm.js
 * @brief Watch cart source directory, auto-rebuild WASM, and serve with
 *        live reload.  Combines file watching, cmake rebuild, static
 *        serving, and SSE-based browser reload into one process.
 *
 * Usage:  node tools/watch-wasm.js <cartDir> [port]
 *
 * Environment:
 *   Expects the WASM build to already be configured
 *   (cmake --preset wasm -DDTR_WASM_CART_DIR=<cartDir>).
 */

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const zlib = require("node:zlib");
const { execSync, spawn } = require("node:child_process");

const { matchesAny, readAssetsConfig } = require("./resolve-assets.js");

const CART_DIR = process.argv[2];
if (!CART_DIR) {
    console.error("Usage: node tools/watch-wasm.js <cartDir> [port]");
    process.exit(1);
}

const PORT = parseInt(process.argv[3], 10) || 8080;
const ROOT = path.resolve(__dirname, "..");
const BUILD_DIR = path.join(ROOT, "build", "wasm");
const DYNAMIC_DIR = path.join(BUILD_DIR, "dynamic");
const CART_ABS = path.resolve(CART_DIR);

/* Read cart.json to determine the JS entry-point file and dynamic globs */
let ENTRY_JS = null;
let DYNAMIC_GLOBS = [];
function reloadCartConfig() {
    try {
        const cartJson = JSON.parse(fs.readFileSync(path.join(CART_ABS, "cart.json"), "utf8"));
        if (cartJson.code) {
            ENTRY_JS = cartJson.code.replace(/\\/g, "/");
        }
        const cfg = readAssetsConfig(CART_ABS);
        DYNAMIC_GLOBS = cfg.dynamic;
    } catch (err) {
        console.warn("Failed to parse cart.json:", err.message);
        /* cart.json missing or unparseable — fall back to treating all .js as entry */
    }
}
reloadCartConfig();

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
    const url = new URL(req.url, `http://localhost:${PORT}`);
    let relPath = decodeURIComponent(url.pathname);

    /* SSE endpoint for live reload */
    if (relPath === "/__livereload") {
        res.writeHead(200, {
            "Content-Type": "text/event-stream",
            "Cache-Control": "no-cache",
            Connection: "keep-alive",
        });
        res.write(":\n\n"); // SSE comment to keep connection alive
        sseClients.add(res);
        req.on("close", () => sseClients.delete(res));
        return;
    }

    /* Serve raw cart files for hot reload */
    if (relPath.startsWith("/__cart/")) {
        const cartRel = relPath.slice("/__cart/".length);
        const cartFile = path.join(CART_ABS, cartRel);
        if (!cartFile.startsWith(CART_ABS)) {
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

    if (relPath === "/") relPath = "/dithr.html";

    // Prevent path traversal
    const filePath = path.join(BUILD_DIR, relPath);
    if (!filePath.startsWith(BUILD_DIR)) {
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
    console.log("\n[watch] Rebuilding…");

    const proc = spawn("cmake", ["--build", BUILD_DIR], {
        cwd: ROOT,
        stdio: "inherit",
        shell: true,
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
                startServer(); // still start server so user can iterate
            }
        }

        if (pendingRebuild) {
            pendingRebuild = false;
            rebuild(false);
        }
    });
}

/* Debounce: collect rapid changes into a single rebuild */
let debounceTimer = null;
let pendingFiles = new Set();

/**
 * Returns true for editor temp / backup files that should be ignored.
 * This prevents atomic-save artefacts (VS Code, vim, etc.) from
 * defeating the JS-only hot-reload path.
 */
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

        /* Filter out any remaining unrecognised files (e.g. no extension) */
        const known = files.filter((f) => path.extname(f) !== "");
        if (known.length === 0) return;

        /* If cart.json itself changed, reload our cached config so subsequent
           classifications use the new dynamic glob set. */
        if (known.includes("cart.json")) {
            reloadCartConfig();
        }

        /* Files matching assets.dynamic globs only need to be re-staged into
           build/wasm/dynamic/ — no cmake link required. */
        const dynamicOnly =
            DYNAMIC_GLOBS.length > 0 && known.every((f) => matchesAny(f, DYNAMIC_GLOBS));

        const jsOnly = known.every((f) => f.endsWith(".js") || f.endsWith(".mjs"));

        /* When we know the entry-point, only hot-reload if the changed file
           IS the entry-point.  Other .js files need a full cmake rebuild. */
        const isEntryChange =
            jsOnly && ENTRY_JS != null && known.length === 1 && known[0] === ENTRY_JS;

        const ASSET_EXTS = [".png", ".tmj", ".ldtk", ".wav", ".ogg", ".mp3"];
        const assetOnly =
            !jsOnly &&
            known.every((f) => {
                const ext = path.extname(f).toLowerCase();
                return ASSET_EXTS.includes(ext);
            });

        if (isEntryChange) {
            /* Entry-point JS change — hot reload without cmake rebuild */
            console.log(`[watch] JS entry changed: ${known.join(", ")} — hot-reloading`);
            for (const client of sseClients) {
                for (const f of known) {
                    client.write(`event: hotreload\ndata: ${f}\n\n`);
                }
            }
        } else if (dynamicOnly) {
            /* Dynamic-asset change — re-stage build/wasm/dynamic/ and signal
               assetreload. Skip cmake entirely. */
            console.log(`[watch] Dynamic assets changed: ${known.join(", ")} — re-staging`);
            const proc = spawn(
                process.execPath,
                [path.join(__dirname, "prepare-dynamic.js"), CART_ABS, DYNAMIC_DIR],
                { stdio: "inherit" },
            );
            proc.on("close", () => {
                for (const client of sseClients) {
                    for (const f of known) {
                        client.write(`event: assetreload\ndata: ${f}\n\n`);
                    }
                }
            });
        } else if (assetOnly) {
            /* Asset-only change — reload assets without cmake rebuild */
            console.log(`[watch] Assets changed: ${known.join(", ")} — asset-reloading`);
            for (const client of sseClients) {
                for (const f of known) {
                    client.write(`event: assetreload\ndata: ${f}\n\n`);
                }
            }
        } else {
            /* Non-JS change — full rebuild + page reload */
            rebuild(false);
        }
    }, 300);
}

fs.watch(CART_ABS, { recursive: true }, onCartChange);
console.log(`[watch] Watching ${CART_ABS} for changes`);

/* ------------------------------------------------------------------ */
/*  Start server                                                       */
/* ------------------------------------------------------------------ */

function startServer() {
    server.listen(PORT, () => {
        const url = `http://localhost:${PORT}`;
        console.log(`[watch] Serving ${BUILD_DIR}`);
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
            console.log(`[watch] Port ${PORT} in use — killing previous process…`);
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
                /* ignore */
            }
            setTimeout(startServer, 500);
        } else {
            throw err;
        }
    });
}

/* Force a fresh build before serving — delete the .data file so Ninja
 * must relink even if it thinks sources are up to date. */
const dataFile = path.join(BUILD_DIR, "dithr.data");
if (fs.existsSync(dataFile)) {
    fs.unlinkSync(dataFile);
    console.log("[watch] Removed stale dithr.data to force relink");
}
rebuild(true);
