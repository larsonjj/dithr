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

static dtr_map_level_t *prv_get_level(JSContext *ctx, int argc, JSValueConst *argv, int slot_idx)
{
    dtr_console_t *con;
    int32_t        slot;

    con  = dtr_api_get_console(ctx);
    slot = dtr_api_opt_int(ctx, argc, argv, slot_idx, con->cart->current_map);

    if (slot < 0 || slot >= con->cart->map_count) {
        return NULL;
    }
    return con->cart->maps[slot];
}

/* ------------------------------------------------------------------ */
/*  map.get(cx, cy, layer?)                                            */
/* ------------------------------------------------------------------ */

static JSValue js_map_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    int32_t          x;
    int32_t          y;
    int32_t          layer_idx;

    (void)this_val;

    level = prv_get_level(ctx, argc, argv, 3);
    if (level == NULL) {
        return JS_NewInt32(ctx, 0);
    }

    x         = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    y         = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    layer_idx = dtr_api_opt_int(ctx, argc, argv, 2, 0);

    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_NewInt32(ctx, 0);
    }

    {
        dtr_map_layer_t *layer;

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
/*  map.set(cx, cy, tile, layer?)                                      */
/* ------------------------------------------------------------------ */

static JSValue js_map_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    int32_t          x;
    int32_t          y;
    int32_t          tile;
    int32_t          layer_idx;

    (void)this_val;

    level = prv_get_level(ctx, argc, argv, 4);
    if (level == NULL) {
        return JS_UNDEFINED;
    }

    x         = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    y         = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    tile      = dtr_api_opt_int(ctx, argc, argv, 2, 0);
    layer_idx = dtr_api_opt_int(ctx, argc, argv, 3, 0);

    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_UNDEFINED;
    }

    {
        dtr_map_layer_t *layer;

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
/*  map.flag(cx, cy, f)                                                */
/* ------------------------------------------------------------------ */

static JSValue js_map_flag(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    dtr_map_level_t *level;
    int32_t          cx;
    int32_t          cy;
    int32_t          f;
    int32_t          tile;

    (void)this_val;
    con   = dtr_api_get_console(ctx);
    level = prv_get_level(ctx, argc, argv, 3);

    if (level == NULL || level->layer_count == 0) {
        return JS_FALSE;
    }

    cx = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    cy = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    f  = dtr_api_opt_int(ctx, argc, argv, 2, 0);

    {
        dtr_map_layer_t *layer;

        layer = &level->layers[0];
        if (!layer->is_tile_layer || layer->tiles == NULL) {
            return JS_FALSE;
        }
        if (cx < 0 || cx >= layer->width || cy < 0 || cy >= layer->height) {
            return JS_FALSE;
        }
        tile = layer->tiles[cy * layer->width + cx];
    }

    if (tile <= 0 || tile > CONSOLE_MAX_SPRITES) {
        return JS_FALSE;
    }

    return JS_NewBool(ctx, dtr_gfx_fget_bit(con->graphics, tile - 1, f));
}

/* ------------------------------------------------------------------ */
/*  map.draw(cx, cy, sx, sy, cw, ch, opts)                            */
/* ------------------------------------------------------------------ */

static JSValue js_map_draw(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    dtr_map_level_t *level;
    int32_t          sx;
    int32_t          sy;
    int32_t          dx;
    int32_t          dy;
    int32_t          tw;
    int32_t          th;
    int32_t          layer_idx;

    (void)this_val;

    con   = dtr_api_get_console(ctx);
    level = prv_get_level(ctx, argc, argv, 7);
    if (level == NULL) {
        return JS_UNDEFINED;
    }

    sx        = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    sy        = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    dx        = dtr_api_opt_int(ctx, argc, argv, 2, 0);
    dy        = dtr_api_opt_int(ctx, argc, argv, 3, 0);
    tw        = dtr_api_opt_int(ctx, argc, argv, 4, level->width);
    th        = dtr_api_opt_int(ctx, argc, argv, 5, level->height);
    layer_idx = dtr_api_opt_int(ctx, argc, argv, 6, 0);

    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_UNDEFINED;
    }

    {
        dtr_map_layer_t *layer;

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
                    dtr_gfx_spr(con->graphics,
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
/*  map.width() / map.height()                                         */
/* ------------------------------------------------------------------ */

static JSValue js_map_width(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 0);
    if (level == NULL) {
        return JS_NewInt32(ctx, 0);
    }
    return JS_NewInt32(ctx, level->width);
}

static JSValue js_map_height(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 0);
    if (level == NULL) {
        return JS_NewInt32(ctx, 0);
    }
    return JS_NewInt32(ctx, level->height);
}

/* ------------------------------------------------------------------ */
/*  map.layers()                                                       */
/* ------------------------------------------------------------------ */

static JSValue js_map_layers(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    JSValue          arr;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 0);
    if (level == NULL) {
        return JS_NewArray(ctx);
    }

    arr = JS_NewArray(ctx);
    for (int32_t idx = 0; idx < level->layer_count; ++idx) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, JS_NewString(ctx, level->layers[idx].name));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  map.levels()                                                       */
/* ------------------------------------------------------------------ */

static JSValue js_map_levels(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    JSValue        arr;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    arr = JS_NewArray(ctx);

    for (int32_t idx = 0; idx < con->cart->map_count; ++idx) {
        if (con->cart->maps[idx] != NULL) {
            JS_SetPropertyUint32(
                ctx, arr, (uint32_t)idx, JS_NewString(ctx, con->cart->maps[idx]->name));
        }
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  map.current_level()                                                */
/* ------------------------------------------------------------------ */

static JSValue
js_map_current_level(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    dtr_map_level_t *level;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);

    if (con->cart->current_map >= 0 && con->cart->current_map < con->cart->map_count) {
        level = con->cart->maps[con->cart->current_map];
        if (level != NULL) {
            return JS_NewString(ctx, level->name);
        }
    }
    return JS_NewString(ctx, "");
}

/* ------------------------------------------------------------------ */
/*  map.load(name)                                                     */
/* ------------------------------------------------------------------ */

static JSValue js_map_load(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    const char    *name;

    (void)this_val;
    if (argc < 1) {
        return JS_FALSE;
    }

    con  = dtr_api_get_console(ctx);
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_FALSE;
    }

    for (int32_t idx = 0; idx < con->cart->map_count; ++idx) {
        if (con->cart->maps[idx] != NULL && SDL_strcmp(con->cart->maps[idx]->name, name) == 0) {
            con->cart->current_map = idx;
            JS_FreeCString(ctx, name);
            return JS_TRUE;
        }
    }
    JS_FreeCString(ctx, name);
    return JS_FALSE;
}

/* ------------------------------------------------------------------ */
/*  map.objects(name?)                                                 */
/* ------------------------------------------------------------------ */

static JSValue prv_obj_to_js(JSContext *ctx, dtr_map_object_t *obj)
{
    JSValue jobj;

    jobj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, jobj, "name", JS_NewString(ctx, obj->name));
    JS_SetPropertyStr(ctx, jobj, "type", JS_NewString(ctx, obj->type));
    JS_SetPropertyStr(ctx, jobj, "x", JS_NewFloat64(ctx, obj->x));
    JS_SetPropertyStr(ctx, jobj, "y", JS_NewFloat64(ctx, obj->y));
    JS_SetPropertyStr(ctx, jobj, "w", JS_NewFloat64(ctx, obj->w));
    JS_SetPropertyStr(ctx, jobj, "h", JS_NewFloat64(ctx, obj->h));
    JS_SetPropertyStr(ctx, jobj, "gid", JS_NewInt32(ctx, obj->gid));

    if (!JS_IsUndefined(obj->props) && !JS_IsNull(obj->props)) {
        JS_SetPropertyStr(ctx, jobj, "props", JS_DupValue(ctx, obj->props));
    } else {
        JS_SetPropertyStr(ctx, jobj, "props", JS_NewObject(ctx));
    }

    return jobj;
}

static JSValue js_map_objects(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    const char      *filter_name;
    JSValue          arr;
    uint32_t         out_idx;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewArray(ctx);
    }

    filter_name = NULL;
    if (argc >= 1 && JS_IsString(argv[0])) {
        filter_name = JS_ToCString(ctx, argv[0]);
    }

    arr     = JS_NewArray(ctx);
    out_idx = 0;

    for (int32_t li = 0; li < level->layer_count; ++li) {
        dtr_map_layer_t *layer;

        layer = &level->layers[li];
        for (int32_t oi = 0; oi < layer->object_count; ++oi) {
            dtr_map_object_t *obj;

            obj = &layer->objects[oi];
            if (filter_name != NULL && SDL_strcmp(obj->name, filter_name) != 0) {
                continue;
            }

            JS_SetPropertyUint32(ctx, arr, out_idx, prv_obj_to_js(ctx, obj));
            ++out_idx;
        }
    }

    if (filter_name != NULL) {
        JS_FreeCString(ctx, filter_name);
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  map.object(name)                                                   */
/* ------------------------------------------------------------------ */

static JSValue js_map_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    const char      *name;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_UNDEFINED;
    }

    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_UNDEFINED;
    }

    for (int32_t li = 0; li < level->layer_count; ++li) {
        dtr_map_layer_t *layer;

        layer = &level->layers[li];
        for (int32_t oi = 0; oi < layer->object_count; ++oi) {
            if (SDL_strcmp(layer->objects[oi].name, name) == 0) {
                JS_FreeCString(ctx, name);
                return prv_obj_to_js(ctx, &layer->objects[oi]);
            }
        }
    }

    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  map.objects_in(x, y, w, h)                                         */
/* ------------------------------------------------------------------ */

static JSValue
js_map_objects_in(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    double           rx;
    double           ry;
    double           rw;
    double           rh;
    JSValue          arr;
    uint32_t         out_idx;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 4);
    if (level == NULL) {
        return JS_NewArray(ctx);
    }

    rx = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    ry = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    rw = dtr_api_opt_float(ctx, argc, argv, 2, 0.0);
    rh = dtr_api_opt_float(ctx, argc, argv, 3, 0.0);

    arr     = JS_NewArray(ctx);
    out_idx = 0;

    for (int32_t li = 0; li < level->layer_count; ++li) {
        dtr_map_layer_t *layer;

        layer = &level->layers[li];
        for (int32_t oi = 0; oi < layer->object_count; ++oi) {
            dtr_map_object_t *obj;

            obj = &layer->objects[oi];
            /* AABB overlap test */
            if (obj->x + obj->w > rx && obj->x < rx + rw && obj->y + obj->h > ry &&
                obj->y < ry + rh) {
                JS_SetPropertyUint32(ctx, arr, out_idx, prv_obj_to_js(ctx, obj));
                ++out_idx;
            }
        }
    }

    return arr;
}

/* ------------------------------------------------------------------ */
/*  map.objects_with(prop, value)                                      */
/* ------------------------------------------------------------------ */

static JSValue
js_map_objects_with(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    const char      *prop;
    JSValue          arr;
    uint32_t         out_idx;

    (void)this_val;
    if (argc < 1) {
        return JS_NewArray(ctx);
    }

    level = prv_get_level(ctx, argc, argv, 2);
    if (level == NULL) {
        return JS_NewArray(ctx);
    }

    prop = JS_ToCString(ctx, argv[0]);
    if (prop == NULL) {
        return JS_NewArray(ctx);
    }

    arr     = JS_NewArray(ctx);
    out_idx = 0;

    for (int32_t li = 0; li < level->layer_count; ++li) {
        dtr_map_layer_t *layer;

        layer = &level->layers[li];
        for (int32_t oi = 0; oi < layer->object_count; ++oi) {
            dtr_map_object_t *obj;
            JSValue           pval;

            obj = &layer->objects[oi];

            /* First check the type field */
            if (SDL_strcmp(obj->type, prop) == 0) {
                if (argc < 2 || JS_IsUndefined(argv[1])) {
                    JS_SetPropertyUint32(ctx, arr, out_idx, prv_obj_to_js(ctx, obj));
                    ++out_idx;
                    continue;
                }
            }

            /* Then check the props JS object for a matching property */
            if (!JS_IsUndefined(obj->props) && !JS_IsNull(obj->props)) {
                pval = JS_GetPropertyStr(ctx, obj->props, prop);
                if (!JS_IsUndefined(pval)) {
                    bool match;

                    match = true;
                    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
                        const char *want;
                        const char *got;

                        want = JS_ToCString(ctx, argv[1]);
                        got  = JS_ToCString(ctx, pval);
                        if (want != NULL && got != NULL) {
                            match = (SDL_strcmp(got, want) == 0);
                        } else {
                            match = false;
                        }
                        if (want != NULL)
                            JS_FreeCString(ctx, want);
                        if (got != NULL)
                            JS_FreeCString(ctx, got);
                    }
                    if (match) {
                        JS_SetPropertyUint32(ctx, arr, out_idx, prv_obj_to_js(ctx, obj));
                        ++out_idx;
                    }
                }
                JS_FreeValue(ctx, pval);
            }
        }
    }

    JS_FreeCString(ctx, prop);
    return arr;
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_map_funcs[] = {
    JS_CFUNC_DEF("get", 3, js_map_get),
    JS_CFUNC_DEF("set", 4, js_map_set),
    JS_CFUNC_DEF("flag", 3, js_map_flag),
    JS_CFUNC_DEF("draw", 8, js_map_draw),
    JS_CFUNC_DEF("width", 0, js_map_width),
    JS_CFUNC_DEF("height", 0, js_map_height),
    JS_CFUNC_DEF("layers", 0, js_map_layers),
    JS_CFUNC_DEF("levels", 0, js_map_levels),
    JS_CFUNC_DEF("current_level", 0, js_map_current_level),
    JS_CFUNC_DEF("load", 1, js_map_load),
    JS_CFUNC_DEF("objects", 1, js_map_objects),
    JS_CFUNC_DEF("object", 1, js_map_object),
    JS_CFUNC_DEF("objects_in", 4, js_map_objects_in),
    JS_CFUNC_DEF("objects_with", 2, js_map_objects_with),
};

void dtr_map_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_map_funcs, countof(js_map_funcs));
    JS_SetPropertyStr(ctx, global, "map", ns);
}
