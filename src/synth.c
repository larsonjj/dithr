/**
 * \file            synth.c
 * \brief           Chip-tune synthesizer — procedural SFX generation
 */

#include "synth.h"

#include <SDL3/SDL.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Pitch-to-frequency table (equal temperament, A4 = 440 Hz)          */
/* ------------------------------------------------------------------ */

/**
 * Pitch 1 = C-0 (16.35 Hz), pitch 96 = B-7 (3951.07 Hz).
 * Semitone formula: freq = 440 * 2^((pitch - 58) / 12)
 * where pitch 58 = A-4.
 */
static float prv_pitch_freq(uint8_t pitch)
{
    if (pitch == 0 || pitch > 96) {
        return 0.0f;
    }
    return 440.0f * SDL_powf(2.0f, (float)(pitch - 58) / 12.0f);
}

float dtr_synth_pitch_freq(uint8_t pitch)
{
    return prv_pitch_freq(pitch);
}

/* ------------------------------------------------------------------ */
/*  Note name lookup                                                   */
/* ------------------------------------------------------------------ */

static const char *prv_note_names[] = {
    "C-",
    "C#",
    "D-",
    "D#",
    "E-",
    "F-",
    "F#",
    "G-",
    "G#",
    "A-",
    "A#",
    "B-",
};

const char *dtr_synth_note_name(uint8_t pitch)
{
    static char buf[5];
    int32_t     note;
    int32_t     octave;

    if (pitch == 0 || pitch > 96) {
        return "---";
    }
    note   = (pitch - 1) % 12;
    octave = (pitch - 1) / 12;
    SDL_snprintf(buf, sizeof(buf), "%s%d", prv_note_names[note], octave);
    return buf;
}

/* ------------------------------------------------------------------ */
/*  Waveform generators (return -1.0 .. 1.0)                           */
/* ------------------------------------------------------------------ */

static uint32_t prv_noise_state = 0x12345678;

static float prv_noise(void)
{
    prv_noise_state ^= prv_noise_state << 13;
    prv_noise_state ^= prv_noise_state >> 17;
    prv_noise_state ^= prv_noise_state << 5;
    return (float)(int32_t)prv_noise_state / (float)INT32_MAX;
}

/* ------------------------------------------------------------------ */
/*  PolyBLEP anti-aliasing helper                                      */
/* ------------------------------------------------------------------ */

/**
 * Polynomial bandlimited step correction.
 * \param t   Phase position (0..1)
 * \param dtp Phase increment per sample (freq / sample_rate)
 */
static float prv_polyblep(float ttt, float dtp)
{
    if (ttt < dtp) {
        ttt /= dtp;
        return ttt + ttt - ttt * ttt - 1.0f;
    }
    if (ttt > 1.0f - dtp) {
        ttt = (ttt - 1.0f) / dtp;
        return ttt * ttt + ttt + ttt + 1.0f;
    }
    return 0.0f;
}

static float prv_waveform(uint8_t wave, float phase)
{
    float val;
    float ret;

    switch (wave) {
        case DTR_WAVE_TRIANGLE:
            /* PICO-8: peak ±0.50 */
            return (1.0f - SDL_fabsf(4.0f * phase - 2.0f)) * 0.5f;

        case DTR_WAVE_TILTSAW:
            /* PICO-8: asymmetric triangle, ramp 87.5% up / 12.5% down, peak ±0.50 */
            val = 0.875f;
            ret = phase < val
                ? 2.0f * phase / val - 1.0f
                : 2.0f * (1.0f - phase) / (1.0f - val) - 1.0f;
            return ret * 0.5f;

        case DTR_WAVE_SAW:
            /* PICO-8: peak ±0.327 */
            return 0.653f * (phase < 0.5f ? phase : phase - 1.0f);

        case DTR_WAVE_SQUARE:
            /* PICO-8: 50% duty, peak ±0.25 */
            return phase < 0.5f ? 0.25f : -0.25f;

        case DTR_WAVE_PULSE:
            /* PICO-8: ~31.6% duty, peak ±0.25 */
            return phase < 0.316f ? 0.25f : -0.25f;

        case DTR_WAVE_ORGAN:
            /* PICO-8: dual-speed triangle, peak ±0.333 */
            ret = phase < 0.5f
                ? 3.0f - SDL_fabsf(24.0f * phase - 6.0f)
                : 1.0f - SDL_fabsf(16.0f * phase - 12.0f);
            return ret / 9.0f;

        case DTR_WAVE_NOISE:
            return prv_noise() * 0.5f;

        case DTR_WAVE_PHASER:
            /* PICO-8: two detuned triangles, peak ±0.50 */
            ret = 2.0f - SDL_fabsf(8.0f * phase - 4.0f);
            ret += 1.0f - SDL_fabsf(4.0f * phase - 2.0f);
            return ret / 6.0f;

        default:
            return 0.0f;
    }
}

float dtr_synth_waveform(uint8_t wave, float phase)
{
    return prv_waveform(wave, phase);
}

/**
 * Band-limited waveform: applies PolyBLEP correction to
 * discontinuous waveforms (square, pulse, saw) to reduce aliasing.
 * \param wave  DTR_WAVE_* type
 * \param phase 0..1 oscillator phase
 * \param dtp   Phase increment per sample (freq / sample_rate)
 */
static float prv_waveform_bl(uint8_t wave, float phase, float dtp)
{
    float val;
    float blp;

    switch (wave) {
        case DTR_WAVE_SQUARE:
            /* PICO-8 peak: ±0.25 — PolyBLEP at 50% duty */
            val = phase < 0.5f ? 0.25f : -0.25f;
            val += prv_polyblep(phase, dtp) * 0.25f;
            blp = phase + 0.5f;
            if (blp >= 1.0f) {
                blp -= 1.0f;
            }
            val -= prv_polyblep(blp, dtp) * 0.25f;
            return val;

        case DTR_WAVE_PULSE:
            /* PICO-8 peak: ±0.25 — PolyBLEP at ~31.6% duty */
            val = phase < 0.316f ? 0.25f : -0.25f;
            val += prv_polyblep(phase, dtp) * 0.25f;
            blp = phase + (1.0f - 0.316f);
            if (blp >= 1.0f) {
                blp -= 1.0f;
            }
            val -= prv_polyblep(blp, dtp) * 0.25f;
            return val;

        case DTR_WAVE_SAW:
            /* PICO-8 peak: ±0.327 — PolyBLEP on naive saw */
            val = 0.653f * (phase < 0.5f ? phase : phase - 1.0f);
            val -= prv_polyblep(phase, dtp) * 0.327f;
            return val;

        default:
            /* Triangle, tilted_saw, organ, noise, phaser — no discontinuity */
            return prv_waveform(wave, phase);
    }
}

float dtr_synth_waveform_bl(uint8_t wave, float phase, float dtp)
{
    return prv_waveform_bl(wave, phase, dtp);
}

/* ------------------------------------------------------------------ */
/*  PCM rendering                                                      */
/* ------------------------------------------------------------------ */

/**
 * Speed value maps to samples per note (PICO-8 style: higher = slower):
 *   samples_per_note = speed * (sample_rate / 128)
 * Each speed unit = 1/128 second. UI range: 1-32, clamped internally.
 */
static int32_t prv_samples_per_note(uint8_t speed)
{
    int32_t spd;

    spd = speed;
    if (spd < 1) {
        spd = 1;
    }
    if (spd > 32) {
        spd = 32;
    }
    return spd * (DTR_SYNTH_SAMPLE_RATE / 128);
}

int16_t *dtr_synth_render(const dtr_synth_sfx_t *sfx, size_t *out_len)
{
    int32_t  spn;        /* samples per note */
    int32_t  total;      /* total samples */
    int32_t  note_count; /* how many notes to render */
    int16_t *buf;
    int32_t  idx;
    float    phase;
    float    freq;

    if (sfx == NULL || out_len == NULL) {
        return NULL;
    }

    spn = prv_samples_per_note(sfx->speed);

    /*
     * If looping is active, render many cycles of the loop region
     * [loop_start..loop_end] to fill ~60 seconds of audio.
     * The caller uses MIX_SetTrackLoops(-1) for truly infinite playback.
     */
    if (sfx->loop_end > 0 && sfx->loop_end >= sfx->loop_start && sfx->loop_end < DTR_SYNTH_NOTES) {
        int32_t loop_len;
        int32_t repeats;

        loop_len = sfx->loop_end - sfx->loop_start + 1;
        repeats  = (DTR_SYNTH_SAMPLE_RATE * 60) / (loop_len * spn);
        if (repeats < 1) {
            repeats = 1;
        }
        note_count = loop_len * repeats;
    } else {
        note_count = DTR_SYNTH_NOTES;
    }

    total = note_count * spn;

    buf = DTR_CALLOC((size_t)total, sizeof(int16_t));
    if (buf == NULL) {
        return NULL;
    }

    idx   = 0;
    phase = 0.0f;

    /* Reset noise state for deterministic output */
    prv_noise_state = 0x12345678;

    {
    uint8_t prev_key = 49;   /* C-4 = PICO-8 default C-2 (261.6 Hz) */
    float   prev_vol = 0.0f;
    int32_t first_nti;
    float   smooth_vol_off;

    /* Initialize volume smoother to the first note's volume so the
     * opening of the SFX doesn't get an unintended ramp-up. */
    first_nti = 0;
    if (sfx->loop_end > 0 && sfx->loop_end >= sfx->loop_start &&
        sfx->loop_start < DTR_SYNTH_NOTES) {
        first_nti = sfx->loop_start;
    }
    smooth_vol_off = (float)sfx->notes[first_nti].volume / 7.0f;

    for (int32_t rendered = 0; rendered < note_count; ++rendered) {
        const dtr_synth_note_t *note;
        float                   vol;
        float                   base_freq;
        uint8_t                 wave;
        int32_t                 nti;

        /* Map rendered index to actual note index, wrapping in loop region */
        if (sfx->loop_end > 0 && sfx->loop_end >= sfx->loop_start &&
            sfx->loop_end < DTR_SYNTH_NOTES) {
            int32_t loop_len;

            loop_len = sfx->loop_end - sfx->loop_start + 1;
            nti      = sfx->loop_start + (rendered % loop_len);
        } else {
            nti = rendered;
        }

        note      = &sfx->notes[nti];
        base_freq = prv_pitch_freq(note->pitch);
        vol       = (float)note->volume / 7.0f;
        wave      = note->waveform;

        if (note->pitch == 0 || vol <= 0.0f) {
            /* Rest — silence */
            for (int32_t sid = 0; sid < spn; ++sid) {
                buf[idx++] = 0;
            }
            phase = 0.0f;
            /* Update previous-note tracking for slide (PICO-8) */
            if (prev_key != note->pitch) {
                prev_vol = prev_key > 0 ? vol : 0.0f;
            }
            prev_key = note->pitch;
            continue;
        }

        for (int32_t sid = 0; sid < spn; ++sid) {
            float sample_vol;
            float cur_freq;
            float cur_phase;
            float frac;
            float sample;

            frac = (float)sid / (float)spn;

            /* Apply effects — PICO-8 compatible (matches zepto8) */
            cur_freq   = base_freq;
            sample_vol = vol;

            switch (note->effect) {
                case DTR_FX_SLIDE: {
                    /* Slide from previous note's pitch/vol to current */
                    float prev_freq;

                    prev_freq = prev_key > 0
                        ? prv_pitch_freq(prev_key)
                        : base_freq;
                    cur_freq   = prev_freq
                        + (base_freq - prev_freq) * frac;
                    sample_vol = prev_vol + (vol - prev_vol) * frac;
                    break;
                }
                case DTR_FX_VIBRATO: {
                    /* Triangle-wave LFO at 7.5 Hz, ±quarter semitone */
                    float time_s;
                    float ttt;

                    time_s = (float)idx
                        / (float)DTR_SYNTH_SAMPLE_RATE;
                    ttt = SDL_fabsf(
                        SDL_fmodf(7.5f * time_s, 1.0f) - 0.5f)
                        - 0.25f;
                    cur_freq = base_freq
                        * (1.0f + 0.059463094359f * ttt);
                    break;
                }
                case DTR_FX_DROP:
                    cur_freq = base_freq * (1.0f - frac);
                    if (cur_freq < 1.0f) {
                        cur_freq = 1.0f;
                    }
                    break;
                case DTR_FX_FADE_IN:
                    sample_vol = vol * frac;
                    break;
                case DTR_FX_FADE_OUT:
                    sample_vol = vol * (1.0f - frac);
                    break;
                case DTR_FX_ARPF:
                case DTR_FX_ARPS: {
                    /* 4-note group arpeggio (PICO-8 style).
                     * Cycles through notes (nti & ~3) .. (nti & ~3)+3. */
                    float   time_s;
                    int32_t mrate;
                    int32_t ncyc;
                    int32_t arp_note;

                    time_s = (float)idx
                        / (float)DTR_SYNTH_SAMPLE_RATE;
                    mrate  = (sfx->speed <= 8 ? 32 : 16)
                        / (note->effect == DTR_FX_ARPF ? 4 : 8);
                    ncyc     = (int32_t)(mrate * 7.5f * time_s);
                    arp_note = (nti & ~3) | (ncyc & 3);
                    if (arp_note < DTR_SYNTH_NOTES &&
                        sfx->notes[arp_note].pitch > 0) {
                        cur_freq = prv_pitch_freq(
                            sfx->notes[arp_note].pitch);
                    }
                    break;
                }
                default:
                    break;
            }

            /* Smooth volume transitions at note boundaries (prevents
             * pops when a fade-out note is followed by a loud note).
             * Fade effects supply their own envelope so they bypass
             * the slow smoother — just keep smooth_vol_off in sync. */
            if (note->effect == DTR_FX_FADE_IN ||
                note->effect == DTR_FX_FADE_OUT) {
                smooth_vol_off = sample_vol;
            } else {
                float svd;

                svd = sample_vol - smooth_vol_off;
                if (svd > 0.0001f || svd < -0.0001f) {
                    smooth_vol_off += svd * 0.005f;
                } else {
                    smooth_vol_off = sample_vol;
                }
                sample_vol = smooth_vol_off;
            }

            /* Advance phase */
            freq = cur_freq;
            phase += freq / (float)DTR_SYNTH_SAMPLE_RATE;
            phase -= SDL_floorf(phase);

            /* Generate waveform sample (band-limited) */
            cur_phase = phase;
            sample    = prv_waveform_bl(
                wave, cur_phase,
                freq / (float)DTR_SYNTH_SAMPLE_RATE);

            /* Apply volume and clamp */
            sample *= sample_vol;
            if (sample > 1.0f) {
                sample = 1.0f;
            }
            if (sample < -1.0f) {
                sample = -1.0f;
            }

            buf[idx++] = (int16_t)(sample * 32767.0f);
        }

        /* Update previous-note tracking for slide (PICO-8) */
        if (prev_key != note->pitch) {
            prev_vol = prev_key > 0 ? vol : 0.0f;
        }
        prev_key = note->pitch;
    }
    } /* end prev_key/prev_vol scope */

    /* Fade-out ramp for non-looping SFX to prevent click at end */
    if (!(sfx->loop_end > 0 && sfx->loop_end >= sfx->loop_start &&
          sfx->loop_end < DTR_SYNTH_NOTES)) {
        int32_t ramp;

        ramp = 64;
        if (ramp > total) {
            ramp = total;
        }
        for (int32_t ri = 0; ri < ramp; ++ri) {
            float t;

            t                      = (float)(ramp - ri) / (float)ramp;
            buf[total - ramp + ri] = (int16_t)((float)buf[total - ramp + ri] * t);
        }
    }

    *out_len = (size_t)total;
    return buf;
}
