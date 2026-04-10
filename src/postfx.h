/**
 * \file            postfx.h
 * \brief           Post-processing pipeline — software-based effects
 */

#ifndef DTR_POSTFX_H
#define DTR_POSTFX_H

#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DTR_POSTFX_NAME_LEN   32
#define DTR_POSTFX_MAX_STACK  8
#define DTR_POSTFX_MAX_PARAMS 8

/* Built-in effect identifiers */
typedef enum dtr_postfx_id {
    DTR_POSTFX_NONE = 0,
    DTR_POSTFX_CRT,
    DTR_POSTFX_SCANLINES,
    DTR_POSTFX_BLOOM,
    DTR_POSTFX_ABERRATION,
    DTR_POSTFX_BUILTIN_COUNT
} dtr_postfx_id_t;

/**
 * \brief           A single effect instance in the chain
 */
typedef struct dtr_postfx_entry {
    dtr_postfx_id_t id;
    char            name[DTR_POSTFX_NAME_LEN];
    float           strength;
    float           params[DTR_POSTFX_MAX_PARAMS];
    int32_t         param_count;
} dtr_postfx_entry_t;

/**
 * \brief           Post-processing pipeline state
 */
struct dtr_postfx {
    dtr_postfx_entry_t stack[DTR_POSTFX_MAX_STACK];
    int32_t            count;

    /* Saved snapshot for save/restore */
    dtr_postfx_entry_t saved_stack[DTR_POSTFX_MAX_STACK];
    int32_t            saved_count;

    /* Scratch buffer for multi-pass effects */
    uint32_t *scratch;
    int32_t   width;
    int32_t   height;
};

dtr_postfx_t *dtr_postfx_create(int32_t width, int32_t height);
void          dtr_postfx_destroy(dtr_postfx_t *pfx);

/**
 * \brief           Push an effect onto the stack
 */
void dtr_postfx_push(dtr_postfx_t *pfx, dtr_postfx_id_t id, float strength);

/**
 * \brief           Pop the top effect from the stack
 */
void dtr_postfx_pop(dtr_postfx_t *pfx);

/**
 * \brief           Clear all effects from the stack
 */
void dtr_postfx_clear(dtr_postfx_t *pfx);

/**
 * \brief           Set the active effect (replaces stack with single entry)
 * \return          Index into the stack (for set_param)
 */
int32_t dtr_postfx_use(dtr_postfx_t *pfx, const char *name);

/**
 * \brief           Set a parameter on a stack entry by index
 */
void dtr_postfx_set_param(dtr_postfx_t *pfx, int32_t index, int32_t param_idx, float value);

/**
 * \brief           Apply the current effect stack to an RGBA pixel buffer
 * \param[in,out]   pixels: RGBA pixel buffer (modified in place)
 * \param[in]       width: Buffer width
 * \param[in]       height: Buffer height
 */
void dtr_postfx_apply(dtr_postfx_t *pfx, uint32_t *pixels, int32_t width, int32_t height);

/**
 * \brief           Get list of available effect names
 */
const char **dtr_postfx_available(int32_t *out_count);

/**
 * \brief           Resolve effect name to ID
 */
dtr_postfx_id_t dtr_postfx_id_from_name(const char *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_POSTFX_H */
