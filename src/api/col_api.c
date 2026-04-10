/**
 * \file            col_api.c
 * \brief           JS collision helpers — rect, circle, point tests
 */

#include "api_common.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/*  Rect vs rect (AABB)                                                */
/* ------------------------------------------------------------------ */

static JSValue js_col_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t x1;
    int32_t y1;
    int32_t w1;
    int32_t h1;
    int32_t x2;
    int32_t y2;
    int32_t w2;
    int32_t h2;

    (void)this_val;
    x1 = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    y1 = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    w1 = dtr_api_opt_int(ctx, argc, argv, 2, 0);
    h1 = dtr_api_opt_int(ctx, argc, argv, 3, 0);
    x2 = dtr_api_opt_int(ctx, argc, argv, 4, 0);
    y2 = dtr_api_opt_int(ctx, argc, argv, 5, 0);
    w2 = dtr_api_opt_int(ctx, argc, argv, 6, 0);
    h2 = dtr_api_opt_int(ctx, argc, argv, 7, 0);

    if (w1 <= 0 || h1 <= 0 || w2 <= 0 || h2 <= 0) {
        return JS_FALSE;
    }

    if (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2) {
        return JS_TRUE;
    }
    return JS_FALSE;
}

/* ------------------------------------------------------------------ */
/*  Circle vs circle                                                   */
/* ------------------------------------------------------------------ */

static JSValue js_col_circ(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double x1;
    double y1;
    double r1;
    double x2;
    double y2;
    double r2;
    double dx;
    double dy;
    double dist_sq;
    double rad_sum;

    (void)this_val;
    x1 = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    y1 = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    r1 = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    x2 = dtr_api_opt_float(ctx, argc, argv, 3, 0.0);
    y2 = dtr_api_opt_float(ctx, argc, argv, 4, 0.0);
    r2 = dtr_api_opt_float(ctx, argc, argv, 5, 0.0);

    if (r1 < 0.0 || r2 < 0.0) {
        return JS_FALSE;
    }

    dx      = x2 - x1;
    dy      = y2 - y1;
    dist_sq = dx * dx + dy * dy;
    rad_sum = r1 + r2;

    if (dist_sq <= rad_sum * rad_sum) {
        return JS_TRUE;
    }
    return JS_FALSE;
}

/* ------------------------------------------------------------------ */
/*  Point inside rect                                                  */
/* ------------------------------------------------------------------ */

static JSValue
js_col_point_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t px;
    int32_t py;
    int32_t rx;
    int32_t ry;
    int32_t rw;
    int32_t rh;

    (void)this_val;
    px = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    py = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    rx = dtr_api_opt_int(ctx, argc, argv, 2, 0);
    ry = dtr_api_opt_int(ctx, argc, argv, 3, 0);
    rw = dtr_api_opt_int(ctx, argc, argv, 4, 0);
    rh = dtr_api_opt_int(ctx, argc, argv, 5, 0);

    if (rw <= 0 || rh <= 0) {
        return JS_FALSE;
    }

    if (px >= rx && px < rx + rw && py >= ry && py < ry + rh) {
        return JS_TRUE;
    }
    return JS_FALSE;
}

/* ------------------------------------------------------------------ */
/*  Point inside circle                                                */
/* ------------------------------------------------------------------ */

static JSValue
js_col_point_circ(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double px;
    double py;
    double cx;
    double cy;
    double r;
    double dx;
    double dy;

    (void)this_val;
    px = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    py = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    cx = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    cy = dtr_api_opt_float(ctx, argc, argv, 3, 0.0);
    r  = dtr_api_opt_float(ctx, argc, argv, 4, 0.0);

    if (r < 0.0) {
        return JS_FALSE;
    }

    dx = px - cx;
    dy = py - cy;
    if (dx * dx + dy * dy <= r * r) {
        return JS_TRUE;
    }
    return JS_FALSE;
}

/* ------------------------------------------------------------------ */
/*  Circle vs rect                                                     */
/* ------------------------------------------------------------------ */

static JSValue js_col_circ_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double cx;
    double cy;
    double cr;
    double rx;
    double ry;
    double rw;
    double rh;
    double nearest_x;
    double nearest_y;
    double dx;
    double dy;

    (void)this_val;
    cx = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    cy = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    cr = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    rx = dtr_api_opt_float(ctx, argc, argv, 3, 0.0);
    ry = dtr_api_opt_float(ctx, argc, argv, 4, 0.0);
    rw = dtr_api_opt_float(ctx, argc, argv, 5, 0.0);
    rh = dtr_api_opt_float(ctx, argc, argv, 6, 0.0);

    if (cr < 0.0 || rw <= 0.0 || rh <= 0.0) {
        return JS_FALSE;
    }

    /* Clamp circle center to rect to find nearest point */
    nearest_x = cx;
    if (nearest_x < rx) {
        nearest_x = rx;
    }
    if (nearest_x > rx + rw) {
        nearest_x = rx + rw;
    }
    nearest_y = cy;
    if (nearest_y < ry) {
        nearest_y = ry;
    }
    if (nearest_y > ry + rh) {
        nearest_y = ry + rh;
    }

    dx = cx - nearest_x;
    dy = cy - nearest_y;
    if (dx * dx + dy * dy <= cr * cr) {
        return JS_TRUE;
    }
    return JS_FALSE;
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_col_funcs[] = {
    JS_CFUNC_DEF("rect", 8, js_col_rect),
    JS_CFUNC_DEF("circ", 6, js_col_circ),
    JS_CFUNC_DEF("pointRect", 6, js_col_point_rect),
    JS_CFUNC_DEF("pointCirc", 5, js_col_point_circ),
    JS_CFUNC_DEF("circRect", 7, js_col_circ_rect),
};

void dtr_col_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_col_funcs, countof(js_col_funcs));
    JS_SetPropertyStr(ctx, global, "col", ns);
}
