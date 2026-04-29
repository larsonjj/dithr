// tests/node/test_resolve_assets.js — Node-side tests for tools/resolve-assets.js
//
// Run with: node --test tests/node/test_resolve_assets.js
//
// Uses the built-in node:test runner (Node ≥ 18). Zero external dependencies.

"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const { resolveAssets, globToRegExp, matchesAny } = require("../../tools/resolve-assets.js");

/* ───── Glob-matching unit tests ────────────────────────────────────────── */

test("globToRegExp: literal match", () => {
    const re = globToRegExp("foo.json");
    assert.ok(re.test("foo.json"));
    assert.ok(!re.test("foox.json"));
    assert.ok(!re.test("dir/foo.json"));
});

test("globToRegExp: single * does not cross /", () => {
    const re = globToRegExp("*.json");
    assert.ok(re.test("foo.json"));
    assert.ok(!re.test("dir/foo.json"));
});

test("globToRegExp: ** matches any depth", () => {
    const re = globToRegExp("**/*.psd");
    assert.ok(re.test("foo.psd"));
    assert.ok(re.test("a/b/foo.psd"));
    assert.ok(re.test("deeply/nested/path/file.psd"));
    assert.ok(!re.test("foo.png"));
});

test("globToRegExp: prefix glob src/**", () => {
    const re = globToRegExp("src/**");
    assert.ok(re.test("src/main.ts"));
    assert.ok(re.test("src/sub/helper.ts"));
    assert.ok(!re.test("dist/main.js"));
});

test("globToRegExp: mid-path glob with literal segments", () => {
    const re = globToRegExp("assets/maps/dlc-*.tmj");
    assert.ok(re.test("assets/maps/dlc-forest.tmj"));
    assert.ok(re.test("assets/maps/dlc-cave.tmj"));
    assert.ok(!re.test("assets/maps/world.tmj"));
    assert.ok(!re.test("assets/maps/sub/dlc-x.tmj"));
});

test("globToRegExp: ? matches single non-slash char", () => {
    const re = globToRegExp("file?.txt");
    assert.ok(re.test("file1.txt"));
    assert.ok(re.test("fileA.txt"));
    assert.ok(!re.test("file.txt"));
    assert.ok(!re.test("file12.txt"));
});

test("globToRegExp: regex special chars are escaped", () => {
    const re = globToRegExp("a.b+c.json");
    assert.ok(re.test("a.b+c.json"));
    assert.ok(!re.test("aXbXcXjson"));
});

test("matchesAny: returns true on first match", () => {
    assert.ok(matchesAny("foo.psd", ["**/*.png", "**/*.psd"]));
    assert.ok(!matchesAny("foo.png", ["**/*.psd"]));
});

/* ───── Resolver integration tests via temp fixtures ────────────────────── */

function makeFixture(layout) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "dithr-resolve-"));
    for (const [rel, contents] of Object.entries(layout)) {
        const abs = path.join(dir, rel);
        fs.mkdirSync(path.dirname(abs), { recursive: true });
        fs.writeFileSync(abs, contents);
    }
    return dir;
}

function cleanup(dir) {
    fs.rmSync(dir, { recursive: true, force: true });
}

test("resolveAssets: defaults bundle everything except always-excluded", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({ title: "test" }),
        "src/main.js": "// code",
        "assets/sprites/sheet.png": "PNG",
        "node_modules/pkg/index.js": "// dep",
        ".git/HEAD": "ref: refs/heads/main",
    });
    try {
        const r = resolveAssets(dir);
        assert.deepEqual(
            r.bundled.sort(),
            ["assets/sprites/sheet.png", "cart.json", "src/main.js"].sort(),
        );
        assert.deepEqual(r.dynamic, []);
        assert.ok(r.excluded.includes("node_modules/pkg/index.js"));
        assert.ok(r.excluded.includes(".git/HEAD"));
        assert.equal(r.compress, false);
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: explicit exclude removes files", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({
            assets: { exclude: ["**/*.psd", "src/**"] },
        }),
        "src/main.ts": "// ts",
        "dist/main.js": "// js",
        "assets/sprites/sheet.png": "PNG",
        "assets/sprites/sheet.psd": "PSD",
    });
    try {
        const r = resolveAssets(dir);
        assert.deepEqual(
            r.bundled.sort(),
            ["assets/sprites/sheet.png", "cart.json", "dist/main.js"].sort(),
        );
        assert.ok(r.excluded.includes("src/main.ts"));
        assert.ok(r.excluded.includes("assets/sprites/sheet.psd"));
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: dynamic separates from bundled", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({
            assets: { dynamic: ["maps/dlc-*.tmj"] },
        }),
        "maps/world.tmj": "{}",
        "maps/dlc-forest.tmj": "{}",
        "maps/dlc-cave.tmj": "{}",
    });
    try {
        const r = resolveAssets(dir);
        assert.deepEqual(r.bundled.sort(), ["cart.json", "maps/world.tmj"].sort());
        assert.deepEqual(r.dynamic.sort(), ["maps/dlc-cave.tmj", "maps/dlc-forest.tmj"].sort());
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: exclude wins over dynamic", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({
            assets: {
                exclude: ["**/*.psd"],
                dynamic: ["**/*.psd", "maps/*.tmj"],
            },
        }),
        "art/concept.psd": "PSD",
        "maps/level1.tmj": "{}",
    });
    try {
        const r = resolveAssets(dir);
        assert.ok(r.excluded.includes("art/concept.psd"));
        assert.ok(!r.dynamic.includes("art/concept.psd"));
        assert.ok(r.dynamic.includes("maps/level1.tmj"));
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: include narrowing", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({
            assets: { include: ["assets/**", "dist/**"] },
        }),
        "src/main.ts": "// ts",
        "dist/main.js": "// js",
        "assets/sprites/sheet.png": "PNG",
    });
    try {
        const r = resolveAssets(dir);
        // src/ is not in include → excluded
        assert.ok(r.excluded.includes("src/main.ts"));
        // dist/ and assets/ are included
        assert.ok(r.bundled.includes("dist/main.js"));
        assert.ok(r.bundled.includes("assets/sprites/sheet.png"));
        // cart.json is always bundled
        assert.ok(r.bundled.includes("cart.json"));
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: cart.json always bundled even if matched by exclude", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({
            assets: { exclude: ["**/*.json"] },
        }),
        "data/levels.json": "{}",
    });
    try {
        const r = resolveAssets(dir);
        assert.ok(r.bundled.includes("cart.json"));
        assert.ok(r.excluded.includes("data/levels.json"));
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: compress accepts false, 'br', 'gzip'", () => {
    for (const v of [false, "br", "gzip"]) {
        const dir = makeFixture({
            "cart.json": JSON.stringify({ assets: { compress: v } }),
        });
        try {
            const r = resolveAssets(dir);
            assert.equal(r.compress, v);
        } finally {
            cleanup(dir);
        }
    }
});

test("resolveAssets: compress rejects invalid values", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({ assets: { compress: "lz4" } }),
    });
    try {
        assert.throws(() => resolveAssets(dir), /assets\.compress/);
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: missing cart.json throws", () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "dithr-resolve-empty-"));
    try {
        assert.throws(() => resolveAssets(dir), /cart\.json not found/);
    } finally {
        cleanup(dir);
    }
});

test("resolveAssets: paths are POSIX even on Windows", () => {
    const dir = makeFixture({
        "cart.json": JSON.stringify({}),
        "a/b/c/file.txt": "x",
    });
    try {
        const r = resolveAssets(dir);
        assert.ok(r.bundled.includes("a/b/c/file.txt"));
        for (const f of r.bundled) {
            assert.ok(!f.includes("\\"), `path should not contain backslashes: ${f}`);
        }
    } finally {
        cleanup(dir);
    }
});
