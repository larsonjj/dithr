// ─── Selection helpers ───────────────────────────────────────────────────────

function selOrdered() {
    if (!anchor) return null;
    let a = { x: anchor.x, y: anchor.y };
    let b = { x: cx, y: cy };
    if (a.y > b.y || (a.y === b.y && a.x > b.x)) {
        let t = a;
        a = b;
        b = t;
    }
    return { a, b };
}

function selText() {
    let s = selOrdered();
    if (!s) return "";
    if (s.a.y === s.b.y) return buf[s.a.y].slice(s.a.x, s.b.x);
    let parts = [buf[s.a.y].slice(s.a.x)];
    for (let i = s.a.y + 1; i < s.b.y; i++) parts.push(buf[i]);
    parts.push(buf[s.b.y].slice(0, s.b.x));
    return parts.join("\n");
}

function deleteSel() {
    let s = selOrdered();
    if (!s) return false;
    pushUndo();
    if (s.a.y === s.b.y) {
        buf[s.a.y] = buf[s.a.y].slice(0, s.a.x) + buf[s.a.y].slice(s.b.x);
    } else {
        buf[s.a.y] = buf[s.a.y].slice(0, s.a.x) + buf[s.b.y].slice(s.b.x);
        buf.splice(s.a.y + 1, s.b.y - s.a.y);
    }
    cx = s.a.x;
    cy = s.a.y;
    anchor = null;
    dirty = true;
    return true;
}

// ─── Undo ────────────────────────────────────────────────────────────────────

function snapshot() {
    return { buf: buf.map((l) => l), cx, cy };
}

function pushUndo() {
    undoStack.push(snapshot());
    if (undoStack.length > MAXUNDO) undoStack.shift();
    redoStack = [];
    invalidateCaches();
}

function doUndo() {
    if (!undoStack.length) {
        status("Nothing to undo");
        return;
    }
    redoStack.push(snapshot());
    let s = undoStack.pop();
    buf = s.buf;
    cx = s.cx;
    cy = s.cy;
    anchor = null;
    dirty = true;
    invalidateCaches();
    ensureVisible();
    resetBlink();
}

function doRedo() {
    if (!redoStack.length) {
        status("Nothing to redo");
        return;
    }
    invalidateCaches();
    undoStack.push(snapshot());
    let s = redoStack.pop();
    buf = s.buf;
    cx = s.cx;
    cy = s.cy;
    anchor = null;
    dirty = true;
    ensureVisible();
    resetBlink();
}

// ─── File I/O ────────────────────────────────────────────────────────────────

function openFile(path) {
    let data = sys.readFile(path);
    if (data === undefined) {
        status("Cannot read: " + path);
        return;
    }
    buf = data.replace(/\r\n/g, "\n").split("\n");
    if (!buf.length) buf = [""];
    fname = path;
    cx = cy = ox = oy = 0;
    anchor = null;
    dirty = false;
    undoStack = [];
    redoStack = [];
    savedBuf = buf.map(function (l) {
        return l;
    });
    invalidateCaches();
    status("Opened " + path);
}

function saveFile() {
    if (!fname) {
        status("No file — open one first (Ctrl+O)");
        return;
    }
    let ok = sys.writeFile(fname, buf.join("\n"));
    if (ok) {
        dirty = false;
        savedBuf = buf.map(function (l) {
            return l;
        });
        status("Saved " + fname);
    } else {
        status("Write failed: " + fname);
    }
}

function openBrowser() {
    brDir = "";
    refreshBrowser();
    brMode = true;
}

function refreshBrowser() {
    let dirs = sys.listDirs(brDir);
    let files = sys.listFiles(brDir);
    brEntries = [];
    if (brDir) {
        brEntries.push({ name: "..", isDir: true });
    }
    if (dirs) {
        for (let i = 0; i < dirs.length; i++) {
            brEntries.push({ name: dirs[i], isDir: true });
        }
    }
    if (files) {
        for (let i = 0; i < files.length; i++) {
            brEntries.push({ name: files[i], isDir: false });
        }
    }
    brIdx = 0;
    brScroll = 0;
}
