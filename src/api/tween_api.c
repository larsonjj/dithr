/**
 * \file            tween_api.c
 * \brief           JS bindings for tween engine (Promise-based)
 */

#include "../tween.h"
#include "api_common.h"

#define TWN(ctx) (&dtr_api_get_console(ctx)->tween)

/* ------------------------------------------------------------------ */
/*  Promise storage — one resolve/reject pair per tween slot            */
/* ------------------------------------------------------------------ */

/**
 * Per-slot Promise resolve functions.  Stored as duped JSValues so they
 * survive across frames.  Index matches the tween pool index.
 */
static JSValue prv_resolve[CONSOLE_MAX_TWEENS];
static JSValue prv_reject[CONSOLE_MAX_TWEENS];
static bool    prv_has_promise[CONSOLE_MAX_TWEENS];
static bool    prv_inited = false;

static void prv_init_promises(void)
{
    int32_t idx;

    if (prv_inited) {
        return;
    }
    for (idx = 0; idx < CONSOLE_MAX_TWEENS; ++idx) {
        prv_resolve[idx]     = JS_UNDEFINED;
        prv_reject[idx]      = JS_UNDEFINED;
        prv_has_promise[idx] = false;
    }
    prv_inited = true;
}

static void prv_clear_promise(JSContext *ctx, int32_t idx)
{
    if (idx >= 0 && idx < CONSOLE_MAX_TWEENS && prv_has_promise[idx]) {
        JS_FreeValue(ctx, prv_resolve[idx]);
        JS_FreeValue(ctx, prv_reject[idx]);
        prv_resolve[idx]     = JS_UNDEFINED;
        prv_reject[idx]      = JS_UNDEFINED;
        prv_has_promise[idx] = false;
    }
}

/* ---- Helper: parse easing arg ------------------------------------------ */

static dtr_ease_t prv_get_ease(JSContext *ctx, int argc, JSValueConst *argv, int idx)
{
    const char *name;
    dtr_ease_t  ease;

    ease = DTR_EASE_LINEAR;
    if (idx < argc && JS_IsString(argv[idx])) {
        name = dtr_api_get_string(ctx, argv[idx]);
        ease = dtr_ease_from_name(name);
        if (name != NULL) {
            JS_FreeCString(ctx, name);
        }
    }
    return ease;
}

/* ---- Helper: extract id from number or {id, done} object --------------- */

static int32_t prv_get_id(JSContext *ctx, int argc, JSValueConst *argv, int idx)
{
    int32_t val;
    JSValue prop;

    if (idx >= argc) {
        return -1;
    }

    /* If it's a number, use directly */
    if (JS_IsNumber(argv[idx])) {
        if (JS_ToInt32(ctx, &val, argv[idx]) != 0) {
            return -1;
        }
        return val;
    }

    /* If it's an object, read .id property */
    if (JS_IsObject(argv[idx])) {
        prop = JS_GetPropertyStr(ctx, argv[idx], "id");
        if (JS_ToInt32(ctx, &val, prop) != 0) {
            val = -1;
        }
        JS_FreeValue(ctx, prop);
        return val;
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/*  tween.ease(t, name) → eased value 0..1                             */
/* ------------------------------------------------------------------ */

static JSValue js_tween_ease(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double time;

    (void)this_val;
    time = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    return JS_NewFloat64(ctx, dtr_ease_apply(prv_get_ease(ctx, argc, argv, 1), time));
}

/* ------------------------------------------------------------------ */
/*  tween.start(from, to, dur, ease?, delay?) → {id, done: Promise}     */
/* ------------------------------------------------------------------ */

static JSValue js_tween_start(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double     from;
    double     too;
    double     dur;
    double     delay;
    dtr_ease_t ease;
    int32_t    idx;
    JSValue    resolve_funcs[2];
    JSValue    promise;
    JSValue    result;

    (void)this_val;
    prv_init_promises();

    from  = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    too   = dtr_api_opt_float(ctx, argc, argv, 1, 1.0);
    dur   = dtr_api_opt_float(ctx, argc, argv, 2, 1.0);
    ease  = prv_get_ease(ctx, argc, argv, 3);
    delay = dtr_api_opt_float(ctx, argc, argv, 4, 0.0);

    idx = dtr_tween_add(TWN(ctx), from, too, dur, ease, delay);
    if (idx < 0) {
        return JS_ThrowInternalError(ctx, "tween pool full");
    }

    /* Create a Promise that resolves when the tween finishes */
    promise              = JS_NewPromiseCapability(ctx, resolve_funcs);
    prv_resolve[idx]     = JS_DupValue(ctx, resolve_funcs[0]);
    prv_reject[idx]      = JS_DupValue(ctx, resolve_funcs[1]);
    prv_has_promise[idx] = true;
    JS_FreeValue(ctx, resolve_funcs[0]);
    JS_FreeValue(ctx, resolve_funcs[1]);

    /* Return {id: number, done: Promise} */
    result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, idx));
    JS_SetPropertyStr(ctx, result, "done", JS_DupValue(ctx, promise));
    JS_FreeValue(ctx, promise);
    return result;
}

/* ------------------------------------------------------------------ */
/*  tween.tick(dt) — advance all tweens, resolve finished Promises      */
/* ------------------------------------------------------------------ */

static JSValue js_tween_tick(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_tween_t *twn;
    int32_t      idx;

    (void)this_val;
    twn = TWN(ctx);
    dtr_tween_tick(twn, dtr_api_opt_float(ctx, argc, argv, 0, 0.0));

    /* Resolve any finished tweens that have pending Promises */
    for (idx = 0; idx < twn->count; ++idx) {
        dtr_tween_entry_t *ent;

        ent = &twn->pool[idx];
        if (!ent->active || ent->resolved) {
            continue;
        }
        if (ent->elapsed >= ent->duration) {
            ent->resolved = true;
            if (prv_has_promise[idx]) {
                JSValue id_val;
                JSValue ret;

                id_val = JS_NewInt32(ctx, idx);
                ret    = JS_Call(ctx, prv_resolve[idx], JS_UNDEFINED, 1, &id_val);
                JS_FreeValue(ctx, ret);
                JS_FreeValue(ctx, id_val);
                prv_clear_promise(ctx, idx);
            }
        }
    }
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  tween.val(handle_or_id, default?) → current interpolated value      */
/* ------------------------------------------------------------------ */

static JSValue js_tween_val(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t idx;
    double  dflt;

    (void)this_val;
    idx  = prv_get_id(ctx, argc, argv, 0);
    dflt = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);

    if (idx < 0 || idx >= CONSOLE_MAX_TWEENS || !TWN(ctx)->pool[idx].active) {
        return JS_NewFloat64(ctx, dflt);
    }
    return JS_NewFloat64(ctx, dtr_tween_val(TWN(ctx), idx));
}

/* ------------------------------------------------------------------ */
/*  tween.done(handle_or_id) → bool                                     */
/* ------------------------------------------------------------------ */

static JSValue js_tween_done(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx, dtr_tween_done(TWN(ctx), prv_get_id(ctx, argc, argv, 0)));
}

/* ------------------------------------------------------------------ */
/*  tween.cancel(handle_or_id)                                          */
/* ------------------------------------------------------------------ */

static JSValue js_tween_cancel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t idx;

    (void)this_val;
    idx = prv_get_id(ctx, argc, argv, 0);

    /* Reject the Promise if any */
    if (prv_has_promise[idx]) {
        JSValue reason;
        JSValue ret;

        reason = JS_NewString(ctx, "tween cancelled");
        ret    = JS_Call(ctx, prv_reject[idx], JS_UNDEFINED, 1, &reason);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, reason);
        prv_clear_promise(ctx, idx);
    }
    dtr_tween_cancel(TWN(ctx), idx);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  tween.cancelAll()                                                   */
/* ------------------------------------------------------------------ */

static JSValue
js_tween_cancel_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_tween_t *twn;
    int32_t      idx;

    (void)this_val;
    (void)argc;
    (void)argv;
    twn = TWN(ctx);

    /* Reject all pending Promises */
    for (idx = 0; idx < twn->count; ++idx) {
        if (prv_has_promise[idx]) {
            JSValue reason;
            JSValue ret;

            reason = JS_NewString(ctx, "tween cancelled");
            ret    = JS_Call(ctx, prv_reject[idx], JS_UNDEFINED, 1, &reason);
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, reason);
            prv_clear_promise(ctx, idx);
        }
    }
    dtr_tween_cancel_all(twn);
    return JS_UNDEFINED;
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_tween_funcs[] = {
    JS_CFUNC_DEF("ease", 2, js_tween_ease),
    JS_CFUNC_DEF("start", 5, js_tween_start),
    JS_CFUNC_DEF("tick", 1, js_tween_tick),
    JS_CFUNC_DEF("val", 2, js_tween_val),
    JS_CFUNC_DEF("done", 1, js_tween_done),
    JS_CFUNC_DEF("cancel", 1, js_tween_cancel),
    JS_CFUNC_DEF("cancelAll", 0, js_tween_cancel_all),
};

void dtr_tween_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    prv_init_promises();
    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_tween_funcs, countof(js_tween_funcs));
    JS_SetPropertyStr(ctx, global, "tween", ns);
}
