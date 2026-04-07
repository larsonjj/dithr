// ─── Stroke history — pure function tests ────────────────────────────────────
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

// ─── Functions under test (inlined from stroke_history.js) ───────────────────

function createHistory(maxEntries) {
    return {
        undoStack: [],
        redoStack: [],
        pending: [],
        max: maxEntries || 100,
    };
}

function record(hist, op) {
    hist.pending.push(op);
}

function commit(hist) {
    if (hist.pending.length === 0) return;
    hist.undoStack.push(hist.pending);
    hist.pending = [];
    hist.redoStack.length = 0;
    if (hist.undoStack.length > hist.max) hist.undoStack.shift();
}

function undo(hist, applyOp) {
    if (hist.undoStack.length === 0) return false;
    var stroke = hist.undoStack.pop();
    var redoOps = [];
    for (var i = stroke.length - 1; i >= 0; i--) {
        redoOps.push(applyOp(stroke[i]));
    }
    redoOps.reverse();
    hist.redoStack.push(redoOps);
    return true;
}

function redo(hist, applyOp) {
    if (hist.redoStack.length === 0) return false;
    var stroke = hist.redoStack.pop();
    var undoOps = [];
    for (var i = 0; i < stroke.length; i++) {
        undoOps.push(applyOp(stroke[i]));
    }
    hist.undoStack.push(undoOps);
    return true;
}

// ─── createHistory tests ─────────────────────────────────────────────────────

(function () {
    var h = createHistory(50);
    assertEq(h.undoStack.length, 0, "empty undo");
    assertEq(h.redoStack.length, 0, "empty redo");
    assertEq(h.pending.length, 0, "empty pending");
    assertEq(h.max, 50, "max set");
})();

(function () {
    var h = createHistory();
    assertEq(h.max, 100, "default max");
})();

// ─── record / commit tests ──────────────────────────────────────────────────

(function () {
    var h = createHistory(10);
    record(h, { x: 1, prev: 0, col: 5 });
    record(h, { x: 2, prev: 0, col: 5 });
    assertEq(h.pending.length, 2, "two pending ops");
    assertEq(h.undoStack.length, 0, "not yet committed");

    commit(h);
    assertEq(h.pending.length, 0, "pending cleared");
    assertEq(h.undoStack.length, 1, "one stroke on undo stack");
    assertEq(h.undoStack[0].length, 2, "stroke has 2 ops");
})();

// commit with empty pending is a no-op
(function () {
    var h = createHistory(10);
    commit(h);
    assertEq(h.undoStack.length, 0, "no-op commit");
})();

// max entries eviction
(function () {
    var h = createHistory(3);
    for (var i = 0; i < 5; i++) {
        record(h, { val: i });
        commit(h);
    }
    assertEq(h.undoStack.length, 3, "capped at max");
    assertEq(h.undoStack[0][0].val, 2, "oldest stroke evicted");
})();

// ─── undo tests ──────────────────────────────────────────────────────────────

// Basic undo applies ops in reverse and returns true
(function () {
    var h = createHistory(10);
    record(h, { x: 1, prev: 0 });
    record(h, { x: 2, prev: 0 });
    commit(h);

    var applied = [];
    var ok = undo(h, function (op) {
        applied.push(op.x);
        return { x: op.x, prev: op.prev }; // return redo op
    });
    assert(ok, "undo returns true");
    assertEq(h.undoStack.length, 0, "undo stack empty");
    assertEq(h.redoStack.length, 1, "redo stack has 1");
    assertEq(applied.length, 2, "2 ops applied");
    assertEq(applied[0], 2, "applied in reverse: x=2 first");
    assertEq(applied[1], 1, "applied in reverse: x=1 second");
})();

// Undo on empty stack returns false
(function () {
    var h = createHistory(10);
    var ok = undo(h, function () {
        return {};
    });
    assert(!ok, "undo empty returns false");
})();

// ─── redo tests ──────────────────────────────────────────────────────────────

// Redo after undo re-applies ops
(function () {
    var h = createHistory(10);
    record(h, { val: 10 });
    record(h, { val: 20 });
    commit(h);

    undo(h, function (op) {
        return { val: -op.val };
    });

    var applied = [];
    var ok = redo(h, function (op) {
        applied.push(op.val);
        return { val: -op.val };
    });
    assert(ok, "redo returns true");
    assertEq(h.undoStack.length, 1, "back on undo stack");
    assertEq(h.redoStack.length, 0, "redo stack empty");
    assertEq(applied.length, 2, "2 ops re-applied");
})();

// Redo on empty stack returns false
(function () {
    var h = createHistory(10);
    var ok = redo(h, function () {
        return {};
    });
    assert(!ok, "redo empty returns false");
})();

// New commit clears redo stack
(function () {
    var h = createHistory(10);
    record(h, { val: 1 });
    commit(h);
    undo(h, function (op) {
        return op;
    });
    assertEq(h.redoStack.length, 1, "redo available");

    record(h, { val: 2 });
    commit(h);
    assertEq(h.redoStack.length, 0, "redo cleared after new commit");
})();

// ─── Multiple undo/redo round-trip ──────────────────────────────────────────

(function () {
    var h = createHistory(10);
    // Two strokes
    record(h, { val: "a" });
    commit(h);
    record(h, { val: "b" });
    commit(h);

    assertEq(h.undoStack.length, 2, "2 strokes");

    undo(h, function (op) {
        return op;
    });
    assertEq(h.undoStack.length, 1, "1 stroke after undo");
    assertEq(h.redoStack.length, 1, "1 redo");

    undo(h, function (op) {
        return op;
    });
    assertEq(h.undoStack.length, 0, "0 strokes after 2 undos");
    assertEq(h.redoStack.length, 2, "2 redos");

    redo(h, function (op) {
        return op;
    });
    assertEq(h.undoStack.length, 1, "1 stroke after 1 redo");
    assertEq(h.redoStack.length, 1, "1 redo left");

    redo(h, function (op) {
        return op;
    });
    assertEq(h.undoStack.length, 2, "2 strokes fully restored");
    assertEq(h.redoStack.length, 0, "no redo left");
})();
