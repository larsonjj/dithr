/**
 * \file            test_resources.c
 * \brief           Phase 2 unit tests for the resource registry, async load
 *                  queue, and decode dispatch.
 *
 * No graphics/audio init — these tests focus on registry semantics, the
 * pump for native sync paths (RAW / HEX / JSON), promise resolution, and
 * cleanup. Sprite/Map/SFX/Music decoders go through cart_import / audio
 * APIs that need their own subsystems and are exercised by the existing
 * test_cart_import / test_audio suites.
 */

#include "cart.h"
#include "resources.h"
#include "runtime.h"
#include "test_harness.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Test fixture                                                       */
/* ------------------------------------------------------------------ */

typedef struct test_ctx {
    dtr_console_t  con;
    dtr_runtime_t *rt;
    char           tmpdir[512];
} test_ctx_t;

static void prv_make_tmpdir(test_ctx_t *tc)
{
    /* Use the build-tree temp area beside the test exe so cleanup works
       on Windows even if SDL tmp dirs are restricted. */
    SDL_snprintf(tc->tmpdir, sizeof(tc->tmpdir), "test_resources_tmp");
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

    tc->con.cart = dtr_cart_create();
    DTR_ASSERT_NOT_NULL(tc->con.cart);

    tc->rt = dtr_runtime_create(&tc->con, 8, 1024);
    DTR_ASSERT_NOT_NULL(tc->rt);
    tc->con.runtime = tc->rt;

    tc->con.res = dtr_res_create();
    DTR_ASSERT_NOT_NULL(tc->con.res);

    prv_make_tmpdir(tc);
    SDL_strlcpy(tc->con.cart->base_path, tc->tmpdir, sizeof(tc->con.cart->base_path));

    return tc;
}

static void prv_teardown(test_ctx_t *tc)
{
    dtr_runtime_drain_jobs(tc->rt);
    dtr_res_destroy(&tc->con, tc->con.res);
    dtr_runtime_destroy(tc->rt);
    dtr_cart_destroy(tc->con.cart);
    DTR_FREE(tc);
}

/* ------------------------------------------------------------------ */
/*  Tests                                                              */
/* ------------------------------------------------------------------ */

static void test_create_destroy(void)
{
    test_ctx_t *tc = prv_setup();
    DTR_ASSERT_EQ_INT(tc->con.res->pending_count, 0);
    DTR_ASSERT_EQ_INT(tc->con.res->active_sheet_slot, -1);
    DTR_ASSERT_EQ_INT(dtr_res_find(tc->con.res, "missing"), -1);
    prv_teardown(tc);
    fprintf(stderr, "  PASS create_destroy\n");
}

static void test_reserve_and_find(void)
{
    test_ctx_t *tc = prv_setup();
    int32_t     a;
    int32_t     b;

    a = dtr_res_reserve(&tc->con, tc->con.res, "alpha", DTR_RES_RAW);
    DTR_ASSERT(a > 0);
    DTR_ASSERT_EQ_INT(dtr_res_find(tc->con.res, "alpha"), a);

    b = dtr_res_reserve(&tc->con, tc->con.res, "beta", DTR_RES_RAW);
    DTR_ASSERT(b > 0 && b != a);

    /* Re-reserving "alpha" returns the same slot. */
    DTR_ASSERT_EQ_INT(dtr_res_reserve(&tc->con, tc->con.res, "alpha", DTR_RES_RAW), a);

    prv_teardown(tc);
    fprintf(stderr, "  PASS reserve_and_find\n");
}

static void test_unload(void)
{
    test_ctx_t *tc = prv_setup();
    int32_t     s  = dtr_res_reserve(&tc->con, tc->con.res, "x", DTR_RES_RAW);
    DTR_ASSERT(s > 0);
    dtr_res_unload(&tc->con, tc->con.res, s);
    DTR_ASSERT_EQ_INT(dtr_res_find(tc->con.res, "x"), -1);

    /* Re-reservation should reuse the freed slot. */
    DTR_ASSERT_EQ_INT(dtr_res_reserve(&tc->con, tc->con.res, "x", DTR_RES_RAW), s);
    prv_teardown(tc);
    fprintf(stderr, "  PASS unload\n");
}

static void test_pump_loads_raw_file(void)
{
    test_ctx_t *tc = prv_setup();
    JSValue     resolve;
    JSValue     reject;
    JSValue     promise;
    int32_t     slot;
    const char  payload[] = "hello world";

    prv_write_file(tc->tmpdir, "blob.bin", payload, sizeof(payload) - 1);

    promise = dtr_runtime_make_promise(tc->rt, &resolve, &reject);
    DTR_ASSERT(!JS_IsException(promise));

    slot = dtr_res_reserve(&tc->con, tc->con.res, "blob", DTR_RES_RAW);
    DTR_ASSERT(slot > 0);
    DTR_ASSERT(dtr_res_enqueue(tc->con.res, slot, DTR_RES_RAW, "blob.bin", resolve, reject));
    DTR_ASSERT_EQ_INT(tc->con.res->pending_count, 1);

    dtr_res_pump(&tc->con);
    DTR_ASSERT_EQ_INT(tc->con.res->pending_count, 0);
    DTR_ASSERT_EQ_INT(tc->con.res->entries[slot].status, DTR_RES_STATUS_READY);
    DTR_ASSERT_EQ_INT((int)tc->con.res->entries[slot].payload.blob.len, (int)(sizeof(payload) - 1));
    DTR_ASSERT(memcmp(tc->con.res->entries[slot].payload.blob.data, payload, sizeof(payload) - 1) ==
               0);

    JS_FreeValue(tc->rt->ctx, promise);
    prv_teardown(tc);
    fprintf(stderr, "  PASS pump_loads_raw_file\n");
}

static void test_pump_rejects_missing_file(void)
{
    test_ctx_t *tc = prv_setup();
    JSValue     resolve;
    JSValue     reject;
    JSValue     promise;
    int32_t     slot;

    promise = dtr_runtime_make_promise(tc->rt, &resolve, &reject);
    slot    = dtr_res_reserve(&tc->con, tc->con.res, "ghost", DTR_RES_RAW);
    DTR_ASSERT(
        dtr_res_enqueue(tc->con.res, slot, DTR_RES_RAW, "no_such_file.bin", resolve, reject));

    dtr_res_pump(&tc->con);
    DTR_ASSERT_EQ_INT(tc->con.res->entries[slot].status, DTR_RES_STATUS_ERROR);
    DTR_ASSERT_EQ_INT((int)JS_PromiseState(tc->rt->ctx, promise), JS_PROMISE_REJECTED);

    JS_FreeValue(tc->rt->ctx, promise);
    prv_teardown(tc);
    fprintf(stderr, "  PASS pump_rejects_missing_file\n");
}

static void test_pump_decodes_json(void)
{
    test_ctx_t *tc = prv_setup();
    JSValue     resolve;
    JSValue     reject;
    JSValue     promise;
    int32_t     slot;
    const char  json[] = "{\"x\":7,\"y\":\"ok\"}";

    prv_write_file(tc->tmpdir, "data.json", json, sizeof(json) - 1);

    promise = dtr_runtime_make_promise(tc->rt, &resolve, &reject);
    slot    = dtr_res_reserve(&tc->con, tc->con.res, "data", DTR_RES_JSON);
    DTR_ASSERT(dtr_res_enqueue(tc->con.res, slot, DTR_RES_JSON, "data.json", resolve, reject));

    dtr_res_pump(&tc->con);
    DTR_ASSERT_EQ_INT(tc->con.res->entries[slot].status, DTR_RES_STATUS_READY);
    DTR_ASSERT_EQ_INT((int)JS_PromiseState(tc->rt->ctx, promise), JS_PROMISE_FULFILLED);

    {
        JSValue stored = JS_DupValue(tc->rt->ctx, tc->con.res->entries[slot].payload.json);
        JSValue x      = JS_GetPropertyStr(tc->rt->ctx, stored, "x");
        int32_t xi     = -1;
        JS_ToInt32(tc->rt->ctx, &xi, x);
        DTR_ASSERT_EQ_INT(xi, 7);
        JS_FreeValue(tc->rt->ctx, x);
        JS_FreeValue(tc->rt->ctx, stored);
    }

    JS_FreeValue(tc->rt->ctx, promise);
    prv_teardown(tc);
    fprintf(stderr, "  PASS pump_decodes_json\n");
}

static void test_resolve_handle_string_and_number(void)
{
    test_ctx_t *tc = prv_setup();
    int32_t     slot;
    JSValue     resolve;
    JSValue     reject;
    JSValue     promise;
    const char  payload[] = "x";

    prv_write_file(tc->tmpdir, "h.bin", payload, sizeof(payload) - 1);
    promise = dtr_runtime_make_promise(tc->rt, &resolve, &reject);
    slot    = dtr_res_reserve(&tc->con, tc->con.res, "h", DTR_RES_RAW);
    DTR_ASSERT(dtr_res_enqueue(tc->con.res, slot, DTR_RES_RAW, "h.bin", resolve, reject));
    dtr_res_pump(&tc->con);

    /* Resolve via name. */
    {
        JSValue js_name = JS_NewString(tc->rt->ctx, "h");
        DTR_ASSERT_EQ_INT(dtr_res_resolve_handle(&tc->con, js_name, DTR_RES_RAW), slot);
        JS_FreeValue(tc->rt->ctx, js_name);
    }
    /* Resolve via numeric handle. */
    {
        JSValue js_num = JS_NewInt32(tc->rt->ctx, slot);
        DTR_ASSERT_EQ_INT(dtr_res_resolve_handle(&tc->con, js_num, DTR_RES_RAW), slot);
        JS_FreeValue(tc->rt->ctx, js_num);
    }
    /* Wrong kind rejected. */
    {
        JSValue js_num = JS_NewInt32(tc->rt->ctx, slot);
        DTR_ASSERT_EQ_INT(dtr_res_resolve_handle(&tc->con, js_num, DTR_RES_SPRITE), -1);
        JS_FreeValue(tc->rt->ctx, js_num);
    }

    JS_FreeValue(tc->rt->ctx, promise);
    prv_teardown(tc);
    fprintf(stderr, "  PASS resolve_handle_string_and_number\n");
}

static void test_unload_clears_active_selectors(void)
{
    test_ctx_t *tc = prv_setup();
    int32_t     s  = dtr_res_reserve(&tc->con, tc->con.res, "sheet1", DTR_RES_SPRITE);
    tc->con.res->active_sheet_slot = s;
    dtr_res_unload(&tc->con, tc->con.res, s);
    DTR_ASSERT_EQ_INT(tc->con.res->active_sheet_slot, -1);
    prv_teardown(tc);
    fprintf(stderr, "  PASS unload_clears_active_selectors\n");
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                        */
/* ------------------------------------------------------------------ */

int main(void)
{
    fprintf(stderr, "test_resources:\n");
    test_create_destroy();
    test_reserve_and_find();
    test_unload();
    test_pump_loads_raw_file();
    test_pump_rejects_missing_file();
    test_pump_decodes_json();
    test_resolve_handle_string_and_number();
    test_unload_clears_active_selectors();
    fprintf(stderr, "ALL PASS\n");
    return 0;
}
