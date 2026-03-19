/**
 * \file            runtime.c
 * \brief           QuickJS-NG runtime wrapper — context, js_call, error overlay
 */

#include "runtime.h"

#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * \brief           Extract exception message + line from context
 */
static void prv_capture_exception(dtr_runtime_t *rt)
{
    JSValue     exc;
    JSValue     stack;
    const char *msg;
    const char *stk;

    exc = JS_GetException(rt->ctx);
    msg = JS_ToCString(rt->ctx, exc);
    if (msg != NULL) {
        SDL_snprintf(rt->error_msg, DTR_ERROR_MSG_LEN, "%s", msg);
        JS_FreeCString(rt->ctx, msg);
    } else {
        SDL_strlcpy(rt->error_msg, "Unknown error", DTR_ERROR_MSG_LEN);
    }

    /* Try to get line from stack */
    stack = JS_GetPropertyStr(rt->ctx, exc, "stack");
    if (JS_IsString(stack)) {
        stk = JS_ToCString(rt->ctx, stack);
        if (stk != NULL) {
            /* Append first line of stack to message */
            size_t cur_len;

            cur_len = SDL_strlen(rt->error_msg);
            SDL_snprintf(rt->error_msg + cur_len, DTR_ERROR_MSG_LEN - cur_len, "\n%s", stk);
            JS_FreeCString(rt->ctx, stk);
        }
    }
    JS_FreeValue(rt->ctx, stack);

    /* Attempt to extract lineNumber */
    {
        JSValue line_val;
        int32_t line_num;

        line_val = JS_GetPropertyStr(rt->ctx, exc, "lineNumber");
        if (JS_ToInt32(rt->ctx, &line_num, line_val) == 0) {
            rt->error_line = line_num;
        }
        JS_FreeValue(rt->ctx, line_val);
    }

    JS_FreeValue(rt->ctx, exc);
    rt->error_active = true;

    SDL_Log("JS Error (line %d): %s", rt->error_line, rt->error_msg);
}

/* ------------------------------------------------------------------ */
/*  Create / destroy                                                   */
/* ------------------------------------------------------------------ */

dtr_runtime_t *dtr_runtime_create(dtr_console_t *con, int32_t heap_mb, int32_t stack_kb)
{
    dtr_runtime_t *rt;

    rt = DTR_CALLOC(1, sizeof(dtr_runtime_t));
    if (rt == NULL) {
        return NULL;
    }

    rt->console = con;

    rt->rt = JS_NewRuntime();
    if (rt->rt == NULL) {
        DTR_FREE(rt);
        return NULL;
    }

    /* Apply memory limits */
    JS_SetMemoryLimit(rt->rt, (size_t)heap_mb * 1024 * 1024);
    JS_SetMaxStackSize(rt->rt, (size_t)stack_kb * 1024);

    rt->ctx = JS_NewContext(rt->rt);
    if (rt->ctx == NULL) {
        JS_FreeRuntime(rt->rt);
        DTR_FREE(rt);
        return NULL;
    }

    /* Store console pointer as context opaque for API access */
    JS_SetContextOpaque(rt->ctx, con);

    /* Cache frequently used atoms */
    rt->atom_init   = JS_NewAtom(rt->ctx, "_init");
    rt->atom_update = JS_NewAtom(rt->ctx, "_update");
    rt->atom_draw   = JS_NewAtom(rt->ctx, "_draw");

    rt->error_active = false;
    rt->error_line   = 0;
    rt->error_msg[0] = '\0';

    return rt;
}

void dtr_runtime_destroy(dtr_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    if (rt->ctx != NULL) {
        JS_FreeAtom(rt->ctx, rt->atom_init);
        JS_FreeAtom(rt->ctx, rt->atom_update);
        JS_FreeAtom(rt->ctx, rt->atom_draw);
        JS_FreeContext(rt->ctx);
    }
    if (rt->rt != NULL) {
        JS_FreeRuntime(rt->rt);
    }

    DTR_FREE(rt);
}

/* ------------------------------------------------------------------ */
/*  Eval                                                               */
/* ------------------------------------------------------------------ */

bool dtr_runtime_eval(dtr_runtime_t *rt, const char *code, size_t len, const char *filename)
{
    JSValue result;

    result = JS_Eval(rt->ctx, code, len, filename, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        prv_capture_exception(rt);
        JS_FreeValue(rt->ctx, result);
        return false;
    }

    JS_FreeValue(rt->ctx, result);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Call global function by atom                                       */
/* ------------------------------------------------------------------ */

bool dtr_runtime_call(dtr_runtime_t *rt, JSAtom name)
{
    JSValue global;
    JSValue func;
    JSValue result;

    if (rt->error_active) {
        return false;
    }

    global = JS_GetGlobalObject(rt->ctx);
    func   = JS_GetProperty(rt->ctx, global, name);

    if (!JS_IsFunction(rt->ctx, func)) {
        /* Function not defined — not an error, just skip */
        JS_FreeValue(rt->ctx, func);
        JS_FreeValue(rt->ctx, global);
        return true;
    }

    result = JS_Call(rt->ctx, func, global, 0, NULL);
    if (JS_IsException(result)) {
        prv_capture_exception(rt);
        JS_FreeValue(rt->ctx, result);
        JS_FreeValue(rt->ctx, func);
        JS_FreeValue(rt->ctx, global);
        return false;
    }

    JS_FreeValue(rt->ctx, result);
    JS_FreeValue(rt->ctx, func);
    JS_FreeValue(rt->ctx, global);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Call global function by atom with arguments                        */
/* ------------------------------------------------------------------ */

bool dtr_runtime_call_argv(dtr_runtime_t *rt, JSAtom name, int argc, JSValue *argv)
{
    JSValue global;
    JSValue func;
    JSValue result;

    if (rt->error_active) {
        return false;
    }

    global = JS_GetGlobalObject(rt->ctx);
    func   = JS_GetProperty(rt->ctx, global, name);

    if (!JS_IsFunction(rt->ctx, func)) {
        JS_FreeValue(rt->ctx, func);
        JS_FreeValue(rt->ctx, global);
        return true;
    }

    result = JS_Call(rt->ctx, func, global, argc, argv);
    if (JS_IsException(result)) {
        prv_capture_exception(rt);
        JS_FreeValue(rt->ctx, result);
        JS_FreeValue(rt->ctx, func);
        JS_FreeValue(rt->ctx, global);
        return false;
    }

    JS_FreeValue(rt->ctx, result);
    JS_FreeValue(rt->ctx, func);
    JS_FreeValue(rt->ctx, global);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Drain microtasks                                                   */
/* ------------------------------------------------------------------ */

void dtr_runtime_drain_jobs(dtr_runtime_t *rt)
{
    JSContext *pctx;

    for (;;) {
        int32_t err;

        err = JS_ExecutePendingJob(rt->rt, &pctx);
        if (err <= 0) {
            if (err < 0) {
                prv_capture_exception(rt);
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  JSON parsing                                                       */
/* ------------------------------------------------------------------ */

JSValue dtr_runtime_parse_json(dtr_runtime_t *rt, const char *json, size_t len)
{
    JSValue result;

    result = JS_ParseJSON(rt->ctx, json, len, "<json>");
    if (JS_IsException(result)) {
        prv_capture_exception(rt);
    }
    return result;
}

/* ------------------------------------------------------------------ */
/*  Clear error                                                        */
/* ------------------------------------------------------------------ */

void dtr_runtime_clear_error(dtr_runtime_t *rt)
{
    rt->error_active = false;
    rt->error_line   = 0;
    rt->error_msg[0] = '\0';
}
