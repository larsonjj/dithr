// ─── File browser ────────────────────────────────────────────────────────────

import { st } from './state.js';
import {
    FB_W,
    CW,
    CH,
    LINE_H,
    LINE_PAD,
    EDIT_Y,
    EROWS,
    FG,
    GUTFG,
    SELBG,
    HEADBG,
    HEADFG,
} from './config.js';
import { clamp } from './helpers.js';
import { openFile, refreshBrowser } from './buffer.js';

export function updateBrowser() {
    if (key.btnr(key.UP)) st.brIdx = Math.max(0, st.brIdx - 1);
    if (key.btnr(key.DOWN) && st.brEntries.length)
        st.brIdx = Math.min(st.brEntries.length - 1, st.brIdx + 1);
    if (st.brIdx < st.brScroll) st.brScroll = st.brIdx;
    if (st.brIdx >= st.brScroll + EROWS) st.brScroll = st.brIdx - EROWS + 1;

    // Mouse click in browser list
    const my = mouse.y();
    const editY = EDIT_Y;
    if (mouse.btnp(0) && my >= editY && st.brEntries.length) {
        const row = (st.brScroll + (my - editY) / LINE_H) | 0;
        if (row >= 0 && row < st.brEntries.length) {
            st.brIdx = row;
        }
    }

    // Mouse wheel scroll
    const wheel = mouse.wheel();
    if (wheel !== 0) {
        st.brScroll = clamp(st.brScroll - wheel * 3, 0, Math.max(0, st.brEntries.length - EROWS));
        st.brIdx = clamp(st.brIdx, st.brScroll, st.brScroll + EROWS - 1);
    }

    if (key.btnp(key.ENTER) && st.brEntries.length) {
        const entry = st.brEntries[st.brIdx];
        if (entry.isDir) {
            if (entry.name === '..') {
                // Go up one level
                const parts = st.brDir.split('/').filter((p) => p);
                parts.pop();
                st.brDir = parts.length ? `${parts.join('/')}/` : '';
            } else {
                st.brDir = `${st.brDir + entry.name}/`;
            }
            refreshBrowser();
        } else {
            openFile(st.brDir + entry.name);
            st.brMode = false;
        }
    }
    if (key.btnp(key.ESCAPE)) st.brMode = false;
}

export function drawBrowser() {
    // Header
    gfx.rectfill(0, 0, FB_W - 1, CH - 1, HEADBG);
    const hdr = `Open: /${st.brDir}  Up/Down select  Enter open  Esc cancel`;
    gfx.print(hdr, CW, 0, HEADFG);

    const editY = EDIT_Y;
    for (let i = 0; i < EROWS && st.brScroll + i < st.brEntries.length; i++) {
        const fi = st.brScroll + i;
        const entry = st.brEntries[fi];
        const py = editY + i * LINE_H;
        if (fi === st.brIdx) {
            gfx.rectfill(0, py, FB_W - 1, py + LINE_H - 1, SELBG);
        }
        const label = entry.isDir ? `${entry.name}/` : entry.name;
        gfx.print(label, CW * 2, py + LINE_PAD, fi === st.brIdx ? FG : GUTFG);
    }

    if (!st.brEntries.length) {
        gfx.print('Empty directory', CW * 2, editY + LINE_PAD, GUTFG);
    }
}
