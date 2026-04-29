/**
 * \file            cart_import.h
 * \brief           Asset importers — Tiled (.tmj), LDtk (.ldtk)
 */

#ifndef DTR_CART_IMPORT_H
#define DTR_CART_IMPORT_H

#include "cart.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_CART_IMPORT_H */
