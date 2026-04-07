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
    sprSelAnchor: null, // {x,y} for selection marquee drag start
    sprDirty: false, // sprite sheet has unsaved changes
    sprSizeW: 1, // multi-tile width in tiles
    sprSizeH: 1, // multi-tile height in tiles
    sprZoom: 0, // canvas zoom override (0 = auto-fit)
    sprPanX: 0, // canvas pan offset X (pixels)
    sprPanY: 0, // canvas pan offset Y (pixels)
    sprPanning: false, // middle-click pan in progress
    sprGoto: false, // sprite goto input active
    sprGotoTxt: "", // sprite goto input text
    sprClipboard: null, // {w,h,pixels[]} for copy/paste
    sprFilled: false, // filled shape mode for rect/circle
    sprMirrorX: false, // horizontal symmetry
    sprMirrorY: false, // vertical symmetry
    sprPalScrollY: 0, // palette vertical scroll (in rows)
    sprAnimPlay: false, // animation preview playing
    sprAnimFrom: 0, // animation range start sprite
    sprAnimTo: 0, // animation range end sprite
    sprAnimFps: 8, // animation frames per second
    sprAnimTimer: 0, // accumulator for animation frame timing
    sprAnimFrame: 0, // current frame index in animation
    sprSelRect: null, // {x0,y0,x1,y1} marquee selection in sprite pixels
    sprSelFloat: null, // {x,y,w,h,pixels[]} floating selection being moved
    sprSelDrag: null, // {ox,oy} drag offset when moving float
    sprLastPenX: -1, // last pen pixel X for line interpolation
    sprLastPenY: -1, // last pen pixel Y for line interpolation
    helpOverlay: false, // show keyboard shortcut help

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
