/**
 * \file            resources.h
 * \brief           String-keyed asset registry and async load queue
 *
 * Phase 2 of the res.* asset API. Owns the registry of loaded assets
 * (sprites, maps, sfx, music, json, raw, hex) and a single-threaded
 * "async" load queue: requests are enqueued from the JS API, the engine
 * drains a bounded number per frame from dtr_console_iterate, decodes
 * the payload, then resolves/rejects the associated JS Promise.
 *
 * No JS API surface is exposed here — that lives in src/api/res_api.c
 * (Phase 3). This header provides the C interface that the JS bindings
 * and the console iterate loop use.
 */

#ifndef DTR_RESOURCES_H
#define DTR_RESOURCES_H

#include "console_defs.h"
#include "quickjs_compat.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DTR_RES_MAX_ENTRIES   256
#define DTR_RES_MAX_PENDING   64
#define DTR_RES_NAME_LEN      64
#define DTR_RES_PATH_LEN      512
#define DTR_RES_PUMP_PER_TICK 2

/* Forward decls (avoid pulling in cart.h / SDL into this header) */
struct dtr_map_level;
struct dtr_console;
struct dtr_runtime;

/* ------------------------------------------------------------------------ */
/*  Resource kinds                                                           */
/* ------------------------------------------------------------------------ */

typedef enum dtr_res_kind {
    DTR_RES_NONE   = 0,
    DTR_RES_SPRITE = 1,
    DTR_RES_MAP    = 2,
    DTR_RES_SFX    = 3,
    DTR_RES_MUSIC  = 4,
    DTR_RES_JSON   = 5,
    DTR_RES_RAW    = 6,
    DTR_RES_HEX    = 7,
} dtr_res_kind_t;

typedef enum dtr_res_status {
    DTR_RES_STATUS_FREE    = 0,
    DTR_RES_STATUS_PENDING = 1,
    DTR_RES_STATUS_READY   = 2,
    DTR_RES_STATUS_ERROR   = 3,
} dtr_res_status_t;

/* ------------------------------------------------------------------------ */
/*  Per-entry payload                                                        */
/* ------------------------------------------------------------------------ */

typedef struct dtr_res_payload_sprite {
    uint8_t *rgba; /**< Owned. RGBA8888 pixels. */
    int32_t  w;
    int32_t  h;
} dtr_res_payload_sprite_t;

typedef struct dtr_res_payload_blob {
    uint8_t *data; /**< Owned. */
    size_t   len;
} dtr_res_payload_blob_t;

/**
 * \brief           One registry slot.
 *
 * The slot index is the public "handle" exposed by res.handle(name).
 * Slot 0 is reserved as the "invalid" handle (always FREE/empty name).
 */
typedef struct dtr_res_entry {
    char             name[DTR_RES_NAME_LEN]; /**< "" when slot is FREE. */
    dtr_res_kind_t   kind;
    dtr_res_status_t status;
    union {
        dtr_res_payload_sprite_t sprite;
        dtr_res_payload_blob_t   blob; /**< raw / hex / json text source */
        struct dtr_map_level    *map;
        int32_t                  audio_slot; /**< sfx/music: index into dtr_audio_t */
        JSValue                  json;       /**< parsed JSON value (ref-counted) */
    } payload;
} dtr_res_entry_t;

/* ------------------------------------------------------------------------ */
/*  Pending load request                                                     */
/* ------------------------------------------------------------------------ */

typedef struct dtr_res_request {
    int32_t        slot;                   /**< Index into entries[]; -1 = empty cell. */
    dtr_res_kind_t kind;                   /**< Decode kind. */
    char           path[DTR_RES_PATH_LEN]; /**< Cart-relative or absolute. */
    JSValue        resolve;                /**< Promise resolve fn, owned. */
    JSValue        reject;                 /**< Promise reject fn, owned. */
    bool           dispatched;             /**< true once async I/O has started (WASM). */

#ifdef __EMSCRIPTEN__
    /* Opaque pointer to an in-flight emscripten_fetch_t once dispatched. */
    void *fetch;
#endif
} dtr_res_request_t;

/* ------------------------------------------------------------------------ */
/*  Top-level state                                                          */
/* ------------------------------------------------------------------------ */

struct dtr_res {
    dtr_res_entry_t entries[DTR_RES_MAX_ENTRIES];

    /* Unordered cells of in-flight requests; cell.slot == -1 => empty. */
    dtr_res_request_t pending[DTR_RES_MAX_PENDING];
    int32_t           pending_count;

    /* Active selectors (Phase 3 will set these from JS).
       For Phase 2 they're just storage; nothing reads them yet. */
    int32_t active_sheet_slot;   /**< -1 if none */
    int32_t active_palette_slot; /**< -1 if none */
    int32_t active_flags_slot;   /**< -1 if none */
    int32_t active_map_slot;     /**< -1 if none */
};
/* dtr_res_t is forward-typedef'd in console_defs.h. */

/* ------------------------------------------------------------------------ */
/*  Lifecycle                                                                */
/* ------------------------------------------------------------------------ */

dtr_res_t *dtr_res_create(void);
void       dtr_res_destroy(struct dtr_console *con, dtr_res_t *res);

/* ------------------------------------------------------------------------ */
/*  Registry                                                                 */
/* ------------------------------------------------------------------------ */

/**
 * \brief   Look up a registered slot by name.
 * \return  Slot index in [1, DTR_RES_MAX_ENTRIES) or -1 if not found.
 */
int32_t dtr_res_find(const dtr_res_t *res, const char *name);

/**
 * \brief   Reserve a slot for `name`, replacing any previous entry.
 *
 * The returned slot is initialised with kind=kind, status=PENDING, and
 * an empty payload. Existing payloads (if any) are freed.
 *
 * \return  Slot index, or -1 if the table is full.
 */
int32_t
dtr_res_reserve(struct dtr_console *con, dtr_res_t *res, const char *name, dtr_res_kind_t kind);

/**
 * \brief   Free the payload for a slot and mark it FREE.
 */
void dtr_res_unload(struct dtr_console *con, dtr_res_t *res, int32_t slot);

/**
 * \brief   Resolve a JSValue (string-or-number) to a slot of the given kind.
 *
 * Strings are looked up by name; numbers are validated as in-range slot
 * indices with matching kind and READY status. Returns -1 on failure.
 */
int32_t dtr_res_resolve_handle(struct dtr_console *con, JSValueConst v, dtr_res_kind_t kind);

/* ------------------------------------------------------------------------ */
/*  Async load queue                                                         */
/* ------------------------------------------------------------------------ */

/**
 * \brief   Enqueue a load request and store the resolve/reject pair.
 *
 * The slot must already be reserved via dtr_res_reserve(). Ownership of
 * the `resolve` and `reject` JSValues transfers to the queue.
 *
 * \return  true on success; false if the queue is full (caller should
 *          reject the promise immediately and unreserve the slot).
 */
bool dtr_res_enqueue(dtr_res_t     *res,
                     int32_t        slot,
                     dtr_res_kind_t kind,
                     const char    *path,
                     JSValue        resolve,
                     JSValue        reject);

/**
 * \brief   Drain up to DTR_RES_PUMP_PER_TICK pending requests.
 *
 * Called from dtr_console_iterate after JS callbacks but before
 * microtasks are drained, so settled promises run their .then handlers
 * the same frame they complete.
 */
void dtr_res_pump(struct dtr_console *con);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_RESOURCES_H */
