/**
 * \file            simd.h
 * \brief           Portable SIMD abstraction — SSE2 / NEON / wasm-simd128
 *
 * Provides a thin wrapper over 128-bit SIMD intrinsics that compile
 * to native instructions on x86 (SSE2), ARM (NEON), and WebAssembly
 * (wasm-simd128).  When no SIMD ISA is available the code falls back
 * to plain scalar C via DTR_HAS_SIMD == 0.
 *
 * All vector types are 4×int32 (dtr_v4i) or 4×float (dtr_v4f).
 */

#ifndef DTR_SIMD_H
#define DTR_SIMD_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Detect SIMD support                                                */
/* ------------------------------------------------------------------ */

#if defined(__wasm_simd128__)
#define DTR_SIMD_WASM 1
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define DTR_SIMD_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define DTR_SIMD_NEON 1
#endif

#if defined(DTR_SIMD_WASM) || defined(DTR_SIMD_SSE2) || defined(DTR_SIMD_NEON)
#define DTR_HAS_SIMD 1
#else
#define DTR_HAS_SIMD 0
#endif

#if !DTR_HAS_SIMD
/* No SIMD — header is still includable but nothing is defined */
#else /* DTR_HAS_SIMD */

/* ------------------------------------------------------------------ */
/*  Include platform headers                                           */
/* ------------------------------------------------------------------ */

#if defined(DTR_SIMD_WASM)
#include <wasm_simd128.h>
typedef v128_t dtr_v4i;
typedef v128_t dtr_v4f;
#elif defined(DTR_SIMD_SSE2)
#include <emmintrin.h>
typedef __m128i dtr_v4i;
typedef __m128  dtr_v4f;
#elif defined(DTR_SIMD_NEON)
#include <arm_neon.h>
typedef int32x4_t   dtr_v4i;
typedef float32x4_t dtr_v4f;
#endif

/* ------------------------------------------------------------------ */
/*  4×int32 operations                                                 */
/* ------------------------------------------------------------------ */

/** Set all four lanes to zero */
static inline dtr_v4i dtr_v4i_zero(void)
{
#if defined(DTR_SIMD_WASM)
    return wasm_i32x4_splat(0);
#elif defined(DTR_SIMD_SSE2)
    return _mm_setzero_si128();
#elif defined(DTR_SIMD_NEON)
    return vdupq_n_s32(0);
#endif
}

/** Splat a single int32 to all four lanes */
static inline dtr_v4i dtr_v4i_splat(int32_t v)
{
#if defined(DTR_SIMD_WASM)
    return wasm_i32x4_splat(v);
#elif defined(DTR_SIMD_SSE2)
    return _mm_set1_epi32(v);
#elif defined(DTR_SIMD_NEON)
    return vdupq_n_s32(v);
#endif
}

/** Construct from four int32 values */
static inline dtr_v4i dtr_v4i_set(int32_t a, int32_t b, int32_t c, int32_t d)
{
#if defined(DTR_SIMD_WASM)
    return wasm_i32x4_make(a, b, c, d);
#elif defined(DTR_SIMD_SSE2)
    return _mm_set_epi32(d, c, b, a); /* SSE: reversed order */
#elif defined(DTR_SIMD_NEON)
    int32_t arr[4] = {a, b, c, d};
    return vld1q_s32(arr);
#endif
}

/** Load 4 × int32 from aligned memory */
static inline dtr_v4i dtr_v4i_load(const int32_t *ptr)
{
#if defined(DTR_SIMD_WASM)
    return wasm_v128_load(ptr);
#elif defined(DTR_SIMD_SSE2)
    return _mm_loadu_si128((const __m128i *)ptr);
#elif defined(DTR_SIMD_NEON)
    return vld1q_s32(ptr);
#endif
}

/** Store 4 × int32 to memory */
static inline void dtr_v4i_store(int32_t *ptr, dtr_v4i v)
{
#if defined(DTR_SIMD_WASM)
    wasm_v128_store(ptr, v);
#elif defined(DTR_SIMD_SSE2)
    _mm_storeu_si128((__m128i *)ptr, v);
#elif defined(DTR_SIMD_NEON)
    vst1q_s32(ptr, v);
#endif
}

/** Load 4 × uint32 from memory (reinterpret as int32 vector) */
static inline dtr_v4i dtr_v4i_load_u32(const uint32_t *ptr)
{
    return dtr_v4i_load((const int32_t *)ptr);
}

/** Store 4 × uint32 to memory */
static inline void dtr_v4i_store_u32(uint32_t *ptr, dtr_v4i v)
{
    dtr_v4i_store((int32_t *)ptr, v);
}

/** Add 4 × int32 */
static inline dtr_v4i dtr_v4i_add(dtr_v4i a, dtr_v4i b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_i32x4_add(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_add_epi32(a, b);
#elif defined(DTR_SIMD_NEON)
    return vaddq_s32(a, b);
#endif
}

/** Subtract 4 × int32 */
static inline dtr_v4i dtr_v4i_sub(dtr_v4i a, dtr_v4i b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_i32x4_sub(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_sub_epi32(a, b);
#elif defined(DTR_SIMD_NEON)
    return vsubq_s32(a, b);
#endif
}

/** Bitwise AND */
static inline dtr_v4i dtr_v4i_and(dtr_v4i a, dtr_v4i b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_v128_and(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_and_si128(a, b);
#elif defined(DTR_SIMD_NEON)
    return vandq_s32(a, b);
#endif
}

/** Bitwise OR */
static inline dtr_v4i dtr_v4i_or(dtr_v4i a, dtr_v4i b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_v128_or(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_or_si128(a, b);
#elif defined(DTR_SIMD_NEON)
    return vorrq_s32(a, b);
#endif
}

/** Shift right logical (all lanes by immediate count) */
static inline dtr_v4i dtr_v4i_srl(dtr_v4i v, int32_t count)
{
#if defined(DTR_SIMD_WASM)
    return wasm_u32x4_shr(v, (uint32_t)count);
#elif defined(DTR_SIMD_SSE2)
    return _mm_srli_epi32(v, count);
#elif defined(DTR_SIMD_NEON)
    /* Must use unsigned shift to avoid sign-extension on pixel data */
    return (int32x4_t)vshlq_u32((uint32x4_t)v, vdupq_n_s32(-count));
#endif
}

/** Shift left logical (all lanes by immediate count) */
static inline dtr_v4i dtr_v4i_sll(dtr_v4i v, int32_t count)
{
#if defined(DTR_SIMD_WASM)
    return wasm_i32x4_shl(v, (uint32_t)count);
#elif defined(DTR_SIMD_SSE2)
    return _mm_slli_epi32(v, count);
#elif defined(DTR_SIMD_NEON)
    return vshlq_s32(v, vdupq_n_s32(count));
#endif
}

/** Extract lane (0-3).  idx must be a compile-time constant. */
static inline int32_t dtr_v4i_lane(dtr_v4i v, int32_t idx)
{
    /* All three backends use store+load to avoid the compile-time
     * constant requirement of the native lane-extract intrinsics
     * (NEON vgetq_lane, WASM wasm_i32x4_extract_lane). */
    int32_t tmp[4];
#if defined(DTR_SIMD_WASM)
    wasm_v128_store(tmp, v);
#elif defined(DTR_SIMD_SSE2)
    _mm_storeu_si128((__m128i *)tmp, v);
#elif defined(DTR_SIMD_NEON)
    vst1q_s32(tmp, v);
#endif
    return tmp[idx];
}

/** Pack 4 RGBA channels (each 0-255 in int32 lanes) into one uint32 */
static inline uint32_t dtr_v4i_pack_rgba(dtr_v4i v)
{
    int32_t r = dtr_v4i_lane(v, 0);
    int32_t g = dtr_v4i_lane(v, 1);
    int32_t b = dtr_v4i_lane(v, 2);
    int32_t a = dtr_v4i_lane(v, 3);
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

/** Unpack a uint32 RGBA into 4 int32 lanes {R, G, B, A} */
static inline dtr_v4i dtr_v4i_unpack_rgba(uint32_t px)
{
    return dtr_v4i_set((int32_t)((px >> 24) & 0xFF),
                       (int32_t)((px >> 16) & 0xFF),
                       (int32_t)((px >> 8) & 0xFF),
                       (int32_t)(px & 0xFF));
}

/* ------------------------------------------------------------------ */
/*  4×float operations                                                 */
/* ------------------------------------------------------------------ */

/** Splat a float to all four lanes */
static inline dtr_v4f dtr_v4f_splat(float v)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_splat(v);
#elif defined(DTR_SIMD_SSE2)
    return _mm_set1_ps(v);
#elif defined(DTR_SIMD_NEON)
    return vdupq_n_f32(v);
#endif
}

/** Construct from four floats */
static inline dtr_v4f dtr_v4f_set(float a, float b, float c, float d)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_make(a, b, c, d);
#elif defined(DTR_SIMD_SSE2)
    return _mm_set_ps(d, c, b, a);
#elif defined(DTR_SIMD_NEON)
    float arr[4] = {a, b, c, d};
    return vld1q_f32(arr);
#endif
}

/** Multiply 4 × float */
static inline dtr_v4f dtr_v4f_mul(dtr_v4f a, dtr_v4f b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_mul(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_mul_ps(a, b);
#elif defined(DTR_SIMD_NEON)
    return vmulq_f32(a, b);
#endif
}

/** Add 4 × float */
static inline dtr_v4f dtr_v4f_add(dtr_v4f a, dtr_v4f b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_add(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_add_ps(a, b);
#elif defined(DTR_SIMD_NEON)
    return vaddq_f32(a, b);
#endif
}

/** Subtract 4 × float */
static inline dtr_v4f dtr_v4f_sub(dtr_v4f a, dtr_v4f b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_sub(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_sub_ps(a, b);
#elif defined(DTR_SIMD_NEON)
    return vsubq_f32(a, b);
#endif
}

/** Max(a, b) per lane */
static inline dtr_v4f dtr_v4f_max(dtr_v4f a, dtr_v4f b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_max(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_max_ps(a, b);
#elif defined(DTR_SIMD_NEON)
    return vmaxq_f32(a, b);
#endif
}

/** Min(a, b) per lane */
static inline dtr_v4f dtr_v4f_min(dtr_v4f a, dtr_v4f b)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_min(a, b);
#elif defined(DTR_SIMD_SSE2)
    return _mm_min_ps(a, b);
#elif defined(DTR_SIMD_NEON)
    return vminq_f32(a, b);
#endif
}

/** Convert 4×int32 → 4×float */
static inline dtr_v4f dtr_v4i_to_v4f(dtr_v4i v)
{
#if defined(DTR_SIMD_WASM)
    return wasm_f32x4_convert_i32x4(v);
#elif defined(DTR_SIMD_SSE2)
    return _mm_cvtepi32_ps(v);
#elif defined(DTR_SIMD_NEON)
    return vcvtq_f32_s32(v);
#endif
}

/** Convert 4×float → 4×int32 (truncate towards zero) */
static inline dtr_v4i dtr_v4f_to_v4i(dtr_v4f v)
{
#if defined(DTR_SIMD_WASM)
    return wasm_i32x4_trunc_sat_f32x4(v);
#elif defined(DTR_SIMD_SSE2)
    return _mm_cvttps_epi32(v);
#elif defined(DTR_SIMD_NEON)
    return vcvtq_s32_f32(v);
#endif
}

#endif /* DTR_HAS_SIMD */
#endif /* DTR_SIMD_H */
