/**
 * \file            cart_import.c
 * \brief           Asset importers — Aseprite, Tiled, LDtk, raw PNG
 */

#include "cart_import.h"

#include "cart.h"

#include <SDL3_image/SDL_image.h>
#include <limits.h>

/* Maximum pixel dimension for imported images (4096x4096 RGBA = 64 MB). */
#define IMPORT_MAX_IMAGE_DIM 4096

/* ------------------------------------------------------------------ */
/*  PNG / image loading via SDL_image                                   */
/* ------------------------------------------------------------------ */

uint8_t *dtr_import_png(const char *path, int32_t *out_w, int32_t *out_h)
{
    SDL_Surface *surface;
    SDL_Surface *rgba;
    uint8_t     *pixels;
    size_t       size;

    surface = IMG_Load(path);
    if (surface == NULL) {
        SDL_Log("dtr_import_png: failed to load '%s': %s", path, SDL_GetError());
        *out_w = 0;
        *out_h = 0;
        return NULL;
    }

    /* Convert to RGBA32 */
    rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);

    if (rgba == NULL) {
        SDL_Log("dtr_import_png: RGBA conversion failed");
        *out_w = 0;
        *out_h = 0;
        return NULL;
    }

    *out_w = rgba->w;
    *out_h = rgba->h;

    if (rgba->w > IMPORT_MAX_IMAGE_DIM || rgba->h > IMPORT_MAX_IMAGE_DIM) {
        SDL_Log("dtr_import_png: image too large (%dx%d, max %d)",
                rgba->w,
                rgba->h,
                IMPORT_MAX_IMAGE_DIM);
        SDL_DestroySurface(rgba);
        *out_w = 0;
        *out_h = 0;
        return NULL;
    }

    size   = (size_t)rgba->w * (size_t)rgba->h * 4;
    pixels = DTR_MALLOC(size);
    if (pixels == NULL) {
        SDL_DestroySurface(rgba);
        return NULL;
    }

    SDL_memcpy(pixels, rgba->pixels, size);
    SDL_DestroySurface(rgba);

    return pixels;
}

/* ------------------------------------------------------------------ */
/*  Aseprite JSON importer                                             */
/* ------------------------------------------------------------------ */

bool dtr_import_aseprite(const char *json_path, dtr_cart_t *cart, JSContext *ctx)
{
    /*
     * Aseprite JSON export format:
     * {
     *   "frames": { "sprite 0.ase": { "frame": {x,y,w,h}, ... }, ... },
     *   "meta": { "image": "sprite.png", "size": {w, h}, ... }
     * }
     *
     * Phase 1: We extract the spritesheet image path, load it,
     * and store the RGBA data. Frame/animation data is available
     * for future use.
     */

    char   *json;
    size_t  len;
    JSValue root;
    JSValue meta;
    JSValue image_val;
    char    image_path[512];
    char    dir[512];

    json = (char *)SDL_LoadFile(json_path, &len);
    if (json == NULL) {
        SDL_Log("dtr_import_aseprite: cannot read '%s'", json_path);
        return false;
    }

    root = JS_ParseJSON(ctx, json, len, "<aseprite>");
    SDL_free(json);

    if (JS_IsException(root)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        SDL_Log("dtr_import_aseprite: JSON parse error");
        return false;
    }

    /* Extract meta.image */
    meta = JS_GetPropertyStr(ctx, root, "meta");
    if (!JS_IsObject(meta)) {
        JS_FreeValue(ctx, meta);
        JS_FreeValue(ctx, root);
        return false;
    }

    image_val = JS_GetPropertyStr(ctx, meta, "image");
    if (JS_IsString(image_val)) {
        const char *img;

        img = JS_ToCString(ctx, image_val);
        if (img != NULL) {
            /* Resolve relative to the JSON file directory */
            {
                const char *last_sep;
                size_t      dir_len;

                last_sep = SDL_strrchr(json_path, '/');
                if (last_sep == NULL) {
                    last_sep = SDL_strrchr(json_path, '\\');
                }
                if (last_sep != NULL) {
                    dir_len = (size_t)(last_sep - json_path);
                    if (dir_len >= sizeof(dir)) {
                        dir_len = sizeof(dir) - 1;
                    }
                    SDL_memcpy(dir, json_path, dir_len);
                    dir[dir_len] = '\0';
                } else {
                    SDL_strlcpy(dir, ".", sizeof(dir));
                }
            }

            SDL_snprintf(image_path, sizeof(image_path), "%s/%s", dir, img);
            JS_FreeCString(ctx, img);

            /* Load the image */
            {
                int32_t w;
                int32_t h;

                cart->sprite_rgba   = dtr_import_png(image_path, &w, &h);
                cart->sprite_rgba_w = w;
                cart->sprite_rgba_h = h;
            }
        }
    }
    JS_FreeValue(ctx, image_val);
    JS_FreeValue(ctx, meta);
    JS_FreeValue(ctx, root);

    return (cart->sprite_rgba != NULL);
}

/* ------------------------------------------------------------------ */
/*  Tiled JSON (.tmj) importer                                         */
/* ------------------------------------------------------------------ */

bool dtr_import_tiled(const char *tmj_path, dtr_map_level_t **out_level, JSContext *ctx)
{
    char            *json;
    size_t           len;
    JSValue          root;
    JSValue          layers_arr;
    int32_t          layer_count;
    dtr_map_level_t *level;

    json = (char *)SDL_LoadFile(tmj_path, &len);
    if (json == NULL) {
        SDL_Log("dtr_import_tiled: cannot read '%s'", tmj_path);
        return false;
    }

    root = JS_ParseJSON(ctx, json, len, "<tiled>");
    SDL_free(json);

    if (JS_IsException(root)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        SDL_Log("dtr_import_tiled: JSON parse error");
        return false;
    }

    level = DTR_CALLOC(1, sizeof(dtr_map_level_t));
    if (level == NULL) {
        JS_FreeValue(ctx, root);
        return false;
    }

    level->width  = 0;
    level->height = 0;

    {
        int32_t w;
        int32_t h;
        JSValue wv;
        JSValue hv;

        wv = JS_GetPropertyStr(ctx, root, "width");
        hv = JS_GetPropertyStr(ctx, root, "height");
        JS_ToInt32(ctx, &w, wv);
        JS_ToInt32(ctx, &h, hv);
        level->width  = w;
        level->height = h;
        JS_FreeValue(ctx, wv);
        JS_FreeValue(ctx, hv);
    }

    layers_arr = JS_GetPropertyStr(ctx, root, "layers");
    if (!JS_IsArray(layers_arr)) {
        JS_FreeValue(ctx, layers_arr);
        JS_FreeValue(ctx, root);
        *out_level = level;
        return true;
    }

    {
        JSValue len_val;

        len_val = JS_GetPropertyStr(ctx, layers_arr, "length");
        JS_ToInt32(ctx, &layer_count, len_val);
        JS_FreeValue(ctx, len_val);
    }

    if (layer_count < 0 || layer_count > CONSOLE_MAX_MAP_LAYERS) {
        SDL_Log("dtr_import_tiled: layer count %d exceeds limit %d",
                layer_count,
                CONSOLE_MAX_MAP_LAYERS);
        JS_FreeValue(ctx, layers_arr);
        JS_FreeValue(ctx, root);
        DTR_FREE(level);
        return false;
    }

    level->layers      = DTR_CALLOC((size_t)layer_count, sizeof(dtr_map_layer_t));
    level->layer_count = layer_count;

    for (int32_t li = 0; li < layer_count; ++li) {
        JSValue          layer_obj;
        JSValue          type_val;
        JSValue          name_val;
        JSValue          data_arr;
        const char      *type_str;
        dtr_map_layer_t *layer;

        layer     = &level->layers[li];
        layer_obj = JS_GetPropertyUint32(ctx, layers_arr, (uint32_t)li);

        /* Layer name */
        name_val = JS_GetPropertyStr(ctx, layer_obj, "name");
        if (JS_IsString(name_val)) {
            const char *name;

            name = JS_ToCString(ctx, name_val);
            if (name != NULL) {
                SDL_strlcpy(layer->name, name, sizeof(layer->name));
                JS_FreeCString(ctx, name);
            }
        }
        JS_FreeValue(ctx, name_val);

        /* Layer type */
        type_val = JS_GetPropertyStr(ctx, layer_obj, "type");
        type_str = JS_ToCString(ctx, type_val);

        if (type_str != NULL && SDL_strcmp(type_str, "tilelayer") == 0) {
            /* Tile layer: extract data array */
            layer->is_tile_layer = true;

            {
                JSValue wv;
                JSValue hv;

                wv = JS_GetPropertyStr(ctx, layer_obj, "width");
                hv = JS_GetPropertyStr(ctx, layer_obj, "height");
                JS_ToInt32(ctx, &layer->width, wv);
                JS_ToInt32(ctx, &layer->height, hv);
                JS_FreeValue(ctx, wv);
                JS_FreeValue(ctx, hv);
            }

            data_arr = JS_GetPropertyStr(ctx, layer_obj, "data");
            if (JS_IsArray(data_arr)) {
                int32_t tile_count;

                if (layer->width <= 0 || layer->height <= 0 ||
                    layer->width > INT32_MAX / layer->height) {
                    JS_FreeValue(ctx, data_arr);
                    if (type_str != NULL) {
                        JS_FreeCString(ctx, type_str);
                    }
                    JS_FreeValue(ctx, type_val);
                    JS_FreeValue(ctx, layer_obj);
                    continue;
                }
                tile_count   = layer->width * layer->height;
                layer->tiles = DTR_CALLOC((size_t)tile_count, sizeof(int32_t));
                if (layer->tiles == NULL) {
                    JS_FreeValue(ctx, data_arr);
                    if (type_str != NULL) {
                        JS_FreeCString(ctx, type_str);
                    }
                    JS_FreeValue(ctx, type_val);
                    JS_FreeValue(ctx, layer_obj);
                    continue;
                }

                for (int32_t ti = 0; ti < tile_count; ++ti) {
                    JSValue tv;

                    tv = JS_GetPropertyUint32(ctx, data_arr, (uint32_t)ti);
                    JS_ToInt32(ctx, &layer->tiles[ti], tv);
                    JS_FreeValue(ctx, tv);
                }
            }
            JS_FreeValue(ctx, data_arr);

        } else if (type_str != NULL && SDL_strcmp(type_str, "objectgroup") == 0) {
            /* Object layer */
            JSValue obj_arr;
            int32_t obj_count;

            layer->is_tile_layer = false;

            obj_arr = JS_GetPropertyStr(ctx, layer_obj, "objects");
            if (JS_IsArray(obj_arr)) {
                JSValue olen;

                olen = JS_GetPropertyStr(ctx, obj_arr, "length");
                JS_ToInt32(ctx, &obj_count, olen);
                JS_FreeValue(ctx, olen);

                if (obj_count < 0 || obj_count > CONSOLE_MAX_MAP_OBJECTS) {
                    SDL_Log("dtr_import_tiled: object count %d exceeds limit %d",
                            obj_count,
                            CONSOLE_MAX_MAP_OBJECTS);
                    obj_count = CONSOLE_MAX_MAP_OBJECTS;
                }

                layer->objects = DTR_CALLOC((size_t)obj_count, sizeof(dtr_map_object_t));
                if (layer->objects == NULL) {
                    JS_FreeValue(ctx, obj_arr);
                    if (type_str != NULL) {
                        JS_FreeCString(ctx, type_str);
                    }
                    JS_FreeValue(ctx, type_val);
                    JS_FreeValue(ctx, layer_obj);
                    continue;
                }
                layer->object_count    = obj_count;
                layer->object_capacity = obj_count;

                for (int32_t oi = 0; oi < obj_count; ++oi) {
                    JSValue           oobj;
                    dtr_map_object_t *mobj;
                    JSValue           xv;
                    JSValue           yv;
                    JSValue           wv;
                    JSValue           hv;
                    double            dx;
                    double            dy;
                    double            dw;
                    double            dh;

                    mobj = &layer->objects[oi];
                    oobj = JS_GetPropertyUint32(ctx, obj_arr, (uint32_t)oi);

                    /* x, y, width, height */
                    xv = JS_GetPropertyStr(ctx, oobj, "x");
                    yv = JS_GetPropertyStr(ctx, oobj, "y");
                    wv = JS_GetPropertyStr(ctx, oobj, "width");
                    hv = JS_GetPropertyStr(ctx, oobj, "height");
                    JS_ToFloat64(ctx, &dx, xv);
                    JS_ToFloat64(ctx, &dy, yv);
                    JS_ToFloat64(ctx, &dw, wv);
                    JS_ToFloat64(ctx, &dh, hv);
                    mobj->x = (float)dx;
                    mobj->y = (float)dy;
                    mobj->w = (float)dw;
                    mobj->h = (float)dh;
                    JS_FreeValue(ctx, xv);
                    JS_FreeValue(ctx, yv);
                    JS_FreeValue(ctx, wv);
                    JS_FreeValue(ctx, hv);

                    /* name, type */
                    {
                        JSValue     nv;
                        JSValue     tv;
                        const char *s;

                        nv = JS_GetPropertyStr(ctx, oobj, "name");
                        s  = JS_ToCString(ctx, nv);
                        if (s != NULL) {
                            SDL_strlcpy(mobj->name, s, sizeof(mobj->name));
                            JS_FreeCString(ctx, s);
                        }
                        JS_FreeValue(ctx, nv);

                        tv = JS_GetPropertyStr(ctx, oobj, "type");
                        s  = JS_ToCString(ctx, tv);
                        if (s != NULL) {
                            SDL_strlcpy(mobj->type, s, sizeof(mobj->type));
                            JS_FreeCString(ctx, s);
                        }
                        JS_FreeValue(ctx, tv);
                    }

                    /* gid */
                    {
                        JSValue gv;

                        gv = JS_GetPropertyStr(ctx, oobj, "gid");
                        if (!JS_IsUndefined(gv)) {
                            JS_ToInt32(ctx, &mobj->gid, gv);
                        }
                        JS_FreeValue(ctx, gv);
                    }

                    /* Tiled custom properties → JS object */
                    {
                        JSValue parr;

                        parr = JS_GetPropertyStr(ctx, oobj, "properties");
                        if (JS_IsArray(parr)) {
                            JSValue lenv;
                            int32_t pcount;

                            lenv = JS_GetPropertyStr(ctx, parr, "length");
                            JS_ToInt32(ctx, &pcount, lenv);
                            JS_FreeValue(ctx, lenv);

                            mobj->props = JS_NewObject(ctx);
                            for (int32_t pi = 0; pi < pcount; ++pi) {
                                JSValue     pobj;
                                JSValue     pname;
                                JSValue     pval;
                                const char *pname_s;

                                pobj    = JS_GetPropertyUint32(ctx, parr, (uint32_t)pi);
                                pname   = JS_GetPropertyStr(ctx, pobj, "name");
                                pval    = JS_GetPropertyStr(ctx, pobj, "value");
                                pname_s = JS_ToCString(ctx, pname);

                                if (pname_s != NULL) {
                                    JS_SetPropertyStr(
                                        ctx, mobj->props, pname_s, JS_DupValue(ctx, pval));
                                    JS_FreeCString(ctx, pname_s);
                                }

                                JS_FreeValue(ctx, pval);
                                JS_FreeValue(ctx, pname);
                                JS_FreeValue(ctx, pobj);
                            }
                        } else {
                            mobj->props = JS_UNDEFINED;
                        }
                        JS_FreeValue(ctx, parr);
                    }

                    JS_FreeValue(ctx, oobj);
                }
            }
            JS_FreeValue(ctx, obj_arr);
        }

        if (type_str != NULL) {
            JS_FreeCString(ctx, type_str);
        }
        JS_FreeValue(ctx, type_val);
        JS_FreeValue(ctx, layer_obj);
    }

    JS_FreeValue(ctx, layers_arr);
    JS_FreeValue(ctx, root);

    *out_level = level;
    return true;
}

/* ------------------------------------------------------------------ */
/*  LDtk JSON (.ldtk) importer                                        */
/* ------------------------------------------------------------------ */

bool dtr_import_ldtk(const char *ldtk_path, dtr_map_level_t **out_level, JSContext *ctx)
{
    char            *json;
    size_t           len;
    JSValue          root;
    JSValue          levels_arr;
    dtr_map_level_t *level;

    json = (char *)SDL_LoadFile(ldtk_path, &len);
    if (json == NULL) {
        SDL_Log("dtr_import_ldtk: cannot read '%s'", ldtk_path);
        return false;
    }

    root = JS_ParseJSON(ctx, json, len, "<ldtk>");
    SDL_free(json);

    if (JS_IsException(root)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        SDL_Log("dtr_import_ldtk: JSON parse error");
        return false;
    }

    level = DTR_CALLOC(1, sizeof(dtr_map_level_t));
    if (level == NULL) {
        JS_FreeValue(ctx, root);
        return false;
    }

    /*
     * LDtk format:
     * {
     *   "levels": [
     *     {
     *       "pxWid": ..., "pxHid": ...,
     *       "layerInstances": [
     *         {
     *           "__type": "Tiles" | "IntGrid" | "Entities",
     *           "__gridSize": 16,
     *           "gridTiles": [ {px, src, f, t}, ... ],
     *           "entityInstances": [ ... ]
     *         }
     *       ]
     *     }
     *   ]
     * }
     *
     * For Phase 1, we load the first level and convert tile layers.
     */

    levels_arr = JS_GetPropertyStr(ctx, root, "levels");
    if (JS_IsArray(levels_arr)) {
        JSValue first_level;
        JSValue layer_instances;
        int32_t li_count;

        first_level = JS_GetPropertyUint32(ctx, levels_arr, 0);
        if (!JS_IsObject(first_level)) {
            JS_FreeValue(ctx, first_level);
            JS_FreeValue(ctx, levels_arr);
            JS_FreeValue(ctx, root);
            *out_level = level;
            return true;
        }

        {
            JSValue wv;
            JSValue hv;

            wv = JS_GetPropertyStr(ctx, first_level, "pxWid");
            hv = JS_GetPropertyStr(ctx, first_level, "pxHid");
            JS_ToInt32(ctx, &level->width, wv);
            JS_ToInt32(ctx, &level->height, hv);
            JS_FreeValue(ctx, wv);
            JS_FreeValue(ctx, hv);
        }

        layer_instances = JS_GetPropertyStr(ctx, first_level, "layerInstances");
        if (JS_IsArray(layer_instances)) {
            JSValue li_len;

            li_len = JS_GetPropertyStr(ctx, layer_instances, "length");
            JS_ToInt32(ctx, &li_count, li_len);
            JS_FreeValue(ctx, li_len);

            if (li_count < 0 || li_count > CONSOLE_MAX_MAP_LAYERS) {
                SDL_Log("dtr_import_ldtk: layer count %d exceeds limit %d",
                        li_count,
                        CONSOLE_MAX_MAP_LAYERS);
                JS_FreeValue(ctx, layer_instances);
                JS_FreeValue(ctx, first_level);
                JS_FreeValue(ctx, levels_arr);
                JS_FreeValue(ctx, root);
                DTR_FREE(level);
                return false;
            }

            level->layers      = DTR_CALLOC((size_t)li_count, sizeof(dtr_map_layer_t));
            level->layer_count = li_count;

            for (int32_t idx = 0; idx < li_count; ++idx) {
                JSValue          li_obj;
                JSValue          type_val;
                const char      *type_str;
                dtr_map_layer_t *layer;

                layer  = &level->layers[idx];
                li_obj = JS_GetPropertyUint32(ctx, layer_instances, (uint32_t)idx);

                type_val = JS_GetPropertyStr(ctx, li_obj, "__type");
                type_str = JS_ToCString(ctx, type_val);

                if (type_str != NULL && SDL_strcmp(type_str, "Tiles") == 0) {
                    JSValue grid_tiles;
                    JSValue grid_size_val;
                    int32_t grid_size;
                    int32_t tile_count;

                    layer->is_tile_layer = true;

                    grid_size_val = JS_GetPropertyStr(ctx, li_obj, "__gridSize");
                    JS_ToInt32(ctx, &grid_size, grid_size_val);
                    JS_FreeValue(ctx, grid_size_val);

                    if (grid_size > 0) {
                        layer->width  = level->width / grid_size;
                        layer->height = level->height / grid_size;
                    }

                    grid_tiles = JS_GetPropertyStr(ctx, li_obj, "gridTiles");
                    if (JS_IsArray(grid_tiles)) {
                        JSValue gt_len;

                        gt_len = JS_GetPropertyStr(ctx, grid_tiles, "length");
                        JS_ToInt32(ctx, &tile_count, gt_len);
                        JS_FreeValue(ctx, gt_len);

                        if (layer->width <= 0 || layer->height <= 0 ||
                            layer->width > INT32_MAX / layer->height) {
                            JS_FreeValue(ctx, grid_tiles);
                            if (type_str != NULL) {
                                JS_FreeCString(ctx, type_str);
                            }
                            JS_FreeValue(ctx, type_val);
                            JS_FreeValue(ctx, li_obj);
                            continue;
                        }

                        layer->tiles = DTR_CALLOC((size_t)layer->width * (size_t)layer->height,
                                                  sizeof(int32_t));

                        for (int32_t ti = 0; ti < tile_count; ++ti) {
                            JSValue gt;
                            JSValue px_arr;
                            JSValue t_val;
                            int32_t tile_id;
                            int32_t px_x;
                            int32_t px_y;
                            int32_t gx;
                            int32_t gy;

                            gt = JS_GetPropertyUint32(ctx, grid_tiles, (uint32_t)ti);

                            /* px = [x, y] pixel position */
                            px_arr = JS_GetPropertyStr(ctx, gt, "px");
                            if (JS_IsArray(px_arr)) {
                                JSValue pxv;
                                JSValue pyv;

                                pxv = JS_GetPropertyUint32(ctx, px_arr, 0);
                                pyv = JS_GetPropertyUint32(ctx, px_arr, 1);
                                JS_ToInt32(ctx, &px_x, pxv);
                                JS_ToInt32(ctx, &px_y, pyv);
                                JS_FreeValue(ctx, pxv);
                                JS_FreeValue(ctx, pyv);
                            } else {
                                px_x = 0;
                                px_y = 0;
                            }
                            JS_FreeValue(ctx, px_arr);

                            t_val = JS_GetPropertyStr(ctx, gt, "t");
                            JS_ToInt32(ctx, &tile_id, t_val);
                            JS_FreeValue(ctx, t_val);

                            gx = (grid_size > 0) ? px_x / grid_size : 0;
                            gy = (grid_size > 0) ? px_y / grid_size : 0;

                            if (gx >= 0 && gx < layer->width && gy >= 0 && gy < layer->height) {
                                layer->tiles[gy * layer->width + gx] = tile_id;
                            }

                            JS_FreeValue(ctx, gt);
                        }
                    }
                    JS_FreeValue(ctx, grid_tiles);
                }

                if (type_str != NULL) {
                    JS_FreeCString(ctx, type_str);
                }
                JS_FreeValue(ctx, type_val);
                JS_FreeValue(ctx, li_obj);
            }
        }
        JS_FreeValue(ctx, layer_instances);
        JS_FreeValue(ctx, first_level);
    }
    JS_FreeValue(ctx, levels_arr);
    JS_FreeValue(ctx, root);

    *out_level = level;
    return true;
}
