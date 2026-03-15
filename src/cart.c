/**
 * \file            cart.c
 * \brief           Cart loading, parsing, validation, and persistence
 */

#include "cart.h"

#include "runtime.h"

/* ------------------------------------------------------------------ */
/*  Create                                                             */
/* ------------------------------------------------------------------ */

mvn_cart_t *mvn_cart_create(void)
{
    mvn_cart_t *cart;

    cart = MVN_CALLOC(1, sizeof(mvn_cart_t));
    if (cart != NULL) {
        mvn_cart_defaults(cart);
    }
    return cart;
}

/* ------------------------------------------------------------------ */
/*  Defaults                                                           */
/* ------------------------------------------------------------------ */

void mvn_cart_defaults(mvn_cart_t *cart)
{
    SDL_memset(cart, 0, sizeof(mvn_cart_t));

    /* Display defaults */
    cart->display.width      = CONSOLE_FB_WIDTH;
    cart->display.height     = CONSOLE_FB_HEIGHT;
    cart->display.scale      = CONSOLE_DEFAULT_SCALE;
    cart->display.palette    = 0;
    cart->display.fullscreen = false;

    /* Timing defaults */
    cart->timing.fps    = CONSOLE_FPS;
    cart->timing.ups    = CONSOLE_FPS;
    cart->timing.frozen = false;

    /* Sprite defaults */
    cart->sprites.tile_w = CONSOLE_TILE_W;
    cart->sprites.tile_h = CONSOLE_TILE_H;

    /* Runtime defaults */
    cart->runtime.mem_limit   = CONSOLE_JS_MEM_MB * 1024 * 1024;
    cart->runtime.stack_limit = CONSOLE_JS_STACK_KB * 1024;

    /* Audio defaults */
    cart->audio.channels    = CONSOLE_AUDIO_CHANNELS;
    cart->audio.frequency   = CONSOLE_AUDIO_FREQ;
    cart->audio.buffer_size = CONSOLE_AUDIO_BUFFER;

    /* Meta defaults */
    SDL_strlcpy(cart->meta.title, "Untitled", sizeof(cart->meta.title));
    SDL_strlcpy(cart->meta.version, "0.1.0", sizeof(cart->meta.version));
}

/* ------------------------------------------------------------------ */
/*  Helper: read a JSON property                                       */
/* ------------------------------------------------------------------ */

/**
 * \brief           Read an integer from a JSON object property
 */
static int32_t prv_json_int(JSContext *ctx, JSValue obj, const char *key, int32_t dflt)
{
    JSValue val;
    int32_t result;

    val = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(val) || JS_IsNull(val)) {
        JS_FreeValue(ctx, val);
        return dflt;
    }
    if (JS_ToInt32(ctx, &result, val) != 0) {
        JS_FreeValue(ctx, val);
        return dflt;
    }
    JS_FreeValue(ctx, val);
    return result;
}

/**
 * \brief           Read a string from a JSON object property
 */
static void prv_json_str(JSContext * ctx,
                         JSValue     obj,
                         const char *key,
                         char *      out,
                         size_t      out_sz,
                         const char *dflt)
{
    JSValue     val;
    const char *str;

    val = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(val) || JS_IsNull(val)) {
        JS_FreeValue(ctx, val);
        SDL_strlcpy(out, dflt, out_sz);
        return;
    }

    str = JS_ToCString(ctx, val);
    if (str != NULL) {
        SDL_strlcpy(out, str, out_sz);
        JS_FreeCString(ctx, str);
    } else {
        SDL_strlcpy(out, dflt, out_sz);
    }
    JS_FreeValue(ctx, val);
}

/**
 * \brief           Read a boolean from a JSON object property
 */
static bool prv_json_bool(JSContext *ctx, JSValue obj, const char *key, bool dflt)
{
    JSValue val;
    int     bval;

    val = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(val) || JS_IsNull(val)) {
        JS_FreeValue(ctx, val);
        return dflt;
    }
    bval = JS_ToBool(ctx, val);
    JS_FreeValue(ctx, val);
    return (bval > 0);
}

/* ------------------------------------------------------------------ */
/*  Parse cart.json                                                    */
/* ------------------------------------------------------------------ */

bool mvn_cart_parse(mvn_cart_t *cart, JSContext *ctx, const char *json, size_t len)
{
    JSValue root;
    JSValue sub;

    mvn_cart_defaults(cart);

    root = JS_ParseJSON(ctx, json, len, "<cart>");
    if (JS_IsException(root)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        SDL_Log("Failed to parse cart.json");
        return false;
    }

    /* --- meta --- */
    sub = JS_GetPropertyStr(ctx, root, "meta");
    if (JS_IsObject(sub)) {
        prv_json_str(ctx, sub, "title", cart->meta.title, sizeof(cart->meta.title), "Untitled");
        prv_json_str(ctx, sub, "author", cart->meta.author, sizeof(cart->meta.author), "");
        prv_json_str(ctx, sub, "version", cart->meta.version, sizeof(cart->meta.version), "0.1.0");
        prv_json_str(
            ctx, sub, "description", cart->meta.description, sizeof(cart->meta.description), "");
    }
    JS_FreeValue(ctx, sub);

    /* --- display --- */
    sub = JS_GetPropertyStr(ctx, root, "display");
    if (JS_IsObject(sub)) {
        cart->display.width      = prv_json_int(ctx, sub, "width", CONSOLE_FB_WIDTH);
        cart->display.height     = prv_json_int(ctx, sub, "height", CONSOLE_FB_HEIGHT);
        cart->display.scale      = prv_json_int(ctx, sub, "scale", CONSOLE_DEFAULT_SCALE);
        cart->display.palette    = prv_json_int(ctx, sub, "palette", 0);
        cart->display.fullscreen = prv_json_bool(ctx, sub, "fullscreen", false);
    }
    JS_FreeValue(ctx, sub);

    /* --- timing --- */
    sub = JS_GetPropertyStr(ctx, root, "timing");
    if (JS_IsObject(sub)) {
        cart->timing.fps = prv_json_int(ctx, sub, "fps", CONSOLE_FPS);
        cart->timing.ups = prv_json_int(ctx, sub, "ups", CONSOLE_FPS);
    }
    JS_FreeValue(ctx, sub);

    /* --- sprites --- */
    sub = JS_GetPropertyStr(ctx, root, "sprites");
    if (JS_IsObject(sub)) {
        JSValue sheet;

        cart->sprites.tile_w = prv_json_int(ctx, sub, "tileW", CONSOLE_TILE_W);
        cart->sprites.tile_h = prv_json_int(ctx, sub, "tileH", CONSOLE_TILE_H);

        sheet = JS_GetPropertyStr(ctx, sub, "sheet");
        if (JS_IsString(sheet)) {
            const char *path;

            path = JS_ToCString(ctx, sheet);
            if (path != NULL) {
                SDL_strlcpy(cart->sprite_sheet_path, path, sizeof(cart->sprite_sheet_path));
                JS_FreeCString(ctx, path);
            }
        }
        JS_FreeValue(ctx, sheet);
    }
    JS_FreeValue(ctx, sub);

    /* --- runtime --- */
    sub = JS_GetPropertyStr(ctx, root, "runtime");
    if (JS_IsObject(sub)) {
        int32_t mem_mb;
        int32_t stack_kb;

        mem_mb                    = prv_json_int(ctx, sub, "memoryLimitMB", CONSOLE_JS_MEM_MB);
        stack_kb                  = prv_json_int(ctx, sub, "stackLimitKB", CONSOLE_JS_STACK_KB);
        cart->runtime.mem_limit   = (uint32_t)mem_mb * 1024u * 1024u;
        cart->runtime.stack_limit = (uint32_t)stack_kb * 1024u;
    }
    JS_FreeValue(ctx, sub);

    /* --- audio --- */
    sub = JS_GetPropertyStr(ctx, root, "audio");
    if (JS_IsObject(sub)) {
        cart->audio.channels    = prv_json_int(ctx, sub, "channels", CONSOLE_AUDIO_CHANNELS);
        cart->audio.frequency   = prv_json_int(ctx, sub, "frequency", CONSOLE_AUDIO_FREQ);
        cart->audio.buffer_size = prv_json_int(ctx, sub, "bufferSize", CONSOLE_AUDIO_BUFFER);
    }
    JS_FreeValue(ctx, sub);

    /* --- code (inline or path) --- */
    {
        JSValue code_val;

        code_val = JS_GetPropertyStr(ctx, root, "code");
        if (JS_IsString(code_val)) {
            const char *code;

            code = JS_ToCString(ctx, code_val);
            if (code != NULL) {
                SDL_strlcpy(cart->code_path, code, sizeof(cart->code_path));
                JS_FreeCString(ctx, code);
            }
        }
        JS_FreeValue(ctx, code_val);
    }

    /* --- maps --- */
    {
        JSValue maps_val;
        int32_t maps_len;

        maps_val = JS_GetPropertyStr(ctx, root, "maps");
        if (JS_IsArray(maps_val)) {
            JSValue length_val;

            length_val = JS_GetPropertyStr(ctx, maps_val, "length");
            JS_ToInt32(ctx, &maps_len, length_val);
            JS_FreeValue(ctx, length_val);

            cart->map_count = (maps_len > CONSOLE_MAP_SLOTS) ? CONSOLE_MAP_SLOTS : maps_len;
            for (int32_t idx = 0; idx < cart->map_count; ++idx) {
                JSValue elem;

                elem = JS_GetPropertyUint32(ctx, maps_val, (uint32_t)idx);
                if (JS_IsString(elem)) {
                    const char *path;

                    path = JS_ToCString(ctx, elem);
                    if (path != NULL) {
                        SDL_strlcpy(cart->map_paths[idx], path, sizeof(cart->map_paths[idx]));
                        JS_FreeCString(ctx, path);
                    }
                }
                JS_FreeValue(ctx, elem);
            }
        }
        JS_FreeValue(ctx, maps_val);
    }

    /* --- input.default_mappings --- */
    sub = JS_GetPropertyStr(ctx, root, "input");
    if (JS_IsObject(sub)) {
        JSValue           dm;
        JSPropertyEnum *  props;
        uint32_t          prop_count;

        dm = JS_GetPropertyStr(ctx, sub, "default_mappings");
        if (JS_IsObject(dm)) {
            if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, dm,
                                       JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                for (uint32_t pi = 0; pi < prop_count && cart->input.mapping_count <
                                                             MVN_CART_MAX_INPUT_ACTIONS;
                     ++pi) {
                    const char *                action_name;
                    JSValue                     bindings_arr;
                    mvn_cart_input_mapping_t *  mapping;

                    action_name = JS_AtomToCString(ctx, props[pi].atom);
                    if (action_name == NULL) {
                        continue;
                    }

                    bindings_arr = JS_GetProperty(ctx, dm, props[pi].atom);
                    if (!JS_IsArray(bindings_arr)) {
                        JS_FreeValue(ctx, bindings_arr);
                        JS_FreeCString(ctx, action_name);
                        continue;
                    }

                    mapping = &cart->input.mappings[cart->input.mapping_count];
                    SDL_strlcpy(mapping->action, action_name, sizeof(mapping->action));
                    mapping->bind_count = 0;

                    {
                        JSValue blen_val;
                        int32_t blen;

                        blen_val = JS_GetPropertyStr(ctx, bindings_arr, "length");
                        JS_ToInt32(ctx, &blen, blen_val);
                        JS_FreeValue(ctx, blen_val);

                        for (int32_t bi = 0;
                             bi < blen && mapping->bind_count < MVN_CART_MAX_INPUT_BINDINGS;
                             ++bi) {
                            JSValue     belem;
                            const char *bstr;

                            belem = JS_GetPropertyUint32(ctx, bindings_arr, (uint32_t)bi);
                            bstr  = JS_ToCString(ctx, belem);
                            JS_FreeValue(ctx, belem);
                            if (bstr != NULL) {
                                SDL_strlcpy(mapping->bindings[mapping->bind_count],
                                            bstr,
                                            MVN_CART_BIND_NAME_LEN);
                                mapping->bind_count++;
                                JS_FreeCString(ctx, bstr);
                            }
                        }
                    }

                    cart->input.mapping_count++;
                    JS_FreeValue(ctx, bindings_arr);
                    JS_FreeCString(ctx, action_name);
                }

                for (uint32_t pi = 0; pi < prop_count; ++pi) {
                    JS_FreeAtom(ctx, props[pi].atom);
                }
                js_free(ctx, props);
            }
        }
        JS_FreeValue(ctx, dm);
    }
    JS_FreeValue(ctx, sub);

    JS_FreeValue(ctx, root);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Validate                                                           */
/* ------------------------------------------------------------------ */

bool mvn_cart_validate(mvn_cart_t *cart)
{
    bool valid;

    valid = true;

    /* Clamp display to compiled ceilings */
    if (cart->display.width < 64 || cart->display.width > CONSOLE_FB_WIDTH) {
        SDL_Log("Cart: display.width %d out of range, clamped", cart->display.width);
        cart->display.width = CONSOLE_FB_WIDTH;
    }
    if (cart->display.height < 64 || cart->display.height > CONSOLE_FB_HEIGHT) {
        SDL_Log("Cart: display.height %d out of range, clamped", cart->display.height);
        cart->display.height = CONSOLE_FB_HEIGHT;
    }

    /* Clamp FPS */
    if (cart->timing.fps < 15) {
        cart->timing.fps = 15;
    }
    if (cart->timing.fps > CONSOLE_FPS) {
        cart->timing.fps = CONSOLE_FPS;
    }

    /* Clamp memory limits */
    if (cart->runtime.mem_limit > (uint32_t)CONSOLE_JS_MEM_MB * 1024u * 1024u) {
        cart->runtime.mem_limit = (uint32_t)CONSOLE_JS_MEM_MB * 1024u * 1024u;
    }

    /* Tile dimensions must be power of two */
    if (cart->sprites.tile_w < 4 || cart->sprites.tile_w > 64) {
        cart->sprites.tile_w = CONSOLE_TILE_W;
    }
    if (cart->sprites.tile_h < 4 || cart->sprites.tile_h > 64) {
        cart->sprites.tile_h = CONSOLE_TILE_H;
    }

    return valid;
}

/* ------------------------------------------------------------------ */
/*  File loading                                                       */
/* ------------------------------------------------------------------ */

bool mvn_cart_load(mvn_cart_t *cart, JSContext *ctx, const char *path)
{
    char * json;
    size_t len;
    bool   result;
    char   dir[512];

    /* Store base path for relative asset resolution */
    {
        const char *last_sep;
        size_t      dir_len;

        last_sep = SDL_strrchr(path, '/');
        if (last_sep == NULL) {
            last_sep = SDL_strrchr(path, '\\');
        }
        if (last_sep != NULL) {
            dir_len = (size_t)(last_sep - path);
            if (dir_len >= sizeof(dir)) {
                dir_len = sizeof(dir) - 1;
            }
            SDL_memcpy(dir, path, dir_len);
            dir[dir_len] = '\0';
        } else {
            SDL_strlcpy(dir, ".", sizeof(dir));
        }
        SDL_strlcpy(cart->base_path, dir, sizeof(cart->base_path));
    }

    /* Read JSON file */
    json = (char *)SDL_LoadFile(path, &len);
    if (json == NULL) {
        SDL_Log("Failed to load cart file: %s", path);
        return false;
    }

    result = mvn_cart_parse(cart, ctx, json, len);
    SDL_free(json);

    if (result) {
        result = mvn_cart_validate(cart);
    }

    return result;
}

/* ------------------------------------------------------------------ */
/*  Code loading                                                       */
/* ------------------------------------------------------------------ */

char *mvn_cart_load_code(mvn_cart_t *cart)
{
    char   full_path[1024];
    char * code;
    size_t len;

    if (cart->code_path[0] == '\0') {
        SDL_Log("Cart has no code path");
        return NULL;
    }

    /* Build full path relative to cart base */
    SDL_snprintf(full_path, sizeof(full_path), "%s/%s", cart->base_path, cart->code_path);

    code = (char *)SDL_LoadFile(full_path, &len);
    if (code == NULL) {
        SDL_Log("Failed to load code: %s", full_path);
    }

    return code;
}

/* ------------------------------------------------------------------ */
/*  Persistence (key-value)                                            */
/* ------------------------------------------------------------------ */

/**
 * \brief           Find a key-value entry by key
 */
static int32_t prv_find_key(mvn_cart_t *cart, const char *key)
{
    for (int32_t idx = 0; idx < cart->kv_count; ++idx) {
        if (SDL_strcmp(cart->kv_keys[idx], key) == 0) {
            return idx;
        }
    }
    return -1;
}

void mvn_cart_save(mvn_cart_t *cart, const char *key, const char *value)
{
    int32_t idx;

    idx = prv_find_key(cart, key);
    if (idx >= 0) {
        SDL_strlcpy(cart->kv_values[idx], value, sizeof(cart->kv_values[idx]));
        return;
    }

    if (cart->kv_count >= MVN_CART_MAX_KV) {
        SDL_Log("Cart persistence: max keys reached");
        return;
    }

    SDL_strlcpy(cart->kv_keys[cart->kv_count], key, sizeof(cart->kv_keys[cart->kv_count]));
    SDL_strlcpy(cart->kv_values[cart->kv_count], value, sizeof(cart->kv_values[cart->kv_count]));
    ++cart->kv_count;
}

const char *mvn_cart_load_key(mvn_cart_t *cart, const char *key)
{
    int32_t idx;

    idx = prv_find_key(cart, key);
    if (idx >= 0) {
        return cart->kv_values[idx];
    }
    return NULL;
}

bool mvn_cart_has_key(mvn_cart_t *cart, const char *key)
{
    return prv_find_key(cart, key) >= 0;
}

void mvn_cart_delete_key(mvn_cart_t *cart, const char *key)
{
    int32_t idx;

    idx = prv_find_key(cart, key);
    if (idx < 0) {
        return;
    }

    /* Compact: move last entry into removed slot */
    --cart->kv_count;
    if (idx < cart->kv_count) {
        SDL_strlcpy(cart->kv_keys[idx], cart->kv_keys[cart->kv_count], sizeof(cart->kv_keys[idx]));
        SDL_strlcpy(
            cart->kv_values[idx], cart->kv_values[cart->kv_count], sizeof(cart->kv_values[idx]));
    }
}

/* ------------------------------------------------------------------ */
/*  Data-slot persistence (dset/dget)                                  */
/* ------------------------------------------------------------------ */

void mvn_cart_dset(mvn_cart_t *cart, int32_t slot, double value)
{
    if (slot >= 0 && slot < MVN_CART_MAX_DSLOTS) {
        cart->dslots[slot] = value;
    }
}

double mvn_cart_dget(mvn_cart_t *cart, int32_t slot)
{
    if (slot >= 0 && slot < MVN_CART_MAX_DSLOTS) {
        return cart->dslots[slot];
    }
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Cleanup                                                            */
/* ------------------------------------------------------------------ */

void mvn_cart_destroy(mvn_cart_t *cart)
{
    if (cart == NULL) {
        return;
    }

    for (int32_t idx = 0; idx < cart->map_count; ++idx) {
        if (cart->maps[idx] != NULL) {
            mvn_map_level_t *level;

            level = cart->maps[idx];
            for (int32_t li = 0; li < level->layer_count; ++li) {
                MVN_FREE(level->layers[li].tiles);
                for (int32_t oi = 0; oi < level->layers[li].object_count; ++oi) {
                    /* Object name/type are inline arrays, no free needed */
                }
                MVN_FREE(level->layers[li].objects);
            }
            MVN_FREE(level->layers);
            MVN_FREE(level);
        }
    }

    MVN_FREE(cart->sprite_rgba);
    MVN_FREE(cart);
}
