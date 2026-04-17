// ─── Music editor — pure function tests ──────────────────────────────────────
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

// ─── Inlined constants ───────────────────────────────────────────────────────

var FLAG_NONE = 0;
var FLAG_LOOP_START = 1;
var FLAG_LOOP_BACK = 2;
var FLAG_STOP = 3;

// ─── Inlined: patternDuration ────────────────────────────────────────────────
// Simulates synth.get() returning {speed: N} for channel SFX

function patternDuration(pat, sfxSpeeds) {
    var maxDur = 0;
    for (var c = 0; c < 4; c++) {
        if (pat.ch[c] >= 0) {
            var spd = sfxSpeeds[pat.ch[c]] || 16;
            var dur = spd / 4;
            if (dur > maxDur) maxDur = dur;
        }
    }
    return maxDur;
}

// ─── Inlined: advancePattern logic ───────────────────────────────────────────

function advancePattern(patterns, playRow) {
    var cur = playRow;
    var curPat = patterns[cur];

    if (curPat.flags === FLAG_STOP) {
        return { action: "stop" };
    }

    var next;
    if (curPat.flags === FLAG_LOOP_BACK) {
        next = 0;
        for (var i = cur - 1; i >= 0; i--) {
            if (patterns[i].flags === FLAG_LOOP_START) {
                next = i;
                break;
            }
        }
    } else {
        next = cur + 1;
    }

    if (next >= 64) {
        return { action: "stop" };
    }

    return { action: "play", row: next };
}

// ─── Inlined: number entry digit logic ───────────────────────────────────────

function numberEntry(cur, digit) {
    if (cur < 0) cur = 0;
    var next = (cur % 10) * 10 + digit;
    if (next > 63) next = digit;
    return next;
}

// ─── patternDuration tests ───────────────────────────────────────────────────

// Empty pattern → 0
(function test_duration_empty() {
    var pat = { ch: [-1, -1, -1, -1], flags: 0 };
    assertEq(patternDuration(pat, {}), 0, "empty pattern should have 0 duration");
})();

// Single channel with speed 16 → 4 seconds
(function test_duration_speed16() {
    var pat = { ch: [0, -1, -1, -1], flags: 0 };
    assertEq(patternDuration(pat, { 0: 16 }), 4, "speed 16 → 4 seconds");
})();

// Multiple channels — use the longest
(function test_duration_max_across_channels() {
    var pat = { ch: [0, 1, -1, -1], flags: 0 };
    var dur = patternDuration(pat, { 0: 8, 1: 32 });
    assertEq(dur, 8, "should use max duration (speed 32 → 8s)");
})();

// Speed 1 → 0.25 seconds
(function test_duration_speed1() {
    var pat = { ch: [0, -1, -1, -1], flags: 0 };
    assertEq(patternDuration(pat, { 0: 1 }), 0.25, "speed 1 → 0.25 seconds");
})();

// Default speed when missing → 16 ÷ 4 = 4
(function test_duration_default_speed() {
    var pat = { ch: [5, -1, -1, -1], flags: 0 };
    assertEq(patternDuration(pat, {}), 4, "missing speed defaults to 16 → 4s");
})();

// ─── advancePattern tests ────────────────────────────────────────────────────

function makePatterns(count) {
    var arr = [];
    for (var i = 0; i < 64; i++) {
        arr.push({ ch: [-1, -1, -1, -1], flags: FLAG_NONE });
    }
    return arr;
}

// Normal advance: row 0 → row 1
(function test_advance_normal() {
    var pats = makePatterns();
    pats[0].ch[0] = 0;
    pats[1].ch[0] = 1;
    var result = advancePattern(pats, 0);
    assertEq(result.action, "play", "should advance");
    assertEq(result.row, 1, "should go to row 1");
})();

// Stop flag on current pattern → stop
(function test_advance_stop_flag() {
    var pats = makePatterns();
    pats[2].flags = FLAG_STOP;
    var result = advancePattern(pats, 2);
    assertEq(result.action, "stop", "FLAG_STOP should stop playback");
})();

// Loop back to loop start
(function test_advance_loop_back() {
    var pats = makePatterns();
    pats[0].flags = FLAG_LOOP_START;
    pats[3].flags = FLAG_LOOP_BACK;
    var result = advancePattern(pats, 3);
    assertEq(result.action, "play", "should loop");
    assertEq(result.row, 0, "should jump to loop start (row 0)");
})();

// Loop back with loop start not at 0
(function test_advance_loop_back_mid() {
    var pats = makePatterns();
    pats[2].flags = FLAG_LOOP_START;
    pats[5].flags = FLAG_LOOP_BACK;
    var result = advancePattern(pats, 5);
    assertEq(result.action, "play", "should loop");
    assertEq(result.row, 2, "should jump to loop start (row 2)");
})();

// Loop back with no loop start → defaults to row 0
(function test_advance_loop_back_no_start() {
    var pats = makePatterns();
    pats[3].flags = FLAG_LOOP_BACK;
    var result = advancePattern(pats, 3);
    assertEq(result.action, "play", "should still play");
    assertEq(result.row, 0, "should default to row 0 with no loop-start");
})();

// Advance past end (row 63) → stop
(function test_advance_past_end() {
    var pats = makePatterns();
    var result = advancePattern(pats, 63);
    assertEq(result.action, "stop", "should stop at end of patterns");
})();

// Loop start flag on current → normal advance (loop-start is just a marker)
(function test_advance_loop_start_is_marker() {
    var pats = makePatterns();
    pats[1].flags = FLAG_LOOP_START;
    var result = advancePattern(pats, 1);
    assertEq(result.action, "play", "loop-start should advance normally");
    assertEq(result.row, 2, "should go to next row");
})();

// Multiple loop-start markers — finds nearest one above
(function test_advance_nested_loop_starts() {
    var pats = makePatterns();
    pats[0].flags = FLAG_LOOP_START;
    pats[3].flags = FLAG_LOOP_START;
    pats[5].flags = FLAG_LOOP_BACK;
    var result = advancePattern(pats, 5);
    assertEq(result.action, "play");
    assertEq(result.row, 3, "should find nearest loop-start (row 3, not 0)");
})();

// ─── Number entry tests ─────────────────────────────────────────────────────

// Basic digit entry
(function test_number_entry_single() {
    assertEq(numberEntry(-1, 5), 5, "from empty, press 5 → 5");
})();

// Shift left and add
(function test_number_entry_shift() {
    assertEq(numberEntry(1, 2), 12, "from 1, press 2 → 12");
})();

// Wrap on overflow
(function test_number_entry_overflow() {
    assertEq(numberEntry(7, 8), 8, "78 > 63, should wrap to 8");
})();

// Two-digit boundary
(function test_number_entry_63() {
    assertEq(numberEntry(6, 3), 63, "from 6, press 3 → 63");
})();

// Zero
(function test_number_entry_zero() {
    assertEq(numberEntry(0, 0), 0, "from 0, press 0 → 0");
})();

// Chain three digits
(function test_number_entry_chain() {
    var v = numberEntry(-1, 1); // → 1
    v = numberEntry(v, 2); // → 12
    v = numberEntry(v, 3); // → 23
    assertEq(v, 23, "1 → 12 → 23");
})();

// ─── Insert / Delete row aliasing tests ──────────────────────────────────────

function clonePattern(p) {
    return { ch: [p.ch[0], p.ch[1], p.ch[2], p.ch[3]], flags: p.flags };
}

function insertRow(patterns, sel) {
    for (var i = 63; i > sel; i--) {
        var src = patterns[i - 1];
        patterns[i] = { ch: [src.ch[0], src.ch[1], src.ch[2], src.ch[3]], flags: src.flags };
    }
    patterns[sel] = { ch: [-1, -1, -1, -1], flags: FLAG_NONE };
}

function deleteRow(patterns, sel) {
    for (var i = sel; i < 63; i++) {
        var src = patterns[i + 1];
        patterns[i] = { ch: [src.ch[0], src.ch[1], src.ch[2], src.ch[3]], flags: src.flags };
    }
    patterns[63] = { ch: [-1, -1, -1, -1], flags: FLAG_NONE };
}

// Insert should not share object references between rows
(function test_insert_no_aliasing() {
    var pats = makePatterns();
    pats[0].ch = [1, 2, 3, 4];
    pats[0].flags = FLAG_LOOP_START;
    insertRow(pats, 0);
    // Row 0 should be blank, row 1 should have the original data
    assertEq(pats[0].ch[0], -1, "inserted row should be blank");
    assertEq(pats[1].ch[0], 1, "shifted row should preserve ch[0]");
    assertEq(pats[1].ch[3], 4, "shifted row should preserve ch[3]");
    assertEq(pats[1].flags, FLAG_LOOP_START, "shifted row should preserve flags");
    // Modifying row 1 should NOT affect row 2 (no aliasing)
    pats[1].ch[0] = 99;
    assert(pats[2].ch[0] !== 99, "rows should not be aliased after insert");
})();

// Delete should not share object references between rows
(function test_delete_no_aliasing() {
    var pats = makePatterns();
    pats[0].ch = [10, 20, 30, 40];
    pats[1].ch = [11, 21, 31, 41];
    pats[2].ch = [12, 22, 32, 42];
    deleteRow(pats, 0);
    // Row 0 should now have row 1's old data
    assertEq(pats[0].ch[0], 11, "row 0 should have old row 1 data");
    assertEq(pats[1].ch[0], 12, "row 1 should have old row 2 data");
    // Modifying row 0 should NOT affect row 1 (no aliasing)
    pats[0].ch[0] = 99;
    assert(pats[1].ch[0] !== 99, "rows should not be aliased after delete");
    // Last row should be blank
    assertEq(pats[63].ch[0], -1, "last row should be blank after delete");
})();
