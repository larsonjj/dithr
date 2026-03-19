/**
 * \file            cart_import.h
 * \brief           Asset importers — Aseprite, Tiled (.tmj), LDtk (.ldtk)
 */

#ifndef DTR_CART_IMPORT_H
#define DTR_CART_IMPORT_H

#include "cart.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Import sprite sheet from Aseprite JSON sidecar + PNG
 * \param[in]       json_path: Path to the Aseprite JSON meta
 * \param[in]       cart: Cart to populate sprite data into
 * \param[in]       ctx: JS context for JSON parsing
 * \return          true on success
 */
bool dtr_import_aseprite(const char *json_path, dtr_cart_t *cart, JSContext *ctx);

/**
 * \brief           Import a map from Tiled .tmj format
 * \param[in]       tmj_path: Path to the .tmj file
 * \param[out]      out_level: Pointer to receive allocated level
 * \param[in]       ctx: JS context for JSON parsing
 * \return          true on success
 */
bool dtr_import_tiled(const char *tmj_path, dtr_map_level_t **out_level, JSContext *ctx);

/**
 * \brief           Import first level from an LDtk .ldtk file
 * \param[in]       ldtk_path: Path to the .ldtk file
 * \param[out]      out_level: Pointer to receive allocated level
 * \param[in]       ctx: JS context for JSON parsing
 * \return          true on success
 */
bool dtr_import_ldtk(const char *ldtk_path, dtr_map_level_t **out_level, JSContext *ctx);

/**
 * \brief           Load a raw PNG file and return RGBA data
 * \param[in]       path: File path
 * \param[out]      out_w: Image width
 * \param[out]      out_h: Image height
 * \return          RGBA pixel data (caller must DTR_FREE), or NULL
 */
uint8_t *dtr_import_png(const char *path, int32_t *out_w, int32_t *out_h);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_CART_IMPORT_H */
