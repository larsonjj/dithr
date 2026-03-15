/**
 * \file            map_api.c
 * \brief           JS map namespace — tilemap queries and drawing
 */

#include "../cart.h"
#include "../graphics.h"
#include "api_common.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static mvn_map_level_t *prv_get_level(JSContext *ctx, int argc, JSValueConst *argv, int slot_idx)
{
    mvn_console_t *con;
    int32_t        slot;

    con  = mvn_api_get_console(ctx);
    slot = mvn_api_opt_int(ctx, argc, argv, slot_idx, 0);

    if (slot < 0 || slot >= con->cart->map_count) {
        return NULL;
    }
    return con->cart->maps[slot];
}
/* ------------------------------------------------------------------ */
/*  map.mget(x, y, layer?, slot?)                                      */
/* ------------------------------------------------------------------ */

static JSValue js_map_mget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_map_level_t *level;
    int32_t          x;
    int32_t          y;
    int32_t          layer_idx;

    (void)this_val;

    level = prv_get_level(ctx, argc, argv, 3);
    if (level == NULL) {
        return JS_NewInt32(ctx, 0);
    }

    x         = mvn_api_opt_int(ctx, argc, argv, 0, 0);
    y         = mvn_api_opt_int(ctx, argc, argv, 1, 0);
    layer_idx = mvn_api_opt_int(ctx, argc, argv, 2, 0);

    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_NewInt32(ctx, 0);
    }

    {
        mvn_map_layer_t *layer;

        layer = &level->layers[layer_idx];
        if (!layer->is_tile_layer || layer->tiles == NULL) {
            return JS_NewInt32(ctx, 0);
        }
        if (x < 0 || x >= layer->width || y < 0 || y >= layer->height) {
            return JS_NewInt32(ctx, 0);
        }
        return JS_NewInt32(ctx, layer->tiles[y * layer->width + x]);
    }
}

/* ------------------------------------------------------------------ */
/*  map.mset(x, y, tile, layer?, slot?)                                */
/* ------------------------------------------------------------------ */

static JSValue js_map_mset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_map_level_t *level;
    int32_t          x;
    int32_t          y;
    int32_t          tile;
    int32_t          layer_idx;

    (void)this_val;

    level = prv_get_level(ctx, argc, argv, 4);
    if (level == NULL) {
        return JS_UNDEFINED;
    }

    x         = mvn_api_opt_int(ctx, argc, argv, 0, 0);
    y         = mvn_api_opt_int(ctx, argc, argv, 1, 0);
    tile      = mvn_api_opt_int(ctx, argc, argv, 2, 0);
    layer_idx = mvn_api_opt_int(ctx, argc, argv, 3, 0);

    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_UNDEFINED;
    }

    {
        mvn_map_layer_t *layer;

        layer = &level->layers[layer_idx];
        if (!layer->is_tile_layer || layer->tiles == NULL) {
            return JS_UNDEFINED;
        }
        if (x >= 0 && x < layer->width && y >= 0 && y < layer->height) {
            layer->tiles[y * layer->width + x] = tile;
        }
    }

    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  map.draw(sx, sy, dx, dy, tw, th, layer?, slot?)                    */
/* ------------------------------------------------------------------ */

static JSValue js_map_draw(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_console_t *  con;
    mvn_map_level_t *level;
    int32_t          sx;
    int32_t          sy;
    int32_t          dx;
    int32_t          dy;
    int32_t          tw;
    int32_t          th;
    int32_t          layer_idx;

    (void)this_val;

    con   = mvn_api_get_console(ctx);
    level = prv_get_level(ctx, argc, argv, 7);
    if (level == NULL) {
        return JS_UNDEFINED;
    }

    sx        = mvn_api_opt_int(ctx, argc, argv, 0, 0);
    sy        = mvn_api_opt_int(ctx, argc, argv, 1, 0);
    dx        = mvn_api_opt_int(ctx, argc, argv, 2, 0);
    dy        = mvn_api_opt_int(ctx, argc, argv, 3, 0);
    tw        = mvn_api_opt_int(ctx, argc, argv, 4, level->width);
    th        = mvn_api_opt_int(ctx, argc, argv, 5, level->height);
    layer_idx = mvn_api_opt_int(ctx, argc, argv, 6, 0);

    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_UNDEFINED;
    }

    {
        mvn_map_layer_t *layer;

        layer = &level->layers[layer_idx];
        if (!layer->is_tile_layer || layer->tiles == NULL) {
            return JS_UNDEFINED;
        }

        for (int32_t ty = 0; ty < th; ++ty) {
            for (int32_t tx = 0; tx < tw; ++tx) {
                int32_t mx;
                int32_t my;
                int32_t tile;

                mx = sx + tx;
                my = sy + ty;
                if (mx < 0 || mx >= layer->width || my < 0 || my >= layer->height) {
                    continue;
                }

                tile = layer->tiles[my * layer->width + mx];
                if (tile > 0) {
                    mvn_gfx_spr(con->graphics,
                                tile - 1,
                                dx + tx * con->cart->sprites.tile_w,
                                dy + ty * con->cart->sprites.tile_h,
                                1,
                                1,
                                false,
                                false);
                }
            }
        }
    }

    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  map.width(layer?, slot?) / map.height(layer?, slot?)               */
/* ------------------------------------------------------------------ */

static JSValue js_map_width(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_map_level_t *level;
    int32_t          layer_idx;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewInt32(ctx, 0);
    }

    layer_idx = mvn_api_opt_int(ctx, argc, argv, 0, 0);
    if (layer_idx >= 0 && layer_idx < level->layer_count) {
        return JS_NewInt32(ctx, level->layers[layer_idx].width);
    }
    return JS_NewInt32(ctx, level->width);
}

static JSValue js_map_height(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_map_level_t *level;
    int32_t          layer_idx;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewInt32(ctx, 0);
    }

    layer_idx = mvn_api_opt_int(ctx, argc, argv, 0, 0);
    if (layer_idx >= 0 && layer_idx < level->layer_count) {
        return JS_NewInt32(ctx, level->layers[layer_idx].height);
    }
    return JS_NewInt32(ctx, level->height);
}

/* ------------------------------------------------------------------ */
/*  map.objects(layer?, slot?)                                         */
/* ------------------------------------------------------------------ */

static JSValue js_map_objects(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_map_level_t *level;
    int32_t          layer_idx;
    JSValue          arr;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewArray(ctx);
    }

    layer_idx = mvn_api_opt_int(ctx, argc, argv, 0, 0);
    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_NewArray(ctx);
    }

    {
        mvn_map_layer_t *layer;

        layer = &level->layers[layer_idx];
        arr   = JS_NewArray(ctx);

        for (int32_t idx = 0; idx < layer->object_count; ++idx) {
            mvn_map_object_t *obj;
            JSValue           jobj;

            obj  = &layer->objects[idx];
            jobj = JS_NewObject(ctx);

            JS_SetPropertyStr(ctx, jobj, "name", JS_NewString(ctx, obj->name));
            JS_SetPropertyStr(ctx, jobj, "type", JS_NewString(ctx, obj->type));
            JS_SetPropertyStr(ctx, jobj, "x", JS_NewFloat64(ctx, obj->x));
            JS_SetPropertyStr(ctx, jobj, "y", JS_NewFloat64(ctx, obj->y));
            JS_SetPropertyStr(ctx, jobj, "w", JS_NewFloat64(ctx, obj->w));
            JS_SetPropertyStr(ctx, jobj, "h", JS_NewFloat64(ctx, obj->h));
            JS_SetPropertyStr(ctx, jobj, "gid", JS_NewInt32(ctx, obj->gid));

            JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, jobj);
        }
    }

    return arr;
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_map_funcs[] = {
    JS_CFUNC_DEF("mget", 4, js_map_mget),
    JS_CFUNC_DEF("mset", 5, js_map_mset),
    JS_CFUNC_DEF("draw", 8, js_map_draw),
    JS_CFUNC_DEF("width", 2, js_map_width),
    JS_CFUNC_DEF("height", 2, js_map_height),
    JS_CFUNC_DEF("objects", 2, js_map_objects),
};

void mvn_map_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_map_funcs, countof(js_map_funcs));
    JS_SetPropertyStr(ctx, global, "map", ns);
}
