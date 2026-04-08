/**
 * \file            synth.h
 * \brief           Chip-tune synthesizer — procedural SFX generation
 */

#ifndef DTR_SYNTH_H
#define DTR_SYNTH_H

#include "console_defs.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define DTR_SYNTH_NOTES       32 /**< Notes per SFX pattern */
#define DTR_SYNTH_MAX_SFX     64 /**< Max synth SFX slots */
#define DTR_SYNTH_SAMPLE_RATE 44100

/** Waveform types (matching PICO-8 instrument order) */
#define DTR_WAVE_TRIANGLE  0 /**< Triangle wave (PICO-8 inst 0) */
#define DTR_WAVE_TILTSAW   1 /**< Tilted / asymmetric saw (PICO-8 inst 1) */
#define DTR_WAVE_SAW       2 /**< Sawtooth (PICO-8 inst 2) */
#define DTR_WAVE_SQUARE    3 /**< Square 50% duty (PICO-8 inst 3) */
#define DTR_WAVE_PULSE     4 /**< Pulse ~31.6% duty (PICO-8 inst 4) */
#define DTR_WAVE_ORGAN     5 /**< Dual triangle / organ (PICO-8 inst 5) */
#define DTR_WAVE_NOISE     6 /**< Filtered noise (PICO-8 inst 6) */
#define DTR_WAVE_PHASER    7 /**< Detuned dual triangle (PICO-8 inst 7) */
#define DTR_WAVE_COUNT     8

/** Effect types (PICO-8 compatible) */
#define DTR_FX_NONE       0
#define DTR_FX_SLIDE      1 /**< Pitch glides toward next note */
#define DTR_FX_VIBRATO    2 /**< Pitch oscillates (sine vibrato) */
#define DTR_FX_DROP       3 /**< Pitch fall */
#define DTR_FX_FADE_IN    4
#define DTR_FX_FADE_OUT   5
#define DTR_FX_ARPF       6 /**< Arpeggio fast (4-note groups) */
#define DTR_FX_ARPS       7 /**< Arpeggio slow (4-note groups) */
#define DTR_FX_COUNT      8

/* ------------------------------------------------------------------ */
/*  Data structures                                                    */
/* ------------------------------------------------------------------ */

/**
 * \brief           Single note in a synth SFX pattern
 */
typedef struct dtr_synth_note {
    uint8_t pitch;    /**< 0 = rest, 1-96 = C-0..B-7 */
    uint8_t waveform; /**< DTR_WAVE_* (0-7) */
    uint8_t volume;   /**< 0-7 */
    uint8_t effect;   /**< DTR_FX_* (0-7) */
} dtr_synth_note_t;

/**
 * \brief           A complete synth SFX definition (32 notes)
 */
typedef struct dtr_synth_sfx {
    dtr_synth_note_t notes[DTR_SYNTH_NOTES];
    uint8_t          speed;      /**< Ticks per note (1-255, higher = slower) */
    uint8_t          loop_start; /**< Loop start note index (0-31) */
    uint8_t          loop_end;   /**< Loop end note index (0-31, 0 = no loop) */
} dtr_synth_sfx_t;

/* ------------------------------------------------------------------ */
/*  PCM rendering                                                      */
/* ------------------------------------------------------------------ */

/**
 * \brief           Render a synth SFX to 16-bit signed PCM audio
 * \param[in]       sfx: Synth SFX definition
 * \param[out]      out_len: Number of samples written (mono)
 * \return          Heap-allocated int16_t buffer (caller must DTR_FREE),
 *                  or NULL on failure
 */
int16_t *dtr_synth_render(const dtr_synth_sfx_t *sfx, size_t *out_len);

/**
 * \brief           Get note name string for a pitch value
 * \param[in]       pitch: 0-96 (0 = rest)
 * \return          Static string like "C-0", "A#4", or "---"
 */
const char *dtr_synth_note_name(uint8_t pitch);

/**
 * \brief           Get frequency in Hz for a pitch value
 * \param[in]       pitch: 1-96
 * \return          Frequency in Hz (0.0 for rest/invalid)
 */
float dtr_synth_pitch_freq(uint8_t pitch);

/**
 * \brief           Generate a waveform sample at a given phase
 * \param[in]       wave: DTR_WAVE_* (0-7)
 * \param[in]       phase: 0.0 .. 1.0
 * \return          Sample value (amplitude varies per waveform, PICO-8 style)
 */
float dtr_synth_waveform(uint8_t wave, float phase);

/**
 * \brief           Generate a band-limited waveform sample (PolyBLEP)
 * \param[in]       wave: DTR_WAVE_* (0-7)
 * \param[in]       phase: 0.0 .. 1.0
 * \param[in]       dtp: Phase increment per sample (freq / sample_rate)
 * \return          Sample value (amplitude varies per waveform, PICO-8 style)
 */
float dtr_synth_waveform_bl(uint8_t wave, float phase, float dtp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_SYNTH_H */
