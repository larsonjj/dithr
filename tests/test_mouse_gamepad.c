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
    mvn_mouse_state_t *mouse;

    mouse = mvn_mouse_create();
    MVN_ASSERT(mouse != NULL);
    MVN_ASSERT(mouse->visible == true);
    MVN_ASSERT_NEAR(mouse->scale_x, 1.0f, 0.001f);
    MVN_ASSERT_NEAR(mouse->scale_y, 1.0f, 0.001f);
    MVN_ASSERT_NEAR(mouse->offset_x, 0.0f, 0.001f);
    MVN_ASSERT_NEAR(mouse->offset_y, 0.0f, 0.001f);
    mvn_mouse_destroy(mouse);
    MVN_PASS();
}

static void test_mouse_destroy_null(void)
{
    mvn_mouse_destroy(NULL); /* must not crash */
    MVN_PASS();
}

static void test_mouse_btn_default_false(void)
{
    mvn_mouse_state_t *mouse;

    mouse = mvn_mouse_create();
    MVN_ASSERT(!mvn_mouse_btn(mouse, MVN_MOUSE_LEFT));
    MVN_ASSERT(!mvn_mouse_btn(mouse, MVN_MOUSE_RIGHT));
    MVN_ASSERT(!mvn_mouse_btn(mouse, MVN_MOUSE_MIDDLE));
    mvn_mouse_destroy(mouse);
    MVN_PASS();
}

static void test_mouse_btn_set_and_query(void)
{
    mvn_mouse_state_t *mouse;

    mouse = mvn_mouse_create();

    /* Simulate a left-click */
    mouse->btn_current[MVN_MOUSE_LEFT] = true;
    MVN_ASSERT(mvn_mouse_btn(mouse, MVN_MOUSE_LEFT));
    MVN_ASSERT(!mvn_mouse_btn(mouse, MVN_MOUSE_RIGHT));

    mouse->btn_current[MVN_MOUSE_LEFT] = false;
    MVN_ASSERT(!mvn_mouse_btn(mouse, MVN_MOUSE_LEFT));

    mvn_mouse_destroy(mouse);
    MVN_PASS();
}

static void test_mouse_btnp(void)
{
    mvn_mouse_state_t *mouse;

    mouse = mvn_mouse_create();

    /* Frame 1: press left */
    mouse->btn_current[MVN_MOUSE_LEFT] = true;
    mouse->btn_pressed[MVN_MOUSE_LEFT] = true;
    MVN_ASSERT(mvn_mouse_btnp(mouse, MVN_MOUSE_LEFT));

    /* Frame 2: held */
    mvn_mouse_update(mouse);
    MVN_ASSERT(!mvn_mouse_btnp(mouse, MVN_MOUSE_LEFT));

    /* Frame 3: release then re-press */
    mvn_mouse_update(mouse);
    mouse->btn_current[MVN_MOUSE_LEFT] = false;
    MVN_ASSERT(!mvn_mouse_btnp(mouse, MVN_MOUSE_LEFT));

    mvn_mouse_update(mouse);
    mouse->btn_current[MVN_MOUSE_LEFT] = true;
    mouse->btn_pressed[MVN_MOUSE_LEFT] = true;
    MVN_ASSERT(mvn_mouse_btnp(mouse, MVN_MOUSE_LEFT));

    mvn_mouse_destroy(mouse);
    MVN_PASS();
}

static void test_mouse_btn_out_of_range(void)
{
    mvn_mouse_state_t *mouse;

    mouse = mvn_mouse_create();
    MVN_ASSERT(!mvn_mouse_btn(mouse, MVN_MOUSE_BTN_COUNT));
    MVN_ASSERT(!mvn_mouse_btnp(mouse, MVN_MOUSE_BTN_COUNT));
    MVN_ASSERT(!mvn_mouse_btn(mouse, (mvn_mouse_btn_t)-1));
    mvn_mouse_destroy(mouse);
    MVN_PASS();
}

static void test_mouse_set_mapping(void)
{
    mvn_mouse_state_t *mouse;

    mouse = mvn_mouse_create();
    mvn_mouse_set_mapping(mouse, 2.0f, 3.0f, 10.0f, 20.0f);
    MVN_ASSERT_NEAR(mouse->scale_x, 2.0f, 0.001f);
    MVN_ASSERT_NEAR(mouse->scale_y, 3.0f, 0.001f);
    MVN_ASSERT_NEAR(mouse->offset_x, 10.0f, 0.001f);
    MVN_ASSERT_NEAR(mouse->offset_y, 20.0f, 0.001f);
    mvn_mouse_destroy(mouse);
    MVN_PASS();
}

static void test_mouse_update_copies_previous(void)
{
    mvn_mouse_state_t *mouse;

    mouse = mvn_mouse_create();

    mouse->btn_current[MVN_MOUSE_LEFT] = true;
    mvn_mouse_update(mouse);
    MVN_ASSERT(mouse->btn_previous[MVN_MOUSE_LEFT] == true);

    mouse->btn_current[MVN_MOUSE_LEFT] = false;
    mvn_mouse_update(mouse);
    MVN_ASSERT(mouse->btn_previous[MVN_MOUSE_LEFT] == false);

    mvn_mouse_destroy(mouse);
    MVN_PASS();
}

/* ================================================================== */
/*  Gamepad state tests                                                */
/* ================================================================== */

static void test_gamepad_create_destroy(void)
{
    mvn_gamepad_state_t *gp;

    gp = mvn_gamepad_create();
    MVN_ASSERT(gp != NULL);
    MVN_ASSERT_EQ_INT(gp->count, 0);

    /* All pads should have default deadzone */
    for (int32_t i = 0; i < MVN_MAX_GAMEPADS; ++i) {
        MVN_ASSERT_NEAR(gp->pads[i].deadzone, 0.15f, 0.001f);
        MVN_ASSERT(!gp->pads[i].connected);
    }

    mvn_gamepad_destroy(gp);
    MVN_PASS();
}

static void test_gamepad_destroy_null(void)
{
    mvn_gamepad_destroy(NULL); /* must not crash */
    MVN_PASS();
}

static void test_gamepad_btn_default_false(void)
{
    mvn_gamepad_state_t *gp;

    gp = mvn_gamepad_create();
    MVN_ASSERT(!mvn_gamepad_btn(gp, MVN_PAD_A, 0));
    MVN_ASSERT(!mvn_gamepad_btn(gp, MVN_PAD_B, 0));
    MVN_ASSERT(!mvn_gamepad_btnp(gp, MVN_PAD_A, 0));
    mvn_gamepad_destroy(gp);
    MVN_PASS();
}

static void test_gamepad_count_zero(void)
{
    mvn_gamepad_state_t *gp;

    gp = mvn_gamepad_create();
    MVN_ASSERT_EQ_INT(mvn_gamepad_count(gp), 0);
    mvn_gamepad_destroy(gp);
    MVN_PASS();
}

static void test_gamepad_connected_false(void)
{
    mvn_gamepad_state_t *gp;

    gp = mvn_gamepad_create();
    for (int32_t i = 0; i < MVN_MAX_GAMEPADS; ++i) {
        MVN_ASSERT(!mvn_gamepad_connected(gp, i));
    }
    mvn_gamepad_destroy(gp);
    MVN_PASS();
}

static void test_gamepad_deadzone(void)
{
    mvn_gamepad_state_t *gp;

    gp = mvn_gamepad_create();
    MVN_ASSERT_NEAR(mvn_gamepad_get_deadzone(gp, 0), 0.15f, 0.001f);

    mvn_gamepad_set_deadzone(gp, 0.25f, 0);
    MVN_ASSERT_NEAR(mvn_gamepad_get_deadzone(gp, 0), 0.25f, 0.001f);

    /* Other pads should keep default */
    MVN_ASSERT_NEAR(mvn_gamepad_get_deadzone(gp, 1), 0.15f, 0.001f);

    mvn_gamepad_destroy(gp);
    MVN_PASS();
}

static void test_gamepad_axis_default_zero(void)
{
    mvn_gamepad_state_t *gp;

    gp = mvn_gamepad_create();
    MVN_ASSERT_NEAR(mvn_gamepad_axis(gp, MVN_PAD_AXIS_LX, 0), 0.0f, 0.001f);
    MVN_ASSERT_NEAR(mvn_gamepad_axis(gp, MVN_PAD_AXIS_LY, 0), 0.0f, 0.001f);
    MVN_ASSERT_NEAR(mvn_gamepad_axis(gp, MVN_PAD_AXIS_RX, 0), 0.0f, 0.001f);
    MVN_ASSERT_NEAR(mvn_gamepad_axis(gp, MVN_PAD_AXIS_RY, 0), 0.0f, 0.001f);
    mvn_gamepad_destroy(gp);
    MVN_PASS();
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
