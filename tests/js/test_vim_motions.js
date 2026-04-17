// ─── Vim word motions — pure function tests ─────────────────────────────────
//
// Self-contained: motion functions inlined from helpers.js.

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

var st = { buf: [], cx: 0, cy: 0 };

function reset(lines, cx, cy) {
    st.buf = lines.slice();
    st.cx = cx;
    st.cy = cy;
}

// ─── Inlined helpers from helpers.js ─────────────────────────────────────────

function isWordChar(c) {
    return (
        (c >= "a" && c <= "z") ||
        (c >= "A" && c <= "Z") ||
        (c >= "0" && c <= "9") ||
        c === "_" ||
        c === "$"
    );
}

function vimWordForward() {
    var line = st.buf[st.cy];
    if (st.cx >= line.length) {
        if (st.cy < st.buf.length - 1) {
            st.cy++;
            st.cx = 0;
            line = st.buf[st.cy];
        }
        while (st.cx < line.length && line[st.cx] === " ") st.cx++;
        return;
    }
    if (isWordChar(line[st.cx])) {
        while (st.cx < line.length && isWordChar(line[st.cx])) st.cx++;
    } else if (line[st.cx] !== " ") {
        while (st.cx < line.length && !isWordChar(line[st.cx]) && line[st.cx] !== " ") st.cx++;
    }
    while (st.cx < line.length && line[st.cx] === " ") st.cx++;
    if (st.cx >= line.length && st.cy < st.buf.length - 1) {
        st.cy++;
        st.cx = 0;
        line = st.buf[st.cy];
        while (st.cx < line.length && line[st.cx] === " ") st.cx++;
    }
}

function vimWordBack() {
    if (st.cx === 0) {
        if (st.cy > 0) {
            st.cy--;
            st.cx = st.buf[st.cy].length;
        }
        return;
    }
    var line = st.buf[st.cy];
    st.cx--;
    while (st.cx > 0 && line[st.cx] === " ") st.cx--;
    if (isWordChar(line[st.cx])) {
        while (st.cx > 0 && isWordChar(line[st.cx - 1])) st.cx--;
    } else {
        while (st.cx > 0 && !isWordChar(line[st.cx - 1]) && line[st.cx - 1] !== " ") st.cx--;
    }
}

function vimWordEnd() {
    var line = st.buf[st.cy];
    if (st.cx >= line.length - 1) {
        if (st.cy < st.buf.length - 1) {
            st.cy++;
            st.cx = 0;
            line = st.buf[st.cy];
            while (st.cx < line.length && line[st.cx] === " ") st.cx++;
        }
    } else {
        st.cx++;
    }
    line = st.buf[st.cy];
    while (st.cx < line.length && line[st.cx] === " ") st.cx++;
    if (isWordChar(line[st.cx])) {
        while (st.cx < line.length - 1 && isWordChar(line[st.cx + 1])) st.cx++;
    } else {
        while (st.cx < line.length - 1 && !isWordChar(line[st.cx + 1]) && line[st.cx + 1] !== " ")
            st.cx++;
    }
}

// ─── Also test wordBoundaryLeft / wordBoundaryRight ──────────────────────────

function wordBoundaryLeft(line, col) {
    var c = col;
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
    var c = col;
    if (c >= line.length) return line.length;
    if (isWordChar(line[c])) {
        while (c < line.length && isWordChar(line[c])) c++;
    } else if (line[c] !== " ") {
        while (c < line.length && !isWordChar(line[c]) && line[c] !== " ") c++;
    }
    while (c < line.length && line[c] === " ") c++;
    return c;
}

// ═════════════════════════════════════════════════════════════════════════════
//  vimWordForward (w)
// ═════════════════════════════════════════════════════════════════════════════

// Basic: jump over a word
(function test_w_basic() {
    reset(["hello world"], 0, 0);
    vimWordForward();
    assertEq(st.cx, 6, 'w: past "hello" + space → 6');
    assertEq(st.cy, 0);
})();

// Past punctuation group
(function test_w_punctuation() {
    reset(["foo++bar"], 0, 0);
    vimWordForward();
    assertEq(st.cx, 3, "w: stops at punctuation boundary");
})();

// From punctuation to next word
(function test_w_from_punct() {
    reset(["foo++bar"], 3, 0);
    vimWordForward();
    assertEq(st.cx, 5, "w: past ++ to bar");
})();

// End-of-line wraps to next line
(function test_w_next_line() {
    reset(["hello", "world"], 5, 0);
    vimWordForward();
    assertEq(st.cy, 1, "w: moves to next line");
    assertEq(st.cx, 0, "w: start of next line");
})();

// Leading spaces on next line are skipped
(function test_w_skip_indent() {
    reset(["end", "  start"], 3, 0);
    vimWordForward();
    assertEq(st.cy, 1);
    assertEq(st.cx, 2, "w: skips leading spaces");
})();

// Last line, at end — stays
(function test_w_last_line_end() {
    reset(["only"], 4, 0);
    vimWordForward();
    assertEq(st.cy, 0);
    assertEq(st.cx, 4, "w: no-op at end of last line");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  vimWordBack (b)
// ═════════════════════════════════════════════════════════════════════════════

// Basic back
(function test_b_basic() {
    reset(["hello world"], 6, 0);
    vimWordBack();
    assertEq(st.cx, 0, 'b: back to "hello"');
})();

// Back from mid-word
(function test_b_mid_word() {
    reset(["hello world"], 8, 0);
    vimWordBack();
    assertEq(st.cx, 6, 'b: back to start of "world"');
})();

// Back at col 0 wraps to prev line
(function test_b_prev_line() {
    reset(["hello", "world"], 0, 1);
    vimWordBack();
    assertEq(st.cy, 0);
    assertEq(st.cx, 5, "b: end of previous line");
})();

// Back past spaces
(function test_b_spaces() {
    reset(["aaa   bbb"], 6, 0);
    vimWordBack();
    assertEq(st.cx, 0, 'b: back past spaces to "aaa"');
})();

// At start of buffer — stays
(function test_b_start() {
    reset(["hello"], 0, 0);
    vimWordBack();
    assertEq(st.cx, 0);
    assertEq(st.cy, 0, "b: no-op at buffer start");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  vimWordEnd (e)
// ═════════════════════════════════════════════════════════════════════════════

// Basic end
(function test_e_basic() {
    reset(["hello world"], 0, 0);
    vimWordEnd();
    assertEq(st.cx, 4, 'e: end of "hello"');
})();

// From end-of-word, jump to next word end
(function test_e_next_word() {
    reset(["hello world"], 4, 0);
    vimWordEnd();
    assertEq(st.cx, 10, 'e: end of "world"');
})();

// End wraps to next line
(function test_e_next_line() {
    reset(["hi", "there"], 1, 0);
    vimWordEnd();
    assertEq(st.cy, 1, "e: next line");
    assertEq(st.cx, 4, 'e: end of "there"');
})();

// Skips leading spaces
(function test_e_skip_spaces() {
    reset(["go", "   to"], 1, 0);
    vimWordEnd();
    assertEq(st.cy, 1);
    assertEq(st.cx, 4, 'e: end of "to" past spaces');
})();

// ═════════════════════════════════════════════════════════════════════════════
//  wordBoundaryLeft / wordBoundaryRight (Ctrl+Left / Ctrl+Right)
// ═════════════════════════════════════════════════════════════════════════════

(function test_wbl_basic() {
    assertEq(wordBoundaryLeft("hello world", 6), 0, 'wbl: back from "w" to "h"');
})();

(function test_wbl_at_zero() {
    assertEq(wordBoundaryLeft("hello", 0), 0, "wbl: already at 0");
})();

(function test_wbl_mid_word() {
    assertEq(wordBoundaryLeft("hello world", 9), 6, 'wbl: mid-"world" back to 6');
})();

(function test_wbr_basic() {
    assertEq(wordBoundaryRight("hello world", 0), 6, 'wbr: past "hello" + space');
})();

(function test_wbr_from_space() {
    assertEq(wordBoundaryRight("hello world", 5), 6, 'wbr: from space to "world"');
})();

(function test_wbr_at_end() {
    assertEq(wordBoundaryRight("hello", 5), 5, "wbr: at end stays");
})();

(function test_wbr_punctuation() {
    assertEq(wordBoundaryRight("a++b", 0), 1, "wbr: word char stops at punct");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  Vim G and gg — go-to-line motions
// ═════════════════════════════════════════════════════════════════════════════

function firstNonBlank(row) {
    var line = st.buf[row];
    for (var i = 0; i < line.length; i++) {
        if (line[i] !== " ") return i;
    }
    return line.length;
}

// G with no count (count=1 by default) should go to line 1 (index 0)
(function test_G_no_count() {
    reset(["line0", "line1", "line2"], 0, 2);
    // G with count=1 (the default when user just types G without a number)
    // In real vim, bare G goes to last line. Our impl uses count default of 1,
    // so 1G goes to line 0 (first line).
    st.cy = Math.min(1 - 1, st.buf.length - 1);
    st.cx = firstNonBlank(st.cy);
    assertEq(st.cy, 0, "1G should go to first line (index 0)");
})();

// 3G should go to line 3 (index 2)
(function test_G_with_count() {
    reset(["aaa", "bbb", "ccc", "ddd"], 0, 0);
    var count = 3;
    st.cy = Math.min(count - 1, st.buf.length - 1);
    st.cx = firstNonBlank(st.cy);
    assertEq(st.cy, 2, "3G should go to line index 2");
})();

// gg with no extra count goes to first line
(function test_gg_goes_to_first_line() {
    reset(["aaa", "bbb", "ccc"], 0, 2);
    // gg with opCount=1 → since opCount is not > 1, go to line 0
    var opCount = 1;
    st.cy = opCount > 1 ? Math.min(opCount - 1, st.buf.length - 1) : 0;
    st.cx = firstNonBlank(st.cy);
    assertEq(st.cy, 0, "gg should go to first line");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  Vim paste with newlines
// ═════════════════════════════════════════════════════════════════════════════

// Paste single-line text (p)
(function test_paste_single_line() {
    reset(["hello world"], 4, 0);
    var reg = "XY";
    var pos = Math.min(st.cx + 1, st.buf[st.cy].length);
    st.buf[st.cy] = st.buf[st.cy].slice(0, pos) + reg + st.buf[st.cy].slice(pos);
    st.cx = pos + reg.length - 1;
    assertEq(st.buf[0], "helloXY world", "single-line paste inserts after cursor");
    assertEq(st.cx, 6, "cursor at end of pasted text");
})();

// Paste multi-line text (p) — should split the line
(function test_paste_multi_line() {
    reset(["abcdef"], 2, 0);
    var reg = "X\nY\nZ";
    var lines = reg.split("\n");
    var pos = Math.min(st.cx + 1, st.buf[st.cy].length);
    var head = st.buf[st.cy].slice(0, pos);
    var tail = st.buf[st.cy].slice(pos);
    st.buf[st.cy] = head + lines[0];
    for (var i = 1; i < lines.length - 1; i++) {
        st.buf.splice(st.cy + i, 0, lines[i]);
    }
    var lastIdx = st.cy + lines.length - 1;
    st.buf.splice(lastIdx, 0, lines[lines.length - 1] + tail);
    st.cy = lastIdx;
    st.cx = lines[lines.length - 1].length - 1;
    if (st.cx < 0) st.cx = 0;

    assertEq(st.buf.length, 3, "should split into 3 lines");
    assertEq(st.buf[0], "abcX", "first line = head + first paste line");
    assertEq(st.buf[1], "Y", "middle paste line");
    assertEq(st.buf[2], "Zdef", "last paste line + tail");
    assertEq(st.cy, 2, "cursor on last pasted line");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  Vim backward search
// ═════════════════════════════════════════════════════════════════════════════

// Backward search should find match at cursor position - 1
(function test_backward_search_same_line() {
    reset(["abcabc"], 3, 0);
    var search = "abc";
    // backward search: startCol = cx + dir = cx + (-1) = cx - 1
    // col = line.lastIndexOf(search, Math.max(0, cx - 1))
    // lastIndexOf("abc", 2) should return 0 (matches at 0..2)
    var line = st.buf[st.cy];
    var col = line.lastIndexOf(search, Math.max(0, st.cx - 1));
    assert(col >= 0, "should find match");
    assertEq(col, 0, "backward search should find at col 0");
})();
