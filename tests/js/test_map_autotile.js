// ─── Map editor auto-tile — pure function tests ─────────────────────────────
//
// Self-contained: auto-tile helpers inlined from map_editor.js.

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

// ─── Inlined tile conversion helpers ─────────────────────────────────────────

function sprToTile(sprIdx) {
    return sprIdx + 1;
}

function tileToSpr(tileId) {
    return tileId > 0 ? tileId - 1 : -1;
}

// ─── Inlined auto-tile helpers ───────────────────────────────────────────────

// Simulated map and state for testing
var testMapData = {};
var testMapW = 10;
var testMapH = 10;
var testAutoGroups = [];

function testMapGet(tx, ty) {
    var key = tx + "," + ty;
    return testMapData[key] || 0;
}

function testMapSet(tx, ty, val) {
    testMapData[tx + "," + ty] = val;
}

function getAutoGroup(sprIdx) {
    for (var i = 0; i < testAutoGroups.length; i++) {
        var base = testAutoGroups[i];
        if (sprIdx >= base && sprIdx < base + 16) return base;
    }
    return -1;
}

function autoTileMask(tx, ty, base) {
    var mask = 0;
    // N
    if (ty > 0) {
        var t = tileToSpr(testMapGet(tx, ty - 1));
        if (t >= base && t < base + 16) mask |= 1;
    }
    // E
    if (tx < testMapW - 1) {
        var t2 = tileToSpr(testMapGet(tx + 1, ty));
        if (t2 >= base && t2 < base + 16) mask |= 2;
    }
    // S
    if (ty < testMapH - 1) {
        var t3 = tileToSpr(testMapGet(tx, ty + 1));
        if (t3 >= base && t3 < base + 16) mask |= 4;
    }
    // W
    if (tx > 0) {
        var t4 = tileToSpr(testMapGet(tx - 1, ty));
        if (t4 >= base && t4 < base + 16) mask |= 8;
    }
    return mask;
}

// ─── Setup helper ────────────────────────────────────────────────────────────

function resetMap() {
    testMapData = {};
    testAutoGroups = [];
}

// ═════════════════════════════════════════════════════════════════════════════
//  sprToTile / tileToSpr
// ═════════════════════════════════════════════════════════════════════════════

assertEq(sprToTile(0), 1, "spr 0 → tile 1");
assertEq(sprToTile(15), 16, "spr 15 → tile 16");
assertEq(tileToSpr(1), 0, "tile 1 → spr 0");
assertEq(tileToSpr(0), -1, "tile 0 (empty) → spr -1");
assertEq(tileToSpr(-1), -1, "tile -1 → spr -1");
assertEq(tileToSpr(sprToTile(42)), 42, "round-trip");

// ═════════════════════════════════════════════════════════════════════════════
//  getAutoGroup
// ═════════════════════════════════════════════════════════════════════════════

// No groups defined
(function test_no_groups() {
    resetMap();
    assertEq(getAutoGroup(5), -1, "no groups → -1");
})();

// Single group at base 0
(function test_single_group_base0() {
    resetMap();
    testAutoGroups = [0];
    assertEq(getAutoGroup(0), 0, "spr 0 in group 0");
    assertEq(getAutoGroup(15), 0, "spr 15 in group 0");
    assertEq(getAutoGroup(16), -1, "spr 16 not in group 0");
})();

// Multiple groups
(function test_multiple_groups() {
    resetMap();
    testAutoGroups = [0, 32, 64];
    assertEq(getAutoGroup(10), 0, "spr 10 in group 0");
    assertEq(getAutoGroup(32), 32, "spr 32 in group 32");
    assertEq(getAutoGroup(47), 32, "spr 47 in group 32");
    assertEq(getAutoGroup(64), 64, "spr 64 in group 64");
    assertEq(getAutoGroup(80), -1, "spr 80 not in any group");
})();

// ═════════════════════════════════════════════════════════════════════════════
//  autoTileMask — NESW neighbor detection
// ═════════════════════════════════════════════════════════════════════════════

// Isolated tile (no neighbors) → mask 0
(function test_isolated() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    assertEq(autoTileMask(5, 5, 0), 0, "isolated tile, no neighbors");
})();

// North neighbor only → mask 1
(function test_north() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(5, 4, sprToTile(3)); // north, same group
    assertEq(autoTileMask(5, 5, 0), 1, "north neighbor → 1");
})();

// East neighbor only → mask 2
(function test_east() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(6, 5, sprToTile(7)); // east, same group
    assertEq(autoTileMask(5, 5, 0), 2, "east neighbor → 2");
})();

// South neighbor only → mask 4
(function test_south() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(5, 6, sprToTile(1)); // south, same group
    assertEq(autoTileMask(5, 5, 0), 4, "south neighbor → 4");
})();

// West neighbor only → mask 8
(function test_west() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(4, 5, sprToTile(2)); // west, same group
    assertEq(autoTileMask(5, 5, 0), 8, "west neighbor → 8");
})();

// All four neighbors → mask 15
(function test_all_neighbors() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(5, 4, sprToTile(1)); // N
    testMapSet(6, 5, sprToTile(2)); // E
    testMapSet(5, 6, sprToTile(3)); // S
    testMapSet(4, 5, sprToTile(4)); // W
    assertEq(autoTileMask(5, 5, 0), 15, "all neighbors → 15");
})();

// Neighbor from different group → not counted
(function test_different_group() {
    resetMap();
    testAutoGroups = [0, 32];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(5, 4, sprToTile(32)); // N, different group
    testMapSet(6, 5, sprToTile(3)); // E, same group
    assertEq(autoTileMask(5, 5, 0), 2, "only same-group counted");
})();

// Edge of map — neighbors outside bounds not counted
(function test_map_edge() {
    resetMap();
    testAutoGroups = [0];
    // Top-left corner
    testMapSet(0, 0, sprToTile(0));
    testMapSet(1, 0, sprToTile(1)); // E
    testMapSet(0, 1, sprToTile(2)); // S
    // N and W are out of bounds
    assertEq(autoTileMask(0, 0, 0), 6, "corner: E+S = 2+4 = 6");
})();

// Bottom-right corner
(function test_bottom_right() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(9, 9, sprToTile(0));
    testMapSet(9, 8, sprToTile(1)); // N
    testMapSet(8, 9, sprToTile(2)); // W
    assertEq(autoTileMask(9, 9, 0), 9, "bottom-right: N+W = 1+8 = 9");
})();

// N+S (vertical corridor) → mask 5
(function test_vertical_corridor() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(5, 4, sprToTile(1)); // N
    testMapSet(5, 6, sprToTile(2)); // S
    assertEq(autoTileMask(5, 5, 0), 5, "N+S = 5");
})();

// E+W (horizontal corridor) → mask 10
(function test_horizontal_corridor() {
    resetMap();
    testAutoGroups = [0];
    testMapSet(5, 5, sprToTile(0));
    testMapSet(6, 5, sprToTile(1)); // E
    testMapSet(4, 5, sprToTile(2)); // W
    assertEq(autoTileMask(5, 5, 0), 10, "E+W = 10");
})();
