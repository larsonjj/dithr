/**
 * \file            graphics_internal.h
 * \brief           Helpers shared between graphics.c and the split graphics_*.c
 *                  translation units.  Not part of the public API.
 */

#ifndef DTR_GRAPHICS_INTERNAL_H
#define DTR_GRAPHICS_INTERNAL_H

#include "console_defs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Convert a hex ASCII character to its 0–15 nibble value.
 * \return          true on success, false if \p chr is not a hex digit.
 */
static inline bool dtr_gfx_hex_nibble(uint8_t chr, int *out)
{
    if (chr >= '0' && chr <= '9') {
        *out = chr - '0';
        return true;
    }
    if (chr >= 'a' && chr <= 'f') {
        *out = chr - 'a' + 10;
        return true;
    }
    if (chr >= 'A' && chr <= 'F') {
        *out = chr - 'A' + 10;
        return true;
    }
    return false;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_GRAPHICS_INTERNAL_H */
