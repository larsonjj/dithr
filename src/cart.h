/**
 * \file            cart.h
 * \brief           Cart configuration, data structures, loading, and save/load
 */

#ifndef MVN_CART_H
#define MVN_CART_H

#include "console.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MVN_CART_MAX_MAPS      CONSOLE_MAX_MAPS
#define MVN_CART_PERSIST_SLOTS 64
#define MVN_CART_MAX_DSLOTS    MVN_CART_PERSIST_SLOTS
#define MVN_CART_MAX_KV        64
#define MVN_CART_KEY_LEN       64
#define MVN_CART_VAL_LEN       256
#define MVN_CART_TITLE_LEN     64

/* ------------------------------------------------------------------------ */
/*  Map object                                                               */
/* ------------------------------------------------------------------------ */

typedef struct mvn_map_object {
    char    name[64];
    char    type[64];
    float   x;
    float   y;
    float   w;
    float   h;
    int32_t gid;
    /* Properties stored as a JS object (ref-counted) */
    JSValue props;
} mvn_map_object_t;

/* ------------------------------------------------------------------------ */
/*  Map layer                                                                */
/* ------------------------------------------------------------------------ */

typedef struct mvn_map_layer {
    char              name[32];
    bool              is_tile_layer;
    int32_t           width;
    int32_t           height;
    int32_t *         tiles; /**< Tile indices, width*height (tile layers) */
    mvn_map_object_t *objects;
    int32_t           object_count;
} mvn_map_layer_t;

/* ------------------------------------------------------------------------ */
/*  Map level                                                                */
/* ------------------------------------------------------------------------ */

typedef struct mvn_map_level {
    char             name[64];
    int32_t          width;  /**< Map width in tiles */
    int32_t          height; /**< Map height in tiles */
    int32_t          tile_w;
    int32_t          tile_h;
    mvn_map_layer_t *layers;
    int32_t          layer_count;
} mvn_map_level_t;

/* ------------------------------------------------------------------------ */
/*  Cart display config                                                      */
/* ------------------------------------------------------------------------ */

typedef struct mvn_cart_display {
    int32_t width;
    int32_t height;
    int32_t palette_size;
    int32_t scale;
    int32_t palette;
    bool    fullscreen;
    char    window_title[128];
} mvn_cart_display_t;

/* ------------------------------------------------------------------------ */
/*  Cart timing config                                                       */
/* ------------------------------------------------------------------------ */

typedef struct mvn_cart_timing {
    int32_t fps;
    int32_t ups;
    bool    frozen;
} mvn_cart_timing_t;

/* ------------------------------------------------------------------------ */
/*  Cart sprite config                                                       */
/* ------------------------------------------------------------------------ */

typedef struct mvn_cart_sprites {
    int32_t tile_w;
    int32_t tile_h;
    int32_t max_sprites;
} mvn_cart_sprites_t;

/* ------------------------------------------------------------------------ */
/*  Cart runtime config                                                      */
/* ------------------------------------------------------------------------ */

typedef struct mvn_cart_runtime {
    uint32_t mem_limit;   /**< JS heap limit in bytes */
    uint32_t stack_limit; /**< JS stack limit in bytes */
} mvn_cart_runtime_t;

/* ------------------------------------------------------------------------ */
/*  Cart audio config                                                        */
/* ------------------------------------------------------------------------ */

typedef struct mvn_cart_audio {
    int32_t channels;
    int32_t frequency;
    int32_t buffer_size;
} mvn_cart_audio_t;

/* ------------------------------------------------------------------------ */
/*  Cart input mapping config                                                */
/* ------------------------------------------------------------------------ */

#define MVN_CART_MAX_INPUT_ACTIONS  16
#define MVN_CART_MAX_INPUT_BINDINGS 8
#define MVN_CART_BIND_NAME_LEN     32

typedef struct mvn_cart_input_mapping {
    char    action[32];
    char    bindings[MVN_CART_MAX_INPUT_BINDINGS][MVN_CART_BIND_NAME_LEN];
    int32_t bind_count;
} mvn_cart_input_mapping_t;

typedef struct mvn_cart_input {
    mvn_cart_input_mapping_t mappings[MVN_CART_MAX_INPUT_ACTIONS];
    int32_t                  mapping_count;
} mvn_cart_input_t;

/* ------------------------------------------------------------------------ */
/*  Cart meta (full configuration)                                           */
/* ------------------------------------------------------------------------ */

typedef struct mvn_cart_meta {
    char title[MVN_CART_TITLE_LEN];
    char author[MVN_CART_TITLE_LEN];
    char version[32];
    char description[256];
    char default_postfx[32];
} mvn_cart_meta_t;

/* ------------------------------------------------------------------------ */
/*  Cart — complete cart state                                               */
/* ------------------------------------------------------------------------ */

struct mvn_cart {
    mvn_cart_meta_t meta;

    /* Config sections (flat, not nested in meta) */
    mvn_cart_display_t display;
    mvn_cart_timing_t  timing;
    mvn_cart_sprites_t sprites;
    mvn_cart_runtime_t runtime;
    mvn_cart_audio_t   audio;
    mvn_cart_input_t   input;

    /* JS source code (owned) */
    char * code;
    size_t code_len;

    /* Asset paths from cart.json */
    char sprite_sheet_path[512];
    char code_path[512];
    char map_paths[MVN_CART_MAX_MAPS][512];

    /* Map levels */
    mvn_map_level_t *maps[MVN_CART_MAX_MAPS];
    int32_t          map_count;
    int32_t          current_map;

    /* Sprite sheet raw RGBA (loaded, then passed to graphics) */
    uint8_t *sprite_rgba;
    int32_t  sprite_w;
    int32_t  sprite_h;
    int32_t  sprite_rgba_w;
    int32_t  sprite_rgba_h;

    /* dget/dset numeric persistence (PICO-8 compat) */
    double dslots[MVN_CART_PERSIST_SLOTS];

    /* Named key/value persistence */
    char    kv_keys[MVN_CART_MAX_KV][MVN_CART_KEY_LEN];
    char    kv_values[MVN_CART_MAX_KV][MVN_CART_VAL_LEN];
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

mvn_cart_t *mvn_cart_create(void);
void        mvn_cart_destroy(mvn_cart_t *cart);

/**
 * \brief           Fill cart with compiled defaults
 */
void mvn_cart_defaults(mvn_cart_t *cart);

/**
 * \brief           Parse and validate a cart.json or cart.baked.json
 * \param[in]       cart: Cart state
 * \param[in]       ctx: JS context for JSON parsing
 * \param[in]       json: Raw JSON string
 * \param[in]       len: JSON string length
 * \return          true on success
 */
bool mvn_cart_parse(mvn_cart_t *cart, JSContext *ctx, const char *json, size_t len);

/**
 * \brief           Validate cart meta values against compiled ceilings
 * \return          true if all values are within bounds
 */
bool mvn_cart_validate(mvn_cart_t *cart);

/**
 * \brief           Load cart file from disk path
 */
bool mvn_cart_load(mvn_cart_t *cart, JSContext *ctx, const char *path);

/**
 * \brief           Load code file referenced by cart
 */
char *mvn_cart_load_code(mvn_cart_t *cart);

/* Persistence (key-value) */
void        mvn_cart_save(mvn_cart_t *cart, const char *key, const char *value);
const char *mvn_cart_load_key(mvn_cart_t *cart, const char *key);
void        mvn_cart_delete_key(mvn_cart_t *cart, const char *key);
bool        mvn_cart_has_key(mvn_cart_t *cart, const char *key);

/* Persistence (numeric slots) */
void   mvn_cart_dset(mvn_cart_t *cart, int32_t index, double value);
double mvn_cart_dget(mvn_cart_t *cart, int32_t index);

/* Disk persistence — flush/load dslots + kv to/from a JSON file */
bool mvn_cart_persist_save(mvn_cart_t *cart);
bool mvn_cart_persist_load(mvn_cart_t *cart);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_CART_H */
