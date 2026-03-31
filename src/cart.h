/**
 * \file            cart.h
 * \brief           Cart configuration, data structures, loading, and save/load
 */

#ifndef DTR_CART_H
#define DTR_CART_H

#include "console.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DTR_CART_MAX_MAPS      CONSOLE_MAX_MAPS
#define DTR_CART_MAX_SFX       256
#define DTR_CART_MAX_MUSIC     64
#define DTR_CART_PERSIST_SLOTS 64
#define DTR_CART_MAX_DSLOTS    DTR_CART_PERSIST_SLOTS
#define DTR_CART_MAX_KV        64
#define DTR_CART_KEY_LEN       64
#define DTR_CART_VAL_LEN       256
#define DTR_CART_TITLE_LEN     64

/* ------------------------------------------------------------------------ */
/*  Map object                                                               */
/* ------------------------------------------------------------------------ */

typedef struct dtr_map_object {
    char    name[64];
    char    type[64];
    float   x;
    float   y;
    float   w;
    float   h;
    int32_t gid;
    /* Properties stored as a JS object (ref-counted) */
    JSValue props;
} dtr_map_object_t;

/* ------------------------------------------------------------------------ */
/*  Map layer                                                                */
/* ------------------------------------------------------------------------ */

typedef struct dtr_map_layer {
    char              name[32];
    bool              is_tile_layer;
    int32_t           width;
    int32_t           height;
    int32_t *         tiles; /**< Tile indices, width*height (tile layers) */
    dtr_map_object_t *objects;
    int32_t           object_count;
} dtr_map_layer_t;

/* ------------------------------------------------------------------------ */
/*  Map level                                                                */
/* ------------------------------------------------------------------------ */

typedef struct dtr_map_level {
    char             name[64];
    int32_t          width;  /**< Map width in tiles */
    int32_t          height; /**< Map height in tiles */
    int32_t          tile_w;
    int32_t          tile_h;
    dtr_map_layer_t *layers;
    int32_t          layer_count;
} dtr_map_level_t;

/* ------------------------------------------------------------------------ */
/*  Cart display config                                                      */
/* ------------------------------------------------------------------------ */

typedef struct dtr_cart_display {
    int32_t width;
    int32_t height;
    int32_t palette_size;
    int32_t scale;
    int32_t palette;
    int32_t scale_mode; /* 0=integer (default), 1=letterbox, 2=stretch, 3=overscan */
    bool    fullscreen;
    bool    pause_key;
    char    window_title[128];
} dtr_cart_display_t;

/* ------------------------------------------------------------------------ */
/*  Cart timing config                                                       */
/* ------------------------------------------------------------------------ */

typedef struct dtr_cart_timing {
    int32_t fps;
    int32_t ups;
    bool    frozen;
} dtr_cart_timing_t;

/* ------------------------------------------------------------------------ */
/*  Cart sprite config                                                       */
/* ------------------------------------------------------------------------ */

typedef struct dtr_cart_sprites {
    int32_t tile_w;
    int32_t tile_h;
    int32_t max_sprites;
} dtr_cart_sprites_t;

/* ------------------------------------------------------------------------ */
/*  Cart runtime config                                                      */
/* ------------------------------------------------------------------------ */

typedef struct dtr_cart_runtime {
    uint32_t mem_limit;   /**< JS heap limit in bytes */
    uint32_t stack_limit; /**< JS stack limit in bytes */
} dtr_cart_runtime_t;

/* ------------------------------------------------------------------------ */
/*  Cart audio config                                                        */
/* ------------------------------------------------------------------------ */

typedef struct dtr_cart_audio {
    int32_t channels;
    int32_t frequency;
    int32_t buffer_size;
} dtr_cart_audio_t;

/* ------------------------------------------------------------------------ */
/*  Cart input mapping config                                                */
/* ------------------------------------------------------------------------ */

#define DTR_CART_MAX_INPUT_ACTIONS  16
#define DTR_CART_MAX_INPUT_BINDINGS 8
#define DTR_CART_BIND_NAME_LEN     32

typedef struct dtr_cart_input_mapping {
    char    action[32];
    char    bindings[DTR_CART_MAX_INPUT_BINDINGS][DTR_CART_BIND_NAME_LEN];
    int32_t bind_count;
} dtr_cart_input_mapping_t;

typedef struct dtr_cart_input {
    dtr_cart_input_mapping_t mappings[DTR_CART_MAX_INPUT_ACTIONS];
    int32_t                  mapping_count;
} dtr_cart_input_t;

/* ------------------------------------------------------------------------ */
/*  Cart meta (full configuration)                                           */
/* ------------------------------------------------------------------------ */

typedef struct dtr_cart_meta {
    char title[DTR_CART_TITLE_LEN];
    char author[DTR_CART_TITLE_LEN];
    char version[32];
    char description[256];
    char default_postfx[32];
} dtr_cart_meta_t;

/* ------------------------------------------------------------------------ */
/*  Cart — complete cart state                                               */
/* ------------------------------------------------------------------------ */

struct dtr_cart {
    dtr_cart_meta_t meta;

    /* Config sections (flat, not nested in meta) */
    dtr_cart_display_t display;
    dtr_cart_timing_t  timing;
    dtr_cart_sprites_t sprites;
    dtr_cart_runtime_t runtime;
    dtr_cart_audio_t   audio;
    dtr_cart_input_t   input;

    /* JS source code (owned) */
    char * code;
    size_t code_len;

    /* Asset paths from cart.json */
    char sprite_sheet_path[512];
    char code_path[512];
    char map_paths[DTR_CART_MAX_MAPS][512];
    char sfx_paths[DTR_CART_MAX_SFX][512];
    int32_t sfx_count;
    char music_paths[DTR_CART_MAX_MUSIC][512];
    int32_t music_count;

    /* Map levels */
    dtr_map_level_t *maps[DTR_CART_MAX_MAPS];
    int32_t          map_count;
    int32_t          current_map;

    /* Sprite sheet raw RGBA (loaded, then passed to graphics) */
    uint8_t *sprite_rgba;
    int32_t  sprite_w;
    int32_t  sprite_h;
    int32_t  sprite_rgba_w;
    int32_t  sprite_rgba_h;

    /* dget/dset numeric persistence (PICO-8 compat) */
    double dslots[DTR_CART_PERSIST_SLOTS];

    /* Named key/value persistence */
    char    kv_keys[DTR_CART_MAX_KV][DTR_CART_KEY_LEN];
    char    kv_values[DTR_CART_MAX_KV][DTR_CART_VAL_LEN];
    int32_t kv_count;

    /* Path for resolving relative asset paths */
    char base_path[512];

    /* Is this a baked cart? */
    bool baked;

    /* JS context ref (for object properties in map objects) */
    JSContext *ctx;
};

/* ------------------------------------------------------------------------ */
/*  Cart lifecycle                                                           */
/* ------------------------------------------------------------------------ */

dtr_cart_t *dtr_cart_create(void);
void        dtr_cart_destroy(dtr_cart_t *cart);
void        dtr_cart_free_map(dtr_cart_t *cart, int32_t idx);

/**
 * \brief           Fill cart with compiled defaults
 */
void dtr_cart_defaults(dtr_cart_t *cart);

/**
 * \brief           Parse and validate a cart.json or cart.baked.json
 * \param[in]       cart: Cart state
 * \param[in]       ctx: JS context for JSON parsing
 * \param[in]       json: Raw JSON string
 * \param[in]       len: JSON string length
 * \return          true on success
 */
bool dtr_cart_parse(dtr_cart_t *cart, JSContext *ctx, const char *json, size_t len);

/**
 * \brief           Validate cart meta values against compiled ceilings
 * \return          true if all values are within bounds
 */
bool dtr_cart_validate(dtr_cart_t *cart);

/**
 * \brief           Load cart file from disk path
 */
bool dtr_cart_load(dtr_cart_t *cart, JSContext *ctx, const char *path);

/**
 * \brief           Load code file referenced by cart
 */
char *dtr_cart_load_code(dtr_cart_t *cart);

/* Persistence (key-value) */
void        dtr_cart_save(dtr_cart_t *cart, const char *key, const char *value);
const char *dtr_cart_load_key(dtr_cart_t *cart, const char *key);
void        dtr_cart_delete_key(dtr_cart_t *cart, const char *key);
bool        dtr_cart_has_key(dtr_cart_t *cart, const char *key);

/* Persistence (numeric slots) */
void   dtr_cart_dset(dtr_cart_t *cart, int32_t index, double value);
double dtr_cart_dget(dtr_cart_t *cart, int32_t index);

/* Disk persistence — flush/load dslots + kv to/from a JSON file */
bool dtr_cart_persist_save(dtr_cart_t *cart);
bool dtr_cart_persist_load(dtr_cart_t *cart);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_CART_H */
