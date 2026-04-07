/**
 * \file            test_map.c
 * \brief           Unit tests for map data structures and lifecycle
 *
 * Tests map level / layer / object CRUD, tile access, resize, rename,
 * and cleanup via the public cart helpers and direct struct manipulation.
 */

#include "cart.h"
#include "console_defs.h"
#include "test_harness.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/** Create a minimal cart with one map level of the given size. */
static dtr_cart_t *prv_make_cart(int32_t width, int32_t height, const char *name)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;

    cart = dtr_cart_create();
    DTR_ASSERT_NOT_NULL(cart);

    level = DTR_CALLOC(1, sizeof(dtr_map_level_t));
    DTR_ASSERT_NOT_NULL(level);

    level->width       = width;
    level->height      = height;
    level->tile_w      = 8;
    level->tile_h      = 8;
    level->layer_count = 1;
    level->layers      = DTR_CALLOC(1, sizeof(dtr_map_layer_t));
    DTR_ASSERT_NOT_NULL(level->layers);

    layer                = &level->layers[0];
    layer->is_tile_layer = true;
    layer->width         = width;
    layer->height        = height;
    layer->tiles         = DTR_CALLOC((size_t)(width * height), sizeof(int32_t));
    DTR_ASSERT_NOT_NULL(layer->tiles);
    SDL_strlcpy(layer->name, "Layer 1", sizeof(layer->name));

    SDL_strlcpy(level->name, name, sizeof(level->name));

    cart->maps[0]     = level;
    cart->map_count   = 1;
    cart->current_map = 0;

    return cart;
}

/** Add a tile layer to an existing level. Returns the new layer index or -1. */
static int32_t prv_add_tile_layer(dtr_map_level_t *level, const char *name)
{
    dtr_map_layer_t *new_layers;
    dtr_map_layer_t *layer;

    if (level->layer_count >= CONSOLE_MAX_MAP_LAYERS) {
        return -1;
    }

    new_layers =
        DTR_REALLOC(level->layers, (size_t)(level->layer_count + 1) * sizeof(dtr_map_layer_t));
    if (new_layers == NULL) {
        return -1;
    }
    level->layers = new_layers;

    layer = &level->layers[level->layer_count];
    SDL_memset(layer, 0, sizeof(dtr_map_layer_t));
    layer->is_tile_layer = true;
    layer->width         = level->width;
    layer->height        = level->height;
    layer->tiles         = DTR_CALLOC((size_t)(level->width * level->height), sizeof(int32_t));
    if (layer->tiles == NULL) {
        return -1;
    }
    SDL_strlcpy(layer->name, name, sizeof(layer->name));

    level->layer_count += 1;
    return level->layer_count - 1;
}

/** Add an object layer to an existing level. Returns the new layer index or -1. */
static int32_t prv_add_object_layer(dtr_map_level_t *level, const char *name)
{
    dtr_map_layer_t *new_layers;
    dtr_map_layer_t *layer;

    if (level->layer_count >= CONSOLE_MAX_MAP_LAYERS) {
        return -1;
    }

    new_layers =
        DTR_REALLOC(level->layers, (size_t)(level->layer_count + 1) * sizeof(dtr_map_layer_t));
    if (new_layers == NULL) {
        return -1;
    }
    level->layers = new_layers;

    layer = &level->layers[level->layer_count];
    SDL_memset(layer, 0, sizeof(dtr_map_layer_t));
    layer->is_tile_layer = false;
    layer->width         = level->width;
    layer->height        = level->height;
    SDL_strlcpy(layer->name, name, sizeof(layer->name));

    level->layer_count += 1;
    return level->layer_count - 1;
}

/** Add an object to an object layer. Returns the new object index or -1. */
static int32_t prv_add_object(dtr_map_layer_t *layer,
                              const char      *name,
                              float            x_pos,
                              float            y_pos,
                              float            wid,
                              float            hgt)
{
    dtr_map_object_t *new_objs;
    dtr_map_object_t *obj;

    if (layer->object_count >= CONSOLE_MAX_MAP_OBJECTS) {
        return -1;
    }

    new_objs =
        DTR_REALLOC(layer->objects, (size_t)(layer->object_count + 1) * sizeof(dtr_map_object_t));
    if (new_objs == NULL) {
        return -1;
    }
    layer->objects = new_objs;

    obj = &layer->objects[layer->object_count];
    SDL_memset(obj, 0, sizeof(dtr_map_object_t));
    obj->props = JS_UNDEFINED;
    SDL_strlcpy(obj->name, name, sizeof(obj->name));
    obj->x = x_pos;
    obj->y = y_pos;
    obj->w = wid;
    obj->h = hgt;

    layer->object_count += 1;
    return layer->object_count - 1;
}

/** Remove a layer by index, shifting remaining layers down. */
static bool prv_remove_layer(dtr_map_level_t *level, int32_t idx)
{
    if (idx < 0 || idx >= level->layer_count || level->layer_count <= 1) {
        return false;
    }

    DTR_FREE(level->layers[idx].tiles);
    DTR_FREE(level->layers[idx].objects);

    for (int32_t shi = idx; shi < level->layer_count - 1; ++shi) {
        level->layers[shi] = level->layers[shi + 1];
    }
    level->layer_count -= 1;
    return true;
}

/** Remove an object by index, shifting remaining objects down. */
static bool prv_remove_object(dtr_map_layer_t *layer, int32_t idx)
{
    if (idx < 0 || idx >= layer->object_count) {
        return false;
    }

    for (int32_t shi = idx; shi < layer->object_count - 1; ++shi) {
        layer->objects[shi] = layer->objects[shi + 1];
    }
    layer->object_count -= 1;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Level creation                                                     */
/* ------------------------------------------------------------------ */

static void test_map_create_basic(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(16, 16, "test");
    level = cart->maps[0];

    DTR_ASSERT_EQ_INT(level->width, 16);
    DTR_ASSERT_EQ_INT(level->height, 16);
    DTR_ASSERT_EQ_INT(level->layer_count, 1);
    DTR_ASSERT_EQ_STR(level->name, "test");
    DTR_ASSERT(level->layers[0].is_tile_layer);
    DTR_ASSERT_NOT_NULL(level->layers[0].tiles);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_create_bounds(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(4096, 4096, "maxmap");
    level = cart->maps[0];

    DTR_ASSERT_EQ_INT(level->width, 4096);
    DTR_ASSERT_EQ_INT(level->height, 4096);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Tile get / set                                                     */
/* ------------------------------------------------------------------ */

static void test_map_tile_set_get(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;

    cart  = prv_make_cart(8, 8, "tiles");
    level = cart->maps[0];
    layer = &level->layers[0];

    /* Set a tile and read it back */
    layer->tiles[3 * 8 + 5] = 42;
    DTR_ASSERT_EQ_INT(layer->tiles[3 * 8 + 5], 42);

    /* Verify default is 0 */
    DTR_ASSERT_EQ_INT(layer->tiles[0], 0);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_tile_all_cells(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          idx;

    cart  = prv_make_cart(4, 4, "fill");
    level = cart->maps[0];
    layer = &level->layers[0];

    /* Fill all cells */
    for (idx = 0; idx < 16; ++idx) {
        layer->tiles[idx] = idx + 1;
    }
    /* Verify */
    for (idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(layer->tiles[idx], idx + 1);
    }

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Layer add / remove                                                 */
/* ------------------------------------------------------------------ */

static void test_map_add_layer(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    int32_t          idx;

    cart  = prv_make_cart(8, 8, "layers");
    level = cart->maps[0];

    idx = prv_add_tile_layer(level, "Second");
    DTR_ASSERT_EQ_INT(idx, 1);
    DTR_ASSERT_EQ_INT(level->layer_count, 2);
    DTR_ASSERT_EQ_STR(level->layers[1].name, "Second");
    DTR_ASSERT(level->layers[1].is_tile_layer);
    DTR_ASSERT_NOT_NULL(level->layers[1].tiles);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_add_layer_max(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    int32_t          idx;
    char             name[32];

    cart  = prv_make_cart(4, 4, "maxlayers");
    level = cart->maps[0];

    /* Add layers up to the max (already have 1) */
    for (int32_t cnt = 1; cnt < CONSOLE_MAX_MAP_LAYERS; ++cnt) {
        SDL_snprintf(name, sizeof(name), "Layer %d", cnt + 1);
        idx = prv_add_tile_layer(level, name);
        DTR_ASSERT(idx >= 0);
    }
    DTR_ASSERT_EQ_INT(level->layer_count, CONSOLE_MAX_MAP_LAYERS);

    /* One more should fail */
    idx = prv_add_tile_layer(level, "overflow");
    DTR_ASSERT_EQ_INT(idx, -1);
    DTR_ASSERT_EQ_INT(level->layer_count, CONSOLE_MAX_MAP_LAYERS);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_remove_layer(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    bool             res;

    cart  = prv_make_cart(4, 4, "rmtest");
    level = cart->maps[0];
    prv_add_tile_layer(level, "Second");
    DTR_ASSERT_EQ_INT(level->layer_count, 2);

    /* Remove first layer */
    res = prv_remove_layer(level, 0);
    DTR_ASSERT(res);
    DTR_ASSERT_EQ_INT(level->layer_count, 1);
    DTR_ASSERT_EQ_STR(level->layers[0].name, "Second");

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_remove_last_layer_fails(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    bool             res;

    cart  = prv_make_cart(4, 4, "single");
    level = cart->maps[0];

    /* Cannot remove the only layer */
    res = prv_remove_layer(level, 0);
    DTR_ASSERT(!res);
    DTR_ASSERT_EQ_INT(level->layer_count, 1);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_remove_layer_out_of_bounds(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(4, 4, "oob");
    level = cart->maps[0];
    prv_add_tile_layer(level, "Extra");

    DTR_ASSERT(!prv_remove_layer(level, -1));
    DTR_ASSERT(!prv_remove_layer(level, 99));
    DTR_ASSERT_EQ_INT(level->layer_count, 2);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Layer rename                                                       */
/* ------------------------------------------------------------------ */

static void test_map_rename_layer(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(4, 4, "rename");
    level = cart->maps[0];

    SDL_strlcpy(level->layers[0].name, "NewName", sizeof(level->layers[0].name));
    DTR_ASSERT_EQ_STR(level->layers[0].name, "NewName");

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_rename_layer_truncation(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(4, 4, "truncate");
    level = cart->maps[0];

    /* name[32]: string longer than 31 chars should be truncated */
    SDL_strlcpy(level->layers[0].name,
                "ThisIsAVeryLongLayerNameThatExceeds32Characters",
                sizeof(level->layers[0].name));

    DTR_ASSERT(strlen(level->layers[0].name) <= 31);
    DTR_ASSERT_EQ_INT(level->layers[0].name[31], '\0');

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Resize                                                             */
/* ------------------------------------------------------------------ */

static void test_map_resize_grow(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t         *new_tiles;
    int32_t          old_w;
    int32_t          old_h;
    int32_t          new_w;
    int32_t          new_h;
    int32_t          copy_w;
    int32_t          copy_h;

    cart  = prv_make_cart(4, 4, "grow");
    level = cart->maps[0];
    layer = &level->layers[0];

    /* Paint a known pattern */
    layer->tiles[0 * 4 + 0] = 1;
    layer->tiles[1 * 4 + 1] = 2;
    layer->tiles[3 * 4 + 3] = 3;

    /* Resize to 8x8 */
    old_w = level->width;
    old_h = level->height;
    new_w = 8;
    new_h = 8;

    new_tiles = DTR_CALLOC((size_t)(new_w * new_h), sizeof(int32_t));
    DTR_ASSERT_NOT_NULL(new_tiles);

    copy_w = (old_w < new_w) ? old_w : new_w;
    copy_h = (old_h < new_h) ? old_h : new_h;
    for (int32_t row = 0; row < copy_h; ++row) {
        SDL_memcpy(&new_tiles[(size_t)row * new_w],
                   &layer->tiles[(size_t)row * old_w],
                   (size_t)copy_w * sizeof(int32_t));
    }

    DTR_FREE(layer->tiles);
    layer->tiles  = new_tiles;
    layer->width  = new_w;
    layer->height = new_h;
    level->width  = new_w;
    level->height = new_h;

    /* Verify old data preserved */
    DTR_ASSERT_EQ_INT(layer->tiles[0 * 8 + 0], 1);
    DTR_ASSERT_EQ_INT(layer->tiles[1 * 8 + 1], 2);
    DTR_ASSERT_EQ_INT(layer->tiles[3 * 8 + 3], 3);

    /* New cells should be 0 */
    DTR_ASSERT_EQ_INT(layer->tiles[0 * 8 + 7], 0);
    DTR_ASSERT_EQ_INT(layer->tiles[7 * 8 + 7], 0);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_resize_shrink(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t         *new_tiles;
    int32_t          old_w;
    int32_t          new_w;
    int32_t          new_h;
    int32_t          copy_w;
    int32_t          copy_h;

    cart  = prv_make_cart(8, 8, "shrink");
    level = cart->maps[0];
    layer = &level->layers[0];

    /* Paint tiles */
    layer->tiles[0 * 8 + 0] = 10;
    layer->tiles[1 * 8 + 1] = 20;
    layer->tiles[7 * 8 + 7] = 30; /* will be lost */

    old_w = level->width;
    new_w = 4;
    new_h = 4;

    new_tiles = DTR_CALLOC((size_t)(new_w * new_h), sizeof(int32_t));
    DTR_ASSERT_NOT_NULL(new_tiles);

    copy_w = (old_w < new_w) ? old_w : new_w;
    copy_h = (level->height < new_h) ? level->height : new_h;
    for (int32_t row = 0; row < copy_h; ++row) {
        SDL_memcpy(&new_tiles[(size_t)row * new_w],
                   &layer->tiles[(size_t)row * old_w],
                   (size_t)copy_w * sizeof(int32_t));
    }

    DTR_FREE(layer->tiles);
    layer->tiles  = new_tiles;
    layer->width  = new_w;
    layer->height = new_h;
    level->width  = new_w;
    level->height = new_h;

    /* Tiles within new bounds should be preserved */
    DTR_ASSERT_EQ_INT(layer->tiles[0 * 4 + 0], 10);
    DTR_ASSERT_EQ_INT(layer->tiles[1 * 4 + 1], 20);

    /* Can't access (7,7) anymore — it's outside 4x4 */
    DTR_ASSERT_EQ_INT(level->width, 4);
    DTR_ASSERT_EQ_INT(level->height, 4);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Object layers                                                      */
/* ------------------------------------------------------------------ */

static void test_map_add_object_layer(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    int32_t          idx;

    cart  = prv_make_cart(8, 8, "objlayer");
    level = cart->maps[0];

    idx = prv_add_object_layer(level, "Objects 1");
    DTR_ASSERT_EQ_INT(idx, 1);
    DTR_ASSERT_EQ_INT(level->layer_count, 2);
    DTR_ASSERT(!level->layers[1].is_tile_layer);
    DTR_ASSERT_EQ_STR(level->layers[1].name, "Objects 1");
    DTR_ASSERT(level->layers[1].tiles == NULL);
    DTR_ASSERT_EQ_INT(level->layers[1].object_count, 0);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Object CRUD                                                        */
/* ------------------------------------------------------------------ */

static void test_map_add_object(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          obj_idx;
    int32_t          lay_idx;

    cart    = prv_make_cart(8, 8, "addobj");
    level   = cart->maps[0];
    lay_idx = prv_add_object_layer(level, "Objects");
    layer   = &level->layers[lay_idx];

    obj_idx = prv_add_object(layer, "enemy_0", 16.0f, 32.0f, 8.0f, 8.0f);
    DTR_ASSERT_EQ_INT(obj_idx, 0);
    DTR_ASSERT_EQ_INT(layer->object_count, 1);
    DTR_ASSERT_EQ_STR(layer->objects[0].name, "enemy_0");
    DTR_ASSERT_NEAR(layer->objects[0].x, 16.0, 0.01);
    DTR_ASSERT_NEAR(layer->objects[0].y, 32.0, 0.01);
    DTR_ASSERT_NEAR(layer->objects[0].w, 8.0, 0.01);
    DTR_ASSERT_NEAR(layer->objects[0].h, 8.0, 0.01);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_add_multiple_objects(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          lay_idx;

    cart    = prv_make_cart(8, 8, "multiobj");
    level   = cart->maps[0];
    lay_idx = prv_add_object_layer(level, "Objects");
    layer   = &level->layers[lay_idx];

    prv_add_object(layer, "obj_0", 0.0f, 0.0f, 8.0f, 8.0f);
    prv_add_object(layer, "obj_1", 16.0f, 0.0f, 8.0f, 8.0f);
    prv_add_object(layer, "obj_2", 32.0f, 0.0f, 8.0f, 8.0f);

    DTR_ASSERT_EQ_INT(layer->object_count, 3);
    DTR_ASSERT_EQ_STR(layer->objects[0].name, "obj_0");
    DTR_ASSERT_EQ_STR(layer->objects[1].name, "obj_1");
    DTR_ASSERT_EQ_STR(layer->objects[2].name, "obj_2");

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_remove_object(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          lay_idx;
    bool             res;

    cart    = prv_make_cart(8, 8, "rmobj");
    level   = cart->maps[0];
    lay_idx = prv_add_object_layer(level, "Objects");
    layer   = &level->layers[lay_idx];

    prv_add_object(layer, "first", 0.0f, 0.0f, 8.0f, 8.0f);
    prv_add_object(layer, "second", 16.0f, 0.0f, 8.0f, 8.0f);
    prv_add_object(layer, "third", 32.0f, 0.0f, 8.0f, 8.0f);

    /* Remove the middle one */
    res = prv_remove_object(layer, 1);
    DTR_ASSERT(res);
    DTR_ASSERT_EQ_INT(layer->object_count, 2);
    DTR_ASSERT_EQ_STR(layer->objects[0].name, "first");
    DTR_ASSERT_EQ_STR(layer->objects[1].name, "third");

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_remove_object_out_of_bounds(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          lay_idx;

    cart    = prv_make_cart(4, 4, "rmoob");
    level   = cart->maps[0];
    lay_idx = prv_add_object_layer(level, "Objects");
    layer   = &level->layers[lay_idx];

    prv_add_object(layer, "only", 0.0f, 0.0f, 8.0f, 8.0f);

    DTR_ASSERT(!prv_remove_object(layer, -1));
    DTR_ASSERT(!prv_remove_object(layer, 1));
    DTR_ASSERT_EQ_INT(layer->object_count, 1);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_set_object(void)
{
    dtr_cart_t       *cart;
    dtr_map_level_t  *level;
    dtr_map_layer_t  *layer;
    dtr_map_object_t *obj;
    int32_t           lay_idx;

    cart    = prv_make_cart(8, 8, "setobj");
    level   = cart->maps[0];
    lay_idx = prv_add_object_layer(level, "Objects");
    layer   = &level->layers[lay_idx];

    prv_add_object(layer, "moveme", 0.0f, 0.0f, 8.0f, 8.0f);
    obj = &layer->objects[0];

    /* Simulate set_object: update position */
    obj->x = 100.0f;
    obj->y = 200.0f;
    DTR_ASSERT_NEAR(obj->x, 100.0, 0.01);
    DTR_ASSERT_NEAR(obj->y, 200.0, 0.01);

    /* Simulate set_object: update size */
    obj->w = 24.0f;
    obj->h = 32.0f;
    DTR_ASSERT_NEAR(obj->w, 24.0, 0.01);
    DTR_ASSERT_NEAR(obj->h, 32.0, 0.01);

    /* Simulate set_object: update name */
    SDL_strlcpy(obj->name, "renamed", sizeof(obj->name));
    DTR_ASSERT_EQ_STR(obj->name, "renamed");

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Multi-layer tile isolation                                         */
/* ------------------------------------------------------------------ */

static void test_map_layer_tile_isolation(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(4, 4, "isolate");
    level = cart->maps[0];
    prv_add_tile_layer(level, "Layer 2");

    /* Set different tiles on each layer */
    level->layers[0].tiles[0] = 10;
    level->layers[1].tiles[0] = 20;

    /* Verify isolation */
    DTR_ASSERT_EQ_INT(level->layers[0].tiles[0], 10);
    DTR_ASSERT_EQ_INT(level->layers[1].tiles[0], 20);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Cart free_map                                                      */
/* ------------------------------------------------------------------ */

static void test_map_cart_free_map(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;
    int32_t          lay_idx;

    cart  = prv_make_cart(4, 4, "freeme");
    level = cart->maps[0];

    /* Add extra layers and objects to exercise cleanup */
    prv_add_tile_layer(level, "Extra");
    lay_idx = prv_add_object_layer(level, "Objs");
    prv_add_object(&level->layers[lay_idx], "obj_a", 0.0f, 0.0f, 8.0f, 8.0f);
    prv_add_object(&level->layers[lay_idx], "obj_b", 8.0f, 8.0f, 16.0f, 16.0f);

    /* Free the map — should not crash or leak */
    dtr_cart_free_map(cart, 0);
    DTR_ASSERT(cart->maps[0] == NULL);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

static void test_map_cart_free_map_invalid(void)
{
    dtr_cart_t *cart;

    cart = prv_make_cart(4, 4, "invalid");

    /* Out-of-bounds index should not crash */
    dtr_cart_free_map(cart, -1);
    dtr_cart_free_map(cart, 99);

    /* NULL cart should not crash */
    dtr_cart_free_map(NULL, 0);

    DTR_ASSERT_NOT_NULL(cart->maps[0]);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Multiple maps                                                      */
/* ------------------------------------------------------------------ */

static void test_map_multiple_levels(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level2;
    dtr_map_layer_t *layer;

    cart = prv_make_cart(4, 4, "level_0");

    /* Create a second map level manually */
    level2 = DTR_CALLOC(1, sizeof(dtr_map_level_t));
    DTR_ASSERT_NOT_NULL(level2);
    level2->width       = 8;
    level2->height      = 8;
    level2->tile_w      = 8;
    level2->tile_h      = 8;
    level2->layer_count = 1;
    level2->layers      = DTR_CALLOC(1, sizeof(dtr_map_layer_t));
    DTR_ASSERT_NOT_NULL(level2->layers);

    layer                = &level2->layers[0];
    layer->is_tile_layer = true;
    layer->width         = 8;
    layer->height        = 8;
    layer->tiles         = DTR_CALLOC(64, sizeof(int32_t));
    DTR_ASSERT_NOT_NULL(layer->tiles);
    SDL_strlcpy(layer->name, "Layer 1", sizeof(layer->name));
    SDL_strlcpy(level2->name, "level_1", sizeof(level2->name));

    cart->maps[1]   = level2;
    cart->map_count = 2;

    DTR_ASSERT_EQ_INT(cart->map_count, 2);
    DTR_ASSERT_EQ_STR(cart->maps[0]->name, "level_0");
    DTR_ASSERT_EQ_STR(cart->maps[1]->name, "level_1");

    /* Verify independent dimensions */
    DTR_ASSERT_EQ_INT(cart->maps[0]->width, 4);
    DTR_ASSERT_EQ_INT(cart->maps[1]->width, 8);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Layer type checks                                                  */
/* ------------------------------------------------------------------ */

static void test_map_layer_type(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(4, 4, "types");
    level = cart->maps[0];
    prv_add_object_layer(level, "Objects");

    DTR_ASSERT(level->layers[0].is_tile_layer);
    DTR_ASSERT(!level->layers[1].is_tile_layer);

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Remove middle layer shifts correctly                               */
/* ------------------------------------------------------------------ */

static void test_map_remove_middle_layer(void)
{
    dtr_cart_t      *cart;
    dtr_map_level_t *level;

    cart  = prv_make_cart(4, 4, "midremove");
    level = cart->maps[0];
    prv_add_tile_layer(level, "B");
    prv_add_tile_layer(level, "C");
    DTR_ASSERT_EQ_INT(level->layer_count, 3);

    /* Remove middle layer "B" (index 1) */
    DTR_ASSERT(prv_remove_layer(level, 1));
    DTR_ASSERT_EQ_INT(level->layer_count, 2);
    DTR_ASSERT_EQ_STR(level->layers[0].name, "Layer 1");
    DTR_ASSERT_EQ_STR(level->layers[1].name, "C");

    dtr_cart_destroy(cart);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                        */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("map");

    /* Creation */
    DTR_RUN_TEST(test_map_create_basic);
    DTR_RUN_TEST(test_map_create_bounds);

    /* Tile access */
    DTR_RUN_TEST(test_map_tile_set_get);
    DTR_RUN_TEST(test_map_tile_all_cells);

    /* Layers */
    DTR_RUN_TEST(test_map_add_layer);
    DTR_RUN_TEST(test_map_add_layer_max);
    DTR_RUN_TEST(test_map_remove_layer);
    DTR_RUN_TEST(test_map_remove_last_layer_fails);
    DTR_RUN_TEST(test_map_remove_layer_out_of_bounds);
    DTR_RUN_TEST(test_map_remove_middle_layer);
    DTR_RUN_TEST(test_map_layer_tile_isolation);
    DTR_RUN_TEST(test_map_layer_type);

    /* Rename */
    DTR_RUN_TEST(test_map_rename_layer);
    DTR_RUN_TEST(test_map_rename_layer_truncation);

    /* Resize */
    DTR_RUN_TEST(test_map_resize_grow);
    DTR_RUN_TEST(test_map_resize_shrink);

    /* Object layers */
    DTR_RUN_TEST(test_map_add_object_layer);
    DTR_RUN_TEST(test_map_add_object);
    DTR_RUN_TEST(test_map_add_multiple_objects);
    DTR_RUN_TEST(test_map_remove_object);
    DTR_RUN_TEST(test_map_remove_object_out_of_bounds);
    DTR_RUN_TEST(test_map_set_object);

    /* Cleanup */
    DTR_RUN_TEST(test_map_cart_free_map);
    DTR_RUN_TEST(test_map_cart_free_map_invalid);

    /* Multiple levels */
    DTR_RUN_TEST(test_map_multiple_levels);

    DTR_TEST_END();
}
