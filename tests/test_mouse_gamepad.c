/**
 * \file            test_mouse_gamepad.c
 * \brief           Unit tests for mouse and gamepad state subsystems
 */

#include <SDL3/SDL.h>
#include <stdio.h>

#include "test_harness.h"
#include "gamepad.h"
#include "mouse.h"

/* ================================================================== */
/*  Mouse state tests                                                  */
/* ================================================================== */

static void test_mouse_create_destroy(void)
{
    dtr_mouse_state_t *mouse;

    mouse = dtr_mouse_create();
    DTR_ASSERT(mouse != NULL);
    DTR_ASSERT(mouse->visible == true);
    DTR_ASSERT_NEAR(mouse->scale_x, 1.0f, 0.001f);
    DTR_ASSERT_NEAR(mouse->scale_y, 1.0f, 0.001f);
    DTR_ASSERT_NEAR(mouse->offset_x, 0.0f, 0.001f);
    DTR_ASSERT_NEAR(mouse->offset_y, 0.0f, 0.001f);
    dtr_mouse_destroy(mouse);
    DTR_PASS();
}

static void test_mouse_destroy_null(void)
{
    dtr_mouse_destroy(NULL); /* must not crash */
    DTR_PASS();
}

static void test_mouse_btn_default_false(void)
{
    dtr_mouse_state_t *mouse;

    mouse = dtr_mouse_create();
    DTR_ASSERT(!dtr_mouse_btn(mouse, DTR_MOUSE_LEFT));
    DTR_ASSERT(!dtr_mouse_btn(mouse, DTR_MOUSE_RIGHT));
    DTR_ASSERT(!dtr_mouse_btn(mouse, DTR_MOUSE_MIDDLE));
    dtr_mouse_destroy(mouse);
    DTR_PASS();
}

static void test_mouse_btn_set_and_query(void)
{
    dtr_mouse_state_t *mouse;

    mouse = dtr_mouse_create();

    /* Simulate a left-click */
    mouse->btn_current[DTR_MOUSE_LEFT] = true;
    DTR_ASSERT(dtr_mouse_btn(mouse, DTR_MOUSE_LEFT));
    DTR_ASSERT(!dtr_mouse_btn(mouse, DTR_MOUSE_RIGHT));

    mouse->btn_current[DTR_MOUSE_LEFT] = false;
    DTR_ASSERT(!dtr_mouse_btn(mouse, DTR_MOUSE_LEFT));

    dtr_mouse_destroy(mouse);
    DTR_PASS();
}

static void test_mouse_btnp(void)
{
    dtr_mouse_state_t *mouse;

    mouse = dtr_mouse_create();

    /* Frame 1: press left */
    mouse->btn_current[DTR_MOUSE_LEFT] = true;
    mouse->btn_pressed[DTR_MOUSE_LEFT] = true;
    DTR_ASSERT(dtr_mouse_btnp(mouse, DTR_MOUSE_LEFT));

    /* Frame 2: held */
    dtr_mouse_update(mouse);
    DTR_ASSERT(!dtr_mouse_btnp(mouse, DTR_MOUSE_LEFT));

    /* Frame 3: release then re-press */
    dtr_mouse_update(mouse);
    mouse->btn_current[DTR_MOUSE_LEFT] = false;
    DTR_ASSERT(!dtr_mouse_btnp(mouse, DTR_MOUSE_LEFT));

    dtr_mouse_update(mouse);
    mouse->btn_current[DTR_MOUSE_LEFT] = true;
    mouse->btn_pressed[DTR_MOUSE_LEFT] = true;
    DTR_ASSERT(dtr_mouse_btnp(mouse, DTR_MOUSE_LEFT));

    dtr_mouse_destroy(mouse);
    DTR_PASS();
}

static void test_mouse_btn_out_of_range(void)
{
    dtr_mouse_state_t *mouse;

    mouse = dtr_mouse_create();
    DTR_ASSERT(!dtr_mouse_btn(mouse, DTR_MOUSE_BTN_COUNT));
    DTR_ASSERT(!dtr_mouse_btnp(mouse, DTR_MOUSE_BTN_COUNT));
    DTR_ASSERT(!dtr_mouse_btn(mouse, (dtr_mouse_btn_t)-1));
    dtr_mouse_destroy(mouse);
    DTR_PASS();
}

static void test_mouse_set_mapping(void)
{
    dtr_mouse_state_t *mouse;

    mouse = dtr_mouse_create();
    dtr_mouse_set_mapping(mouse, 2.0f, 3.0f, 10.0f, 20.0f);
    DTR_ASSERT_NEAR(mouse->scale_x, 2.0f, 0.001f);
    DTR_ASSERT_NEAR(mouse->scale_y, 3.0f, 0.001f);
    DTR_ASSERT_NEAR(mouse->offset_x, 10.0f, 0.001f);
    DTR_ASSERT_NEAR(mouse->offset_y, 20.0f, 0.001f);
    dtr_mouse_destroy(mouse);
    DTR_PASS();
}

static void test_mouse_update_copies_previous(void)
{
    dtr_mouse_state_t *mouse;

    mouse = dtr_mouse_create();

    mouse->btn_current[DTR_MOUSE_LEFT] = true;
    dtr_mouse_update(mouse);
    DTR_ASSERT(mouse->btn_previous[DTR_MOUSE_LEFT] == true);

    mouse->btn_current[DTR_MOUSE_LEFT] = false;
    dtr_mouse_update(mouse);
    DTR_ASSERT(mouse->btn_previous[DTR_MOUSE_LEFT] == false);

    dtr_mouse_destroy(mouse);
    DTR_PASS();
}

/* ================================================================== */
/*  Gamepad state tests                                                */
/* ================================================================== */

static void test_gamepad_create_destroy(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    DTR_ASSERT(gp != NULL);
    DTR_ASSERT_EQ_INT(gp->count, 0);

    /* All pads should have default deadzone */
    for (int32_t i = 0; i < DTR_MAX_GAMEPADS; ++i) {
        DTR_ASSERT_NEAR(gp->pads[i].deadzone, 0.15f, 0.001f);
        DTR_ASSERT(!gp->pads[i].connected);
    }

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_destroy_null(void)
{
    dtr_gamepad_destroy(NULL); /* must not crash */
    DTR_PASS();
}

static void test_gamepad_btn_default_false(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    DTR_ASSERT(!dtr_gamepad_btn(gp, DTR_PAD_A, 0));
    DTR_ASSERT(!dtr_gamepad_btn(gp, DTR_PAD_B, 0));
    DTR_ASSERT(!dtr_gamepad_btnp(gp, DTR_PAD_A, 0));
    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_count_zero(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    DTR_ASSERT_EQ_INT(dtr_gamepad_count(gp), 0);
    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_connected_false(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    for (int32_t i = 0; i < DTR_MAX_GAMEPADS; ++i) {
        DTR_ASSERT(!dtr_gamepad_connected(gp, i));
    }
    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_deadzone(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    DTR_ASSERT_NEAR(dtr_gamepad_get_deadzone(gp, 0), 0.15f, 0.001f);

    dtr_gamepad_set_deadzone(gp, 0.25f, 0);
    DTR_ASSERT_NEAR(dtr_gamepad_get_deadzone(gp, 0), 0.25f, 0.001f);

    /* Other pads should keep default */
    DTR_ASSERT_NEAR(dtr_gamepad_get_deadzone(gp, 1), 0.15f, 0.001f);

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_axis_default_zero(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_LX, 0), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_LY, 0), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_RX, 0), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_RY, 0), 0.0f, 0.001f);
    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("test_mouse_gamepad\n");

    /* Mouse tests */
    test_mouse_create_destroy();
    test_mouse_destroy_null();
    test_mouse_btn_default_false();
    test_mouse_btn_set_and_query();
    test_mouse_btnp();
    test_mouse_btn_out_of_range();
    test_mouse_set_mapping();
    test_mouse_update_copies_previous();

    /* Gamepad tests */
    test_gamepad_create_destroy();
    test_gamepad_destroy_null();
    test_gamepad_btn_default_false();
    test_gamepad_count_zero();
    test_gamepad_connected_false();
    test_gamepad_deadzone();
    test_gamepad_axis_default_zero();

    printf("All tests passed.\n");
    return 0;
}
