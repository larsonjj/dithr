// ─── Tokenizer — pure function tests ─────────────────────────────────────────
//
// Self-contained: tokenize() inlined from tokenizer.js with config constants.

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

// ─── Inlined constants ───────────────────────────────────────────────────────

var FG = 7;
var KWCOL = 12;
var STRCOL = 11;
var COMCOL = 5;
var NUMCOL = 9;

var KEYWORDS = {};
var kwList = [
    "break",
    "case",
    "catch",
    "class",
    "const",
    "continue",
    "debugger",
    "default",
    "delete",
    "do",
    "else",
    "export",
    "extends",
    "false",
    "finally",
    "for",
    "function",
    "if",
    "import",
    "in",
    "instanceof",
    "let",
    "new",
    "null",
    "of",
    "return",
    "static",
    "super",
    "switch",
    "this",
    "throw",
    "true",
    "try",
    "typeof",
    "undefined",
    "var",
    "void",
    "while",
    "with",
    "yield",
];
for (var ki = 0; ki < kwList.length; ki++) KEYWORDS[kwList[ki]] = true;

// ─── Inlined: tokenize ──────────────────────────────────────────────────────

function tokenize(line, inBlock) {
    var toks = [];
    var i = 0;
    var n = line.length;

    if (inBlock) {
        var end = line.indexOf("*/");
        if (end >= 0) {
            toks.push({ text: line.slice(0, end + 2), col: COMCOL });
            i = end + 2;
            inBlock = false;
        } else {
            toks.push({ text: line, col: COMCOL });
            return { toks: toks, inBlock: true };
        }
    }

    while (i < n) {
        if (line[i] === "/" && i + 1 < n && line[i + 1] === "*") {
            var end2 = line.indexOf("*/", i + 2);
            if (end2 >= 0) {
                toks.push({ text: line.slice(i, end2 + 2), col: COMCOL });
                i = end2 + 2;
                continue;
            } else {
                toks.push({ text: line.slice(i), col: COMCOL });
                return { toks: toks, inBlock: true };
            }
        }

        if (line[i] === "/" && i + 1 < n && line[i + 1] === "/") {
            toks.push({ text: line.slice(i), col: COMCOL });
            return { toks: toks, inBlock: false };
        }

        if (line[i] === '"' || line[i] === "'" || line[i] === "`") {
            var q = line[i];
            var j = i + 1;
            while (j < n && line[j] !== q) {
                if (line[j] === "\\") j++;
                j++;
            }
            if (j < n) j++;
            toks.push({ text: line.slice(i, j), col: STRCOL });
            i = j;
            continue;
        }

        if (line[i] >= "0" && line[i] <= "9") {
            var j2 = i;
            while (
                j2 < n &&
                ((line[j2] >= "0" && line[j2] <= "9") ||
                    line[j2] === "." ||
                    line[j2] === "x" ||
                    line[j2] === "X" ||
                    (line[j2] >= "a" && line[j2] <= "f") ||
                    (line[j2] >= "A" && line[j2] <= "F"))
            )
                j2++;
            toks.push({ text: line.slice(i, j2), col: NUMCOL });
            i = j2;
            continue;
        }

        var ch = line[i];
        if ((ch >= "a" && ch <= "z") || (ch >= "A" && ch <= "Z") || ch === "_" || ch === "$") {
            var j3 = i + 1;
            while (j3 < n) {
                var c = line[j3];
                if (
                    (c >= "a" && c <= "z") ||
                    (c >= "A" && c <= "Z") ||
                    (c >= "0" && c <= "9") ||
                    c === "_" ||
                    c === "$"
                )
                    j3++;
                else break;
            }
            var word = line.slice(i, j3);
            toks.push({ text: word, col: KEYWORDS[word] ? KWCOL : FG });
            i = j3;
            continue;
        }

        toks.push({ text: line[i], col: FG });
        i++;
    }
    return { toks: toks, inBlock: false };
}

// ─── Helper: join token texts ────────────────────────────────────────────────

function tokTexts(result) {
    var out = [];
    for (var i = 0; i < result.toks.length; i++) out.push(result.toks[i].text);
    return out;
}

function tokCols(result) {
    var out = [];
    for (var i = 0; i < result.toks.length; i++) out.push(result.toks[i].col);
    return out;
}

function joinText(result) {
    return tokTexts(result).join("");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Basic tokenization
// ═════════════════════════════════════════════════════════════════════════════

// Plain identifier + operator + number
(function test_basic() {
    var r = tokenize("x = 42", false);
    assertEq(joinText(r), "x = 42", "text round-trip");
    assertEq(r.inBlock, false, "no block state");
    assertEq(r.toks[0].col, FG, "identifier is FG");
    assertEq(r.toks[r.toks.length - 1].col, NUMCOL, "number is NUMCOL");
})();

// Keyword recognition
(function test_keywords() {
    var r = tokenize("const foo = function() {}", false);
    assertEq(r.toks[0].text, "const", "first token text");
    assertEq(r.toks[0].col, KWCOL, "const is keyword");
    // 'foo' is not a keyword
    assertEq(r.toks[2].text, "foo", "identifier");
    assertEq(r.toks[2].col, FG, "foo is not keyword");
    // 'function' is keyword
    assertEq(r.toks[6].text, "function", "function token");
    assertEq(r.toks[6].col, KWCOL, "function is keyword");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  String handling
// ═════════════════════════════════════════════════════════════════════════════

// Double-quoted string
(function test_double_string() {
    var r = tokenize('let s = "hello"', false);
    var strs = r.toks.filter(function (t) {
        return t.col === STRCOL;
    });
    assertEq(strs.length, 1, "one string token");
    assertEq(strs[0].text, '"hello"', "string content");
})();

// Single-quoted string
(function test_single_string() {
    var r = tokenize("let s = 'world'", false);
    var strs = r.toks.filter(function (t) {
        return t.col === STRCOL;
    });
    assertEq(strs.length, 1, "one string token");
    assertEq(strs[0].text, "'world'", "string content");
})();

// Template literal
(function test_template_literal() {
    var r = tokenize("let s = `tmpl`", false);
    var strs = r.toks.filter(function (t) {
        return t.col === STRCOL;
    });
    assertEq(strs.length, 1, "one template string");
    assertEq(strs[0].text, "`tmpl`", "template literal");
})();

// Escaped quote in string
(function test_escaped_quote() {
    var r = tokenize('x = "a\\"b"', false);
    var strs = r.toks.filter(function (t) {
        return t.col === STRCOL;
    });
    assertEq(strs.length, 1, "one string");
    assertEq(strs[0].text, '"a\\"b"', "escaped quote preserved");
})();

// Unterminated string consumes to end of line
(function test_unterminated_string() {
    var r = tokenize('x = "no close', false);
    assertEq(joinText(r), 'x = "no close', "text preserved");
    assertEq(r.inBlock, false, "string does not open block");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  Comment handling
// ═════════════════════════════════════════════════════════════════════════════

// Line comment
(function test_line_comment() {
    var r = tokenize("x = 1; // comment", false);
    assertEq(r.inBlock, false, "line comment no block");
    var last = r.toks[r.toks.length - 1];
    assertEq(last.col, COMCOL, "comment colored");
    assertEq(last.text, "// comment", "comment text");
})();

// Block comment (same line, closed)
(function test_block_comment_closed() {
    var r = tokenize("x /* hi */ y", false);
    assertEq(r.inBlock, false, "closed block");
    var comments = r.toks.filter(function (t) {
        return t.col === COMCOL;
    });
    assertEq(comments.length, 1, "one comment token");
    assertEq(comments[0].text, "/* hi */", "block comment text");
})();

// Block comment (unclosed → sets inBlock)
(function test_block_comment_unclosed() {
    var r = tokenize("x /* no close", false);
    assertEq(r.inBlock, true, "unclosed block opens inBlock");
})();

// Block comment continuation (inBlock=true, closes)
(function test_block_continuation_closes() {
    var r = tokenize("still in */ x", true);
    assertEq(r.inBlock, false, "block closed");
    assertEq(r.toks[0].col, COMCOL, "continuation is comment");
    // x should be FG
    var last = r.toks[r.toks.length - 1];
    assertEq(last.text, "x", "code after close");
    assertEq(last.col, FG, "code is FG");
})();

// Block comment continuation (never closes)
(function test_block_continuation_never_closes() {
    var r = tokenize("all comment no close", true);
    assertEq(r.inBlock, true, "still in block");
    assertEq(r.toks.length, 1, "entire line is one comment token");
    assertEq(r.toks[0].col, COMCOL, "comment color");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  Number handling
// ═════════════════════════════════════════════════════════════════════════════

// Hex number
(function test_hex_number() {
    var r = tokenize("0xFF", false);
    assertEq(r.toks[0].text, "0xFF", "hex literal");
    assertEq(r.toks[0].col, NUMCOL, "hex is number");
})();

// Decimal with dot
(function test_decimal_dot() {
    var r = tokenize("3.14", false);
    assertEq(r.toks[0].text, "3.14", "decimal literal");
    assertEq(r.toks[0].col, NUMCOL, "decimal is number");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  Edge cases
// ═════════════════════════════════════════════════════════════════════════════

// Empty line
(function test_empty_line() {
    var r = tokenize("", false);
    assertEq(r.toks.length, 0, "no tokens");
    assertEq(r.inBlock, false, "no block");
})();

// Only whitespace
(function test_whitespace_line() {
    var r = tokenize("   ", false);
    assertEq(joinText(r), "   ", "whitespace preserved");
    for (var i = 0; i < r.toks.length; i++) {
        assertEq(r.toks[i].col, FG, "whitespace is FG");
    }
})();

// Mixed: keyword, string, number, comment on one line
(function test_mixed_line() {
    var r = tokenize('let x = "hi" + 42; // done', false);
    assertEq(r.toks[0].col, KWCOL, "let is keyword");
    var strs = r.toks.filter(function (t) {
        return t.col === STRCOL;
    });
    assertEq(strs.length, 1, "one string");
    var nums = r.toks.filter(function (t) {
        return t.col === NUMCOL;
    });
    assertEq(nums.length, 1, "one number");
    var coms = r.toks.filter(function (t) {
        return t.col === COMCOL;
    });
    assertEq(coms.length, 1, "one comment");
})();

// Dollar-sign identifier
(function test_dollar_ident() {
    var r = tokenize("$foo", false);
    assertEq(r.toks[0].text, "$foo", "dollar ident");
    assertEq(r.toks[0].col, FG, "dollar ident is FG");
})();

// Underscore identifier
(function test_underscore_ident() {
    var r = tokenize("_init()", false);
    assertEq(r.toks[0].text, "_init", "underscore ident");
    assertEq(r.toks[0].col, FG, "not keyword");
})();

// Round-trip: reconstructed text always matches original
(function test_roundtrip_complex() {
    var lines = [
        "const a = 1 + 2;",
        'if (x > 0) { return "yes"; }',
        "/* block */ let y = `tmpl`;",
        "foo.bar(0xFF, 3.14);",
    ];
    for (var i = 0; i < lines.length; i++) {
        var r = tokenize(lines[i], false);
        assertEq(joinText(r), lines[i], "round-trip line " + i);
    }
})();
