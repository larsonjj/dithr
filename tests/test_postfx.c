/**
 * \file            test_postfx.c
 * \brief           Unit tests for the post-processing pipeline
 */

#include <string.h>

#include "postfx.h"
#include "test_harness.h"

#define TW 16
#define TH 16

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void test_postfx_create_destroy(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    MVN_ASSERT(pfx != NULL);
    MVN_ASSERT_EQ_INT(pfx->width, TW);
    MVN_ASSERT_EQ_INT(pfx->height, TH);
    MVN_ASSERT_EQ_INT(pfx->count, 0);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

static void test_postfx_destroy_null(void)
{
    mvn_postfx_destroy(NULL); /* must not crash */
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Stack management                                                   */
/* ------------------------------------------------------------------ */

static void test_postfx_push_pop(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    mvn_postfx_push(pfx, MVN_POSTFX_SCANLINES, 0.8f);
    MVN_ASSERT_EQ_INT(pfx->count, 1);
    MVN_ASSERT_EQ_INT(pfx->stack[0].id, MVN_POSTFX_SCANLINES);
    MVN_ASSERT_NEAR(pfx->stack[0].strength, 0.8f, 0.001);

    mvn_postfx_push(pfx, MVN_POSTFX_CRT, 0.5f);
    MVN_ASSERT_EQ_INT(pfx->count, 2);

    mvn_postfx_pop(pfx);
    MVN_ASSERT_EQ_INT(pfx->count, 1);

    mvn_postfx_pop(pfx);
    MVN_ASSERT_EQ_INT(pfx->count, 0);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

static void test_postfx_pop_empty(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    mvn_postfx_pop(pfx); /* pop on empty — should not crash */
    MVN_ASSERT_EQ_INT(pfx->count, 0);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

static void test_postfx_push_overflow(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    for (int32_t idx = 0; idx < MVN_POSTFX_MAX_STACK + 2; ++idx) {
        mvn_postfx_push(pfx, MVN_POSTFX_BLOOM, 1.0f);
    }
    /* Should cap at max */
    MVN_ASSERT_EQ_INT(pfx->count, MVN_POSTFX_MAX_STACK);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

static void test_postfx_clear(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    mvn_postfx_push(pfx, MVN_POSTFX_CRT, 1.0f);
    mvn_postfx_push(pfx, MVN_POSTFX_BLOOM, 0.5f);
    MVN_ASSERT_EQ_INT(pfx->count, 2);

    mvn_postfx_clear(pfx);
    MVN_ASSERT_EQ_INT(pfx->count, 0);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  set_param                                                          */
/* ------------------------------------------------------------------ */

static void test_postfx_set_param(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    mvn_postfx_push(pfx, MVN_POSTFX_CRT, 1.0f);
    mvn_postfx_set_param(pfx, 0, 0, 42.0f);
    MVN_ASSERT_NEAR(pfx->stack[0].params[0], 42.0f, 0.001);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

static void test_postfx_set_param_out_of_bounds(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    mvn_postfx_push(pfx, MVN_POSTFX_CRT, 1.0f);

    /* Invalid stack index */
    mvn_postfx_set_param(pfx, -1, 0, 1.0f);
    mvn_postfx_set_param(pfx, 99, 0, 1.0f);

    /* Invalid param index */
    mvn_postfx_set_param(pfx, 0, -1, 1.0f);
    mvn_postfx_set_param(pfx, 0, MVN_POSTFX_MAX_PARAMS, 1.0f);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Name resolution                                                    */
/* ------------------------------------------------------------------ */

static void test_postfx_id_from_name(void)
{
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("none"), MVN_POSTFX_NONE);
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("crt"), MVN_POSTFX_CRT);
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("scanlines"), MVN_POSTFX_SCANLINES);
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("bloom"), MVN_POSTFX_BLOOM);
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("aberration"), MVN_POSTFX_ABERRATION);

    /* Case insensitive */
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("CRT"), MVN_POSTFX_CRT);
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("Bloom"), MVN_POSTFX_BLOOM);

    /* Unknown returns NONE */
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name("unknown"), MVN_POSTFX_NONE);
    MVN_ASSERT_EQ_INT(mvn_postfx_id_from_name(NULL), MVN_POSTFX_NONE);

    MVN_PASS();
}

static void test_postfx_available(void)
{
    int32_t      count = 0;
    const char **names = mvn_postfx_available(&count);

    MVN_ASSERT(names != NULL);
    MVN_ASSERT_EQ_INT(count, MVN_POSTFX_BUILTIN_COUNT);
    MVN_ASSERT(strcmp(names[0], "none") == 0);
    MVN_ASSERT(strcmp(names[1], "crt") == 0);

    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  use (replace stack with single effect)                             */
/* ------------------------------------------------------------------ */

static void test_postfx_use(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);

    mvn_postfx_push(pfx, MVN_POSTFX_BLOOM, 0.5f);
    mvn_postfx_push(pfx, MVN_POSTFX_SCANLINES, 0.3f);
    MVN_ASSERT_EQ_INT(pfx->count, 2);

    int32_t idx = mvn_postfx_use(pfx, "crt");
    MVN_ASSERT_EQ_INT(idx, 0);
    MVN_ASSERT_EQ_INT(pfx->count, 1);
    MVN_ASSERT_EQ_INT(pfx->stack[0].id, MVN_POSTFX_CRT);
    MVN_ASSERT_NEAR(pfx->stack[0].strength, 1.0f, 0.001);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  apply                                                              */
/* ------------------------------------------------------------------ */

static void test_postfx_apply_empty_noop(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    memset(pixels, 0xAB, sizeof(pixels));

    uint32_t saved = pixels[0];
    mvn_postfx_apply(pfx, pixels, TW, TH);
    MVN_ASSERT_EQ_INT(pixels[0], saved); /* no change with empty stack */

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

static void test_postfx_apply_null_noop(void)
{
    uint32_t pixels[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

    mvn_postfx_apply(NULL, pixels, 2, 2);
    MVN_ASSERT_EQ_INT(pixels[0], 0xFFFFFFFF); /* unchanged */

    MVN_PASS();
}

static void test_postfx_apply_scanlines(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with white (RGBA) */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0xFF808080u; /* A=0xFF, B=0x80, G=0x80, R=0x80 */
    }

    mvn_postfx_push(pfx, MVN_POSTFX_SCANLINES, 1.0f);
    mvn_postfx_apply(pfx, pixels, TW, TH);

    /* Even rows should be darkened, odd rows unchanged */
    uint32_t even_r = pixels[0] & 0xFF;
    uint32_t odd_r  = pixels[TW] & 0xFF; /* row 1 */

    MVN_ASSERT(even_r < 0x80); /* darkened */
    MVN_ASSERT(odd_r == 0x80); /* unchanged */

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

static void test_postfx_apply_aberration(void)
{
    mvn_postfx_t *pfx = mvn_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with uniform colour */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0xFF404040u;
    }

    mvn_postfx_push(pfx, MVN_POSTFX_ABERRATION, 2.0f);
    mvn_postfx_apply(pfx, pixels, TW, TH);

    /* Scratch buffer should have been allocated */
    MVN_ASSERT(pfx->scratch != NULL);

    mvn_postfx_destroy(pfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Runner                                                             */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("test_postfx\n");

    /* Lifecycle */
    test_postfx_create_destroy();
    test_postfx_destroy_null();

    /* Stack management */
    test_postfx_push_pop();
    test_postfx_pop_empty();
    test_postfx_push_overflow();
    test_postfx_clear();

    /* Parameters */
    test_postfx_set_param();
    test_postfx_set_param_out_of_bounds();

    /* Name resolution */
    test_postfx_id_from_name();
    test_postfx_available();
    test_postfx_use();

    /* Apply pipeline */
    test_postfx_apply_empty_noop();
    test_postfx_apply_null_noop();
    test_postfx_apply_scanlines();
    test_postfx_apply_aberration();

    printf("test_postfx: all passed\n");
    return 0;
}
