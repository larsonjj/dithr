// ─── Vim mode ────────────────────────────────────────────────────────────────

import { st } from './state.js';
import { EDIT_Y, FOOT_Y, LINE_H, GUTTER, CW } from './config.js';
import {
    clamp,
    ensureVisible,
    resetBlink,
    status,
    firstNonBlank,
    getIndent,
    vimWordForward,
    vimWordBack,
    vimWordEnd,
} from './helpers.js';
import {
    selText,
    deleteSel,
    pushUndo,
    doUndo,
    saveFile,
    openBrowser,
    closeFile,
} from './buffer.js';

export function updateVimKeys() {
    if (st.vim === 'command') {
        if (key.btnp(key.ESCAPE)) {
            st.vim = 'normal';
            st.vimCmd = '';
            return;
        }
        if (key.btnp(key.ENTER)) {
            vimExecCmd(st.vimCmd);
            st.vim = 'normal';
            st.vimCmd = '';
            return;
        }
        if (key.btnr(key.BACKSPACE)) {
            if (st.vimCmd.length > 0) {
                st.vimCmd = st.vimCmd.slice(0, -1);
            } else {
                st.vim = 'normal';
            }
            return;
        }
        return;
    }

    if (key.btnp(key.ESCAPE)) {
        if (st.vim === 'visual' || st.vim === 'vline') {
            st.vim = 'normal';
            st.anchor = null;
        }
        st.vimCount = '';
        st.vimPending = '';
        return;
    }

    const mx = mouse.x();
    const my = mouse.y();
    const editY = EDIT_Y;
    const footY = FOOT_Y;
    const gutterPx = GUTTER * CW;

    if (my >= editY && my < footY && mx >= gutterPx) {
        if (mouse.btnp(0)) {
            let row = (st.oy + (my - editY) / LINE_H) | 0;
            let col = (st.ox + (mx - gutterPx) / CW) | 0;
            row = clamp(row, 0, st.buf.length - 1);
            col = clamp(col, 0, Math.max(0, st.buf[row].length - 1));
            st.cx = col;
            st.cy = row;
            if (st.vim !== 'visual' && st.vim !== 'vline') st.anchor = null;
            ensureVisible();
            resetBlink();
        }
    }
}

// ── Vim normal mode dispatch ──

export function vimNormal(ch: string) {
    if (st.vim === 'visual' || st.vim === 'vline') {
        vimVisual(ch);
        return;
    }

    if ((ch >= '1' && ch <= '9') || (ch === '0' && st.vimCount !== '')) {
        st.vimCount += ch;
        return;
    }

    const count = st.vimCount !== '' ? parseInt(st.vimCount, 10) : 1;
    st.vimCount = '';

    if (st.vimPending !== '') {
        vimResolvePending(ch, count);
        return;
    }

    switch (ch) {
        case 'i':
            st.vim = 'insert';
            return;
        case 'I':
            st.cx = firstNonBlank(st.cy);
            st.vim = 'insert';
            return;
        case 'a':
            st.cx = Math.min(st.cx + 1, st.buf[st.cy].length);
            st.vim = 'insert';
            return;
        case 'A':
            st.cx = st.buf[st.cy].length;
            st.vim = 'insert';
            return;
        case 'o': {
            pushUndo();
            const ind = getIndent(st.cy);
            st.buf.splice(st.cy + 1, 0, ind);
            st.cy++;
            st.cx = ind.length;
            st.dirty = true;
            st.vim = 'insert';
            ensureVisible();
            resetBlink();
            return;
        }
        case 'O': {
            pushUndo();
            const ind = getIndent(st.cy);
            st.buf.splice(st.cy, 0, ind);
            st.cx = ind.length;
            st.dirty = true;
            st.vim = 'insert';
            ensureVisible();
            resetBlink();
            return;
        }
        case 'v':
            st.anchor = { x: st.cx, y: st.cy };
            st.vim = 'visual';
            return;
        case 'V':
            st.vimVlineStart = st.cy;
            st.anchor = { x: 0, y: st.cy };
            st.cx = st.buf[st.cy].length;
            st.vim = 'vline';
            return;
        case ':':
            st.vim = 'command';
            st.vimCmd = '';
            return;
        case '/':
            st.vim = 'command';
            st.vimCmd = '/';
            return;

        case 'h':
            for (let i = 0; i < count; i++) if (st.cx > 0) st.cx--;
            break;
        case 'j':
            for (let i = 0; i < count; i++) if (st.cy < st.buf.length - 1) st.cy++;
            st.cx = clamp(st.cx, 0, Math.max(0, st.buf[st.cy].length - 1));
            break;
        case 'k':
            for (let i = 0; i < count; i++) if (st.cy > 0) st.cy--;
            st.cx = clamp(st.cx, 0, Math.max(0, st.buf[st.cy].length - 1));
            break;
        case 'l':
            for (let i = 0; i < count; i++) if (st.cx < st.buf[st.cy].length - 1) st.cx++;
            break;
        case 'w':
            for (let i = 0; i < count; i++) vimWordForward();
            break;
        case 'b':
            for (let i = 0; i < count; i++) vimWordBack();
            break;
        case 'e':
            for (let i = 0; i < count; i++) vimWordEnd();
            break;
        case '0':
            st.cx = 0;
            break;
        case '$':
            st.cx = Math.max(0, st.buf[st.cy].length - 1);
            break;
        case '^':
            st.cx = firstNonBlank(st.cy);
            break;
        case 'G':
            st.cy = clamp(count - 1, 0, st.buf.length - 1);
            st.cx = firstNonBlank(st.cy);
            break;
        case 'g':
            st.vimPending = 'g';
            st.vimOpCount = count;
            return;

        case 'x':
            pushUndo();
            for (let i = 0; i < count; i++) {
                if (st.cx < st.buf[st.cy].length) {
                    st.vimReg = st.buf[st.cy][st.cx];
                    st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + st.buf[st.cy].slice(st.cx + 1);
                    st.dirty = true;
                }
            }
            st.vimLinewise = false;
            if (st.cx >= st.buf[st.cy].length && st.cx > 0) st.cx = st.buf[st.cy].length - 1;
            break;
        case 's':
            pushUndo();
            if (st.cx < st.buf[st.cy].length) {
                st.vimReg = st.buf[st.cy][st.cx];
                st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + st.buf[st.cy].slice(st.cx + 1);
                st.dirty = true;
            }
            st.vimLinewise = false;
            st.vim = 'insert';
            break;
        case 'r':
            st.vimPending = 'r';
            return;
        case 'p':
            vimPaste(false, count);
            break;
        case 'P':
            vimPaste(true, count);
            break;
        case 'u':
            for (let i = 0; i < count; i++) doUndo();
            break;
        case 'J':
            pushUndo();
            for (let i = 0; i < count; i++) {
                if (st.cy < st.buf.length - 1) {
                    const trail = st.buf[st.cy + 1].replace(/^\s+/, '');
                    st.buf[st.cy] += (trail ? ' ' : '') + trail;
                    st.buf.splice(st.cy + 1, 1);
                    st.dirty = true;
                }
            }
            break;
        case 'D':
            pushUndo();
            st.vimReg = st.buf[st.cy].slice(st.cx);
            st.vimLinewise = false;
            st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx);
            if (st.cx > 0) st.cx--;
            st.dirty = true;
            break;
        case 'C':
            pushUndo();
            st.vimReg = st.buf[st.cy].slice(st.cx);
            st.vimLinewise = false;
            st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx);
            st.dirty = true;
            st.vim = 'insert';
            break;

        case 'd':
            st.vimPending = 'd';
            st.vimOpCount = count;
            return;
        case 'c':
            st.vimPending = 'c';
            st.vimOpCount = count;
            return;
        case 'y':
            st.vimPending = 'y';
            st.vimOpCount = count;
            return;

        case 'n':
            for (let i = 0; i < count; i++) vimSearchNext(1);
            break;
        case 'N':
            for (let i = 0; i < count; i++) vimSearchNext(-1);
            break;

        default:
            return;
    }

    ensureVisible();
    resetBlink();
}

// ── Vim visual mode ──

function vimVisual(ch: string) {
    if ((ch >= '1' && ch <= '9') || (ch === '0' && st.vimCount !== '')) {
        st.vimCount += ch;
        return;
    }

    const count = st.vimCount !== '' ? parseInt(st.vimCount, 10) : 1;
    st.vimCount = '';

    switch (ch) {
        case 'd':
        case 'x':
            st.vimReg = selText();
            st.vimLinewise = st.vim === 'vline';
            deleteSel();
            st.vim = 'normal';
            if (st.cx >= st.buf[st.cy].length && st.cx > 0) st.cx = st.buf[st.cy].length - 1;
            ensureVisible();
            resetBlink();
            return;
        case 'y':
            st.vimReg = selText();
            st.vimLinewise = st.vim === 'vline';
            st.anchor = null;
            st.vim = 'normal';
            status('Yanked');
            return;
        case 'c':
            st.vimReg = selText();
            st.vimLinewise = st.vim === 'vline';
            deleteSel();
            st.vim = 'insert';
            ensureVisible();
            resetBlink();
            return;
        default:
            break;
    }

    switch (ch) {
        case 'h':
            for (let i = 0; i < count; i++) if (st.cx > 0) st.cx--;
            break;
        case 'j':
            for (let i = 0; i < count; i++) if (st.cy < st.buf.length - 1) st.cy++;
            st.cx = clamp(st.cx, 0, st.buf[st.cy].length);
            break;
        case 'k':
            for (let i = 0; i < count; i++) if (st.cy > 0) st.cy--;
            st.cx = clamp(st.cx, 0, st.buf[st.cy].length);
            break;
        case 'l':
            for (let i = 0; i < count; i++) if (st.cx < st.buf[st.cy].length) st.cx++;
            break;
        case 'w':
            for (let i = 0; i < count; i++) vimWordForward();
            break;
        case 'b':
            for (let i = 0; i < count; i++) vimWordBack();
            break;
        case 'e':
            for (let i = 0; i < count; i++) vimWordEnd();
            break;
        case '0':
            st.cx = 0;
            break;
        case '$':
            st.cx = st.buf[st.cy].length;
            break;
        case '^':
            st.cx = firstNonBlank(st.cy);
            break;
        case 'G':
            st.cy = st.buf.length - 1;
            st.cx = clamp(st.cx, 0, st.buf[st.cy].length);
            break;
        case 'g':
            st.vimPending = 'g';
            return;
        default:
            return;
    }

    if (st.vim === 'vline') {
        if (st.cy >= st.vimVlineStart) {
            st.anchor = { x: 0, y: st.vimVlineStart };
            st.cx = st.buf[st.cy].length;
        } else {
            st.anchor = { x: st.buf[st.vimVlineStart].length, y: st.vimVlineStart };
            st.cx = 0;
        }
    }

    ensureVisible();
    resetBlink();
}

// ── Resolve pending operator ──

function vimResolvePending(ch: string, motionCount: number) {
    const op = st.vimPending;
    const opCount = st.vimOpCount;
    st.vimPending = '';
    st.vimOpCount = 1;

    const totalCount = opCount * motionCount;

    if (op === 'r') {
        pushUndo();
        if (st.cx < st.buf[st.cy].length) {
            st.buf[st.cy] = st.buf[st.cy].slice(0, st.cx) + ch + st.buf[st.cy].slice(st.cx + 1);
            st.dirty = true;
        }
        ensureVisible();
        resetBlink();
        return;
    }

    if (op === 'g') {
        if (ch === 'g') {
            st.cy = opCount > 1 ? clamp(opCount - 1, 0, st.buf.length - 1) : 0;
            st.cx = firstNonBlank(st.cy);
        }
        ensureVisible();
        resetBlink();
        return;
    }

    if (ch === op) {
        vimLinewiseOp(op, totalCount);
        return;
    }

    vimMotionOp(op, ch, totalCount);
}

// ── Linewise operator (dd, yy, cc) ──

function vimLinewiseOp(op: string, count: number) {
    const startLine = st.cy;
    const endLine = Math.min(st.cy + count - 1, st.buf.length - 1);
    let text = '';
    for (let i = startLine; i <= endLine; i++) text += `${st.buf[i]}\n`;
    st.vimReg = text;
    st.vimLinewise = true;

    if (op === 'd' || op === 'c') {
        pushUndo();
        st.buf.splice(startLine, endLine - startLine + 1);
        if (!st.buf.length) st.buf = [''];
        st.cy = Math.min(startLine, st.buf.length - 1);
        st.cx = firstNonBlank(st.cy);
        st.dirty = true;
    }
    if (op === 'c') st.vim = 'insert';
    if (op === 'y') status(`Yanked ${endLine - startLine + 1} lines`);

    ensureVisible();
    resetBlink();
}

// ── Operator + motion ──

function vimMotionOp(op: string, ch: string, count: number) {
    const ocx = st.cx;
    const ocy = st.cy;

    let moved = false;
    switch (ch) {
        case 'h':
            for (let i = 0; i < count; i++)
                if (st.cx > 0) {
                    st.cx--;
                    moved = true;
                }
            break;
        case 'j':
            for (let i = 0; i < count; i++)
                if (st.cy < st.buf.length - 1) {
                    st.cy++;
                    moved = true;
                }
            break;
        case 'k':
            for (let i = 0; i < count; i++)
                if (st.cy > 0) {
                    st.cy--;
                    moved = true;
                }
            break;
        case 'l':
            for (let i = 0; i < count; i++)
                if (st.cx < st.buf[st.cy].length) {
                    st.cx++;
                    moved = true;
                }
            break;
        case 'w':
            for (let i = 0; i < count; i++) vimWordForward();
            moved = true;
            break;
        case 'b':
            for (let i = 0; i < count; i++) vimWordBack();
            moved = true;
            break;
        case 'e':
            for (let i = 0; i < count; i++) {
                vimWordEnd();
                st.cx++;
            }
            moved = true;
            break;
        case '$':
            st.cx = st.buf[st.cy].length;
            moved = true;
            break;
        case '0':
            st.cx = 0;
            moved = true;
            break;
        case '^':
            st.cx = firstNonBlank(st.cy);
            moved = true;
            break;
        case 'G':
            st.cy = st.buf.length - 1;
            moved = true;
            break;
        default:
            return;
    }

    if (!moved) return;

    const linewise = ch === 'j' || ch === 'k' || ch === 'G';

    if (linewise) {
        const sl = Math.min(ocy, st.cy);
        const el = Math.max(ocy, st.cy);
        let text = '';
        for (let i = sl; i <= el; i++) text += `${st.buf[i]}\n`;
        st.vimReg = text;
        st.vimLinewise = true;

        if (op === 'd' || op === 'c') {
            pushUndo();
            st.buf.splice(sl, el - sl + 1);
            if (!st.buf.length) st.buf = [''];
            st.cy = Math.min(sl, st.buf.length - 1);
            st.cx = firstNonBlank(st.cy);
            st.dirty = true;
        }
        if (op === 'c') st.vim = 'insert';
        if (op === 'y') {
            st.cx = ocx;
            st.cy = ocy;
            status(`Yanked ${el - sl + 1} lines`);
        }
    } else {
        let a = { x: ocx, y: ocy };
        let b = { x: st.cx, y: st.cy };
        if (a.y > b.y || (a.y === b.y && a.x > b.x)) {
            const t = a;
            a = b;
            b = t;
        }

        let text;
        if (a.y === b.y) {
            text = st.buf[a.y].slice(a.x, b.x);
        } else {
            const parts = [st.buf[a.y].slice(a.x)];
            for (let i = a.y + 1; i < b.y; i++) parts.push(st.buf[i]);
            parts.push(st.buf[b.y].slice(0, b.x));
            text = parts.join('\n');
        }
        st.vimReg = text;
        st.vimLinewise = false;

        if (op === 'd' || op === 'c') {
            pushUndo();
            if (a.y === b.y) {
                st.buf[a.y] = st.buf[a.y].slice(0, a.x) + st.buf[a.y].slice(b.x);
            } else {
                st.buf[a.y] = st.buf[a.y].slice(0, a.x) + st.buf[b.y].slice(b.x);
                st.buf.splice(a.y + 1, b.y - a.y);
            }
            st.cx = a.x;
            st.cy = a.y;
            if (st.cx >= st.buf[st.cy].length && st.cx > 0 && op === 'd') st.cx--;
            st.dirty = true;
        }
        if (op === 'c') st.vim = 'insert';
        if (op === 'y') {
            st.cx = ocx;
            st.cy = ocy;
            status('Yanked');
        }
    }

    ensureVisible();
    resetBlink();
}

// ── Vim paste ──

function vimPaste(before: boolean, count: number) {
    if (!st.vimReg) return;
    pushUndo();

    if (st.vimLinewise) {
        const lines = st.vimReg.split('\n');
        if (lines.length && lines[lines.length - 1] === '') lines.pop();
        for (let c = 0; c < count; c++) {
            const insertAt = before ? st.cy : st.cy + 1;
            for (let i = 0; i < lines.length; i++) {
                st.buf.splice(insertAt + i, 0, lines[i]);
            }
            if (!before) st.cy += lines.length;
        }
        st.cx = firstNonBlank(st.cy);
    } else {
        const lines = st.vimReg.split('\n');
        if (lines.length <= 1) {
            for (let c = 0; c < count; c++) {
                const pos = before ? st.cx : Math.min(st.cx + 1, st.buf[st.cy].length);
                st.buf[st.cy] = st.buf[st.cy].slice(0, pos) + st.vimReg + st.buf[st.cy].slice(pos);
                st.cx = pos + st.vimReg.length - 1;
            }
        } else {
            for (let c = 0; c < count; c++) {
                const pos = before ? st.cx : Math.min(st.cx + 1, st.buf[st.cy].length);
                const head = st.buf[st.cy].slice(0, pos);
                const tail = st.buf[st.cy].slice(pos);
                st.buf[st.cy] = head + lines[0];
                for (let i = 1; i < lines.length - 1; i++) {
                    st.buf.splice(st.cy + i, 0, lines[i]);
                }
                const lastIdx = st.cy + lines.length - 1;
                st.buf.splice(lastIdx, 0, lines[lines.length - 1] + tail);
                st.cy = lastIdx;
                st.cx = lines[lines.length - 1].length - 1;
                if (st.cx < 0) st.cx = 0;
            }
        }
    }

    st.dirty = true;
    ensureVisible();
    resetBlink();
}

// ── Vim command execution ──

export function vimExecCmd(cmd: string) {
    if (cmd === 'w') {
        saveFile();
    } else if (cmd === 'q') {
        closeFile(st.fileIdx);
    } else if (cmd === 'wq') {
        saveFile();
        closeFile(st.fileIdx);
    } else if (cmd === 'e' || cmd === 'e ') {
        openBrowser();
    } else if (/^[0-9]+$/.test(cmd)) {
        const line = parseInt(cmd, 10);
        st.cy = clamp(line - 1, 0, st.buf.length - 1);
        st.cx = firstNonBlank(st.cy);
        ensureVisible();
        resetBlink();
    } else if (cmd.charAt(0) === '/') {
        st.vimSearch = cmd.slice(1);
        if (st.vimSearch) vimSearchNext(1);
    } else {
        status(`Unknown: :${cmd}`);
    }
}

// ── Vim search ──

export function vimSearchNext(dir: number) {
    if (!st.vimSearch) {
        status('No search pattern');
        return;
    }

    const total = st.buf.length;
    const startCol = st.cx + dir;

    for (let i = 0; i < total; i++) {
        const li = (st.cy + i * dir + total) % total;
        const line = st.buf[li];
        let col;

        if (i === 0) {
            if (dir > 0) {
                col = line.indexOf(st.vimSearch, Math.max(0, startCol));
            } else {
                col = line.lastIndexOf(st.vimSearch, Math.max(0, st.cx - 1));
            }
        } else {
            col = dir > 0 ? line.indexOf(st.vimSearch) : line.lastIndexOf(st.vimSearch);
        }

        if (col >= 0) {
            st.cy = li;
            st.cx = col;
            ensureVisible();
            resetBlink();
            status(`/${st.vimSearch}`);
            return;
        }
    }

    status(`Not found: ${st.vimSearch}`);
}
