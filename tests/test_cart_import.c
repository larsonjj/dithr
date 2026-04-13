/**
 * \file            test_cart_import.c
 * \brief           Unit tests for Tiled / LDtk map importers
 */

#include "cart_import.h"
#include "quickjs.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
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

/** Write a string to a temporary file.  Returns true on success. */
static bool prv_write_file(const char *path, const char *data)
{
    FILE *f;

    f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    fwrite(data, 1, strlen(data), f);
    fclose(f);
    return true;
}

/** Free a parsed level and its sub-allocations (tiles, objects, layers). */
static void prv_free_level(dtr_map_level_t *level)
{
    if (level == NULL) {
        return;
    }
    for (int32_t li = 0; li < level->layer_count; ++li) {
        DTR_FREE(level->layers[li].tiles);
        for (int32_t oi = 0; oi < level->layers[li].object_count; ++oi) {
            dtr_map_object_t *obj = &level->layers[li].objects[oi];
            if (!JS_IsUndefined(obj->props)) {
                JS_FreeValue(s_ctx, obj->props);
            }
        }
        DTR_FREE(level->layers[li].objects);
    }
    DTR_FREE(level->layers);
    DTR_FREE(level);
}

/* ------------------------------------------------------------------ */
/*  Tiled importer                                                     */
/* ------------------------------------------------------------------ */

static const char *TILED_TILE_LAYER_JSON = "{"
                                           "  \"width\": 4,"
                                           "  \"height\": 3,"
                                           "  \"layers\": ["
                                           "    {"
                                           "      \"name\": \"ground\","
                                           "      \"type\": \"tilelayer\","
                                           "      \"width\": 4,"
                                           "      \"height\": 3,"
                                           "      \"data\": [1,2,3,0, 0,1,0,2, 3,0,1,0]"
                                           "    }"
                                           "  ]"
                                           "}";

static void test_tiled_tile_layer(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_tiled_tile.tmj";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, TILED_TILE_LAYER_JSON));
    ok = dtr_import_tiled(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->width, 4);
    DTR_ASSERT_EQ_INT(level->height, 3);
    DTR_ASSERT_EQ_INT(level->layer_count, 1);
    DTR_ASSERT(level->layers[0].is_tile_layer);
    DTR_ASSERT(strcmp(level->layers[0].name, "ground") == 0);
    DTR_ASSERT_EQ_INT(level->layers[0].width, 4);
    DTR_ASSERT_EQ_INT(level->layers[0].height, 3);

    /* Verify tile data */
    DTR_ASSERT_EQ_INT(level->layers[0].tiles[0], 1);
    DTR_ASSERT_EQ_INT(level->layers[0].tiles[1], 2);
    DTR_ASSERT_EQ_INT(level->layers[0].tiles[2], 3);
    DTR_ASSERT_EQ_INT(level->layers[0].tiles[3], 0);

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

static const char *TILED_OBJECT_LAYER_JSON = "{"
                                             "  \"width\": 2,"
                                             "  \"height\": 2,"
                                             "  \"layers\": ["
                                             "    {"
                                             "      \"name\": \"entities\","
                                             "      \"type\": \"objectgroup\","
                                             "      \"objects\": ["
                                             "        {"
                                             "          \"name\": \"player\","
                                             "          \"type\": \"spawn\","
                                             "          \"x\": 10.5,"
                                             "          \"y\": 20.0,"
                                             "          \"width\": 16,"
                                             "          \"height\": 16,"
                                             "          \"gid\": 7"
                                             "        }"
                                             "      ]"
                                             "    }"
                                             "  ]"
                                             "}";

static void test_tiled_object_layer(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_tiled_obj.tmj";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, TILED_OBJECT_LAYER_JSON));
    ok = dtr_import_tiled(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->layer_count, 1);
    DTR_ASSERT(!level->layers[0].is_tile_layer);
    DTR_ASSERT_EQ_INT(level->layers[0].object_count, 1);

    /* Verify object data */
    DTR_ASSERT(strcmp(level->layers[0].objects[0].name, "player") == 0);
    DTR_ASSERT(strcmp(level->layers[0].objects[0].type, "spawn") == 0);
    DTR_ASSERT_NEAR(level->layers[0].objects[0].x, 10.5f, 0.01);
    DTR_ASSERT_NEAR(level->layers[0].objects[0].y, 20.0f, 0.01);
    DTR_ASSERT_EQ_INT(level->layers[0].objects[0].gid, 7);

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

static void test_tiled_missing_file(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;

    prv_setup();
    ok = dtr_import_tiled("nonexistent_file.tmj", &level, s_ctx);
    DTR_ASSERT(!ok);
    DTR_ASSERT(level == NULL);
    prv_teardown();
    DTR_PASS();
}

static void test_tiled_invalid_json(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_tiled_bad.tmj";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, "{ this is not valid json }"));
    ok = dtr_import_tiled(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(!ok);
    DTR_ASSERT(level == NULL);
    prv_teardown();
    DTR_PASS();
}

static void test_tiled_no_layers(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_tiled_nolayers.tmj";
    const char      *json = "{\"width\": 2, \"height\": 2}";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, json));
    ok = dtr_import_tiled(path, &level, s_ctx);
    remove(path);

    /* Should succeed with empty map */
    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->width, 2);
    DTR_ASSERT_EQ_INT(level->height, 2);
    DTR_ASSERT_EQ_INT(level->layer_count, 0);

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

static const char *TILED_MIXED_LAYERS_JSON = "{"
                                             "  \"width\": 2,"
                                             "  \"height\": 2,"
                                             "  \"layers\": ["
                                             "    {"
                                             "      \"name\": \"bg\","
                                             "      \"type\": \"tilelayer\","
                                             "      \"width\": 2,"
                                             "      \"height\": 2,"
                                             "      \"data\": [1, 2, 3, 4]"
                                             "    },"
                                             "    {"
                                             "      \"name\": \"objs\","
                                             "      \"type\": \"objectgroup\","
                                             "      \"objects\": []"
                                             "    }"
                                             "  ]"
                                             "}";

static void test_tiled_mixed_layers(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_tiled_mixed.tmj";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, TILED_MIXED_LAYERS_JSON));
    ok = dtr_import_tiled(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->layer_count, 2);
    DTR_ASSERT(level->layers[0].is_tile_layer);
    DTR_ASSERT(!level->layers[1].is_tile_layer);
    DTR_ASSERT_EQ_INT(level->layers[1].object_count, 0);

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Tiled: object with properties                                      */
/* ------------------------------------------------------------------ */

static const char *TILED_OBJECT_PROPS_JSON =
    "{"
    "  \"width\": 2,"
    "  \"height\": 2,"
    "  \"layers\": ["
    "    {"
    "      \"name\": \"ents\","
    "      \"type\": \"objectgroup\","
    "      \"objects\": ["
    "        {"
    "          \"name\": \"door\","
    "          \"type\": \"trigger\","
    "          \"x\": 0,"
    "          \"y\": 0,"
    "          \"width\": 8,"
    "          \"height\": 8,"
    "          \"properties\": ["
    "            { \"name\": \"target\", \"value\": \"level2\" }"
    "          ]"
    "        }"
    "      ]"
    "    }"
    "  ]"
    "}";

static void test_tiled_object_properties(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_tiled_props.tmj";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, TILED_OBJECT_PROPS_JSON));
    ok = dtr_import_tiled(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->layers[0].object_count, 1);

    /* Verify property was stored in JS object */
    {
        dtr_map_object_t *obj = &level->layers[0].objects[0];
        JSValue           val;
        const char       *str;

        DTR_ASSERT(!JS_IsUndefined(obj->props));
        val = JS_GetPropertyStr(s_ctx, obj->props, "target");
        str = JS_ToCString(s_ctx, val);
        DTR_ASSERT(str != NULL);
        DTR_ASSERT(strcmp(str, "level2") == 0);
        JS_FreeCString(s_ctx, str);
        JS_FreeValue(s_ctx, val);
    }

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  LDtk importer                                                     */
/* ------------------------------------------------------------------ */

static const char *LDTK_BASIC_JSON = "{"
                                     "  \"levels\": ["
                                     "    {"
                                     "      \"pxWid\": 160,"
                                     "      \"pxHid\": 128,"
                                     "      \"layerInstances\": ["
                                     "        {"
                                     "          \"__type\": \"Tiles\","
                                     "          \"__gridSize\": 16,"
                                     "          \"gridTiles\": ["
                                     "            { \"px\": [0, 0], \"t\": 5 },"
                                     "            { \"px\": [16, 0], \"t\": 3 }"
                                     "          ]"
                                     "        }"
                                     "      ]"
                                     "    }"
                                     "  ]"
                                     "}";

static void test_ldtk_basic(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_ldtk_basic.ldtk";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, LDTK_BASIC_JSON));
    ok = dtr_import_ldtk(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->width, 160);
    DTR_ASSERT_EQ_INT(level->height, 128);
    DTR_ASSERT_EQ_INT(level->layer_count, 1);
    DTR_ASSERT(level->layers[0].is_tile_layer);

    /* Grid is 160/16=10 x 128/16=8 */
    DTR_ASSERT_EQ_INT(level->layers[0].width, 10);
    DTR_ASSERT_EQ_INT(level->layers[0].height, 8);

    /* Tile at grid (0,0) = tile id 5 */
    DTR_ASSERT_EQ_INT(level->layers[0].tiles[0], 5);
    /* Tile at grid (1,0) = tile id 3 */
    DTR_ASSERT_EQ_INT(level->layers[0].tiles[1], 3);

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

static void test_ldtk_missing_file(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;

    prv_setup();
    ok = dtr_import_ldtk("nonexistent_file.ldtk", &level, s_ctx);
    DTR_ASSERT(!ok);
    DTR_ASSERT(level == NULL);
    prv_teardown();
    DTR_PASS();
}

static void test_ldtk_invalid_json(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_ldtk_bad.ldtk";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, "not json at all!!!"));
    ok = dtr_import_ldtk(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(!ok);
    DTR_ASSERT(level == NULL);
    prv_teardown();
    DTR_PASS();
}

static void test_ldtk_empty_levels(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_ldtk_empty.ldtk";
    const char      *json = "{\"levels\": []}";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, json));
    ok = dtr_import_ldtk(path, &level, s_ctx);
    remove(path);

    /* Should succeed with an empty level struct */
    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->layer_count, 0);

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

static void test_ldtk_no_levels_key(void)
{
    dtr_map_level_t *level = NULL;
    bool             ok;
    const char      *path = "_test_ldtk_nolevels.ldtk";
    const char      *json = "{\"something\": 42}";

    prv_setup();

    DTR_ASSERT(prv_write_file(path, json));
    ok = dtr_import_ldtk(path, &level, s_ctx);
    remove(path);

    DTR_ASSERT(ok);
    DTR_ASSERT(level != NULL);
    DTR_ASSERT_EQ_INT(level->layer_count, 0);

    prv_free_level(level);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Aseprite importer (can test NULL / missing file path)              */
/* ------------------------------------------------------------------ */

static void test_aseprite_missing_file(void)
{
    dtr_cart_t *cart;
    bool        ok;

    prv_setup();
    cart = dtr_cart_create();
    DTR_ASSERT(cart != NULL);

    ok = dtr_import_aseprite("nonexistent_file.json", cart, s_ctx);
    DTR_ASSERT(!ok);
    DTR_ASSERT(cart->sprite_rgba == NULL);

    dtr_cart_destroy(cart);
    prv_teardown();
    DTR_PASS();
}

static void test_aseprite_invalid_json(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *path = "_test_ase_bad.json";

    prv_setup();
    cart = dtr_cart_create();
    DTR_ASSERT(cart != NULL);

    DTR_ASSERT(prv_write_file(path, "NOT JSON"));
    ok = dtr_import_aseprite(path, cart, s_ctx);
    remove(path);

    DTR_ASSERT(!ok);

    dtr_cart_destroy(cart);
    prv_teardown();
    DTR_PASS();
}

static void test_aseprite_no_meta(void)
{
    dtr_cart_t *cart;
    bool        ok;
    const char *path = "_test_ase_nometa.json";
    const char *json = "{\"frames\": {}}";

    prv_setup();
    cart = dtr_cart_create();
    DTR_ASSERT(cart != NULL);

    DTR_ASSERT(prv_write_file(path, json));
    ok = dtr_import_aseprite(path, cart, s_ctx);
    remove(path);

    /* No meta → sprite_rgba stays NULL → returns false */
    DTR_ASSERT(!ok);

    dtr_cart_destroy(cart);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  PNG importer (missing file)                                        */
/* ------------------------------------------------------------------ */

static void test_png_missing_file(void)
{
    int32_t  w = -1;
    int32_t  h = -1;
    uint8_t *pixels;

    pixels = dtr_import_png("nonexistent_file.png", &w, &h);
    DTR_ASSERT(pixels == NULL);
    DTR_ASSERT_EQ_INT(w, 0);
    DTR_ASSERT_EQ_INT(h, 0);

    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  PNG importer (corrupt file → graceful failure)                     */
/* ------------------------------------------------------------------ */

static void test_png_corrupt_file(void)
{
    int32_t     w = -1;
    int32_t     h = -1;
    uint8_t    *pixels;
    const char *path = "_test_corrupt.png";

    DTR_ASSERT(prv_write_file(path, "this is not a PNG file"));
    pixels = dtr_import_png(path, &w, &h);
    remove(path);

    DTR_ASSERT(pixels == NULL);
    DTR_ASSERT_EQ_INT(w, 0);
    DTR_ASSERT_EQ_INT(h, 0);

    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    DTR_TEST_BEGIN("test_cart_import");

    /* Tiled */
    DTR_RUN_TEST(test_tiled_tile_layer);
    DTR_RUN_TEST(test_tiled_object_layer);
    DTR_RUN_TEST(test_tiled_missing_file);
    DTR_RUN_TEST(test_tiled_invalid_json);
    DTR_RUN_TEST(test_tiled_no_layers);
    DTR_RUN_TEST(test_tiled_mixed_layers);
    DTR_RUN_TEST(test_tiled_object_properties);

    /* LDtk */
    DTR_RUN_TEST(test_ldtk_basic);
    DTR_RUN_TEST(test_ldtk_missing_file);
    DTR_RUN_TEST(test_ldtk_invalid_json);
    DTR_RUN_TEST(test_ldtk_empty_levels);
    DTR_RUN_TEST(test_ldtk_no_levels_key);

    /* Aseprite */
    DTR_RUN_TEST(test_aseprite_missing_file);
    DTR_RUN_TEST(test_aseprite_invalid_json);
    DTR_RUN_TEST(test_aseprite_no_meta);

    /* PNG */
    DTR_RUN_TEST(test_png_missing_file);
    DTR_RUN_TEST(test_png_corrupt_file);

    DTR_TEST_END();
}
