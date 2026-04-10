/**
 * \file            test_touch.c
 * \brief           Unit tests for touch state subsystem
 */

#include "touch.h"
#include "test_harness.h"

#include <stdio.h>

/* ================================================================== */
/*  Lifecycle                                                          */
/* ================================================================== */

static void test_touch_create_destroy(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);
    DTR_ASSERT(touch != NULL);
    DTR_ASSERT_EQ_INT(touch->count, 0);
    DTR_ASSERT_EQ_INT(touch->fb_width, 320);
    DTR_ASSERT_EQ_INT(touch->fb_height, 180);
    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_destroy_null(void)
{
    dtr_touch_destroy(NULL); /* must not crash */
    DTR_PASS();
}

/* ================================================================== */
/*  Queries on empty state                                             */
/* ================================================================== */

static void test_touch_count_default_zero(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);
    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), 0);
    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_active_default_false(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);
    for (int32_t i = 0; i < DTR_MAX_FINGERS; ++i) {
        DTR_ASSERT(!dtr_touch_active(touch, i));
    }
    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_queries_out_of_range(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);
    DTR_ASSERT(!dtr_touch_active(touch, -1));
    DTR_ASSERT(!dtr_touch_active(touch, DTR_MAX_FINGERS));
    DTR_ASSERT(!dtr_touch_pressed(touch, -1));
    DTR_ASSERT(!dtr_touch_released(touch, -1));
    DTR_ASSERT_NEAR(dtr_touch_x(touch, -1), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_touch_y(touch, -1), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_touch_dx(touch, -1), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_touch_dy(touch, -1), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_touch_pressure(touch, -1), 0.0f, 0.001f);
    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_queries_null(void)
{
    DTR_ASSERT_EQ_INT(dtr_touch_count(NULL), 0);
    DTR_ASSERT(!dtr_touch_active(NULL, 0));
    DTR_ASSERT(!dtr_touch_pressed(NULL, 0));
    DTR_ASSERT(!dtr_touch_released(NULL, 0));
    DTR_ASSERT_NEAR(dtr_touch_x(NULL, 0), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_touch_y(NULL, 0), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_touch_pressure(NULL, 0), 0.0f, 0.001f);
    DTR_PASS();
}

/* ================================================================== */
/*  Finger down / up                                                   */
/* ================================================================== */

static void test_touch_finger_down(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    /* Finger down at normalized (0.5, 0.5) */
    dtr_touch_on_down(touch, 42, 0.5f, 0.5f, 0.8f);

    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), 1);
    DTR_ASSERT(dtr_touch_active(touch, 0));
    DTR_ASSERT(dtr_touch_pressed(touch, 0));
    DTR_ASSERT(!dtr_touch_released(touch, 0));
    DTR_ASSERT_NEAR(dtr_touch_x(touch, 0), 160.0f, 0.1f);
    DTR_ASSERT_NEAR(dtr_touch_y(touch, 0), 90.0f, 0.1f);
    DTR_ASSERT_NEAR(dtr_touch_pressure(touch, 0), 0.8f, 0.001f);

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_finger_up(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    dtr_touch_on_down(touch, 42, 0.5f, 0.5f, 1.0f);
    dtr_touch_update(touch); /* clear pressed latch */

    dtr_touch_on_up(touch, 42);

    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), 0);
    DTR_ASSERT(!dtr_touch_active(touch, 0));
    DTR_ASSERT(dtr_touch_released(touch, 0));

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_pressed_clears_after_update(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    dtr_touch_on_down(touch, 1, 0.0f, 0.0f, 1.0f);
    DTR_ASSERT(dtr_touch_pressed(touch, 0));

    dtr_touch_update(touch);
    DTR_ASSERT(!dtr_touch_pressed(touch, 0));
    DTR_ASSERT(dtr_touch_active(touch, 0)); /* still held */

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_released_clears_after_update(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    dtr_touch_on_down(touch, 1, 0.0f, 0.0f, 1.0f);
    dtr_touch_update(touch);
    dtr_touch_on_up(touch, 1);
    DTR_ASSERT(dtr_touch_released(touch, 0));

    dtr_touch_update(touch);
    DTR_ASSERT(!dtr_touch_released(touch, 0));

    dtr_touch_destroy(touch);
    DTR_PASS();
}

/* ================================================================== */
/*  Motion                                                             */
/* ================================================================== */

static void test_touch_motion(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    dtr_touch_on_down(touch, 10, 0.0f, 0.0f, 1.0f);
    dtr_touch_update(touch); /* clear dx/dy from down */

    /* Move to (0.5, 0.25) with delta (0.5, 0.25) */
    dtr_touch_on_motion(touch, 10, 0.5f, 0.25f, 0.5f, 0.25f, 0.9f);

    DTR_ASSERT_NEAR(dtr_touch_x(touch, 0), 160.0f, 0.1f);
    DTR_ASSERT_NEAR(dtr_touch_y(touch, 0), 45.0f, 0.1f);
    DTR_ASSERT_NEAR(dtr_touch_dx(touch, 0), 160.0f, 0.1f); /* 0.5 * 320 */
    DTR_ASSERT_NEAR(dtr_touch_dy(touch, 0), 45.0f, 0.1f);  /* 0.25 * 180 */
    DTR_ASSERT_NEAR(dtr_touch_pressure(touch, 0), 0.9f, 0.001f);

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_dx_dy_reset_after_update(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    dtr_touch_on_down(touch, 10, 0.0f, 0.0f, 1.0f);
    dtr_touch_update(touch);
    dtr_touch_on_motion(touch, 10, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f);

    /* Deltas present after motion */
    DTR_ASSERT(dtr_touch_dx(touch, 0) != 0.0f);

    dtr_touch_update(touch);
    /* Deltas cleared */
    DTR_ASSERT_NEAR(dtr_touch_dx(touch, 0), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_touch_dy(touch, 0), 0.0f, 0.001f);

    dtr_touch_destroy(touch);
    DTR_PASS();
}

/* ================================================================== */
/*  Multi-finger                                                       */
/* ================================================================== */

static void test_touch_multi_finger(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    dtr_touch_on_down(touch, 1, 0.0f, 0.0f, 1.0f);
    dtr_touch_on_down(touch, 2, 1.0f, 1.0f, 0.5f);
    dtr_touch_on_down(touch, 3, 0.5f, 0.5f, 0.3f);

    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), 3);
    DTR_ASSERT(dtr_touch_active(touch, 0));
    DTR_ASSERT(dtr_touch_active(touch, 1));
    DTR_ASSERT(dtr_touch_active(touch, 2));

    /* Remove middle finger */
    dtr_touch_on_up(touch, 2);
    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), 2);
    DTR_ASSERT(!dtr_touch_active(touch, 1));

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_max_fingers_exceeded(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    /* Fill all slots */
    for (int64_t i = 0; i < DTR_MAX_FINGERS; ++i) {
        dtr_touch_on_down(touch, i + 1, 0.0f, 0.0f, 1.0f);
    }
    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), DTR_MAX_FINGERS);

    /* Extra finger should be silently ignored */
    dtr_touch_on_down(touch, 999, 0.5f, 0.5f, 1.0f);
    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), DTR_MAX_FINGERS);

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_update_null(void)
{
    dtr_touch_update(NULL); /* must not crash */
    DTR_PASS();
}

static void test_touch_event_null(void)
{
    /* Event handlers on NULL must not crash */
    dtr_touch_on_down(NULL, 1, 0.0f, 0.0f, 1.0f);
    dtr_touch_on_motion(NULL, 1, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    dtr_touch_on_up(NULL, 1);
    DTR_PASS();
}

static void test_touch_motion_unknown_finger(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    /* Motion for a finger that was never pressed — should be a no-op */
    dtr_touch_on_motion(touch, 99, 0.5f, 0.5f, 0.1f, 0.1f, 0.5f);
    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), 0);

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_up_unknown_finger(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    /* Up for a finger that was never pressed — should be a no-op */
    dtr_touch_on_up(touch, 99);
    DTR_ASSERT_EQ_INT(dtr_touch_count(touch), 0);

    dtr_touch_destroy(touch);
    DTR_PASS();
}

static void test_touch_coord_mapping(void)
{
    dtr_touch_state_t *touch;

    touch = dtr_touch_create(320, 180);

    /* (0, 0) → top-left corner */
    dtr_touch_on_down(touch, 1, 0.0f, 0.0f, 1.0f);
    DTR_ASSERT_NEAR(dtr_touch_x(touch, 0), 0.0f, 0.01f);
    DTR_ASSERT_NEAR(dtr_touch_y(touch, 0), 0.0f, 0.01f);
    dtr_touch_on_up(touch, 1);
    dtr_touch_update(touch);

    /* (1, 1) → bottom-right corner */
    dtr_touch_on_down(touch, 2, 1.0f, 1.0f, 1.0f);
    DTR_ASSERT_NEAR(dtr_touch_x(touch, 0), 320.0f, 0.01f);
    DTR_ASSERT_NEAR(dtr_touch_y(touch, 0), 180.0f, 0.01f);

    dtr_touch_destroy(touch);
    DTR_PASS();
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    DTR_TEST_BEGIN("test_touch");

    DTR_RUN_TEST(test_touch_create_destroy);
    DTR_RUN_TEST(test_touch_destroy_null);
    DTR_RUN_TEST(test_touch_count_default_zero);
    DTR_RUN_TEST(test_touch_active_default_false);
    DTR_RUN_TEST(test_touch_queries_out_of_range);
    DTR_RUN_TEST(test_touch_queries_null);
    DTR_RUN_TEST(test_touch_finger_down);
    DTR_RUN_TEST(test_touch_finger_up);
    DTR_RUN_TEST(test_touch_pressed_clears_after_update);
    DTR_RUN_TEST(test_touch_released_clears_after_update);
    DTR_RUN_TEST(test_touch_motion);
    DTR_RUN_TEST(test_touch_dx_dy_reset_after_update);
    DTR_RUN_TEST(test_touch_multi_finger);
    DTR_RUN_TEST(test_touch_max_fingers_exceeded);
    DTR_RUN_TEST(test_touch_update_null);
    DTR_RUN_TEST(test_touch_event_null);
    DTR_RUN_TEST(test_touch_motion_unknown_finger);
    DTR_RUN_TEST(test_touch_up_unknown_finger);
    DTR_RUN_TEST(test_touch_coord_mapping);

    DTR_TEST_END();
}
