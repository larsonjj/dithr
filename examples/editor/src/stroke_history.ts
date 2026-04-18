// ─── Stroke History ──────────────────────────────────────────────────────────
//
// Reusable undo/redo stack for stroke-based editors (sprite, map).
// Each "stroke" is an array of ops collected while the mouse is held.
// Redo is supported: undone strokes go to a redo stack, cleared on new edits.

export interface StrokeHistory<T> {
    undoStack: T[][];
    redoStack: T[][];
    pending: T[];
    max: number;
}

export function createHistory<T>(maxEntries: number): StrokeHistory<T> {
    return {
        undoStack: [],
        redoStack: [],
        pending: [],
        max: maxEntries || 100,
    };
}

/** Record a single op within the current stroke. */
export function record<T>(hist: StrokeHistory<T>, op: T) {
    hist.pending.push(op);
}

/** Commit the pending stroke (call on mouse release). */
export function commit<T>(hist: StrokeHistory<T>) {
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
export function undo<T>(hist: StrokeHistory<T>, applyOp: (op: T) => T) {
    if (hist.undoStack.length === 0) return false;
    const stroke = hist.undoStack.pop()!;
    const redoOps: T[] = [];
    for (let i = stroke.length - 1; i >= 0; i--) {
        redoOps.push(applyOp(stroke[i]));
    }
    redoOps.reverse();
    hist.redoStack.push(redoOps);
    return true;
}

/**
 * Redo the last undone stroke.
 * @param {object} hist
 * @param {function} applyOp  Called with each op to re-apply it.
 * @returns {boolean} true if something was redone.
 */
export function redo<T>(hist: StrokeHistory<T>, applyOp: (op: T) => T) {
    if (hist.redoStack.length === 0) return false;
    const stroke = hist.redoStack.pop()!;
    const undoOps: T[] = [];
    for (let i = 0; i < stroke.length; i++) {
        undoOps.push(applyOp(stroke[i]));
    }
    hist.undoStack.push(undoOps);
    return true;
}
