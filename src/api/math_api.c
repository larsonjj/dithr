/**
 * \file            math_api.c
 * \brief           JS math extensions — rng, lerp, clamp, trig, distance, angle
 */

#include "api_common.h"

/* ------------------------------------------------------------------ */
/*  PRNG — xorshift64                                                  */
/* ------------------------------------------------------------------ */

static uint64_t prv_xorshift64(dtr_console_t *con)
{
    uint64_t x;

    x = con->rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    con->rng_state = x;
    return x;
}

/* ------------------------------------------------------------------ */
/*  RNG functions                                                      */
/* ------------------------------------------------------------------ */

static JSValue js_math_rnd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double max;

    (void)this_val;
    max = dtr_api_opt_float(ctx, argc, argv, 0, 1.0);

    {
        double val;

        val = (double)(prv_xorshift64(dtr_api_get_console(ctx)) & 0xFFFFFFFF) / 4294967296.0;
        return JS_NewFloat64(ctx, val * max);
    }
}

static JSValue js_math_rnd_int(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t max;

    (void)this_val;
    max = dtr_api_opt_int(ctx, argc, argv, 0, 1);
    if (max <= 0) {
        return JS_NewInt32(ctx, 0);
    }
    return JS_NewInt32(ctx, (int32_t)(prv_xorshift64(dtr_api_get_console(ctx)) % (uint64_t)max));
}

static JSValue
js_math_rnd_range(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double lo;
    double hi;
    double t;

    (void)this_val;
    lo = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    hi = dtr_api_opt_float(ctx, argc, argv, 1, 1.0);
    t  = (double)(prv_xorshift64(dtr_api_get_console(ctx)) & 0xFFFFFFFF) / 4294967296.0;
    return JS_NewFloat64(ctx, lo + (hi - lo) * t);
}

static JSValue js_math_seed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t        seed;
    dtr_console_t *con;

    (void)this_val;
    seed           = dtr_api_opt_int(ctx, argc, argv, 0, 1);
    con            = dtr_api_get_console(ctx);
    con->rng_state = (uint64_t)seed;
    if (con->rng_state == 0) {
        con->rng_state = 1;
    }
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  Rounding / basic math                                              */
/* ------------------------------------------------------------------ */

static JSValue js_math_flr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, floor(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_ceil(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, ceil(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_round(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, round(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_trunc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, trunc(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_abs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, fabs(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_sign(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double val;

    (void)this_val;
    val = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
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
    return JS_NewFloat64(ctx, sqrt(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_pow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx,
                         pow(dtr_api_opt_float(ctx, argc, argv, 0, 0.0),
                             dtr_api_opt_float(ctx, argc, argv, 1, 1.0)));
}

static JSValue js_math_min(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;

    (void)this_val;
    a = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    return JS_NewFloat64(ctx, (a < b) ? a : b);
}

static JSValue js_math_max(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;

    (void)this_val;
    a = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    return JS_NewFloat64(ctx, (a > b) ? a : b);
}

static JSValue js_math_mid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;
    double c;

    (void)this_val;
    a = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    c = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);

    if ((a >= b && a <= c) || (a <= b && a >= c)) {
        return JS_NewFloat64(ctx, a);
    }
    if ((b >= a && b <= c) || (b <= a && b >= c)) {
        return JS_NewFloat64(ctx, b);
    }
    return JS_NewFloat64(ctx, c);
}

/* ------------------------------------------------------------------ */
/*  Trigonometry (0..1 = full rotation)                                 */
/* ------------------------------------------------------------------ */

#define DTR_PI  3.14159265358979323846
#define DTR_TAU 6.28318530717958647692

static JSValue js_math_sin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;

    (void)this_val;
    t = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    return JS_NewFloat64(ctx, -sin(t * DTR_TAU));
}

static JSValue js_math_cos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;

    (void)this_val;
    t = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    return JS_NewFloat64(ctx, cos(t * DTR_TAU));
}

static JSValue js_math_tan(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;

    (void)this_val;
    t = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    return JS_NewFloat64(ctx, tan(t * DTR_TAU));
}

static JSValue js_math_asin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, asin(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_acos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, acos(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_atan(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, atan(dtr_api_opt_float(ctx, argc, argv, 0, 0.0)));
}

static JSValue js_math_atan2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double dy;
    double dx;
    double a;

    (void)this_val;
    dy = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    dx = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);

    /* Return in 0..1 range (full rotation) */
    a = atan2(-dy, dx) / DTR_TAU;
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
    a = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    t = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    return JS_NewFloat64(ctx, a + (b - a) * t);
}

static JSValue js_math_unlerp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double a;
    double b;
    double v;
    double range;

    (void)this_val;
    a     = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    b     = dtr_api_opt_float(ctx, argc, argv, 1, 1.0);
    v     = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
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
    v     = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    a     = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    b     = dtr_api_opt_float(ctx, argc, argv, 2, 1.0);
    c     = dtr_api_opt_float(ctx, argc, argv, 3, 0.0);
    d     = dtr_api_opt_float(ctx, argc, argv, 4, 1.0);
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
    val = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    lo  = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    hi  = dtr_api_opt_float(ctx, argc, argv, 2, 1.0);
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
    a = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    b = dtr_api_opt_float(ctx, argc, argv, 1, 1.0);
    t = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);

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

static JSValue js_math_pingpong(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double t;
    double len;
    double mod_val;

    (void)this_val;
    t   = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    len = dtr_api_opt_float(ctx, argc, argv, 1, 1.0);

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
    x1 = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    y1 = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    x2 = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    y2 = dtr_api_opt_float(ctx, argc, argv, 3, 0.0);
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
    x1 = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    y1 = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    x2 = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    y2 = dtr_api_opt_float(ctx, argc, argv, 3, 0.0);
    return JS_NewFloat64(ctx, atan2(y2 - y1, x2 - x1));
}

/* ------------------------------------------------------------------ */
/*  Perlin noise (Ken Perlin's "improved noise", public-domain)        */
/* ------------------------------------------------------------------ */

/* Reference permutation table (256 entries, doubled to 512 for wrap). */
static const uint8_t prv_perlin_perm[512] = {
    151,
    160,
    137,
    91,
    90,
    15,
    131,
    13,
    201,
    95,
    96,
    53,
    194,
    233,
    7,
    225,
    140,
    36,
    103,
    30,
    69,
    142,
    8,
    99,
    37,
    240,
    21,
    10,
    23,
    190,
    6,
    148,
    247,
    120,
    234,
    75,
    0,
    26,
    197,
    62,
    94,
    252,
    219,
    203,
    117,
    35,
    11,
    32,
    57,
    177,
    33,
    88,
    237,
    149,
    56,
    87,
    174,
    20,
    125,
    136,
    171,
    168,
    68,
    175,
    74,
    165,
    71,
    134,
    139,
    48,
    27,
    166,
    77,
    146,
    158,
    231,
    83,
    111,
    229,
    122,
    60,
    211,
    133,
    230,
    220,
    105,
    92,
    41,
    55,
    46,
    245,
    40,
    244,
    102,
    143,
    54,
    65,
    25,
    63,
    161,
    1,
    216,
    80,
    73,
    209,
    76,
    132,
    187,
    208,
    89,
    18,
    169,
    200,
    196,
    135,
    130,
    116,
    188,
    159,
    86,
    164,
    100,
    109,
    198,
    173,
    186,
    3,
    64,
    52,
    217,
    226,
    250,
    124,
    123,
    5,
    202,
    38,
    147,
    118,
    126,
    255,
    82,
    85,
    212,
    207,
    206,
    59,
    227,
    47,
    16,
    58,
    17,
    182,
    189,
    28,
    42,
    223,
    183,
    170,
    213,
    119,
    248,
    152,
    2,
    44,
    154,
    163,
    70,
    221,
    153,
    101,
    155,
    167,
    43,
    172,
    9,
    129,
    22,
    39,
    253,
    19,
    98,
    108,
    110,
    79,
    113,
    224,
    232,
    178,
    185,
    112,
    104,
    218,
    246,
    97,
    228,
    251,
    34,
    242,
    193,
    238,
    210,
    144,
    12,
    191,
    179,
    162,
    241,
    81,
    51,
    145,
    235,
    249,
    14,
    239,
    107,
    49,
    192,
    214,
    31,
    181,
    199,
    106,
    157,
    184,
    84,
    204,
    176,
    115,
    121,
    50,
    45,
    127,
    4,
    150,
    254,
    138,
    236,
    205,
    93,
    222,
    114,
    67,
    29,
    24,
    72,
    243,
    141,
    128,
    195,
    78,
    66,
    215,
    61,
    156,
    180,
    /* duplicated */
    151,
    160,
    137,
    91,
    90,
    15,
    131,
    13,
    201,
    95,
    96,
    53,
    194,
    233,
    7,
    225,
    140,
    36,
    103,
    30,
    69,
    142,
    8,
    99,
    37,
    240,
    21,
    10,
    23,
    190,
    6,
    148,
    247,
    120,
    234,
    75,
    0,
    26,
    197,
    62,
    94,
    252,
    219,
    203,
    117,
    35,
    11,
    32,
    57,
    177,
    33,
    88,
    237,
    149,
    56,
    87,
    174,
    20,
    125,
    136,
    171,
    168,
    68,
    175,
    74,
    165,
    71,
    134,
    139,
    48,
    27,
    166,
    77,
    146,
    158,
    231,
    83,
    111,
    229,
    122,
    60,
    211,
    133,
    230,
    220,
    105,
    92,
    41,
    55,
    46,
    245,
    40,
    244,
    102,
    143,
    54,
    65,
    25,
    63,
    161,
    1,
    216,
    80,
    73,
    209,
    76,
    132,
    187,
    208,
    89,
    18,
    169,
    200,
    196,
    135,
    130,
    116,
    188,
    159,
    86,
    164,
    100,
    109,
    198,
    173,
    186,
    3,
    64,
    52,
    217,
    226,
    250,
    124,
    123,
    5,
    202,
    38,
    147,
    118,
    126,
    255,
    82,
    85,
    212,
    207,
    206,
    59,
    227,
    47,
    16,
    58,
    17,
    182,
    189,
    28,
    42,
    223,
    183,
    170,
    213,
    119,
    248,
    152,
    2,
    44,
    154,
    163,
    70,
    221,
    153,
    101,
    155,
    167,
    43,
    172,
    9,
    129,
    22,
    39,
    253,
    19,
    98,
    108,
    110,
    79,
    113,
    224,
    232,
    178,
    185,
    112,
    104,
    218,
    246,
    97,
    228,
    251,
    34,
    242,
    193,
    238,
    210,
    144,
    12,
    191,
    179,
    162,
    241,
    81,
    51,
    145,
    235,
    249,
    14,
    239,
    107,
    49,
    192,
    214,
    31,
    181,
    199,
    106,
    157,
    184,
    84,
    204,
    176,
    115,
    121,
    50,
    45,
    127,
    4,
    150,
    254,
    138,
    236,
    205,
    93,
    222,
    114,
    67,
    29,
    24,
    72,
    243,
    141,
    128,
    195,
    78,
    66,
    215,
    61,
    156,
    180,
};

static inline double prv_perlin_fade(double t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static inline double prv_perlin_grad(int32_t hash, double x, double y, double z)
{
    int32_t h;
    double  u;
    double  v;

    h = hash & 15;
    u = (h < 8) ? x : y;
    v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

static double prv_perlin3(double x, double y, double z)
{
    int32_t xi;
    int32_t yi;
    int32_t zi;
    double  xf;
    double  yf;
    double  zf;
    double  u;
    double  v;
    double  w;
    int32_t a;
    int32_t aa;
    int32_t ab;
    int32_t b;
    int32_t ba;
    int32_t bb;
    double  x1;
    double  x2;
    double  y1;
    double  y2;

    xi = (int32_t)floor(x) & 255;
    yi = (int32_t)floor(y) & 255;
    zi = (int32_t)floor(z) & 255;

    xf = x - floor(x);
    yf = y - floor(y);
    zf = z - floor(z);

    u = prv_perlin_fade(xf);
    v = prv_perlin_fade(yf);
    w = prv_perlin_fade(zf);

    a  = prv_perlin_perm[xi] + yi;
    aa = prv_perlin_perm[a & 255] + zi;
    ab = prv_perlin_perm[(a + 1) & 255] + zi;
    b  = prv_perlin_perm[(xi + 1) & 255] + yi;
    ba = prv_perlin_perm[b & 255] + zi;
    bb = prv_perlin_perm[(b + 1) & 255] + zi;

    x1 = prv_perlin_grad(prv_perlin_perm[aa & 255], xf, yf, zf) +
         (prv_perlin_grad(prv_perlin_perm[ba & 255], xf - 1.0, yf, zf) -
          prv_perlin_grad(prv_perlin_perm[aa & 255], xf, yf, zf)) *
             u;
    x2 = prv_perlin_grad(prv_perlin_perm[ab & 255], xf, yf - 1.0, zf) +
         (prv_perlin_grad(prv_perlin_perm[bb & 255], xf - 1.0, yf - 1.0, zf) -
          prv_perlin_grad(prv_perlin_perm[ab & 255], xf, yf - 1.0, zf)) *
             u;
    y1 = x1 + (x2 - x1) * v;

    x1 = prv_perlin_grad(prv_perlin_perm[(aa + 1) & 255], xf, yf, zf - 1.0) +
         (prv_perlin_grad(prv_perlin_perm[(ba + 1) & 255], xf - 1.0, yf, zf - 1.0) -
          prv_perlin_grad(prv_perlin_perm[(aa + 1) & 255], xf, yf, zf - 1.0)) *
             u;
    x2 = prv_perlin_grad(prv_perlin_perm[(ab + 1) & 255], xf, yf - 1.0, zf - 1.0) +
         (prv_perlin_grad(prv_perlin_perm[(bb + 1) & 255], xf - 1.0, yf - 1.0, zf - 1.0) -
          prv_perlin_grad(prv_perlin_perm[(ab + 1) & 255], xf, yf - 1.0, zf - 1.0)) *
             u;
    y2 = x1 + (x2 - x1) * v;

    return y1 + (y2 - y1) * w;
}

static JSValue js_math_noise2d(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double x;
    double y;

    (void)this_val;
    x = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    y = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    return JS_NewFloat64(ctx, prv_perlin3(x, y, 0.0));
}

static JSValue js_math_noise3d(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double x;
    double y;
    double z;

    (void)this_val;
    x = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    y = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    z = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    return JS_NewFloat64(ctx, prv_perlin3(x, y, z));
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
    JS_CFUNC_DEF("rndInt", 1, js_math_rnd_int),
    JS_CFUNC_DEF("rndRange", 2, js_math_rnd_range),
    JS_CFUNC_DEF("seed", 1, js_math_seed),
    JS_CFUNC_DEF("dist", 4, js_math_dist),
    JS_CFUNC_DEF("angle", 4, js_math_angle),
    JS_CFUNC_DEF("noise2d", 2, js_math_noise2d),
    JS_CFUNC_DEF("noise3d", 3, js_math_noise3d),
};

void dtr_math_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_math_funcs, countof(js_math_funcs));

    JS_SetPropertyStr(ctx, ns, "PI", JS_NewFloat64(ctx, DTR_PI));
    JS_SetPropertyStr(ctx, ns, "TAU", JS_NewFloat64(ctx, DTR_TAU));
    JS_SetPropertyStr(ctx, ns, "HUGE", JS_NewFloat64(ctx, 1e308));

    JS_SetPropertyStr(ctx, global, "math", ns);
}
