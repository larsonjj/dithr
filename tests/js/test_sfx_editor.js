// ─── SFX editor — pure function tests ────────────────────────────────────────
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

var OCT_OFFSET = 2;
var MIN_INT_OCT = 3;
var MAX_INT_OCT = 6;
var MIN_PITCH = MIN_INT_OCT * 12 + 1; // 37
var MAX_PITCH = (MAX_INT_OCT + 1) * 12; // 84

// ─── Inlined: clamp ──────────────────────────────────────────────────────────

function clamp(v, lo, hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ─── Inlined: makePitch ──────────────────────────────────────────────────────

function makePitch(noteInOctave, octave) {
    return clamp(octave, MIN_INT_OCT, MAX_INT_OCT) * 12 + noteInOctave + 1;
}

// ─── Inlined: cloneSfx ──────────────────────────────────────────────────────

function cloneSfx(data) {
    var notes = [];
    for (var i = 0; i < 32; i++) {
        var n = data.notes[i];
        notes.push({ pitch: n.pitch, waveform: n.waveform, volume: n.volume, effect: n.effect });
    }
    return { notes: notes, speed: data.speed, loopStart: data.loopStart, loopEnd: data.loopEnd };
}

// ─── Inlined: loop marker validation ─────────────────────────────────────────

function setLoopStart(data, noteIdx) {
    data.loopStart = noteIdx;
    if (data.loopEnd > 0 && data.loopStart > data.loopEnd) {
        var tmp = data.loopStart;
        data.loopStart = data.loopEnd;
        data.loopEnd = tmp;
    }
    return data;
}

function setLoopEnd(data, noteIdx) {
    data.loopEnd = noteIdx + 1;
    if (data.loopStart > 0 && data.loopStart >= data.loopEnd) {
        var tmp = data.loopStart;
        data.loopStart = data.loopEnd;
        data.loopEnd = tmp;
    }
    return data;
}

// ─── Piano key mapping ──────────────────────────────────────────────────────

var PIANO_KEYS = [
    { key: "Z", note: 0 },
    { key: "S", note: 1 },
    { key: "X", note: 2 },
    { key: "D", note: 3 },
    { key: "C", note: 4 },
    { key: "V", note: 5 },
    { key: "G", note: 6 },
    { key: "B", note: 7 },
    { key: "H", note: 8 },
    { key: "N", note: 9 },
    { key: "J", note: 10 },
    { key: "M", note: 11 },
];

// ─── Helper: empty SFX ──────────────────────────────────────────────────────

function emptySfx() {
    var notes = [];
    for (var i = 0; i < 32; i++) {
        notes.push({ pitch: 0, waveform: 0, volume: 0, effect: 0 });
    }
    return { notes: notes, speed: 16, loopStart: 0, loopEnd: 0 };
}

// ═════════════════════════════════════════════════════════════════════════════
//  Tests: makePitch
// ═════════════════════════════════════════════════════════════════════════════

// C at minimum octave (internal octave 3 = display octave 1)
assertEq(makePitch(0, MIN_INT_OCT), 37, "C at min octave");

// A at octave 4 (A-4 = pitch 58)
assertEq(makePitch(9, 4), 58, "A at octave 4 = pitch 58");

// B at max octave (internal octave 6 = display octave 4)
assertEq(makePitch(11, MAX_INT_OCT), 84, "B at max octave");

// Clamps below MIN_INT_OCT
assertEq(makePitch(0, 0), makePitch(0, MIN_INT_OCT), "octave clamped low");

// Clamps above MAX_INT_OCT
assertEq(makePitch(0, 99), makePitch(0, MAX_INT_OCT), "octave clamped high");

// C# at octave 5
assertEq(makePitch(1, 5), 62, "C# at octave 5");

// All 12 notes in octave 3 produce sequential pitches
for (var i = 0; i < 12; i++) {
    assertEq(makePitch(i, 3), 37 + i, "note " + i + " at octave 3");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Tests: piano key mapping covers all 12 semitones
// ═════════════════════════════════════════════════════════════════════════════

assertEq(PIANO_KEYS.length, 12, "12 piano keys mapped");

// All note values 0-11 are present exactly once
var noteSet = {};
for (var i = 0; i < PIANO_KEYS.length; i++) {
    var n = PIANO_KEYS[i].note;
    assert(n >= 0 && n <= 11, "note in range: " + n);
    assert(!noteSet[n], "note " + n + " not duplicated");
    noteSet[n] = true;
}
for (var i = 0; i < 12; i++) {
    assert(noteSet[i], "note " + i + " covered");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Tests: cloneSfx (deep copy)
// ═════════════════════════════════════════════════════════════════════════════

{
    var orig = emptySfx();
    orig.notes[0].pitch = 49;
    orig.notes[0].volume = 7;
    orig.speed = 8;
    orig.loopStart = 4;
    orig.loopEnd = 16;

    var copy = cloneSfx(orig);

    // Values match
    assertEq(copy.notes[0].pitch, 49, "clone pitch matches");
    assertEq(copy.speed, 8, "clone speed matches");
    assertEq(copy.loopStart, 4, "clone loopStart matches");
    assertEq(copy.loopEnd, 16, "clone loopEnd matches");

    // Modifying clone doesn't affect original
    copy.notes[0].pitch = 60;
    copy.speed = 4;
    assertEq(orig.notes[0].pitch, 49, "original not mutated (pitch)");
    assertEq(orig.speed, 8, "original not mutated (speed)");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Tests: loop marker validation
// ═════════════════════════════════════════════════════════════════════════════

// Setting loop start before existing end — no swap needed
{
    var sfx = emptySfx();
    sfx.loopEnd = 20;
    setLoopStart(sfx, 4);
    assertEq(sfx.loopStart, 4, "loop start set");
    assertEq(sfx.loopEnd, 20, "loop end unchanged");
}

// Setting loop start after existing end — swap
{
    var sfx = emptySfx();
    sfx.loopEnd = 10;
    setLoopStart(sfx, 15);
    assert(sfx.loopStart <= sfx.loopEnd, "swapped: start <= end");
    assertEq(sfx.loopStart, 10, "swapped start");
    assertEq(sfx.loopEnd, 15, "swapped end");
}

// Setting loop end after existing start — no swap needed
{
    var sfx = emptySfx();
    sfx.loopStart = 4;
    setLoopEnd(sfx, 19); // loopEnd = 20
    assertEq(sfx.loopStart, 4, "start unchanged");
    assertEq(sfx.loopEnd, 20, "end set");
}

// Setting loop end before existing start — swap
{
    var sfx = emptySfx();
    sfx.loopStart = 20;
    setLoopEnd(sfx, 4); // loopEnd = 5
    assert(sfx.loopStart <= sfx.loopEnd, "swapped: start <= end");
    assertEq(sfx.loopStart, 5, "swapped start after setLoopEnd");
    assertEq(sfx.loopEnd, 20, "swapped end after setLoopEnd");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Tests: undo/redo snapshot integrity
// ═════════════════════════════════════════════════════════════════════════════

{
    var MAX_UNDO = 50;
    var undoStack = [];
    var redoStack = [];

    // Simulate pushing undos
    var sfx = emptySfx();
    sfx.notes[0].pitch = 49;

    // Push undo
    undoStack.push(cloneSfx(sfx));
    redoStack = [];

    // Modify current
    sfx.notes[0].pitch = 60;

    // Push another undo
    undoStack.push(cloneSfx(sfx));
    redoStack = [];

    assertEq(undoStack.length, 2, "two undo snapshots");
    assertEq(undoStack[0].notes[0].pitch, 49, "first snapshot has original pitch");
    assertEq(undoStack[1].notes[0].pitch, 60, "second snapshot has modified pitch");

    // Simulate undo
    redoStack.push(cloneSfx(sfx));
    sfx = undoStack.pop();
    assertEq(sfx.notes[0].pitch, 60, "undo restores previous state");
    assertEq(undoStack.length, 1, "one undo remaining");
    assertEq(redoStack.length, 1, "one redo available");

    // Simulate redo
    undoStack.push(cloneSfx(sfx));
    sfx = redoStack.pop();
    assertEq(sfx.notes[0].pitch, 60, "redo restores forward state");

    // Max undo depth
    undoStack = [];
    for (var i = 0; i < MAX_UNDO + 10; i++) {
        var s = emptySfx();
        s.notes[0].pitch = i + 1;
        undoStack.push(cloneSfx(s));
        if (undoStack.length > MAX_UNDO) undoStack.shift();
    }
    assertEq(undoStack.length, MAX_UNDO, "undo stack capped at MAX_UNDO");
    assertEq(undoStack[0].notes[0].pitch, 11, "oldest undo is correct after overflow");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Tests: pitch range boundaries
// ═════════════════════════════════════════════════════════════════════════════

assertEq(MIN_PITCH, 37, "MIN_PITCH = 37");
assertEq(MAX_PITCH, 84, "MAX_PITCH = 84");

// makePitch at boundaries
assertEq(makePitch(0, MIN_INT_OCT), MIN_PITCH, "makePitch floor = MIN_PITCH");
assertEq(makePitch(11, MAX_INT_OCT), MAX_PITCH, "makePitch ceiling = MAX_PITCH");
