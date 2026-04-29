/**
 * \file            resources.c
 * \brief           Resource registry, async load queue, and decode dispatch.
 *
 * Phase 2 of the res.* asset API. JS-visible bindings live in
 * src/api/res_api.c (Phase 3); this file is the engine-facing core.
 *
 * Threading model: single-threaded. dtr_res_pump() processes a bounded
 * number of pending requests per frame, calling SDL_LoadFile (native +
 * WASM bundled paths under /cart/) or emscripten_fetch (WASM dynamic
 * paths) and decoding via the existing cart_import.c / audio.c helpers.
 */

#include "resources.h"

#include "audio.h"
#include "cart.h"
#include "cart_import.h"
#include "console.h"
#include "runtime.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif

#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static void prv_free_payload(JSContext *ctx, dtr_res_entry_t *e)
{
    switch (e->kind) {
        case DTR_RES_SPRITE:
            if (e->payload.sprite.rgba != NULL) {
                DTR_FREE(e->payload.sprite.rgba);
                e->payload.sprite.rgba = NULL;
            }
            break;
        case DTR_RES_MAP:
            if (e->payload.map != NULL) {
                /* Mirror dtr_cart_free_map(): walk layers, free tiles + objects. */
                struct dtr_map_level *m = e->payload.map;
                for (int32_t li = 0; li < m->layer_count; ++li) {
                    dtr_map_layer_t *layer = &m->layers[li];
                    if (layer->tiles != NULL) {
                        DTR_FREE(layer->tiles);
                    }
                    for (int32_t oi = 0; oi < layer->object_count; ++oi) {
                        if (ctx != NULL) {
                            JS_FreeValue(ctx, layer->objects[oi].props);
                        }
                    }
                    if (layer->objects != NULL) {
                        DTR_FREE(layer->objects);
                    }
                }
                if (m->layers != NULL) {
                    DTR_FREE(m->layers);
                }
                DTR_FREE(m);
                e->payload.map = NULL;
            }
            break;
        case DTR_RES_JSON:
            if (ctx != NULL) {
                JS_FreeValue(ctx, e->payload.json);
            }
            e->payload.json = JS_UNDEFINED;
            break;
        case DTR_RES_RAW:
        case DTR_RES_HEX:
            if (e->payload.blob.data != NULL) {
                DTR_FREE(e->payload.blob.data);
                e->payload.blob.data = NULL;
                e->payload.blob.len  = 0;
            }
            break;
        case DTR_RES_SFX:
        case DTR_RES_MUSIC:
            /* Backing audio data lives in dtr_audio_t and is freed when the
               audio subsystem is destroyed. The slot is a non-owning index. */
            e->payload.audio_slot = -1;
            break;
        case DTR_RES_NONE:
            break;
    }
}

/**
 * \brief   Resolve a cart-relative path against cart->base_path with the
 *          same safety checks used by the legacy loader.
 *
 * Returns true on success; out_full receives the resolved path.
 */
static bool
prv_resolve_cart_path(dtr_console_t *con, const char *rel, char *out_full, size_t out_size)
{
    char       *pos;
    size_t      base_len;
    const char *base;

    if (rel == NULL || rel[0] == '\0') {
        return false;
    }
    if (rel[0] == '/' || rel[0] == '\\') {
        return false; /* No absolute paths. */
    }
    if (SDL_strstr(rel, "..") != NULL) {
        return false; /* No traversal. */
    }

    base = con->cart->base_path;
    if (base == NULL || base[0] == '\0') {
        SDL_strlcpy(out_full, rel, out_size);
    } else {
        base_len = SDL_strlen(base);
        if (base_len > 0 && (base[base_len - 1] == '/' || base[base_len - 1] == '\\')) {
            SDL_snprintf(out_full, out_size, "%s%s", base, rel);
        } else {
            SDL_snprintf(out_full, out_size, "%s/%s", base, rel);
        }
    }

    /* Normalise backslashes for downstream consumers. */
    for (pos = out_full; *pos != '\0'; ++pos) {
        if (*pos == '\\') {
            *pos = '/';
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

dtr_res_t *dtr_res_create(void)
{
    dtr_res_t *res;

    res = (dtr_res_t *)DTR_CALLOC(1, sizeof(dtr_res_t));
    if (res == NULL) {
        return NULL;
    }

    /* Slot 0 reserved as the "invalid handle" sentinel. */
    SDL_strlcpy(res->entries[0].name, "", DTR_RES_NAME_LEN);
    res->entries[0].status = DTR_RES_STATUS_FREE;

    for (int32_t i = 0; i < DTR_RES_MAX_PENDING; ++i) {
        res->pending[i].slot       = -1;
        res->pending[i].resolve    = JS_UNDEFINED;
        res->pending[i].reject     = JS_UNDEFINED;
        res->pending[i].dispatched = false;
    }

    res->active_sheet_slot   = -1;
    res->active_palette_slot = -1;
    res->active_flags_slot   = -1;
    res->active_map_slot     = -1;
    return res;
}

void dtr_res_destroy(dtr_console_t *con, dtr_res_t *res)
{
    JSContext *ctx;

    if (res == NULL) {
        return;
    }

    ctx = (con != NULL && con->runtime != NULL) ? con->runtime->ctx : NULL;

    /* Cancel any pending requests — release their JSValues. */
    for (int32_t i = 0; i < DTR_RES_MAX_PENDING; ++i) {
        dtr_res_request_t *r = &res->pending[i];
        if (r->slot < 0) {
            continue;
        }
#ifdef __EMSCRIPTEN__
        if (r->fetch != NULL) {
            emscripten_fetch_close((emscripten_fetch_t *)r->fetch);
            r->fetch = NULL;
        }
#endif
        if (ctx != NULL) {
            JS_FreeValue(ctx, r->resolve);
            JS_FreeValue(ctx, r->reject);
        }
        r->slot = -1;
    }

    /* Free all payloads. */
    for (int32_t i = 0; i < DTR_RES_MAX_ENTRIES; ++i) {
        dtr_res_entry_t *e = &res->entries[i];
        if (e->status == DTR_RES_STATUS_FREE) {
            continue;
        }
        prv_free_payload(ctx, e);
    }

    DTR_FREE(res);
}

/* ------------------------------------------------------------------ */
/*  Registry                                                           */
/* ------------------------------------------------------------------ */

int32_t dtr_res_find(const dtr_res_t *res, const char *name)
{
    if (res == NULL || name == NULL || name[0] == '\0') {
        return -1;
    }
    for (int32_t i = 1; i < DTR_RES_MAX_ENTRIES; ++i) {
        if (res->entries[i].status == DTR_RES_STATUS_FREE) {
            continue;
        }
        if (SDL_strcmp(res->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int32_t dtr_res_reserve(dtr_console_t *con, dtr_res_t *res, const char *name, dtr_res_kind_t kind)
{
    int32_t    slot;
    JSContext *ctx;

    if (res == NULL || name == NULL || name[0] == '\0') {
        return -1;
    }
    ctx = (con != NULL && con->runtime != NULL) ? con->runtime->ctx : NULL;

    /* Replace existing entry of the same name. */
    slot = dtr_res_find(res, name);
    if (slot >= 0) {
        prv_free_payload(ctx, &res->entries[slot]);
    } else {
        for (int32_t i = 1; i < DTR_RES_MAX_ENTRIES; ++i) {
            if (res->entries[i].status == DTR_RES_STATUS_FREE) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            return -1;
        }
    }

    SDL_strlcpy(res->entries[slot].name, name, DTR_RES_NAME_LEN);
    res->entries[slot].kind   = kind;
    res->entries[slot].status = DTR_RES_STATUS_PENDING;
    SDL_memset(&res->entries[slot].payload, 0, sizeof(res->entries[slot].payload));
    if (kind == DTR_RES_JSON) {
        res->entries[slot].payload.json = JS_UNDEFINED;
    } else if (kind == DTR_RES_SFX || kind == DTR_RES_MUSIC) {
        res->entries[slot].payload.audio_slot = -1;
    }
    return slot;
}

void dtr_res_unload(dtr_console_t *con, dtr_res_t *res, int32_t slot)
{
    JSContext *ctx;

    if (res == NULL || slot <= 0 || slot >= DTR_RES_MAX_ENTRIES) {
        return;
    }
    if (res->entries[slot].status == DTR_RES_STATUS_FREE) {
        return;
    }

    ctx = (con != NULL && con->runtime != NULL) ? con->runtime->ctx : NULL;
    prv_free_payload(ctx, &res->entries[slot]);
    res->entries[slot].name[0] = '\0';
    res->entries[slot].kind    = DTR_RES_NONE;
    res->entries[slot].status  = DTR_RES_STATUS_FREE;

    /* Clear active selectors that point at this slot. */
    if (res->active_sheet_slot == slot) {
        res->active_sheet_slot = -1;
    }
    if (res->active_palette_slot == slot) {
        res->active_palette_slot = -1;
    }
    if (res->active_flags_slot == slot) {
        res->active_flags_slot = -1;
    }
    if (res->active_map_slot == slot) {
        res->active_map_slot = -1;
    }
}

int32_t dtr_res_resolve_handle(dtr_console_t *con, JSValueConst v, dtr_res_kind_t kind)
{
    dtr_res_t  *res;
    JSContext  *ctx;
    int32_t     slot;
    const char *name;

    if (con == NULL) {
        return -1;
    }
    res = NULL;
    /* Defensive: console may not yet have a registry attached. */
    /* Fetched via opaque field added by Phase 2.5 console wiring. */
    /* See console.h: dtr_console_t::res */
    res = ((dtr_console_t *)con)->res;
    if (res == NULL) {
        return -1;
    }

    ctx = con->runtime->ctx;
    if (JS_IsNumber(v)) {
        if (JS_ToInt32(ctx, &slot, v) != 0) {
            return -1;
        }
        if (slot <= 0 || slot >= DTR_RES_MAX_ENTRIES) {
            return -1;
        }
        if (res->entries[slot].status != DTR_RES_STATUS_READY) {
            return -1;
        }
        if (res->entries[slot].kind != kind) {
            return -1;
        }
        return slot;
    }

    name = JS_ToCString(ctx, v);
    if (name == NULL) {
        return -1;
    }
    slot = dtr_res_find(res, name);
    JS_FreeCString(ctx, name);
    if (slot < 0) {
        return -1;
    }
    if (res->entries[slot].status != DTR_RES_STATUS_READY) {
        return -1;
    }
    if (res->entries[slot].kind != kind) {
        return -1;
    }
    return slot;
}

/* ------------------------------------------------------------------ */
/*  Async queue                                                        */
/* ------------------------------------------------------------------ */

bool dtr_res_enqueue(dtr_res_t     *res,
                     int32_t        slot,
                     dtr_res_kind_t kind,
                     const char    *path,
                     JSValue        resolve,
                     JSValue        reject)
{
    if (res == NULL || res->pending_count >= DTR_RES_MAX_PENDING) {
        return false;
    }
    if (slot <= 0 || slot >= DTR_RES_MAX_ENTRIES) {
        return false;
    }
    for (int32_t i = 0; i < DTR_RES_MAX_PENDING; ++i) {
        if (res->pending[i].slot >= 0) {
            continue;
        }
        res->pending[i].slot       = slot;
        res->pending[i].kind       = kind;
        res->pending[i].dispatched = false;
        SDL_strlcpy(res->pending[i].path, path != NULL ? path : "", DTR_RES_PATH_LEN);
        res->pending[i].resolve = resolve;
        res->pending[i].reject  = reject;
#ifdef __EMSCRIPTEN__
        res->pending[i].fetch = NULL;
#endif
        ++res->pending_count;
        return true;
    }
    return false;
}

/* ---------- Decode helpers ---------- */

static uint8_t *prv_decode_png_mem(const uint8_t *data, size_t len, int32_t *out_w, int32_t *out_h)
{
    SDL_IOStream *io;
    SDL_Surface  *surface;
    SDL_Surface  *rgba;
    uint8_t      *pixels;
    size_t        px_size;

    *out_w = 0;
    *out_h = 0;

    io = SDL_IOFromConstMem(data, len);
    if (io == NULL) {
        return NULL;
    }
    surface = IMG_Load_IO(io, true); /* closeio=true */
    if (surface == NULL) {
        return NULL;
    }
    rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (rgba == NULL) {
        return NULL;
    }

    *out_w  = rgba->w;
    *out_h  = rgba->h;
    px_size = (size_t)rgba->w * (size_t)rgba->h * 4u;
    pixels  = (uint8_t *)DTR_MALLOC(px_size);
    if (pixels == NULL) {
        SDL_DestroySurface(rgba);
        return NULL;
    }
    SDL_memcpy(pixels, rgba->pixels, px_size);
    SDL_DestroySurface(rgba);
    return pixels;
}

static bool
prv_decode_request(dtr_console_t *con, dtr_res_request_t *r, const uint8_t *bytes, size_t len)
{
    dtr_res_t       *res;
    dtr_res_entry_t *e;
    JSContext       *ctx;

    res = con->res;
    e   = &res->entries[r->slot];
    ctx = con->runtime->ctx;

    switch (r->kind) {
        case DTR_RES_SPRITE: {
            int32_t  w;
            int32_t  h;
            uint8_t *rgba = prv_decode_png_mem(bytes, len, &w, &h);
            if (rgba == NULL) {
                return false;
            }
            e->payload.sprite.rgba = rgba;
            e->payload.sprite.w    = w;
            e->payload.sprite.h    = h;
            return true;
        }
        case DTR_RES_MAP: {
            /* Detect TMJ vs LDtk by extension on the path. */
            const char           *ext   = SDL_strrchr(r->path, '.');
            struct dtr_map_level *level = NULL;
            bool                  ok;
            char                  full[DTR_RES_PATH_LEN];

            /* Importers take a path; resolve via base_path then call. */
            if (!prv_resolve_cart_path(con, r->path, full, sizeof(full))) {
                return false;
            }
            if (ext != NULL && SDL_strcasecmp(ext, ".ldtk") == 0) {
                ok = dtr_import_ldtk(full, &level, ctx);
            } else {
                ok = dtr_import_tiled(full, &level, ctx);
            }
            if (!ok || level == NULL) {
                return false;
            }
            e->payload.map = level;
            return true;
        }
        case DTR_RES_SFX: {
            if (!dtr_audio_load_sfx(con->audio, bytes, len)) {
                return false;
            }
            e->payload.audio_slot = con->audio->sfx_count - 1;
            return true;
        }
        case DTR_RES_MUSIC: {
            if (!dtr_audio_load_music(con->audio, bytes, len)) {
                return false;
            }
            e->payload.audio_slot = con->audio->music_count - 1;
            return true;
        }
        case DTR_RES_JSON: {
            JSValue v = JS_ParseJSON(ctx, (const char *)bytes, len, "<res:json>");
            if (JS_IsException(v)) {
                JS_FreeValue(ctx, JS_GetException(ctx));
                return false;
            }
            e->payload.json = v;
            return true;
        }
        case DTR_RES_RAW:
        case DTR_RES_HEX: {
            uint8_t *copy = (uint8_t *)DTR_MALLOC(len);
            if (copy == NULL) {
                return false;
            }
            SDL_memcpy(copy, bytes, len);
            e->payload.blob.data = copy;
            e->payload.blob.len  = len;
            return true;
        }
        case DTR_RES_NONE:
            break;
    }
    return false;
}

/**
 * \brief   Build the JSValue that resolves a load request, depending on
 *          the resource kind. Caller must free the returned value.
 */
static JSValue prv_make_resolve_value(JSContext *ctx, dtr_res_entry_t *e)
{
    switch (e->kind) {
        case DTR_RES_JSON:
            return JS_DupValue(ctx, e->payload.json);
        case DTR_RES_RAW:
        case DTR_RES_HEX: {
            /* Return a fresh Uint8Array that copies the registry blob. */
            JSValue arr;
            JSValue ctor;
            JSValue u8;
            JSValue global;
            JSValue argv[1];

            arr = JS_NewArrayBufferCopy(ctx, e->payload.blob.data, e->payload.blob.len);
            if (JS_IsException(arr)) {
                return arr;
            }
            global = JS_GetGlobalObject(ctx);
            ctor   = JS_GetPropertyStr(ctx, global, "Uint8Array");
            JS_FreeValue(ctx, global);
            argv[0] = arr;
            u8      = JS_CallConstructor(ctx, ctor, 1, argv);
            JS_FreeValue(ctx, arr);
            JS_FreeValue(ctx, ctor);
            return u8;
        }
        default:
            /* loadSprite / loadMap / loadSfx / loadMusic resolve to undefined;
               the resource is identified by its name from then on. */
            return JS_UNDEFINED;
    }
}

static void
prv_complete_request(dtr_console_t *con, dtr_res_request_t *r, bool ok, const char *err_msg)
{
    JSContext       *ctx;
    dtr_res_entry_t *e;
    JSValue          arg;
    JSValue          ret;

    ctx = con->runtime->ctx;
    e   = &con->res->entries[r->slot];

    if (ok) {
        e->status = DTR_RES_STATUS_READY;
        arg       = prv_make_resolve_value(ctx, e);
        ret       = JS_Call(ctx, r->resolve, JS_UNDEFINED, 1, (JSValueConst *)&arg);
        JS_FreeValue(ctx, arg);
    } else {
        e->status = DTR_RES_STATUS_ERROR;
        {
            JSValue err = JS_NewError(ctx);
            JS_SetPropertyStr(
                ctx, err, "message", JS_NewString(ctx, err_msg != NULL ? err_msg : "load failed"));
            JS_SetPropertyStr(ctx, err, "resource", JS_NewString(ctx, e->name));
            ret = JS_Call(ctx, r->reject, JS_UNDEFINED, 1, (JSValueConst *)&err);
            JS_FreeValue(ctx, err);
        }
    }
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
    }
    JS_FreeValue(ctx, ret);

    JS_FreeValue(ctx, r->resolve);
    JS_FreeValue(ctx, r->reject);
    r->resolve    = JS_UNDEFINED;
    r->reject     = JS_UNDEFINED;
    r->slot       = -1;
    r->dispatched = false;
#ifdef __EMSCRIPTEN__
    r->fetch = NULL;
#endif
    --con->res->pending_count;
}

/* ------------------------------------------------------------------ */
/*  Pump                                                               */
/* ------------------------------------------------------------------ */

void dtr_res_pump(dtr_console_t *con)
{
    dtr_res_t *res;
    int32_t    work_done;

    if (con == NULL || con->res == NULL) {
        return;
    }
    res = con->res;
    if (res->pending_count == 0) {
        return;
    }

    work_done = 0;

    for (int32_t i = 0; i < DTR_RES_MAX_PENDING && work_done < DTR_RES_PUMP_PER_TICK; ++i) {
        dtr_res_request_t *r = &res->pending[i];
        if (r->slot < 0) {
            continue;
        }

#ifdef __EMSCRIPTEN__
        /* WASM path: try bundled (/cart/) first via SDL_LoadFile (sync MEMFS),
           then fall back to emscripten_fetch for dynamic resources. */
        if (!r->dispatched) {
            char   full[DTR_RES_PATH_LEN];
            size_t len   = 0;
            void  *bytes = NULL;

            r->dispatched = true;

            if (prv_resolve_cart_path(con, r->path, full, sizeof(full))) {
                bytes = SDL_LoadFile(full, &len);
            }
            if (bytes != NULL) {
                bool ok = prv_decode_request(con, r, (const uint8_t *)bytes, len);
                SDL_free(bytes);
                prv_complete_request(con, r, ok, ok ? NULL : "decode failed");
                ++work_done;
                continue;
            }

            /* Fall back to fetching the dynamic copy. The dev server
               (tools/serve-wasm.js) and prepare-dynamic.js expose dynamic
               files at /dynamic/<rel>. */
            {
                emscripten_fetch_attr_t attr;
                char                    url[DTR_RES_PATH_LEN];

                emscripten_fetch_attr_init(&attr);
                SDL_strlcpy(attr.requestMethod, "GET", sizeof(attr.requestMethod));
                attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                SDL_snprintf(url, sizeof(url), "/dynamic/%s", r->path);
                r->fetch = emscripten_fetch(&attr, url);
                if (r->fetch == NULL) {
                    prv_complete_request(con, r, false, "fetch dispatch failed");
                    ++work_done;
                }
            }
            continue;
        }

        /* Already dispatched — poll fetch state. */
        if (r->fetch != NULL) {
            emscripten_fetch_t *f = (emscripten_fetch_t *)r->fetch;
            if (f->readyState == 4 /* DONE */) {
                if (f->status >= 200 && f->status < 300 && f->numBytes > 0) {
                    bool ok =
                        prv_decode_request(con, r, (const uint8_t *)f->data, (size_t)f->numBytes);
                    emscripten_fetch_close(f);
                    prv_complete_request(con, r, ok, ok ? NULL : "decode failed");
                } else {
                    char err[64];
                    SDL_snprintf(err, sizeof(err), "HTTP %d", (int)f->status);
                    emscripten_fetch_close(f);
                    prv_complete_request(con, r, false, err);
                }
                ++work_done;
            }
        }
#else
        /* Native path: synchronous SDL_LoadFile + decode. */
        char   full[DTR_RES_PATH_LEN];
        size_t len = 0;
        void  *bytes;

        if (!prv_resolve_cart_path(con, r->path, full, sizeof(full))) {
            prv_complete_request(con, r, false, "invalid path");
            ++work_done;
            continue;
        }
        bytes = SDL_LoadFile(full, &len);
        if (bytes == NULL) {
            char err[256];
            SDL_snprintf(err, sizeof(err), "file not found: %s", r->path);
            prv_complete_request(con, r, false, err);
            ++work_done;
            continue;
        }
        {
            bool ok = prv_decode_request(con, r, (const uint8_t *)bytes, len);
            SDL_free(bytes);
            prv_complete_request(con, r, ok, ok ? NULL : "decode failed");
            ++work_done;
        }
#endif
    }
}
