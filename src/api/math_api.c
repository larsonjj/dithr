/**
 * \file            math_api.c
 * \brief           JS math extensions — rng, lerp, clamp, trig, distance, angle
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
/*  RNG functions                                                      */
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

static JSValue js_math_rnd_int(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t max;

    (void)this_val;
    max = mvn_api_opt_int(ctx, argc, argv, 0, 1);
    if (max <= 0) {
        return JS_NewInt32(ctx, 0);
    }
    return JS_NewInt32(ctx, (int32_t)(prv_xorshift64() % (uint64_t)max));
}

static JSValue
js_math_rnd_range(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double lo;
    double hi;
    double t;

    (void)this_val;
    lo = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    hi = mvn_api_opt_float(ctx, argc, argv, 1, 1.0);
    t  = (double)(prv_xorshift64() & 0xFFFFFFFF) / 4294967296.0;
    return JS_NewFloat64(ctx, lo + (hi - lo) * t);
}

static JSValue js_math_seed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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

/* ------------------------------------------------------------------ */
/*  Rounding / basic math                                              */
/* ------------------------------------------------------------------ */

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

static JSValue js_math_round(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, round(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_trunc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, trunc(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_abs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, fabs(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
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

static JSValue js_math_sqrt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, sqrt(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_pow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx,
                         pow(mvn_api_opt_float(ctx, argc, argv, 0, 0.0),
                             mvn_api_opt_float(ctx, argc, argv, 1, 1.0)));
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

static JSValue js_math_mid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;
    double c;

    (void)this_val;
    a = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    c = mvn_api_opt_float(ctx, argc, argv, 2, 0.0);

    if ((a >= b && a <= c) || (a <= b && a >= c)) {
        return JS_NewFloat64(ctx, a);
    }
    if ((b >= a && b <= c) || (b <= a && b >= c)) {
        return JS_NewFloat64(ctx, b);
    }
    return JS_NewFloat64(ctx, c);
}

/* ------------------------------------------------------------------ */
/*  Trigonometry (PICO-8 style: 0..1 = full rotation)                  */
/* ------------------------------------------------------------------ */

#define MVN_PI  3.14159265358979323846
#define MVN_TAU 6.28318530717958647692

static JSValue js_math_sin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;

    (void)this_val;
    t = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    return JS_NewFloat64(ctx, -sin(t * MVN_TAU));
}

static JSValue js_math_cos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;

    (void)this_val;
    t = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    return JS_NewFloat64(ctx, cos(t * MVN_TAU));
}

static JSValue js_math_tan(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;

    (void)this_val;
    t = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    return JS_NewFloat64(ctx, tan(t * MVN_TAU));
}

static JSValue js_math_asin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, asin(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_acos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, acos(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_atan(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, atan(mvn_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_atan2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double dy;
    double dx;
    double a;

    (void)this_val;
    dy = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    dx = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);

    /* Return in 0..1 range (full rotation), PICO-8 convention */
    a = atan2(-dy, dx) / MVN_TAU;
    if (a < 0.0) {
        a += 1.0;
    }
    return JS_NewFloat64(ctx, a);
}

/* ------------------------------------------------------------------ */
/*  Interpolation                                                      */
/* ------------------------------------------------------------------ */

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

static JSValue js_math_unlerp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;
    double v;
    double range;

    (void)this_val;
    a     = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    b     = mvn_api_opt_float(ctx, argc, argv, 1, 1.0);
    v     = mvn_api_opt_float(ctx, argc, argv, 2, 0.0);
    range = b - a;
    if (range == 0.0) {
        return JS_NewFloat64(ctx, 0.0);
    }
    return JS_NewFloat64(ctx, (v - a) / range);
}

static JSValue js_math_remap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double v;
    double a;
    double b;
    double c;
    double d;
    double t;
    double range;

    (void)this_val;
    v     = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    a     = mvn_api_opt_float(ctx, argc, argv, 1, 0.0);
    b     = mvn_api_opt_float(ctx, argc, argv, 2, 1.0);
    c     = mvn_api_opt_float(ctx, argc, argv, 3, 0.0);
    d     = mvn_api_opt_float(ctx, argc, argv, 4, 1.0);
    range = b - a;
    if (range == 0.0) {
        return JS_NewFloat64(ctx, c);
    }
    t = (v - a) / range;
    return JS_NewFloat64(ctx, c + (d - c) * t);
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

static JSValue
js_math_smoothstep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;
    double t;
    double x;

    (void)this_val;
    a = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = mvn_api_opt_float(ctx, argc, argv, 1, 1.0);
    t = mvn_api_opt_float(ctx, argc, argv, 2, 0.0);

    /* Normalise t to 0..1 within [a,b] */
    if (b - a == 0.0) {
        return JS_NewFloat64(ctx, 0.0);
    }
    x = (t - a) / (b - a);
    if (x < 0.0) {
        x = 0.0;
    }
    if (x > 1.0) {
        x = 1.0;
    }
    return JS_NewFloat64(ctx, x * x * (3.0 - 2.0 * x));
}

static JSValue
js_math_pingpong(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;
    double len;
    double mod_val;

    (void)this_val;
    t   = mvn_api_opt_float(ctx, argc, argv, 0, 0.0);
    len = mvn_api_opt_float(ctx, argc, argv, 1, 1.0);

    if (len <= 0.0) {
        return JS_NewFloat64(ctx, 0.0);
    }
    mod_val = fmod(t, len * 2.0);
    if (mod_val < 0.0) {
        mod_val += len * 2.0;
    }
    if (mod_val > len) {
        mod_val = len * 2.0 - mod_val;
    }
    return JS_NewFloat64(ctx, mod_val);
}

/* ------------------------------------------------------------------ */
/*  Distance / angle (convenience)                                     */
/* ------------------------------------------------------------------ */

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

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_math_funcs[] = {
    JS_CFUNC_DEF("flr", 1, js_math_flr),
    JS_CFUNC_DEF("ceil", 1, js_math_ceil),
    JS_CFUNC_DEF("round", 1, js_math_round),
    JS_CFUNC_DEF("trunc", 1, js_math_trunc),
    JS_CFUNC_DEF("abs", 1, js_math_abs),
    JS_CFUNC_DEF("sign", 1, js_math_sign),
    JS_CFUNC_DEF("min", 2, js_math_min),
    JS_CFUNC_DEF("max", 2, js_math_max),
    JS_CFUNC_DEF("mid", 3, js_math_mid),
    JS_CFUNC_DEF("sqrt", 1, js_math_sqrt),
    JS_CFUNC_DEF("pow", 2, js_math_pow),
    JS_CFUNC_DEF("sin", 1, js_math_sin),
    JS_CFUNC_DEF("cos", 1, js_math_cos),
    JS_CFUNC_DEF("tan", 1, js_math_tan),
    JS_CFUNC_DEF("asin", 1, js_math_asin),
    JS_CFUNC_DEF("acos", 1, js_math_acos),
    JS_CFUNC_DEF("atan", 1, js_math_atan),
    JS_CFUNC_DEF("atan2", 2, js_math_atan2),
    JS_CFUNC_DEF("lerp", 3, js_math_lerp),
    JS_CFUNC_DEF("unlerp", 3, js_math_unlerp),
    JS_CFUNC_DEF("remap", 5, js_math_remap),
    JS_CFUNC_DEF("clamp", 3, js_math_clamp),
    JS_CFUNC_DEF("smoothstep", 3, js_math_smoothstep),
    JS_CFUNC_DEF("pingpong", 2, js_math_pingpong),
    JS_CFUNC_DEF("rnd", 1, js_math_rnd),
    JS_CFUNC_DEF("rnd_int", 1, js_math_rnd_int),
    JS_CFUNC_DEF("rnd_range", 2, js_math_rnd_range),
    JS_CFUNC_DEF("seed", 1, js_math_seed),
    JS_CFUNC_DEF("dist", 4, js_math_dist),
    JS_CFUNC_DEF("angle", 4, js_math_angle),
};

void mvn_math_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_math_funcs, countof(js_math_funcs));

    JS_SetPropertyStr(ctx, ns, "PI", JS_NewFloat64(ctx, MVN_PI));
    JS_SetPropertyStr(ctx, ns, "TAU", JS_NewFloat64(ctx, MVN_TAU));
    JS_SetPropertyStr(ctx, ns, "HUGE", JS_NewFloat64(ctx, 1e308));

    JS_SetPropertyStr(ctx, global, "math", ns);
}
