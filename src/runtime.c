/**
 * \file            runtime.c
 * \brief           QuickJS-NG runtime wrapper — context, js_call, error overlay
 */

#include "runtime.h"

#include "cart.h"

#include <stdio.h>
#include <string.h>

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
/*  ES module normalizer + loader                                      */
/* ------------------------------------------------------------------ */

/**
 * \brief           Resolve a module specifier relative to the importing file.
 *
 * Relative specifiers (starting with "./" or "../") are resolved against
 * the cart's base_path combined with the importing module's path.
 * Absolute specifiers are returned as-is.
 */
static char *
prv_module_normalize(JSContext *ctx, const char *base_name, const char *name, void *opaque)
{
    (void)opaque;

    /* Non-relative specifier — return as-is */
    if (name[0] != '.') {
        return js_strdup(ctx, name);
    }

    /* Find directory portion of base_name */
    const char *sep;
    int         base_len;
    int         cap;
    char       *result;

    sep = strrchr(base_name, '/');
    if (sep != NULL) {
        base_len = (int)(sep - base_name);
    } else {
        base_len = 0;
    }

    cap    = base_len + (int)strlen(name) + 2;
    result = js_malloc(ctx, (size_t)cap);
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, base_name, (size_t)base_len);
    result[base_len] = '\0';

    /* Resolve leading "./" and "../" segments */
    const char *ptr;

    ptr = name;
    for (;;) {
        if (ptr[0] == '.' && ptr[1] == '/') {
            ptr += 2;
        } else if (ptr[0] == '.' && ptr[1] == '.' && ptr[2] == '/') {
            /* Walk up one directory */
            if (result[0] != '\0') {
                char *last;

                last = strrchr(result, '/');
                if (last != NULL) {
                    *last = '\0';
                } else {
                    result[0] = '\0';
                }
            }
            ptr += 3;
        } else {
            break;
        }
    }

    if (result[0] != '\0') {
        size_t cur;

        cur             = strlen(result);
        result[cur]     = '/';
        result[cur + 1] = '\0';
    }
    SDL_strlcat(result, ptr, (size_t)cap);

    return result;
}

/**
 * \brief           Load a JS module file from disk relative to the cart base.
 *
 * The normalizer has already produced a path relative to the cart root.
 * We prepend base_path and load the file with SDL_LoadFile.
 */
static JSModuleDef *prv_module_loader(JSContext *ctx, const char *module_name, void *opaque)
{
    dtr_console_t *con;
    char           path[1024];
    size_t         buf_len;
    char          *buf;
    JSValue        func_val;
    JSModuleDef   *mod;

    con = (dtr_console_t *)opaque;

    SDL_snprintf(path, sizeof(path), "%s%s", con->cart->base_path, module_name);
    buf = (char *)SDL_LoadFile(path, &buf_len);
    if (buf == NULL) {
        JS_ThrowReferenceError(ctx, "could not load module '%s'", module_name);
        return NULL;
    }

    func_val =
        JS_Eval(ctx, buf, buf_len, module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    SDL_free(buf);

    if (JS_IsException(func_val)) {
        return NULL;
    }

    mod = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
    return mod;
}

/**
 * \brief           Check whether JS source contains top-level import/export.
 *
 * A simple heuristic: scan for "import " or "export " at the start of a
 * line (ignoring leading whitespace).  Not a full parser, but reliable
 * for well-formed source.
 */
static bool prv_code_is_module(const char *code, size_t len)
{
    const char *end;
    const char *ptr;
    bool        at_line_start;

    end           = code + len;
    ptr           = code;
    at_line_start = true;

    while (ptr < end) {
        char cur;

        cur = *ptr;

        if (at_line_start) {
            /* Skip leading whitespace */
            if (cur == ' ' || cur == '\t') {
                ++ptr;
                continue;
            }
            /* Check for single-line comment — skip entire line */
            if (cur == '/' && (ptr + 1) < end && ptr[1] == '/') {
                while (ptr < end && *ptr != '\n') {
                    ++ptr;
                }
                continue;
            }
            /* Check for block comment */
            if (cur == '/' && (ptr + 1) < end && ptr[1] == '*') {
                ptr += 2;
                while (ptr < end) {
                    if (*ptr == '*' && (ptr + 1) < end && ptr[1] == '/') {
                        ptr += 2;
                        break;
                    }
                    ++ptr;
                }
                continue;
            }
            /* Check for "import " or "export " */
            if (cur == 'i' && (ptr + 7) <= end && memcmp(ptr, "import ", 7) == 0) {
                return true;
            }
            if (cur == 'e' && (ptr + 7) <= end && memcmp(ptr, "export ", 7) == 0) {
                return true;
            }
            at_line_start = false;
        }

        if (cur == '\n') {
            at_line_start = true;
        }
        ++ptr;
    }
    return false;
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

    /* Register ES module normalizer + loader */
    JS_SetModuleLoaderFunc(rt->rt, prv_module_normalize, prv_module_loader, con);

    /* Cache frequently used atoms */
    rt->atom_init         = JS_NewAtom(rt->ctx, "_init");
    rt->atom_update       = JS_NewAtom(rt->ctx, "_update");
    rt->atom_fixed_update = JS_NewAtom(rt->ctx, "_fixedUpdate");
    rt->atom_draw         = JS_NewAtom(rt->ctx, "_draw");
    rt->atom_save         = JS_NewAtom(rt->ctx, "_save");
    rt->atom_restore      = JS_NewAtom(rt->ctx, "_restore");

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
        JS_FreeAtom(rt->ctx, rt->atom_fixed_update);
        JS_FreeAtom(rt->ctx, rt->atom_draw);
        JS_FreeAtom(rt->ctx, rt->atom_save);
        JS_FreeAtom(rt->ctx, rt->atom_restore);
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
    bool    is_module;

    is_module = prv_code_is_module(code, len);

    if (is_module) {
        JSModuleDef *mod;

        /* Compile the module without executing it */
        result =
            JS_Eval(rt->ctx, code, len, filename, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(result)) {
            prv_capture_exception(rt);
            JS_FreeValue(rt->ctx, result);
            return false;
        }

        mod = JS_VALUE_GET_PTR(result);

        /* Execute the compiled module */
        result = JS_EvalFunction(rt->ctx, result);
        if (JS_IsException(result)) {
            prv_capture_exception(rt);
            JS_FreeValue(rt->ctx, result);
            return false;
        }
        JS_FreeValue(rt->ctx, result);

        /* Drain pending jobs (module execution may create promises) */
        dtr_runtime_drain_jobs(rt);

        /* Promote exported lifecycle functions to the global object */
        {
            JSValue ns;
            JSValue global;
            JSAtom  atoms[] = {
                rt->atom_init, rt->atom_update, rt->atom_draw, rt->atom_save, rt->atom_restore};
            int32_t cnt;
            int32_t idx;

            ns     = JS_GetModuleNamespace(rt->ctx, mod);
            global = JS_GetGlobalObject(rt->ctx);
            cnt    = (int32_t)(sizeof(atoms) / sizeof(atoms[0]));

            for (idx = 0; idx < cnt; ++idx) {
                JSValue val;

                val = JS_GetProperty(rt->ctx, ns, atoms[idx]);
                if (JS_IsFunction(rt->ctx, val)) {
                    JS_SetProperty(rt->ctx, global, atoms[idx], JS_DupValue(rt->ctx, val));
                }
                JS_FreeValue(rt->ctx, val);
            }

            JS_FreeValue(rt->ctx, global);
            JS_FreeValue(rt->ctx, ns);
        }
    } else {
        result = JS_Eval(rt->ctx, code, len, filename, JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            prv_capture_exception(rt);
            JS_FreeValue(rt->ctx, result);
            return false;
        }
        JS_FreeValue(rt->ctx, result);
    }

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
