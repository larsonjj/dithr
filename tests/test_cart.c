/**
 * \file            test_cart.c
 * \brief           Unit tests for cart parsing and validation
 */


#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cart.h"
#include "quickjs.h"

/* ------------------------------------------------------------------ */
/*  Helper: create a minimal QuickJS context for JSON parsing          */
/* ------------------------------------------------------------------ */

static JSRuntime *s_rt;
static JSContext * s_ctx;

static void prv_setup(void)
{
    s_rt  = JS_NewRuntime();
    s_ctx = JS_NewContext(s_rt);
}

static void prv_teardown(void)
{
    JS_FreeContext(s_ctx);
    JS_FreeRuntime(s_rt);
    s_ctx = NULL;
    s_rt  = NULL;
}

/* ------------------------------------------------------------------ */
/*  Defaults                                                           */
/* ------------------------------------------------------------------ */

static void test_cart_defaults(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();
    assert(cart != NULL);
    assert(cart->display.width == CONSOLE_FB_WIDTH);
    assert(cart->display.height == CONSOLE_FB_HEIGHT);
    assert(cart->display.scale == CONSOLE_DEFAULT_SCALE);
    assert(cart->timing.fps == CONSOLE_FPS);
    assert(cart->audio.channels == CONSOLE_AUDIO_CHANNELS);
    assert(cart->audio.frequency == CONSOLE_AUDIO_FREQ);
    assert(cart->sprites.tile_w == CONSOLE_TILE_W);
    assert(strcmp(cart->meta.title, "Untitled") == 0);
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_defaults\n");
}

/* ------------------------------------------------------------------ */
/*  Parse minimal JSON                                                 */
/* ------------------------------------------------------------------ */

static void test_cart_parse_minimal(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    assert(ok);
    /* Should keep defaults */
    assert(cart->display.width == CONSOLE_FB_WIDTH);
    assert(cart->timing.fps == CONSOLE_FPS);
    prv_teardown();
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_parse_minimal\n");
}

/* ------------------------------------------------------------------ */
/*  Parse display overrides                                            */
/* ------------------------------------------------------------------ */

static void test_cart_parse_display(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json =
        "{"
        "  \"display\": {"
        "    \"width\": 128,"
        "    \"height\": 128,"
        "    \"scale\": 2"
        "  }"
        "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    assert(ok);
    assert(cart->display.width == 128);
    assert(cart->display.height == 128);
    assert(cart->display.scale == 2);
    prv_teardown();
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_parse_display\n");
}

/* ------------------------------------------------------------------ */
/*  Parse meta                                                         */
/* ------------------------------------------------------------------ */

static void test_cart_parse_meta(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json =
        "{"
        "  \"meta\": {"
        "    \"title\": \"My Game\","
        "    \"author\": \"Dev\","
        "    \"version\": \"1.0.0\""
        "  }"
        "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    assert(ok);
    assert(strcmp(cart->meta.title, "My Game") == 0);
    assert(strcmp(cart->meta.author, "Dev") == 0);
    assert(strcmp(cart->meta.version, "1.0.0") == 0);
    prv_teardown();
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_parse_meta\n");
}

/* ------------------------------------------------------------------ */
/*  Parse input mappings                                               */
/* ------------------------------------------------------------------ */

static void test_cart_parse_input(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json =
        "{"
        "  \"input\": {"
        "    \"default_mappings\": {"
        "      \"jump\": [\"KEY_Z\", \"PAD_A\"],"
        "      \"fire\": [\"KEY_X\"]"
        "    }"
        "  }"
        "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    assert(ok);
    assert(cart->input.mapping_count == 2);

    /* Find jump mapping */
    {
        bool found = false;
        for (int32_t i = 0; i < cart->input.mapping_count; i++) {
            if (strcmp(cart->input.mappings[i].action, "jump") == 0) {
                found = true;
                assert(cart->input.mappings[i].bind_count == 2);
                assert(strcmp(cart->input.mappings[i].bindings[0], "KEY_Z") == 0);
                assert(strcmp(cart->input.mappings[i].bindings[1], "PAD_A") == 0);
            }
        }
        assert(found);
    }

    prv_teardown();
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_parse_input\n");
}

/* ------------------------------------------------------------------ */
/*  Parse invalid JSON                                                 */
/* ------------------------------------------------------------------ */

static void test_cart_parse_invalid_json(void)
{
    dtr_cart_t *cart;
    bool        ok;

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, "{bad json!!!", 13);
    assert(!ok);
    prv_teardown();
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_parse_invalid_json\n");
}

/* ------------------------------------------------------------------ */
/*  Validate — good values                                             */
/* ------------------------------------------------------------------ */

static void test_cart_validate_defaults(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();
    assert(dtr_cart_validate(cart));
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_validate_defaults\n");
}

/* ------------------------------------------------------------------ */
/*  Validate — out of range width triggers clamp                       */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_width(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();
    cart->display.width = 9999;
    dtr_cart_validate(cart);
    assert(cart->display.width == CONSOLE_FB_WIDTH);
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_validate_clamp_width\n");
}

/* ------------------------------------------------------------------ */
/*  Validate — FPS clamped                                             */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_fps(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();
    cart->timing.fps = 1;
    dtr_cart_validate(cart);
    assert(cart->timing.fps == 15);
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_validate_clamp_fps\n");
}

/* ------------------------------------------------------------------ */
/*  Validate — tile dimensions clamped                                 */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_tile(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();
    cart->sprites.tile_w = 2;
    cart->sprites.tile_h = 128;
    dtr_cart_validate(cart);
    assert(cart->sprites.tile_w == CONSOLE_TILE_W);
    assert(cart->sprites.tile_h == CONSOLE_TILE_H);
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_validate_clamp_tile\n");
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;


    printf("=== test_cart ===\n");

    test_cart_defaults();
    test_cart_parse_minimal();
    test_cart_parse_display();
    test_cart_parse_meta();
    test_cart_parse_input();
    test_cart_parse_invalid_json();
    test_cart_validate_defaults();
    test_cart_validate_clamp_width();
    test_cart_validate_clamp_fps();
    test_cart_validate_clamp_tile();

    printf("All cart tests passed.\n");
    return 0;
}
