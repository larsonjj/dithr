// ─── Tab bar (shared across all tabs) ────────────────────────────────────────

import { st } from "./state.js";
import {
    FB_W,
    FB_H,
    CW,
    CH,
    GUTTER,
    ROWS,
    HEAD,
    FOOT,
    EROWS,
    ECOLS,
    TAB,
    BG,
    FG,
    GUTBG,
    GUTFG,
    HEADBG,
    HEADFG,
    FOOTBG,
    FOOTFG,
    CURFG,
    LINEBG,
    SELBG,
    MATCHBG,
    TRAILBG,
    GUIDECOL,
    DIRTCOL,
    BRACKETBG,
    BRACKET_PAIRS,
    BRACKET_COLORS,
    SCROLLBAR_W,
    SCROLLBG,
    SCROLLFG,
    MINIMAP_W,
    MODCOL,
    ADDCOL,
    TABBG,
    TABFG,
    TABHOV,
    TABINACT,
    TAB_H,
    TAB_NAMES,
} from "./config.js";
import { clamp, ensureCaches, getCachedBracketMatch } from "./helpers.js";
import { tokenize } from "./tokenizer.js";
import { selOrdered, switchToFile, closeFile } from "./buffer.js";

export function drawTabBar() {
    gfx.rectfill(0, 0, FB_W - 1, TAB_H - 1, TABBG);
    let mx = mouse.x();
    let my = mouse.y();
    let tx = 0;
    let textY = Math.floor((TAB_H - CH) / 2); // vertically center text
    for (let i = 0; i < TAB_NAMES.length; i++) {
        let label = " " + (i + 1) + ":" + TAB_NAMES[i] + " ";
        let w = label.length * CW;
        let hovered = mx >= tx && mx < tx + w && my < TAB_H;
        if (i === st.activeTab) {
            gfx.rectfill(tx, 0, tx + w - 1, TAB_H - 1, BG);
            gfx.print(label, tx, textY, TABFG);
            // underline
            gfx.line(tx, TAB_H - 1, tx + w - 1, TAB_H - 1, TABFG);
        } else {
            if (hovered) {
                gfx.rectfill(tx, 0, tx + w - 1, TAB_H - 1, TABHOV);
            }
            gfx.print(label, tx, textY, hovered ? TABFG : TABINACT);
        }
        // Click to switch tabs
        if (mouse.btnp(0) && my < TAB_H && mx >= tx && mx < tx + w) {
            st.activeTab = i;
        }
        tx += w + 1;
    }
}
// ─── File tab bar (open files, code mode only) ─────────────────────────────────

export function drawFileTabs() {
    let y = TAB_H;
    gfx.rectfill(0, y, FB_W - 1, y + CH - 1, TABBG);
    if (!st.openFiles.length) return;
    let mx = mouse.x();
    let my = mouse.y();
    let tx = 0;
    for (let i = 0; i < st.openFiles.length; i++) {
        let f = st.openFiles[i];
        let base = f.path.split("/").pop();
        let dirty = i === st.fileIdx ? st.dirty : f.dirty;
        let label = (dirty ? "\x07" : " ") + base + " ";
        let w = label.length * CW;
        let hovered = mx >= tx && mx < tx + w && my >= y && my < y + CH;
        if (i === st.fileIdx) {
            gfx.rectfill(tx, y, tx + w - 1, y + CH - 1, BG);
            gfx.print(label, tx, y, FG);
            gfx.line(tx, y + CH - 1, tx + w - 1, y + CH - 1, FG);
        } else {
            if (hovered) gfx.rectfill(tx, y, tx + w - 1, y + CH - 1, TABHOV);
            gfx.print(label, tx, y, hovered ? FG : TABINACT);
        }
        if (mouse.btnp(0) && hovered) {
            switchToFile(i);
        }
        if (mouse.btnp(1) && hovered) {
            closeFile(i);
            return;
        }
        tx += w;
    }
}
// ─── Editor drawing ──────────────────────────────────────────────────────────

export function drawEditor() {
    let editY = HEAD * CH;
    let footY = (ROWS - FOOT) * CH;
    let gutterPx = GUTTER * CW;

    // ── Vim indicator (right side of mode tab bar) ──
    if (st.vimEnabled) {
        let tabTextY = Math.floor((TAB_H - CH) / 2);
        gfx.print("[vim]", FB_W - 6 * CW, tabTextY, GUTFG);
    }

    // ── Gutter background ──
    gfx.rectfill(0, editY, gutterPx - 2, footY - 1, GUTBG);

    // ── Use cached block comment and bracket depth state ──
    ensureCaches();
    let inBlock = st.oy < st._blockStateCache.length ? st._blockStateCache[st.oy] : false;

    // ── Text area ──
    for (let r = 0; r < EROWS; r++) {
        let li = st.oy + r;
        if (li >= st.buf.length) break;
        let py = editY + r * CH;

        // Current-line highlight
        if (li === st.cy) {
            gfx.rectfill(gutterPx, py, FB_W - 1, py + CH - 1, LINEBG);
        }

        // Selection highlight
        drawSelection(li, py);

        // Find match highlighting (persistent: use lastFindText when not in findMode)
        let highlightText = st.findMode ? st.findText : st.lastFindText;
        if (highlightText) {
            let line = st.buf[li];
            let idx = line.indexOf(highlightText);
            while (idx >= 0) {
                let x0 = gutterPx + (idx - st.ox) * CW;
                let x1 = gutterPx + (idx + highlightText.length - st.ox) * CW - 1;
                x0 = clamp(x0, gutterPx, FB_W - 1);
                x1 = clamp(x1, gutterPx, FB_W - 1);
                if (x1 >= x0 && idx + highlightText.length > st.ox && idx < st.ox + ECOLS) {
                    gfx.rectfill(x0, py, x1, py + CH - 1, MATCHBG);
                }
                idx = line.indexOf(highlightText, idx + 1);
            }
        }

        // Trailing whitespace highlight (skip current line)
        if (li !== st.cy) {
            let line = st.buf[li];
            let trimEnd = line.length;
            while (trimEnd > 0 && (line[trimEnd - 1] === " " || line[trimEnd - 1] === "\t"))
                trimEnd--;
            if (trimEnd < line.length && trimEnd >= st.ox) {
                let x0 = gutterPx + Math.max(0, trimEnd - st.ox) * CW;
                let x1 = gutterPx + (line.length - st.ox) * CW - 1;
                x0 = clamp(x0, gutterPx, FB_W - 1);
                x1 = clamp(x1, gutterPx, FB_W - 1);
                if (x1 >= x0) {
                    gfx.rectfill(x0, py, x1, py + CH - 2, TRAILBG);
                }
            }
        }

        // Indent guides
        let indentLevel = 0;
        let line = st.buf[li];
        while (indentLevel < line.length && line[indentLevel] === " ") indentLevel++;
        let tabW = TAB.length;
        for (let g = tabW; g < Math.max(indentLevel, li === st.cy ? 0 : indentLevel); g += tabW) {
            let gx = gutterPx + (g - st.ox) * CW - 1;
            if (gx > gutterPx && gx < FB_W - MINIMAP_W) {
                for (let dy = 0; dy < CH; dy += 2) {
                    gfx.pset(gx, py + dy, GUIDECOL);
                }
            }
        }

        // Line number (highlight current line number)
        let num = String(li + 1);
        let pad = GUTTER - 1 - num.length;
        if (li === st.cy) {
            gfx.rectfill(0, py, gutterPx - 2, py + CH - 1, GUTBG);
            gfx.print(num, pad * CW, py, CURFG);
        } else {
            gfx.print(num, pad * CW, py, GUTFG);
        }

        // Modified line indicator in gutter
        if (st.savedBuf.length > 0) {
            if (li >= st.savedBuf.length || st.buf[li] !== st.savedBuf[li]) {
                let indicatorCol = li >= st.savedBuf.length ? ADDCOL : MODCOL;
                gfx.rectfill(gutterPx - 2, py, gutterPx - 2, py + CH - 2, indicatorCol);
            }
        }

        // Syntax-highlighted text (with block comment tracking)
        let result = tokenize(st.buf[li], inBlock);
        let tokens = result.toks;
        inBlock = result.inBlock;
        let col = 0;
        // Running bracket depth from cached line-start value
        let bDepth = li < st._bracketDepthCache.length ? st._bracketDepthCache[li] : 0;
        for (let t = 0; t < tokens.length; t++) {
            let tok = tokens[t];
            let tokCol = tok.col;
            for (let c = 0; c < tok.text.length; c++) {
                let ch = tok.text[c];
                // Track bracket depth inline
                let isBracket = false;
                if (tokCol === FG && BRACKET_PAIRS[ch]) {
                    isBracket = true;
                    let isOpen = ch === "(" || ch === "[" || ch === "{";
                    if (!isOpen) bDepth--;
                    let vc = col - st.ox;
                    if (vc >= 0 && vc < ECOLS) {
                        let bCol = BRACKET_COLORS[Math.abs(bDepth) % BRACKET_COLORS.length];
                        gfx.print(ch, gutterPx + vc * CW, py, bCol);
                    }
                    if (isOpen) bDepth++;
                }
                if (!isBracket) {
                    let vc = col - st.ox;
                    if (vc >= 0 && vc < ECOLS) {
                        if (ch === "\t") {
                            gfx.print("\x1a", gutterPx + vc * CW, py, GUIDECOL);
                        } else {
                            gfx.print(ch, gutterPx + vc * CW, py, tokCol);
                        }
                    }
                }
                col++;
            }
        }
    }

    // ── Matching bracket highlight ──
    let matchBracket = getCachedBracketMatch();
    if (matchBracket) {
        let mr = matchBracket.y - st.oy;
        let mc = matchBracket.x - st.ox;
        if (mr >= 0 && mr < EROWS && mc >= 0 && mc < ECOLS) {
            let mx2 = gutterPx + mc * CW;
            let my2 = editY + mr * CH;
            gfx.rectfill(mx2, my2, mx2 + CW - 2, my2 + CH - 2, BRACKETBG);
            gfx.print(st.buf[matchBracket.y][matchBracket.x], mx2, my2, FG);
        }
    }

    // ── Scrollbar ──
    if (st.buf.length > EROWS) {
        let trackX = FB_W - SCROLLBAR_W;
        let trackH = footY - editY;
        gfx.rectfill(trackX, editY, FB_W - 1, footY - 1, SCROLLBG);
        let thumbH = Math.max(8, (EROWS / st.buf.length) * trackH) | 0;
        let thumbY = (editY + (st.oy / Math.max(1, st.buf.length - EROWS)) * (trackH - thumbH)) | 0;
        gfx.rectfill(trackX, thumbY, FB_W - 1, thumbY + thumbH - 1, SCROLLFG);
    }

    // ── Minimap ──
    {
        let mmX = FB_W - MINIMAP_W - SCROLLBAR_W - 1;
        let mmH = footY - editY;
        let totalLines = st.buf.length;
        if (totalLines > 0) {
            let scale = mmH / totalLines;

            // Recompute cached line lengths only when buffer changes
            if (st._mmCacheVersion !== st._bufVersion) {
                st._mmCacheVersion = st._bufVersion;
                st._mmLineLens = new Array(totalLines);
                for (let i = 0; i < totalLines; i++) {
                    st._mmLineLens[i] = Math.min(st.buf[i].length, MINIMAP_W);
                }
            }

            let pixelRows = Math.min(mmH, totalLines);
            for (let py2 = 0; py2 < pixelRows; py2++) {
                let lineIdx = (py2 / scale) | 0;
                if (lineIdx >= totalLines) break;
                let lineLen = st._mmLineLens[lineIdx];
                if (lineLen > 0) {
                    let mmCol = lineIdx === st.cy ? CURFG : GUTFG;
                    gfx.rectfill(mmX, editY + py2, mmX + lineLen - 1, editY + py2, mmCol);
                }
            }

            // Draw visible region overlay
            let vrY = (editY + st.oy * scale) | 0;
            let vrH = Math.max(1, (EROWS * scale) | 0);
            gfx.rectfill(mmX - 1, vrY, mmX - 1, Math.min(vrY + vrH, footY - 1), SCROLLFG);
        }
    }

    // ── Cursor ──
    if (st.curOn) {
        let scrX = gutterPx + (st.cx - st.ox) * CW;
        let scrY = editY + (st.cy - st.oy) * CH;
        if (st.cx >= st.ox && st.cx <= st.ox + ECOLS && st.cy >= st.oy && st.cy < st.oy + EROWS) {
            if (st.vimEnabled && st.vim !== "insert") {
                // Block cursor
                gfx.rectfill(scrX, scrY, scrX + CW - 2, scrY + CH - 2, CURFG);
                if (st.cx < st.buf[st.cy].length) {
                    gfx.print(st.buf[st.cy][st.cx], scrX, scrY, BG);
                }
            } else {
                // Line cursor
                gfx.rectfill(scrX, scrY, scrX, scrY + CH - 2, CURFG);
            }
        }
    }

    // ── Status bar ──
    gfx.rectfill(0, footY, FB_W - 1, footY + CH - 1, FOOTBG);
    if (st.vimEnabled && st.vim === "command") {
        gfx.print(":" + st.vimCmd + "_", gutterPx, footY, FG);
    } else {
        let modeStr = "";
        if (st.vimEnabled) {
            if (st.vim === "insert") modeStr = "-- INSERT --  ";
            else if (st.vim === "visual") modeStr = "-- VISUAL --  ";
            else if (st.vim === "vline") modeStr = "-- V-LINE --  ";
        }
        let pos =
            modeStr +
            "Ln " +
            (st.cy + 1) +
            ", Col " +
            (st.cx + 1) +
            "  (" +
            st.buf.length +
            " lines)";
        gfx.print(pos, gutterPx, footY, FOOTFG);
        // Pending operator / count feedback
        let right = "";
        if (st.vimEnabled && (st.vimCount || st.vimPending)) right = st.vimCount + st.vimPending;
        if (st.msg) right = st.msg;
        if (right) {
            let rw = gfx.textWidth(right);
            gfx.print(right, FB_W - rw - CW, footY, FOOTFG);
        }
    }
}

// ─── Selection drawing ───────────────────────────────────────────────────────

export function drawSelection(lineIdx, py) {
    let s = selOrdered();
    if (!s) return;
    if (lineIdx < s.a.y || lineIdx > s.b.y) return;

    let sc, ec;
    if (lineIdx === s.a.y && lineIdx === s.b.y) {
        sc = s.a.x;
        ec = s.b.x;
    } else if (lineIdx === s.a.y) {
        sc = s.a.x;
        ec = st.buf[lineIdx].length;
    } else if (lineIdx === s.b.y) {
        sc = 0;
        ec = s.b.x;
    } else {
        sc = 0;
        ec = st.buf[lineIdx].length;
    }

    let gutterPx = GUTTER * CW;
    let x0 = gutterPx + (sc - st.ox) * CW;
    let x1 = gutterPx + (ec - st.ox) * CW - 1;
    x0 = clamp(x0, gutterPx, FB_W - 1);
    x1 = clamp(x1, gutterPx, FB_W - 1);
    if (x1 >= x0) {
        gfx.rectfill(x0, py, x1, py + CH - 1, SELBG);
    }
}
