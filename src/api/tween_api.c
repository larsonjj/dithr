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
        result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -1));
        JS_SetPropertyStr(ctx, result, "done", JS_UNDEFINED);
        return result;
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

/* ------------------------------------------------------------------ */
/*  tween.sequence([{from, to, dur, ease?}, ...]) → {id, done}         */
/* ------------------------------------------------------------------ */

/**
 * Internal: parse an array of step configs into parallel arrays.
 * Returns step count, or -1 on error.
 */
static int32_t prv_parse_steps(JSContext   *ctx,
                               JSValueConst arr,
                               double      *from,
                               double      *too,
                               double      *dur,
                               dtr_ease_t  *ease,
                               int32_t      max_steps)
{
    int32_t len;
    JSValue len_val;
    int32_t step;

    len_val = JS_GetPropertyStr(ctx, arr, "length");
    JS_ToInt32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);
    if (len > max_steps) {
        len = max_steps;
    }

    for (step = 0; step < len; ++step) {
        JSValue elem;
        JSValue val;
        double  v;

        elem = JS_GetPropertyUint32(ctx, arr, (uint32_t)step);

        val = JS_GetPropertyStr(ctx, elem, "from");
        JS_ToFloat64(ctx, &v, val);
        from[step] = v;
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, elem, "to");
        JS_ToFloat64(ctx, &v, val);
        too[step] = v;
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, elem, "dur");
        JS_ToFloat64(ctx, &v, val);
        dur[step] = v;
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, elem, "ease");
        if (JS_IsString(val)) {
            const char *name;

            name       = JS_ToCString(ctx, val);
            ease[step] = dtr_ease_from_name(name);
            JS_FreeCString(ctx, name);
        } else {
            ease[step] = DTR_EASE_LINEAR;
        }
        JS_FreeValue(ctx, val);

        JS_FreeValue(ctx, elem);
    }
    return len;
}

static JSValue
js_tween_sequence(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double     from[CONSOLE_MAX_SEQ_STEPS];
    double     too[CONSOLE_MAX_SEQ_STEPS];
    double     dur[CONSOLE_MAX_SEQ_STEPS];
    dtr_ease_t ease[CONSOLE_MAX_SEQ_STEPS];
    int32_t    count;
    int32_t    si;
    int32_t    last_ti;
    JSValue    resolve_funcs[2];
    JSValue    promise;
    JSValue    result;

    (void)this_val;
    prv_init_promises();

    if (argc < 1 || !JS_IsObject(argv[0])) {
        result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -1));
        JS_SetPropertyStr(ctx, result, "done", JS_UNDEFINED);
        return result;
    }

    count = prv_parse_steps(ctx, argv[0], from, too, dur, ease, CONSOLE_MAX_SEQ_STEPS);
    si    = dtr_tween_seq_add(TWN(ctx), from, too, dur, ease, count);

    result = JS_NewObject(ctx);
    if (si < 0) {
        JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -1));
        JS_SetPropertyStr(ctx, result, "done", JS_UNDEFINED);
        return result;
    }

    /* Attach a Promise to the last sub-tween so it resolves when sequence ends */
    last_ti                  = TWN(ctx)->seqs[si].steps[count - 1];
    promise                  = JS_NewPromiseCapability(ctx, resolve_funcs);
    prv_resolve[last_ti]     = JS_DupValue(ctx, resolve_funcs[0]);
    prv_reject[last_ti]      = JS_DupValue(ctx, resolve_funcs[1]);
    prv_has_promise[last_ti] = true;
    JS_FreeValue(ctx, resolve_funcs[0]);
    JS_FreeValue(ctx, resolve_funcs[1]);

    /* Return {id: seq_index (negative to distinguish), done: Promise} */
    JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -(si + 1)));
    JS_SetPropertyStr(ctx, result, "done", JS_DupValue(ctx, promise));
    JS_FreeValue(ctx, promise);
    return result;
}

/* ------------------------------------------------------------------ */
/*  tween.parallel([{from, to, dur, ease?}, ...]) → {id, done}         */
/* ------------------------------------------------------------------ */

static JSValue
js_tween_parallel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double     from[CONSOLE_MAX_SEQ_STEPS];
    double     too[CONSOLE_MAX_SEQ_STEPS];
    double     dur[CONSOLE_MAX_SEQ_STEPS];
    dtr_ease_t ease[CONSOLE_MAX_SEQ_STEPS];
    int32_t    count;
    int32_t    si;
    JSValue    result;

    (void)this_val;
    prv_init_promises();

    if (argc < 1 || !JS_IsObject(argv[0])) {
        result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -1));
        JS_SetPropertyStr(ctx, result, "done", JS_UNDEFINED);
        return result;
    }

    count  = prv_parse_steps(ctx, argv[0], from, too, dur, ease, CONSOLE_MAX_SEQ_STEPS);
    result = JS_NewObject(ctx);
    if (count <= 0) {
        JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -1));
        JS_SetPropertyStr(ctx, result, "done", JS_UNDEFINED);
        return result;
    }

    si = dtr_tween_par_add(TWN(ctx), from, too, dur, ease, count);
    if (si < 0) {
        JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -1));
        JS_SetPropertyStr(ctx, result, "done", JS_UNDEFINED);
        return result;
    }

    /* Find the longest sub-tween and attach Promise to it */
    {
        int32_t longest_ti;
        double  max_dur;
        JSValue resolve_funcs[2];
        JSValue promise;

        longest_ti = TWN(ctx)->seqs[si].steps[0];
        max_dur    = dur[0];
        for (int32_t s = 1; s < count; ++s) {
            if (dur[s] > max_dur) {
                max_dur    = dur[s];
                longest_ti = TWN(ctx)->seqs[si].steps[s];
            }
        }
        promise                     = JS_NewPromiseCapability(ctx, resolve_funcs);
        prv_resolve[longest_ti]     = JS_DupValue(ctx, resolve_funcs[0]);
        prv_reject[longest_ti]      = JS_DupValue(ctx, resolve_funcs[1]);
        prv_has_promise[longest_ti] = true;
        JS_FreeValue(ctx, resolve_funcs[0]);
        JS_FreeValue(ctx, resolve_funcs[1]);

        JS_SetPropertyStr(ctx, result, "id", JS_NewInt32(ctx, -(si + 1)));
        JS_SetPropertyStr(ctx, result, "done", JS_DupValue(ctx, promise));
        JS_FreeValue(ctx, promise);
    }
    return result;
}

/* ------------------------------------------------------------------ */
/*  tween.seqVal(handle_or_id) → current value of active step          */
/* ------------------------------------------------------------------ */

static JSValue js_tween_seq_val(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t raw_id;
    int32_t seq_idx;

    (void)this_val;
    raw_id = prv_get_id(ctx, argc, argv, 0);
    if (raw_id >= 0) {
        /* Plain tween id — delegate to val */
        return JS_NewFloat64(ctx, dtr_tween_val(TWN(ctx), raw_id));
    }
    seq_idx = -(raw_id + 1);
    return JS_NewFloat64(ctx, dtr_tween_seq_val(TWN(ctx), seq_idx));
}

/* ------------------------------------------------------------------ */
/*  tween.seqDone(handle_or_id) → bool                                 */
/* ------------------------------------------------------------------ */

static JSValue
js_tween_seq_done(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t raw_id;
    int32_t seq_idx;

    (void)this_val;
    raw_id = prv_get_id(ctx, argc, argv, 0);
    if (raw_id >= 0) {
        return JS_NewBool(ctx, dtr_tween_done(TWN(ctx), raw_id));
    }
    seq_idx = -(raw_id + 1);
    return JS_NewBool(ctx, dtr_tween_seq_done(TWN(ctx), seq_idx));
}

/* ------------------------------------------------------------------ */
/*  tween.seqCancel(handle_or_id)                                      */
/* ------------------------------------------------------------------ */

static JSValue
js_tween_seq_cancel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t raw_id;
    int32_t seq_idx;
    int32_t step;

    (void)this_val;
    raw_id = prv_get_id(ctx, argc, argv, 0);
    if (raw_id >= 0) {
        if (prv_has_promise[raw_id]) {
            JSValue reason;
            JSValue ret;

            reason = JS_NewString(ctx, "tween cancelled");
            ret    = JS_Call(ctx, prv_reject[raw_id], JS_UNDEFINED, 1, &reason);
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, reason);
            prv_clear_promise(ctx, raw_id);
        }
        dtr_tween_cancel(TWN(ctx), raw_id);
        return JS_UNDEFINED;
    }

    seq_idx = -(raw_id + 1);
    if (seq_idx >= 0 && seq_idx < CONSOLE_MAX_TWEEN_SEQS && TWN(ctx)->seqs[seq_idx].active) {
        for (step = 0; step < TWN(ctx)->seqs[seq_idx].step_count; ++step) {
            int32_t ti;

            ti = TWN(ctx)->seqs[seq_idx].steps[step];
            if (prv_has_promise[ti]) {
                JSValue reason;
                JSValue ret;

                reason = JS_NewString(ctx, "tween cancelled");
                ret    = JS_Call(ctx, prv_reject[ti], JS_UNDEFINED, 1, &reason);
                JS_FreeValue(ctx, ret);
                JS_FreeValue(ctx, reason);
                prv_clear_promise(ctx, ti);
            }
        }
        dtr_tween_seq_cancel(TWN(ctx), seq_idx);
    }
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
    JS_CFUNC_DEF("sequence", 1, js_tween_sequence),
    JS_CFUNC_DEF("parallel", 1, js_tween_parallel),
    JS_CFUNC_DEF("seqVal", 1, js_tween_seq_val),
    JS_CFUNC_DEF("seqDone", 1, js_tween_seq_done),
    JS_CFUNC_DEF("seqCancel", 1, js_tween_seq_cancel),
};

void dtr_tween_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    prv_init_promises();
    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_tween_funcs, countof(js_tween_funcs));
    JS_SetPropertyStr(ctx, global, "tween", ns);
}
