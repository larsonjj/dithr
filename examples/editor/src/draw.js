// ─── Editor drawing ──────────────────────────────────────────────────────────

function drawEditor() {
    let editY = HEAD * CH;
    let footY = (ROWS - FOOT) * CH;
    let gutterPx = GUTTER * CW;

    // ── Header bar ──
    gfx.rectfill(0, 0, FB_W - 1, CH - 1, HEADBG);
    let title = (dirty ? "\x07 " : "  ") + (fname || "[untitled]");
    gfx.print(title, gutterPx, 0, dirty ? DIRTCOL : HEADFG);
    if (vimEnabled) {
        gfx.print("[vim]", FB_W - 6 * CW, 0, GUTFG);
    }

    // ── Gutter background ──
    gfx.rectfill(0, editY, gutterPx - 2, footY - 1, GUTBG);

    // ── Use cached block comment and bracket depth state ──
    ensureCaches();
    let inBlock = oy < _blockStateCache.length ? _blockStateCache[oy] : false;

    // ── Text area ──
    for (let r = 0; r < EROWS; r++) {
        let li = oy + r;
        if (li >= buf.length) break;
        let py = editY + r * CH;

        // Current-line highlight
        if (li === cy) {
            gfx.rectfill(gutterPx, py, FB_W - 1, py + CH - 1, LINEBG);
        }

        // Selection highlight
        drawSelection(li, py);

        // Find match highlighting (persistent: use lastFindText when not in findMode)
        let highlightText = findMode ? findText : lastFindText;
        if (highlightText) {
            let line = buf[li];
            let idx = line.indexOf(highlightText);
            while (idx >= 0) {
                let x0 = gutterPx + (idx - ox) * CW;
                let x1 = gutterPx + (idx + highlightText.length - ox) * CW - 1;
                x0 = clamp(x0, gutterPx, FB_W - 1);
                x1 = clamp(x1, gutterPx, FB_W - 1);
                if (x1 >= x0 && idx + highlightText.length > ox && idx < ox + ECOLS) {
                    gfx.rectfill(x0, py, x1, py + CH - 1, MATCHBG);
                }
                idx = line.indexOf(highlightText, idx + 1);
            }
        }

        // Trailing whitespace highlight (skip current line)
        if (li !== cy) {
            let line = buf[li];
            let trimEnd = line.length;
            while (trimEnd > 0 && (line[trimEnd - 1] === " " || line[trimEnd - 1] === "\t"))
                trimEnd--;
            if (trimEnd < line.length && trimEnd >= ox) {
                let x0 = gutterPx + Math.max(0, trimEnd - ox) * CW;
                let x1 = gutterPx + (line.length - ox) * CW - 1;
                x0 = clamp(x0, gutterPx, FB_W - 1);
                x1 = clamp(x1, gutterPx, FB_W - 1);
                if (x1 >= x0) {
                    gfx.rectfill(x0, py, x1, py + CH - 2, TRAILBG);
                }
            }
        }

        // Indent guides
        let indentLevel = 0;
        let line = buf[li];
        while (indentLevel < line.length && line[indentLevel] === " ") indentLevel++;
        let tabW = TAB.length;
        for (let g = tabW; g < Math.max(indentLevel, li === cy ? 0 : indentLevel); g += tabW) {
            let gx = gutterPx + (g - ox) * CW - 1;
            if (gx > gutterPx && gx < FB_W - MINIMAP_W) {
                for (let dy = 0; dy < CH; dy += 2) {
                    gfx.pset(gx, py + dy, GUIDECOL);
                }
            }
        }

        // Line number (highlight current line number)
        let num = String(li + 1);
        let pad = GUTTER - 1 - num.length;
        if (li === cy) {
            gfx.rectfill(0, py, gutterPx - 2, py + CH - 1, GUTBG);
            gfx.print(num, pad * CW, py, CURFG);
        } else {
            gfx.print(num, pad * CW, py, GUTFG);
        }

        // Modified line indicator in gutter
        if (savedBuf.length > 0) {
            if (li >= savedBuf.length || buf[li] !== savedBuf[li]) {
                let indicatorCol = li >= savedBuf.length ? ADDCOL : MODCOL;
                gfx.rectfill(gutterPx - 2, py, gutterPx - 2, py + CH - 2, indicatorCol);
            }
        }

        // Syntax-highlighted text (with block comment tracking)
        let result = tokenize(buf[li], inBlock);
        let tokens = result.toks;
        inBlock = result.inBlock;
        let col = 0;
        // Running bracket depth from cached line-start value
        let bDepth = li < _bracketDepthCache.length ? _bracketDepthCache[li] : 0;
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
                    let vc = col - ox;
                    if (vc >= 0 && vc < ECOLS) {
                        let bCol = BRACKET_COLORS[Math.abs(bDepth) % BRACKET_COLORS.length];
                        gfx.print(ch, gutterPx + vc * CW, py, bCol);
                    }
                    if (isOpen) bDepth++;
                }
                if (!isBracket) {
                    let vc = col - ox;
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
        let mr = matchBracket.y - oy;
        let mc = matchBracket.x - ox;
        if (mr >= 0 && mr < EROWS && mc >= 0 && mc < ECOLS) {
            let mx2 = gutterPx + mc * CW;
            let my2 = editY + mr * CH;
            gfx.rectfill(mx2, my2, mx2 + CW - 2, my2 + CH - 2, BRACKETBG);
            gfx.print(buf[matchBracket.y][matchBracket.x], mx2, my2, FG);
        }
    }

    // ── Scrollbar ──
    if (buf.length > EROWS) {
        let trackX = FB_W - SCROLLBAR_W;
        let trackH = footY - editY;
        gfx.rectfill(trackX, editY, FB_W - 1, footY - 1, SCROLLBG);
        let thumbH = Math.max(8, (EROWS / buf.length) * trackH) | 0;
        let thumbY = (editY + (oy / Math.max(1, buf.length - EROWS)) * (trackH - thumbH)) | 0;
        gfx.rectfill(trackX, thumbY, FB_W - 1, thumbY + thumbH - 1, SCROLLFG);
    }

    // ── Minimap ──
    {
        let mmX = FB_W - MINIMAP_W - SCROLLBAR_W - 1;
        let mmH = footY - editY;
        let totalLines = buf.length;
        if (totalLines > 0) {
            let scale = mmH / totalLines;
            // Iterate by pixel row instead of by line to avoid O(totalLines) per frame
            let pixelRows = Math.min(mmH, totalLines);
            for (let py2 = 0; py2 < pixelRows; py2++) {
                let lineIdx = (py2 / scale) | 0;
                if (lineIdx >= totalLines) break;
                let my2 = editY + py2;
                let lineLen = Math.min(buf[lineIdx].length, MINIMAP_W);
                if (lineLen > 0) {
                    let mmCol = lineIdx === cy ? CURFG : GUTFG;
                    gfx.rectfill(mmX, my2, mmX + lineLen - 1, my2, mmCol);
                }
            }
            // Draw visible region overlay
            let vrY = (editY + oy * scale) | 0;
            let vrH = Math.max(1, (EROWS * scale) | 0);
            gfx.rectfill(mmX - 1, vrY, mmX - 1, Math.min(vrY + vrH, footY - 1), SCROLLFG);
        }
    }

    // ── Cursor ──
    if (curOn) {
        let scrX = gutterPx + (cx - ox) * CW;
        let scrY = editY + (cy - oy) * CH;
        if (cx >= ox && cx <= ox + ECOLS && cy >= oy && cy < oy + EROWS) {
            if (vimEnabled && vim !== "insert") {
                // Block cursor
                gfx.rectfill(scrX, scrY, scrX + CW - 2, scrY + CH - 2, CURFG);
                if (cx < buf[cy].length) {
                    gfx.print(buf[cy][cx], scrX, scrY, BG);
                }
            } else {
                // Line cursor
                gfx.rectfill(scrX, scrY, scrX, scrY + CH - 2, CURFG);
            }
        }
    }

    // ── Status bar ──
    gfx.rectfill(0, footY, FB_W - 1, footY + CH - 1, FOOTBG);
    if (vimEnabled && vim === "command") {
        gfx.print(":" + vimCmd + "_", gutterPx, footY, FG);
    } else {
        let modeStr = "";
        if (vimEnabled) {
            if (vim === "insert") modeStr = "-- INSERT --  ";
            else if (vim === "visual") modeStr = "-- VISUAL --  ";
            else if (vim === "vline") modeStr = "-- V-LINE --  ";
        }
        let pos = modeStr + "Ln " + (cy + 1) + ", Col " + (cx + 1) + "  (" + buf.length + " lines)";
        gfx.print(pos, gutterPx, footY, FOOTFG);
        // Pending operator / count feedback
        let right = "";
        if (vimEnabled && (vimCount || vimPending)) right = vimCount + vimPending;
        if (msg) right = msg;
        if (right) {
            let rw = gfx.textWidth(right);
            gfx.print(right, FB_W - rw - CW, footY, FOOTFG);
        }
    }
}

// ─── Selection drawing ───────────────────────────────────────────────────────

function drawSelection(lineIdx, py) {
    let s = selOrdered();
    if (!s) return;
    if (lineIdx < s.a.y || lineIdx > s.b.y) return;

    let sc, ec;
    if (lineIdx === s.a.y && lineIdx === s.b.y) {
        sc = s.a.x;
        ec = s.b.x;
    } else if (lineIdx === s.a.y) {
        sc = s.a.x;
        ec = buf[lineIdx].length;
    } else if (lineIdx === s.b.y) {
        sc = 0;
        ec = s.b.x;
    } else {
        sc = 0;
        ec = buf[lineIdx].length;
    }

    let gutterPx = GUTTER * CW;
    let x0 = gutterPx + (sc - ox) * CW;
    let x1 = gutterPx + (ec - ox) * CW - 1;
    x0 = clamp(x0, gutterPx, FB_W - 1);
    x1 = clamp(x1, gutterPx, FB_W - 1);
    if (x1 >= x0) {
        gfx.rectfill(x0, py, x1, py + CH - 1, SELBG);
    }
}
