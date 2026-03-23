/**
 * \file            test_mouse_gamepad.c
 * \brief           Unit tests for mouse and gamepad state subsystems
 */


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

/* ------------------------------------------------------------------ */
/*  Gamepad button / axis via direct struct manipulation                */
/* ------------------------------------------------------------------ */

static void test_gamepad_btn_set_and_query(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();

    /* Simulate a connected pad (without SDL handle) */
    gp->pads[0].connected = true;
    gp->count              = 1;

    /* Press button A */
    gp->pads[0].btn_current[DTR_PAD_A] = true;
    DTR_ASSERT(dtr_gamepad_btn(gp, DTR_PAD_A, 0));
    DTR_ASSERT(!dtr_gamepad_btn(gp, DTR_PAD_B, 0));

    /* Release */
    gp->pads[0].btn_current[DTR_PAD_A] = false;
    DTR_ASSERT(!dtr_gamepad_btn(gp, DTR_PAD_A, 0));

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_btnp(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    gp->pads[0].connected = true;
    gp->count              = 1;

    /* Frame 1: press A */
    gp->pads[0].btn_pressed[DTR_PAD_A] = true;
    DTR_ASSERT(dtr_gamepad_btnp(gp, DTR_PAD_A, 0));

    /* Clear pressed flag (as update would) */
    gp->pads[0].btn_pressed[DTR_PAD_A] = false;
    DTR_ASSERT(!dtr_gamepad_btnp(gp, DTR_PAD_A, 0));

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_btn_out_of_range(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();

    /* Index out of range */
    DTR_ASSERT(!dtr_gamepad_btn(gp, DTR_PAD_A, -1));
    DTR_ASSERT(!dtr_gamepad_btn(gp, DTR_PAD_A, DTR_MAX_GAMEPADS));
    DTR_ASSERT(!dtr_gamepad_btnp(gp, DTR_PAD_A, -1));
    DTR_ASSERT(!dtr_gamepad_btnp(gp, DTR_PAD_A, DTR_MAX_GAMEPADS));

    /* Button out of range */
    gp->pads[0].connected = true;
    gp->count              = 1;
    DTR_ASSERT(!dtr_gamepad_btn(gp, DTR_PAD_BTN_COUNT, 0));
    DTR_ASSERT(!dtr_gamepad_btnp(gp, DTR_PAD_BTN_COUNT, 0));

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_axis_set_and_query(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    gp->pads[0].connected = true;
    gp->count              = 1;

    gp->pads[0].axes[DTR_PAD_AXIS_LX] = 0.75f;
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_LX, 0), 0.75f, 0.001f);

    /* Out-of-range index */
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_LX, -1), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_LX, DTR_MAX_GAMEPADS), 0.0f, 0.001f);

    /* Out-of-range axis */
    DTR_ASSERT_NEAR(dtr_gamepad_axis(gp, DTR_PAD_AXIS_COUNT, 0), 0.0f, 0.001f);

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_connected_index_out_of_range(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    DTR_ASSERT(!dtr_gamepad_connected(gp, -1));
    DTR_ASSERT(!dtr_gamepad_connected(gp, DTR_MAX_GAMEPADS));
    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_name_default(void)
{
    dtr_gamepad_state_t *gp;
    const char         *name;

    gp = dtr_gamepad_create();

    /* Connected pad without a name set */
    name = dtr_gamepad_name(gp, 0);
    DTR_ASSERT(name != NULL);

    /* Out-of-range returns empty string */
    name = dtr_gamepad_name(gp, -1);
    DTR_ASSERT(name != NULL);
    DTR_ASSERT(name[0] == '\0');

    name = dtr_gamepad_name(gp, DTR_MAX_GAMEPADS);
    DTR_ASSERT(name != NULL);
    DTR_ASSERT(name[0] == '\0');

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_update_deadzone(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();
    gp->pads[0].connected = true;
    gp->pads[0].handle    = NULL; /* update skips pads without handle */
    gp->count              = 1;

    /* Axis below deadzone should be zeroed — but update skips NULL handle */
    gp->pads[0].axes[DTR_PAD_AXIS_LX] = 0.1f;
    dtr_gamepad_update(gp);
    /* With NULL handle, update skips this pad */
    DTR_ASSERT_NEAR(gp->pads[0].axes[DTR_PAD_AXIS_LX], 0.1f, 0.001f);

    dtr_gamepad_destroy(gp);
    DTR_PASS();
}

static void test_gamepad_deadzone_out_of_range(void)
{
    dtr_gamepad_state_t *gp;

    gp = dtr_gamepad_create();

    /* Out-of-range set should not crash */
    dtr_gamepad_set_deadzone(gp, 0.3f, -1);
    dtr_gamepad_set_deadzone(gp, 0.3f, DTR_MAX_GAMEPADS);

    /* Out-of-range get returns default-ish value (no crash) */
    DTR_ASSERT_NEAR(dtr_gamepad_get_deadzone(gp, -1), 0.0f, 0.001f);
    DTR_ASSERT_NEAR(dtr_gamepad_get_deadzone(gp, DTR_MAX_GAMEPADS), 0.0f, 0.001f);

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

    DTR_TEST_BEGIN("test_mouse_gamepad");

    /* Mouse tests */
    DTR_RUN_TEST(test_mouse_create_destroy);
    DTR_RUN_TEST(test_mouse_destroy_null);
    DTR_RUN_TEST(test_mouse_btn_default_false);
    DTR_RUN_TEST(test_mouse_btn_set_and_query);
    DTR_RUN_TEST(test_mouse_btnp);
    DTR_RUN_TEST(test_mouse_btn_out_of_range);
    DTR_RUN_TEST(test_mouse_set_mapping);
    DTR_RUN_TEST(test_mouse_update_copies_previous);

    /* Gamepad tests */
    DTR_RUN_TEST(test_gamepad_create_destroy);
    DTR_RUN_TEST(test_gamepad_destroy_null);
    DTR_RUN_TEST(test_gamepad_btn_default_false);
    DTR_RUN_TEST(test_gamepad_count_zero);
    DTR_RUN_TEST(test_gamepad_connected_false);
    DTR_RUN_TEST(test_gamepad_deadzone);
    DTR_RUN_TEST(test_gamepad_axis_default_zero);
    DTR_RUN_TEST(test_gamepad_btn_set_and_query);
    DTR_RUN_TEST(test_gamepad_btnp);
    DTR_RUN_TEST(test_gamepad_btn_out_of_range);
    DTR_RUN_TEST(test_gamepad_axis_set_and_query);
    DTR_RUN_TEST(test_gamepad_connected_index_out_of_range);
    DTR_RUN_TEST(test_gamepad_name_default);
    DTR_RUN_TEST(test_gamepad_update_deadzone);
    DTR_RUN_TEST(test_gamepad_deadzone_out_of_range);

    DTR_TEST_END();
}
