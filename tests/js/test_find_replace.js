// ─── Find & Replace — pure function tests ───────────────────────────────────
//
// Self-contained: find/replace logic inlined from find.js.

var __failures = 0;
var __tests = 0;

function assert(cond, msg) {
    __tests++;
    if (!cond) {
        __failures++;
        throw new Error("ASSERT: " + (msg || "assertion failed"));
    }
}

function assertEq(a, b, msg) {
    __tests++;
    if (a !== b) {
        __failures++;
        throw new Error(
            "ASSERT_EQ: " +
                JSON.stringify(a) +
                " !== " +
                JSON.stringify(b) +
                (msg ? " — " + msg : ""),
        );
    }
}

// ─── Minimal state mock ──────────────────────────────────────────────────────

var st = {};

function resetState(lines, cx, cy, findText, replaceText) {
    st.buf = lines.slice();
    st.cx = cx || 0;
    st.cy = cy || 0;
    st.findText = findText || "";
    st.replaceText = replaceText || "";
    st.anchor = null;
    st.dirty = false;
}

// ─── Inlined findNext ────────────────────────────────────────────────────────

function findNext(dir) {
    if (!st.findText) return false;
    var total = st.buf.length;
    var startCol = st.cx + (dir > 0 ? 1 : 0);
    var startRow = st.cy;

    for (var i = 0; i < total; i++) {
        var li = (((st.cy + i * dir) % total) + total) % total;
        var line = st.buf[li];
        var col;

        if (i === 0) {
            col =
                dir > 0
                    ? line.indexOf(st.findText, startCol)
                    : line.lastIndexOf(st.findText, Math.max(0, startCol - 1));
        } else {
            col = dir > 0 ? line.indexOf(st.findText) : line.lastIndexOf(st.findText);
        }

        if (col >= 0) {
            var wrapped = dir > 0 ? li < startRow : li > startRow;
            st.cy = li;
            st.cx = col + st.findText.length;
            st.anchor = { x: col, y: li };
            return wrapped ? "wrapped" : "found";
        }
    }
    return false;
}

// ─── Inlined replaceAll ──────────────────────────────────────────────────────

function replaceAll() {
    if (!st.findText) return 0;
    var count = 0;
    for (var i = 0; i < st.buf.length; i++) {
        var parts = st.buf[i].split(st.findText);
        if (parts.length > 1) {
            count += parts.length - 1;
            st.buf[i] = parts.join(st.replaceText);
        }
    }
    st.anchor = null;
    if (count) st.dirty = true;
    return count;
}

// ─── Inlined replaceCurrent ──────────────────────────────────────────────────

function replaceCurrent() {
    if (!st.findText || !st.anchor) return false;
    var ax = st.anchor.x;
    var ay = st.anchor.y;
    var bx = st.cx;
    var by = st.cy;
    // Only single-line selections
    if (ay !== by) return false;
    if (ax > bx) {
        var tmp = ax;
        ax = bx;
        bx = tmp;
    }
    var sel = st.buf[ay].slice(ax, bx);
    if (sel === st.findText) {
        st.buf[ay] = st.buf[ay].slice(0, ax) + st.replaceText + st.buf[ay].slice(bx);
        st.cx = ax + st.replaceText.length;
        st.cy = ay;
        st.anchor = null;
        st.dirty = true;
        return true;
    }
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
//  findNext — forward
// ═════════════════════════════════════════════════════════════════════════════

// Simple find on same line
(function test_find_same_line() {
    resetState(["hello world foo"], 0, 0, "world");
    var r = findNext(1);
    assertEq(r, "found", "should find");
    assertEq(st.cy, 0, "same line");
    assertEq(st.cx, 11, "cursor after match");
    assertEq(st.anchor.x, 6, "anchor at match start");
})();

// Find on next line
(function test_find_next_line() {
    resetState(["aaa", "bbb", "ccc"], 0, 0, "bbb");
    var r = findNext(1);
    assertEq(r, "found", "found on line 1");
    assertEq(st.cy, 1, "line 1");
    assertEq(st.cx, 3, "end of match");
})();

// Find wraps around
(function test_find_wraps() {
    resetState(["hello", "world"], 0, 1, "hello");
    var r = findNext(1);
    assertEq(r, "wrapped", "should wrap");
    assertEq(st.cy, 0, "wrapped to line 0");
})();

// Find not found
(function test_find_not_found() {
    resetState(["abc", "def"], 0, 0, "xyz");
    var r = findNext(1);
    assertEq(r, false, "not found");
})();

// Empty find text
(function test_find_empty() {
    resetState(["abc"], 0, 0, "");
    var r = findNext(1);
    assertEq(r, false, "empty search returns false");
})();

// Multiple occurrences — advances past current
(function test_find_advances() {
    resetState(["aa aa aa"], 0, 0, "aa");
    // cx=0, startCol=1, indexOf('aa',1) finds at col 3
    var r1 = findNext(1);
    assertEq(r1, "found");
    assertEq(st.cx, 5, "first match end (col 3 + len 2)");
    // cx=5, startCol=6, indexOf('aa',6) finds at col 6
    var r2 = findNext(1);
    assertEq(r2, "found");
    assertEq(st.cx, 8, "second match end (col 6 + len 2)");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  findNext — backward (dir = -1)
// ═════════════════════════════════════════════════════════════════════════════

(function test_find_backward() {
    resetState(["abc", "def", "ghi"], 3, 2, "abc");
    // cx=3, dir=-1, startCol=3, i=0: li=2, line='ghi', lastIndexOf('abc',2) → -1
    // i=1: li=1, line='def', lastIndexOf('abc') → -1
    // i=2: li=0, line='abc', lastIndexOf('abc') → 0 → found, li=0 < startRow=2 → not wrapped (backward: wrapped = li > startRow)
    var r = findNext(-1);
    assertEq(r, "found", "backward found");
    assertEq(st.cy, 0, "found on line 0");
})();

(function test_find_backward_wraps() {
    resetState(["abc", "def", "ghi"], 0, 0, "ghi");
    var r = findNext(-1);
    assertEq(r, "wrapped", "backward wraps");
    assertEq(st.cy, 2, "wrapped to line 2");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  replaceAll
// ═════════════════════════════════════════════════════════════════════════════

(function test_replace_all_basic() {
    resetState(["hello world", "world world"], 0, 0, "world", "earth");
    var count = replaceAll();
    assertEq(count, 3, "3 occurrences replaced");
    assertEq(st.buf[0], "hello earth", "line 0 replaced");
    assertEq(st.buf[1], "earth earth", "line 1 replaced");
    assert(st.dirty, "dirty flag set");
})();

(function test_replace_all_no_match() {
    resetState(["hello"], 0, 0, "xyz", "abc");
    var count = replaceAll();
    assertEq(count, 0, "no matches");
    assertEq(st.buf[0], "hello", "unchanged");
    assert(!st.dirty, "dirty not set");
})();

(function test_replace_all_empty_replacement() {
    resetState(["aXbXc"], 0, 0, "X", "");
    var count = replaceAll();
    assertEq(count, 2, "2 occurrences");
    assertEq(st.buf[0], "abc", "Xs removed");
})();

(function test_replace_all_longer() {
    resetState(["ab"], 0, 0, "a", "123");
    var count = replaceAll();
    assertEq(count, 1, "1 occurrence");
    assertEq(st.buf[0], "123b", "replaced with longer");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  replaceCurrent
// ═════════════════════════════════════════════════════════════════════════════

(function test_replace_current_match() {
    resetState(["hello world"], 11, 0, "world", "earth");
    st.anchor = { x: 6, y: 0 };
    var r = replaceCurrent();
    assert(r, "replaced");
    assertEq(st.buf[0], "hello earth");
    assertEq(st.cx, 11, "cursor at end of replacement");
    assert(st.dirty, "dirty set");
    assertEq(st.anchor, null, "anchor cleared");
})();

(function test_replace_current_no_anchor() {
    resetState(["hello world"], 5, 0, "world", "earth");
    var r = replaceCurrent();
    assert(!r, "no anchor");
})();

(function test_replace_current_mismatch() {
    resetState(["hello world"], 11, 0, "xyz", "abc");
    st.anchor = { x: 6, y: 0 };
    var r = replaceCurrent();
    assert(!r, "selection does not match findText");
    assertEq(st.buf[0], "hello world", "unchanged");
})();
