/**
 * \file            test_postfx.c
 * \brief           Unit tests for the post-processing pipeline
 */

#include "postfx.h"
#include "test_harness.h"

#include <string.h>

#define TW 16
#define TH 16

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void test_postfx_create_destroy(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    DTR_ASSERT(pfx != NULL);
    DTR_ASSERT_EQ_INT(pfx->width, TW);
    DTR_ASSERT_EQ_INT(pfx->height, TH);
    DTR_ASSERT_EQ_INT(pfx->count, 0);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_destroy_null(void)
{
    dtr_postfx_destroy(NULL); /* must not crash */
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Stack management                                                   */
/* ------------------------------------------------------------------ */

static void test_postfx_push_pop(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    dtr_postfx_push(pfx, DTR_POSTFX_SCANLINES, 0.8f);
    DTR_ASSERT_EQ_INT(pfx->count, 1);
    DTR_ASSERT_EQ_INT(pfx->stack[0].id, DTR_POSTFX_SCANLINES);
    DTR_ASSERT_NEAR(pfx->stack[0].strength, 0.8f, 0.001);

    dtr_postfx_push(pfx, DTR_POSTFX_CRT, 0.5f);
    DTR_ASSERT_EQ_INT(pfx->count, 2);

    dtr_postfx_pop(pfx);
    DTR_ASSERT_EQ_INT(pfx->count, 1);

    dtr_postfx_pop(pfx);
    DTR_ASSERT_EQ_INT(pfx->count, 0);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_pop_empty(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    dtr_postfx_pop(pfx); /* pop on empty — should not crash */
    DTR_ASSERT_EQ_INT(pfx->count, 0);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_push_overflow(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    for (int32_t idx = 0; idx < DTR_POSTFX_MAX_STACK + 2; ++idx) {
        dtr_postfx_push(pfx, DTR_POSTFX_BLOOM, 1.0f);
    }
    /* Should cap at max */
    DTR_ASSERT_EQ_INT(pfx->count, DTR_POSTFX_MAX_STACK);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_clear(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    dtr_postfx_push(pfx, DTR_POSTFX_CRT, 1.0f);
    dtr_postfx_push(pfx, DTR_POSTFX_BLOOM, 0.5f);
    DTR_ASSERT_EQ_INT(pfx->count, 2);

    dtr_postfx_clear(pfx);
    DTR_ASSERT_EQ_INT(pfx->count, 0);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  set_param                                                          */
/* ------------------------------------------------------------------ */

static void test_postfx_set_param(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    dtr_postfx_push(pfx, DTR_POSTFX_CRT, 1.0f);
    dtr_postfx_set_param(pfx, 0, 0, 42.0f);
    DTR_ASSERT_NEAR(pfx->stack[0].params[0], 42.0f, 0.001);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_set_param_out_of_bounds(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    dtr_postfx_push(pfx, DTR_POSTFX_CRT, 1.0f);

    /* Invalid stack index */
    dtr_postfx_set_param(pfx, -1, 0, 1.0f);
    dtr_postfx_set_param(pfx, 99, 0, 1.0f);

    /* Invalid param index */
    dtr_postfx_set_param(pfx, 0, -1, 1.0f);
    dtr_postfx_set_param(pfx, 0, DTR_POSTFX_MAX_PARAMS, 1.0f);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Name resolution                                                    */
/* ------------------------------------------------------------------ */

static void test_postfx_id_from_name(void)
{
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("none"), DTR_POSTFX_NONE);
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("crt"), DTR_POSTFX_CRT);
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("scanlines"), DTR_POSTFX_SCANLINES);
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("bloom"), DTR_POSTFX_BLOOM);
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("aberration"), DTR_POSTFX_ABERRATION);

    /* Case insensitive */
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("CRT"), DTR_POSTFX_CRT);
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("Bloom"), DTR_POSTFX_BLOOM);

    /* Unknown returns NONE */
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name("unknown"), DTR_POSTFX_NONE);
    DTR_ASSERT_EQ_INT(dtr_postfx_id_from_name(NULL), DTR_POSTFX_NONE);

    DTR_PASS();
}

static void test_postfx_available(void)
{
    int32_t      count = 0;
    const char **names = dtr_postfx_available(&count);

    DTR_ASSERT(names != NULL);
    DTR_ASSERT_EQ_INT(count, DTR_POSTFX_BUILTIN_COUNT);
    DTR_ASSERT(strcmp(names[0], "none") == 0);
    DTR_ASSERT(strcmp(names[1], "crt") == 0);

    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  use (replace stack with single effect)                             */
/* ------------------------------------------------------------------ */

static void test_postfx_use(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);

    dtr_postfx_push(pfx, DTR_POSTFX_BLOOM, 0.5f);
    dtr_postfx_push(pfx, DTR_POSTFX_SCANLINES, 0.3f);
    DTR_ASSERT_EQ_INT(pfx->count, 2);

    int32_t idx = dtr_postfx_use(pfx, "crt");
    DTR_ASSERT_EQ_INT(idx, 0);
    DTR_ASSERT_EQ_INT(pfx->count, 1);
    DTR_ASSERT_EQ_INT(pfx->stack[0].id, DTR_POSTFX_CRT);
    DTR_ASSERT_NEAR(pfx->stack[0].strength, 1.0f, 0.001);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  apply                                                              */
/* ------------------------------------------------------------------ */

static void test_postfx_apply_empty_noop(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    memset(pixels, 0xAB, sizeof(pixels));

    uint32_t saved = pixels[0];
    dtr_postfx_apply(pfx, pixels, TW, TH);
    DTR_ASSERT_EQ_INT(pixels[0], saved); /* no change with empty stack */

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_apply_null_noop(void)
{
    uint32_t pixels[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

    dtr_postfx_apply(NULL, pixels, 2, 2);
    DTR_ASSERT_EQ_INT(pixels[0], 0xFFFFFFFF); /* unchanged */

    DTR_PASS();
}

static void test_postfx_apply_scanlines(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with gray, fully opaque (RGBA8888: R=0x80, G=0x80, B=0x80, A=0xFF) */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0x808080FFu;
    }

    dtr_postfx_push(pfx, DTR_POSTFX_SCANLINES, 1.0f);
    dtr_postfx_apply(pfx, pixels, TW, TH);

    /* Even rows should be darkened, odd rows unchanged */
    uint32_t even_r = (pixels[0] >> 24) & 0xFF;
    uint32_t odd_r  = (pixels[TW] >> 24) & 0xFF; /* row 1 */

    DTR_ASSERT(even_r < 0x80); /* darkened */
    DTR_ASSERT(odd_r == 0x80); /* unchanged */

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_apply_aberration(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with uniform colour (RGBA8888: R=0x40, G=0x40, B=0x40, A=0xFF) */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0x404040FFu;
    }

    dtr_postfx_push(pfx, DTR_POSTFX_ABERRATION, 2.0f);
    dtr_postfx_apply(pfx, pixels, TW, TH);

    /* Scratch buffer should have been allocated */
    DTR_ASSERT(pfx->scratch != NULL);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_apply_crt(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with uniform gray (R=0x80, G=0x80, B=0x80, A=0xFF) */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0x808080FFu;
    }

    /* Use strength > 0.3 to trigger color bleeding path */
    dtr_postfx_push(pfx, DTR_POSTFX_CRT, 1.0f);
    dtr_postfx_apply(pfx, pixels, TW, TH);

    /* Center pixel should be brighter than corner due to vignette */
    uint32_t center_r = (pixels[(TH / 2) * TW + (TW / 2)] >> 24) & 0xFF;
    uint32_t corner_r = (pixels[0] >> 24) & 0xFF;
    DTR_ASSERT(center_r > corner_r);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_apply_bloom(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with bright pixels (above threshold) */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0xF0F0F0FFu; /* R=0xF0, G=0xF0, B=0xF0, A=0xFF */
    }

    uint32_t before_r = (pixels[0] >> 24) & 0xFF;

    dtr_postfx_push(pfx, DTR_POSTFX_BLOOM, 1.0f);
    dtr_postfx_apply(pfx, pixels, TW, TH);

    /* Bright pixels should be boosted (or clamped at 255) */
    uint32_t after_r = (pixels[0] >> 24) & 0xFF;
    DTR_ASSERT(after_r >= before_r);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_apply_bloom_below_threshold(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with dim pixels (below bloom threshold) */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0x202020FFu; /* R=0x20, G=0x20, B=0x20, A=0xFF */
    }

    uint32_t before = pixels[0];

    dtr_postfx_push(pfx, DTR_POSTFX_BLOOM, 0.5f);
    dtr_postfx_apply(pfx, pixels, TW, TH);

    /* Dim pixels should be unchanged */
    DTR_ASSERT_EQ_INT(pixels[0], before);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_apply_multiple_stacked(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with mid-gray */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0x808080FFu;
    }

    uint32_t before = pixels[0];

    /* Stack scanlines + CRT */
    dtr_postfx_push(pfx, DTR_POSTFX_SCANLINES, 0.8f);
    dtr_postfx_push(pfx, DTR_POSTFX_CRT, 0.5f);
    DTR_ASSERT_EQ_INT(pfx->count, 2);

    dtr_postfx_apply(pfx, pixels, TW, TH);

    /* Both effects should have modified the corner pixel */
    DTR_ASSERT(pixels[0] != before);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

static void test_postfx_apply_crt_low_strength(void)
{
    dtr_postfx_t *pfx = dtr_postfx_create(TW, TH);
    uint32_t      pixels[TW * TH];

    /* Fill with uniform gray */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        pixels[idx] = 0x808080FFu;
    }

    /* Use strength <= 0.3 to skip color bleeding path */
    dtr_postfx_push(pfx, DTR_POSTFX_CRT, 0.2f);
    dtr_postfx_apply(pfx, pixels, TW, TH);

    /* Should still apply vignette (corner darker than center) */
    uint32_t center_r = (pixels[(TH / 2) * TW + (TW / 2)] >> 24) & 0xFF;
    uint32_t corner_r = (pixels[0] >> 24) & 0xFF;
    DTR_ASSERT(center_r >= corner_r);

    dtr_postfx_destroy(pfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Runner                                                             */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("test_postfx");

    /* Lifecycle */
    DTR_RUN_TEST(test_postfx_create_destroy);
    DTR_RUN_TEST(test_postfx_destroy_null);

    /* Stack management */
    DTR_RUN_TEST(test_postfx_push_pop);
    DTR_RUN_TEST(test_postfx_pop_empty);
    DTR_RUN_TEST(test_postfx_push_overflow);
    DTR_RUN_TEST(test_postfx_clear);

    /* Parameters */
    DTR_RUN_TEST(test_postfx_set_param);
    DTR_RUN_TEST(test_postfx_set_param_out_of_bounds);

    /* Name resolution */
    DTR_RUN_TEST(test_postfx_id_from_name);
    DTR_RUN_TEST(test_postfx_available);
    DTR_RUN_TEST(test_postfx_use);

    /* Apply pipeline */
    DTR_RUN_TEST(test_postfx_apply_empty_noop);
    DTR_RUN_TEST(test_postfx_apply_null_noop);
    DTR_RUN_TEST(test_postfx_apply_scanlines);
    DTR_RUN_TEST(test_postfx_apply_aberration);
    DTR_RUN_TEST(test_postfx_apply_crt);
    DTR_RUN_TEST(test_postfx_apply_bloom);
    DTR_RUN_TEST(test_postfx_apply_bloom_below_threshold);
    DTR_RUN_TEST(test_postfx_apply_multiple_stacked);
    DTR_RUN_TEST(test_postfx_apply_crt_low_strength);

    DTR_TEST_END();
}
