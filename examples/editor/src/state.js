// ─── Mutable state ───────────────────────────────────────────────────────────
//
// All editor state lives on this single exported object so that every module
// can freely read *and* write it via `st.cx`, `st.buf`, etc.

import { TAB_CODE } from "./config.js";

export const st = {
    buf: [""], // line buffer
    cx: 0,
    cy: 0, // cursor col / row in buffer
    ox: 0,
    oy: 0, // scroll offset col / row
    anchor: null, // selection anchor {x,y} or null

    blink: 0,
    curOn: true, // cursor blink state
    fname: "", // current filename
    dirty: false,
    openFiles: [], // [{path,buf,cx,cy,ox,oy,targetOy,anchor,undoStack,redoStack,dirty,savedBuf}]
    fileIdx: -1, // active file index (-1 = no files open)
    msg: "",
    msgT: 0, // status message + timer
    restored: false, // set by _restore to skip default open in _init

    // undo / redo
    MAXUNDO: 200,
    undoStack: [],
    redoStack: [],

    // file browser
    brMode: false,
    brEntries: [], // {name, isDir}
    brIdx: 0,
    brScroll: 0,
    brDir: "", // current directory relative to cart root

    // find & replace
    findMode: false,
    findText: "",
    replaceText: "",
    findReplace: false, // true = show replace field
    findField: 0, // 0 = find, 1 = replace

    // go to line
    gotoMode: false,
    gotoText: "",

    // vim mode
    vimEnabled: false,
    vim: "normal", // "normal", "insert", "visual", "vline", "command"
    vimCount: "",
    vimPending: "",
    vimOpCount: 1,
    vimCmd: "",
    vimReg: "",
    vimLinewise: false,
    vimSearch: "",
    vimVlineStart: 0,

    // double-click state
    lastClickTime: 0,
    lastClickX: 0,
    lastClickY: 0,
    clickCount: 0,

    // smooth scrolling
    targetOy: 0,

    // modified line tracking (since last save)
    savedBuf: [], // snapshot at last save/open

    // persistent find highlighting
    lastFindText: "",

    // tab state
    activeTab: TAB_CODE,

    // sprite editor state
    sprSel: 0,
    sprCol: 7,
    sprTool: 0, // 0=pen, 1=eraser, 2=fill, 3=rect, 4=line, 5=circle
    sprScrollY: 0,
    sprHoverX: -1, // pixel coord under cursor (-1 = none)
    sprHoverY: -1,
    sprRectAnchor: null, // {x,y} for rectangle drag start
    sprLineAnchor: null, // {x,y} for line tool start
    sprCircAnchor: null, // {x,y} for circle tool center
    sprSizeW: 1, // multi-tile width (1 or 2)
    sprSizeH: 1, // multi-tile height (1 or 2)
    sprClipboard: null, // {w,h,pixels[]} for copy/paste

    // map editor state
    mapCamX: 0,
    mapCamY: 0,
    mapLayer: 0,
    mapTile: 0,
    mapTool: 0, // 0=pencil, 1=eraser
    mapGridOn: true,
    mapPickScrollY: 0,

    // caches (invalidated on edit)
    _blockStateCache: [],
    _blockStateDirty: true,
    _bracketMatchCache: null,
    _bracketDepthCache: [],
    _bracketDepthDirty: true,
    _bufVersion: 0,
    _lastCacheVersion: -1,

    // minimap cache
    _mmCacheVersion: -1,
    _mmLineLens: [],

    invalidateCaches() {
        this._bufVersion++;
    },
};
