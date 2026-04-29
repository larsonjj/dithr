/**
 * \file            test_api_res.c
 * \brief           Phase 3.1 unit tests for the res.* JS namespace.
 *
 * Drives the bindings end-to-end via JS_Eval, exercising load → pump →
 * promise resolution, sync inspectors, handle hoisting, list filtering,
 * and the active-flags / active-palette / active-sheet selectors.
 */

#include "api/api_common.h"
#include "audio.h"
#include "cart.h"
#include "console_defs.h"
#include "graphics.h"
#include "resources.h"
#include "runtime.h"
#include "test_harness.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct test_ctx {
    dtr_console_t  con;
    dtr_runtime_t *rt;
    char           tmpdir[512];
} test_ctx_t;

static void prv_make_tmpdir(test_ctx_t *tc)
{
    /* Write scratch beside the test exe (under build/) so the repo source
       tree never accumulates stray files when tests are run from any cwd. */
    const char *base = SDL_GetBasePath();
    SDL_snprintf(tc->tmpdir, sizeof(tc->tmpdir), "%stest_api_res_tmp", base != NULL ? base : "");
    SDL_CreateDirectory(tc->tmpdir);
}

static void prv_write_file(const char *dir, const char *rel, const void *data, size_t len)
{
    char          path[1024];
    SDL_IOStream *io;

    SDL_snprintf(path, sizeof(path), "%s/%s", dir, rel);
    io = SDL_IOFromFile(path, "wb");
    DTR_ASSERT_NOT_NULL(io);
    SDL_WriteIO(io, data, len);
    SDL_CloseIO(io);
}

static test_ctx_t *prv_setup(void)
{
    test_ctx_t *tc;

    tc = DTR_CALLOC(1, sizeof(test_ctx_t));
    DTR_ASSERT_NOT_NULL(tc);

    tc->con.fb_width  = 32;
    tc->con.fb_height = 32;

    tc->con.cart     = dtr_cart_create();
    tc->con.graphics = dtr_gfx_create(tc->con.fb_width, tc->con.fb_height);
    DTR_ASSERT_NOT_NULL(tc->con.graphics);

    tc->rt = dtr_runtime_create(&tc->con, 8, 2048);
    DTR_ASSERT_NOT_NULL(tc->rt);
    tc->con.runtime = tc->rt;

    tc->con.res = dtr_res_create();
    DTR_ASSERT_NOT_NULL(tc->con.res);

    prv_make_tmpdir(tc);
    SDL_strlcpy(tc->con.cart->base_path, tc->tmpdir, sizeof(tc->con.cart->base_path));

    {
        JSValue global = JS_GetGlobalObject(tc->rt->ctx);
        dtr_res_api_register(tc->rt->ctx, global);
        dtr_sfx_api_register(tc->rt->ctx, global);
        dtr_mus_api_register(tc->rt->ctx, global);
        dtr_map_api_register(tc->rt->ctx, global);
        JS_FreeValue(tc->rt->ctx, global);
    }
    return tc;
}

static void prv_teardown(test_ctx_t *tc)
{
    dtr_runtime_drain_jobs(tc->rt);
    dtr_res_destroy(&tc->con, tc->con.res);
    dtr_runtime_destroy(tc->rt);
    dtr_gfx_destroy(tc->con.graphics);
    dtr_cart_destroy(tc->con.cart);
    DTR_FREE(tc);
}

static void prv_dump_exception(test_ctx_t *tc, const char *code)
{
    JSValue     exc = JS_GetException(tc->rt->ctx);
    const char *msg = JS_ToCString(tc->rt->ctx, exc);
    fprintf(stderr, "  EXC in <%s>: %s\n", code, msg ? msg : "(unknown)");
    if (msg) {
        JS_FreeCString(tc->rt->ctx, msg);
    }
    JS_FreeValue(tc->rt->ctx, exc);
}

static JSValue prv_eval(test_ctx_t *tc, const char *code)
{
    JSValue v = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        prv_dump_exception(tc, code);
    }
    return v;
}

static bool prv_eval_bool(test_ctx_t *tc, const char *code)
{
    JSValue v;
    bool    b;
    v = prv_eval(tc, code);
    DTR_ASSERT(!JS_IsException(v));
    b = JS_ToBool(tc->rt->ctx, v) != 0;
    JS_FreeValue(tc->rt->ctx, v);
    return b;
}

static int32_t prv_eval_i32(test_ctx_t *tc, const char *code)
{
    JSValue v;
    int32_t i;
    v = prv_eval(tc, code);
    DTR_ASSERT(!JS_IsException(v));
    JS_ToInt32(tc->rt->ctx, &i, v);
    JS_FreeValue(tc->rt->ctx, v);
    return i;
}

/* ------------------------------------------------------------------ */
/*  Tests                                                              */
/* ------------------------------------------------------------------ */

static void test_namespace_registered(void)
{
    test_ctx_t *tc = prv_setup();
    DTR_ASSERT(prv_eval_bool(tc, "typeof res === 'object' && typeof res.loadJson === 'function'"));
    prv_teardown(tc);
    fprintf(stderr, "  PASS namespace_registered\n");
}

static void test_load_json_resolves(void)
{
    test_ctx_t *tc     = prv_setup();
    const char  json[] = "{\"a\":1,\"b\":\"hi\"}";

    prv_write_file(tc->tmpdir, "data.json", json, sizeof(json) - 1);

    /* Kick off a load and stash the resulting promise. */
    {
        JSValue v = prv_eval(tc, "globalThis.__p = res.loadJson('data', 'data.json'); 1");
        DTR_ASSERT(!JS_IsException(v));
        JS_FreeValue(tc->rt->ctx, v);
    }
    DTR_ASSERT(prv_eval_bool(tc, "res.has('data')"));
    DTR_ASSERT(!prv_eval_bool(tc, "res.isLoaded('data')"));

    dtr_res_pump(&tc->con);
    dtr_runtime_drain_jobs(tc->rt);

    DTR_ASSERT(prv_eval_bool(tc, "res.isLoaded('data')"));
    /* Promise resolved value should be the parsed object. */
    {
        JSValue v;
        v = prv_eval(tc, "globalThis.__seen = null; __p.then(v => { globalThis.__seen = v; });");
        JS_FreeValue(tc->rt->ctx, v);
        dtr_runtime_drain_jobs(tc->rt);
        v = prv_eval(tc, "__seen && __seen.a");
        DTR_ASSERT(!JS_IsException(v));
        {
            int32_t a = -1;
            JS_ToInt32(tc->rt->ctx, &a, v);
            DTR_ASSERT_EQ_INT(a, 1);
        }
        JS_FreeValue(tc->rt->ctx, v);
    }
    prv_teardown(tc);
    fprintf(stderr, "  PASS load_json_resolves\n");
}

static void test_load_raw_uint8array(void)
{
    test_ctx_t *tc        = prv_setup();
    const char  payload[] = "abc";

    prv_write_file(tc->tmpdir, "blob.bin", payload, sizeof(payload) - 1);
    {
        JSValue v = prv_eval(tc, "globalThis.__pr = res.loadRaw('blob', 'blob.bin'); 1");
        JS_FreeValue(tc->rt->ctx, v);
    }
    dtr_res_pump(&tc->con);
    dtr_runtime_drain_jobs(tc->rt);

    /* Verify promise resolved with a Uint8Array of the right contents. */
    {
        JSValue v =
            prv_eval(tc, "globalThis.__seen2 = null; __pr.then(v => { globalThis.__seen2 = v; });");
        JS_FreeValue(tc->rt->ctx, v);
    }
    dtr_runtime_drain_jobs(tc->rt);

    DTR_ASSERT(prv_eval_bool(tc, "__seen2 instanceof Uint8Array"));
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "__seen2.length"), 3);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "__seen2[0]"), (int)'a');
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "__seen2[2]"), (int)'c');
    prv_teardown(tc);
    fprintf(stderr, "  PASS load_raw_uint8array\n");
}

static void test_handle_and_unload(void)
{
    test_ctx_t *tc        = prv_setup();
    const char  payload[] = "x";

    prv_write_file(tc->tmpdir, "h.bin", payload, sizeof(payload) - 1);
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.loadRaw('h', 'h.bin')"));
    dtr_res_pump(&tc->con);

    /* handle() returns a positive integer slot. */
    DTR_ASSERT(prv_eval_i32(tc, "res.handle('h')") > 0);

    /* handle() throws ReferenceError for unknown name. */
    DTR_ASSERT(prv_eval_bool(
        tc, "(()=>{ try { res.handle('nope'); return false; } catch(e){ return true; } })()"));

    /* unload makes has() false. */
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.unload('h')"));
    DTR_ASSERT(!prv_eval_bool(tc, "res.has('h')"));
    prv_teardown(tc);
    fprintf(stderr, "  PASS handle_and_unload\n");
}

static void test_list_filter(void)
{
    test_ctx_t *tc        = prv_setup();
    const char  json[]    = "{}";
    const char  payload[] = "x";

    prv_write_file(tc->tmpdir, "a.json", json, sizeof(json) - 1);
    prv_write_file(tc->tmpdir, "b.bin", payload, sizeof(payload) - 1);
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.loadJson('a', 'a.json')"));
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.loadRaw('b', 'b.bin')"));
    dtr_res_pump(&tc->con);
    dtr_res_pump(&tc->con);

    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "res.list().length"), 2);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "res.list('json').length"), 1);
    DTR_ASSERT(prv_eval_bool(tc, "res.list('json')[0] === 'a'"));
    DTR_ASSERT(prv_eval_bool(tc, "res.list('raw')[0] === 'b'"));
    prv_teardown(tc);
    fprintf(stderr, "  PASS list_filter\n");
}

static void test_set_active_flags(void)
{
    test_ctx_t *tc = prv_setup();
    /* CONSOLE_MAX_SPRITES (256) flag bytes encoded as hex pairs (512 chars).
     * First three set to 0x01, 0xFF, 0x02; rest are 0x00. */
    char    hex[CONSOLE_MAX_SPRITES * 2 + 1];
    int32_t i;

    for (i = 0; i < CONSOLE_MAX_SPRITES * 2; ++i) {
        hex[i] = '0';
    }
    hex[CONSOLE_MAX_SPRITES * 2] = '\0';
    hex[1]                       = '1';
    hex[2]                       = 'F';
    hex[3]                       = 'F';
    hex[5]                       = '2';

    prv_write_file(tc->tmpdir, "f.hex", hex, CONSOLE_MAX_SPRITES * 2);
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.loadHex('flags', 'f.hex')"));
    dtr_res_pump(&tc->con);

    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.setActiveFlags('flags')"));
    DTR_ASSERT_EQ_INT((int)tc->con.graphics->sheet.flags[0], 0x01);
    DTR_ASSERT_EQ_INT((int)tc->con.graphics->sheet.flags[1], 0xFF);
    DTR_ASSERT_EQ_INT((int)tc->con.graphics->sheet.flags[2], 0x02);
    prv_teardown(tc);
    fprintf(stderr, "  PASS set_active_flags\n");
}

static void test_set_active_palette(void)
{
    test_ctx_t *tc = prv_setup();
    /* Two RGB triples: red, green. */
    const char hex[] = "FF0000 00FF00";

    prv_write_file(tc->tmpdir, "p.hex", hex, sizeof(hex) - 1);
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.loadHex('pal', 'p.hex')"));
    dtr_res_pump(&tc->con);
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "res.setActivePalette('pal')"));

    DTR_ASSERT_EQ_INT((int)tc->con.graphics->colors[0], (int)0xFF0000FFu);
    DTR_ASSERT_EQ_INT((int)tc->con.graphics->colors[1], (int)0x00FF00FFu);
    prv_teardown(tc);
    fprintf(stderr, "  PASS set_active_palette\n");
}

static void test_load_missing_rejects(void)
{
    test_ctx_t *tc = prv_setup();
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "globalThis.__err = null;"));
    JS_FreeValue(tc->rt->ctx,
                 prv_eval(tc, "res.loadJson('miss', 'nope.json').catch(e => { __err = e; });"));
    dtr_res_pump(&tc->con);
    dtr_runtime_drain_jobs(tc->rt);

    DTR_ASSERT(prv_eval_bool(tc, "__err !== null && typeof __err.message === 'string'"));
    DTR_ASSERT(prv_eval_bool(tc, "__err.resource === 'miss'"));
    prv_teardown(tc);
    fprintf(stderr, "  PASS load_missing_rejects\n");
}

/* ------------------------------------------------------------------ */
/*  Phase 3.2: string-or-handle dispatch in sfx/mus/map               */
/* ------------------------------------------------------------------ */

static void test_sfx_play_unknown_throws(void)
{
    test_ctx_t *tc = prv_setup();
    /* Unknown name → TypeError; resolver returns -1 before audio is touched. */
    DTR_ASSERT(prv_eval_bool(
        tc,
        "(()=>{ try { sfx.play('nope'); return false; } catch(e){ return e instanceof TypeError; "
        "} })()"));
    /* Missing arg → also throws. */
    DTR_ASSERT(prv_eval_bool(tc,
                             "(()=>{ try { sfx.play(); return false; } catch(e){ return e "
                             "instanceof TypeError; } })()"));
    /* Out-of-range numeric handle → throws. */
    DTR_ASSERT(prv_eval_bool(
        tc,
        "(()=>{ try { sfx.play(9999); return false; } catch(e){ return e instanceof TypeError; } "
        "})()"));
    prv_teardown(tc);
    fprintf(stderr, "  PASS sfx_play_unknown_throws\n");
}

static void test_mus_play_unknown_throws(void)
{
    test_ctx_t *tc = prv_setup();
    DTR_ASSERT(prv_eval_bool(
        tc,
        "(()=>{ try { mus.play('nope'); return false; } catch(e){ return e instanceof TypeError; "
        "} })()"));
    DTR_ASSERT(prv_eval_bool(tc,
                             "(()=>{ try { mus.play(); return false; } catch(e){ return e "
                             "instanceof TypeError; } })()"));
    prv_teardown(tc);
    fprintf(stderr, "  PASS mus_play_unknown_throws\n");
}

static void test_map_use_unknown_throws(void)
{
    test_ctx_t *tc = prv_setup();
    DTR_ASSERT(prv_eval_bool(
        tc,
        "(()=>{ try { map.use('nope'); return false; } catch(e){ return e instanceof TypeError; "
        "} })()"));
    DTR_ASSERT(prv_eval_bool(
        tc,
        "(()=>{ try { map.use(); return false; } catch(e){ return e instanceof TypeError; } })()"));
    prv_teardown(tc);
    fprintf(stderr, "  PASS map_use_unknown_throws\n");
}

static void test_map_use_sets_active(void)
{
    test_ctx_t           *tc   = prv_setup();
    struct dtr_map_level *fake = DTR_CALLOC(1, sizeof(*fake));
    int32_t               slot;

    DTR_ASSERT_NOT_NULL(fake);

    /* Manually inject a READY map entry so map.use() can resolve it. */
    slot = dtr_res_reserve(&tc->con, tc->con.res, "lvl", DTR_RES_MAP);
    DTR_ASSERT(slot > 0);
    tc->con.res->entries[slot].status      = DTR_RES_STATUS_READY;
    tc->con.res->entries[slot].payload.map = fake;

    DTR_ASSERT_EQ_INT(tc->con.res->active_map_slot, -1);
    JS_FreeValue(tc->rt->ctx, prv_eval(tc, "map.use('lvl')"));
    DTR_ASSERT_EQ_INT(tc->con.res->active_map_slot, slot);

    /* Numeric handle path. */
    tc->con.res->active_map_slot = -1;
    {
        char code[64];
        SDL_snprintf(code, sizeof(code), "map.use(%d)", (int)slot);
        JS_FreeValue(tc->rt->ctx, prv_eval(tc, code));
    }
    DTR_ASSERT_EQ_INT(tc->con.res->active_map_slot, slot);

    /* Detach the fake before teardown so the importer's free path is skipped. */
    tc->con.res->entries[slot].payload.map = NULL;
    DTR_FREE(fake);
    prv_teardown(tc);
    fprintf(stderr, "  PASS map_use_sets_active\n");
}

static void test_init_promise_pending_then_resolves(void)
{
    test_ctx_t *tc;
    JSValue     v;

    fprintf(stderr, "  init_promise_pending_then_resolves\n");
    tc = prv_setup();

    /* Define _init returning a pending Promise we can resolve later. */
    v = prv_eval(tc,
                 "globalThis._init = function () {"
                 "  return new Promise(function (res) {"
                 "    globalThis.__resolveInit = res;"
                 "  });"
                 "};");
    JS_FreeValue(tc->rt->ctx, v);

    DTR_ASSERT(dtr_runtime_call_init(tc->rt));
    DTR_ASSERT(dtr_runtime_init_pending(tc->rt));

    /* Resolve and drain. */
    v = prv_eval(tc, "globalThis.__resolveInit(42);");
    JS_FreeValue(tc->rt->ctx, v);
    dtr_runtime_drain_jobs(tc->rt);

    DTR_ASSERT(!dtr_runtime_init_pending(tc->rt));
    DTR_ASSERT(!tc->rt->error_active);

    prv_teardown(tc);
    fprintf(stderr, "  PASS init_promise_pending_then_resolves\n");
}

static void test_init_promise_rejects_sets_error(void)
{
    test_ctx_t *tc;
    JSValue     v;

    fprintf(stderr, "  init_promise_rejects_sets_error\n");
    tc = prv_setup();

    v = prv_eval(tc,
                 "globalThis._init = function () {"
                 "  return Promise.reject(new Error('boom'));"
                 "};");
    JS_FreeValue(tc->rt->ctx, v);

    DTR_ASSERT(dtr_runtime_call_init(tc->rt));
    /* Microtask drain settles the rejection. */
    dtr_runtime_drain_jobs(tc->rt);

    DTR_ASSERT(!dtr_runtime_init_pending(tc->rt));
    DTR_ASSERT(tc->rt->error_active);

    prv_teardown(tc);
    fprintf(stderr, "  PASS init_promise_rejects_sets_error\n");
}

static void test_init_sync_does_not_pend(void)
{
    test_ctx_t *tc;
    JSValue     v;

    fprintf(stderr, "  init_sync_does_not_pend\n");
    tc = prv_setup();

    v = prv_eval(tc, "globalThis._init = function () { return 7; };");
    JS_FreeValue(tc->rt->ctx, v);

    DTR_ASSERT(dtr_runtime_call_init(tc->rt));
    DTR_ASSERT(!dtr_runtime_init_pending(tc->rt));

    prv_teardown(tc);
    fprintf(stderr, "  PASS init_sync_does_not_pend\n");
}

static void test_init_slow_warning_fires_once(void)
{
    test_ctx_t *tc;
    JSValue     v;

    fprintf(stderr, "  init_slow_warning_fires_once\n");
    tc = prv_setup();

    /* A pending Promise we never resolve. */
    v = prv_eval(tc,
                 "globalThis._init = function () {"
                 "  return new Promise(function () {});"
                 "};");
    JS_FreeValue(tc->rt->ctx, v);

    DTR_ASSERT(dtr_runtime_call_init(tc->rt));
    DTR_ASSERT(dtr_runtime_init_pending(tc->rt));
    DTR_ASSERT(!tc->rt->init_warned_slow);

    /* Fault-inject: pretend _init started long ago so elapsed_ms > 30000. */
    tc->rt->init_start_perf = 1;
    DTR_ASSERT(dtr_runtime_init_pending(tc->rt));
    DTR_ASSERT(tc->rt->init_warned_slow);

    /* Second call must not re-fire (one-shot). */
    {
        bool was_set = tc->rt->init_warned_slow;
        DTR_ASSERT(dtr_runtime_init_pending(tc->rt));
        DTR_ASSERT_EQ_INT((int)was_set, (int)tc->rt->init_warned_slow);
    }

    prv_teardown(tc);
    fprintf(stderr, "  PASS init_slow_warning_fires_once\n");
}

/* Minimal 1x1 RGBA PNG (red pixel), 70 bytes. */
static const uint8_t k_one_px_png[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
    0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
    0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x78,
    0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0, 0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0x89, 0x99,
    0x3D, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static void test_load_sprite_decodes_png(void)
{
    test_ctx_t *tc;
    int32_t     slot;

    fprintf(stderr, "  load_sprite_decodes_png\n");
    tc = prv_setup();

    prv_write_file(tc->tmpdir, "px.png", k_one_px_png, sizeof(k_one_px_png));

    {
        JSValue v = prv_eval(tc, "globalThis.__ps = res.loadSprite('px', 'px.png'); 1");
        DTR_ASSERT(!JS_IsException(v));
        JS_FreeValue(tc->rt->ctx, v);
    }
    DTR_ASSERT(prv_eval_bool(tc, "res.has('px')"));
    DTR_ASSERT(!prv_eval_bool(tc, "res.isLoaded('px')"));

    dtr_res_pump(&tc->con);
    dtr_runtime_drain_jobs(tc->rt);

    DTR_ASSERT(prv_eval_bool(tc, "res.isLoaded('px')"));

    slot = dtr_res_find(tc->con.res, "px");
    DTR_ASSERT(slot > 0);
    DTR_ASSERT_EQ_INT(tc->con.res->entries[slot].kind, (int)DTR_RES_SPRITE);
    DTR_ASSERT_EQ_INT(tc->con.res->entries[slot].payload.sprite.w, 1);
    DTR_ASSERT_EQ_INT(tc->con.res->entries[slot].payload.sprite.h, 1);
    DTR_ASSERT(tc->con.res->entries[slot].payload.sprite.rgba != NULL);

    prv_teardown(tc);
    fprintf(stderr, "  PASS load_sprite_decodes_png\n");
}

int main(void)
{
    fprintf(stderr, "test_api_res:\n");
    test_namespace_registered();
    test_load_json_resolves();
    test_load_raw_uint8array();
    test_handle_and_unload();
    test_list_filter();
    test_set_active_flags();
    test_set_active_palette();
    test_load_missing_rejects();
    test_sfx_play_unknown_throws();
    test_mus_play_unknown_throws();
    test_map_use_unknown_throws();
    test_map_use_sets_active();
    test_init_promise_pending_then_resolves();
    test_init_promise_rejects_sets_error();
    test_init_sync_does_not_pend();
    test_init_slow_warning_fires_once();
    test_load_sprite_decodes_png();
    fprintf(stderr, "ALL PASS\n");
    return 0;
}
