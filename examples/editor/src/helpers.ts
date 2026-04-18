// ─── Helpers ─────────────────────────────────────────────────────────────────

import { st } from './state.js';
import {
    EROWS,
    ECOLS,
    SCROLL_MARGIN,
    BRACKET_PAIRS,
    TAB_CODE,
    TAB_SPRITES,
    TAB_MAP,
    TAB_SFX,
    TAB_MUSIC,
} from './config.js';

const IS_MAC = sys.platform() === 'macos';
export const MOD_NAME = IS_MAC ? 'Cmd' : 'Ctrl';

export function modKey() {
    if (IS_MAC)
        return key.btn(key.LGUI) || key.btn(key.RGUI) || key.btn(key.LCTRL) || key.btn(key.RCTRL);
    return key.btn(key.LCTRL) || key.btn(key.RCTRL);
}

export function clamp(v, lo, hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

/** Check mod+1/2/3 tab switching. Returns true if a switch happened. */
export function handleTabSwitch() {
    if (!modKey()) return false;
    if (key.btnp(key.NUM1)) {
        st.activeTab = TAB_CODE;
        return true;
    }
    if (key.btnp(key.NUM2)) {
        st.activeTab = TAB_SPRITES;
        return true;
    }
    if (key.btnp(key.NUM3)) {
        st.activeTab = TAB_MAP;
        return true;
    }
    if (key.btnp(key.NUM4)) {
        st.activeTab = TAB_SFX;
        return true;
    }
    if (key.btnp(key.NUM5)) {
        st.activeTab = TAB_MUSIC;
        return true;
    }
    return false;
}

export function status(text, sec = 3) {
    st.msg = text;
    st.msgT = sec || 3;
}

export function resetBlink() {
    st.blink = 0;
    st.curOn = true;
}

export function ensureVisible() {
    let newOy = st.oy;
    if (st.cy < newOy + SCROLL_MARGIN) newOy = Math.max(0, st.cy - SCROLL_MARGIN);
    if (st.cy > newOy + EROWS - 1 - SCROLL_MARGIN) newOy = st.cy - EROWS + 1 + SCROLL_MARGIN;
    newOy = clamp(newOy, 0, Math.max(0, st.buf.length - 1));
    st.oy = newOy;
    st.targetOy = newOy;
    if (st.cx < st.ox) st.ox = st.cx;
    if (st.cx >= st.ox + ECOLS) st.ox = st.cx - ECOLS + 1;
}

// ─── Word helpers ────────────────────────────────────────────────────────────

export function isWordChar(c) {
    return (
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c === '_' ||
        c === '$'
    );
}

export function wordBoundaryLeft(line, col) {
    let c = col;
    if (c <= 0) return 0;
    c--;
    while (c > 0 && line[c] === ' ') c--;
    if (isWordChar(line[c])) {
        while (c > 0 && isWordChar(line[c - 1])) c--;
    } else {
        while (c > 0 && !isWordChar(line[c - 1]) && line[c - 1] !== ' ') c--;
    }
    return c;
}

export function wordBoundaryRight(line, col) {
    let c = col;
    if (c >= line.length) return line.length;
    if (isWordChar(line[c])) {
        while (c < line.length && isWordChar(line[c])) c++;
    } else if (line[c] !== ' ') {
        while (c < line.length && !isWordChar(line[c]) && line[c] !== ' ') c++;
    }
    while (c < line.length && line[c] === ' ') c++;
    return c;
}

// ─── Bracket matching ───────────────────────────────────────────────────────

// ─── Bracket context helper ─────────────────────────────────────────────────
// Returns a boolean array: true at positions inside a string or comment.
function buildContextMask(line, inBlock) {
    const mask = new Uint8Array(line.length);
    let i = 0;
    const n = line.length;
    if (inBlock) {
        const end = line.indexOf('*/');
        if (end >= 0) {
            for (let j = 0; j <= end + 1; j++) mask[j] = 1;
            i = end + 2;
        } else {
            mask.fill(1);
            return mask;
        }
    }
    while (i < n) {
        if (line[i] === '/' && i + 1 < n) {
            if (line[i + 1] === '*') {
                const end = line.indexOf('*/', i + 2);
                if (end >= 0) {
                    for (let j = i; j <= end + 1; j++) mask[j] = 1;
                    i = end + 2;
                    continue;
                } else {
                    for (let j = i; j < n; j++) mask[j] = 1;
                    return mask;
                }
            }
            if (line[i + 1] === '/') {
                for (let j = i; j < n; j++) mask[j] = 1;
                return mask;
            }
        }
        if (line[i] === '"' || line[i] === "'" || line[i] === '`') {
            const q = line[i];
            mask[i] = 1;
            let j = i + 1;
            while (j < n && line[j] !== q) {
                if (line[j] === '\\') {
                    mask[j] = 1;
                    j++;
                }
                mask[j] = 1;
                j++;
            }
            if (j < n) mask[j] = 1;
            i = j < n ? j + 1 : j;
            continue;
        }
        i++;
    }
    return mask;
}

export function findMatchingBracket(row, col) {
    const ch = st.buf[row] ? st.buf[row][col] : undefined;
    if (!ch || !BRACKET_PAIRS[ch]) return null;
    const open = '([{';
    const isOpen = open.indexOf(ch) >= 0;
    const target = BRACKET_PAIRS[ch];
    const dir = isOpen ? 1 : -1;
    let depth = 0;
    let r = row;
    let c = col;
    let steps = 0;
    // Get block state for this row from cache
    let inBlock = r < st._blockStateCache.length ? st._blockStateCache[r] : false;
    let mask = buildContextMask(st.buf[r], inBlock);
    while (r >= 0 && r < st.buf.length && steps < 5000) {
        const line = st.buf[r];
        while (c >= 0 && c < line.length) {
            if (!mask[c]) {
                const cur = line[c];
                if (cur === ch) depth++;
                else if (cur === target) depth--;
                if (depth === 0) return { y: r, x: c };
            }
            c += dir;
            steps++;
        }
        r += dir;
        if (r >= 0 && r < st.buf.length) {
            c = dir > 0 ? 0 : st.buf[r].length - 1;
            inBlock = r < st._blockStateCache.length ? st._blockStateCache[r] : false;
            mask = buildContextMask(st.buf[r], inBlock);
        }
    }
    return null;
}

export function getCachedBracketMatch() {
    if (
        st._bracketMatchCache &&
        st._bracketMatchCache.cy === st.cy &&
        st._bracketMatchCache.cx === st.cx
    ) {
        return st._bracketMatchCache.result;
    }
    const result = findMatchingBracket(st.cy, st.cx);
    st._bracketMatchCache = { cy: st.cy, cx: st.cx, result };
    return result;
}

// ─── Block comment state tracking ────────────────────────────────────────────

export function trackBlockState(line, inBlock) {
    let i = 0;
    const n = line.length;
    if (inBlock) {
        const end = line.indexOf('*/');
        if (end >= 0) {
            i = end + 2;
            inBlock = false;
        } else return true;
    }
    while (i < n) {
        const c = line[i];
        if (c === '/' && i + 1 < n) {
            if (line[i + 1] === '*') {
                const end = line.indexOf('*/', i + 2);
                if (end >= 0) {
                    i = end + 2;
                    continue;
                } else return true;
            }
            if (line[i + 1] === '/') return false;
        }
        if (c === '"' || c === "'" || c === '`') {
            let j = i + 1;
            while (j < n && line[j] !== c) {
                if (line[j] === '\\') j++;
                j++;
            }
            i = j < n ? j + 1 : j;
            continue;
        }
        i++;
    }
    return false;
}

// ─── Cache rebuilders ────────────────────────────────────────────────────────

function rebuildBracketDepthCache(upTo) {
    st._bracketDepthCache = new Array(upTo);
    let depth = 0;
    for (let r = 0; r < upTo; r++) {
        st._bracketDepthCache[r] = depth;
        const line = st.buf[r];
        const inBlock = r < st._blockStateCache.length ? st._blockStateCache[r] : false;
        const mask = buildContextMask(line, inBlock);
        for (let c = 0; c < line.length; c++) {
            if (mask[c]) continue;
            const ch = line[c];
            if (ch === '(' || ch === '[' || ch === '{') depth++;
            else if (ch === ')' || ch === ']' || ch === '}') depth--;
        }
    }
}

function rebuildBlockStateCache(upTo) {
    st._blockStateCache = new Array(upTo);
    let inBlock = false;
    for (let i = 0; i < upTo; i++) {
        st._blockStateCache[i] = inBlock;
        inBlock = trackBlockState(st.buf[i], inBlock);
    }
}

export function ensureCaches() {
    const needed = Math.min(st.oy + EROWS + 1, st.buf.length);
    if (st._lastCacheVersion !== st._bufVersion || needed > st._blockStateCache.length) {
        rebuildBlockStateCache(needed);
        rebuildBracketDepthCache(needed);
        st._bracketMatchCache = null;
        st._lastCacheVersion = st._bufVersion;
    }
}

// ─── Indent / line helpers ───────────────────────────────────────────────────

export function firstNonBlank(line) {
    const s = st.buf[line];
    let i = 0;
    while (i < s.length && (s[i] === ' ' || s[i] === '\t')) i++;
    return i;
}

export function getIndent(line) {
    const m = st.buf[line].match(/^(\s*)/);
    return m ? m[1] : '';
}

// ─── Vim word motions ────────────────────────────────────────────────────────

export function vimWordForward() {
    let line = st.buf[st.cy];
    if (st.cx >= line.length) {
        if (st.cy < st.buf.length - 1) {
            st.cy++;
            st.cx = 0;
            line = st.buf[st.cy];
        }
        while (st.cx < line.length && line[st.cx] === ' ') st.cx++;
        return;
    }
    if (isWordChar(line[st.cx])) {
        while (st.cx < line.length && isWordChar(line[st.cx])) st.cx++;
    } else if (line[st.cx] !== ' ') {
        while (st.cx < line.length && !isWordChar(line[st.cx]) && line[st.cx] !== ' ') st.cx++;
    }
    while (st.cx < line.length && line[st.cx] === ' ') st.cx++;
    if (st.cx >= line.length && st.cy < st.buf.length - 1) {
        st.cy++;
        st.cx = 0;
        line = st.buf[st.cy];
        while (st.cx < line.length && line[st.cx] === ' ') st.cx++;
    }
}

export function vimWordBack() {
    if (st.cx === 0) {
        if (st.cy > 0) {
            st.cy--;
            st.cx = st.buf[st.cy].length;
        }
        return;
    }
    const line = st.buf[st.cy];
    st.cx--;
    while (st.cx > 0 && line[st.cx] === ' ') st.cx--;
    if (isWordChar(line[st.cx])) {
        while (st.cx > 0 && isWordChar(line[st.cx - 1])) st.cx--;
    } else {
        while (st.cx > 0 && !isWordChar(line[st.cx - 1]) && line[st.cx - 1] !== ' ') st.cx--;
    }
}

export function vimWordEnd() {
    let line = st.buf[st.cy];
    if (st.cx >= line.length - 1) {
        if (st.cy < st.buf.length - 1) {
            st.cy++;
            st.cx = 0;
            line = st.buf[st.cy];
            while (st.cx < line.length && line[st.cx] === ' ') st.cx++;
        }
    } else {
        st.cx++;
    }
    line = st.buf[st.cy];
    while (st.cx < line.length && line[st.cx] === ' ') st.cx++;
    if (isWordChar(line[st.cx])) {
        while (st.cx < line.length - 1 && isWordChar(line[st.cx + 1])) st.cx++;
    } else {
        while (st.cx < line.length - 1 && !isWordChar(line[st.cx + 1]) && line[st.cx + 1] !== ' ')
            st.cx++;
    }
}
