/**
 * \file            test_cart.c
 * \brief           Unit tests for cart parsing and validation
 */

#include "cart.h"
#include "quickjs.h"
#include "test_harness.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helper: create a minimal QuickJS context for JSON parsing          */
/* ------------------------------------------------------------------ */

static JSRuntime *s_rt;
static JSContext *s_ctx;

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
    const char *json = "{"
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
    const char *json = "{"
                       "  \"title\": \"My Game\","
                       "  \"author\": \"Dev\","
                       "  \"version\": \"1.0.0\""
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
    const char *json = "{"
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

    cart                = dtr_cart_create();
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

    cart             = dtr_cart_create();
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

    cart                 = dtr_cart_create();
    cart->sprites.tile_w = 2;
    cart->sprites.tile_h = 128;
    dtr_cart_validate(cart);
    assert(cart->sprites.tile_w == CONSOLE_TILE_W);
    assert(cart->sprites.tile_h == CONSOLE_TILE_H);
    dtr_cart_destroy(cart);
    printf("  PASS test_cart_validate_clamp_tile\n");
}

/* ------------------------------------------------------------------ */
/*  dset / dget — numeric persistence slots                            */
/* ------------------------------------------------------------------ */

static void test_cart_dset_dget(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();

    /* Default slot value is 0 */
    DTR_ASSERT_NEAR(dtr_cart_dget(cart, 0), 0.0, 0.001);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart, 63), 0.0, 0.001);

    /* Set and retrieve */
    dtr_cart_dset(cart, 0, 42.5);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart, 0), 42.5, 0.001);

    dtr_cart_dset(cart, 63, -100.0);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart, 63), -100.0, 0.001);

    /* Overwrite */
    dtr_cart_dset(cart, 0, 99.0);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart, 0), 99.0, 0.001);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_cart_dset_dget_out_of_range(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();

    /* Out-of-range indices should be safe */
    dtr_cart_dset(cart, -1, 1.0);
    dtr_cart_dset(cart, DTR_CART_MAX_DSLOTS, 1.0);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart, -1), 0.0, 0.001);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart, DTR_CART_MAX_DSLOTS), 0.0, 0.001);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Key-value persistence                                              */
/* ------------------------------------------------------------------ */

static void test_cart_kv_save_load(void)
{
    dtr_cart_t *cart;
    const char *val;

    cart = dtr_cart_create();

    /* No keys initially */
    DTR_ASSERT(!dtr_cart_has_key(cart, "score"));
    val = dtr_cart_load_key(cart, "score");
    DTR_ASSERT(val == NULL);

    /* Save and load */
    dtr_cart_save(cart, "score", "100");
    DTR_ASSERT(dtr_cart_has_key(cart, "score"));
    val = dtr_cart_load_key(cart, "score");
    DTR_ASSERT(val != NULL);
    DTR_ASSERT(strcmp(val, "100") == 0);

    /* Overwrite existing key */
    dtr_cart_save(cart, "score", "200");
    val = dtr_cart_load_key(cart, "score");
    DTR_ASSERT(strcmp(val, "200") == 0);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_cart_kv_delete(void)
{
    dtr_cart_t *cart;

    cart = dtr_cart_create();

    dtr_cart_save(cart, "a", "1");
    dtr_cart_save(cart, "b", "2");
    dtr_cart_save(cart, "c", "3");
    DTR_ASSERT_EQ_INT(cart->kv_count, 3);

    /* Delete middle key */
    dtr_cart_delete_key(cart, "b");
    DTR_ASSERT_EQ_INT(cart->kv_count, 2);
    DTR_ASSERT(!dtr_cart_has_key(cart, "b"));
    DTR_ASSERT(dtr_cart_has_key(cart, "a"));
    DTR_ASSERT(dtr_cart_has_key(cart, "c"));

    /* Delete non-existent key — no crash */
    dtr_cart_delete_key(cart, "zzz");
    DTR_ASSERT_EQ_INT(cart->kv_count, 2);

    /* Delete remaining keys */
    dtr_cart_delete_key(cart, "a");
    dtr_cart_delete_key(cart, "c");
    DTR_ASSERT_EQ_INT(cart->kv_count, 0);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_cart_kv_max_keys(void)
{
    dtr_cart_t *cart;
    char        key[DTR_CART_KEY_LEN];

    cart = dtr_cart_create();

    /* Fill to capacity */
    for (int32_t i = 0; i < DTR_CART_MAX_KV; ++i) {
        snprintf(key, sizeof(key), "key%d", i);
        dtr_cart_save(cart, key, "val");
    }
    DTR_ASSERT_EQ_INT(cart->kv_count, DTR_CART_MAX_KV);

    /* One more should be silently rejected */
    dtr_cart_save(cart, "overflow", "nope");
    DTR_ASSERT_EQ_INT(cart->kv_count, DTR_CART_MAX_KV);
    DTR_ASSERT(!dtr_cart_has_key(cart, "overflow"));

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — height out of range                                     */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_height(void)
{
    dtr_cart_t *cart;

    cart                 = dtr_cart_create();
    cart->display.height = 9999;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->display.height, CONSOLE_FB_HEIGHT);
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Parse timing overrides                                             */
/* ------------------------------------------------------------------ */

static void test_cart_parse_timing(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"timing\": {"
                       "    \"fps\": 30,"
                       "    \"ups\": 2"
                       "  }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(cart->timing.fps, 30);
    DTR_ASSERT_EQ_INT(cart->timing.ups, 2);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Parse audio overrides                                              */
/* ------------------------------------------------------------------ */

static void test_cart_parse_audio(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"audio\": {"
                       "    \"channels\": 4,"
                       "    \"frequency\": 22050"
                       "  }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(cart->audio.channels, 4);
    DTR_ASSERT_EQ_INT(cart->audio.frequency, 22050);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — scale out of range                                      */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_scale(void)
{
    dtr_cart_t *cart;

    cart                = dtr_cart_create();
    cart->display.scale = 0;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->display.scale, CONSOLE_DEFAULT_SCALE);

    cart->display.scale = 20;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->display.scale, CONSOLE_DEFAULT_SCALE);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — FPS upper bound                                         */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_fps_upper(void)
{
    dtr_cart_t *cart;

    cart             = dtr_cart_create();
    cart->timing.fps = 999;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->timing.fps, CONSOLE_FPS);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — ups clamped                                             */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_ups_lower(void)
{
    dtr_cart_t *cart;

    cart             = dtr_cart_create();
    cart->timing.ups = 1;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->timing.ups, 15);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_cart_validate_clamp_ups_upper(void)
{
    dtr_cart_t *cart;

    cart             = dtr_cart_create();
    cart->timing.ups = 999;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->timing.ups, 240);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — audio channels                                          */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_audio_channels(void)
{
    dtr_cart_t *cart;

    cart                 = dtr_cart_create();
    cart->audio.channels = -1;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->audio.channels, CONSOLE_AUDIO_CHANNELS);

    cart->audio.channels = 0;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->audio.channels, 0);

    cart->audio.channels = 999;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->audio.channels, CONSOLE_AUDIO_CHANNELS);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — audio frequency                                         */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_audio_freq(void)
{
    dtr_cart_t *cart;

    cart                  = dtr_cart_create();
    cart->audio.frequency = 100;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->audio.frequency, CONSOLE_AUDIO_FREQ);

    cart->audio.frequency = 200000;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->audio.frequency, CONSOLE_AUDIO_FREQ);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — audio buffer                                            */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_audio_buffer(void)
{
    dtr_cart_t *cart;

    cart                    = dtr_cart_create();
    cart->audio.buffer_size = 10;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->audio.buffer_size, CONSOLE_AUDIO_BUFFER);

    cart->audio.buffer_size = 99999;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->audio.buffer_size, CONSOLE_AUDIO_BUFFER);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Validate — runtime limits                                          */
/* ------------------------------------------------------------------ */

static void test_cart_validate_clamp_runtime(void)
{
    dtr_cart_t *cart;
    uint32_t    max_mem;
    uint32_t    max_stack;

    cart = dtr_cart_create();

    max_mem   = (uint32_t)CONSOLE_JS_MEM_MB * 1024u * 1024u;
    max_stack = (uint32_t)CONSOLE_JS_STACK_KB * 1024u;

    /* Exceed limits */
    cart->runtime.mem_limit   = max_mem + 1;
    cart->runtime.stack_limit = max_stack + 1;
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->runtime.mem_limit, max_mem);
    DTR_ASSERT_EQ_INT(cart->runtime.stack_limit, max_stack);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Parse — runtime overrides                                          */
/* ------------------------------------------------------------------ */

static void test_cart_parse_runtime(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"runtime\": {"
                       "    \"memoryLimitMB\": 32,"
                       "    \"stackLimitKB\": 256"
                       "  }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(cart->runtime.mem_limit, 32u * 1024u * 1024u);
    DTR_ASSERT_EQ_INT(cart->runtime.stack_limit, 256u * 1024u);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Parse — sfx and music arrays                                       */
/* ------------------------------------------------------------------ */

static void test_cart_parse_sfx_music(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"sfx\": [\"jump.wav\", \"hit.wav\"],"
                       "  \"music\": [\"theme.ogg\"]"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(cart->sfx_count, 2);
    DTR_ASSERT(strcmp(cart->sfx_paths[0], "jump.wav") == 0);
    DTR_ASSERT(strcmp(cart->sfx_paths[1], "hit.wav") == 0);
    DTR_ASSERT_EQ_INT(cart->music_count, 1);
    DTR_ASSERT(strcmp(cart->music_paths[0], "theme.ogg") == 0);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Parse — code path and maps                                         */
/* ------------------------------------------------------------------ */

static void test_cart_parse_code_maps(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"code\": \"main.js\","
                       "  \"maps\": [\"level1.json\", \"level2.json\"]"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT(strcmp(cart->code_path, "main.js") == 0);
    DTR_ASSERT_EQ_INT(cart->map_count, 2);
    DTR_ASSERT(strcmp(cart->map_paths[0], "level1.json") == 0);
    DTR_ASSERT(strcmp(cart->map_paths[1], "level2.json") == 0);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_cart_parse_build_command(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"code\": \"dist/main.js\","
                       "  \"buildCommand\": \"npx esbuild src/main.ts --bundle\""
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT(strcmp(cart->code_path, "dist/main.js") == 0);
    DTR_ASSERT(strcmp(cart->build_command, "npx esbuild src/main.ts --bundle") == 0);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_cart_parse_build_command_absent(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"code\": \"src/main.js\""
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT(cart->build_command[0] == '\0');
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Parse — sprites section                                            */
/* ------------------------------------------------------------------ */

static void test_cart_parse_sprites(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"sprites\": {"
                       "    \"tileW\": 16,"
                       "    \"tileH\": 16"
                       "  }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(cart->sprites.tile_w, 16);
    DTR_ASSERT_EQ_INT(cart->sprites.tile_h, 16);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Destroy null safety                                                */
/* ------------------------------------------------------------------ */

static void test_cart_destroy_null(void)
{
    dtr_cart_destroy(NULL);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Edge case: extremely large display dimensions are clamped          */
/* ------------------------------------------------------------------ */

static void test_cart_parse_huge_display(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"display\": {"
                       "    \"width\": 99999,"
                       "    \"height\": 99999,"
                       "    \"scale\": 100"
                       "  }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    /* Validate should clamp these back to defaults */
    dtr_cart_validate(cart);
    DTR_ASSERT_EQ_INT(cart->display.width, CONSOLE_FB_WIDTH);
    DTR_ASSERT_EQ_INT(cart->display.height, CONSOLE_FB_HEIGHT);
    DTR_ASSERT_EQ_INT(cart->display.scale, CONSOLE_DEFAULT_SCALE);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Edge case: negative values in numeric fields                       */
/* ------------------------------------------------------------------ */

static void test_cart_parse_negative_values(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"display\": { \"width\": -1, \"height\": -100 },"
                       "  \"timing\": { \"fps\": -5 },"
                       "  \"audio\": { \"channels\": -10, \"frequency\": -1 }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    /* All should be clamped by validate */
    dtr_cart_validate(cart);
    DTR_ASSERT(cart->display.width >= 64);
    DTR_ASSERT(cart->display.height >= 64);
    DTR_ASSERT(cart->timing.fps >= 15);
    DTR_ASSERT(cart->audio.channels >= 0);
    DTR_ASSERT(cart->audio.frequency >= 8000);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Edge case: huge runtime memory limit is clamped                    */
/* ------------------------------------------------------------------ */

static void test_cart_parse_huge_runtime(void)
{
    dtr_cart_t *cart;
    bool        ok;
    uint32_t    max_mem;
    uint32_t    max_stack;
    const char *json = "{"
                       "  \"runtime\": {"
                       "    \"memoryLimitMB\": 99999,"
                       "    \"stackLimitKB\": 99999"
                       "  }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    dtr_cart_validate(cart);
    max_mem   = (uint32_t)CONSOLE_JS_MEM_MB * 1024u * 1024u;
    max_stack = (uint32_t)CONSOLE_JS_STACK_KB * 1024u;
    DTR_ASSERT(cart->runtime.mem_limit <= max_mem);
    DTR_ASSERT(cart->runtime.stack_limit <= max_stack);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Edge case: empty string fields                                     */
/* ------------------------------------------------------------------ */

static void test_cart_parse_empty_strings(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"title\": \"\", \"author\": \"\","
                       "  \"code\": \"\","
                       "  \"sfx\": [],"
                       "  \"music\": [],"
                       "  \"maps\": []"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(cart->sfx_count, 0);
    DTR_ASSERT_EQ_INT(cart->music_count, 0);
    DTR_ASSERT_EQ_INT(cart->map_count, 0);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Edge case: unknown top-level keys are safely ignored               */
/* ------------------------------------------------------------------ */

static void test_cart_parse_unknown_keys(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *json = "{"
                       "  \"unknown_key\": 42,"
                       "  \"another\": { \"nested\": true },"
                       "  \"display\": { \"width\": 256 }"
                       "}";

    cart = dtr_cart_create();
    prv_setup();
    ok = dtr_cart_parse(cart, s_ctx, json, strlen(json));
    DTR_ASSERT(ok);
    DTR_ASSERT_EQ_INT(cart->display.width, 256);
    prv_teardown();
    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Persist round-trip — dslots and KV survive save/load               */
/* ------------------------------------------------------------------ */

static void test_cart_persist_round_trip(void)
{
    dtr_cart_t *cart;
    dtr_cart_t *cart2;
    bool        ok;

    /* Save cart with data */
    cart = dtr_cart_create();
    SDL_strlcpy(cart->meta.title, "dithr_test_persist", sizeof(cart->meta.title));

    dtr_cart_dset(cart, 0, 42.5);
    dtr_cart_dset(cart, 63, -100.0);
    dtr_cart_save(cart, "score", "9999");
    dtr_cart_save(cart, "name", "test");

    ok = dtr_cart_persist_save(cart);
    DTR_ASSERT(ok);
    dtr_cart_destroy(cart);

    /* Load into a fresh cart */
    cart2 = dtr_cart_create();
    SDL_strlcpy(cart2->meta.title, "dithr_test_persist", sizeof(cart2->meta.title));

    ok = dtr_cart_persist_load(cart2);
    DTR_ASSERT(ok);

    /* Verify dslots restored */
    DTR_ASSERT_NEAR(dtr_cart_dget(cart2, 0), 42.5, 0.001);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart2, 63), -100.0, 0.001);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart2, 1), 0.0, 0.001);

    /* Verify KV restored */
    DTR_ASSERT(dtr_cart_has_key(cart2, "score"));
    DTR_ASSERT_EQ_STR(dtr_cart_load_key(cart2, "score"), "9999");
    DTR_ASSERT(dtr_cart_has_key(cart2, "name"));
    DTR_ASSERT_EQ_STR(dtr_cart_load_key(cart2, "name"), "test");

    dtr_cart_destroy(cart2);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Persist — empty data round-trip                                    */
/* ------------------------------------------------------------------ */

static void test_cart_persist_empty(void)
{
    dtr_cart_t *cart;
    dtr_cart_t *cart2;
    bool        ok;

    /* Save cart with no data at all */
    cart = dtr_cart_create();
    SDL_strlcpy(cart->meta.title, "dithr_test_empty", sizeof(cart->meta.title));

    ok = dtr_cart_persist_save(cart);
    DTR_ASSERT(ok);
    dtr_cart_destroy(cart);

    /* Load into fresh cart */
    cart2 = dtr_cart_create();
    SDL_strlcpy(cart2->meta.title, "dithr_test_empty", sizeof(cart2->meta.title));

    ok = dtr_cart_persist_load(cart2);
    DTR_ASSERT(ok);

    /* All slots should still be zero */
    for (int32_t i = 0; i < DTR_CART_MAX_DSLOTS; ++i) {
        DTR_ASSERT_NEAR(dtr_cart_dget(cart2, i), 0.0, 0.001);
    }
    DTR_ASSERT_EQ_INT(cart2->kv_count, 0);

    dtr_cart_destroy(cart2);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Persist load — missing file returns true (not an error)            */
/* ------------------------------------------------------------------ */

static void test_cart_persist_load_missing(void)
{
    dtr_cart_t *cart;
    bool        ok;

    cart = dtr_cart_create();
    /* Use a title that has no save file */
    SDL_strlcpy(cart->meta.title, "dithr_test_nosuchgame_xyz", sizeof(cart->meta.title));

    ok = dtr_cart_persist_load(cart);
    /* Missing file is not an error */
    DTR_ASSERT(ok);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Persist — overwrite existing save                                  */
/* ------------------------------------------------------------------ */

static void test_cart_persist_overwrite(void)
{
    dtr_cart_t *cart;
    dtr_cart_t *cart2;
    bool        ok;

    /* First save */
    cart = dtr_cart_create();
    SDL_strlcpy(cart->meta.title, "dithr_test_overwrite", sizeof(cart->meta.title));
    dtr_cart_dset(cart, 0, 1.0);
    ok = dtr_cart_persist_save(cart);
    DTR_ASSERT(ok);
    dtr_cart_destroy(cart);

    /* Overwrite with different data */
    cart = dtr_cart_create();
    SDL_strlcpy(cart->meta.title, "dithr_test_overwrite", sizeof(cart->meta.title));
    dtr_cart_dset(cart, 0, 999.0);
    dtr_cart_save(cart, "key", "val");
    ok = dtr_cart_persist_save(cart);
    DTR_ASSERT(ok);
    dtr_cart_destroy(cart);

    /* Load and verify latest data */
    cart2 = dtr_cart_create();
    SDL_strlcpy(cart2->meta.title, "dithr_test_overwrite", sizeof(cart2->meta.title));
    ok = dtr_cart_persist_load(cart2);
    DTR_ASSERT(ok);
    DTR_ASSERT_NEAR(dtr_cart_dget(cart2, 0), 999.0, 0.001);
    DTR_ASSERT(dtr_cart_has_key(cart2, "key"));
    DTR_ASSERT_EQ_STR(dtr_cart_load_key(cart2, "key"), "val");

    dtr_cart_destroy(cart2);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    DTR_TEST_BEGIN("test_cart");

    DTR_RUN_TEST(test_cart_defaults);
    DTR_RUN_TEST(test_cart_parse_minimal);
    DTR_RUN_TEST(test_cart_parse_display);
    DTR_RUN_TEST(test_cart_parse_meta);
    DTR_RUN_TEST(test_cart_parse_input);
    DTR_RUN_TEST(test_cart_parse_invalid_json);
    DTR_RUN_TEST(test_cart_validate_defaults);
    DTR_RUN_TEST(test_cart_validate_clamp_width);
    DTR_RUN_TEST(test_cart_validate_clamp_fps);
    DTR_RUN_TEST(test_cart_validate_clamp_tile);
    DTR_RUN_TEST(test_cart_dset_dget);
    DTR_RUN_TEST(test_cart_dset_dget_out_of_range);
    DTR_RUN_TEST(test_cart_kv_save_load);
    DTR_RUN_TEST(test_cart_kv_delete);
    DTR_RUN_TEST(test_cart_kv_max_keys);
    DTR_RUN_TEST(test_cart_validate_clamp_height);
    DTR_RUN_TEST(test_cart_parse_timing);
    DTR_RUN_TEST(test_cart_parse_audio);
    DTR_RUN_TEST(test_cart_validate_clamp_scale);
    DTR_RUN_TEST(test_cart_validate_clamp_fps_upper);
    DTR_RUN_TEST(test_cart_validate_clamp_ups_lower);
    DTR_RUN_TEST(test_cart_validate_clamp_ups_upper);
    DTR_RUN_TEST(test_cart_validate_clamp_audio_channels);
    DTR_RUN_TEST(test_cart_validate_clamp_audio_freq);
    DTR_RUN_TEST(test_cart_validate_clamp_audio_buffer);
    DTR_RUN_TEST(test_cart_validate_clamp_runtime);
    DTR_RUN_TEST(test_cart_parse_runtime);
    DTR_RUN_TEST(test_cart_parse_sfx_music);
    DTR_RUN_TEST(test_cart_parse_code_maps);
    DTR_RUN_TEST(test_cart_parse_build_command);
    DTR_RUN_TEST(test_cart_parse_build_command_absent);
    DTR_RUN_TEST(test_cart_parse_sprites);
    DTR_RUN_TEST(test_cart_destroy_null);
    DTR_RUN_TEST(test_cart_parse_huge_display);
    DTR_RUN_TEST(test_cart_parse_negative_values);
    DTR_RUN_TEST(test_cart_parse_huge_runtime);
    DTR_RUN_TEST(test_cart_parse_empty_strings);
    DTR_RUN_TEST(test_cart_parse_unknown_keys);
    DTR_RUN_TEST(test_cart_persist_round_trip);
    DTR_RUN_TEST(test_cart_persist_empty);
    DTR_RUN_TEST(test_cart_persist_load_missing);
    DTR_RUN_TEST(test_cart_persist_overwrite);

    DTR_TEST_END();
}
