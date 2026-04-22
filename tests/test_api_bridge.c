/**
 * \file            test_api_bridge.c
 * \brief           Unit tests for JS API bridge functions (math, key, evt,
 *                  input, col, pad, mouse, touch, tween, sys, cart)
 *
 * Creates a minimal console stub + runtime, registers individual API
 * namespaces, and exercises them via JS_Eval.
 */

#include "api/api_common.h"
#include "cart.h"
#include "event.h"
#include "gamepad.h"
#include "input.h"
#include "mouse.h"
#include "runtime.h"
#include "test_harness.h"
#include "touch.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ================================================================== */
/*  Test console stub                                                  */
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

    /* Deterministic RNG seed */
    tc->con.rng_state = 42;

    /* Framebuffer dimensions (needed by sys.width/height and touch) */
    tc->con.fb_width   = CONSOLE_FB_WIDTH;
    tc->con.fb_height  = CONSOLE_FB_HEIGHT;
    tc->con.target_fps = CONSOLE_TARGET_FPS;

    /* Create subsystems needed by APIs under test */
    tc->con.keys     = dtr_key_create();
    tc->con.mouse    = dtr_mouse_create();
    tc->con.gamepads = dtr_gamepad_create();
    tc->con.input    = dtr_input_create();
    tc->con.touch    = dtr_touch_create(tc->con.fb_width, tc->con.fb_height);
    tc->con.cart     = dtr_cart_create();

    DTR_ASSERT(tc->con.keys != NULL);
    DTR_ASSERT(tc->con.mouse != NULL);
    DTR_ASSERT(tc->con.gamepads != NULL);
    DTR_ASSERT(tc->con.input != NULL);
    DTR_ASSERT(tc->con.touch != NULL);
    DTR_ASSERT(tc->con.cart != NULL);

    /* Create runtime — sets JS_SetContextOpaque(ctx, &tc->con) */
    tc->rt = dtr_runtime_create(&tc->con, 8, 256);
    DTR_ASSERT(tc->rt != NULL);

    /* Event bus needs the JSContext */
    tc->con.events = dtr_event_create(tc->rt->ctx);
    DTR_ASSERT(tc->con.events != NULL);

    return tc;
}

static void prv_teardown(test_ctx_t *tc)
{
    /* Drain any pending JS jobs (Promise microtasks like .catch handlers) so
     * they don't leak into runtime destruction and trip GC asserts under
     * macOS ASan/UBSan. */
    dtr_runtime_drain_jobs(tc->rt);
    dtr_event_destroy(tc->con.events);
    dtr_runtime_destroy(tc->rt);
    dtr_cart_destroy(tc->con.cart);
    dtr_input_destroy(tc->con.input);
    dtr_gamepad_destroy(tc->con.gamepads);
    dtr_touch_destroy(tc->con.touch);
    dtr_mouse_destroy(tc->con.mouse);
    dtr_key_destroy(tc->con.keys);
    DTR_FREE(tc);
}

/** Register a single API namespace. */
static void prv_register(test_ctx_t *tc, void (*fn)(JSContext *, JSValue))
{
    JSValue global;

    global = JS_GetGlobalObject(tc->rt->ctx);
    fn(tc->rt->ctx, global);
    JS_FreeValue(tc->rt->ctx, global);
}

/** Print a JS exception (message + stack) to stderr for diagnostics. */
static void prv_dump_exception(test_ctx_t *tc, const char *code)
{
    JSValue     exc;
    JSValue     stack;
    const char *msg;
    const char *stk;

    exc = JS_GetException(tc->rt->ctx);
    msg = JS_ToCString(tc->rt->ctx, exc);
    fprintf(stderr, "  EXC in <%s>: %s\n", code, msg ? msg : "(unknown)");
    if (msg) {
        JS_FreeCString(tc->rt->ctx, msg);
    }
    if (JS_IsError(exc)) {
        stack = JS_GetPropertyStr(tc->rt->ctx, exc, "stack");
        if (!JS_IsUndefined(stack)) {
            stk = JS_ToCString(tc->rt->ctx, stack);
            if (stk) {
                fprintf(stderr, "%s", stk);
                JS_FreeCString(tc->rt->ctx, stk);
            }
        }
        JS_FreeValue(tc->rt->ctx, stack);
    }
    JS_FreeValue(tc->rt->ctx, exc);
    fflush(stderr);
}

/** Evaluate JS and return the float64 result. */
static double prv_eval_f64(test_ctx_t *tc, const char *code)
{
    JSValue val;
    double  result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        prv_dump_exception(tc, code);
    }
    DTR_ASSERT(!JS_IsException(val));
    JS_ToFloat64(tc->rt->ctx, &result, val);
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

/** Evaluate JS and return the int32 result. */
static int32_t prv_eval_i32(test_ctx_t *tc, const char *code)
{
    JSValue val;
    int32_t result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        prv_dump_exception(tc, code);
    }
    DTR_ASSERT(!JS_IsException(val));
    JS_ToInt32(tc->rt->ctx, &result, val);
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

/** Evaluate JS and return the bool result. */
static bool prv_eval_bool(test_ctx_t *tc, const char *code)
{
    JSValue val;
    bool    result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        prv_dump_exception(tc, code);
    }
    DTR_ASSERT(!JS_IsException(val));
    result = JS_ToBool(tc->rt->ctx, val) != 0;
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

/** Evaluate JS expecting no exception. */
static void prv_eval_void(test_ctx_t *tc, const char *code)
{
    JSValue val;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        prv_dump_exception(tc, code);
    }
    DTR_ASSERT(!JS_IsException(val));
    JS_FreeValue(tc->rt->ctx, val);
}

/* ================================================================== */
/*  Math API tests                                                     */
/* ================================================================== */

static void test_math_flr(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.flr(3.7)"), 3.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.flr(-1.2)"), -2.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_ceil(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.ceil(3.1)"), 4.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.ceil(-1.9)"), -1.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_round(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.round(3.5)"), 4.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.round(3.4)"), 3.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_trunc(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.trunc(3.9)"), 3.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.trunc(-3.9)"), -3.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_abs(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.abs(-5)"), 5.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.abs(7)"), 7.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_sign(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.sign(10)"), 1.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.sign(-3)"), -1.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.sign(0)"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_min_max(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.min(3, 7)"), 3.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.max(3, 7)"), 7.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_mid(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.mid(1, 5, 3)"), 3.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.mid(5, 1, 3)"), 3.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.mid(3, 5, 1)"), 3.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_sqrt_pow(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.sqrt(16)"), 4.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.pow(2, 10)"), 1024.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_trig(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);

    /* sin(0) = 0, sin(0.25) = -1 (0..1 = full rotation, negated) */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.sin(0)"), 0.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.sin(0.25)"), -1.0, 0.001);

    /* cos(0) = 1, cos(0.25) = 0 */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.cos(0)"), 1.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.cos(0.25)"), 0.0, 0.001);

    /* tan(0) = 0 */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.tan(0)"), 0.0, 0.001);

    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_inverse_trig(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.asin(0)"), 0.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.acos(1)"), 0.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.atan(0)"), 0.0, 0.001);
    /* atan2(0, 1) = 0 in 0..1 range */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.atan2(0, 1)"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_lerp(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.lerp(0, 100, 0.5)"), 50.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.lerp(10, 20, 0)"), 10.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.lerp(10, 20, 1)"), 20.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_unlerp(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.unlerp(0, 100, 50)"), 0.5, 0.001);
    /* Division by zero returns 0 */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.unlerp(5, 5, 5)"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_remap(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    /* remap(50, 0, 100, 0, 1000) = 500 */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.remap(50, 0, 100, 0, 1000)"), 500.0, 0.001);
    /* remap with zero range returns c */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.remap(5, 5, 5, 10, 20)"), 10.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_clamp(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.clamp(50, 0, 100)"), 50.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.clamp(-10, 0, 100)"), 0.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.clamp(200, 0, 100)"), 100.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_smoothstep(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.smoothstep(0, 1, 0)"), 0.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.smoothstep(0, 1, 1)"), 1.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.smoothstep(0, 1, 0.5)"), 0.5, 0.001);
    /* Zero range returns 0 */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.smoothstep(5, 5, 5)"), 0.0, 0.001);
    /* Clamped outside range */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.smoothstep(0, 1, -1)"), 0.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.smoothstep(0, 1, 2)"), 1.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_pingpong(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.pingpong(0.5, 1)"), 0.5, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.pingpong(1.5, 1)"), 0.5, 0.001);
    /* Zero length returns 0 */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.pingpong(5, 0)"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_dist(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.dist(0, 0, 3, 4)"), 5.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.dist(0, 0, 0, 0)"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_angle(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    /* angle(0,0, 1,0) = atan2(0,1) = 0 */
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.angle(0, 0, 1, 0)"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_rnd(void)
{
    test_ctx_t *tc = prv_setup();
    double      val;

    prv_register(tc, dtr_math_api_register);
    val = prv_eval_f64(tc, "math.rnd(100)");
    DTR_ASSERT(val >= 0.0 && val < 100.0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_rnd_int(void)
{
    test_ctx_t *tc = prv_setup();
    int32_t     val;

    prv_register(tc, dtr_math_api_register);
    val = prv_eval_i32(tc, "math.rndInt(50)");
    DTR_ASSERT(val >= 0 && val < 50);
    /* Zero max returns 0 */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "math.rndInt(0)"), 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_rnd_range(void)
{
    test_ctx_t *tc = prv_setup();
    double      val;

    prv_register(tc, dtr_math_api_register);
    val = prv_eval_f64(tc, "math.rndRange(10, 20)");
    DTR_ASSERT(val >= 10.0 && val <= 20.0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_seed(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);

    /* Seed then get a value, reseed and get same value */
    prv_eval_void(tc, "math.seed(12345)");
    double v1 = prv_eval_f64(tc, "math.rnd(1000)");
    prv_eval_void(tc, "math.seed(12345)");
    double v2 = prv_eval_f64(tc, "math.rnd(1000)");
    DTR_ASSERT_NEAR(v1, v2, 0.001);

    /* Seed with 0 should be bumped to 1 */
    prv_eval_void(tc, "math.seed(0)");
    DTR_ASSERT(tc->con.rng_state == 1);

    prv_teardown(tc);
    DTR_PASS();
}

static void test_math_constants(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_math_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.PI"), 3.14159265, 0.0001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "math.TAU"), 6.28318530, 0.0001);
    DTR_ASSERT(prv_eval_f64(tc, "math.HUGE") > 1e300);
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Key API tests                                                      */
/* ================================================================== */

static void test_key_btn_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_key_api_register);
    DTR_ASSERT(!prv_eval_bool(tc, "key.btn(key.Z)"));
    DTR_ASSERT(!prv_eval_bool(tc, "key.btn(key.SPACE)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_key_btn_after_press(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_key_api_register);
    dtr_key_set(tc->con.keys, DTR_KEY_Z, true);
    DTR_ASSERT(prv_eval_bool(tc, "key.btn(key.Z)"));
    DTR_ASSERT(!prv_eval_bool(tc, "key.btn(key.X)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_key_btnp(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_key_api_register);

    /* btnp = current && !previous */
    dtr_key_set(tc->con.keys, DTR_KEY_X, true);
    DTR_ASSERT(prv_eval_bool(tc, "key.btnp(key.X)"));

    /* After update, previous = true → btnp false */
    dtr_key_update(tc->con.keys, 1.0f / 60.0f);
    DTR_ASSERT(!prv_eval_bool(tc, "key.btnp(key.X)"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_key_btnr(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_key_api_register);

    /* btnr fires on initial press (current && !previous) */
    dtr_key_set(tc->con.keys, DTR_KEY_C, true);
    DTR_ASSERT(prv_eval_bool(tc, "key.btnr(key.C)"));

    /* After update, it's no longer a fresh press (unless key-repeat fires) */
    dtr_key_update(tc->con.keys, 1.0f / 60.0f);
    DTR_ASSERT(!prv_eval_bool(tc, "key.btnr(key.C)"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_key_name(void)
{
    test_ctx_t *tc = prv_setup();
    JSValue     val;
    const char *str;

    prv_register(tc, dtr_key_api_register);
    val = JS_Eval(tc->rt->ctx, "key.name(key.Z)", 15, "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    str = JS_ToCString(tc->rt->ctx, val);
    DTR_ASSERT(str != NULL);
    DTR_ASSERT(strlen(str) > 0);
    JS_FreeCString(tc->rt->ctx, str);
    JS_FreeValue(tc->rt->ctx, val);
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Event API tests                                                    */
/* ================================================================== */

static void test_evt_on_emit(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_evt_api_register);
    prv_eval_void(tc, "var _fired = false");
    prv_eval_void(tc, "evt.on('test', function() { _fired = true; })");
    prv_eval_void(tc, "evt.emit('test')");
    dtr_event_flush(tc->con.events);
    DTR_ASSERT(prv_eval_bool(tc, "_fired"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_evt_once(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_evt_api_register);
    prv_eval_void(tc, "var _count = 0");
    prv_eval_void(tc, "evt.once('ping', function() { _count++; })");
    prv_eval_void(tc, "evt.emit('ping')");
    dtr_event_flush(tc->con.events);
    prv_eval_void(tc, "evt.emit('ping')");
    dtr_event_flush(tc->con.events);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "_count"), 1);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_evt_off(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_evt_api_register);
    prv_eval_void(tc, "var _count2 = 0");
    prv_eval_void(tc, "var _h = evt.on('foo', function() { _count2++; })");
    prv_eval_void(tc, "evt.emit('foo')");
    dtr_event_flush(tc->con.events);
    prv_eval_void(tc, "evt.off(_h)");
    prv_eval_void(tc, "evt.emit('foo')");
    dtr_event_flush(tc->con.events);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "_count2"), 1);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_evt_emit_payload(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_evt_api_register);
    prv_eval_void(tc, "var _payload = null");
    prv_eval_void(tc, "evt.on('data', function(p) { _payload = p; })");
    prv_eval_void(tc, "evt.emit('data', 42)");
    dtr_event_flush(tc->con.events);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "_payload"), 42);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_evt_invalid_args(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_evt_api_register);
    /* on() with missing callback returns -1 */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "evt.on('x')"), -1);
    /* on() with non-function returns -1 */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "evt.on('x', 42)"), -1);
    /* once() with missing callback returns -1 */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "evt.once('y')"), -1);
    /* emit() with no args — should not crash */
    prv_eval_void(tc, "evt.emit()");

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Input API tests                                                    */
/* ================================================================== */

static void test_input_btn_unmapped(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_input_api_register);
    DTR_ASSERT(!prv_eval_bool(tc, "input.btn('jump')"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_input_map_btn(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_input_api_register);
    prv_register(tc, dtr_key_api_register);

    /* Map "jump" to KEY_Z via JS */
    prv_eval_void(tc, "input.map('jump', [{type: input.KEY, code: key.Z}])");

    /* Not pressed yet */
    dtr_input_update(tc->con.input, tc->con.keys, tc->con.gamepads, tc->con.mouse, tc->con.touch);
    DTR_ASSERT(!prv_eval_bool(tc, "input.btn('jump')"));

    /* Press Z */
    dtr_key_set(tc->con.keys, DTR_KEY_Z, true);
    dtr_input_update(tc->con.input, tc->con.keys, tc->con.gamepads, tc->con.mouse, tc->con.touch);
    DTR_ASSERT(prv_eval_bool(tc, "input.btn('jump')"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_input_btnp(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_input_api_register);

    /* Map via C */
    {
        dtr_binding_t b = {DTR_BIND_KEY, DTR_KEY_Z, 0.0f};
        dtr_input_map(tc->con.input, "jump", &b, 1);
    }

    dtr_key_set(tc->con.keys, DTR_KEY_Z, true);
    dtr_input_update(tc->con.input, tc->con.keys, tc->con.gamepads, tc->con.mouse, tc->con.touch);
    DTR_ASSERT(prv_eval_bool(tc, "input.btnp('jump')"));

    /* Second frame: no longer "just pressed" */
    dtr_key_update(tc->con.keys, 1.0f / 60.0f);
    dtr_input_update(tc->con.input, tc->con.keys, tc->con.gamepads, tc->con.mouse, tc->con.touch);
    DTR_ASSERT(!prv_eval_bool(tc, "input.btnp('jump')"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_input_axis(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_input_api_register);

    /* No mapping → axis returns 0 */
    dtr_input_update(tc->con.input, tc->con.keys, tc->con.gamepads, tc->con.mouse, tc->con.touch);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "input.axis('move')"), 0.0, 0.001);

    prv_teardown(tc);
    DTR_PASS();
}

static void test_input_clear(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_input_api_register);

    /* Map, then clear specific action */
    prv_eval_void(tc, "input.map('run', [{type: input.KEY, code: 1}])");
    prv_eval_void(tc, "input.clear('run')");

    /* Map again, then clear all */
    prv_eval_void(tc, "input.map('run', [{type: input.KEY, code: 1}])");
    prv_eval_void(tc, "input.clear()");

    prv_teardown(tc);
    DTR_PASS();
}

static void test_input_no_args(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_input_api_register);
    /* btn/btnp/axis with no args should return false/0 */
    DTR_ASSERT(!prv_eval_bool(tc, "input.btn()"));
    DTR_ASSERT(!prv_eval_bool(tc, "input.btnp()"));
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "input.axis()"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Collision API tests                                                */
/* ================================================================== */

static void test_col_rect(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_col_api_register);
    /* Overlapping rects */
    DTR_ASSERT(prv_eval_bool(tc, "col.rect(0, 0, 10, 10, 5, 5, 10, 10)"));
    /* Non-overlapping */
    DTR_ASSERT(!prv_eval_bool(tc, "col.rect(0, 0, 5, 5, 10, 10, 5, 5)"));
    /* Zero-size rect */
    DTR_ASSERT(!prv_eval_bool(tc, "col.rect(0, 0, 0, 10, 0, 0, 10, 10)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_col_circ(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_col_api_register);
    /* Overlapping circles */
    DTR_ASSERT(prv_eval_bool(tc, "col.circ(0, 0, 5, 3, 0, 5)"));
    /* Non-overlapping */
    DTR_ASSERT(!prv_eval_bool(tc, "col.circ(0, 0, 1, 10, 0, 1)"));
    /* Negative radius */
    DTR_ASSERT(!prv_eval_bool(tc, "col.circ(0, 0, -1, 0, 0, 5)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_col_point_rect(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_col_api_register);
    DTR_ASSERT(prv_eval_bool(tc, "col.pointRect(5, 5, 0, 0, 10, 10)"));
    DTR_ASSERT(!prv_eval_bool(tc, "col.pointRect(15, 5, 0, 0, 10, 10)"));
    /* Zero-size rect */
    DTR_ASSERT(!prv_eval_bool(tc, "col.pointRect(0, 0, 0, 0, 0, 10)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_col_point_circ(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_col_api_register);
    DTR_ASSERT(prv_eval_bool(tc, "col.pointCirc(3, 4, 0, 0, 6)"));
    DTR_ASSERT(!prv_eval_bool(tc, "col.pointCirc(10, 0, 0, 0, 5)"));
    /* Negative radius */
    DTR_ASSERT(!prv_eval_bool(tc, "col.pointCirc(0, 0, 0, 0, -1)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_col_circ_rect(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_col_api_register);
    /* Circle overlapping rect */
    DTR_ASSERT(prv_eval_bool(tc, "col.circRect(5, 5, 3, 0, 0, 10, 10)"));
    /* Circle outside rect */
    DTR_ASSERT(!prv_eval_bool(tc, "col.circRect(20, 20, 1, 0, 0, 10, 10)"));
    /* Negative radius */
    DTR_ASSERT(!prv_eval_bool(tc, "col.circRect(5, 5, -1, 0, 0, 10, 10)"));
    /* Zero-size rect */
    DTR_ASSERT(!prv_eval_bool(tc, "col.circRect(5, 5, 1, 0, 0, 0, 10)"));
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Pad API tests                                                      */
/* ================================================================== */

static void test_pad_count_and_connected(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_pad_api_register);
    /* No gamepads connected initially */
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "pad.count()"), 0);
    DTR_ASSERT(!prv_eval_bool(tc, "pad.connected(0)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_pad_btn_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_pad_api_register);
    DTR_ASSERT(!prv_eval_bool(tc, "pad.btn(pad.A, 0)"));
    DTR_ASSERT(!prv_eval_bool(tc, "pad.btnp(pad.B, 0)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_pad_axis_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_pad_api_register);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "pad.axis(pad.LX, 0)"), 0.0, 0.001);
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "pad.axis(pad.LY, 0)"), 0.0, 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_pad_name(void)
{
    test_ctx_t *tc = prv_setup();
    JSValue     val;
    const char *str;

    prv_register(tc, dtr_pad_api_register);
    val = JS_Eval(tc->rt->ctx, "pad.name(0)", 11, "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    str = JS_ToCString(tc->rt->ctx, val);
    DTR_ASSERT(str != NULL);
    JS_FreeCString(tc->rt->ctx, str);
    JS_FreeValue(tc->rt->ctx, val);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_pad_deadzone(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_pad_api_register);
    /* Default deadzone */
    double dz = prv_eval_f64(tc, "pad.deadzone()");
    DTR_ASSERT(dz >= 0.0 && dz <= 1.0);

    /* Set deadzone */
    prv_eval_f64(tc, "pad.deadzone(0.25)");
    DTR_ASSERT_NEAR(prv_eval_f64(tc, "pad.deadzone()"), 0.25, 0.01);

    prv_teardown(tc);
    DTR_PASS();
}

static void test_pad_rumble(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_pad_api_register);
    /* Rumble on disconnected pad should not crash */
    prv_eval_void(tc, "pad.rumble(0, 0xFFFF, 0xFFFF, 200)");
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Mouse API tests                                                    */
/* ================================================================== */

static void test_mouse_pos_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_mouse_api_register);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.x()"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.y()"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.dx()"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.dy()"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.wheel()"), 0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_mouse_btn_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_mouse_api_register);
    DTR_ASSERT(!prv_eval_bool(tc, "mouse.btn(mouse.LEFT)"));
    DTR_ASSERT(!prv_eval_bool(tc, "mouse.btn(mouse.RIGHT)"));
    DTR_ASSERT(!prv_eval_bool(tc, "mouse.btn(mouse.MIDDLE)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_mouse_btn_press(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_mouse_api_register);

    tc->con.mouse->btn_current[DTR_MOUSE_LEFT] = true;
    DTR_ASSERT(prv_eval_bool(tc, "mouse.btn(mouse.LEFT)"));

    /* btnp uses btn_pressed[] (set externally by SDL event handling) */
    tc->con.mouse->btn_pressed[DTR_MOUSE_LEFT] = true;
    DTR_ASSERT(prv_eval_bool(tc, "mouse.btnp(mouse.LEFT)"));

    /* After update, btn_pressed is cleared */
    dtr_mouse_update(tc->con.mouse);
    DTR_ASSERT(!prv_eval_bool(tc, "mouse.btnp(mouse.LEFT)"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_mouse_show_hide(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_mouse_api_register);
    DTR_ASSERT(tc->con.mouse->visible);
    prv_eval_void(tc, "mouse.hide()");
    DTR_ASSERT(!tc->con.mouse->visible);
    prv_eval_void(tc, "mouse.show()");
    DTR_ASSERT(tc->con.mouse->visible);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_mouse_pos_set(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_mouse_api_register);
    tc->con.mouse->x  = 100;
    tc->con.mouse->y  = 50;
    tc->con.mouse->dx = 5;
    tc->con.mouse->dy = -3;
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.x()"), 100);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.y()"), 50);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.dx()"), 5);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "mouse.dy()"), -3);
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Touch API tests                                                    */
/* ================================================================== */

static void test_touch_count_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_touch_api_register);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "touch.count()"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "touch.MAX_FINGERS"), DTR_MAX_FINGERS);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_touch_active_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_touch_api_register);
    DTR_ASSERT(!prv_eval_bool(tc, "touch.active(0)"));
    DTR_ASSERT(!prv_eval_bool(tc, "touch.pressed(0)"));
    DTR_ASSERT(!prv_eval_bool(tc, "touch.released(0)"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_touch_finger_down(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_touch_api_register);
    dtr_touch_on_down(tc->con.touch, 1, 0.5f, 0.5f, 0.8f);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "touch.count()"), 1);
    DTR_ASSERT(prv_eval_bool(tc, "touch.active(0)"));
    DTR_ASSERT(prv_eval_bool(tc, "touch.pressed(0)"));
    DTR_ASSERT(prv_eval_f64(tc, "touch.pressure(0)") > 0.0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_touch_position(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_touch_api_register);
    /* Normalized 0.5,0.5 maps to center of framebuffer */
    dtr_touch_on_down(tc->con.touch, 1, 0.5f, 0.5f, 1.0f);
    DTR_ASSERT(prv_eval_f64(tc, "touch.x(0)") > 0.0);
    DTR_ASSERT(prv_eval_f64(tc, "touch.y(0)") > 0.0);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_touch_released(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_touch_api_register);
    dtr_touch_on_down(tc->con.touch, 1, 0.5f, 0.5f, 1.0f);
    dtr_touch_update(tc->con.touch);
    dtr_touch_on_up(tc->con.touch, 1);
    DTR_ASSERT(prv_eval_bool(tc, "touch.released(0)"));
    DTR_ASSERT(!prv_eval_bool(tc, "touch.active(0)"));
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Tween API tests                                                    */
/* ================================================================== */

static void test_tween_ease_linear(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    double val = prv_eval_f64(tc, "tween.ease(0.5, 'linear')");
    DTR_ASSERT(fabs(val - 0.5) < 0.01);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_tween_ease_endpoints(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    DTR_ASSERT(fabs(prv_eval_f64(tc, "tween.ease(0, 'linear')")) < 0.01);
    DTR_ASSERT(fabs(prv_eval_f64(tc, "tween.ease(1, 'linear')") - 1.0) < 0.01);
    DTR_ASSERT(fabs(prv_eval_f64(tc, "tween.ease(0, 'inQuad')")) < 0.01);
    DTR_ASSERT(fabs(prv_eval_f64(tc, "tween.ease(1, 'outQuad')") - 1.0) < 0.01);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_tween_sequence_basic(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    /* valid steps → done is a Promise (not undefined) */
    DTR_ASSERT(
        prv_eval_bool(tc,
                      "(function(){"
                      "  var h = tween.sequence([{from:0,to:100,dur:1},{from:100,to:200,dur:1}]);"
                      "  var ok = h.done !== undefined;"
                      "  if (ok) h.done.catch(function(){});"
                      "  tween.seqCancel(h);"
                      "  return ok;"
                      "})()"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_tween_sequence_invalid(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    /* empty array → failure, done is undefined */
    DTR_ASSERT(prv_eval_bool(tc,
                             "(function(){"
                             "  var h = tween.sequence([]);"
                             "  return h.done === undefined;"
                             "})()"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_tween_parallel_basic(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    /* valid steps → done is a Promise (not undefined) */
    DTR_ASSERT(
        prv_eval_bool(tc,
                      "(function(){"
                      "  var h = tween.parallel([{from:0,to:100,dur:1},{from:0,to:50,dur:0.5}]);"
                      "  var ok = h.done !== undefined;"
                      "  if (ok) h.done.catch(function(){});"
                      "  tween.seqCancel(h);"
                      "  return ok;"
                      "})()"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_tween_parallel_empty(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    /* empty array → count<=0 guard fires, done is undefined */
    DTR_ASSERT(prv_eval_bool(tc,
                             "(function(){"
                             "  var h = tween.parallel([]);"
                             "  return h.done === undefined;"
                             "})()"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_tween_seq_val(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    /* seqVal on a live sequence returns a number */
    DTR_ASSERT(prv_eval_bool(tc,
                             "(function(){"
                             "  var h = tween.sequence([{from:0,to:100,dur:1}]);"
                             "  h.done.catch(function(){});"
                             "  var ok = typeof tween.seqVal(h) === 'number';"
                             "  tween.seqCancel(h);"
                             "  return ok;"
                             "})()"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_tween_seq_done_cancel(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_tween_api_register);
    /* seqDone is false before cancel, true after */
    DTR_ASSERT(prv_eval_bool(tc,
                             "(function(){"
                             "  var h = tween.sequence([{from:0,to:100,dur:1}]);"
                             "  h.done.catch(function(){});"
                             "  var before = tween.seqDone(h);"
                             "  tween.seqCancel(h);"
                             "  var after = tween.seqDone(h);"
                             "  return !before && after;"
                             "})()"));
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Synth API tests                                                    */
/* ================================================================== */

static void test_synth_env_set_get(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_synth_api_register);
    DTR_ASSERT(prv_eval_bool(
        tc, "synth.set(0, [{pitch:49,waveform:1,volume:7}], 8, 0, 0, 50, 60, 128, 70)"));
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envAttack"), 50);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envDecay"), 60);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envSustain"), 128);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envRelease"), 70);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_synth_env_zero_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_synth_api_register);
    /* no ADSR args → all env fields default to 0 */
    DTR_ASSERT(prv_eval_bool(tc, "synth.set(0, [{pitch:49,waveform:1,volume:7}])"));
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envAttack"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envDecay"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envSustain"), 0);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "synth.get(0).envRelease"), 0);
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Sys API tests                                                      */
/* ================================================================== */

static void test_sys_dimensions(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_sys_api_register);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "sys.width()"), CONSOLE_FB_WIDTH);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "sys.height()"), CONSOLE_FB_HEIGHT);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_sys_target_fps(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_sys_api_register);
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "sys.targetFps()"), CONSOLE_TARGET_FPS);
    prv_eval_void(tc, "sys.setTargetFps(30)");
    DTR_ASSERT_EQ_INT(prv_eval_i32(tc, "sys.targetFps()"), 30);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_sys_version(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_sys_api_register);
    /* version() should return a non-empty string */
    DTR_ASSERT(prv_eval_bool(tc, "typeof sys.version() === 'string'"));
    DTR_ASSERT(prv_eval_bool(tc, "sys.version().length > 0"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_sys_platform(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_sys_api_register);
    DTR_ASSERT(prv_eval_bool(tc, "typeof sys.platform() === 'string'"));
    DTR_ASSERT(prv_eval_bool(tc, "sys.platform().length > 0"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_sys_pause_resume(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_sys_api_register);
    DTR_ASSERT(!prv_eval_bool(tc, "sys.paused()"));
    prv_eval_void(tc, "sys.pause()");
    DTR_ASSERT(prv_eval_bool(tc, "sys.paused()"));
    prv_eval_void(tc, "sys.resume()");
    DTR_ASSERT(!prv_eval_bool(tc, "sys.paused()"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_sys_time_delta(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_sys_api_register);
    tc->con.time  = 1.5f;
    tc->con.delta = 0.016f;
    DTR_ASSERT(fabs(prv_eval_f64(tc, "sys.time()") - 1.5) < 0.01);
    DTR_ASSERT(fabs(prv_eval_f64(tc, "sys.delta()") - 0.016) < 0.001);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_sys_log_no_crash(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_sys_api_register);
    /* Just assert these don't crash */
    prv_eval_void(tc, "sys.log('test message')");
    prv_eval_void(tc, "sys.warn('test warning')");
    prv_eval_void(tc, "sys.error('test error')");
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Cart API tests                                                     */
/* ================================================================== */

static void test_cart_dset_dget(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_cart_api_register);
    prv_eval_void(tc, "cart.dset(0, 42.5)");
    DTR_ASSERT(fabs(prv_eval_f64(tc, "cart.dget(0)") - 42.5) < 0.01);
    prv_eval_void(tc, "cart.dset(63, -1.0)");
    DTR_ASSERT(fabs(prv_eval_f64(tc, "cart.dget(63)") - (-1.0)) < 0.01);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_cart_dget_default(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_cart_api_register);
    DTR_ASSERT(fabs(prv_eval_f64(tc, "cart.dget(0)")) < 0.01);
    prv_teardown(tc);
    DTR_PASS();
}

static void test_cart_metadata(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_cart_api_register);
    /* Default cart has empty metadata strings */
    DTR_ASSERT(prv_eval_bool(tc, "typeof cart.title() === 'string'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof cart.author() === 'string'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof cart.version() === 'string'"));
    prv_teardown(tc);
    DTR_PASS();
}

static void test_cart_kv_roundtrip(void)
{
    test_ctx_t *tc = prv_setup();

    prv_register(tc, dtr_cart_api_register);
    prv_eval_void(tc, "cart.save('testkey', 'testval')");
    DTR_ASSERT(prv_eval_bool(tc, "cart.has('testkey')"));
    DTR_ASSERT(prv_eval_bool(tc, "cart.load('testkey') === 'testval'"));
    prv_eval_void(tc, "cart.delete('testkey')");
    DTR_ASSERT(!prv_eval_bool(tc, "cart.has('testkey')"));
    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  API registration test                                              */
/* ================================================================== */

static void test_register_all_namespaces(void)
{
    test_ctx_t *tc = prv_setup();

    /* Register the namespaces we can safely use without graphics */
    prv_register(tc, dtr_math_api_register);
    prv_register(tc, dtr_key_api_register);
    prv_register(tc, dtr_evt_api_register);
    prv_register(tc, dtr_input_api_register);
    prv_register(tc, dtr_col_api_register);
    prv_register(tc, dtr_pad_api_register);
    prv_register(tc, dtr_mouse_api_register);
    prv_register(tc, dtr_touch_api_register);
    prv_register(tc, dtr_tween_api_register);
    prv_register(tc, dtr_sys_api_register);
    prv_register(tc, dtr_cart_api_register);

    /* Verify all namespaces are accessible */
    DTR_ASSERT(prv_eval_bool(tc, "typeof math === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof key === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof evt === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof input === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof col === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof pad === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof mouse === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof touch === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof tween === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof sys === 'object'"));
    DTR_ASSERT(prv_eval_bool(tc, "typeof cart === 'object'"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Runner                                                             */
/* ================================================================== */

int main(void)
{
    DTR_TEST_BEGIN("test_api_bridge");

    /* Math API — 22 tests */
    DTR_RUN_TEST(test_math_flr);
    DTR_RUN_TEST(test_math_ceil);
    DTR_RUN_TEST(test_math_round);
    DTR_RUN_TEST(test_math_trunc);
    DTR_RUN_TEST(test_math_abs);
    DTR_RUN_TEST(test_math_sign);
    DTR_RUN_TEST(test_math_min_max);
    DTR_RUN_TEST(test_math_mid);
    DTR_RUN_TEST(test_math_sqrt_pow);
    DTR_RUN_TEST(test_math_trig);
    DTR_RUN_TEST(test_math_inverse_trig);
    DTR_RUN_TEST(test_math_lerp);
    DTR_RUN_TEST(test_math_unlerp);
    DTR_RUN_TEST(test_math_remap);
    DTR_RUN_TEST(test_math_clamp);
    DTR_RUN_TEST(test_math_smoothstep);
    DTR_RUN_TEST(test_math_pingpong);
    DTR_RUN_TEST(test_math_dist);
    DTR_RUN_TEST(test_math_angle);
    DTR_RUN_TEST(test_math_rnd);
    DTR_RUN_TEST(test_math_rnd_int);
    DTR_RUN_TEST(test_math_rnd_range);
    DTR_RUN_TEST(test_math_seed);
    DTR_RUN_TEST(test_math_constants);

    /* Key API — 5 tests */
    DTR_RUN_TEST(test_key_btn_default);
    DTR_RUN_TEST(test_key_btn_after_press);
    DTR_RUN_TEST(test_key_btnp);
    DTR_RUN_TEST(test_key_btnr);
    DTR_RUN_TEST(test_key_name);

    /* Event API — 5 tests */
    DTR_RUN_TEST(test_evt_on_emit);
    DTR_RUN_TEST(test_evt_once);
    DTR_RUN_TEST(test_evt_off);
    DTR_RUN_TEST(test_evt_emit_payload);
    DTR_RUN_TEST(test_evt_invalid_args);

    /* Input API — 6 tests */
    DTR_RUN_TEST(test_input_btn_unmapped);
    DTR_RUN_TEST(test_input_map_btn);
    DTR_RUN_TEST(test_input_btnp);
    DTR_RUN_TEST(test_input_axis);
    DTR_RUN_TEST(test_input_clear);
    DTR_RUN_TEST(test_input_no_args);

    /* Collision API — 5 tests */
    DTR_RUN_TEST(test_col_rect);
    DTR_RUN_TEST(test_col_circ);
    DTR_RUN_TEST(test_col_point_rect);
    DTR_RUN_TEST(test_col_point_circ);
    DTR_RUN_TEST(test_col_circ_rect);

    /* Pad API — 6 tests */
    DTR_RUN_TEST(test_pad_count_and_connected);
    DTR_RUN_TEST(test_pad_btn_default);
    DTR_RUN_TEST(test_pad_axis_default);
    DTR_RUN_TEST(test_pad_name);
    DTR_RUN_TEST(test_pad_deadzone);
    DTR_RUN_TEST(test_pad_rumble);

    /* Mouse API — 5 tests */
    DTR_RUN_TEST(test_mouse_pos_default);
    DTR_RUN_TEST(test_mouse_btn_default);
    DTR_RUN_TEST(test_mouse_btn_press);
    DTR_RUN_TEST(test_mouse_show_hide);
    DTR_RUN_TEST(test_mouse_pos_set);

    /* Touch API — 5 tests */
    DTR_RUN_TEST(test_touch_count_default);
    DTR_RUN_TEST(test_touch_active_default);
    DTR_RUN_TEST(test_touch_finger_down);
    DTR_RUN_TEST(test_touch_position);
    DTR_RUN_TEST(test_touch_released);

    /* Tween API — 8 tests */
    DTR_RUN_TEST(test_tween_ease_linear);
    DTR_RUN_TEST(test_tween_ease_endpoints);
    DTR_RUN_TEST(test_tween_sequence_basic);
    DTR_RUN_TEST(test_tween_sequence_invalid);
    DTR_RUN_TEST(test_tween_parallel_basic);
    DTR_RUN_TEST(test_tween_parallel_empty);
    DTR_RUN_TEST(test_tween_seq_val);
    DTR_RUN_TEST(test_tween_seq_done_cancel);

    /* Synth API — 2 tests */
    DTR_RUN_TEST(test_synth_env_set_get);
    DTR_RUN_TEST(test_synth_env_zero_default);

    /* Sys API — 7 tests */
    DTR_RUN_TEST(test_sys_dimensions);
    DTR_RUN_TEST(test_sys_target_fps);
    DTR_RUN_TEST(test_sys_version);
    DTR_RUN_TEST(test_sys_platform);
    DTR_RUN_TEST(test_sys_pause_resume);
    DTR_RUN_TEST(test_sys_time_delta);
    DTR_RUN_TEST(test_sys_log_no_crash);

    /* Cart API — 4 tests */
    DTR_RUN_TEST(test_cart_dset_dget);
    DTR_RUN_TEST(test_cart_dget_default);
    DTR_RUN_TEST(test_cart_metadata);
    DTR_RUN_TEST(test_cart_kv_roundtrip);

    /* Registration — 1 test */
    DTR_RUN_TEST(test_register_all_namespaces);

    DTR_TEST_END();
}
