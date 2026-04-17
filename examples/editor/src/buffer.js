// ─── Selection helpers ───────────────────────────────────────────────────────

import { st } from './state.js';
import { ensureVisible, resetBlink, status, MOD_NAME } from './helpers.js';

export function selOrdered() {
    if (!st.anchor) return null;
    let a = { x: st.anchor.x, y: st.anchor.y };
    let b = { x: st.cx, y: st.cy };
    if (a.y > b.y || (a.y === b.y && a.x > b.x)) {
        const t = a;
        a = b;
        b = t;
    }
    return { a, b };
}

export function selText() {
    const s = selOrdered();
    if (!s) return '';
    if (s.a.y === s.b.y) return st.buf[s.a.y].slice(s.a.x, s.b.x);
    const parts = [st.buf[s.a.y].slice(s.a.x)];
    for (let i = s.a.y + 1; i < s.b.y; i++) parts.push(st.buf[i]);
    parts.push(st.buf[s.b.y].slice(0, s.b.x));
    return parts.join('\n');
}

export function deleteSel() {
    const s = selOrdered();
    if (!s) return false;
    pushUndo();
    if (s.a.y === s.b.y) {
        st.buf[s.a.y] = st.buf[s.a.y].slice(0, s.a.x) + st.buf[s.a.y].slice(s.b.x);
    } else {
        st.buf[s.a.y] = st.buf[s.a.y].slice(0, s.a.x) + st.buf[s.b.y].slice(s.b.x);
        st.buf.splice(s.a.y + 1, s.b.y - s.a.y);
    }
    st.cx = s.a.x;
    st.cy = s.a.y;
    st.anchor = null;
    st.dirty = true;
    return true;
}

// ─── Undo ────────────────────────────────────────────────────────────────────

function snapshot() {
    return { buf: st.buf.map((l) => l), cx: st.cx, cy: st.cy };
}

export function pushUndo() {
    st.undoStack.push(snapshot());
    if (st.undoStack.length > st.MAXUNDO) st.undoStack.shift();
    st.redoStack = [];
    st.invalidateCaches();
}

export function doUndo() {
    if (!st.undoStack.length) {
        status('Nothing to undo');
        return;
    }
    st.redoStack.push(snapshot());
    const s = st.undoStack.pop();
    st.buf = s.buf;
    st.cx = s.cx;
    st.cy = s.cy;
    st.anchor = null;
    st.dirty = true;
    st.invalidateCaches();
    ensureVisible();
    resetBlink();
}

export function doRedo() {
    if (!st.redoStack.length) {
        status('Nothing to redo');
        return;
    }
    st.undoStack.push(snapshot());
    const s = st.redoStack.pop();
    st.buf = s.buf;
    st.cx = s.cx;
    st.cy = s.cy;
    st.anchor = null;
    st.dirty = true;
    st.invalidateCaches();
    ensureVisible();
    resetBlink();
}

// ─── File I/O ────────────────────────────────────────────────────────────────

export function openFile(path) {
    // If already open, just switch to it
    for (let i = 0; i < st.openFiles.length; i++) {
        if (st.openFiles[i].path === path) {
            switchToFile(i);
            status(`Switched to ${path}`);
            return;
        }
    }
    const data = sys.readFile(path);
    if (data === undefined) {
        status(`Cannot read: ${path}`);
        return;
    }
    storeFileState();
    let buf = data.replace(/\r\n/g, '\n').split('\n');
    if (!buf.length) buf = [''];
    const saved = buf.map((l) => l);
    st.openFiles.push({
        path,
        buf,
        cx: 0,
        cy: 0,
        ox: 0,
        oy: 0,
        targetOy: 0,
        anchor: null,
        undoStack: [],
        redoStack: [],
        dirty: false,
        savedBuf: saved,
    });
    st.fileIdx = st.openFiles.length - 1;
    loadFileState(st.fileIdx);
    status(`Opened ${path}`);
}

export function saveFile() {
    if (!st.fname) {
        status(`No file — open one first (${MOD_NAME}+O)`);
        return;
    }
    const ok = sys.writeFile(st.fname, st.buf.join('\n'));
    if (ok) {
        st.dirty = false;
        st.savedBuf = st.buf.map((l) => l);
        if (st.fileIdx >= 0 && st.fileIdx < st.openFiles.length) {
            st.openFiles[st.fileIdx].dirty = false;
            st.openFiles[st.fileIdx].savedBuf = st.savedBuf;
        }
        status(`Saved ${st.fname}`);
    } else {
        status(`Write failed: ${st.fname}`);
    }
}

export function openBrowser() {
    st.brDir = '';
    refreshBrowser();
    st.brMode = true;
}

export function refreshBrowser() {
    const dirs = sys.listDirs(st.brDir);
    const files = sys.listFiles(st.brDir);
    st.brEntries = [];
    if (st.brDir) {
        st.brEntries.push({ name: '..', isDir: true });
    }
    if (dirs) {
        for (let i = 0; i < dirs.length; i++) {
            st.brEntries.push({ name: dirs[i], isDir: true });
        }
    }
    if (files) {
        for (let i = 0; i < files.length; i++) {
            st.brEntries.push({ name: files[i], isDir: false });
        }
    }
    st.brIdx = 0;
    st.brScroll = 0;
}

// ─── Multi-file helpers ──────────────────────────────────────────────────────

export function storeFileState() {
    if (st.fileIdx < 0 || st.fileIdx >= st.openFiles.length) return;
    const f = st.openFiles[st.fileIdx];
    f.buf = st.buf;
    f.cx = st.cx;
    f.cy = st.cy;
    f.ox = st.ox;
    f.oy = st.oy;
    f.targetOy = st.targetOy;
    f.anchor = st.anchor;
    f.undoStack = st.undoStack;
    f.redoStack = st.redoStack;
    f.dirty = st.dirty;
    f.savedBuf = st.savedBuf;
}

export function loadFileState(idx) {
    const f = st.openFiles[idx];
    st.buf = f.buf;
    st.cx = f.cx;
    st.cy = f.cy;
    st.ox = f.ox;
    st.oy = f.oy;
    st.targetOy = f.targetOy;
    st.anchor = f.anchor;
    st.undoStack = f.undoStack;
    st.redoStack = f.redoStack;
    st.dirty = f.dirty;
    st.savedBuf = f.savedBuf;
    st.fname = f.path;
    st.fileIdx = idx;
    st.invalidateCaches();
}

export function switchToFile(idx) {
    if (idx === st.fileIdx) return;
    if (idx < 0 || idx >= st.openFiles.length) return;
    storeFileState();
    loadFileState(idx);
}

export function closeFile(idx) {
    if (idx < 0 || idx >= st.openFiles.length) return;
    if (st.openFiles.length === 1) {
        st.openFiles = [];
        st.fileIdx = -1;
        st.buf = [''];
        st.fname = '';
        st.cx = 0;
        st.cy = 0;
        st.ox = 0;
        st.oy = 0;
        st.targetOy = 0;
        st.anchor = null;
        st.undoStack = [];
        st.redoStack = [];
        st.dirty = false;
        st.savedBuf = [];
        st.invalidateCaches();
        return;
    }
    if (idx === st.fileIdx) {
        st.openFiles.splice(idx, 1);
        st.fileIdx = Math.min(idx, st.openFiles.length - 1);
        loadFileState(st.fileIdx);
    } else {
        st.openFiles.splice(idx, 1);
        if (idx < st.fileIdx) st.fileIdx--;
    }
}
