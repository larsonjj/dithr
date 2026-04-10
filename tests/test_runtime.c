/**
 * \file            test_runtime.c
 * \brief           Unit tests for the QuickJS-NG runtime wrapper
 */

#include "runtime.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* ASan inflates stack frames; give QuickJS more room. */
#if defined(__SANITIZE_ADDRESS__)
#define TEST_STACK_KB 1024
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define TEST_STACK_KB 1024
#endif
#endif
#ifndef TEST_STACK_KB
#define TEST_STACK_KB 256
#endif

/** Create a runtime with no console (safe for pure JS tests). */
static dtr_runtime_t *prv_make_rt(void)
{
    return dtr_runtime_create(NULL, 8, TEST_STACK_KB);
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
    const char    *code = "globalThis.__resolved = 0;\n"
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

/* ------------------------------------------------------------------ */
/*  call_argv — call function with arguments                           */
/* ------------------------------------------------------------------ */

static void test_call_argv_existing(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "globalThis.__sum = 0;\n"
                          "function _init(a) { globalThis.__sum = a; }\n";
    JSValue        argv[1];

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    argv[0] = JS_NewInt32(rt->ctx, 99);
    ok      = dtr_runtime_call_argv(rt, rt->atom_init, 1, argv);
    JS_FreeValue(rt->ctx, argv[0]);
    DTR_ASSERT(ok);

    /* Verify the argument was received */
    {
        JSValue global;
        JSValue val;
        int32_t result;

        global = JS_GetGlobalObject(rt->ctx);
        val    = JS_GetPropertyStr(rt->ctx, global, "__sum");
        JS_ToInt32(rt->ctx, &result, val);
        DTR_ASSERT_EQ_INT(result, 99);
        JS_FreeValue(rt->ctx, val);
        JS_FreeValue(rt->ctx, global);
    }

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_call_argv_missing(void)
{
    dtr_runtime_t *rt;
    bool           ok;

    rt = prv_make_rt();
    /* _init not defined — should silently succeed */
    ok = dtr_runtime_call_argv(rt, rt->atom_init, 0, NULL);
    DTR_ASSERT(ok);
    DTR_ASSERT(!rt->error_active);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_call_argv_throwing(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "function _init(x) { throw new Error('arg_fail'); }";
    JSValue        argv[1];

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    argv[0] = JS_NewInt32(rt->ctx, 1);
    ok      = dtr_runtime_call_argv(rt, rt->atom_init, 1, argv);
    JS_FreeValue(rt->ctx, argv[0]);

    DTR_ASSERT(!ok);
    DTR_ASSERT(rt->error_active);
    DTR_ASSERT(strstr(rt->error_msg, "arg_fail") != NULL);

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_call_argv_blocked_after_error(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "function _init() { throw new Error('e'); }";

    rt = prv_make_rt();
    dtr_runtime_eval(rt, code, strlen(code), "<test>");
    dtr_runtime_call(rt, rt->atom_init); /* trigger error */
    DTR_ASSERT(rt->error_active);

    /* call_argv should also be blocked */
    ok = dtr_runtime_call_argv(rt, rt->atom_update, 0, NULL);
    DTR_ASSERT(!ok);

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

/* ================================================================== */
/*  Module detection — prv_code_is_module (via eval path)              */
/* ================================================================== */

/** Helper: read a globalThis property as int32. */
static int32_t prv_read_global_int(dtr_runtime_t *rt, const char *name)
{
    JSValue global;
    JSValue val;
    int32_t result;

    global = JS_GetGlobalObject(rt->ctx);
    val    = JS_GetPropertyStr(rt->ctx, global, name);
    JS_ToInt32(rt->ctx, &result, val);
    JS_FreeValue(rt->ctx, val);
    JS_FreeValue(rt->ctx, global);
    return result;
}

static void test_eval_export_detected_as_module(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    /* export triggers the module path; promoted _init should be callable */
    const char *code = "export function _init() { globalThis.__mod = 1; }";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);
    DTR_ASSERT(!rt->error_active);

    /* _init should have been promoted to global */
    ok = dtr_runtime_call(rt, rt->atom_init);
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(prv_read_global_int(rt, "__mod"), 1);

    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_import_keyword_not_module(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    /* "import" appears mid-line inside a string literal — script path */
    const char *code = "var s = 'import something';";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);
    DTR_ASSERT(!rt->error_active);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_no_module_keywords(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "globalThis.__plain = 99;";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(prv_read_global_int(rt, "__plain"), 99);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_comment_then_export(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    /* Single-line comment before export keyword — still a module */
    const char *code = "// header comment\n"
                       "export function _init() { globalThis.__commented = 7; }";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    ok = dtr_runtime_call(rt, rt->atom_init);
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(prv_read_global_int(rt, "__commented"), 7);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_block_comment_then_export(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    /* Block comment before export */
    const char *code = "/* block */\n"
                       "export function _draw() { globalThis.__blk = 3; }";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    ok = dtr_runtime_call(rt, rt->atom_draw);
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(prv_read_global_int(rt, "__blk"), 3);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_module_multiple_exports(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "export function _init()   { globalThis.__mi = 10; }\n"
                          "export function _update() { globalThis.__mu = 20; }\n"
                          "export function _draw()   { globalThis.__md = 30; }\n";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    dtr_runtime_call(rt, rt->atom_init);
    dtr_runtime_call(rt, rt->atom_update);
    dtr_runtime_call(rt, rt->atom_draw);

    DTR_ASSERT_EQ_INT(prv_read_global_int(rt, "__mi"), 10);
    DTR_ASSERT_EQ_INT(prv_read_global_int(rt, "__mu"), 20);
    DTR_ASSERT_EQ_INT(prv_read_global_int(rt, "__md"), 30);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_module_syntax_error(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    /* Malformed export — should trigger module compilation error */
    const char *code = "export {";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(!ok);
    DTR_ASSERT(rt->error_active);
    dtr_runtime_destroy(rt);
    DTR_PASS();
}

static void test_eval_module_runtime_error(void)
{
    dtr_runtime_t *rt;
    bool           ok;
    const char    *code = "export function _init() { throw new Error('mod_boom'); }";

    rt = prv_make_rt();
    ok = dtr_runtime_eval(rt, code, strlen(code), "<test>");
    DTR_ASSERT(ok);

    ok = dtr_runtime_call(rt, rt->atom_init);
    DTR_ASSERT(!ok);
    DTR_ASSERT(rt->error_active);
    DTR_ASSERT(strstr(rt->error_msg, "mod_boom") != NULL);
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

    DTR_TEST_BEGIN("test_runtime");

    /* Lifecycle */
    DTR_RUN_TEST(test_create_destroy);
    DTR_RUN_TEST(test_destroy_null);

    /* Eval */
    DTR_RUN_TEST(test_eval_valid);
    DTR_RUN_TEST(test_eval_syntax_error);
    DTR_RUN_TEST(test_eval_runtime_error);

    /* Call */
    DTR_RUN_TEST(test_call_existing);
    DTR_RUN_TEST(test_call_missing_function);
    DTR_RUN_TEST(test_call_throwing_function);
    DTR_RUN_TEST(test_call_blocked_after_error);

    /* Error management */
    DTR_RUN_TEST(test_clear_error);

    /* JSON */
    DTR_RUN_TEST(test_parse_json_valid);
    DTR_RUN_TEST(test_parse_json_invalid);

    /* Microtasks */
    DTR_RUN_TEST(test_drain_jobs);

    /* call_argv */
    DTR_RUN_TEST(test_call_argv_existing);
    DTR_RUN_TEST(test_call_argv_missing);
    DTR_RUN_TEST(test_call_argv_throwing);
    DTR_RUN_TEST(test_call_argv_blocked_after_error);

    /* Module detection + eval */
    DTR_RUN_TEST(test_eval_export_detected_as_module);
    DTR_RUN_TEST(test_eval_import_keyword_not_module);
    DTR_RUN_TEST(test_eval_no_module_keywords);
    DTR_RUN_TEST(test_eval_comment_then_export);
    DTR_RUN_TEST(test_eval_block_comment_then_export);
    DTR_RUN_TEST(test_eval_module_multiple_exports);
    DTR_RUN_TEST(test_eval_module_syntax_error);
    DTR_RUN_TEST(test_eval_module_runtime_error);

    DTR_TEST_END();
}
