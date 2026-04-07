// ─── Editor helpers — pure function tests ────────────────────────────────────
//
// Self-contained: functions under test are inlined (no ES module imports)
// so this file can be evaluated by a bare QuickJS context.

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

function assertArrayEq(a, b, msg) {
    __tests++;
    if (a.length !== b.length) {
        __failures++;
        throw new Error(
            "ASSERT_ARRAY_EQ length: " + a.length + " !== " + b.length + (msg ? " — " + msg : ""),
        );
    }
    for (var i = 0; i < a.length; i++) {
        if (a[i] !== b[i]) {
            __failures++;
            throw new Error(
                "ASSERT_ARRAY_EQ [" + i + "]: " + a[i] + " !== " + b[i] + (msg ? " — " + msg : ""),
            );
        }
    }
}

// ─── Functions under test (inlined from helpers.js) ──────────────────────────

function clamp(v, lo, hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

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

function buildContextMask(line, inBlock) {
    var mask = new Uint8Array(line.length);
    var i = 0,
        n = line.length;
    if (inBlock) {
        var end = line.indexOf("*/");
        if (end >= 0) {
            for (var j = 0; j <= end + 1; j++) mask[j] = 1;
            i = end + 2;
        } else {
            mask.fill(1);
            return mask;
        }
    }
    while (i < n) {
        if (line[i] === "/" && i + 1 < n) {
            if (line[i + 1] === "*") {
                var end2 = line.indexOf("*/", i + 2);
                if (end2 >= 0) {
                    for (var j2 = i; j2 <= end2 + 1; j2++) mask[j2] = 1;
                    i = end2 + 2;
                    continue;
                } else {
                    for (var j3 = i; j3 < n; j3++) mask[j3] = 1;
                    return mask;
                }
            }
            if (line[i + 1] === "/") {
                for (var j4 = i; j4 < n; j4++) mask[j4] = 1;
                return mask;
            }
        }
        if (line[i] === '"' || line[i] === "'" || line[i] === "`") {
            var q = line[i];
            mask[i] = 1;
            var k = i + 1;
            while (k < n && line[k] !== q) {
                if (line[k] === "\\") {
                    mask[k] = 1;
                    k++;
                }
                mask[k] = 1;
                k++;
            }
            if (k < n) mask[k] = 1;
            i = k < n ? k + 1 : k;
            continue;
        }
        i++;
    }
    return mask;
}

function trackBlockState(line, inBlock) {
    var i = 0,
        n = line.length;
    if (inBlock) {
        var end = line.indexOf("*/");
        if (end >= 0) {
            i = end + 2;
            inBlock = false;
        } else return true;
    }
    while (i < n) {
        var c = line[i];
        if (c === "/" && i + 1 < n) {
            if (line[i + 1] === "*") {
                var end2 = line.indexOf("*/", i + 2);
                if (end2 >= 0) {
                    i = end2 + 2;
                    continue;
                } else return true;
            }
            if (line[i + 1] === "/") return false;
        }
        if (c === '"' || c === "'" || c === "`") {
            var k = i + 1;
            while (k < n && line[k] !== c) {
                if (line[k] === "\\") k++;
                k++;
            }
            i = k < n ? k + 1 : k;
            continue;
        }
        i++;
    }
    return false;
}

// ─── clamp tests ─────────────────────────────────────────────────────────────

assertEq(clamp(5, 0, 10), 5, "clamp mid");
assertEq(clamp(-1, 0, 10), 0, "clamp below");
assertEq(clamp(11, 0, 10), 10, "clamp above");
assertEq(clamp(0, 0, 10), 0, "clamp at lo");
assertEq(clamp(10, 0, 10), 10, "clamp at hi");
assertEq(clamp(5, 5, 5), 5, "clamp equal bounds");

// ─── isWordChar tests ────────────────────────────────────────────────────────

assert(isWordChar("a"), "lowercase letter");
assert(isWordChar("Z"), "uppercase letter");
assert(isWordChar("0"), "digit");
assert(isWordChar("_"), "underscore");
assert(isWordChar("$"), "dollar sign");
assert(!isWordChar(" "), "space is not word char");
assert(!isWordChar("("), "paren is not word char");
assert(!isWordChar("+"), "plus is not word char");
assert(!isWordChar("."), "dot is not word char");

// ─── wordBoundaryLeft tests ──────────────────────────────────────────────────

assertEq(wordBoundaryLeft("hello world", 0), 0, "wbl at start");
assertEq(wordBoundaryLeft("hello world", 5), 0, "wbl from space back to word start");
assertEq(wordBoundaryLeft("hello world", 6), 0, "wbl from w skips space to word start");
assertEq(wordBoundaryLeft("hello world", 11), 6, "wbl from end of world");
assertEq(wordBoundaryLeft("  abc", 2), 0, "wbl skip leading spaces");
assertEq(wordBoundaryLeft("abc.def", 4), 3, "wbl stops at punctuation");

// ─── wordBoundaryRight tests ─────────────────────────────────────────────────

assertEq(wordBoundaryRight("hello world", 0), 6, "wbr from h to past space");
assertEq(wordBoundaryRight("hello world", 5), 6, "wbr from space");
assertEq(wordBoundaryRight("hello world", 6), 11, "wbr from w to end");
assertEq(wordBoundaryRight("hello world", 11), 11, "wbr at end");
assertEq(wordBoundaryRight("abc.def", 3), 4, "wbr stops after punctuation");

// ─── buildContextMask tests ──────────────────────────────────────────────────

// Plain code — no masking
(function () {
    var m = buildContextMask("let x = 1;", false);
    for (var i = 0; i < m.length; i++) assertEq(m[i], 0, "plain code pos " + i);
})();

// Double-quoted string
(function () {
    //                     0123456789
    var m = buildContextMask('let x = "hi";', false);
    // Positions 8-12 are inside or are the quotes: "hi"
    assertEq(m[7], 0, "space before string");
    assertEq(m[8], 1, 'opening "');
    assertEq(m[9], 1, "h inside string");
    assertEq(m[10], 1, "i inside string");
    assertEq(m[11], 1, 'closing "');
    assertEq(m[12], 0, "semicolon after string");
})();

// Single-quoted string
(function () {
    var m = buildContextMask("x = 'ab';", false);
    assertEq(m[4], 1, "open quote");
    assertEq(m[5], 1, "a in string");
    assertEq(m[6], 1, "b in string");
    assertEq(m[7], 1, "close quote");
})();

// Escape inside string
(function () {
    var m = buildContextMask('x = "a\\"b";', false);
    assertEq(m[4], 1, "open quote");
    assertEq(m[5], 1, "a");
    assertEq(m[6], 1, "backslash");
    assertEq(m[7], 1, "escaped quote");
    assertEq(m[8], 1, "b");
    assertEq(m[9], 1, "close quote");
    assertEq(m[10], 0, "semicolon outside");
})();

// Line comment
(function () {
    var m = buildContextMask("x = 1; // comment", false);
    assertEq(m[5], 0, "semicolon before comment");
    assertEq(m[7], 1, "first /");
    assertEq(m[8], 1, "second /");
    assertEq(m[9], 1, "space in comment");
    assertEq(m[16], 1, "end of comment");
})();

// Block comment (same line)
(function () {
    var m = buildContextMask("x /* hi */ y", false);
    assertEq(m[0], 0, "x before comment");
    assertEq(m[2], 1, "slash of /*");
    assertEq(m[3], 1, "star of /*");
    assertEq(m[5], 1, "inside comment");
    assertEq(m[8], 1, "star of */");
    assertEq(m[9], 1, "slash of */");
    assertEq(m[11], 0, "y after comment");
})();

// Block comment continuation (inBlock=true)
(function () {
    var m = buildContextMask("still in comment */ code", true);
    assertEq(m[0], 1, "start of block");
    assertEq(m[17], 1, "slash of */");
    assertEq(m[19], 0, "code after close");
})();

// Block comment continuation with no close
(function () {
    var m = buildContextMask("all in comment", true);
    for (var i = 0; i < m.length; i++) assertEq(m[i], 1, "all masked pos " + i);
})();

// Unterminated block comment
(function () {
    var m = buildContextMask("x /* no close", false);
    assertEq(m[0], 0, "x before");
    assertEq(m[2], 1, "slash of /*");
    assertEq(m[12], 1, "end still in comment");
})();

// Bracket inside string should be masked
(function () {
    var m = buildContextMask('func("("))', false);
    assertEq(m[4], 0, "real open paren");
    assertEq(m[5], 1, "open quote");
    assertEq(m[6], 1, "paren in string");
    assertEq(m[7], 1, "close quote");
    assertEq(m[8], 0, "real close paren 1");
    assertEq(m[9], 0, "real close paren 2");
})();

// ─── trackBlockState tests ───────────────────────────────────────────────────

assertEq(trackBlockState("let x = 1;", false), false, "plain code");
assertEq(trackBlockState("x = 1; /* open", false), true, "opens block");
assertEq(trackBlockState("still open", true), true, "continues block");
assertEq(trackBlockState("end */ x = 1;", true), false, "closes block");
assertEq(trackBlockState("/* open */ /* open2", false), true, "reopen after close");
assertEq(trackBlockState("x = 1; // comment", false), false, "line comment doesn't open block");
assertEq(trackBlockState('x = "/*";', false), false, "block comment in string");
