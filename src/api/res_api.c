/**
 * \file            res_api.c
 * \brief           JS res namespace — string-keyed asset registry and async
 *                  loaders.
 *
 * Phase 3.1 of the res.* asset API plan. Each loader returns a Promise
 * that resolves once the underlying file has been read and decoded by
 * the per-frame pump in src/resources.c. Inspectors (has/loaded/list)
 * and the active-selector setters are synchronous.
 */

#include "../audio.h"
#include "../graphics.h"
#include "../resources.h"
#include "../runtime.h"
#include "api_common.h"

#include <SDL3/SDL.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static JSValue prv_throw_type(JSContext *ctx, const char *msg)
{
    return JS_ThrowTypeError(ctx, "%s", msg);
}

/**
 * \brief   Reject `reject_fn` immediately with a new Error and free the
 *          resolve/reject pair. Used when validation fails before the
 *          load can be enqueued.
 */
static void prv_reject_now(JSContext *ctx, JSValue resolve, JSValue reject, const char *msg)
{
    JSValue err;
    JSValue r;

    err = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, err, "message", JS_NewString(ctx, msg));
    r = JS_Call(ctx, reject, JS_UNDEFINED, 1, (JSValueConst *)&err);
    if (JS_IsException(r)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
    }
    JS_FreeValue(ctx, r);
    JS_FreeValue(ctx, err);
    JS_FreeValue(ctx, resolve);
    JS_FreeValue(ctx, reject);
}

/**
 * \brief   Common implementation of res.loadX(name, path).
 *
 * Validates the two string args, reserves a registry slot, enqueues a
 * pending request, and returns the associated Promise. On any failure
 * the promise is settled (rejected) before being returned.
 */
static JSValue prv_load_request(JSContext *ctx, int argc, JSValueConst *argv, dtr_res_kind_t kind)
{
    dtr_console_t *con;
    const char    *name;
    const char    *path;
    JSValue        resolve;
    JSValue        reject;
    JSValue        promise;
    int32_t        slot;

    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL) {
        return prv_throw_type(ctx, "res: console not initialised");
    }
    if (argc < 2) {
        return prv_throw_type(ctx, "res.load*: expected (name, path)");
    }

    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    path = JS_ToCString(ctx, argv[1]);
    if (path == NULL) {
        JS_FreeCString(ctx, name);
        return JS_EXCEPTION;
    }

    promise = dtr_runtime_make_promise(con->runtime, &resolve, &reject);
    if (JS_IsException(promise)) {
        JS_FreeCString(ctx, name);
        JS_FreeCString(ctx, path);
        return promise;
    }

    slot = dtr_res_reserve(con, con->res, name, kind);
    if (slot < 0) {
        prv_reject_now(ctx, resolve, reject, "res: registry full");
        JS_FreeCString(ctx, name);
        JS_FreeCString(ctx, path);
        return promise;
    }

    if (!dtr_res_enqueue(con->res, slot, kind, path, resolve, reject)) {
        /* Queue full — release the slot so the user can retry next frame. */
        dtr_res_unload(con, con->res, slot);
        prv_reject_now(ctx, resolve, reject, "res: load queue full");
        JS_FreeCString(ctx, name);
        JS_FreeCString(ctx, path);
        return promise;
    }

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, path);
    return promise;
}

/* ------------------------------------------------------------------ */
/*  Loaders                                                            */
/* ------------------------------------------------------------------ */

static JSValue
js_res_loadSprite(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_load_request(ctx, argc, argv, DTR_RES_SPRITE);
}

static JSValue js_res_loadMap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_load_request(ctx, argc, argv, DTR_RES_MAP);
}

static JSValue js_res_loadSfx(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_load_request(ctx, argc, argv, DTR_RES_SFX);
}

static JSValue js_res_loadMusic(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_load_request(ctx, argc, argv, DTR_RES_MUSIC);
}

static JSValue js_res_loadJson(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_load_request(ctx, argc, argv, DTR_RES_JSON);
}

static JSValue js_res_loadRaw(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_load_request(ctx, argc, argv, DTR_RES_RAW);
}

static JSValue js_res_loadHex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_load_request(ctx, argc, argv, DTR_RES_HEX);
}

/* ------------------------------------------------------------------ */
/*  Inspectors                                                         */
/* ------------------------------------------------------------------ */

static JSValue js_res_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    const char    *name;
    int32_t        slot;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL || argc < 1) {
        return JS_FALSE;
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    slot = dtr_res_find(con->res, name);
    JS_FreeCString(ctx, name);
    return JS_NewBool(ctx, slot > 0);
}

static JSValue js_res_loaded(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    const char    *name;
    int32_t        slot;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL || argc < 1) {
        return JS_FALSE;
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    slot = dtr_res_find(con->res, name);
    JS_FreeCString(ctx, name);
    if (slot <= 0) {
        return JS_FALSE;
    }
    return JS_NewBool(ctx, con->res->entries[slot].status == DTR_RES_STATUS_READY);
}

static JSValue js_res_handle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    const char    *name;
    int32_t        slot;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL || argc < 1) {
        return prv_throw_type(ctx, "res.handle: expected (name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    slot = dtr_res_find(con->res, name);
    if (slot <= 0) {
        JSValue err = JS_ThrowReferenceError(ctx, "res.handle: '%s' not registered", name);
        JS_FreeCString(ctx, name);
        return err;
    }
    JS_FreeCString(ctx, name);
    return JS_NewInt32(ctx, slot);
}

static JSValue js_res_unload(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    const char    *name;
    int32_t        slot;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL || argc < 1) {
        return JS_UNDEFINED;
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    slot = dtr_res_find(con->res, name);
    JS_FreeCString(ctx, name);
    if (slot > 0) {
        dtr_res_unload(con, con->res, slot);
    }
    return JS_UNDEFINED;
}

static JSValue js_res_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    JSValue        arr;
    uint32_t       out_idx;
    int32_t        filter_kind;
    bool           have_filter;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL) {
        return JS_NewArray(ctx);
    }

    have_filter = false;
    filter_kind = DTR_RES_NONE;
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char *kstr = JS_ToCString(ctx, argv[0]);
        if (kstr == NULL) {
            return JS_EXCEPTION;
        }
        if (SDL_strcmp(kstr, "sprite") == 0) {
            filter_kind = DTR_RES_SPRITE;
        } else if (SDL_strcmp(kstr, "map") == 0) {
            filter_kind = DTR_RES_MAP;
        } else if (SDL_strcmp(kstr, "sfx") == 0) {
            filter_kind = DTR_RES_SFX;
        } else if (SDL_strcmp(kstr, "music") == 0) {
            filter_kind = DTR_RES_MUSIC;
        } else if (SDL_strcmp(kstr, "json") == 0) {
            filter_kind = DTR_RES_JSON;
        } else if (SDL_strcmp(kstr, "raw") == 0) {
            filter_kind = DTR_RES_RAW;
        } else if (SDL_strcmp(kstr, "hex") == 0) {
            filter_kind = DTR_RES_HEX;
        }
        JS_FreeCString(ctx, kstr);
        have_filter = (filter_kind != DTR_RES_NONE);
    }

    arr     = JS_NewArray(ctx);
    out_idx = 0;
    for (int32_t i = 1; i < DTR_RES_MAX_ENTRIES; ++i) {
        const dtr_res_entry_t *e = &con->res->entries[i];
        if (e->status == DTR_RES_STATUS_FREE) {
            continue;
        }
        if (have_filter && e->kind != (dtr_res_kind_t)filter_kind) {
            continue;
        }
        JS_SetPropertyUint32(ctx, arr, out_idx++, JS_NewString(ctx, e->name));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  Active selectors                                                   */
/* ------------------------------------------------------------------ */

static JSValue
js_res_setActiveSheet(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    const char      *name;
    int32_t          slot;
    int32_t          tile_w;
    int32_t          tile_h;
    dtr_res_entry_t *e;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL || argc < 1) {
        return prv_throw_type(ctx, "res.setActiveSheet: expected (name, opts?)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    slot = dtr_res_find(con->res, name);
    if (slot <= 0 || con->res->entries[slot].status != DTR_RES_STATUS_READY ||
        con->res->entries[slot].kind != DTR_RES_SPRITE) {
        JSValue err =
            JS_ThrowReferenceError(ctx, "res.setActiveSheet: '%s' not loaded as sprite", name);
        JS_FreeCString(ctx, name);
        return err;
    }
    JS_FreeCString(ctx, name);

    tile_w = 8;
    tile_h = 8;
    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue v;

        v = JS_GetPropertyStr(ctx, argv[1], "tileW");
        if (!JS_IsUndefined(v)) {
            JS_ToInt32(ctx, &tile_w, v);
        }
        JS_FreeValue(ctx, v);
        v = JS_GetPropertyStr(ctx, argv[1], "tileH");
        if (!JS_IsUndefined(v)) {
            JS_ToInt32(ctx, &tile_h, v);
        }
        JS_FreeValue(ctx, v);
    }

    e = &con->res->entries[slot];
    if (!dtr_gfx_load_sheet(con->graphics,
                            e->payload.sprite.rgba,
                            e->payload.sprite.w,
                            e->payload.sprite.h,
                            tile_w,
                            tile_h)) {
        return JS_ThrowInternalError(ctx, "res.setActiveSheet: gfx load failed");
    }
    con->res->active_sheet_slot = slot;
    return JS_UNDEFINED;
}

static JSValue
js_res_setActiveFlags(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    const char      *name;
    int32_t          slot;
    dtr_res_entry_t *e;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL || argc < 1) {
        return prv_throw_type(ctx, "res.setActiveFlags: expected (name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    slot = dtr_res_find(con->res, name);
    if (slot <= 0 || con->res->entries[slot].status != DTR_RES_STATUS_READY ||
        con->res->entries[slot].kind != DTR_RES_HEX) {
        JSValue err =
            JS_ThrowReferenceError(ctx, "res.setActiveFlags: '%s' not loaded as hex", name);
        JS_FreeCString(ctx, name);
        return err;
    }
    JS_FreeCString(ctx, name);

    e = &con->res->entries[slot];
    if (!dtr_gfx_load_flags_hex(
            con->graphics, (const char *)e->payload.blob.data, e->payload.blob.len)) {
        return JS_ThrowInternalError(ctx, "res.setActiveFlags: parse failed");
    }
    con->res->active_flags_slot = slot;
    return JS_UNDEFINED;
}

/**
 * \brief   Parse a `.hex` palette blob: pairs of hex chars per RGB byte
 *          (so 6 chars per colour). Up to CONSOLE_PALETTE_SIZE entries.
 *
 * Whitespace and `0x` prefixes are skipped; this matches the loose
 * format used by .hex palette files in the existing examples.
 */
static bool prv_apply_palette_hex(dtr_graphics_t *gfx, const char *hex, size_t len)
{
    uint8_t  bytes[CONSOLE_PALETTE_SIZE * 3];
    uint32_t out_count;
    uint32_t nibble_count;
    uint8_t  cur;

    out_count    = 0;
    nibble_count = 0;
    cur          = 0;
    for (size_t i = 0; i < len && out_count < sizeof(bytes); ++i) {
        char c = hex[i];
        int  d;

        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else {
            continue; /* skip whitespace / separators / 'x' from 0x */
        }
        cur = (uint8_t)((cur << 4) | (uint8_t)d);
        ++nibble_count;
        if (nibble_count == 2) {
            bytes[out_count++] = cur;
            cur                = 0;
            nibble_count       = 0;
        }
    }
    if (out_count < 3) {
        return false;
    }
    {
        uint32_t entries = out_count / 3u;
        if (entries > CONSOLE_PALETTE_SIZE) {
            entries = CONSOLE_PALETTE_SIZE;
        }
        for (uint32_t i = 0; i < entries; ++i) {
            uint32_t r     = bytes[i * 3 + 0];
            uint32_t g     = bytes[i * 3 + 1];
            uint32_t b     = bytes[i * 3 + 2];
            gfx->colors[i] = (r << 24) | (g << 16) | (b << 8) | 0xFFu;
        }
    }
    return true;
}

static JSValue
js_res_setActivePalette(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t   *con;
    const char      *name;
    int32_t          slot;
    dtr_res_entry_t *e;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    if (con == NULL || con->res == NULL || argc < 1) {
        return prv_throw_type(ctx, "res.setActivePalette: expected (name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    slot = dtr_res_find(con->res, name);
    if (slot <= 0 || con->res->entries[slot].status != DTR_RES_STATUS_READY ||
        con->res->entries[slot].kind != DTR_RES_HEX) {
        JSValue err =
            JS_ThrowReferenceError(ctx, "res.setActivePalette: '%s' not loaded as hex", name);
        JS_FreeCString(ctx, name);
        return err;
    }
    JS_FreeCString(ctx, name);

    e = &con->res->entries[slot];
    if (!prv_apply_palette_hex(
            con->graphics, (const char *)e->payload.blob.data, e->payload.blob.len)) {
        return JS_ThrowInternalError(ctx, "res.setActivePalette: parse failed");
    }
    con->res->active_palette_slot = slot;
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  Bulk preload                                                       */
/* ------------------------------------------------------------------ */

/**
 * \brief   res.preload([promise, promise, ...]) → Promise.all(args).
 *
 * Sugar that lets a cart write `await res.preload([...])` without
 * importing `Promise` explicitly. Accepts an array of any thenables.
 */
static JSValue js_res_preload(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue global;
    JSValue ctor;
    JSValue all;
    JSValue ret;
    JSValue arg;

    (void)this_val;
    if (argc < 1) {
        return prv_throw_type(ctx, "res.preload: expected (promises[])");
    }
    arg    = argv[0];
    global = JS_GetGlobalObject(ctx);
    ctor   = JS_GetPropertyStr(ctx, global, "Promise");
    all    = JS_GetPropertyStr(ctx, ctor, "all");
    JS_FreeValue(ctx, global);

    ret = JS_Call(ctx, all, ctor, 1, (JSValueConst *)&arg);
    JS_FreeValue(ctx, all);
    JS_FreeValue(ctx, ctor);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  Registration                                                       */
/* ------------------------------------------------------------------ */

static const JSCFunctionListEntry js_res_funcs[] = {
    JS_CFUNC_DEF("loadSprite", 2, js_res_loadSprite),
    JS_CFUNC_DEF("loadMap", 2, js_res_loadMap),
    JS_CFUNC_DEF("loadSfx", 2, js_res_loadSfx),
    JS_CFUNC_DEF("loadMusic", 2, js_res_loadMusic),
    JS_CFUNC_DEF("loadJson", 2, js_res_loadJson),
    JS_CFUNC_DEF("loadRaw", 2, js_res_loadRaw),
    JS_CFUNC_DEF("loadHex", 2, js_res_loadHex),
    JS_CFUNC_DEF("has", 1, js_res_has),
    JS_CFUNC_DEF("isLoaded", 1, js_res_loaded),
    JS_CFUNC_DEF("handle", 1, js_res_handle),
    JS_CFUNC_DEF("unload", 1, js_res_unload),
    JS_CFUNC_DEF("list", 1, js_res_list),
    JS_CFUNC_DEF("setActiveSheet", 2, js_res_setActiveSheet),
    JS_CFUNC_DEF("setActiveFlags", 1, js_res_setActiveFlags),
    JS_CFUNC_DEF("setActivePalette", 1, js_res_setActivePalette),
    JS_CFUNC_DEF("preload", 1, js_res_preload),
};

void dtr_res_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_res_funcs, countof(js_res_funcs));
    JS_SetPropertyStr(ctx, global, "res", ns);
}
