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
        return JS_UNDEFINED;
    }

    x         = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    y         = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    layer_idx = dtr_api_opt_int(ctx, argc, argv, 2, 0);

    if (layer_idx < 0 || layer_idx >= level->layer_count) {
        return JS_UNDEFINED;
    }

    {
        dtr_map_layer_t *layer;

        layer = &level->layers[layer_idx];
        if (!layer->is_tile_layer || layer->tiles == NULL) {
            return JS_UNDEFINED;
        }
        if (x < 0 || x >= layer->width || y < 0 || y >= layer->height) {
            return JS_UNDEFINED;
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

        dtr_gfx_map_draw(con->graphics,
                         layer->tiles,
                         layer->width,
                         layer->height,
                         sx,
                         sy,
                         tw,
                         th,
                         dx,
                         dy,
                         con->cart->sprites.tile_w,
                         con->cart->sprites.tile_h);
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

/* ------------------------------------------------------------------ */
/*  map.create(w, h, name?)                                            */
/* ------------------------------------------------------------------ */

static JSValue js_map_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    int32_t          wid;
    int32_t          hgt;
    const char      *name;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          slot;

    (void)this_val;
    if (argc < 2) {
        return JS_FALSE;
    }

    con = dtr_api_get_console(ctx);
    wid = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    hgt = dtr_api_opt_int(ctx, argc, argv, 1, 0);

    if (wid <= 0 || hgt <= 0 || wid > 4096 || hgt > 4096) {
        return JS_FALSE;
    }

    if (con->cart->map_count >= CONSOLE_MAP_SLOTS) {
        return JS_FALSE;
    }

    name = NULL;
    if (argc >= 3 && JS_IsString(argv[2])) {
        name = JS_ToCString(ctx, argv[2]);
    }

    level = DTR_CALLOC(1, sizeof(dtr_map_level_t));
    if (level == NULL) {
        if (name != NULL) {
            JS_FreeCString(ctx, name);
        }
        return JS_FALSE;
    }

    level->width       = wid;
    level->height      = hgt;
    level->tile_w      = con->cart->sprites.tile_w;
    level->tile_h      = con->cart->sprites.tile_h;
    level->layer_count = 1;
    level->layers      = DTR_CALLOC(1, sizeof(dtr_map_layer_t));
    if (level->layers == NULL) {
        DTR_FREE(level);
        if (name != NULL) {
            JS_FreeCString(ctx, name);
        }
        return JS_FALSE;
    }

    if (name != NULL) {
        SDL_strlcpy(level->name, name, sizeof(level->name));
        JS_FreeCString(ctx, name);
    } else {
        SDL_snprintf(level->name, sizeof(level->name), "map_%d", con->cart->map_count);
    }

    layer                = &level->layers[0];
    layer->is_tile_layer = true;
    layer->width         = wid;
    layer->height        = hgt;
    SDL_strlcpy(layer->name, "Layer 1", sizeof(layer->name));
    layer->tiles = DTR_CALLOC((size_t)wid * (size_t)hgt, sizeof(int32_t));
    if (layer->tiles == NULL) {
        DTR_FREE(level->layers);
        DTR_FREE(level);
        return JS_FALSE;
    }

    slot                   = con->cart->map_count;
    con->cart->maps[slot]  = level;
    con->cart->map_count   = slot + 1;
    con->cart->current_map = slot;

    return JS_TRUE;
}

/* ------------------------------------------------------------------ */
/*  map.resize(w, h, slot?)                                            */
/* ------------------------------------------------------------------ */

static JSValue js_map_resize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    int32_t          new_w;
    int32_t          new_h;
    int32_t          old_w;
    int32_t          old_h;

    (void)this_val;
    if (argc < 2) {
        return JS_FALSE;
    }

    level = prv_get_level(ctx, argc, argv, 2);
    if (level == NULL) {
        return JS_FALSE;
    }

    new_w = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    new_h = dtr_api_opt_int(ctx, argc, argv, 1, 0);

    if (new_w <= 0 || new_h <= 0 || new_w > 4096 || new_h > 4096) {
        return JS_FALSE;
    }

    old_w = level->width;
    old_h = level->height;
    if (new_w == old_w && new_h == old_h) {
        return JS_TRUE;
    }

    for (int32_t li = 0; li < level->layer_count; ++li) {
        dtr_map_layer_t *layer;
        int32_t         *new_tiles;
        int32_t          copy_w;
        int32_t          copy_h;

        layer = &level->layers[li];
        if (!layer->is_tile_layer || layer->tiles == NULL) {
            continue;
        }

        new_tiles = DTR_CALLOC((size_t)new_w * (size_t)new_h, sizeof(int32_t));
        if (new_tiles == NULL) {
            return JS_FALSE;
        }

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
    }

    level->width  = new_w;
    level->height = new_h;

    return JS_TRUE;
}

/* ------------------------------------------------------------------ */
/*  map.add_layer(name?, slot?)                                        */
/* ------------------------------------------------------------------ */

static JSValue js_map_add_layer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    dtr_map_level_t *level;
    dtr_map_layer_t *new_layers;
    dtr_map_layer_t *layer;
    const char      *name;

    (void)this_val;
    con   = dtr_api_get_console(ctx);
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewInt32(ctx, -1);
    }

    if (level->layer_count >= CONSOLE_MAX_MAP_LAYERS) {
        return JS_NewInt32(ctx, -1);
    }

    new_layers =
        DTR_REALLOC(level->layers, (size_t)(level->layer_count + 1) * sizeof(dtr_map_layer_t));
    if (new_layers == NULL) {
        return JS_NewInt32(ctx, -1);
    }
    level->layers = new_layers;

    layer = &level->layers[level->layer_count];
    SDL_memset(layer, 0, sizeof(dtr_map_layer_t));
    layer->is_tile_layer = true;
    layer->width         = level->width;
    layer->height        = level->height;
    layer->tiles         = DTR_CALLOC((size_t)(level->width * level->height), sizeof(int32_t));
    if (layer->tiles == NULL) {
        return JS_NewInt32(ctx, -1);
    }

    name = NULL;
    if (argc >= 1 && JS_IsString(argv[0])) {
        name = JS_ToCString(ctx, argv[0]);
    }
    if (name != NULL) {
        SDL_strlcpy(layer->name, name, sizeof(layer->name));
        JS_FreeCString(ctx, name);
    } else {
        SDL_snprintf(layer->name, sizeof(layer->name), "Layer %d", level->layer_count + 1);
    }

    (void)con;
    level->layer_count += 1;
    return JS_NewInt32(ctx, level->layer_count - 1);
}

/* ------------------------------------------------------------------ */
/*  map.remove_layer(idx, slot?)                                       */
/* ------------------------------------------------------------------ */

static JSValue
js_map_remove_layer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    dtr_map_level_t *level;
    int32_t          idx;

    (void)this_val;
    if (argc < 1) {
        return JS_FALSE;
    }

    con   = dtr_api_get_console(ctx);
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_FALSE;
    }

    idx = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    if (idx < 0 || idx >= level->layer_count) {
        return JS_FALSE;
    }

    /* Must keep at least one layer */
    if (level->layer_count <= 1) {
        return JS_FALSE;
    }

    /* Free the layer's data */
    DTR_FREE(level->layers[idx].tiles);
    for (int32_t oi = 0; oi < level->layers[idx].object_count; ++oi) {
        dtr_map_object_t *obj;

        obj = &level->layers[idx].objects[oi];
        if (con->cart->ctx != NULL && !JS_IsUndefined(obj->props)) {
            JS_FreeValue(con->cart->ctx, obj->props);
        }
    }
    DTR_FREE(level->layers[idx].objects);

    /* Shift remaining layers down */
    for (int32_t shi = idx; shi < level->layer_count - 1; ++shi) {
        level->layers[shi] = level->layers[shi + 1];
    }
    level->layer_count -= 1;

    return JS_TRUE;
}

/* ------------------------------------------------------------------ */
/*  map.rename_layer(idx, name, slot?)                                 */
/* ------------------------------------------------------------------ */

static JSValue
js_map_rename_layer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    int32_t          idx;
    const char      *name;

    (void)this_val;
    if (argc < 2) {
        return JS_FALSE;
    }

    level = prv_get_level(ctx, argc, argv, 2);
    if (level == NULL) {
        return JS_FALSE;
    }

    idx = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    if (idx < 0 || idx >= level->layer_count) {
        return JS_FALSE;
    }

    if (!JS_IsString(argv[1])) {
        return JS_FALSE;
    }

    name = JS_ToCString(ctx, argv[1]);
    if (name == NULL) {
        return JS_FALSE;
    }
    SDL_strlcpy(level->layers[idx].name, name, sizeof(level->layers[idx].name));
    JS_FreeCString(ctx, name);

    return JS_TRUE;
}

/* ------------------------------------------------------------------ */
/*  map.data(slot?) — return tile data as flat array for serialization  */
/* ------------------------------------------------------------------ */

static JSValue js_map_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    JSValue          result;
    JSValue          layer_arr;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 0);
    if (level == NULL) {
        return JS_NULL;
    }

    result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "name", JS_NewString(ctx, level->name));
    JS_SetPropertyStr(ctx, result, "width", JS_NewInt32(ctx, level->width));
    JS_SetPropertyStr(ctx, result, "height", JS_NewInt32(ctx, level->height));
    JS_SetPropertyStr(ctx, result, "tileW", JS_NewInt32(ctx, level->tile_w));
    JS_SetPropertyStr(ctx, result, "tileH", JS_NewInt32(ctx, level->tile_h));

    layer_arr = JS_NewArray(ctx);
    for (int32_t li = 0; li < level->layer_count; ++li) {
        dtr_map_layer_t *layer;
        JSValue          lobj;

        layer = &level->layers[li];
        lobj  = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, lobj, "name", JS_NewString(ctx, layer->name));
        JS_SetPropertyStr(ctx,
                          lobj,
                          "type",
                          JS_NewString(ctx, layer->is_tile_layer ? "tilelayer" : "objectgroup"));

        if (layer->is_tile_layer && layer->tiles != NULL) {
            JSValue tiles;
            int32_t cnt;

            cnt   = layer->width * layer->height;
            tiles = JS_NewArray(ctx);
            for (int32_t idx = 0; idx < cnt; ++idx) {
                JS_SetPropertyUint32(
                    ctx, tiles, (uint32_t)idx, JS_NewInt32(ctx, layer->tiles[idx]));
            }
            JS_SetPropertyStr(ctx, lobj, "data", tiles);
            JS_SetPropertyStr(ctx, lobj, "width", JS_NewInt32(ctx, layer->width));
            JS_SetPropertyStr(ctx, lobj, "height", JS_NewInt32(ctx, layer->height));
        } else if (!layer->is_tile_layer) {
            JSValue objs;

            objs = JS_NewArray(ctx);
            for (int32_t oidx = 0; oidx < layer->object_count; ++oidx) {
                JS_SetPropertyUint32(
                    ctx, objs, (uint32_t)oidx, prv_obj_to_js(ctx, &layer->objects[oidx]));
            }
            JS_SetPropertyStr(ctx, lobj, "objects", objs);
        }

        JS_SetPropertyUint32(ctx, layer_arr, (uint32_t)li, lobj);
    }
    JS_SetPropertyStr(ctx, result, "layers", layer_arr);

    return result;
}

/* ------------------------------------------------------------------ */
/*  map.add_object_layer(name?, slot?)                                 */
/* ------------------------------------------------------------------ */

static JSValue
js_map_add_object_layer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    dtr_map_layer_t *new_layers;
    dtr_map_layer_t *layer;
    const char      *name;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewInt32(ctx, -1);
    }

    if (level->layer_count >= CONSOLE_MAX_MAP_LAYERS) {
        return JS_NewInt32(ctx, -1);
    }

    new_layers =
        DTR_REALLOC(level->layers, (size_t)(level->layer_count + 1) * sizeof(dtr_map_layer_t));
    if (new_layers == NULL) {
        return JS_NewInt32(ctx, -1);
    }
    level->layers = new_layers;

    layer = &level->layers[level->layer_count];
    SDL_memset(layer, 0, sizeof(dtr_map_layer_t));
    layer->is_tile_layer = false;
    layer->width         = level->width;
    layer->height        = level->height;

    name = NULL;
    if (argc >= 1 && JS_IsString(argv[0])) {
        name = JS_ToCString(ctx, argv[0]);
    }
    if (name != NULL) {
        SDL_strlcpy(layer->name, name, sizeof(layer->name));
        JS_FreeCString(ctx, name);
    } else {
        SDL_snprintf(layer->name, sizeof(layer->name), "Objects %d", level->layer_count + 1);
    }

    level->layer_count += 1;
    return JS_NewInt32(ctx, level->layer_count - 1);
}

/* ------------------------------------------------------------------ */
/*  map.layer_type(idx, slot?)                                         */
/* ------------------------------------------------------------------ */

static JSValue
js_map_layer_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    int32_t          idx;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewString(ctx, "");
    }

    idx = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    if (idx < 0 || idx >= level->layer_count) {
        return JS_NewString(ctx, "");
    }

    if (level->layers[idx].is_tile_layer) {
        return JS_NewString(ctx, "tilelayer");
    }
    return JS_NewString(ctx, "objectgroup");
}

/* ------------------------------------------------------------------ */
/*  map.layer_objects(layerIdx, slot?)                                  */
/* ------------------------------------------------------------------ */

static JSValue
js_map_layer_objects(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          idx;
    JSValue          arr;

    (void)this_val;
    level = prv_get_level(ctx, argc, argv, 1);
    if (level == NULL) {
        return JS_NewArray(ctx);
    }

    idx = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    if (idx < 0 || idx >= level->layer_count) {
        return JS_NewArray(ctx);
    }

    layer = &level->layers[idx];
    arr   = JS_NewArray(ctx);

    for (int32_t oidx = 0; oidx < layer->object_count; ++oidx) {
        JSValue jobj;

        jobj = prv_obj_to_js(ctx, &layer->objects[oidx]);
        JS_SetPropertyStr(ctx, jobj, "idx", JS_NewInt32(ctx, oidx));
        JS_SetPropertyUint32(ctx, arr, (uint32_t)oidx, jobj);
    }

    return arr;
}

/* ------------------------------------------------------------------ */
/*  map.add_object(layerIdx, {name,type,x,y,w,h}, slot?)              */
/* ------------------------------------------------------------------ */

static JSValue
js_map_add_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t  *level;
    dtr_map_layer_t  *layer;
    dtr_map_object_t *new_objs;
    dtr_map_object_t *obj;
    int32_t           lay_idx;
    JSValue           jobj;
    JSValue           val;

    (void)this_val;
    if (argc < 2) {
        return JS_NewInt32(ctx, -1);
    }

    level = prv_get_level(ctx, argc, argv, 2);
    if (level == NULL) {
        return JS_NewInt32(ctx, -1);
    }

    lay_idx = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    if (lay_idx < 0 || lay_idx >= level->layer_count) {
        return JS_NewInt32(ctx, -1);
    }

    layer = &level->layers[lay_idx];
    if (layer->object_count >= CONSOLE_MAX_MAP_OBJECTS) {
        return JS_NewInt32(ctx, -1);
    }

    if (layer->object_count >= layer->object_capacity) {
        int32_t new_cap;

        new_cap = layer->object_capacity < 8 ? 8 : layer->object_capacity * 2;
        if (new_cap > CONSOLE_MAX_MAP_OBJECTS) {
            new_cap = CONSOLE_MAX_MAP_OBJECTS;
        }
        new_objs = DTR_REALLOC(layer->objects, (size_t)new_cap * sizeof(dtr_map_object_t));
        if (new_objs == NULL) {
            return JS_NewInt32(ctx, -1);
        }
        layer->objects         = new_objs;
        layer->object_capacity = new_cap;
    }

    obj = &layer->objects[layer->object_count];
    SDL_memset(obj, 0, sizeof(dtr_map_object_t));
    obj->props = JS_UNDEFINED;

    jobj = argv[1];

    /* Read name */
    val = JS_GetPropertyStr(ctx, jobj, "name");
    if (JS_IsString(val)) {
        const char *str;

        str = JS_ToCString(ctx, val);
        if (str != NULL) {
            SDL_strlcpy(obj->name, str, sizeof(obj->name));
            JS_FreeCString(ctx, str);
        }
    }
    JS_FreeValue(ctx, val);

    /* Read type */
    val = JS_GetPropertyStr(ctx, jobj, "type");
    if (JS_IsString(val)) {
        const char *str;

        str = JS_ToCString(ctx, val);
        if (str != NULL) {
            SDL_strlcpy(obj->type, str, sizeof(obj->type));
            JS_FreeCString(ctx, str);
        }
    }
    JS_FreeValue(ctx, val);

    /* Read position and size */
    {
        double tmp;

        tmp = 0.0;
        val = JS_GetPropertyStr(ctx, jobj, "x");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->x = (float)tmp;
        }
        JS_FreeValue(ctx, val);

        tmp = 0.0;
        val = JS_GetPropertyStr(ctx, jobj, "y");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->y = (float)tmp;
        }
        JS_FreeValue(ctx, val);

        tmp = 0.0;
        val = JS_GetPropertyStr(ctx, jobj, "w");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->w = (float)tmp;
        }
        JS_FreeValue(ctx, val);

        tmp = 0.0;
        val = JS_GetPropertyStr(ctx, jobj, "h");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->h = (float)tmp;
        }
        JS_FreeValue(ctx, val);
    }

    /* Read gid */
    val = JS_GetPropertyStr(ctx, jobj, "gid");
    if (!JS_IsUndefined(val)) {
        JS_ToInt32(ctx, &obj->gid, val);
    }
    JS_FreeValue(ctx, val);

    layer->object_count += 1;
    return JS_NewInt32(ctx, layer->object_count - 1);
}

/* ------------------------------------------------------------------ */
/*  map.remove_object(layerIdx, objIdx, slot?)                         */
/* ------------------------------------------------------------------ */

static JSValue
js_map_remove_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    dtr_map_level_t *level;
    dtr_map_layer_t *layer;
    int32_t          lay_idx;
    int32_t          obj_idx;

    (void)this_val;
    if (argc < 2) {
        return JS_FALSE;
    }

    con   = dtr_api_get_console(ctx);
    level = prv_get_level(ctx, argc, argv, 2);
    if (level == NULL) {
        return JS_FALSE;
    }

    lay_idx = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    if (lay_idx < 0 || lay_idx >= level->layer_count) {
        return JS_FALSE;
    }

    layer   = &level->layers[lay_idx];
    obj_idx = dtr_api_opt_int(ctx, argc, argv, 1, -1);
    if (obj_idx < 0 || obj_idx >= layer->object_count) {
        return JS_FALSE;
    }

    /* Free props if set */
    {
        dtr_map_object_t *obj;

        obj = &layer->objects[obj_idx];
        if (con->cart->ctx != NULL && !JS_IsUndefined(obj->props)) {
            JS_FreeValue(con->cart->ctx, obj->props);
        }
    }

    /* Shift remaining objects down */
    for (int32_t shi = obj_idx; shi < layer->object_count - 1; ++shi) {
        layer->objects[shi] = layer->objects[shi + 1];
    }
    layer->object_count -= 1;

    return JS_TRUE;
}

/* ------------------------------------------------------------------ */
/*  map.set_object(layerIdx, objIdx, {fields}, slot?)                  */
/* ------------------------------------------------------------------ */

static JSValue
js_map_set_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_map_level_t  *level;
    dtr_map_layer_t  *layer;
    dtr_map_object_t *obj;
    int32_t           lay_idx;
    int32_t           obj_idx;
    JSValue           jobj;
    JSValue           val;

    (void)this_val;
    if (argc < 3) {
        return JS_FALSE;
    }

    level = prv_get_level(ctx, argc, argv, 3);
    if (level == NULL) {
        return JS_FALSE;
    }

    lay_idx = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    if (lay_idx < 0 || lay_idx >= level->layer_count) {
        return JS_FALSE;
    }

    layer   = &level->layers[lay_idx];
    obj_idx = dtr_api_opt_int(ctx, argc, argv, 1, -1);
    if (obj_idx < 0 || obj_idx >= layer->object_count) {
        return JS_FALSE;
    }

    obj  = &layer->objects[obj_idx];
    jobj = argv[2];

    /* Update name if present */
    val = JS_GetPropertyStr(ctx, jobj, "name");
    if (JS_IsString(val)) {
        const char *str;

        str = JS_ToCString(ctx, val);
        if (str != NULL) {
            SDL_strlcpy(obj->name, str, sizeof(obj->name));
            JS_FreeCString(ctx, str);
        }
    }
    JS_FreeValue(ctx, val);

    /* Update type if present */
    val = JS_GetPropertyStr(ctx, jobj, "type");
    if (JS_IsString(val)) {
        const char *str;

        str = JS_ToCString(ctx, val);
        if (str != NULL) {
            SDL_strlcpy(obj->type, str, sizeof(obj->type));
            JS_FreeCString(ctx, str);
        }
    }
    JS_FreeValue(ctx, val);

    /* Update position and size */
    {
        double tmp;

        val = JS_GetPropertyStr(ctx, jobj, "x");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->x = (float)tmp;
        }
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, jobj, "y");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->y = (float)tmp;
        }
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, jobj, "w");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->w = (float)tmp;
        }
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, jobj, "h");
        if (!JS_IsUndefined(val)) {
            JS_ToFloat64(ctx, &tmp, val);
            obj->h = (float)tmp;
        }
        JS_FreeValue(ctx, val);
    }

    /* Update gid if present */
    val = JS_GetPropertyStr(ctx, jobj, "gid");
    if (!JS_IsUndefined(val)) {
        JS_ToInt32(ctx, &obj->gid, val);
    }
    JS_FreeValue(ctx, val);

    return JS_TRUE;
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
    JS_CFUNC_DEF("currentLevel", 0, js_map_current_level),
    JS_CFUNC_DEF("load", 1, js_map_load),
    JS_CFUNC_DEF("objects", 1, js_map_objects),
    JS_CFUNC_DEF("object", 1, js_map_object),
    JS_CFUNC_DEF("objectsIn", 4, js_map_objects_in),
    JS_CFUNC_DEF("objectsWith", 2, js_map_objects_with),
    JS_CFUNC_DEF("create", 3, js_map_create),
    JS_CFUNC_DEF("resize", 3, js_map_resize),
    JS_CFUNC_DEF("addLayer", 2, js_map_add_layer),
    JS_CFUNC_DEF("removeLayer", 2, js_map_remove_layer),
    JS_CFUNC_DEF("renameLayer", 3, js_map_rename_layer),
    JS_CFUNC_DEF("data", 1, js_map_data),
    JS_CFUNC_DEF("addObjectLayer", 2, js_map_add_object_layer),
    JS_CFUNC_DEF("layerType", 2, js_map_layer_type),
    JS_CFUNC_DEF("layerObjects", 2, js_map_layer_objects),
    JS_CFUNC_DEF("addObject", 3, js_map_add_object),
    JS_CFUNC_DEF("removeObject", 3, js_map_remove_object),
    JS_CFUNC_DEF("setObject", 4, js_map_set_object),
};

void dtr_map_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_map_funcs, countof(js_map_funcs));
    JS_SetPropertyStr(ctx, global, "map", ns);
}
