/**
 * \file            math_api.c
 * \brief           JS math extensions — rng, lerp, clamp, distance, angle
 */

#include "api_common.h"

/* ------------------------------------------------------------------ */
/*  PRNG — xorshift64                                                  */
/* ------------------------------------------------------------------ */

static uint64_t prv_rng_state = 1;

static uint64_t prv_xorshift64(void)
{
    uint64_t x;

    x = prv_rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    prv_rng_state = x;
    return x;
}

/* ------------------------------------------------------------------ */
/*  JS functions                                                       */
/* ------------------------------------------------------------------ */

static JSValue js_math_rnd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double max;

    (void)this_val;
    max = mvn_api_opt_float(ctx, argc, argv, 0, 1.0);

    {
        double val;

        val = (double)(prv_xorshift64() & 0xFFFFFFFF) / 4294967296.0;
        return JS_NewFloat64(ctx, val * max);
    }
}

static JSValue js_math_srand(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t seed;

    (void)this_val;
    seed          = mvn_api_opt_int(ctx, argc, argv, 0, 1);
    prv_rng_state = (uint64_t)seed;
    if (prv_rng_state == 0) {
        prv_rng_state = 1;
    }
    return JS_UNDEFINED;
}

static JSValue js_math_flr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, floor(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_ceil(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, ceil(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_mid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;
    double c;

    (void)this_val;
    a = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    c = mvn_api_opt_float(ctx, argc, argv, 2, 0.0);

    /* Return the middle value */
    if ((a >= b && a <= c) || (a <= b && a >= c)) {
        return JS_NewFloat64(ctx, a);
    }
    if ((b >= a && b <= c) || (b <= a && b >= c)) {
        return JS_NewFloat64(ctx, b);
    }
    return JS_NewFloat64(ctx, c);
}

static JSValue js_math_lerp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;
    double t;

    (void)this_val;
    a = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    t = mvn_api_opt_float(ctx, argc, argv, 2, 0.0);

    return JS_NewFloat64(ctx, a + (b - a) * t);
}

static JSValue js_math_clamp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double val;
    double lo;
    double hi;

    (void)this_val;
    val = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    lo  = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    hi  = mvn_api_opt_float(ctx, argc, argv, 2, 1.0);

    if (val < lo) {
        val = lo;
    }
    if (val > hi) {
        val = hi;
    }
    return JS_NewFloat64(ctx, val);
}

static JSValue js_math_dist(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double x1;
    double y1;
    double x2;
    double y2;
    double dx;
    double dy;

    (void)this_val;
    x1 = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    y1 = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    x2 = mvn_api_opt_float(ctx, argc, argv, 2, 0.0);
    y2 = mvn_api_opt_float(ctx, argc, argv, 3, 0.0);

    dx = x2 - x1;
    dy = y2 - y1;
    return JS_NewFloat64(ctx, sqrt(dx * dx + dy * dy));
}

static JSValue js_math_angle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double x1;
    double y1;
    double x2;
    double y2;

    (void)this_val;
    x1 = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    y1 = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    x2 = mvn_api_opt_float(ctx, argc, argv, 2, 0.0);
    y2 = mvn_api_opt_float(ctx, argc, argv, 3, 0.0);

    return JS_NewFloat64(ctx, atan2(y2 - y1, x2 - x1));
}

static JSValue js_math_sign(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double val;

    (void)this_val;
    val = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);

    if (val > 0.0) {
        return JS_NewFloat64(ctx, 1.0);
    }
    if (val < 0.0) {
        return JS_NewFloat64(ctx, -1.0);
    }
    return JS_NewFloat64(ctx, 0.0);
}

static JSValue js_math_sin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    /* PICO-8 style: input 0..1 = full rotation, inverted */
    {
        double t;

        t = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
        return JS_NewFloat64(ctx, -sin(t * 2.0 * 3.14159265358979323846));
    }
}

static JSValue js_math_cos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    {
        double t;

        t = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
        return JS_NewFloat64(ctx, cos(t * 2.0 * 3.14159265358979323846));
    }
}

static JSValue js_math_atan2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double dx;
    double dy;
    double a;

    (void)this_val;
    dx = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    dy = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);

    /* Return in 0..1 range (full rotation) */
    a = atan2(-dy, dx) / (2.0 * 3.14159265358979323846);
    if (a < 0.0) {
        a += 1.0;
    }
    return JS_NewFloat64(ctx, a);
}

static JSValue js_math_abs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, fabs(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_min(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;

    (void)this_val;
    a = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    return JS_NewFloat64(ctx, (a < b) ? a : b);
}

static JSValue js_math_max(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;

    (void)this_val;
    a = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    return JS_NewFloat64(ctx, (a > b) ? a : b);
}

static const JSCFunctionListEntry js_math_funcs[] = {
    JS_CFUNC_DEF("rnd", 1, js_math_rnd),
    JS_CFUNC_DEF("srand", 1, js_math_srand),
    JS_CFUNC_DEF("flr", 1, js_math_flr),
    JS_CFUNC_DEF("ceil", 1, js_math_ceil),
    JS_CFUNC_DEF("mid", 3, js_math_mid),
    JS_CFUNC_DEF("lerp", 3, js_math_lerp),
    JS_CFUNC_DEF("clamp", 3, js_math_clamp),
    JS_CFUNC_DEF("dist", 4, js_math_dist),
    JS_CFUNC_DEF("angle", 4, js_math_angle),
    JS_CFUNC_DEF("sign", 1, js_math_sign),
    JS_CFUNC_DEF("sin", 1, js_math_sin),
    JS_CFUNC_DEF("cos", 1, js_math_cos),
    JS_CFUNC_DEF("atan2", 2, js_math_atan2),
    JS_CFUNC_DEF("abs", 1, js_math_abs),
    JS_CFUNC_DEF("min", 2, js_math_min),
    JS_CFUNC_DEF("max", 2, js_math_max),
};

void mvn_math_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_math_funcs, countof(js_math_funcs));

    /* Common constants */
    JS_SetPropertyStr(ctx, ns, "PI", JS_NewFloat64(ctx, 3.14159265358979323846));
    JS_SetPropertyStr(ctx, ns, "TAU", JS_NewFloat64(ctx, 6.28318530717958647692));

    JS_SetPropertyStr(ctx, global, "math", ns);
}
