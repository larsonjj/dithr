/**
 * \file            test_editor_js.c
 * \brief           Run self-contained JS test files via QuickJS
 *
 * Each JS file defines __failures (int) which is checked after eval.
 * Test files are standalone scripts (no ES module imports) so they
 * run in a bare QuickJS context created with dtr_runtime_create(NULL).
 */

#include "runtime.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JS_TEST_DIR is defined by CMake — absolute path to tests/js/ */
#ifndef JS_TEST_DIR
#error "JS_TEST_DIR must be defined at compile time"
#endif

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static char *prv_read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, (size_t)len, f);
    buf[read]   = '\0';
    fclose(f);
    return buf;
}

/** Evaluate a JS test file and assert zero failures. */
static void prv_run_js_test(const char *filename)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", JS_TEST_DIR, filename);

    char *code = prv_read_file(path);
    DTR_ASSERT_NOT_NULL(code);

    dtr_runtime_t *rt = dtr_runtime_create(NULL, 8, 256);
    DTR_ASSERT_NOT_NULL(rt);

    bool ok = dtr_runtime_eval(rt, code, strlen(code), filename);
    if (!ok) {
        fprintf(stderr, "  JS error in %s: %s\n", filename, rt->error_msg);
    }
    DTR_ASSERT(ok);
    DTR_ASSERT(!rt->error_active);

    /* Read __failures and __tests from global scope */
    JSValue global   = JS_GetGlobalObject(rt->ctx);
    JSValue failures = JS_GetPropertyStr(rt->ctx, global, "__failures");
    JSValue tests    = JS_GetPropertyStr(rt->ctx, global, "__tests");

    int32_t fail_count = 0;
    int32_t test_count = 0;
    if (!JS_IsUndefined(failures))
        JS_ToInt32(rt->ctx, &fail_count, failures);
    if (!JS_IsUndefined(tests))
        JS_ToInt32(rt->ctx, &test_count, tests);

    JS_FreeValue(rt->ctx, tests);
    JS_FreeValue(rt->ctx, failures);
    JS_FreeValue(rt->ctx, global);

    printf("  %s: %d assertions, %d failures\n", filename, test_count, fail_count);

    free(code);
    dtr_runtime_destroy(rt);

    DTR_ASSERT_EQ_INT(fail_count, 0);
}

/* ------------------------------------------------------------------ */
/*  Test functions                                                     */
/* ------------------------------------------------------------------ */

static void test_helpers(void)
{
    prv_run_js_test("test_helpers.js");
    DTR_PASS();
}

static void test_stroke_history(void)
{
    prv_run_js_test("test_stroke_history.js");
    DTR_PASS();
}

static void test_sprite_algo(void)
{
    prv_run_js_test("test_sprite_algo.js");
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("editor_js");

    DTR_RUN_TEST(test_helpers);
    DTR_RUN_TEST(test_stroke_history);
    DTR_RUN_TEST(test_sprite_algo);

    DTR_TEST_END();
}
