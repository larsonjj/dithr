// ─── Stroke History ──────────────────────────────────────────────────────────
//
// Reusable undo/redo stack for stroke-based editors (sprite, map).
// Each "stroke" is an array of ops collected while the mouse is held.
// Redo is supported: undone strokes go to a redo stack, cleared on new edits.

export function createHistory(maxEntries) {
    return {
        undoStack: [],
        redoStack: [],
        pending: [],
        max: maxEntries || 100,
    };
}

/** Record a single op within the current stroke. */
export function record(hist, op) {
    hist.pending.push(op);
}

/** Commit the pending stroke (call on mouse release). */
export function commit(hist) {
    if (hist.pending.length === 0) return;
    hist.undoStack.push(hist.pending);
    hist.pending = [];
    hist.redoStack.length = 0; // new edit clears redo
    if (hist.undoStack.length > hist.max) hist.undoStack.shift();
}

/**
 * Undo the last stroke.
 * @param {object} hist
 * @param {function} applyOp  Called with each op to revert it.
 * @returns {boolean} true if something was undone.
 */
export function undo(hist, applyOp) {
    if (hist.undoStack.length === 0) return false;
    let stroke = hist.undoStack.pop();
    let redo = [];
    for (let i = stroke.length - 1; i >= 0; i--) {
        redo.push(applyOp(stroke[i]));
    }
    redo.reverse();
    hist.redoStack.push(redo);
    return true;
}

/**
 * Redo the last undone stroke.
 * @param {object} hist
 * @param {function} applyOp  Called with each op to re-apply it.
 * @returns {boolean} true if something was redone.
 */
export function redo(hist, applyOp) {
    if (hist.redoStack.length === 0) return false;
    let stroke = hist.redoStack.pop();
    let undoOps = [];
    for (let i = 0; i < stroke.length; i++) {
        undoOps.push(applyOp(stroke[i]));
    }
    hist.undoStack.push(undoOps);
    return true;
}
