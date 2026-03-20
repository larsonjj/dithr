/**
 * \file            test_runtime.c
 * \brief           Unit tests for the QuickJS-NG runtime wrapper
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "test_harness.h"
#include "runtime.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/** Create a runtime with no console (safe for pure JS tests). */
static dtr_runtime_t *prv_make_rt(void)
{
    return dtr_runtime_create(NULL, 8, 256);
}

/* ------------------------------------------------------------------ */
/*  Lifecycle tests                                                    */
/* ------------------------------------------------------------------ */

static void test_create_destroy(void)
{
    dtr_runtime_t *rt;

    rt = prv_make_rt();
    DTR_ASSERT(rt != NULL);
    DTR_ASSERT(rt->ctx != NULL);
    DTR_ASSERT(rt->rt != NULL);
    DTR_ASSERT(!rt->error_active);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_destroy_null(void)
{
    dtr_runtime_destroy(NULL); /* must not crash */
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Eval tests                                                         */
/* ------------------------------------------------------------------ */

static void test_eval_valid(void)
{
    dtr_runtime_t *rt;
    bool           ok;

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, "var x = 42;", 11, "<test>");
    DTR_ASSERT(ok);
    DTR_ASSERT(!rt->error_active);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_syntax_error(void)
{
    dtr_runtime_t *rt;
    bool           ok;

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, "var x = ;", 9, "<test>");
    DTR_ASSERT(!ok);
    DTR_ASSERT(rt->error_active);
    DTR_ASSERT(strlen(rt->error_msg) > 0);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_runtime_error(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "undefined_var.foo;";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(!ok);
    DTR_ASSERT(rt->error_active);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Call tests                                                         */
/* ------------------------------------------------------------------ */

static void test_call_existing(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "globalThis.__val = 0; function _init() { globalThis.__val = 1; }";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    ok = dtr_runtime_call(rt, rt->atom_init);
    DTR_ASSERT(ok);
    DTR_ASSERT(!rt->error_active);

    /* Verify the function ran by reading __val */
    {
        JSValue global;
        JSValue val;
        int32_t result;

        global = JS_GetGlobalObject(rt->ctx);
        val    = JS_GetPropertyStr(rt->ctx, global, "__val");
        JS_ToInt32(rt->ctx, &result, val);
        DTR_ASSERT_EQ_INT(result, 1);
        JS_FreeValue(rt->ctx, val);
        JS_FreeValue(rt->ctx, global);
    }

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_call_missing_function(void)
{
    dtr_runtime_t *rt;
    bool           ok;

    rt = prv_make_rt();
    /* _init is not defined — call should succeed (skip silently) */
    ok = dtr_runtime_call(rt, rt->atom_init);
    DTR_ASSERT(ok);
    DTR_ASSERT(!rt->error_active);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_call_throwing_function(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "function _update() { throw new Error('boom'); }";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    ok = dtr_runtime_call(rt, rt->atom_update);
    DTR_ASSERT(!ok);
    DTR_ASSERT(rt->error_active);
    DTR_ASSERT(strstr(rt->error_msg, "boom") != NULL);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_call_blocked_after_error(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "function _update() { throw new Error('fail'); }\n"
                          "function _draw() { globalThis.__draw_ran = 1; }";

    rt = prv_make_rt();
    dtr_runtime_eval(rt, code, strlen(code), "<test>");

    /* Trigger error in _update */
    ok = dtr_runtime_call(rt, rt->atom_update);
    DTR_ASSERT(!ok);
    DTR_ASSERT(rt->error_active);

    /* _draw should be blocked while error is active */
    ok = dtr_runtime_call(rt, rt->atom_draw);
    DTR_ASSERT(!ok);

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Clear error tests                                                  */
/* ------------------------------------------------------------------ */

static void test_clear_error(void)
{
    dtr_runtime_t *rt;
    const char    *code = "function _update() { throw new Error('oops'); }";

    rt = prv_make_rt();
    dtr_runtime_eval(rt, code, strlen(code), "<test>");
    dtr_runtime_call(rt, rt->atom_update);
    DTR_ASSERT(rt->error_active);

    dtr_runtime_clear_error(rt);
    DTR_ASSERT(!rt->error_active);
    DTR_ASSERT_EQ_INT(rt->error_line, 0);
    DTR_ASSERT_EQ_INT(rt->error_msg[0], '\0');

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  JSON parsing tests                                                 */
/* ------------------------------------------------------------------ */

static void test_parse_json_valid(void)
{
    dtr_runtime_t *rt;
    JSValue        val;
    JSValue        prop;
    int32_t        num;
    const char    *json = "{\"x\":42}";

    rt  = prv_make_rt();
    val = dtr_runtime_parse_json(rt, json, strlen(json));
    DTR_ASSERT(!JS_IsException(val));
    DTR_ASSERT(!rt->error_active);

    prop = JS_GetPropertyStr(rt->ctx, val, "x");
    JS_ToInt32(rt->ctx, &num, prop);
    DTR_ASSERT_EQ_INT(num, 42);

    JS_FreeValue(rt->ctx, prop);
    JS_FreeValue(rt->ctx, val);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_parse_json_invalid(void)
{
    dtr_runtime_t *rt;
    JSValue        val;

    rt  = prv_make_rt();
    val = dtr_runtime_parse_json(rt, "{bad", 4);
    DTR_ASSERT(JS_IsException(val));
    DTR_ASSERT(rt->error_active);

    JS_FreeValue(rt->ctx, val);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Drain jobs tests                                                   */
/* ------------------------------------------------------------------ */

static void test_drain_jobs(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code =
        "globalThis.__resolved = 0;\n"
        "Promise.resolve().then(() => { globalThis.__resolved = 1; });\n";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    /* Before draining, promise callback hasn't run */
    dtr_runtime_drain_jobs(rt);

    /* After draining, callback should have executed */
    {
        JSValue global;
        JSValue val;
        int32_t result;

        global = JS_GetGlobalObject(rt->ctx);
        val    = JS_GetPropertyStr(rt->ctx, global, "__resolved");
        JS_ToInt32(rt->ctx, &result, val);
        DTR_ASSERT_EQ_INT(result, 1);
        JS_FreeValue(rt->ctx, val);
        JS_FreeValue(rt->ctx, global);
    }

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    SDL_Init(0);

    printf("test_runtime\n");

    /* Lifecycle */
    test_create_destroy();
    test_destroy_null();

    /* Eval */
    test_eval_valid();
    test_eval_syntax_error();
    test_eval_runtime_error();

    /* Call */
    test_call_existing();
    test_call_missing_function();
    test_call_throwing_function();
    test_call_blocked_after_error();

    /* Error management */
    test_clear_error();

    /* JSON */
    test_parse_json_valid();
    test_parse_json_invalid();

    /* Microtasks */
    test_drain_jobs();

    printf("All tests passed.\n");
    SDL_Quit();
    return 0;
}
