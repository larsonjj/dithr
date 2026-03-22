/**
 * \file            test_input.c
 * \brief           Unit tests for keyboard state and virtual input mapping
 */


#include <assert.h>
#include <stdio.h>

#include "input.h"
#include "test_harness.h"

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
    dtr_key_update(keys); /* copies current→previous */
    assert(!dtr_key_btnp(keys, DTR_KEY_Z)); /* held, not newly pressed */

    /* Frame 3: release */
    dtr_key_set(keys, DTR_KEY_Z, false);
    assert(!dtr_key_btnp(keys, DTR_KEY_Z));

    /* Frame 4: re-press */
    dtr_key_update(keys);
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
    dtr_key_state_t *  keys;
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
    dtr_key_update(keys);
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

    inp = dtr_input_create();
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

    DTR_TEST_END();
}
