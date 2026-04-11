/**
 * \file            test_api_bridge_gfx.c
 * \brief           Unit tests for JS API bridge functions that need a
 *                  graphics context (gfx, cam, postfx, ui)
 *
 * Uses a small framebuffer and no windowing — safe for headless CI.
 */

#include "api/api_common.h"
#include "cart.h"
#include "graphics.h"
#include "postfx.h"
#include "runtime.h"
#include "test_harness.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TW 32
#define TH 32

/* ================================================================== */
/*  Test context                                                       */
/* ================================================================== */

typedef struct test_ctx {
    dtr_console_t  con;
    dtr_runtime_t *rt;
} test_ctx_t;

static test_ctx_t *prv_setup(void)
{
    test_ctx_t *tc;

    tc = DTR_CALLOC(1, sizeof(test_ctx_t));
    DTR_ASSERT(tc != NULL);

    tc->con.fb_width  = TW;
    tc->con.fb_height = TH;

    tc->con.graphics = dtr_gfx_create(TW, TH);
    tc->con.postfx   = dtr_postfx_create(TW, TH);
    tc->con.cart     = dtr_cart_create();

    DTR_ASSERT(tc->con.graphics != NULL);
    DTR_ASSERT(tc->con.postfx != NULL);
    DTR_ASSERT(tc->con.cart != NULL);

    tc->rt = dtr_runtime_create(&tc->con, 8, 256);
    DTR_ASSERT(tc->rt != NULL);

    return tc;
}

static void prv_teardown(test_ctx_t *tc)
{
    dtr_runtime_destroy(tc->rt);
    dtr_cart_destroy(tc->con.cart);
    dtr_postfx_destroy(tc->con.postfx);
    dtr_gfx_destroy(tc->con.graphics);
    DTR_FREE(tc);
}

static void prv_register(test_ctx_t *tc, void (*fn)(JSContext *, JSValue))
{
    JSValue global;

    global = JS_GetGlobalObject(tc->rt->ctx);
    fn(tc->rt->ctx, global);
    JS_FreeValue(tc->rt->ctx, global);
}

static double prv_eval_f64(test_ctx_t *tc, const char *code)
{
    JSValue val;
    double  result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    JS_ToFloat64(tc->rt->ctx, &result, val);
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

static int32_t prv_eval_i32(test_ctx_t *tc, const char *code)
{
    JSValue val;
    int32_t result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    JS_ToInt32(tc->rt->ctx, &result, val);
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

static bool prv_eval_bool(test_ctx_t *tc, const char *code)
{
    JSValue val;
    bool    result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    result = JS_ToBool(tc->rt->ctx, val) != 0;
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

static void prv_eval_void(test_ctx_t *tc, const char *code)
{
    JSValue val;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    JS_FreeValue(tc->rt->ctx, val);
}

/* ================================================================== */
/*  GFX API tests                                                      */
/* ================================================================== */

static void test_gfx_cls(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(7)");
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(0, 0)"), 7);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(15, 15)"), 7);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_pset_pget(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.pset(5, 5, 12)");
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(5, 5)"), 12);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(0, 0)"), 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_line(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.line(0, 0, 10, 0, 3)");
    /* First pixel on the line should be colored */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(0, 0)"), 3);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(5, 0)"), 3);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_rectfill(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.rectfill(2, 2, 6, 6, 9)");
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(4, 4)"), 9);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(0, 0)"), 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_rect(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.rect(2, 2, 10, 10, 5)");
    /* Corner pixel on the outline */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(2, 2)"), 5);
    /* Interior should be transparent (cls color) */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(5, 5)"), 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_circ(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.circfill(16, 16, 5, 11)");
    /* Center should be filled */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(16, 16)"), 11);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_print(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    /* print should not crash; textWidth gives the advance */
    prv_eval_void(tc, "gfx.print('A', 0, 0, 7)");
    int32_t w = prv_eval_i32(tc, "gfx.textWidth('A')");
    DTR_ASSERT(w > 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_text_width(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    int32_t w = prv_eval_i32(tc, "gfx.textWidth('Hello')");
    DTR_ASSERT(w > 0);
    DTR_ASSERT(prv_eval_i32(tc, "gfx.textHeight('Hello')") > 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_clip(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.clip(5, 5, 10, 10)");
    /* Draw across the entire row, only clipped pixels are set */
    prv_eval_void(tc, "gfx.line(0, 7, 31, 7, 4)");
    /* Inside clip region */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(7, 7)"), 4);
    /* Outside clip region — should remain 0 */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(0, 7)"), 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_camera(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.camera(10, 20)");
    DTR_ASSERT_EQ_INT(tc->con.graphics->camera_x, 10);
    DTR_ASSERT_EQ_INT(tc->con.graphics->camera_y, 20);
    prv_eval_void(tc, "gfx.camera(0, 0)");
    DTR_ASSERT_EQ_INT(tc->con.graphics->camera_x, 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_pal(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    /* Remap color 1 to color 7 */
    prv_eval_void(tc, "gfx.pal(1, 7)");
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.pset(0, 0, 1)");
    /* After pal(1,7), drawing 1 should produce 7 */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(0, 0)"), 7);
    /* Reset palette */
    prv_eval_void(tc, "gfx.pal()");
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_peek_poke(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    prv_eval_void(tc, "gfx.cls(0)");
    prv_eval_void(tc, "gfx.poke(0, 42)");
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.peek(0)"), 42);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_color(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    /* color() sets the default drawing color */
    prv_eval_void(tc, "gfx.color(9)");
    prv_eval_void(tc, "gfx.cls(0)");
    /* pset without explicit color uses the default */
    prv_eval_void(tc, "gfx.pset(0, 0)");
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "gfx.pget(0, 0)"), 9);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_gfx_fget_fset(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_gfx_api_register);
    /* Default flag should be false */
    DTR_ASSERT(!prv_eval_bool(tc, "gfx.fget(0, 0)"));
    prv_eval_void(tc, "gfx.fset(0, 0, true)");
    DTR_ASSERT(prv_eval_bool(tc, "gfx.fget(0, 0)"));
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Camera API tests                                                   */
/* ================================================================== */

static void test_cam_set_get(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_cam_api_register);
    prv_eval_void(tc, "cam.set(100, 200)");
    DTR_ASSERT(prv_eval_bool(tc, "cam.get().x === 100"));
    DTR_ASSERT(prv_eval_bool(tc, "cam.get().y === 200"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_cam_reset(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_cam_api_register);
    prv_eval_void(tc, "cam.set(50, 75)");
    prv_eval_void(tc, "cam.reset()");
    DTR_ASSERT(prv_eval_bool(tc, "cam.get().x === 0"));
    DTR_ASSERT(prv_eval_bool(tc, "cam.get().y === 0"));
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  PostFX API tests                                                   */
/* ================================================================== */

static void test_postfx_available(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_postfx_api_register);
    /* available() returns an array of effect names */
    DTR_ASSERT(prv_eval_bool(tc, "Array.isArray(postfx.available())"));
    DTR_ASSERT(prv_eval_i32(tc, "postfx.available().length") > 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_postfx_push_pop(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_postfx_api_register);
    /* push using numeric effect ID (1 = CRT) */
    prv_eval_void(tc, "postfx.push(1, 0.5)");
    DTR_ASSERT_EQ_INT(tc->con.postfx->count, 1);
    prv_eval_void(tc, "postfx.pop()");
    DTR_ASSERT_EQ_INT(tc->con.postfx->count, 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_postfx_clear(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_postfx_api_register);
    prv_eval_void(tc, "postfx.push(1, 1.0)");
    prv_eval_void(tc, "postfx.push(2, 0.5)");
    DTR_ASSERT_EQ_INT(tc->con.postfx->count, 2);
    prv_eval_void(tc, "postfx.clear()");
    DTR_ASSERT_EQ_INT(tc->con.postfx->count, 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_postfx_save_restore(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_postfx_api_register);
    prv_eval_void(tc, "postfx.push(1, 1.0)");
    prv_eval_void(tc, "postfx.save()");
    prv_eval_void(tc, "postfx.push(3, 0.5)");
    DTR_ASSERT_EQ_INT(tc->con.postfx->count, 2);
    prv_eval_void(tc, "postfx.restore()");
    DTR_ASSERT_EQ_INT(tc->con.postfx->count, 1);
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  UI API tests                                                       */
/* ================================================================== */

static void test_ui_rect(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_ui_api_register);
    /* ui.rect returns {x, y, w, h} */
    DTR_ASSERT(prv_eval_bool(tc, "var r = ui.rect(10, 20, 100, 50); r.x === 10"));
    DTR_ASSERT(prv_eval_bool(tc, "r.y === 20"));
    DTR_ASSERT(prv_eval_bool(tc, "r.w === 100"));
    DTR_ASSERT(prv_eval_bool(tc, "r.h === 50"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_ui_inset(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_ui_api_register);
    prv_eval_void(tc, "var r = ui.rect(0, 0, 100, 100)");
    prv_eval_void(tc, "var inner = ui.inset(r, 10)");
    DTR_ASSERT(prv_eval_bool(tc, "inner.x === 10"));
    DTR_ASSERT(prv_eval_bool(tc, "inner.y === 10"));
    DTR_ASSERT(prv_eval_bool(tc, "inner.w === 80"));
    DTR_ASSERT(prv_eval_bool(tc, "inner.h === 80"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_ui_hsplit(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_ui_api_register);
    prv_eval_void(tc, "var r = ui.rect(0, 0, 100, 50)");
    prv_eval_void(tc, "var parts = ui.hsplit(r, 0.5)");
    DTR_ASSERT(prv_eval_bool(tc, "parts[0].w === 50"));
    DTR_ASSERT(prv_eval_bool(tc, "parts[1].w === 50"));
    DTR_ASSERT(prv_eval_bool(tc, "parts[0].h === 50"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_ui_vsplit(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_ui_api_register);
    prv_eval_void(tc, "var r = ui.rect(0, 0, 100, 100)");
    prv_eval_void(tc, "var parts = ui.vsplit(r, 0.5)");
    DTR_ASSERT(prv_eval_bool(tc, "parts[0].h === 50"));
    DTR_ASSERT(prv_eval_bool(tc, "parts[1].h === 50"));
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Runner                                                             */
/* ================================================================== */

int main(void)
{
    DTR_TEST_BEGIN("test_api_bridge_gfx");

    /* GFX API — 14 tests */
    DTR_RUN_TEST(test_gfx_cls);
    DTR_RUN_TEST(test_gfx_pset_pget);
    DTR_RUN_TEST(test_gfx_line);
    DTR_RUN_TEST(test_gfx_rectfill);
    DTR_RUN_TEST(test_gfx_rect);
    DTR_RUN_TEST(test_gfx_circ);
    DTR_RUN_TEST(test_gfx_print);
    DTR_RUN_TEST(test_gfx_text_width);
    DTR_RUN_TEST(test_gfx_clip);
    DTR_RUN_TEST(test_gfx_camera);
    DTR_RUN_TEST(test_gfx_pal);
    DTR_RUN_TEST(test_gfx_peek_poke);
    DTR_RUN_TEST(test_gfx_color);
    DTR_RUN_TEST(test_gfx_fget_fset);

    /* Camera API — 2 tests */
    DTR_RUN_TEST(test_cam_set_get);
    DTR_RUN_TEST(test_cam_reset);

    /* PostFX API — 4 tests */
    DTR_RUN_TEST(test_postfx_available);
    DTR_RUN_TEST(test_postfx_push_pop);
    DTR_RUN_TEST(test_postfx_clear);
    DTR_RUN_TEST(test_postfx_save_restore);

    /* UI API — 4 tests */
    DTR_RUN_TEST(test_ui_rect);
    DTR_RUN_TEST(test_ui_inset);
    DTR_RUN_TEST(test_ui_hsplit);
    DTR_RUN_TEST(test_ui_vsplit);

    DTR_TEST_END();
}
