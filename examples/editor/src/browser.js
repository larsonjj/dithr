// ─── File browser ────────────────────────────────────────────────────────────

function updateBrowser() {
    if (key.btnr(key.UP)) brIdx = Math.max(0, brIdx - 1);
    if (key.btnr(key.DOWN) && brEntries.length) brIdx = Math.min(brEntries.length - 1, brIdx + 1);
    if (brIdx < brScroll) brScroll = brIdx;
    if (brIdx >= brScroll + EROWS) brScroll = brIdx - EROWS + 1;

    // Mouse click in browser list
    let my = mouse.y();
    let editY = HEAD * CH;
    if (mouse.btnp(0) && my >= editY && brEntries.length) {
        let row = (brScroll + (my - editY) / CH) | 0;
        if (row >= 0 && row < brEntries.length) {
            brIdx = row;
        }
    }

    // Mouse wheel scroll
    let wheel = mouse.wheel();
    if (wheel !== 0) {
        brScroll = clamp(brScroll - wheel * 3, 0, Math.max(0, brEntries.length - EROWS));
        brIdx = clamp(brIdx, brScroll, brScroll + EROWS - 1);
    }

    if (key.btnp(key.ENTER) && brEntries.length) {
        let entry = brEntries[brIdx];
        if (entry.isDir) {
            if (entry.name === "..") {
                // Go up one level
                let parts = brDir.split("/").filter(function (p) {
                    return p;
                });
                parts.pop();
                brDir = parts.length ? parts.join("/") + "/" : "";
            } else {
                brDir = brDir + entry.name + "/";
            }
            refreshBrowser();
        } else {
            openFile(brDir + entry.name);
            brMode = false;
        }
    }
    if (key.btnp(key.ESCAPE)) brMode = false;
}

function drawBrowser() {
    // Header
    gfx.rectfill(0, 0, FB_W - 1, CH - 1, HEADBG);
    let hdr = "Open: /" + brDir + "  Up/Down select  Enter open  Esc cancel";
    gfx.print(hdr, CW, 0, HEADFG);

    let editY = HEAD * CH;
    for (let i = 0; i < EROWS && brScroll + i < brEntries.length; i++) {
        let fi = brScroll + i;
        let entry = brEntries[fi];
        let py = editY + i * CH;
        if (fi === brIdx) {
            gfx.rectfill(0, py, FB_W - 1, py + CH - 1, SELBG);
        }
        let label = entry.isDir ? entry.name + "/" : entry.name;
        gfx.print(label, CW * 2, py, fi === brIdx ? FG : GUTFG);
    }

    if (!brEntries.length) {
        gfx.print("Empty directory", CW * 2, editY, GUTFG);
    }
}
