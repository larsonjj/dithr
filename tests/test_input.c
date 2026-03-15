/**
 * \file            test_input.c
 * \brief           Unit tests for keyboard state and virtual input mapping
 */

#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>

#include "input.h"

/* ------------------------------------------------------------------ */
/*  Keyboard state tests                                               */
/* ------------------------------------------------------------------ */

static void test_key_btn_default_false(void)
{
    mvn_key_state_t *keys;

    keys = mvn_key_create();
    assert(keys != NULL);
    assert(!mvn_key_btn(keys, MVN_KEY_Z));
    assert(!mvn_key_btn(keys, MVN_KEY_LEFT));
    mvn_key_destroy(keys);
    printf("  PASS test_key_btn_default_false\n");
}

static void test_key_set_and_btn(void)
{
    mvn_key_state_t *keys;

    keys = mvn_key_create();
    mvn_key_set(keys, MVN_KEY_Z, true);
    assert(mvn_key_btn(keys, MVN_KEY_Z));
    assert(!mvn_key_btn(keys, MVN_KEY_X));

    mvn_key_set(keys, MVN_KEY_Z, false);
    assert(!mvn_key_btn(keys, MVN_KEY_Z));
    mvn_key_destroy(keys);
    printf("  PASS test_key_set_and_btn\n");
}

static void test_key_btnp(void)
{
    mvn_key_state_t *keys;

    keys = mvn_key_create();

    /* Frame 1: press Z */
    mvn_key_set(keys, MVN_KEY_Z, true);
    assert(mvn_key_btnp(keys, MVN_KEY_Z)); /* first frame pressed → true */

    /* Frame 2: still held */
    mvn_key_update(keys); /* copies current→previous */
    assert(!mvn_key_btnp(keys, MVN_KEY_Z)); /* held, not newly pressed */

    /* Frame 3: release */
    mvn_key_set(keys, MVN_KEY_Z, false);
    assert(!mvn_key_btnp(keys, MVN_KEY_Z));

    /* Frame 4: re-press */
    mvn_key_update(keys);
    mvn_key_set(keys, MVN_KEY_Z, true);
    assert(mvn_key_btnp(keys, MVN_KEY_Z)); /* new press → true */

    mvn_key_destroy(keys);
    printf("  PASS test_key_btnp\n");
}

static void test_key_out_of_range(void)
{
    mvn_key_state_t *keys;

    keys = mvn_key_create();
    mvn_key_set(keys, MVN_KEY_NONE, true);
    assert(!mvn_key_btn(keys, MVN_KEY_NONE));
    assert(!mvn_key_btn(keys, MVN_KEY_COUNT));
    mvn_key_destroy(keys);
    printf("  PASS test_key_out_of_range\n");
}

/* ------------------------------------------------------------------ */
/*  Parse binding tests                                                */
/* ------------------------------------------------------------------ */

static void test_parse_key_binding(void)
{
    mvn_binding_t bind;

    assert(mvn_input_parse_binding("KEY_LEFT", &bind));
    assert(bind.type == MVN_BIND_KEY);
    assert(bind.code == MVN_KEY_LEFT);

    assert(mvn_input_parse_binding("KEY_SPACE", &bind));
    assert(bind.type == MVN_BIND_KEY);
    assert(bind.code == MVN_KEY_SPACE);

    assert(!mvn_input_parse_binding("KEY_BOGUS", &bind));
    printf("  PASS test_parse_key_binding\n");
}

static void test_parse_pad_binding(void)
{
    mvn_binding_t bind;

    assert(mvn_input_parse_binding("PAD_A", &bind));
    assert(bind.type == MVN_BIND_PAD_BTN);
    assert(bind.code == (int32_t)MVN_PAD_A);

    assert(mvn_input_parse_binding("PAD_START", &bind));
    assert(bind.type == MVN_BIND_PAD_BTN);
    assert(bind.code == (int32_t)MVN_PAD_START);

    assert(!mvn_input_parse_binding("PAD_BOGUS", &bind));
    printf("  PASS test_parse_pad_binding\n");
}

static void test_parse_axis_binding(void)
{
    mvn_binding_t bind;

    assert(mvn_input_parse_binding("PAD_AXIS_LX", &bind));
    assert(bind.type == MVN_BIND_PAD_AXIS);
    assert(bind.code == MVN_PAD_AXIS_LX);
    assert(bind.threshold > 0.0f);

    assert(!mvn_input_parse_binding("PAD_AXIS_BOGUS", &bind));
    printf("  PASS test_parse_axis_binding\n");
}

static void test_parse_mouse_binding(void)
{
    mvn_binding_t bind;

    assert(mvn_input_parse_binding("MOUSE_LEFT", &bind));
    assert(bind.type == MVN_BIND_MOUSE_BTN);
    assert(bind.code == MVN_MOUSE_LEFT);

    assert(mvn_input_parse_binding("MOUSE_RIGHT", &bind));
    assert(bind.code == MVN_MOUSE_RIGHT);

    assert(!mvn_input_parse_binding("MOUSE_BOGUS", &bind));
    printf("  PASS test_parse_mouse_binding\n");
}

static void test_parse_invalid_prefix(void)
{
    mvn_binding_t bind;

    assert(!mvn_input_parse_binding("BOGUS_LEFT", &bind));
    assert(!mvn_input_parse_binding("", &bind));
    printf("  PASS test_parse_invalid_prefix\n");
}

/* ------------------------------------------------------------------ */
/*  Virtual input mapping tests                                        */
/* ------------------------------------------------------------------ */

static void test_input_map_and_query(void)
{
    mvn_input_state_t *inp;
    mvn_key_state_t *  keys;
    mvn_binding_t      binds[1];

    inp  = mvn_input_create();
    keys = mvn_key_create();

    /* Map "jump" → KEY_Z */
    binds[0].type      = MVN_BIND_KEY;
    binds[0].code      = MVN_KEY_Z;
    binds[0].threshold = 0.0f;
    mvn_input_map(inp, "jump", binds, 1);

    /* Not pressed yet */
    mvn_input_update(inp, keys, NULL);
    assert(!mvn_input_btn(inp, "jump"));

    /* Press the key */
    mvn_key_set(keys, MVN_KEY_Z, true);
    mvn_input_update(inp, keys, NULL);
    assert(mvn_input_btn(inp, "jump"));
    assert(mvn_input_btnp(inp, "jump")); /* first frame → just pressed */

    /* Hold */
    mvn_key_update(keys);
    mvn_input_update(inp, keys, NULL);
    assert(mvn_input_btn(inp, "jump"));
    assert(!mvn_input_btnp(inp, "jump")); /* held → not just pressed */

    mvn_input_destroy(inp);
    mvn_key_destroy(keys);
    printf("  PASS test_input_map_and_query\n");
}

static void test_input_unknown_action(void)
{
    mvn_input_state_t *inp;

    inp = mvn_input_create();
    assert(!mvn_input_btn(inp, "nonexistent"));
    assert(!mvn_input_btnp(inp, "nonexistent"));
    assert(mvn_input_axis(inp, "nonexistent") == 0.0f);
    mvn_input_destroy(inp);
    printf("  PASS test_input_unknown_action\n");
}

static void test_input_clear(void)
{
    mvn_input_state_t *inp;
    mvn_binding_t      binds[1];

    inp = mvn_input_create();
    binds[0].type      = MVN_BIND_KEY;
    binds[0].code      = MVN_KEY_A;
    binds[0].threshold = 0.0f;
    mvn_input_map(inp, "fire", binds, 1);

    assert(mvn_input_axis(inp, "fire") == 0.0f); /* action exists */
    mvn_input_clear_action(inp, "fire");

    /* Clearing should remove bindings but action stays in array */
    mvn_input_clear_all(inp);
    assert(!mvn_input_btn(inp, "fire")); /* action gone after clear_all */

    mvn_input_destroy(inp);
    printf("  PASS test_input_clear\n");
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== test_input ===\n");

    /* Keyboard state */
    test_key_btn_default_false();
    test_key_set_and_btn();
    test_key_btnp();
    test_key_out_of_range();

    /* Binding parser */
    test_parse_key_binding();
    test_parse_pad_binding();
    test_parse_axis_binding();
    test_parse_mouse_binding();
    test_parse_invalid_prefix();

    /* Virtual input mapping */
    test_input_map_and_query();
    test_input_unknown_action();
    test_input_clear();

    printf("All input tests passed.\n");
    return 0;
}
