/**
 * \file            test_input.c
 * \brief           Unit tests for keyboard state and virtual input mapping
 */

#include "gamepad.h"
#include "input.h"
#include "test_harness.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Keyboard state tests                                               */
/* ------------------------------------------------------------------ */

static void test_key_btn_default_false(void)
{
    dtr_key_state_t *keys;

    keys = dtr_key_create();
    assert(keys != NULL);
    assert(!dtr_key_btn(keys, DTR_KEY_Z));
    assert(!dtr_key_btn(keys, DTR_KEY_LEFT));
    dtr_key_destroy(keys);
    printf("  PASS test_key_btn_default_false\n");
}

static void test_key_set_and_btn(void)
{
    dtr_key_state_t *keys;

    keys = dtr_key_create();
    dtr_key_set(keys, DTR_KEY_Z, true);
    assert(dtr_key_btn(keys, DTR_KEY_Z));
    assert(!dtr_key_btn(keys, DTR_KEY_X));

    dtr_key_set(keys, DTR_KEY_Z, false);
    assert(!dtr_key_btn(keys, DTR_KEY_Z));
    dtr_key_destroy(keys);
    printf("  PASS test_key_set_and_btn\n");
}

static void test_key_btnp(void)
{
    dtr_key_state_t *keys;

    keys = dtr_key_create();

    /* Frame 1: press Z */
    dtr_key_set(keys, DTR_KEY_Z, true);
    assert(dtr_key_btnp(keys, DTR_KEY_Z)); /* first frame pressed → true */

    /* Frame 2: still held */
    dtr_key_update(keys, 0.016f);           /* copies current→previous */
    assert(!dtr_key_btnp(keys, DTR_KEY_Z)); /* held, not newly pressed */

    /* Frame 3: release */
    dtr_key_set(keys, DTR_KEY_Z, false);
    assert(!dtr_key_btnp(keys, DTR_KEY_Z));

    /* Frame 4: re-press */
    dtr_key_update(keys, 0.016f);
    dtr_key_set(keys, DTR_KEY_Z, true);
    assert(dtr_key_btnp(keys, DTR_KEY_Z)); /* new press → true */

    dtr_key_destroy(keys);
    printf("  PASS test_key_btnp\n");
}

static void test_key_out_of_range(void)
{
    dtr_key_state_t *keys;

    keys = dtr_key_create();
    dtr_key_set(keys, DTR_KEY_NONE, true);
    assert(!dtr_key_btn(keys, DTR_KEY_NONE));
    assert(!dtr_key_btn(keys, DTR_KEY_COUNT));
    dtr_key_destroy(keys);
    printf("  PASS test_key_out_of_range\n");
}

/* ------------------------------------------------------------------ */
/*  Parse binding tests                                                */
/* ------------------------------------------------------------------ */

static void test_parse_key_binding(void)
{
    dtr_binding_t bind;

    assert(dtr_input_parse_binding("KEY_LEFT", &bind));
    assert(bind.type == DTR_BIND_KEY);
    assert(bind.code == DTR_KEY_LEFT);

    assert(dtr_input_parse_binding("KEY_SPACE", &bind));
    assert(bind.type == DTR_BIND_KEY);
    assert(bind.code == DTR_KEY_SPACE);

    assert(!dtr_input_parse_binding("KEY_BOGUS", &bind));
    printf("  PASS test_parse_key_binding\n");
}

static void test_parse_pad_binding(void)
{
    dtr_binding_t bind;

    assert(dtr_input_parse_binding("PAD_A", &bind));
    assert(bind.type == DTR_BIND_PAD_BTN);
    assert(bind.code == (int32_t)DTR_PAD_A);

    assert(dtr_input_parse_binding("PAD_START", &bind));
    assert(bind.type == DTR_BIND_PAD_BTN);
    assert(bind.code == (int32_t)DTR_PAD_START);

    assert(!dtr_input_parse_binding("PAD_BOGUS", &bind));
    printf("  PASS test_parse_pad_binding\n");
}

static void test_parse_axis_binding(void)
{
    dtr_binding_t bind;

    assert(dtr_input_parse_binding("PAD_AXIS_LX", &bind));
    assert(bind.type == DTR_BIND_PAD_AXIS);
    assert(bind.code == DTR_PAD_AXIS_LX);
    assert(bind.threshold > 0.0f);

    assert(!dtr_input_parse_binding("PAD_AXIS_BOGUS", &bind));
    printf("  PASS test_parse_axis_binding\n");
}

static void test_parse_mouse_binding(void)
{
    dtr_binding_t bind;

    assert(dtr_input_parse_binding("MOUSE_LEFT", &bind));
    assert(bind.type == DTR_BIND_MOUSE_BTN);
    assert(bind.code == DTR_MOUSE_LEFT);

    assert(dtr_input_parse_binding("MOUSE_RIGHT", &bind));
    assert(bind.code == DTR_MOUSE_RIGHT);

    assert(!dtr_input_parse_binding("MOUSE_BOGUS", &bind));
    printf("  PASS test_parse_mouse_binding\n");
}

static void test_parse_invalid_prefix(void)
{
    dtr_binding_t bind;

    assert(!dtr_input_parse_binding("BOGUS_LEFT", &bind));
    assert(!dtr_input_parse_binding("", &bind));
    printf("  PASS test_parse_invalid_prefix\n");
}

/* ------------------------------------------------------------------ */
/*  Virtual input mapping tests                                        */
/* ------------------------------------------------------------------ */

static void test_input_map_and_query(void)
{
    dtr_input_state_t *inp;
    dtr_key_state_t   *keys;
    dtr_binding_t      binds[1];

    inp  = dtr_input_create();
    keys = dtr_key_create();

    /* Map "jump" → KEY_Z */
    binds[0].type      = DTR_BIND_KEY;
    binds[0].code      = DTR_KEY_Z;
    binds[0].threshold = 0.0f;
    dtr_input_map(inp, "jump", binds, 1);

    /* Not pressed yet */
    dtr_input_update(inp, keys, NULL);
    assert(!dtr_input_btn(inp, "jump"));

    /* Press the key */
    dtr_key_set(keys, DTR_KEY_Z, true);
    dtr_input_update(inp, keys, NULL);
    assert(dtr_input_btn(inp, "jump"));
    assert(dtr_input_btnp(inp, "jump")); /* first frame → just pressed */

    /* Hold */
    dtr_key_update(keys, 0.016f);
    dtr_input_update(inp, keys, NULL);
    assert(dtr_input_btn(inp, "jump"));
    assert(!dtr_input_btnp(inp, "jump")); /* held → not just pressed */

    dtr_input_destroy(inp);
    dtr_key_destroy(keys);
    printf("  PASS test_input_map_and_query\n");
}

static void test_input_unknown_action(void)
{
    dtr_input_state_t *inp;

    inp = dtr_input_create();
    assert(!dtr_input_btn(inp, "nonexistent"));
    assert(!dtr_input_btnp(inp, "nonexistent"));
    assert(dtr_input_axis(inp, "nonexistent") == 0.0f);
    dtr_input_destroy(inp);
    printf("  PASS test_input_unknown_action\n");
}

static void test_input_clear(void)
{
    dtr_input_state_t *inp;
    dtr_binding_t      binds[1];

    inp                = dtr_input_create();
    binds[0].type      = DTR_BIND_KEY;
    binds[0].code      = DTR_KEY_A;
    binds[0].threshold = 0.0f;
    dtr_input_map(inp, "fire", binds, 1);

    assert(dtr_input_axis(inp, "fire") == 0.0f); /* action exists */
    dtr_input_clear_action(inp, "fire");

    /* Clearing should remove bindings but action stays in array */
    dtr_input_clear_all(inp);
    assert(!dtr_input_btn(inp, "fire")); /* action gone after clear_all */

    dtr_input_destroy(inp);
    printf("  PASS test_input_clear\n");
}

/* ------------------------------------------------------------------ */
/*  Key name / scancode mapping                                        */
/* ------------------------------------------------------------------ */

static void test_key_name(void)
{
    DTR_ASSERT(strcmp(dtr_key_name(DTR_KEY_UP), "UP") == 0);
    DTR_ASSERT(strcmp(dtr_key_name(DTR_KEY_Z), "Z") == 0);
    DTR_ASSERT(strcmp(dtr_key_name(DTR_KEY_SPACE), "SPACE") == 0);
    DTR_ASSERT(strcmp(dtr_key_name(DTR_KEY_F12), "F12") == 0);

    /* Out-of-range returns "NONE" */
    DTR_ASSERT(strcmp(dtr_key_name(DTR_KEY_COUNT), "NONE") == 0);
    DTR_ASSERT(strcmp(dtr_key_name((dtr_key_t)-1), "NONE") == 0);
    DTR_PASS();
}

static void test_key_from_scancode(void)
{
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_UP), DTR_KEY_UP);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_Z), DTR_KEY_Z);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_SPACE), DTR_KEY_SPACE);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_RETURN), DTR_KEY_ENTER);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_F1), DTR_KEY_F1);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_0), DTR_KEY_0);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_9), DTR_KEY_9);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_BACKSPACE), DTR_KEY_BACKSPACE);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_DELETE), DTR_KEY_DELETE);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_TAB), DTR_KEY_TAB);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_HOME), DTR_KEY_HOME);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_END), DTR_KEY_END);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_PAGEUP), DTR_KEY_PAGEUP);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_PAGEDOWN), DTR_KEY_PAGEDOWN);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_LCTRL), DTR_KEY_LCTRL);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_RCTRL), DTR_KEY_RCTRL);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_LALT), DTR_KEY_LALT);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_RALT), DTR_KEY_RALT);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_LGUI), DTR_KEY_LGUI);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_RGUI), DTR_KEY_RGUI);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_LEFTBRACKET), DTR_KEY_LBRACKET);
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_RIGHTBRACKET), DTR_KEY_RBRACKET);

    /* Unmapped scancode returns NONE */
    DTR_ASSERT_EQ_INT(dtr_key_from_scancode(SDL_SCANCODE_CAPSLOCK), DTR_KEY_NONE);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Input remap (alias for map)                                        */
/* ------------------------------------------------------------------ */

static void test_input_remap(void)
{
    dtr_input_state_t *inp;
    dtr_key_state_t   *keys;
    dtr_binding_t      binds[1];

    inp  = dtr_input_create();
    keys = dtr_key_create();

    /* Map "move" → KEY_LEFT */
    binds[0].type      = DTR_BIND_KEY;
    binds[0].code      = DTR_KEY_LEFT;
    binds[0].threshold = 0.0f;
    dtr_input_map(inp, "move", binds, 1);

    dtr_key_set(keys, DTR_KEY_LEFT, true);
    dtr_input_update(inp, keys, NULL);
    DTR_ASSERT(dtr_input_btn(inp, "move"));

    /* Remap "move" → KEY_RIGHT */
    binds[0].code = DTR_KEY_RIGHT;
    dtr_input_remap(inp, "move", binds, 1);

    /* LEFT should no longer fire "move" */
    dtr_key_set(keys, DTR_KEY_LEFT, true);
    dtr_key_set(keys, DTR_KEY_RIGHT, false);
    dtr_input_update(inp, keys, NULL);
    DTR_ASSERT(!dtr_input_btn(inp, "move"));

    /* RIGHT should fire "move" */
    dtr_key_set(keys, DTR_KEY_RIGHT, true);
    dtr_input_update(inp, keys, NULL);
    DTR_ASSERT(dtr_input_btn(inp, "move"));

    dtr_input_destroy(inp);
    dtr_key_destroy(keys);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Input axis via gamepad binding                                     */
/* ------------------------------------------------------------------ */

static void test_input_axis_with_gamepad(void)
{
    dtr_input_state_t   *inp;
    dtr_key_state_t     *keys;
    dtr_gamepad_state_t *pads;
    dtr_binding_t        binds[1];

    inp  = dtr_input_create();
    keys = dtr_key_create();
    pads = dtr_gamepad_create();

    /* Simulate connected pad */
    pads->pads[0].connected = true;
    pads->count             = 1;

    /* Map "horizontal" → PAD_AXIS_LX */
    binds[0].type      = DTR_BIND_PAD_AXIS;
    binds[0].code      = DTR_PAD_AXIS_LX;
    binds[0].threshold = 0.1f;
    dtr_input_map(inp, "horizontal", binds, 1);

    /* Axis at 0 — below threshold */
    pads->pads[0].axes[DTR_PAD_AXIS_LX] = 0.0f;
    dtr_input_update(inp, keys, pads);
    DTR_ASSERT(!dtr_input_btn(inp, "horizontal"));
    DTR_ASSERT_NEAR(dtr_input_axis(inp, "horizontal"), 0.0f, 0.001f);

    /* Axis above threshold */
    pads->pads[0].axes[DTR_PAD_AXIS_LX] = 0.8f;
    dtr_input_update(inp, keys, pads);
    DTR_ASSERT(dtr_input_btn(inp, "horizontal"));
    DTR_ASSERT_NEAR(dtr_input_axis(inp, "horizontal"), 0.8f, 0.001f);

    /* Negative axis */
    pads->pads[0].axes[DTR_PAD_AXIS_LX] = -0.6f;
    dtr_input_update(inp, keys, pads);
    DTR_ASSERT(dtr_input_btn(inp, "horizontal"));
    DTR_ASSERT_NEAR(dtr_input_axis(inp, "horizontal"), -0.6f, 0.001f);

    dtr_input_destroy(inp);
    dtr_key_destroy(keys);
    dtr_gamepad_destroy(pads);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Input with gamepad button binding                                  */
/* ------------------------------------------------------------------ */

static void test_input_pad_button_binding(void)
{
    dtr_input_state_t   *inp;
    dtr_key_state_t     *keys;
    dtr_gamepad_state_t *pads;
    dtr_binding_t        binds[1];

    inp  = dtr_input_create();
    keys = dtr_key_create();
    pads = dtr_gamepad_create();

    pads->pads[0].connected = true;
    pads->count             = 1;

    /* Map "jump" → PAD_A */
    binds[0].type      = DTR_BIND_PAD_BTN;
    binds[0].code      = DTR_PAD_A;
    binds[0].threshold = 0.0f;
    dtr_input_map(inp, "jump", binds, 1);

    /* Not pressed */
    dtr_input_update(inp, keys, pads);
    DTR_ASSERT(!dtr_input_btn(inp, "jump"));

    /* Press pad A */
    pads->pads[0].btn_current[DTR_PAD_A] = true;
    dtr_input_update(inp, keys, pads);
    DTR_ASSERT(dtr_input_btn(inp, "jump"));

    dtr_input_destroy(inp);
    dtr_key_destroy(keys);
    dtr_gamepad_destroy(pads);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Multiple bindings per action                                       */
/* ------------------------------------------------------------------ */

static void test_input_multiple_bindings(void)
{
    dtr_input_state_t *inp;
    dtr_key_state_t   *keys;
    dtr_binding_t      binds[2];

    inp  = dtr_input_create();
    keys = dtr_key_create();

    /* Map "fire" → KEY_Z or KEY_X */
    binds[0].type      = DTR_BIND_KEY;
    binds[0].code      = DTR_KEY_Z;
    binds[0].threshold = 0.0f;
    binds[1].type      = DTR_BIND_KEY;
    binds[1].code      = DTR_KEY_X;
    binds[1].threshold = 0.0f;
    dtr_input_map(inp, "fire", binds, 2);

    /* Neither pressed */
    dtr_input_update(inp, keys, NULL);
    DTR_ASSERT(!dtr_input_btn(inp, "fire"));

    /* Only second binding pressed */
    dtr_key_set(keys, DTR_KEY_X, true);
    dtr_input_update(inp, keys, NULL);
    DTR_ASSERT(dtr_input_btn(inp, "fire"));

    dtr_input_destroy(inp);
    dtr_key_destroy(keys);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Input clear_action preserves other actions                         */
/* ------------------------------------------------------------------ */

static void test_input_clear_action_preserves_others(void)
{
    dtr_input_state_t *inp;
    dtr_key_state_t   *keys;
    dtr_binding_t      binds[1];

    inp  = dtr_input_create();
    keys = dtr_key_create();

    binds[0].type      = DTR_BIND_KEY;
    binds[0].code      = DTR_KEY_Z;
    binds[0].threshold = 0.0f;
    dtr_input_map(inp, "jump", binds, 1);

    binds[0].code = DTR_KEY_X;
    dtr_input_map(inp, "fire", binds, 1);

    /* Clear only "jump" */
    dtr_input_clear_action(inp, "jump");

    /* "fire" should still work */
    dtr_key_set(keys, DTR_KEY_X, true);
    dtr_input_update(inp, keys, NULL);
    DTR_ASSERT(dtr_input_btn(inp, "fire"));

    /* "jump" should not fire (bindings cleared) */
    dtr_key_set(keys, DTR_KEY_Z, true);
    dtr_input_update(inp, keys, NULL);
    DTR_ASSERT(!dtr_input_btn(inp, "jump"));

    dtr_input_destroy(inp);
    dtr_key_destroy(keys);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    DTR_TEST_BEGIN("test_input");

    /* Keyboard state */
    DTR_RUN_TEST(test_key_btn_default_false);
    DTR_RUN_TEST(test_key_set_and_btn);
    DTR_RUN_TEST(test_key_btnp);
    DTR_RUN_TEST(test_key_out_of_range);

    /* Binding parser */
    DTR_RUN_TEST(test_parse_key_binding);
    DTR_RUN_TEST(test_parse_pad_binding);
    DTR_RUN_TEST(test_parse_axis_binding);
    DTR_RUN_TEST(test_parse_mouse_binding);
    DTR_RUN_TEST(test_parse_invalid_prefix);

    /* Virtual input mapping */
    DTR_RUN_TEST(test_input_map_and_query);
    DTR_RUN_TEST(test_input_unknown_action);
    DTR_RUN_TEST(test_input_clear);
    DTR_RUN_TEST(test_key_name);
    DTR_RUN_TEST(test_key_from_scancode);
    DTR_RUN_TEST(test_input_remap);
    DTR_RUN_TEST(test_input_axis_with_gamepad);
    DTR_RUN_TEST(test_input_pad_button_binding);
    DTR_RUN_TEST(test_input_multiple_bindings);
    DTR_RUN_TEST(test_input_clear_action_preserves_others);

    DTR_TEST_END();
}
