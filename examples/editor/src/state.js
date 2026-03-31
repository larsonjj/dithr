// ─── Mutable state ───────────────────────────────────────────────────────────

let buf = [""]; // line buffer
let cx = 0,
    cy = 0; // cursor col / row in buffer
let ox = 0,
    oy = 0; // scroll offset col / row
let anchor = null; // selection anchor {x,y} or null

let blink = 0,
    curOn = true; // cursor blink state
let fname = ""; // current filename
let dirty = false;
let msg = "",
    msgT = 0; // status message + timer
let restored = false; // set by _restore to skip default open in _init

// undo / redo
const MAXUNDO = 200;
let undoStack = [];
let redoStack = [];

// file browser
let brMode = false;
let brEntries = []; // {name, isDir}
let brIdx = 0;
let brScroll = 0;
let brDir = ""; // current directory relative to cart root ("" = root, "src/" etc.)

// find & replace
let findMode = false;
let findText = "";
let replaceText = "";
let findReplace = false; // true = show replace field
let findField = 0; // 0 = find, 1 = replace

// go to line
let gotoMode = false;
let gotoText = "";

// vim mode
let vimEnabled = false;
let vim = "normal"; // "normal", "insert", "visual", "vline", "command"
let vimCount = "";
let vimPending = "";
let vimOpCount = 1;
let vimCmd = "";
let vimReg = "";
let vimLinewise = false;
let vimSearch = "";
let vimVlineStart = 0;

// double-click state
let lastClickTime = 0;
let lastClickX = 0;
let lastClickY = 0;
let clickCount = 0;

// smooth scrolling
let targetOy = 0;
let smoothOy = 0;

// modified line tracking (since last save)
let savedBuf = []; // snapshot at last save/open

// persistent find highlighting
let lastFindText = "";

// ─── Caches (invalidated on edit) ────────────────────────────────────────────

let _blockStateCache = []; // inBlock state at start of each line
let _blockStateDirty = true;
let _bracketMatchCache = null; // { cy, cx, result }
let _bracketDepthCache = []; // cumulative bracket depth at start of each line
let _bracketDepthDirty = true;
let _bufVersion = 0; // incremented on every edit
let _lastCacheVersion = -1; // version at which caches were last valid

function invalidateCaches() {
    _bufVersion++;
}
