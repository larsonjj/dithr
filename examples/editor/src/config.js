// ─── dithr editor ─────────────────────────────────────────────────────────────
//
// A minimal text editor built on the dithr fantasy console engine.
// Framebuffer: 640×360, built-in 4×6 font (5×7 advance with spacing).
//
// Shortcuts (Cmd on macOS, Ctrl elsewhere):
//   Mod+O   Open file browser       Mod+S   Save file
//   Mod+Z   Undo                    Mod+Y / Mod+Shift+Z   Redo
//   Mod+X   Cut (selection/line)    Mod+C   Copy (selection/line)
//   Mod+V   Paste                   Mod+A   Select all
//   Mod+D   Duplicate line          Mod+Shift+D  Duplicate selection
//   Mod+Shift+K  Delete line        Mod+/   Toggle line comment
//   Mod+F   Find                    Mod+H   Find & replace
//   Mod+G   Go to line              Mod+`   Toggle vim mode
//   Mod+Left/Right  Word navigation
//   Mod+Shift+Up/Down  Move line    Mod+Shift+Enter  Insert line above
//   Mod+Backspace  Delete word left Mod+Delete  Delete word right
//   Tab   Indent selection   Shift+Tab  Dedent selection   Esc  Clear sel
//   Mod+Tab / Mod+Shift+Tab  Cycle open files
//   Mod+W   Close current file tab
//   Home  Smart home (toggle col 0 / first non-blank)
//   Double-click  Select word        Triple-click  Select line
//
// Features:  Bracket matching & rainbow colorization, indent guides,
//   minimap, scrollbar, modified line indicators, smooth scrolling,
//   persistent find highlights, smart auto-indent after {, tab rendering
//
// Vim mode (toggle with Mod+`):
//   Normal:  h/j/k/l  w/b/e  0/$/^  gg/G  x  dd/yy/cc  d/c/y+motion
//            i/I/a/A/o/O  p/P  u  J  D/C  v/V  :  /pattern  n/N  r
//   Visual:  motions extend selection, d/c/y/x operate
//   Command: :w :q :wq :e :<line>
//
// ──────────────────────────────────────────────────────────────────────────────

// ─── Layout ──────────────────────────────────────────────────────────────────

export const CW = 5; // char advance (4px glyph + 1px)
export const CH = 8; // line advance (6px glyph + 2px spacing)
export const FB_W = 640;
export const FB_H = 360;
export const COLS = (FB_W / CW) | 0; // 128
export const ROWS = (FB_H / CH) | 0; // 45

export const GUTTER = 5; // chars for line numbers
export const TAB_H = CH * 2; // tab bar height in pixels
export const FILE_TAB_H = CH * 2 - 2; // file tab bar height in pixels (12)
export const FOOT_H = CH * 2 - 2; // footer height in pixels (12)
export const EDIT_Y = TAB_H + FILE_TAB_H; // first text pixel row
export const LINE_H = CH + 2; // code editor line height (glyph + 2px spacing)
export const LINE_PAD = 2; // vertical text offset within LINE_H
export const FOOT_Y = FB_H - FOOT_H; // footer start pixel row (348)
export const EROWS = ((FOOT_Y - EDIT_Y) / LINE_H) | 0; // visible text rows
export const MINIMAP_W = 10; // minimap width in pixels
export const SCROLLBAR_W = 2; // scrollbar width in pixels
export const ECOLS = COLS - GUTTER; // 123 (minimap overlays text area)

export const TAB = "  "; // soft tab (2 spaces)
export const SCROLL_MARGIN = 4; // lines of context kept above/below cursor

// ─── Palette indices ─────────────────────────────────────────────────────────

export const BG = 0; // black
export const FG = 7; // white (near-white #FFF1E8)
export const GUTBG = 1; // dark blue
export const GUTFG = 6; // light gray
export const HEADBG = 1;
export const HEADFG = 7;
export const FOOTBG = 1;
export const FOOTFG = 6;
export const CURFG = 7; // cursor colour
export const LINEBG = 17; // #222222 — subtle current-line highlight
export const SELBG = 2; // dark purple — selection
export const KWCOL = 12; // cyan — keywords
export const STRCOL = 11; // green — strings
export const COMCOL = 5; // dark gray — comments
export const NUMCOL = 9; // orange — numbers
export const DIRTCOL = 8; // red — unsaved indicator
export const TRAILBG = 20; // dark brown — trailing whitespace
export const MATCHBG = 9; // orange — find match highlight
export const BRACKETBG = 18; // #3A3A3A — matching bracket highlight
export const GUIDECOL = 17; // #222222 — indent guide
export const SCROLLBG = 17; // scrollbar track
export const SCROLLFG = 6; // scrollbar thumb
export const ADDCOL = 11; // green — added line indicator
export const MODCOL = 12; // cyan — modified line indicator
export const TABBG = 1; // tab bar background
export const FILETABBG = 1; // file sub-header background (dark blue, same family as main tab)
export const TABFG = 7; // active tab text
export const TABINACT = 5; // inactive tab text
export const TABHOV = 17; // tab hover background
export const GRIDC = 17; // sprite/map grid lines
export const PANELBG = 16; // panel background (distinct from header/footer)
export const SEPC = 18; // separator / panel grid lines

// ─── Tab names ───────────────────────────────────────────────────────────────

export const TAB_CODE = 0;
export const TAB_SPRITES = 1;
export const TAB_MAP = 2;
export const TAB_NAMES = ["CODE", "SPRITES", "MAP"];

// ─── Bracket / auto-close tables ─────────────────────────────────────────────

export const AUTO_CLOSE = { "(": ")", "[": "]", "{": "}" };
export const BRACKET_PAIRS = { "(": ")", ")": "(", "[": "]", "]": "[", "{": "}", "}": "{" };
export const BRACKET_COLORS = [8, 9, 12, 11, 13, 14]; // rainbow bracket colors

// ─── JS keywords for syntax highlighting ─────────────────────────────────────

export const KEYWORDS = new Set([
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
