/**
 * \file            postfx.h
 * \brief           Post-processing pipeline — software-based effects
 */

#ifndef MVN_POSTFX_H
#define MVN_POSTFX_H

#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MVN_POSTFX_NAME_LEN   32
#define MVN_POSTFX_MAX_STACK  8
#define MVN_POSTFX_MAX_PARAMS 8

/* Built-in effect identifiers */
typedef enum mvn_postfx_id {
    MVN_POSTFX_NONE = 0,
    MVN_POSTFX_CRT,
    MVN_POSTFX_SCANLINES,
    MVN_POSTFX_BLOOM,
    MVN_POSTFX_ABERRATION,
    MVN_POSTFX_BUILTIN_COUNT
} mvn_postfx_id_t;

/**
 * \brief           A single effect instance in the chain
 */
typedef struct mvn_postfx_entry {
    mvn_postfx_id_t id;
    char            name[MVN_POSTFX_NAME_LEN];
    float           strength;
    float           params[MVN_POSTFX_MAX_PARAMS];
    int32_t         param_count;
} mvn_postfx_entry_t;

/**
 * \brief           Post-processing pipeline state
 */
typedef struct mvn_postfx {
    mvn_postfx_entry_t stack[MVN_POSTFX_MAX_STACK];
    int32_t            count;

    /* Scratch buffer for multi-pass effects */
    uint32_t *scratch;
    int32_t   width;
    int32_t   height;
} mvn_postfx_t;

mvn_postfx_t *mvn_postfx_create(int32_t width, int32_t height);
void          mvn_postfx_destroy(mvn_postfx_t *pfx);

/**
 * \brief           Push an effect onto the stack
 */
void mvn_postfx_push(mvn_postfx_t *pfx, mvn_postfx_id_t id, float strength);

/**
 * \brief           Pop the top effect from the stack
 */
void mvn_postfx_pop(mvn_postfx_t *pfx);

/**
 * \brief           Clear all effects from the stack
 */
void mvn_postfx_clear(mvn_postfx_t *pfx);

/**
 * \brief           Set the active effect (replaces stack with single entry)
 * \return          Index into the stack (for set_param)
 */
int32_t mvn_postfx_use(mvn_postfx_t *pfx, const char *name);

/**
 * \brief           Set a parameter on a stack entry by index
 */
void mvn_postfx_set_param(mvn_postfx_t *pfx, int32_t index, int32_t param_idx, float value);

/**
 * \brief           Apply the current effect stack to an RGBA pixel buffer
 * \param[in,out]   pixels: RGBA pixel buffer (modified in place)
 * \param[in]       width: Buffer width
 * \param[in]       height: Buffer height
 */
void mvn_postfx_apply(mvn_postfx_t *pfx, uint32_t *pixels, int32_t width, int32_t height);

/**
 * \brief           Get list of available effect names
 */
const char **mvn_postfx_available(int32_t *out_count);

/**
 * \brief           Resolve effect name to ID
 */
mvn_postfx_id_t mvn_postfx_id_from_name(const char *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_POSTFX_H */
