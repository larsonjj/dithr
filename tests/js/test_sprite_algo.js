// ─── Sprite algorithm tests ──────────────────────────────────────────────────
//
// Self-contained: pure rasterisation algorithms inlined from sprite_editor.js.
// Tests midpointCircle, midpointCircleFill, plotRect geometry.

var __failures = 0;
var __tests = 0;

function assert(cond, msg) {
    __tests++;
    if (!cond) {
        __failures++;
        throw new Error("ASSERT: " + (msg || "assertion failed"));
    }
}

function assertEq(a, b, msg) {
    __tests++;
    if (a !== b) {
        __failures++;
        throw new Error(
            "ASSERT_EQ: " +
                JSON.stringify(a) +
                " !== " +
                JSON.stringify(b) +
                (msg ? " — " + msg : ""),
        );
    }
}

// ─── Functions under test (inlined from sprite_editor.js) ────────────────────

function midpointCircle(cx, cy, r, plot) {
    if (r <= 0) {
        plot(cx, cy);
        return;
    }
    var x = r,
        y = 0,
        d = 1 - r;
    while (x >= y) {
        plot(cx + x, cy + y);
        plot(cx - x, cy + y);
        plot(cx + x, cy - y);
        plot(cx - x, cy - y);
        plot(cx + y, cy + x);
        plot(cx - y, cy + x);
        plot(cx + y, cy - x);
        plot(cx - y, cy - x);
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

function midpointCircleFill(cx, cy, r, plot) {
    if (r <= 0) {
        plot(cx, cy);
        return;
    }
    var x = r,
        y = 0,
        d = 1 - r;
    function hline(x0, x1, yy) {
        for (var i = x0; i <= x1; i++) plot(i, yy);
    }
    while (x >= y) {
        hline(cx - x, cx + x, cy + y);
        hline(cx - x, cx + x, cy - y);
        hline(cx - y, cx + y, cy + x);
        hline(cx - y, cx + y, cy - x);
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

// Helper: collect plotted pixels as a Set of "x,y" strings
function collectPixels(fn) {
    var pixels = {};
    fn(function (x, y) {
        pixels[x + "," + y] = true;
    });
    return pixels;
}

function pixelCount(pixels) {
    return Object.keys(pixels).length;
}

function hasPixel(pixels, x, y) {
    return pixels[x + "," + y] === true;
}

// ─── midpointCircle tests ────────────────────────────────────────────────────

// Radius 0: single pixel
(function () {
    var px = collectPixels(function (plot) {
        midpointCircle(5, 5, 0, plot);
    });
    assertEq(pixelCount(px), 1, "r=0 single pixel");
    assert(hasPixel(px, 5, 5), "r=0 center");
})();

// Radius 1: should produce a small ring (no duplicate pixels ideally)
(function () {
    var px = collectPixels(function (plot) {
        midpointCircle(5, 5, 1, plot);
    });
    // Should include cardinal and diagonal points
    assert(hasPixel(px, 6, 5), "r=1 east");
    assert(hasPixel(px, 4, 5), "r=1 west");
    assert(hasPixel(px, 5, 6), "r=1 south");
    assert(hasPixel(px, 5, 4), "r=1 north");
})();

// Circle is symmetric: all 8 octants reflected
(function () {
    var px = collectPixels(function (plot) {
        midpointCircle(10, 10, 5, plot);
    });
    // Check symmetry: if (10+dx, 10+dy) exists, so should all reflections
    var keys = Object.keys(px);
    for (var i = 0; i < keys.length; i++) {
        var parts = keys[i].split(",");
        var dx = parseInt(parts[0]) - 10;
        var dy = parseInt(parts[1]) - 10;
        assert(hasPixel(px, 10 + dx, 10 + dy), "sym check +" + dx + ",+" + dy);
        assert(hasPixel(px, 10 - dx, 10 + dy), "sym check -" + dx + ",+" + dy);
        assert(hasPixel(px, 10 + dx, 10 - dy), "sym check +" + dx + ",-" + dy);
        assert(hasPixel(px, 10 - dx, 10 - dy), "sym check -" + dx + ",-" + dy);
    }
})();

// ─── midpointCircleFill tests ────────────────────────────────────────────────

// Radius 0: single pixel
(function () {
    var px = collectPixels(function (plot) {
        midpointCircleFill(5, 5, 0, plot);
    });
    assertEq(pixelCount(px), 1, "fill r=0 single pixel");
    assert(hasPixel(px, 5, 5), "fill r=0 center");
})();

// Filled circle should contain all outline pixels
(function () {
    var outline = collectPixels(function (plot) {
        midpointCircle(10, 10, 5, plot);
    });
    var filled = collectPixels(function (plot) {
        midpointCircleFill(10, 10, 5, plot);
    });
    var outlineKeys = Object.keys(outline);
    for (var i = 0; i < outlineKeys.length; i++) {
        assert(filled[outlineKeys[i]], "filled contains outline pixel " + outlineKeys[i]);
    }
})();

// Filled circle should have MORE pixels than outline (for r >= 2)
(function () {
    var outline = collectPixels(function (plot) {
        midpointCircle(10, 10, 4, plot);
    });
    var filled = collectPixels(function (plot) {
        midpointCircleFill(10, 10, 4, plot);
    });
    assert(pixelCount(filled) > pixelCount(outline), "filled has more pixels than outline for r=4");
})();

// Filled circle center is included
(function () {
    var px = collectPixels(function (plot) {
        midpointCircleFill(10, 10, 3, plot);
    });
    assert(hasPixel(px, 10, 10), "fill center pixel exists");
})();

// Filled circle has no holes — check all points within bounding box
// that are within manhattan-like distance
(function () {
    var r = 5;
    var cx = 10,
        cy = 10;
    var px = collectPixels(function (plot) {
        midpointCircleFill(cx, cy, r, plot);
    });
    // Every pixel on horizontal scanline between leftmost and rightmost plotted
    // should be filled (i.e., no horizontal gaps)
    for (var y = cy - r; y <= cy + r; y++) {
        var minX = null,
            maxX = null;
        for (var x = cx - r - 1; x <= cx + r + 1; x++) {
            if (hasPixel(px, x, y)) {
                if (minX === null) minX = x;
                maxX = x;
            }
        }
        if (minX !== null) {
            for (var x2 = minX; x2 <= maxX; x2++) {
                assert(hasPixel(px, x2, y), "no horizontal gap at (" + x2 + "," + y + ")");
            }
        }
    }
})();

// ─── plotRect logic tests (outline vs filled) ────────────────────────────────

function plotRectLogic(x0, y0, x1, y1, filled) {
    var pixels = {};
    // Normalize
    if (x0 > x1) {
        var t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        var t2 = y0;
        y0 = y1;
        y1 = t2;
    }
    if (filled) {
        for (var yy = y0; yy <= y1; yy++)
            for (var xx = x0; xx <= x1; xx++) pixels[xx + "," + yy] = true;
    } else {
        for (var xx2 = x0; xx2 <= x1; xx2++) {
            pixels[xx2 + "," + y0] = true;
            pixels[xx2 + "," + y1] = true;
        }
        for (var yy2 = y0; yy2 <= y1; yy2++) {
            pixels[x0 + "," + yy2] = true;
            pixels[x1 + "," + yy2] = true;
        }
    }
    return pixels;
}

// Outline rect
(function () {
    var px = plotRectLogic(2, 2, 5, 5, false);
    // Should have perimeter pixels
    assert(hasPixel(px, 2, 2), "outline top-left");
    assert(hasPixel(px, 5, 5), "outline bottom-right");
    assert(hasPixel(px, 3, 2), "outline top edge");
    assert(hasPixel(px, 2, 4), "outline left edge");
    // Interior should be empty
    assert(!hasPixel(px, 3, 3), "outline interior empty");
    assert(!hasPixel(px, 4, 4), "outline interior empty 2");
    // Perimeter count: 4 sides of 4, minus 4 corners counted twice = 12
    assertEq(pixelCount(px), 12, "outline pixel count");
})();

// Filled rect
(function () {
    var px = plotRectLogic(2, 2, 5, 5, true);
    // All interior filled
    assert(hasPixel(px, 3, 3), "filled interior");
    assert(hasPixel(px, 4, 4), "filled interior 2");
    // Total: 4x4 = 16
    assertEq(pixelCount(px), 16, "filled pixel count");
})();

// 1x1 rect
(function () {
    var px = plotRectLogic(3, 3, 3, 3, false);
    assertEq(pixelCount(px), 1, "1x1 outline");
    assert(hasPixel(px, 3, 3), "1x1 pixel");
})();

// Inverted coordinates (x0 > x1)
(function () {
    var px = plotRectLogic(5, 5, 2, 2, true);
    assertEq(pixelCount(px), 16, "inverted coords same result");
    assert(hasPixel(px, 3, 3), "inverted interior");
})();

// ─── Pen dedup tests ─────────────────────────────────────────────────────────
// Mirrors the dedup logic in sprite_editor.js: given a pending stroke (array of
// {x,y,prev,next} ops recorded in draw order), dedup keeps the *last* record
// per pixel, replaces its prev with the *earliest* original prev for that pixel,
// and removes no-ops where prev === next.

function dedupPending(pending) {
    var seen = {};
    var deduped = [];
    for (var i = pending.length - 1; i >= 0; i--) {
        var op = pending[i];
        var k = op.x + "," + op.y;
        if (seen[k] === undefined) {
            seen[k] = deduped.length;
            deduped.push({ x: op.x, y: op.y, prev: op.prev, next: op.next });
        } else {
            deduped[seen[k]].prev = op.prev;
        }
    }
    deduped.reverse();
    var result = [];
    for (var j = 0; j < deduped.length; j++) {
        if (deduped[j].prev !== deduped[j].next) result.push(deduped[j]);
    }
    return result;
}

// Basic: single pixel touched once → kept as-is
(function () {
    var ops = [{ x: 1, y: 2, prev: 0, next: 5 }];
    var res = dedupPending(ops);
    assertEq(res.length, 1, "dedup single op kept");
    assertEq(res[0].prev, 0, "dedup single prev");
    assertEq(res[0].next, 5, "dedup single next");
})();

// Same pixel overwritten twice → one record, prev from first, next from last
(function () {
    var ops = [
        { x: 3, y: 3, prev: 0, next: 5 },
        { x: 3, y: 3, prev: 5, next: 7 },
    ];
    var res = dedupPending(ops);
    assertEq(res.length, 1, "dedup two same pixel → one");
    assertEq(res[0].prev, 0, "dedup keeps original prev");
    assertEq(res[0].next, 7, "dedup keeps final next");
})();

// No-op removal: pixel returns to original color
(function () {
    var ops = [
        { x: 1, y: 1, prev: 3, next: 5 },
        { x: 1, y: 1, prev: 5, next: 3 },
    ];
    var res = dedupPending(ops);
    assertEq(res.length, 0, "dedup removes no-op (prev===next after merge)");
})();

// Symmetry: two different pixels in same stroke (e.g., mirror X)
(function () {
    var ops = [
        { x: 1, y: 0, prev: 0, next: 5 },
        { x: 6, y: 0, prev: 0, next: 5 },
    ];
    var res = dedupPending(ops);
    assertEq(res.length, 2, "dedup keeps both mirror pixels");
    assertEq(res[0].x, 1, "dedup order preserved: first pixel");
    assertEq(res[1].x, 6, "dedup order preserved: mirror pixel");
})();

// Symmetry bug regression: with mirror, pixel A is recorded, then pixel B
// (mirror), then pixel A again. The prev update must target A's entry, not B's.
(function () {
    var ops = [
        { x: 1, y: 0, prev: 0, next: 5 }, // pen on pixel A
        { x: 6, y: 0, prev: 0, next: 5 }, // mirror of A
        { x: 1, y: 0, prev: 5, next: 7 }, // pen revisits A (new color)
        { x: 6, y: 0, prev: 5, next: 7 }, // mirror revisits
    ];
    var res = dedupPending(ops);
    assertEq(res.length, 2, "sym dedup → 2 pixels");
    // Pixel A (x=1): first prev was 0, last next is 7
    var a = null,
        b = null;
    for (var i = 0; i < res.length; i++) {
        if (res[i].x === 1) a = res[i];
        if (res[i].x === 6) b = res[i];
    }
    assert(a !== null, "pixel A found");
    assert(b !== null, "pixel B found");
    assertEq(a.prev, 0, "sym dedup A.prev = original (0)");
    assertEq(a.next, 7, "sym dedup A.next = final (7)");
    assertEq(b.prev, 0, "sym dedup B.prev = original (0)");
    assertEq(b.next, 7, "sym dedup B.next = final (7)");
})();

// Three overwrites of same pixel
(function () {
    var ops = [
        { x: 2, y: 2, prev: 0, next: 1 },
        { x: 2, y: 2, prev: 1, next: 2 },
        { x: 2, y: 2, prev: 2, next: 3 },
    ];
    var res = dedupPending(ops);
    assertEq(res.length, 1, "triple overwrite → 1");
    assertEq(res[0].prev, 0, "triple keeps earliest prev");
    assertEq(res[0].next, 3, "triple keeps latest next");
})();

// Mixed: multiple pixels, some deduped, some not
(function () {
    var ops = [
        { x: 0, y: 0, prev: 0, next: 1 },
        { x: 1, y: 0, prev: 0, next: 2 },
        { x: 0, y: 0, prev: 1, next: 3 },
        { x: 2, y: 0, prev: 0, next: 4 },
    ];
    var res = dedupPending(ops);
    assertEq(res.length, 3, "mixed: 3 distinct pixels");
    // Find each pixel by coordinate (order depends on last-occurrence position)
    var p0 = null,
        p1 = null,
        p2 = null;
    for (var i = 0; i < res.length; i++) {
        if (res[i].x === 0) p0 = res[i];
        if (res[i].x === 1) p1 = res[i];
        if (res[i].x === 2) p2 = res[i];
    }
    assert(p0 !== null, "mixed: pixel (0,0) found");
    assert(p1 !== null, "mixed: pixel (1,0) found");
    assert(p2 !== null, "mixed: pixel (2,0) found");
    assertEq(p0.prev, 0, "mixed (0,0) prev");
    assertEq(p0.next, 3, "mixed (0,0) next");
    assertEq(p1.prev, 0, "mixed (1,0) prev");
    assertEq(p1.next, 2, "mixed (1,0) next");
    assertEq(p2.prev, 0, "mixed (2,0) prev");
    assertEq(p2.next, 4, "mixed (2,0) next");
})();
