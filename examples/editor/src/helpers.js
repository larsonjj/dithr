// ─── Helpers ─────────────────────────────────────────────────────────────────

function clamp(v, lo, hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

function status(text, sec) {
    msg = text;
    msgT = sec || 3;
}

function resetBlink() {
    blink = 0;
    curOn = true;
}

function ensureVisible() {
    let newOy = oy;
    if (cy < newOy + SCROLL_MARGIN) newOy = Math.max(0, cy - SCROLL_MARGIN);
    if (cy > newOy + EROWS - 1 - SCROLL_MARGIN) newOy = cy - EROWS + 1 + SCROLL_MARGIN;
    newOy = clamp(newOy, 0, Math.max(0, buf.length - EROWS));
    oy = newOy;
    targetOy = newOy;
    if (cx < ox) ox = cx;
    if (cx >= ox + ECOLS) ox = cx - ECOLS + 1;
}

// ─── Word helpers ────────────────────────────────────────────────────────────

function isWordChar(c) {
    return (
        (c >= "a" && c <= "z") ||
        (c >= "A" && c <= "Z") ||
        (c >= "0" && c <= "9") ||
        c === "_" ||
        c === "$"
    );
}

function wordBoundaryLeft(line, col) {
    let c = col;
    if (c <= 0) return 0;
    c--;
    while (c > 0 && line[c] === " ") c--;
    if (isWordChar(line[c])) {
        while (c > 0 && isWordChar(line[c - 1])) c--;
    } else {
        while (c > 0 && !isWordChar(line[c - 1]) && line[c - 1] !== " ") c--;
    }
    return c;
}

function wordBoundaryRight(line, col) {
    let c = col;
    if (c >= line.length) return line.length;
    if (isWordChar(line[c])) {
        while (c < line.length && isWordChar(line[c])) c++;
    } else if (line[c] !== " ") {
        while (c < line.length && !isWordChar(line[c]) && line[c] !== " ") c++;
    }
    while (c < line.length && line[c] === " ") c++;
    return c;
}

// ─── Bracket matching ───────────────────────────────────────────────────────

function findMatchingBracket(row, col) {
    let ch = buf[row] ? buf[row][col] : undefined;
    if (!ch || !BRACKET_PAIRS[ch]) return null;
    let open = "([{";
    let isOpen = open.indexOf(ch) >= 0;
    let target = BRACKET_PAIRS[ch];
    let dir = isOpen ? 1 : -1;
    let depth = 0;
    let r = row,
        c = col;
    // Limit search to 5000 characters to avoid freezing on large files
    let steps = 0;
    while (r >= 0 && r < buf.length && steps < 5000) {
        let line = buf[r];
        while (c >= 0 && c < line.length) {
            let cur = line[c];
            if (cur === ch) depth++;
            else if (cur === target) depth--;
            if (depth === 0) return { y: r, x: c };
            c += dir;
            steps++;
        }
        r += dir;
        if (r >= 0 && r < buf.length) {
            c = dir > 0 ? 0 : buf[r].length - 1;
        }
    }
    return null;
}

function getCachedBracketMatch() {
    if (_bracketMatchCache && _bracketMatchCache.cy === cy && _bracketMatchCache.cx === cx) {
        return _bracketMatchCache.result;
    }
    let result = findMatchingBracket(cy, cx);
    _bracketMatchCache = { cy, cx, result };
    return result;
}

// ─── Block comment state tracking ────────────────────────────────────────────

// Lightweight block-comment state tracker — no object allocation.
// Returns true if the line ends inside a block comment.
function trackBlockState(line, inBlock) {
    let i = 0,
        n = line.length;
    if (inBlock) {
        let end = line.indexOf("*/");
        if (end >= 0) {
            i = end + 2;
            inBlock = false;
        } else return true;
    }
    while (i < n) {
        let c = line[i];
        if (c === "/" && i + 1 < n) {
            if (line[i + 1] === "*") {
                let end = line.indexOf("*/", i + 2);
                if (end >= 0) {
                    i = end + 2;
                    continue;
                } else return true;
            }
            if (line[i + 1] === "/") return false;
        }
        if (c === '"' || c === "'" || c === "`") {
            let j = i + 1;
            while (j < n && line[j] !== c) {
                if (line[j] === "\\") j++;
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
    _bracketDepthCache = new Array(upTo);
    let depth = 0;
    for (let r = 0; r < upTo; r++) {
        _bracketDepthCache[r] = depth;
        let line = buf[r];
        for (let c = 0; c < line.length; c++) {
            let ch = line[c];
            if (ch === "(" || ch === "[" || ch === "{") depth++;
            else if (ch === ")" || ch === "]" || ch === "}") depth--;
        }
    }
}

function rebuildBlockStateCache(upTo) {
    _blockStateCache = new Array(upTo);
    let inBlock = false;
    for (let i = 0; i < upTo; i++) {
        _blockStateCache[i] = inBlock;
        inBlock = trackBlockState(buf[i], inBlock);
    }
}

function ensureCaches() {
    // Only rebuild up to the last visible line
    let needed = Math.min(oy + EROWS + 1, buf.length);
    if (_lastCacheVersion !== _bufVersion || needed > _blockStateCache.length) {
        rebuildBlockStateCache(needed);
        rebuildBracketDepthCache(needed);
        _bracketMatchCache = null;
        _lastCacheVersion = _bufVersion;
    }
}

// ─── Indent / line helpers ───────────────────────────────────────────────────

function firstNonBlank(line) {
    let s = buf[line];
    let i = 0;
    while (i < s.length && (s[i] === " " || s[i] === "\t")) i++;
    return i;
}

function getIndent(line) {
    let m = buf[line].match(/^(\s*)/);
    return m ? m[1] : "";
}

// ─── Vim word motions ────────────────────────────────────────────────────────

function vimWordForward() {
    let line = buf[cy];
    if (cx >= line.length) {
        if (cy < buf.length - 1) {
            cy++;
            cx = 0;
            line = buf[cy];
        }
        while (cx < line.length && line[cx] === " ") cx++;
        return;
    }
    if (isWordChar(line[cx])) {
        while (cx < line.length && isWordChar(line[cx])) cx++;
    } else if (line[cx] !== " ") {
        while (cx < line.length && !isWordChar(line[cx]) && line[cx] !== " ") cx++;
    }
    while (cx < line.length && line[cx] === " ") cx++;
    if (cx >= line.length && cy < buf.length - 1) {
        cy++;
        cx = 0;
        line = buf[cy];
        while (cx < line.length && line[cx] === " ") cx++;
    }
}

function vimWordBack() {
    if (cx === 0) {
        if (cy > 0) {
            cy--;
            cx = buf[cy].length;
        }
        return;
    }
    let line = buf[cy];
    cx--;
    while (cx > 0 && line[cx] === " ") cx--;
    if (isWordChar(line[cx])) {
        while (cx > 0 && isWordChar(line[cx - 1])) cx--;
    } else {
        while (cx > 0 && !isWordChar(line[cx - 1]) && line[cx - 1] !== " ") cx--;
    }
}

function vimWordEnd() {
    let line = buf[cy];
    if (cx >= line.length - 1) {
        if (cy < buf.length - 1) {
            cy++;
            cx = 0;
            line = buf[cy];
            while (cx < line.length && line[cx] === " ") cx++;
        }
    } else {
        cx++;
    }
    line = buf[cy];
    while (cx < line.length && line[cx] === " ") cx++;
    if (isWordChar(line[cx])) {
        while (cx < line.length - 1 && isWordChar(line[cx + 1])) cx++;
    } else {
        while (cx < line.length - 1 && !isWordChar(line[cx + 1]) && line[cx + 1] !== " ") cx++;
    }
}
