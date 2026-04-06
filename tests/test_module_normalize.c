/**
 * \file            test_module_normalize.c
 * \brief           Unit tests for the ES module path normalizer
 *
 * Since prv_module_normalize and prv_code_is_module are static functions
 * in runtime.c, we include the implementation directly to test them.
 */

/* Include the implementation — gives access to static helpers */
#include "runtime.c"
#include "test_harness.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/** Create a bare QuickJS context for normalizer calls. */
static JSContext *prv_make_ctx(void)
{
    JSRuntime *rt;
    JSContext *ctx;

    rt  = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    return ctx;
}

static void prv_free_ctx(JSContext *ctx)
{
    JSRuntime *rt;

    rt = JS_GetRuntime(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* ------------------------------------------------------------------ */
/*  prv_module_normalize tests                                         */
/* ------------------------------------------------------------------ */

static void test_normalize_dot_slash(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "src/main.js", "./utils.js", NULL);
    DTR_ASSERT_NOT_NULL(out);
    DTR_ASSERT_EQ_STR(out, "src/utils.js");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

static void test_normalize_dot_dot_slash(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "src/lib/a.js", "../b.js", NULL);
    DTR_ASSERT_NOT_NULL(out);
    DTR_ASSERT_EQ_STR(out, "src/b.js");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

static void test_normalize_double_dot_dot(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "a/b/c.js", "../../d.js", NULL);
    DTR_ASSERT_NOT_NULL(out);
    DTR_ASSERT_EQ_STR(out, "d.js");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

static void test_normalize_bare_specifier(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "src/main.js", "lodash", NULL);
    DTR_ASSERT_NOT_NULL(out);
    DTR_ASSERT_EQ_STR(out, "lodash");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

static void test_normalize_no_dir_in_base(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "main.js", "./util.js", NULL);
    DTR_ASSERT_NOT_NULL(out);
    DTR_ASSERT_EQ_STR(out, "util.js");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

static void test_normalize_dot_dot_past_root(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "main.js", "../x.js", NULL);
    DTR_ASSERT_NOT_NULL(out);
    /* ".." from root-level file — no parent to walk, result is just "x.js" */
    DTR_ASSERT_EQ_STR(out, "x.js");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

static void test_normalize_chained_dot_slash(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "src/a.js", "././b.js", NULL);
    DTR_ASSERT_NOT_NULL(out);
    DTR_ASSERT_EQ_STR(out, "src/b.js");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

static void test_normalize_nested_relative(void)
{
    JSContext *ctx;
    char      *out;

    ctx = prv_make_ctx();
    out = prv_module_normalize(ctx, "lib/foo.js", "./bar/baz.js", NULL);
    DTR_ASSERT_NOT_NULL(out);
    DTR_ASSERT_EQ_STR(out, "lib/bar/baz.js");
    js_free(ctx, out);
    prv_free_ctx(ctx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  prv_code_is_module tests                                           */
/* ------------------------------------------------------------------ */

static void test_is_module_import(void)
{
    const char *code = "import { foo } from './bar.js';";
    DTR_ASSERT(prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_module_export(void)
{
    const char *code = "export function _init() {}";
    DTR_ASSERT(prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_module_export_default(void)
{
    const char *code = "export default 42;";
    DTR_ASSERT(prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_module_with_leading_whitespace(void)
{
    const char *code = "  \timport { x } from 'y';";
    DTR_ASSERT(prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_not_module_midline_import(void)
{
    const char *code = "var x = 'import something';";
    DTR_ASSERT(!prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_not_module_plain_script(void)
{
    const char *code = "var x = 42;\nfunction foo() {}";
    DTR_ASSERT(!prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_not_module_empty(void)
{
    DTR_ASSERT(!prv_code_is_module("", 0));
    DTR_PASS();
}

static void test_is_module_after_line_comment(void)
{
    const char *code = "// comment\nimport { x } from 'y';";
    DTR_ASSERT(prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_module_after_block_comment(void)
{
    const char *code = "/* block */\nexport const x = 1;";
    DTR_ASSERT(prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

static void test_is_not_module_importmap(void)
{
    /* "importmap" starts with "import" but no space after 7 chars */
    const char *code = "importmap {}";
    DTR_ASSERT(!prv_code_is_module(code, strlen(code)));
    DTR_PASS();
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    DTR_TEST_BEGIN("test_module_normalize");

    /* Path normalization */
    DTR_RUN_TEST(test_normalize_dot_slash);
    DTR_RUN_TEST(test_normalize_dot_dot_slash);
    DTR_RUN_TEST(test_normalize_double_dot_dot);
    DTR_RUN_TEST(test_normalize_bare_specifier);
    DTR_RUN_TEST(test_normalize_no_dir_in_base);
    DTR_RUN_TEST(test_normalize_dot_dot_past_root);
    DTR_RUN_TEST(test_normalize_chained_dot_slash);
    DTR_RUN_TEST(test_normalize_nested_relative);

    /* Code-is-module heuristic */
    DTR_RUN_TEST(test_is_module_import);
    DTR_RUN_TEST(test_is_module_export);
    DTR_RUN_TEST(test_is_module_export_default);
    DTR_RUN_TEST(test_is_module_with_leading_whitespace);
    DTR_RUN_TEST(test_is_not_module_midline_import);
    DTR_RUN_TEST(test_is_not_module_plain_script);
    DTR_RUN_TEST(test_is_not_module_empty);
    DTR_RUN_TEST(test_is_module_after_line_comment);
    DTR_RUN_TEST(test_is_module_after_block_comment);
    DTR_RUN_TEST(test_is_not_module_importmap);

    DTR_TEST_END();
}
