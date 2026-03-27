/**
 * \file            test_tween.c
 * \brief           Unit tests for tween engine — easing + managed pool
 */

#include "tween.h"
#include "test_harness.h"

#define EPS 0.001

/* ------------------------------------------------------------------ */
/*  Easing: boundary values                                            */
/* ------------------------------------------------------------------ */

static void test_ease_linear(void)
{
    DTR_ASSERT_NEAR(dtr_ease_apply(DTR_EASE_LINEAR, 0.0), 0.0, EPS);
    DTR_ASSERT_NEAR(dtr_ease_apply(DTR_EASE_LINEAR, 0.5), 0.5, EPS);
    DTR_ASSERT_NEAR(dtr_ease_apply(DTR_EASE_LINEAR, 1.0), 1.0, EPS);
    DTR_PASS();
}

static void test_ease_clamp(void)
{
    /* Values outside 0..1 should clamp */
    DTR_ASSERT_NEAR(dtr_ease_apply(DTR_EASE_IN_QUAD, -0.5), 0.0, EPS);
    DTR_ASSERT_NEAR(dtr_ease_apply(DTR_EASE_IN_QUAD, 1.5), 1.0, EPS);
    DTR_PASS();
}

static void test_ease_boundaries(void)
{
    int32_t idx;

    /* Every easing should return 0 at t=0 and 1 at t=1 */
    for (idx = 0; idx < DTR_EASE_COUNT; ++idx) {
        DTR_ASSERT_NEAR(dtr_ease_apply((dtr_ease_t)idx, 0.0), 0.0, EPS);
        DTR_ASSERT_NEAR(dtr_ease_apply((dtr_ease_t)idx, 1.0), 1.0, EPS);
    }
    DTR_PASS();
}

static void test_ease_out_quad(void)
{
    double val;

    val = dtr_ease_apply(DTR_EASE_OUT_QUAD, 0.5);
    DTR_ASSERT(val > 0.5);    /* Out-ease should be ahead at midpoint */
    DTR_PASS();
}

static void test_ease_in_quad(void)
{
    double val;

    val = dtr_ease_apply(DTR_EASE_IN_QUAD, 0.5);
    DTR_ASSERT(val < 0.5);    /* In-ease should be behind at midpoint */
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Easing: name lookup                                                */
/* ------------------------------------------------------------------ */

static void test_ease_from_name(void)
{
    DTR_ASSERT_EQ_INT(dtr_ease_from_name("linear"), DTR_EASE_LINEAR);
    DTR_ASSERT_EQ_INT(dtr_ease_from_name("easeInQuad"), DTR_EASE_IN_QUAD);
    DTR_ASSERT_EQ_INT(dtr_ease_from_name("easeOutBounce"), DTR_EASE_OUT_BOUNCE);
    DTR_ASSERT_EQ_INT(dtr_ease_from_name("easeIn"), DTR_EASE_IN_QUAD);
    DTR_ASSERT_EQ_INT(dtr_ease_from_name("easeOut"), DTR_EASE_OUT_QUAD);
    DTR_ASSERT_EQ_INT(dtr_ease_from_name("easeInOut"), DTR_EASE_IN_OUT_QUAD);
    DTR_ASSERT_EQ_INT(dtr_ease_from_name("unknown"), DTR_EASE_LINEAR);
    DTR_ASSERT_EQ_INT(dtr_ease_from_name(NULL), DTR_EASE_LINEAR);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Managed tweens: add + val + done                                   */
/* ------------------------------------------------------------------ */

static void test_tween_add_val(void)
{
    dtr_tween_t twn;
    int32_t     idx;
    double      val;

    dtr_tween_init(&twn);
    idx = dtr_tween_add(&twn, 0.0, 100.0, 1.0, DTR_EASE_LINEAR, 0.0);
    DTR_ASSERT(idx >= 0);

    /* At t=0 should be at start */
    val = dtr_tween_val(&twn, idx);
    DTR_ASSERT_NEAR(val, 0.0, EPS);

    /* Tick half-way */
    dtr_tween_tick(&twn, 0.5);
    val = dtr_tween_val(&twn, idx);
    DTR_ASSERT_NEAR(val, 50.0, EPS);

    /* Tick to completion */
    dtr_tween_tick(&twn, 0.5);
    val = dtr_tween_val(&twn, idx);
    DTR_ASSERT_NEAR(val, 100.0, EPS);
    DTR_ASSERT(dtr_tween_done(&twn, idx));
    DTR_PASS();
}

static void test_tween_delay(void)
{
    dtr_tween_t twn;
    int32_t     idx;
    double      val;

    dtr_tween_init(&twn);
    idx = dtr_tween_add(&twn, 0.0, 100.0, 1.0, DTR_EASE_LINEAR, 0.5);

    /* During delay, value should be at start */
    dtr_tween_tick(&twn, 0.3);
    val = dtr_tween_val(&twn, idx);
    DTR_ASSERT_NEAR(val, 0.0, EPS);

    /* Tick past delay — leftover carries over */
    dtr_tween_tick(&twn, 0.4);    /* 0.2 into delay left, 0.2 into tween */
    val = dtr_tween_val(&twn, idx);
    DTR_ASSERT(val > 0.0);
    DTR_ASSERT(!dtr_tween_done(&twn, idx));
    DTR_PASS();
}

static void test_tween_cancel(void)
{
    dtr_tween_t twn;
    int32_t     idx;

    dtr_tween_init(&twn);
    idx = dtr_tween_add(&twn, 0.0, 100.0, 1.0, DTR_EASE_LINEAR, 0.0);
    dtr_tween_tick(&twn, 0.5);
    dtr_tween_cancel(&twn, idx);

    /* After cancel, done should return true (inactive) */
    DTR_ASSERT(dtr_tween_done(&twn, idx));
    DTR_PASS();
}

static void test_tween_cancel_all(void)
{
    dtr_tween_t twn;
    int32_t     idx0;
    int32_t     idx1;

    dtr_tween_init(&twn);
    idx0 = dtr_tween_add(&twn, 0.0, 10.0, 1.0, DTR_EASE_LINEAR, 0.0);
    idx1 = dtr_tween_add(&twn, 0.0, 20.0, 2.0, DTR_EASE_LINEAR, 0.0);
    dtr_tween_cancel_all(&twn);

    DTR_ASSERT(dtr_tween_done(&twn, idx0));
    DTR_ASSERT(dtr_tween_done(&twn, idx1));
    DTR_PASS();
}

static void test_tween_pool_full(void)
{
    dtr_tween_t twn;
    int32_t     idx;
    int32_t     cnt;

    dtr_tween_init(&twn);
    for (cnt = 0; cnt < CONSOLE_MAX_TWEENS; ++cnt) {
        idx = dtr_tween_add(&twn, 0.0, 1.0, 1.0, DTR_EASE_LINEAR, 0.0);
        DTR_ASSERT(idx >= 0);
    }
    /* Pool should be full */
    idx = dtr_tween_add(&twn, 0.0, 1.0, 1.0, DTR_EASE_LINEAR, 0.0);
    DTR_ASSERT_EQ_INT(idx, -1);
    DTR_PASS();
}

static void test_tween_reuse_slot(void)
{
    dtr_tween_t twn;
    int32_t     idx0;
    int32_t     idx1;

    dtr_tween_init(&twn);
    idx0 = dtr_tween_add(&twn, 0.0, 1.0, 1.0, DTR_EASE_LINEAR, 0.0);
    dtr_tween_cancel(&twn, idx0);

    /* Cancelled slot should be reusable */
    idx1 = dtr_tween_add(&twn, 0.0, 1.0, 1.0, DTR_EASE_LINEAR, 0.0);
    DTR_ASSERT_EQ_INT(idx1, idx0);
    DTR_PASS();
}

static void test_tween_eased(void)
{
    dtr_tween_t twn;
    int32_t     idx;
    double      val;

    dtr_tween_init(&twn);
    idx = dtr_tween_add(&twn, 0.0, 100.0, 1.0, DTR_EASE_IN_QUAD, 0.0);
    dtr_tween_tick(&twn, 0.5);
    val = dtr_tween_val(&twn, idx);

    /* easeInQuad at t=0.5 → 0.25, so val ≈ 25 */
    DTR_ASSERT_NEAR(val, 25.0, 1.0);
    DTR_PASS();
}

static void test_tween_val_invalid(void)
{
    dtr_tween_t twn;
    double      val;

    dtr_tween_init(&twn);
    val = dtr_tween_val(&twn, -1);
    DTR_ASSERT_NEAR(val, 0.0, EPS);
    val = dtr_tween_val(&twn, 999);
    DTR_ASSERT_NEAR(val, 0.0, EPS);
    DTR_PASS();
}

static void test_tween_done_invalid(void)
{
    dtr_tween_t twn;

    dtr_tween_init(&twn);
    DTR_ASSERT(dtr_tween_done(&twn, -1));
    DTR_ASSERT(dtr_tween_done(&twn, 999));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("Tween Engine");

    DTR_RUN_TEST(test_ease_linear);
    DTR_RUN_TEST(test_ease_clamp);
    DTR_RUN_TEST(test_ease_boundaries);
    DTR_RUN_TEST(test_ease_out_quad);
    DTR_RUN_TEST(test_ease_in_quad);
    DTR_RUN_TEST(test_ease_from_name);
    DTR_RUN_TEST(test_tween_add_val);
    DTR_RUN_TEST(test_tween_delay);
    DTR_RUN_TEST(test_tween_cancel);
    DTR_RUN_TEST(test_tween_cancel_all);
    DTR_RUN_TEST(test_tween_pool_full);
    DTR_RUN_TEST(test_tween_reuse_slot);
    DTR_RUN_TEST(test_tween_eased);
    DTR_RUN_TEST(test_tween_val_invalid);
    DTR_RUN_TEST(test_tween_done_invalid);

    DTR_TEST_END();
}
