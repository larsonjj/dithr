/**
 * \file            particles_api.c
 * \brief           JS bindings for the particle system (`particles.*`)
 */

#include "../graphics.h"
#include "../particles.h"
#include "api_common.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static int32_t prv_obj_int(JSContext *ctx, JSValueConst obj, const char *key, int32_t dflt)
{
    JSValue v;
    int32_t out;

    v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return dflt;
    }
    if (JS_ToInt32(ctx, &out, v) != 0) {
        out = dflt;
    }
    JS_FreeValue(ctx, v);
    return out;
}

static double prv_obj_float(JSContext *ctx, JSValueConst obj, const char *key, double dflt)
{
    JSValue v;
    double  out;

    v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return dflt;
    }
    if (JS_ToFloat64(ctx, &out, v) != 0) {
        out = dflt;
    }
    JS_FreeValue(ctx, v);
    return out;
}

/* ------------------------------------------------------------------ */
/*  particles.emit(cfg)                                                */
/*                                                                     */
/* cfg fields:                                                         */
/*   x, y          (number) origin (default 0)                         */
/*   vx, vy        (number) initial velocity                           */
/*   ax, ay        (number) constant acceleration                      */
/*   drag          (number) per-second velocity decay 0..1             */
/*   life          (number) seconds (default 1)                        */
/*   color         (int)    palette index (sets color0=color1)         */
/*   color1        (int)    optional fade-to color                     */
/*   size          (int)    pixels (default 1)                         */
/*   kind          (int)    0=pixel, 1=rect, 2=circle                  */
/*   count         (int)    burst size (default 1)                     */
/*   spread        (number) cone half-angle in radians around (vx,vy)  */
/*   speedMin/Max  (number) randomise speed when count>1               */
/*                                                                     */
/* Returns:  number of particles actually spawned (may be < count when */
/*           the pool is full).                                        */
/* ------------------------------------------------------------------ */

static JSValue js_p_emit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t       *con;
    dtr_particle_spawn_t spawn;
    int32_t              count;
    double               base_vx;
    double               base_vy;
    double               base_speed;
    double               base_angle;
    double               spread;
    double               speed_min;
    double               speed_max;
    int32_t              spawned;
    int32_t              i;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con->particles == NULL || argc < 1 || !JS_IsObject(argv[0])) {
        return JS_NewInt32(ctx, 0);
    }

    spawn.x      = (float)prv_obj_float(ctx, argv[0], "x", 0.0);
    spawn.y      = (float)prv_obj_float(ctx, argv[0], "y", 0.0);
    base_vx      = prv_obj_float(ctx, argv[0], "vx", 0.0);
    base_vy      = prv_obj_float(ctx, argv[0], "vy", 0.0);
    spawn.ax     = (float)prv_obj_float(ctx, argv[0], "ax", 0.0);
    spawn.ay     = (float)prv_obj_float(ctx, argv[0], "ay", 0.0);
    spawn.drag   = (float)prv_obj_float(ctx, argv[0], "drag", 0.0);
    spawn.life   = (float)prv_obj_float(ctx, argv[0], "life", 1.0);
    spawn.color0 = (uint8_t)(prv_obj_int(ctx, argv[0], "color", 7) & 0xFF);
    spawn.color1 = (uint8_t)(prv_obj_int(ctx, argv[0], "color1", spawn.color0) & 0xFF);
    spawn.size   = (uint8_t)(prv_obj_int(ctx, argv[0], "size", 1) & 0xFF);
    spawn.kind   = (uint8_t)(prv_obj_int(ctx, argv[0], "kind", DTR_PARTICLE_PIXEL) & 0xFF);

    count     = prv_obj_int(ctx, argv[0], "count", 1);
    spread    = prv_obj_float(ctx, argv[0], "spread", 0.0);
    speed_min = prv_obj_float(ctx, argv[0], "speedMin", 0.0);
    speed_max = prv_obj_float(ctx, argv[0], "speedMax", 0.0);

    if (count <= 1) {
        spawn.vx = (float)base_vx;
        spawn.vy = (float)base_vy;
        return JS_NewInt32(ctx, dtr_particles_emit(con->particles, &spawn) >= 0 ? 1 : 0);
    }

    /* Burst mode: derive direction & speed from base velocity. */
    base_speed = sqrt(base_vx * base_vx + base_vy * base_vy);
    base_angle = (base_speed > 0.0) ? atan2(base_vy, base_vx) : 0.0;

    spawned = 0;
    for (i = 0; i < count; ++i) {
        double angle;
        double speed;
        double t;

        /* Deterministic-ish jitter via console RNG */
        t = (double)(con->rng_state & 0xFFFFFFFFu) / 4294967296.0;
        con->rng_state ^= con->rng_state << 13;
        con->rng_state ^= con->rng_state >> 7;
        con->rng_state ^= con->rng_state << 17;
        if (con->rng_state == 0) {
            con->rng_state = 1;
        }

        angle = base_angle + (t * 2.0 - 1.0) * spread;
        if (speed_max > speed_min) {
            double s;

            s     = (double)(con->rng_state & 0xFFFFFFFFu) / 4294967296.0;
            speed = speed_min + s * (speed_max - speed_min);
            con->rng_state ^= con->rng_state << 13;
            con->rng_state ^= con->rng_state >> 7;
            con->rng_state ^= con->rng_state << 17;
            if (con->rng_state == 0) {
                con->rng_state = 1;
            }
        } else {
            speed = base_speed;
        }
        spawn.vx = (float)(cos(angle) * speed);
        spawn.vy = (float)(sin(angle) * speed);

        if (dtr_particles_emit(con->particles, &spawn) >= 0) {
            ++spawned;
        }
    }

    return JS_NewInt32(ctx, spawned);
}

/* particles.update(dt) — advance simulation */
static JSValue js_p_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    double         dt;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con->particles == NULL) {
        return JS_UNDEFINED;
    }
    dt = dtr_api_opt_float(ctx, argc, argv, 0, (double)con->delta);
    dtr_particles_tick(con->particles, (float)dt);
    return JS_UNDEFINED;
}

/* particles.draw() — render every active particle */
static JSValue js_p_draw(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    dtr_particles_t *p;
    dtr_graphics_t  *gfx;
    int32_t          i;

    (void)this_val;
    (void)argc;
    (void)argv;

    con = dtr_api_get_console(ctx);
    p   = con->particles;
    gfx = con->graphics;
    if (p == NULL || gfx == NULL) {
        return JS_UNDEFINED;
    }

    for (i = 0; i < p->high_water; ++i) {
        dtr_particle_t *pt;
        float           t;
        uint8_t         col;
        int32_t         px;
        int32_t         py;
        int32_t         sz;

        pt = &p->pool[i];
        if (!pt->active) {
            continue;
        }

        t   = pt->max_life > 0.0f ? (1.0f - pt->life / pt->max_life) : 0.0f;
        col = dtr_particles_lerp_color(pt->color0, pt->color1, t);
        px  = (int32_t)pt->x;
        py  = (int32_t)pt->y;
        sz  = pt->size;

        switch (pt->kind) {
            case DTR_PARTICLE_RECT:
                dtr_gfx_rectfill(gfx, px, py, px + sz - 1, py + sz - 1, col);
                break;
            case DTR_PARTICLE_CIRCLE:
                dtr_gfx_circfill(gfx, px, py, sz, col);
                break;
            case DTR_PARTICLE_PIXEL:
            default:
                if (sz <= 1) {
                    dtr_gfx_pset(gfx, px, py, col);
                } else {
                    dtr_gfx_rectfill(gfx, px, py, px + sz - 1, py + sz - 1, col);
                }
                break;
        }
    }

    return JS_UNDEFINED;
}

/* particles.clear() */
static JSValue js_p_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    dtr_particles_clear(con->particles);
    return JS_UNDEFINED;
}

/* particles.count() */
static JSValue js_p_count(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    return JS_NewInt32(ctx, dtr_particles_count(con->particles));
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_particles_funcs[] = {
    JS_CFUNC_DEF("emit", 1, js_p_emit),
    JS_CFUNC_DEF("update", 1, js_p_update),
    JS_CFUNC_DEF("draw", 0, js_p_draw),
    JS_CFUNC_DEF("clear", 0, js_p_clear),
    JS_CFUNC_DEF("count", 0, js_p_count),
};

void dtr_particles_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_particles_funcs, countof(js_particles_funcs));

    /* Render-kind constants for ergonomics */
    JS_SetPropertyStr(ctx, ns, "PIXEL", JS_NewInt32(ctx, DTR_PARTICLE_PIXEL));
    JS_SetPropertyStr(ctx, ns, "RECT", JS_NewInt32(ctx, DTR_PARTICLE_RECT));
    JS_SetPropertyStr(ctx, ns, "CIRCLE", JS_NewInt32(ctx, DTR_PARTICLE_CIRCLE));

    JS_SetPropertyStr(ctx, global, "particles", ns);
}
