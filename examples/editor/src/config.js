// ─── dithr editor ─────────────────────────────────────────────────────────────
//
// A minimal text editor built on the dithr fantasy console engine.
// Framebuffer: 640×360, built-in 4×6 font (5×7 advance with spacing).
//
// Shortcuts:
//   Ctrl+O   Open file browser       Ctrl+S   Save file
//   Ctrl+Z   Undo                    Ctrl+Y   Redo
//   Ctrl+X   Cut (selection/line)    Ctrl+C   Copy (selection/line)
//   Ctrl+V   Paste                   Ctrl+A   Select all
//   Ctrl+D   Duplicate line          Ctrl+Shift+D  Duplicate selection
//   Ctrl+Shift+K  Delete line        Ctrl+/   Toggle line comment
//   Ctrl+F   Find                    Ctrl+H   Find & replace
//   Ctrl+G   Go to line              Ctrl+`   Toggle vim mode
//   Ctrl+Left/Right  Word navigation
//   Ctrl+Shift+Up/Down  Move line    Ctrl+Shift+Enter  Insert line above
//   Ctrl+Backspace  Delete word left Ctrl+Delete  Delete word right
//   Tab   Indent selection   Shift+Tab  Dedent selection   Esc  Clear sel
//   Home  Smart home (toggle col 0 / first non-blank)
//   Double-click  Select word        Triple-click  Select line
//
// Features:  Bracket matching & rainbow colorization, indent guides,
//   minimap, scrollbar, modified line indicators, smooth scrolling,
//   persistent find highlights, smart auto-indent after {, tab rendering
//
// Vim mode (toggle with Ctrl+`):
//   Normal:  h/j/k/l  w/b/e  0/$/^  gg/G  x  dd/yy/cc  d/c/y+motion
//            i/I/a/A/o/O  p/P  u  J  D/C  v/V  :  /pattern  n/N  r
//   Visual:  motions extend selection, d/c/y/x operate
//   Command: :w :q :wq :e :<line>
//
// ──────────────────────────────────────────────────────────────────────────────

// ─── Layout ──────────────────────────────────────────────────────────────────

const CW = 5; // char advance (4px glyph + 1px)
const CH = 7; // line advance (6px glyph + 1px)
const FB_W = 640,
    FB_H = 360;
const COLS = (FB_W / CW) | 0; // 128
const ROWS = (FB_H / CH) | 0; // 51

const GUTTER = 5; // chars for line numbers
const HEAD = 1; // header rows
const FOOT = 1; // status-bar rows
const EROWS = ROWS - HEAD - FOOT; // 49
const MINIMAP_W = 10; // minimap width in pixels
const SCROLLBAR_W = 2; // scrollbar width in pixels
const ECOLS = COLS - GUTTER; // 123 (minimap overlays text area)

const TAB = "  "; // soft tab (2 spaces)
const SCROLL_MARGIN = 4; // lines of context kept above/below cursor

// ─── Palette indices ─────────────────────────────────────────────────────────

const BG = 0; // black
const FG = 7; // white (near-white #FFF1E8)
const GUTBG = 1; // dark blue
const GUTFG = 6; // light gray
const HEADBG = 1;
const HEADFG = 7;
const FOOTBG = 1;
const FOOTFG = 6;
const CURFG = 7; // cursor colour
const LINEBG = 17; // #222222 — subtle current-line highlight
const SELBG = 2; // dark purple — selection
const KWCOL = 12; // cyan — keywords
const STRCOL = 11; // green — strings
const COMCOL = 5; // dark gray — comments
const NUMCOL = 9; // orange — numbers
const DIRTCOL = 8; // red — unsaved indicator
const TRAILBG = 20; // dark brown — trailing whitespace
const MATCHBG = 9; // orange — find match highlight
const BRACKETBG = 18; // #3A3A3A — matching bracket highlight
const GUIDECOL = 17; // #222222 — indent guide
const SCROLLBG = 17; // scrollbar track
const SCROLLFG = 6; // scrollbar thumb
const ADDCOL = 11; // green — added line indicator
const MODCOL = 12; // cyan — modified line indicator

// ─── Bracket / auto-close tables ─────────────────────────────────────────────

const AUTO_CLOSE = { "(": ")", "[": "]", "{": "}" };
const BRACKET_PAIRS = { "(": ")", ")": "(", "[": "]", "]": "[", "{": "}", "}": "{" };
const BRACKET_COLORS = [8, 9, 12, 11, 13, 14]; // rainbow bracket colors

// ─── JS keywords for syntax highlighting ─────────────────────────────────────

const KEYWORDS = new Set([
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
]);
