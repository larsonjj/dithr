/**
 * \file            quickjs_compat.h
 * \brief           Wrapper around the QuickJS-NG header that silences
 *                  third-party MSVC C4244 warnings (int64_t -> int32_t in
 *                  quickjs.h:380). Include this instead of <quickjs.h> from
 *                  dithr code so the suppression stays narrowly scoped.
 */

#ifndef DTR_QUICKJS_COMPAT_H
#define DTR_QUICKJS_COMPAT_H

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244) /* QuickJS-NG: int64_t -> int32_t in quickjs.h */
#endif

#include "quickjs.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif /* DTR_QUICKJS_COMPAT_H */
