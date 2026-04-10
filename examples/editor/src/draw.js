// ─── Tab bar (shared across all tabs) ────────────────────────────────────────

import { st } from './state.js';
import {
    FB_W,
    FB_H,
    CW,
    CH,
    LINE_H,
    LINE_PAD,
    GUTTER,
    EDIT_Y,
    FOOT_Y,
    EROWS,
    ECOLS,
    TAB,
    BG,
    FG,
    GUTBG,
    GUTFG,
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
    FILETABBG,
    SEPC,
    TAB_H,
    TAB_NAMES,
    FILE_TAB_H,
    FOOT_H,
} from './config.js';
import { clamp, ensureCaches, getCachedBracketMatch } from './helpers.js';
import { tokenize } from './tokenizer.js';
import { selOrdered, switchToFile, closeFile } from './buffer.js';

export function drawTabBar() {
    gfx.rectfill(0, 0, FB_W - 1, TAB_H - 1, TABBG);
    const mx = mouse.x();
    const my = mouse.y();
    let tx = 0;
    const textY = Math.floor((TAB_H - CH) / 2); // vertically center text
    for (let i = 0; i < TAB_NAMES.length; i++) {
        const dirtyFlags = [st.dirty, st.sprDirty, st.mapDirty, st.sfxDirty, st.musDirty];
        const dirty = dirtyFlags[i] || false;
        const label = ` ${i + 1}:${TAB_NAMES[i]} `;
        const w = label.length * CW;
        const hovered = mx >= tx && mx < tx + w && my < TAB_H;
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
        // Draw dirty dot (2x2 filled rect centered in the leading space)
        if (dirty) {
            const dx = tx + Math.floor(CW / 2) - 1;
            const dy = textY + Math.floor(CH / 2) - 1;
            gfx.rectfill(dx, dy, dx + 1, dy + 1, DIRTCOL);
        }
        // Click to switch tabs
        if (mouse.btnp(0) && my < TAB_H && mx >= tx && mx < tx + w) {
            st.activeTab = i;
            mouse.show(); // restore cursor when leaving sprite editor
        }
        tx += w + 1;
    }
}
// ─── File tab bar (open files, code mode only) ─────────────────────────────────

export function drawFileTabs() {
    const y = TAB_H;
    const h = FILE_TAB_H;
    const textY = y + Math.floor((h - CH) / 2);
    gfx.rectfill(0, y, FB_W - 1, y + h - 1, FILETABBG);
    // Separator between main tab bar and file tabs
    gfx.line(0, y, FB_W - 1, y, SEPC);
    if (!st.openFiles.length) return;
    const mx = mouse.x();
    const my = mouse.y();
    let tx = 0;
    for (let i = 0; i < st.openFiles.length; i++) {
        const f = st.openFiles[i];
        const base = f.path.split('/').pop();
        const dirty = i === st.fileIdx ? st.dirty : f.dirty;
        const label = ` ${base} `;
        const w = label.length * CW;
        const hovered = mx >= tx && mx < tx + w && my >= y && my < y + h;
        if (i === st.fileIdx) {
            gfx.rectfill(tx, y, tx + w - 1, y + h - 1, BG);
            gfx.print(label, tx, textY, FG);
            gfx.line(tx, y + h - 1, tx + w - 1, y + h - 1, FG);
        } else {
            if (hovered) gfx.rectfill(tx, y, tx + w - 1, y + h - 1, TABHOV);
            gfx.print(label, tx, textY, hovered ? FG : TABINACT);
        }
        // Draw dirty dot
        if (dirty) {
            const dx = tx + Math.floor(CW / 2) - 1;
            const dy = textY + Math.floor(CH / 2) - 1;
            gfx.rectfill(dx, dy, dx + 1, dy + 1, DIRTCOL);
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
    const editY = EDIT_Y;
    const footY = FOOT_Y;
    const gutterPx = GUTTER * CW;

    // ── Vim indicator (right side of mode tab bar) ──
    if (st.vimEnabled) {
        const tabTextY = Math.floor((TAB_H - CH) / 2);
        gfx.print('[vim]', FB_W - 6 * CW, tabTextY, GUTFG);
    }

    // ── Gutter background ──
    gfx.rectfill(0, editY, gutterPx - 2, footY - 1, GUTBG);

    // ── Use cached block comment and bracket depth state ──
    ensureCaches();

    // ── Rebuild token cache when buffer changes ──
    if (st._tokenCacheVersion !== st._bufVersion) {
        st._tokenCacheVersion = st._bufVersion;
        const tc = st._tokenCache;
        let ib = false;
        for (let i = 0; i < st.buf.length; i++) {
            const r = tokenize(st.buf[i], ib);
            tc[i] = r;
            ib = r.inBlock;
        }
        tc.length = st.buf.length;
    }

    const inBlock = st.oy < st._blockStateCache.length ? st._blockStateCache[st.oy] : false;

    // Pre-compute selection bounds once for the entire frame
    const sel = selOrdered();

    // ── Text area ──
    for (let r = 0; r < EROWS; r++) {
        const li = st.oy + r;
        if (li >= st.buf.length) break;
        const py = editY + r * LINE_H;

        // Current-line highlight
        if (li === st.cy) {
            gfx.rectfill(gutterPx, py, FB_W - 1, py + LINE_H - 1, LINEBG);
        }

        // Selection highlight
        drawSelection(li, py, sel);

        // Find match highlighting (persistent: use lastFindText when not in findMode)
        const highlightText = st.findMode ? st.findText : st.lastFindText;
        if (highlightText) {
            const line = st.buf[li];
            let idx = line.indexOf(highlightText);
            while (idx >= 0) {
                let x0 = gutterPx + (idx - st.ox) * CW;
                let x1 = gutterPx + (idx + highlightText.length - st.ox) * CW - 1;
                x0 = clamp(x0, gutterPx, FB_W - 1);
                x1 = clamp(x1, gutterPx, FB_W - 1);
                if (x1 >= x0 && idx + highlightText.length > st.ox && idx < st.ox + ECOLS) {
                    gfx.rectfill(x0, py, x1, py + LINE_H - 1, MATCHBG);
                }
                idx = line.indexOf(highlightText, idx + 1);
            }
        }

        // Trailing whitespace highlight (skip current line)
        if (li !== st.cy) {
            const line = st.buf[li];
            let trimEnd = line.length;
            while (trimEnd > 0 && (line[trimEnd - 1] === ' ' || line[trimEnd - 1] === '\t'))
                trimEnd--;
            if (trimEnd < line.length && trimEnd >= st.ox) {
                let x0 = gutterPx + Math.max(0, trimEnd - st.ox) * CW;
                let x1 = gutterPx + (line.length - st.ox) * CW - 1;
                x0 = clamp(x0, gutterPx, FB_W - 1);
                x1 = clamp(x1, gutterPx, FB_W - 1);
                if (x1 >= x0) {
                    gfx.rectfill(x0, py, x1, py + LINE_H - 2, TRAILBG);
                }
            }
        }

        // Indent guides
        let indentLevel = 0;
        const line = st.buf[li];
        while (indentLevel < line.length && line[indentLevel] === ' ') indentLevel++;
        const tabW = TAB.length;
        for (let g = tabW; g < indentLevel; g += tabW) {
            const gx = gutterPx + (g - st.ox) * CW - 1;
            if (gx > gutterPx && gx < FB_W - MINIMAP_W) {
                for (let dy = 0; dy < LINE_H; dy += 2) {
                    gfx.pset(gx, py + dy, GUIDECOL);
                }
            }
        }

        // Line number (highlight current line number)
        const num = String(li + 1);
        const pad = GUTTER - 1 - num.length;
        if (li === st.cy) {
            gfx.rectfill(0, py, gutterPx - 2, py + LINE_H - 1, GUTBG);
            gfx.print(num, pad * CW, py + LINE_PAD, CURFG);
        } else {
            gfx.print(num, pad * CW, py + LINE_PAD, GUTFG);
        }

        // Modified line indicator in gutter
        if (st.savedBuf.length > 0) {
            if (li >= st.savedBuf.length || st.buf[li] !== st.savedBuf[li]) {
                const indicatorCol = li >= st.savedBuf.length ? ADDCOL : MODCOL;
                gfx.rectfill(gutterPx - 2, py, gutterPx - 2, py + LINE_H - 2, indicatorCol);
            }
        }

        // Syntax-highlighted text (from token cache)
        const cached = st._tokenCache[li];
        const tokens = cached ? cached.toks : tokenize(st.buf[li], inBlock).toks;
        let col = 0;
        // Running bracket depth from cached line-start value
        let bDepth = li < st._bracketDepthCache.length ? st._bracketDepthCache[li] : 0;

        // Batched printing: accumulate same-color runs and flush as one gfx.print() call
        let runStr = '';
        let runCol = -1;
        let runX = 0;

        for (let t = 0; t < tokens.length; t++) {
            const tok = tokens[t];
            const tokCol = tok.col;
            for (let c = 0; c < tok.text.length; c++) {
                let ch = tok.text[c];
                const vc = col - st.ox;
                let charCol = tokCol;
                let isBracket = false;

                // Track bracket depth inline
                if (tokCol === FG && BRACKET_PAIRS[ch]) {
                    isBracket = true;
                    const isOpen = ch === '(' || ch === '[' || ch === '{';
                    if (!isOpen) bDepth--;
                    charCol = BRACKET_COLORS[Math.abs(bDepth) % BRACKET_COLORS.length];
                    if (isOpen) bDepth++;
                }

                if (ch === '\t') {
                    charCol = GUIDECOL;
                    ch = '\x1a';
                }

                if (vc >= 0 && vc < ECOLS) {
                    if (charCol !== runCol || isBracket) {
                        // Flush previous run
                        if (runStr) gfx.print(runStr, runX, py + LINE_PAD, runCol);
                        runStr = ch;
                        runCol = charCol;
                        runX = gutterPx + vc * CW;
                    } else {
                        runStr += ch;
                    }
                } else if (runStr) {
                    // Off-screen: flush any pending run
                    gfx.print(runStr, runX, py + LINE_PAD, runCol);
                    runStr = '';
                    runCol = -1;
                }
                col++;
            }
        }
        // Flush final run
        if (runStr) gfx.print(runStr, runX, py + LINE_PAD, runCol);
    }

    // ── Matching bracket highlight ──
    const matchBracket = getCachedBracketMatch();
    if (matchBracket) {
        const mr = matchBracket.y - st.oy;
        const mc = matchBracket.x - st.ox;
        if (mr >= 0 && mr < EROWS && mc >= 0 && mc < ECOLS) {
            const mx2 = gutterPx + mc * CW;
            const my2 = editY + mr * LINE_H;
            gfx.rectfill(mx2, my2 + LINE_PAD, mx2 + CW - 2, my2 + LINE_PAD + CH - 2, BRACKETBG);
            gfx.print(st.buf[matchBracket.y][matchBracket.x], mx2, my2 + LINE_PAD, FG);
        }
    }

    // ── Scrollbar ──
    if (st.buf.length > EROWS) {
        const trackX = FB_W - SCROLLBAR_W;
        const trackH = footY - editY;
        gfx.rectfill(trackX, editY, FB_W - 1, footY - 1, SCROLLBG);
        const thumbH = Math.max(8, (EROWS / st.buf.length) * trackH) | 0;
        const thumbY =
            (editY + (st.oy / Math.max(1, st.buf.length - EROWS)) * (trackH - thumbH)) | 0;
        gfx.rectfill(trackX, thumbY, FB_W - 1, thumbY + thumbH - 1, SCROLLFG);
    }

    // ── Minimap ──
    {
        const mmX = FB_W - MINIMAP_W - SCROLLBAR_W - 1;
        const mmH = footY - editY;
        const totalLines = st.buf.length;
        if (totalLines > 0) {
            const scale = mmH / totalLines;

            // Recompute cached line lengths only when buffer changes
            if (st._mmCacheVersion !== st._bufVersion) {
                st._mmCacheVersion = st._bufVersion;
                st._mmLineLens = new Array(totalLines);
                for (let i = 0; i < totalLines; i++) {
                    st._mmLineLens[i] = Math.min(st.buf[i].length, MINIMAP_W);
                }
            }

            const pixelRows = Math.min(mmH, totalLines);
            for (let py2 = 0; py2 < pixelRows; py2++) {
                const lineIdx = (py2 / scale) | 0;
                if (lineIdx >= totalLines) break;
                const lineLen = st._mmLineLens[lineIdx];
                if (lineLen > 0) {
                    const mmCol = lineIdx === st.cy ? CURFG : GUTFG;
                    gfx.rectfill(mmX, editY + py2, mmX + lineLen - 1, editY + py2, mmCol);
                }
            }

            // Draw visible region overlay
            const vrY = (editY + st.oy * scale) | 0;
            const vrH = Math.max(1, (EROWS * scale) | 0);
            gfx.rectfill(mmX - 1, vrY, mmX - 1, Math.min(vrY + vrH, footY - 1), SCROLLFG);
        }
    }

    // ── Cursor ──
    if (st.curOn) {
        const scrX = gutterPx + (st.cx - st.ox) * CW;
        const scrY = editY + (st.cy - st.oy) * LINE_H;
        if (st.cx >= st.ox && st.cx <= st.ox + ECOLS && st.cy >= st.oy && st.cy < st.oy + EROWS) {
            if (st.vimEnabled && st.vim !== 'insert') {
                // Block cursor
                gfx.rectfill(scrX, scrY + LINE_PAD, scrX + CW - 2, scrY + LINE_PAD + CH - 2, CURFG);
                if (st.cx < st.buf[st.cy].length) {
                    gfx.print(st.buf[st.cy][st.cx], scrX, scrY + LINE_PAD, BG);
                }
            } else {
                // Line cursor
                gfx.rectfill(scrX, scrY + LINE_PAD, scrX, scrY + LINE_PAD + CH - 2, CURFG);
            }
        }
    }

    // ── Status bar ──
    gfx.rectfill(0, footY, FB_W - 1, FB_H - 1, FOOTBG);
    const footTextY = footY + Math.floor((FOOT_H - CH) / 2);
    if (st.vimEnabled && st.vim === 'command') {
        gfx.print(`:${st.vimCmd}_`, gutterPx, footTextY, FG);
    } else {
        let modeStr = '';
        if (st.vimEnabled) {
            if (st.vim === 'insert') modeStr = '-- INSERT --  ';
            else if (st.vim === 'visual') modeStr = '-- VISUAL --  ';
            else if (st.vim === 'vline') modeStr = '-- V-LINE --  ';
        }
        const pos = `${modeStr}Ln ${st.cy + 1}, Col ${st.cx + 1}  (${st.buf.length} lines)`;
        gfx.print(pos, gutterPx, footTextY, FOOTFG);
        // Pending operator / count feedback
        let right = '';
        if (st.vimEnabled && (st.vimCount || st.vimPending)) right = st.vimCount + st.vimPending;
        if (st.msg) right = st.msg;
        if (right) {
            const rw = gfx.textWidth(right);
            gfx.print(right, FB_W - rw - CW, footTextY, FOOTFG);
        }
    }

    // ── Autocomplete popup ──
    if (st.acActive && st.acItems.length > 0) {
        let maxW = 0;
        for (let i = 0; i < st.acItems.length; i++) {
            const w = st.acItems[i].length * CW;
            if (w > maxW) maxW = w;
        }
        const popW = maxW + CW * 2;
        const popH = st.acItems.length * CH;
        let px = st.acX;
        let py = st.acY;
        if (px + popW > FB_W) px = FB_W - popW;
        if (py + popH > footY) py = st.acY - LINE_H - popH;
        gfx.rectfill(px, py, px + popW - 1, py + popH - 1, GUTBG);
        gfx.rect(px, py, px + popW - 1, py + popH - 1, SEPC);
        for (let i = 0; i < st.acItems.length; i++) {
            const iy = py + i * CH;
            if (i === st.acIdx) {
                gfx.rectfill(px + 1, iy, px + popW - 2, iy + CH - 1, SELBG);
            }
            gfx.print(st.acItems[i], px + CW, iy, i === st.acIdx ? FG : FOOTFG);
        }
    }
}

// ─── Selection drawing ───────────────────────────────────────────────────────

export function drawSelection(lineIdx, py, s) {
    if (!s) return;
    if (lineIdx < s.a.y || lineIdx > s.b.y) return;

    let sc;
    let ec;
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

    const gutterPx = GUTTER * CW;
    let x0 = gutterPx + (sc - st.ox) * CW;
    let x1 = gutterPx + (ec - st.ox) * CW - 1;
    x0 = clamp(x0, gutterPx, FB_W - 1);
    x1 = clamp(x1, gutterPx, FB_W - 1);
    if (x1 >= x0) {
        gfx.rectfill(x0, py, x1, py + LINE_H - 1, SELBG);
    }
}
