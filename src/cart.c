/**
 * \file            cart.c
 * \brief           Cart loading, parsing, validation, and persistence
 */

#include "cart.h"

#include "runtime.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* ------------------------------------------------------------------ */
/*  Create                                                             */
/* ------------------------------------------------------------------ */

dtr_cart_t *dtr_cart_create(void)
{
    dtr_cart_t *cart;

    cart = DTR_CALLOC(1, sizeof(dtr_cart_t));
    if (cart != NULL) {
        dtr_cart_defaults(cart);
    }
    return cart;
}

/* ------------------------------------------------------------------ */
/*  Defaults                                                           */
/* ------------------------------------------------------------------ */

void dtr_cart_defaults(dtr_cart_t *cart)
{
    SDL_memset(cart, 0, sizeof(dtr_cart_t));

    /* Display defaults */
    cart->display.width      = CONSOLE_FB_WIDTH;
    cart->display.height     = CONSOLE_FB_HEIGHT;
    cart->display.scale      = CONSOLE_DEFAULT_SCALE;
    cart->display.scale_mode = 0; /* integer */
    cart->display.palette    = 0;
    cart->display.fullscreen = false;
    cart->display.pause_key  = true;

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
static void prv_json_str(JSContext  *ctx,
                         JSValue     obj,
                         const char *key,
                         char       *out,
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

bool dtr_cart_parse(dtr_cart_t *cart, JSContext *ctx, const char *json, size_t len)
{
    JSValue root;
    JSValue sub;

    dtr_cart_defaults(cart);

    root = JS_ParseJSON(ctx, json, len, "<cart>");
    if (JS_IsException(root)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        SDL_Log("Failed to parse cart.json");
        return false;
    }

    /* --- meta (flat at root level) --- */
    prv_json_str(ctx, root, "title", cart->meta.title, sizeof(cart->meta.title), "Untitled");
    prv_json_str(ctx, root, "author", cart->meta.author, sizeof(cart->meta.author), "");
    prv_json_str(ctx, root, "version", cart->meta.version, sizeof(cart->meta.version), "0.1.0");
    prv_json_str(
        ctx, root, "description", cart->meta.description, sizeof(cart->meta.description), "");
    prv_json_str(ctx, root, "id", cart->meta.id, sizeof(cart->meta.id), "");

    /* --- display --- */
    sub = JS_GetPropertyStr(ctx, root, "display");
    if (JS_IsObject(sub)) {
        cart->display.width      = prv_json_int(ctx, sub, "width", CONSOLE_FB_WIDTH);
        cart->display.height     = prv_json_int(ctx, sub, "height", CONSOLE_FB_HEIGHT);
        cart->display.scale      = prv_json_int(ctx, sub, "scale", CONSOLE_DEFAULT_SCALE);
        cart->display.palette    = prv_json_int(ctx, sub, "palette", 0);
        cart->display.fullscreen = prv_json_bool(ctx, sub, "fullscreen", false);
        cart->display.pause_key  = prv_json_bool(ctx, sub, "pauseKey", true);

        /* scaleMode: "integer" (default), "letterbox", "stretch", "overscan" */
        {
            char mode_str[32] = "";
            prv_json_str(ctx, sub, "scaleMode", mode_str, sizeof(mode_str), "integer");
            if (SDL_strcmp(mode_str, "letterbox") == 0)
                cart->display.scale_mode = 1;
            else if (SDL_strcmp(mode_str, "stretch") == 0)
                cart->display.scale_mode = 2;
            else if (SDL_strcmp(mode_str, "overscan") == 0)
                cart->display.scale_mode = 3;
            else
                cart->display.scale_mode = 0;
        }
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

        cart->sprites.tile_w  = prv_json_int(ctx, sub, "tileW", CONSOLE_TILE_W);
        cart->sprites.tile_h  = prv_json_int(ctx, sub, "tileH", CONSOLE_TILE_H);
        cart->sprites.sheet_w = prv_json_int(ctx, sub, "sheetW", 128);
        cart->sprites.sheet_h = prv_json_int(ctx, sub, "sheetH", 128);

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

        {
            JSValue flags_val;

            flags_val = JS_GetPropertyStr(ctx, sub, "flags");
            if (JS_IsString(flags_val)) {
                const char *path;

                path = JS_ToCString(ctx, flags_val);
                if (path != NULL) {
                    SDL_strlcpy(cart->sprite_flags_path, path, sizeof(cart->sprite_flags_path));
                    JS_FreeCString(ctx, path);
                }
            }
            JS_FreeValue(ctx, flags_val);
        }
    }
    JS_FreeValue(ctx, sub);

    /* --- runtime --- */
    sub = JS_GetPropertyStr(ctx, root, "runtime");
    if (JS_IsObject(sub)) {
        int32_t mem_mb;
        int32_t stack_kb;

        mem_mb   = prv_json_int(ctx, sub, "memoryLimitMB", CONSOLE_JS_MEM_MB);
        stack_kb = prv_json_int(ctx, sub, "stackLimitKB", CONSOLE_JS_STACK_KB);
        if (mem_mb < 1) {
            mem_mb = CONSOLE_JS_MEM_MB;
        }
        if (stack_kb < 1) {
            stack_kb = CONSOLE_JS_STACK_KB;
        }
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

    /* --- sfx (array of file paths) --- */
    {
        JSValue arr;

        arr = JS_GetPropertyStr(ctx, root, "sfx");
        if (JS_IsArray(arr)) {
            JSValue len_val;
            int32_t arr_len;

            len_val = JS_GetPropertyStr(ctx, arr, "length");
            JS_ToInt32(ctx, &arr_len, len_val);
            JS_FreeValue(ctx, len_val);

            cart->sfx_count = (arr_len > DTR_CART_MAX_SFX) ? DTR_CART_MAX_SFX : arr_len;
            for (int32_t idx = 0; idx < cart->sfx_count; ++idx) {
                JSValue elem;

                elem = JS_GetPropertyUint32(ctx, arr, (uint32_t)idx);
                if (JS_IsString(elem)) {
                    const char *str;

                    str = JS_ToCString(ctx, elem);
                    if (str != NULL) {
                        SDL_strlcpy(cart->sfx_paths[idx], str, sizeof(cart->sfx_paths[idx]));
                        JS_FreeCString(ctx, str);
                    }
                }
                JS_FreeValue(ctx, elem);
            }
        }
        JS_FreeValue(ctx, arr);
    }

    /* --- music (array of file paths) --- */
    {
        JSValue arr;

        arr = JS_GetPropertyStr(ctx, root, "music");
        if (JS_IsArray(arr)) {
            JSValue len_val;
            int32_t arr_len;

            len_val = JS_GetPropertyStr(ctx, arr, "length");
            JS_ToInt32(ctx, &arr_len, len_val);
            JS_FreeValue(ctx, len_val);

            cart->music_count = (arr_len > DTR_CART_MAX_MUSIC) ? DTR_CART_MAX_MUSIC : arr_len;
            for (int32_t idx = 0; idx < cart->music_count; ++idx) {
                JSValue elem;

                elem = JS_GetPropertyUint32(ctx, arr, (uint32_t)idx);
                if (JS_IsString(elem)) {
                    const char *str;

                    str = JS_ToCString(ctx, elem);
                    if (str != NULL) {
                        SDL_strlcpy(cart->music_paths[idx], str, sizeof(cart->music_paths[idx]));
                        JS_FreeCString(ctx, str);
                    }
                }
                JS_FreeValue(ctx, elem);
            }
        }
        JS_FreeValue(ctx, arr);
    }

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

    /* --- buildCommand (optional, DEV_BUILD only) --- */
    {
        JSValue build_val;

        build_val = JS_GetPropertyStr(ctx, root, "buildCommand");
        if (JS_IsString(build_val)) {
            const char *cmd;

            cmd = JS_ToCString(ctx, build_val);
            if (cmd != NULL) {
                SDL_strlcpy(cart->build_command, cmd, sizeof(cart->build_command));
                JS_FreeCString(ctx, cmd);
            }
        }
        JS_FreeValue(ctx, build_val);
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
        JSValue         dm;
        JSPropertyEnum *props;
        uint32_t        prop_count;

        dm = JS_GetPropertyStr(ctx, sub, "default_mappings");
        if (JS_IsObject(dm)) {
            if (JS_GetOwnPropertyNames(
                    ctx, &props, &prop_count, dm, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                for (uint32_t pi = 0;
                     pi < prop_count && cart->input.mapping_count < DTR_CART_MAX_INPUT_ACTIONS;
                     ++pi) {
                    const char               *action_name;
                    JSValue                   bindings_arr;
                    dtr_cart_input_mapping_t *mapping;

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
                             bi < blen && mapping->bind_count < DTR_CART_MAX_INPUT_BINDINGS;
                             ++bi) {
                            JSValue     belem;
                            const char *bstr;

                            belem = JS_GetPropertyUint32(ctx, bindings_arr, (uint32_t)bi);
                            bstr  = JS_ToCString(ctx, belem);
                            JS_FreeValue(ctx, belem);
                            if (bstr != NULL) {
                                SDL_strlcpy(mapping->bindings[mapping->bind_count],
                                            bstr,
                                            DTR_CART_BIND_NAME_LEN);
                                mapping->bind_count++;
                                JS_FreeCString(ctx, bstr);
                            }
                        }
                    }

                    cart->input.mapping_count++;
                    JS_FreeValue(ctx, bindings_arr);
                    JS_FreeCString(ctx, action_name);
                }

                if (prop_count > DTR_CART_MAX_INPUT_ACTIONS) {
                    SDL_Log("Warning: cart defines %u input actions, max is %d — "
                            "excess mappings ignored",
                            (unsigned)prop_count,
                            DTR_CART_MAX_INPUT_ACTIONS);
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

bool dtr_cart_validate(dtr_cart_t *cart)
{
    bool valid;

    valid = true;

    /* Clamp display to compiled ceilings */
    if (cart->display.width < 64 || cart->display.width > CONSOLE_FB_MAX_WIDTH) {
        SDL_Log("Cart: display.width %d out of range, clamped", cart->display.width);
        cart->display.width = CONSOLE_FB_WIDTH;
        valid               = false;
    }
    if (cart->display.height < 64 || cart->display.height > CONSOLE_FB_MAX_HEIGHT) {
        SDL_Log("Cart: display.height %d out of range, clamped", cart->display.height);
        cart->display.height = CONSOLE_FB_HEIGHT;
        valid                = false;
    }

    /* Scale must be at least 1 */
    if (cart->display.scale < 1 || cart->display.scale > 8) {
        SDL_Log("Cart: display.scale %d out of range, clamped", cart->display.scale);
        cart->display.scale = CONSOLE_DEFAULT_SCALE;
        valid               = false;
    }

    /* Clamp FPS */
    if (cart->timing.fps < 15) {
        cart->timing.fps = 15;
        valid            = false;
    }
    if (cart->timing.fps > CONSOLE_FPS) {
        cart->timing.fps = CONSOLE_FPS;
        valid            = false;
    }

    /* Clamp UPS */
    if (cart->timing.ups < 15) {
        cart->timing.ups = 15;
        valid            = false;
    }
    if (cart->timing.ups > 240) {
        cart->timing.ups = 240;
        valid            = false;
    }

    /* Audio channels */
    if (cart->audio.channels < 0 || cart->audio.channels > CONSOLE_MAX_CHANNELS) {
        SDL_Log("Cart: audio.channels %d out of range, clamped", cart->audio.channels);
        cart->audio.channels = CONSOLE_AUDIO_CHANNELS;
        valid                = false;
    }

    /* Audio frequency */
    if (cart->audio.frequency < 8000 || cart->audio.frequency > 96000) {
        SDL_Log("Cart: audio.frequency %d out of range, clamped", cart->audio.frequency);
        cart->audio.frequency = CONSOLE_AUDIO_FREQ;
        valid                 = false;
    }

    /* Audio buffer size */
    if (cart->audio.buffer_size < 256 || cart->audio.buffer_size > 8192) {
        SDL_Log("Cart: audio.buffer_size %d out of range, clamped", cart->audio.buffer_size);
        cart->audio.buffer_size = CONSOLE_AUDIO_BUFFER;
        valid                   = false;
    }

    /* Clamp memory limits */
    if (cart->runtime.mem_limit > (uint32_t)CONSOLE_JS_MEM_MB * 1024u * 1024u) {
        cart->runtime.mem_limit = (uint32_t)CONSOLE_JS_MEM_MB * 1024u * 1024u;
        valid                   = false;
    }

    /* Stack limit */
    if (cart->runtime.stack_limit > (uint32_t)CONSOLE_JS_STACK_KB * 1024u) {
        cart->runtime.stack_limit = (uint32_t)CONSOLE_JS_STACK_KB * 1024u;
        valid                     = false;
    }

    /* Tile dimensions must be in range 4-64 */
    if (cart->sprites.tile_w < 4 || cart->sprites.tile_w > 64) {
        cart->sprites.tile_w = CONSOLE_TILE_W;
        valid                = false;
    }
    if (cart->sprites.tile_h < 4 || cart->sprites.tile_h > 64) {
        cart->sprites.tile_h = CONSOLE_TILE_H;
        valid                = false;
    }

    return valid;
}

/* ------------------------------------------------------------------ */
/*  File loading                                                       */
/* ------------------------------------------------------------------ */

bool dtr_cart_load(dtr_cart_t *cart, JSContext *ctx, const char *path)
{
    char  *json;
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

    result = dtr_cart_parse(cart, ctx, json, len);
    SDL_free(json);

    if (result) {
        result = dtr_cart_validate(cart);
    }

    return result;
}

/* ------------------------------------------------------------------ */
/*  Code loading                                                       */
/* ------------------------------------------------------------------ */

char *dtr_cart_load_code(dtr_cart_t *cart)
{
    char   full_path[1024];
    char  *code;
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
static int32_t prv_find_key(dtr_cart_t *cart, const char *key)
{
    for (int32_t idx = 0; idx < cart->kv_count; ++idx) {
        if (SDL_strcmp(cart->kv_keys[idx], key) == 0) {
            return idx;
        }
    }
    return -1;
}

void dtr_cart_save(dtr_cart_t *cart, const char *key, const char *value)
{
    int32_t idx;

    idx = prv_find_key(cart, key);
    if (idx >= 0) {
        SDL_strlcpy(cart->kv_values[idx], value, sizeof(cart->kv_values[idx]));
        return;
    }

    if (cart->kv_count >= DTR_CART_MAX_KV) {
        SDL_Log("Cart persistence: max keys reached");
        return;
    }

    SDL_strlcpy(cart->kv_keys[cart->kv_count], key, sizeof(cart->kv_keys[cart->kv_count]));
    SDL_strlcpy(cart->kv_values[cart->kv_count], value, sizeof(cart->kv_values[cart->kv_count]));
    ++cart->kv_count;
}

const char *dtr_cart_load_key(dtr_cart_t *cart, const char *key)
{
    int32_t idx;

    idx = prv_find_key(cart, key);
    if (idx >= 0) {
        return cart->kv_values[idx];
    }
    return NULL;
}

bool dtr_cart_has_key(dtr_cart_t *cart, const char *key)
{
    return prv_find_key(cart, key) >= 0;
}

void dtr_cart_delete_key(dtr_cart_t *cart, const char *key)
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

void dtr_cart_dset(dtr_cart_t *cart, int32_t slot, double value)
{
    if (slot >= 0 && slot < DTR_CART_MAX_DSLOTS) {
        cart->dslots[slot] = value;
    }
}

double dtr_cart_dget(dtr_cart_t *cart, int32_t slot)
{
    if (slot >= 0 && slot < DTR_CART_MAX_DSLOTS) {
        return cart->dslots[slot];
    }
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Disk persistence — JSON file                                       */
/* ------------------------------------------------------------------ */

/**
 * \brief           Build the save-file path from the cart title
 *
 * Stores under SDL_GetPrefPath("dithr", <title>)/save.json.
 */
static bool prv_save_path(dtr_cart_t *cart, char *out, size_t out_sz)
{
    const char *title;
    char       *pref;

    out[0] = '\0';

    title = cart->meta.title;
    if (title[0] == '\0') {
        title = "default";
    }

    pref = SDL_GetPrefPath("dithr", title);
    if (pref == NULL) {
        return false;
    }

    SDL_snprintf(out, out_sz, "%ssave.json", pref);
    SDL_free(pref);
    return true;
}

bool dtr_cart_persist_save(dtr_cart_t *cart)
{
    char    path[512];
    char   *buf;
    size_t  cap;
    size_t  pos;
    int32_t idx;
    bool    first;

    if (!prv_save_path(cart, path, sizeof(path))) {
        return false;
    }

    /* Conservative upper bound: 64 slots * 30 chars + 64 kv * 350 chars + overhead */
    cap = 64 * 30 + 64 * 350 + 256;
    buf = DTR_MALLOC(cap);
    if (buf == NULL) {
        return false;
    }

    pos = 0;
    pos += (size_t)SDL_snprintf(buf + pos, cap - pos, "{\n\"dslots\":[");

    /* Write dslots array */
    for (idx = 0; idx < DTR_CART_MAX_DSLOTS; ++idx) {
        if (idx > 0) {
            pos += (size_t)SDL_snprintf(buf + pos, cap - pos, ",");
        }
        pos += (size_t)SDL_snprintf(buf + pos, cap - pos, "%.17g", cart->dslots[idx]);
    }
    pos += (size_t)SDL_snprintf(buf + pos, cap - pos, "],\n\"kv\":{");

    /* Write kv object */
    first = true;
    for (idx = 0; idx < cart->kv_count; ++idx) {
        if (!first) {
            pos += (size_t)SDL_snprintf(buf + pos, cap - pos, ",");
        }
        first = false;
        pos += (size_t)SDL_snprintf(
            buf + pos, cap - pos, "\"%s\":\"%s\"", cart->kv_keys[idx], cart->kv_values[idx]);
    }
    pos += (size_t)SDL_snprintf(buf + pos, cap - pos, "}\n}");

    /* Write file */
    {
        bool ok;

        ok = SDL_SaveFile(path, buf, pos);
        if (!ok) {
            SDL_Log("persist: failed to write %s: %s", path, SDL_GetError());
        }
        DTR_FREE(buf);

#ifdef __EMSCRIPTEN__
        /* Flush virtual FS to IndexedDB so data survives tab close */
        if (ok) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
            EM_ASM(FS.syncfs(
                false, function(err) {
                    if (err)
                        console.warn('IDBFS sync failed:', err);
                }););
#pragma clang diagnostic pop
        }
#endif

        return ok;
    }
}

bool dtr_cart_persist_load(dtr_cart_t *cart)
{
    char       path[512];
    char      *json;
    size_t     len;
    JSContext *ctx;
    JSRuntime *jrt;
    JSValue    root;
    JSValue    dslots;
    JSValue    kv_obj;

    if (!prv_save_path(cart, path, sizeof(path))) {
        return false;
    }

    json = (char *)SDL_LoadFile(path, &len);
    if (json == NULL) {
        /* No save file — not an error */
        return true;
    }

    /* Use a temporary JS context for JSON parsing */
    jrt = JS_NewRuntime();
    ctx = JS_NewContext(jrt);

    root = JS_ParseJSON(ctx, json, len, "<save>");
    SDL_free(json);

    if (JS_IsException(root)) {
        SDL_Log("persist: failed to parse %s", path);
        JS_FreeContext(ctx);
        JS_FreeRuntime(jrt);
        return false;
    }

    /* dslots array */
    dslots = JS_GetPropertyStr(ctx, root, "dslots");
    if (JS_IsArray(dslots)) {
        for (int32_t idx = 0; idx < DTR_CART_MAX_DSLOTS; ++idx) {
            JSValue val;
            double  dbl;

            val = JS_GetPropertyUint32(ctx, dslots, (uint32_t)idx);
            if (JS_ToFloat64(ctx, &dbl, val) == 0) {
                cart->dslots[idx] = dbl;
            }
            JS_FreeValue(ctx, val);
        }
    }
    JS_FreeValue(ctx, dslots);

    /* kv object */
    kv_obj = JS_GetPropertyStr(ctx, root, "kv");
    if (JS_IsObject(kv_obj)) {
        JSPropertyEnum *props;
        uint32_t        prop_count;

        if (JS_GetOwnPropertyNames(
                ctx, &props, &prop_count, kv_obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
            for (uint32_t pi = 0; pi < prop_count && cart->kv_count < DTR_CART_MAX_KV; ++pi) {
                const char *key;
                JSValue     val;
                const char *str;

                key = JS_AtomToCString(ctx, props[pi].atom);
                val = JS_GetProperty(ctx, kv_obj, props[pi].atom);
                str = JS_ToCString(ctx, val);

                if (key != NULL && str != NULL) {
                    SDL_strlcpy(
                        cart->kv_keys[cart->kv_count], key, sizeof(cart->kv_keys[cart->kv_count]));
                    SDL_strlcpy(cart->kv_values[cart->kv_count],
                                str,
                                sizeof(cart->kv_values[cart->kv_count]));
                    ++cart->kv_count;
                }

                if (str != NULL) {
                    JS_FreeCString(ctx, str);
                }
                JS_FreeValue(ctx, val);
                if (key != NULL) {
                    JS_FreeCString(ctx, key);
                }
            }

            for (uint32_t pi = 0; pi < prop_count; ++pi) {
                JS_FreeAtom(ctx, props[pi].atom);
            }
            js_free(ctx, props);
        }
    }
    JS_FreeValue(ctx, kv_obj);

    JS_FreeValue(ctx, root);
    JS_FreeContext(ctx);
    JS_FreeRuntime(jrt);

    SDL_Log("persist: loaded %s (%d kv entries)", path, cart->kv_count);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Cleanup                                                            */
/* ------------------------------------------------------------------ */

void dtr_cart_free_map(dtr_cart_t *cart, int32_t idx)
{
    dtr_map_level_t *level;

    if (cart == NULL || idx < 0 || idx >= cart->map_count) {
        return;
    }
    level = cart->maps[idx];
    if (level == NULL) {
        return;
    }
    for (int32_t li = 0; li < level->layer_count; ++li) {
        DTR_FREE(level->layers[li].tiles);
        for (int32_t oi = 0; oi < level->layers[li].object_count; ++oi) {
            dtr_map_object_t *obj = &level->layers[li].objects[oi];
            if (cart->ctx != NULL && !JS_IsUndefined(obj->props)) {
                JS_FreeValue(cart->ctx, obj->props);
            }
        }
        DTR_FREE(level->layers[li].objects);
    }
    DTR_FREE(level->layers);
    DTR_FREE(level);
    cart->maps[idx] = NULL;
}

void dtr_cart_destroy(dtr_cart_t *cart)
{
    if (cart == NULL) {
        return;
    }

    for (int32_t idx = 0; idx < cart->map_count; ++idx) {
        dtr_cart_free_map(cart, idx);
    }

    DTR_FREE(cart->sprite_rgba);
    DTR_FREE(cart->code);
    DTR_FREE(cart);
}
