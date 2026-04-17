/**
 * \file            test_api_bridge_map.c
 * \brief           Unit tests for the JS map API bridge (26 endpoints)
 *
 * Needs a graphics context for map.draw / map.flag, so follows the
 * same pattern as test_api_bridge_gfx.c.
 */

#include "api/api_common.h"
#include "cart.h"
#include "graphics.h"
#include "postfx.h"
#include "runtime.h"
#include "test_harness.h"

#include <string.h>

#define TW 32
#define TH 32

/* ================================================================== */
/*  Test context                                                       */
/* ================================================================== */

typedef struct test_ctx {
    dtr_console_t  con;
    dtr_runtime_t *rt;
} test_ctx_t;

static test_ctx_t *prv_setup(void)
{
    test_ctx_t *tc;

    tc = DTR_CALLOC(1, sizeof(test_ctx_t));
    DTR_ASSERT(tc != NULL);

    tc->con.fb_width  = TW;
    tc->con.fb_height = TH;

    tc->con.graphics = dtr_gfx_create(TW, TH);
    tc->con.postfx   = dtr_postfx_create(TW, TH);
    tc->con.cart     = dtr_cart_create();

    DTR_ASSERT(tc->con.graphics != NULL);
    DTR_ASSERT(tc->con.postfx != NULL);
    DTR_ASSERT(tc->con.cart != NULL);

    tc->rt = dtr_runtime_create(&tc->con, 8, 256);
    DTR_ASSERT(tc->rt != NULL);

    return tc;
}

/**
 * Setup with a pre-populated 4x4 tile layer so most getters work
 * without calling map.create first.
 */
static test_ctx_t *prv_setup_with_map(void)
{
    test_ctx_t      *tc;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;

    tc = prv_setup();

    level = DTR_CALLOC(1, sizeof(dtr_map_level_t));
    DTR_ASSERT(level != NULL);

    level->width       = 4;
    level->height      = 4;
    level->tile_w      = 8;
    level->tile_h      = 8;
    level->layer_count = 1;
    SDL_strlcpy(level->name, "testmap", sizeof(level->name));

    level->layers = DTR_CALLOC(1, sizeof(dtr_map_layer_t));
    DTR_ASSERT(level->layers != NULL);

    layer                = &level->layers[0];
    layer->is_tile_layer = true;
    layer->width         = 4;
    layer->height        = 4;
    SDL_strlcpy(layer->name, "ground", sizeof(layer->name));

    layer->tiles = DTR_CALLOC(16, sizeof(int32_t));
    DTR_ASSERT(layer->tiles != NULL);

    /* Fill with some tile IDs: row-major, 1-based */
    for (int32_t i = 0; i < 16; ++i) {
        layer->tiles[i] = i + 1;
    }

    tc->con.cart->maps[0]     = level;
    tc->con.cart->map_count   = 1;
    tc->con.cart->current_map = 0;

    return tc;
}

static void prv_teardown(test_ctx_t *tc)
{
    dtr_runtime_destroy(tc->rt);
    dtr_cart_destroy(tc->con.cart);
    dtr_postfx_destroy(tc->con.postfx);
    dtr_gfx_destroy(tc->con.graphics);
    DTR_FREE(tc);
}

static void prv_register(test_ctx_t *tc, void (*fn)(JSContext *, JSValue))
{
    JSValue global;

    global = JS_GetGlobalObject(tc->rt->ctx);
    fn(tc->rt->ctx, global);
    JS_FreeValue(tc->rt->ctx, global);
}

static int32_t prv_eval_i32(test_ctx_t *tc, const char *code)
{
    JSValue val;
    int32_t result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    JS_ToInt32(tc->rt->ctx, &result, val);
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

static bool prv_eval_bool(test_ctx_t *tc, const char *code)
{
    JSValue val;
    bool    result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    result = JS_ToBool(tc->rt->ctx, val) != 0;
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

static void prv_eval_void(test_ctx_t *tc, const char *code)
{
    JSValue val;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    JS_FreeValue(tc->rt->ctx, val);
}

static double prv_eval_f64(test_ctx_t *tc, const char *code)
{
    JSValue val;
    double  result;

    val = JS_Eval(tc->rt->ctx, code, strlen(code), "<test>", JS_EVAL_TYPE_GLOBAL);
    DTR_ASSERT(!JS_IsException(val));
    JS_ToFloat64(tc->rt->ctx, &result, val);
    JS_FreeValue(tc->rt->ctx, val);
    return result;
}

/* ================================================================== */
/*  map.get / map.set                                                  */
/* ================================================================== */

static void test_map_get(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    /* Tile at (0,0) should be 1 (1-based fill) */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(0, 0)") == 1);
    /* Tile at (1,0) should be 2 */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(1, 0)") == 2);
    /* Tile at (0,1) should be 5 (row 1, col 0 = 4*1+0+1) */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(0, 1)") == 5);
    /* Out of bounds returns undefined */
    DTR_ASSERT(prv_eval_bool(tc, "map.get(99, 99) === undefined"));
    /* Negative coords return undefined */
    DTR_ASSERT(prv_eval_bool(tc, "map.get(-1, 0) === undefined"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_map_set(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    prv_eval_void(tc, "map.set(0, 0, 42)");
    DTR_ASSERT(prv_eval_i32(tc, "map.get(0, 0)") == 42);

    /* Out of bounds set is a no-op */
    prv_eval_void(tc, "map.set(99, 99, 7)");
    DTR_ASSERT(prv_eval_bool(tc, "map.get(99, 99) === undefined"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.width / map.height                                             */
/* ================================================================== */

static void test_map_width_height(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    DTR_ASSERT(prv_eval_i32(tc, "map.width()") == 4);
    DTR_ASSERT(prv_eval_i32(tc, "map.height()") == 4);

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.layers                                                         */
/* ================================================================== */

static void test_map_layers(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    DTR_ASSERT(prv_eval_i32(tc, "map.layers().length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.layers()[0] === 'ground'"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.levels                                                         */
/* ================================================================== */

static void test_map_levels(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    DTR_ASSERT(prv_eval_i32(tc, "map.levels().length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.levels()[0] === 'testmap'"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.currentLevel                                                   */
/* ================================================================== */

static void test_map_current_level(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    DTR_ASSERT(prv_eval_bool(tc, "map.currentLevel() === 'testmap'"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.load                                                           */
/* ================================================================== */

static void test_map_load(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    /* Loading existing map returns true */
    DTR_ASSERT(prv_eval_bool(tc, "map.load('testmap')"));

    /* Loading non-existent map returns false */
    DTR_ASSERT(!prv_eval_bool(tc, "map.load('nosuchmap')"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.create                                                         */
/* ================================================================== */

static void test_map_create(void)
{
    test_ctx_t *tc;

    tc = prv_setup();
    prv_register(tc, dtr_map_api_register);

    /* Create a 8x6 map */
    DTR_ASSERT(prv_eval_bool(tc, "map.create(8, 6, 'newmap')"));
    DTR_ASSERT(prv_eval_i32(tc, "map.width()") == 8);
    DTR_ASSERT(prv_eval_i32(tc, "map.height()") == 6);
    DTR_ASSERT(prv_eval_bool(tc, "map.currentLevel() === 'newmap'"));

    /* All tiles should start at 0 */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(0, 0)") == 0);
    DTR_ASSERT(prv_eval_i32(tc, "map.get(7, 5)") == 0);

    /* Invalid dimensions should return false */
    DTR_ASSERT(!prv_eval_bool(tc, "map.create(0, 0)"));
    DTR_ASSERT(!prv_eval_bool(tc, "map.create(-1, 5)"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.resize                                                         */
/* ================================================================== */

static void test_map_resize(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    /* Set a known tile before resize */
    prv_eval_void(tc, "map.set(1, 1, 99)");

    /* Grow to 8x8 */
    DTR_ASSERT(prv_eval_bool(tc, "map.resize(8, 8)"));
    DTR_ASSERT(prv_eval_i32(tc, "map.width()") == 8);
    DTR_ASSERT(prv_eval_i32(tc, "map.height()") == 8);

    /* Original tile preserved */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(1, 1)") == 99);

    /* New area is 0 */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(5, 5)") == 0);

    /* Shrink to 2x2 */
    DTR_ASSERT(prv_eval_bool(tc, "map.resize(2, 2)"));
    DTR_ASSERT(prv_eval_i32(tc, "map.width()") == 2);
    DTR_ASSERT(prv_eval_i32(tc, "map.height()") == 2);
    DTR_ASSERT(prv_eval_i32(tc, "map.get(1, 1)") == 99);

    /* Same-size is a no-op, returns true */
    DTR_ASSERT(prv_eval_bool(tc, "map.resize(2, 2)"));

    /* Invalid dimensions */
    DTR_ASSERT(!prv_eval_bool(tc, "map.resize(0, 0)"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.addLayer / map.removeLayer / map.renameLayer                   */
/* ================================================================== */

static void test_map_add_layer(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    /* Start with 1 layer */
    DTR_ASSERT(prv_eval_i32(tc, "map.layers().length") == 1);

    /* Add a named layer */
    DTR_ASSERT(prv_eval_i32(tc, "map.addLayer('overlay')") == 1);
    DTR_ASSERT(prv_eval_i32(tc, "map.layers().length") == 2);
    DTR_ASSERT(prv_eval_bool(tc, "map.layers()[1] === 'overlay'"));

    /* New layer tiles default to 0 */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(0, 0, 1)") == 0);

    /* Set on new layer */
    prv_eval_void(tc, "map.set(0, 0, 77, 1)");
    DTR_ASSERT(prv_eval_i32(tc, "map.get(0, 0, 1)") == 77);

    /* Original layer untouched */
    DTR_ASSERT(prv_eval_i32(tc, "map.get(0, 0, 0)") == 1);

    prv_teardown(tc);
    DTR_PASS();
}

static void test_map_remove_layer(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    /* Add a second layer so we can remove one */
    prv_eval_void(tc, "map.addLayer('temp')");
    DTR_ASSERT(prv_eval_i32(tc, "map.layers().length") == 2);

    /* Remove it */
    DTR_ASSERT(prv_eval_bool(tc, "map.removeLayer(1)"));
    DTR_ASSERT(prv_eval_i32(tc, "map.layers().length") == 1);

    /* Cannot remove the last layer */
    DTR_ASSERT(!prv_eval_bool(tc, "map.removeLayer(0)"));

    /* Cannot remove invalid index */
    DTR_ASSERT(!prv_eval_bool(tc, "map.removeLayer(99)"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_map_rename_layer(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    DTR_ASSERT(prv_eval_bool(tc, "map.renameLayer(0, 'floor')"));
    DTR_ASSERT(prv_eval_bool(tc, "map.layers()[0] === 'floor'"));

    /* Invalid index */
    DTR_ASSERT(!prv_eval_bool(tc, "map.renameLayer(99, 'x')"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.layerType                                                      */
/* ================================================================== */

static void test_map_layer_type(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    DTR_ASSERT(prv_eval_bool(tc, "map.layerType(0) === 'tilelayer'"));

    /* Invalid index returns empty string */
    DTR_ASSERT(prv_eval_bool(tc, "map.layerType(99) === ''"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.addObjectLayer / map.layerObjects                              */
/* ================================================================== */

static void test_map_add_object_layer(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    int32_t idx = prv_eval_i32(tc, "map.addObjectLayer('entities')");
    DTR_ASSERT(idx == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.layerType(1) === 'objectgroup'"));

    /* layerObjects returns empty array for new object layer */
    DTR_ASSERT(prv_eval_i32(tc, "map.layerObjects(1).length") == 0);

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.addObject / map.removeObject / map.setObject                   */
/* ================================================================== */

static void test_map_add_object(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    /* Add an object layer first */
    prv_eval_void(tc, "map.addObjectLayer('objs')");

    /* Add an object */
    int32_t oidx =
        prv_eval_i32(tc, "map.addObject(1, {name:'spawn', type:'player', x:10, y:20, w:16, h:16})");
    DTR_ASSERT(oidx == 0);

    /* Verify via layerObjects */
    DTR_ASSERT(prv_eval_i32(tc, "map.layerObjects(1).length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.layerObjects(1)[0].name === 'spawn'"));
    DTR_ASSERT(prv_eval_bool(tc, "map.layerObjects(1)[0].type === 'player'"));
    DTR_ASSERT(prv_eval_f64(tc, "map.layerObjects(1)[0].x") == 10.0);
    DTR_ASSERT(prv_eval_f64(tc, "map.layerObjects(1)[0].y") == 20.0);

    prv_teardown(tc);
    DTR_PASS();
}

static void test_map_remove_object(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    prv_eval_void(tc, "map.addObjectLayer('objs')");
    prv_eval_void(tc, "map.addObject(1, {name:'a', type:'npc', x:0, y:0, w:8, h:8})");
    prv_eval_void(tc, "map.addObject(1, {name:'b', type:'npc', x:8, y:0, w:8, h:8})");
    DTR_ASSERT(prv_eval_i32(tc, "map.layerObjects(1).length") == 2);

    /* Remove first object */
    DTR_ASSERT(prv_eval_bool(tc, "map.removeObject(1, 0)"));
    DTR_ASSERT(prv_eval_i32(tc, "map.layerObjects(1).length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.layerObjects(1)[0].name === 'b'"));

    /* Invalid indices */
    DTR_ASSERT(!prv_eval_bool(tc, "map.removeObject(1, 99)"));
    DTR_ASSERT(!prv_eval_bool(tc, "map.removeObject(99, 0)"));

    prv_teardown(tc);
    DTR_PASS();
}

static void test_map_set_object(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    prv_eval_void(tc, "map.addObjectLayer('objs')");
    prv_eval_void(tc, "map.addObject(1, {name:'e', type:'enemy', x:0, y:0, w:8, h:8})");

    /* Update position and name */
    DTR_ASSERT(prv_eval_bool(tc, "map.setObject(1, 0, {name:'moved', x:50, y:60})"));
    DTR_ASSERT(prv_eval_bool(tc, "map.layerObjects(1)[0].name === 'moved'"));
    DTR_ASSERT(prv_eval_f64(tc, "map.layerObjects(1)[0].x") == 50.0);
    DTR_ASSERT(prv_eval_f64(tc, "map.layerObjects(1)[0].y") == 60.0);

    /* Type unchanged */
    DTR_ASSERT(prv_eval_bool(tc, "map.layerObjects(1)[0].type === 'enemy'"));

    /* Invalid indices */
    DTR_ASSERT(!prv_eval_bool(tc, "map.setObject(1, 99, {x:0})"));
    DTR_ASSERT(!prv_eval_bool(tc, "map.setObject(99, 0, {x:0})"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.objects / map.object                                           */
/* ================================================================== */

static void test_map_objects(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    prv_eval_void(tc, "map.addObjectLayer('objs')");
    prv_eval_void(tc, "map.addObject(1, {name:'a', type:'npc', x:0, y:0, w:8, h:8})");
    prv_eval_void(tc, "map.addObject(1, {name:'b', type:'coin', x:16, y:0, w:8, h:8})");

    /* All objects */
    DTR_ASSERT(prv_eval_i32(tc, "map.objects().length") == 2);

    /* Filter by name */
    DTR_ASSERT(prv_eval_i32(tc, "map.objects('a').length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.objects('a')[0].type === 'npc'"));

    /* No match */
    DTR_ASSERT(prv_eval_i32(tc, "map.objects('z').length") == 0);

    prv_teardown(tc);
    DTR_PASS();
}

static void test_map_object(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    prv_eval_void(tc, "map.addObjectLayer('objs')");
    prv_eval_void(tc, "map.addObject(1, {name:'hero', type:'player', x:5, y:10, w:16, h:16})");

    /* Find by name */
    DTR_ASSERT(prv_eval_bool(tc, "map.object('hero') !== undefined"));
    DTR_ASSERT(prv_eval_bool(tc, "map.object('hero').type === 'player'"));
    DTR_ASSERT(prv_eval_f64(tc, "map.object('hero').x") == 5.0);

    /* Not found */
    DTR_ASSERT(prv_eval_bool(tc, "map.object('nobody') === undefined"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.objectsIn                                                      */
/* ================================================================== */

static void test_map_objects_in(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    prv_eval_void(tc, "map.addObjectLayer('objs')");
    prv_eval_void(tc, "map.addObject(1, {name:'a', type:'t', x:10, y:10, w:8, h:8})");
    prv_eval_void(tc, "map.addObject(1, {name:'b', type:'t', x:100, y:100, w:8, h:8})");

    /* Query region that overlaps object 'a' only */
    DTR_ASSERT(prv_eval_i32(tc, "map.objectsIn(0, 0, 20, 20).length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.objectsIn(0, 0, 20, 20)[0].name === 'a'"));

    /* Query region that overlaps nothing */
    DTR_ASSERT(prv_eval_i32(tc, "map.objectsIn(200, 200, 10, 10).length") == 0);

    /* Query region that overlaps both */
    DTR_ASSERT(prv_eval_i32(tc, "map.objectsIn(0, 0, 200, 200).length") == 2);

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.objectsWith                                                    */
/* ================================================================== */

static void test_map_objects_with(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    prv_eval_void(tc, "map.addObjectLayer('objs')");
    prv_eval_void(tc, "map.addObject(1, {name:'a', type:'enemy', x:0, y:0, w:8, h:8})");
    prv_eval_void(tc, "map.addObject(1, {name:'b', type:'coin', x:8, y:0, w:8, h:8})");

    /* Filter by type (type field match) */
    DTR_ASSERT(prv_eval_i32(tc, "map.objectsWith('enemy').length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.objectsWith('enemy')[0].name === 'a'"));

    /* No match */
    DTR_ASSERT(prv_eval_i32(tc, "map.objectsWith('boss').length") == 0);

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.flag                                                           */
/* ================================================================== */

static void test_map_flag(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);
    prv_register(tc, dtr_gfx_api_register);

    /* Set flag bit 0 on sprite 0 (tile ID 1 maps to sprite index 0) */
    prv_eval_void(tc, "gfx.fset(0, 0, true)");

    /* Tile at (0,0) is 1, so flag(0,0, 0) checks sprite 0, bit 0 */
    DTR_ASSERT(prv_eval_bool(tc, "map.flag(0, 0, 0)"));

    /* Flag bit 1 not set */
    DTR_ASSERT(!prv_eval_bool(tc, "map.flag(0, 0, 1)"));

    /* Out of bounds returns false */
    DTR_ASSERT(!prv_eval_bool(tc, "map.flag(99, 99, 0)"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.draw (no crash)                                                */
/* ================================================================== */

static void test_map_draw_no_crash(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    /* Just verify it doesn't crash with valid and edge-case args */
    prv_eval_void(tc, "map.draw(0, 0, 0, 0, 4, 4)");
    prv_eval_void(tc, "map.draw(0, 0, 0, 0)");

    /* Invalid layer */
    prv_eval_void(tc, "map.draw(0, 0, 0, 0, 4, 4, 99)");

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.data                                                           */
/* ================================================================== */

static void test_map_data(void)
{
    test_ctx_t *tc;

    tc = prv_setup_with_map();
    prv_register(tc, dtr_map_api_register);

    DTR_ASSERT(prv_eval_bool(tc, "map.data() !== null"));
    DTR_ASSERT(prv_eval_bool(tc, "map.data().name === 'testmap'"));
    DTR_ASSERT(prv_eval_i32(tc, "map.data().width") == 4);
    DTR_ASSERT(prv_eval_i32(tc, "map.data().height") == 4);
    DTR_ASSERT(prv_eval_i32(tc, "map.data().layers.length") == 1);
    DTR_ASSERT(prv_eval_bool(tc, "map.data().layers[0].type === 'tilelayer'"));
    DTR_ASSERT(prv_eval_i32(tc, "map.data().layers[0].data.length") == 16);

    /* First tile is 1 */
    DTR_ASSERT(prv_eval_i32(tc, "map.data().layers[0].data[0]") == 1);

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  map.get / map.set with no map loaded                               */
/* ================================================================== */

static void test_map_no_map_fallback(void)
{
    test_ctx_t *tc;

    tc = prv_setup(); /* No map created */
    prv_register(tc, dtr_map_api_register);

    /* All should return undefined / 0 / empty / false without crashing */
    DTR_ASSERT(prv_eval_bool(tc, "map.get(0, 0) === undefined"));
    DTR_ASSERT(prv_eval_i32(tc, "map.width()") == 0);
    DTR_ASSERT(prv_eval_i32(tc, "map.height()") == 0);
    DTR_ASSERT(prv_eval_i32(tc, "map.layers().length") == 0);
    DTR_ASSERT(prv_eval_bool(tc, "map.currentLevel() === ''"));
    DTR_ASSERT(prv_eval_bool(tc, "map.data() === null"));

    prv_teardown(tc);
    DTR_PASS();
}

/* ================================================================== */
/*  Runner                                                             */
/* ================================================================== */

int main(void)
{
    DTR_TEST_BEGIN("test_api_bridge_map");

    /* map.get / map.set — 2 tests */
    DTR_RUN_TEST(test_map_get);
    DTR_RUN_TEST(test_map_set);

    /* map.width / map.height — 1 test */
    DTR_RUN_TEST(test_map_width_height);

    /* map.layers — 1 test */
    DTR_RUN_TEST(test_map_layers);

    /* map.levels — 1 test */
    DTR_RUN_TEST(test_map_levels);

    /* map.currentLevel — 1 test */
    DTR_RUN_TEST(test_map_current_level);

    /* map.load — 1 test */
    DTR_RUN_TEST(test_map_load);

    /* map.create — 1 test */
    DTR_RUN_TEST(test_map_create);

    /* map.resize — 1 test */
    DTR_RUN_TEST(test_map_resize);

    /* map.addLayer / removeLayer / renameLayer — 3 tests */
    DTR_RUN_TEST(test_map_add_layer);
    DTR_RUN_TEST(test_map_remove_layer);
    DTR_RUN_TEST(test_map_rename_layer);

    /* map.layerType — 1 test */
    DTR_RUN_TEST(test_map_layer_type);

    /* map.addObjectLayer / layerObjects — 1 test */
    DTR_RUN_TEST(test_map_add_object_layer);

    /* map.addObject / removeObject / setObject — 3 tests */
    DTR_RUN_TEST(test_map_add_object);
    DTR_RUN_TEST(test_map_remove_object);
    DTR_RUN_TEST(test_map_set_object);

    /* map.objects / map.object — 2 tests */
    DTR_RUN_TEST(test_map_objects);
    DTR_RUN_TEST(test_map_object);

    /* map.objectsIn — 1 test */
    DTR_RUN_TEST(test_map_objects_in);

    /* map.objectsWith — 1 test */
    DTR_RUN_TEST(test_map_objects_with);

    /* map.flag — 1 test */
    DTR_RUN_TEST(test_map_flag);

    /* map.draw — 1 test */
    DTR_RUN_TEST(test_map_draw_no_crash);

    /* map.data — 1 test */
    DTR_RUN_TEST(test_map_data);

    /* Null-map fallback — 1 test */
    DTR_RUN_TEST(test_map_no_map_fallback);

    DTR_TEST_END();
}
