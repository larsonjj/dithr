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
 *
 * Precomputed to avoid per-sample powf() calls in the render loop.
 */
/* clang-format off */
static const float prv_pitch_table[97] = {
    0.0f,          /*  0: rest / invalid */
    16.351598f,    /*  1: C-0  */   17.323914f,    /*  2: C#0  */
    18.354048f,    /*  3: D-0  */   19.445436f,    /*  4: D#0  */
    20.601722f,    /*  5: E-0  */   21.826764f,    /*  6: F-0  */
    23.124651f,    /*  7: F#0  */   24.499714f,    /*  8: G-0  */
    25.956543f,    /*  9: G#0  */   27.500000f,    /* 10: A-0  */
    29.135235f,    /* 11: A#0  */   30.867706f,    /* 12: B-0  */
    32.703196f,    /* 13: C-1  */   34.647827f,    /* 14: C#1  */
    36.708096f,    /* 15: D-1  */   38.890873f,    /* 16: D#1  */
    41.203445f,    /* 17: E-1  */   43.653528f,    /* 18: F-1  */
    46.249302f,    /* 19: F#1  */   48.999429f,    /* 20: G-1  */
    51.913086f,    /* 21: G#1  */   55.000000f,    /* 22: A-1  */
    58.270470f,    /* 23: A#1  */   61.735413f,    /* 24: B-1  */
    65.406391f,    /* 25: C-2  */   69.295654f,    /* 26: C#2  */
    73.416191f,    /* 27: D-2  */   77.781746f,    /* 28: D#2  */
    82.406889f,    /* 29: E-2  */   87.307057f,    /* 30: F-2  */
    92.498604f,    /* 31: F#2  */   97.998857f,    /* 32: G-2  */
    103.826172f,   /* 33: G#2  */  110.000000f,    /* 34: A-2  */
    116.540940f,   /* 35: A#2  */  123.470825f,    /* 36: B-2  */
    130.812783f,   /* 37: C-3  */  138.591309f,    /* 38: C#3  */
    146.832382f,   /* 39: D-3  */  155.563492f,    /* 40: D#3  */
    164.813778f,   /* 41: E-3  */  174.614113f,    /* 42: F-3  */
    184.997208f,   /* 43: F#3  */  195.997714f,    /* 44: G-3  */
    207.652344f,   /* 45: G#3  */  220.000000f,    /* 46: A-3  */
    233.081879f,   /* 47: A#3  */  246.941650f,    /* 48: B-3  */
    261.625565f,   /* 49: C-4  */  277.182617f,    /* 50: C#4  */
    293.664764f,   /* 51: D-4  */  311.126984f,    /* 52: D#4  */
    329.627557f,   /* 53: E-4  */  349.228226f,    /* 54: F-4  */
    369.994415f,   /* 55: F#4  */  391.995428f,    /* 56: G-4  */
    415.304688f,   /* 57: G#4  */  440.000000f,    /* 58: A-4  */
    466.163757f,   /* 59: A#4  */  493.883301f,    /* 60: B-4  */
    523.251130f,   /* 61: C-5  */  554.365234f,    /* 62: C#5  */
    587.329529f,   /* 63: D-5  */  622.253967f,    /* 64: D#5  */
    659.255115f,   /* 65: E-5  */  698.456452f,    /* 66: F-5  */
    739.988831f,   /* 67: F#5  */  783.990856f,    /* 68: G-5  */
    830.609375f,   /* 69: G#5  */  880.000000f,    /* 70: A-5  */
    932.327515f,   /* 71: A#5  */  987.766602f,    /* 72: B-5  */
    1046.502319f,  /* 73: C-6  */ 1108.730469f,    /* 74: C#6  */
    1174.659058f,  /* 75: D-6  */ 1244.507935f,    /* 76: D#6  */
    1318.510254f,  /* 77: E-6  */ 1396.912903f,    /* 78: F-6  */
    1479.977661f,  /* 79: F#6  */ 1567.981689f,    /* 80: G-6  */
    1661.218750f,  /* 81: G#6  */ 1760.000000f,    /* 82: A-6  */
    1864.655029f,  /* 83: A#6  */ 1975.533203f,    /* 84: B-6  */
    2093.004639f,  /* 85: C-7  */ 2217.460938f,    /* 86: C#7  */
    2349.318115f,  /* 87: D-7  */ 2489.015869f,    /* 88: D#7  */
    2637.020508f,  /* 89: E-7  */ 2793.825806f,    /* 90: F-7  */
    2959.955322f,  /* 91: F#7  */ 3135.963379f,    /* 92: G-7  */
    3322.437500f,  /* 93: G#7  */ 3520.000000f,    /* 94: A-7  */
    3729.310059f,  /* 95: A#7  */ 3951.066406f,    /* 96: B-7  */
};
/* clang-format on */

static float prv_pitch_freq(uint8_t pitch)
{
    if (pitch > 96) {
        return 0.0f;
    }
    return prv_pitch_table[pitch];
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

static float prv_noise(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return (float)(int32_t)*state / (float)INT32_MAX;
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

static float prv_waveform_ns(uint8_t wave, float phase, uint32_t *noise_state)
{
    float val;
    float ret;

    switch (wave) {
        case DTR_WAVE_TRIANGLE:
            /* peak ±0.50 */
            return (1.0f - SDL_fabsf(4.0f * phase - 2.0f)) * 0.5f;

        case DTR_WAVE_TILTSAW:
            /* Asymmetric triangle, ramp 87.5% up / 12.5% down, peak ±0.50 */
            val = 0.875f;
            ret = phase < val ? 2.0f * phase / val - 1.0f :
                                2.0f * (1.0f - phase) / (1.0f - val) - 1.0f;
            return ret * 0.5f;

        case DTR_WAVE_SAW:
            /* peak ±0.327 */
            return 0.653f * (phase < 0.5f ? phase : phase - 1.0f);

        case DTR_WAVE_SQUARE:
            /* 50% duty, peak ±0.25 */
            return phase < 0.5f ? 0.25f : -0.25f;

        case DTR_WAVE_PULSE:
            /* ~31.6% duty, peak ±0.25 */
            return phase < 0.316f ? 0.25f : -0.25f;

        case DTR_WAVE_ORGAN:
            /* Dual-speed triangle, peak ±0.333 */
            ret = phase < 0.5f ? 3.0f - SDL_fabsf(24.0f * phase - 6.0f) :
                                 1.0f - SDL_fabsf(16.0f * phase - 12.0f);
            return ret / 9.0f;

        case DTR_WAVE_NOISE:
            return prv_noise(noise_state) * 0.5f;

        case DTR_WAVE_PHASER:
            /* Two detuned triangles, peak ±0.50 */
            ret = 2.0f - SDL_fabsf(8.0f * phase - 4.0f);
            ret += 1.0f - SDL_fabsf(4.0f * phase - 2.0f);
            return ret / 6.0f;

        default:
            return 0.0f;
    }
}

float dtr_synth_waveform(uint8_t wave, float phase)
{
    static uint32_t pub_noise_state = 0x12345678;
    return prv_waveform_ns(wave, phase, &pub_noise_state);
}

/**
 * Band-limited waveform: applies PolyBLEP correction to
 * discontinuous waveforms (square, pulse, saw) to reduce aliasing.
 * \param wave        DTR_WAVE_* type
 * \param phase       0..1 oscillator phase
 * \param dtp         Phase increment per sample (freq / sample_rate)
 * \param noise_state Noise PRNG state (passed through for noise waveform)
 */
static float prv_waveform_bl_ns(uint8_t wave, float phase, float dtp, uint32_t *noise_state)
{
    float val;
    float blp;

    switch (wave) {
        case DTR_WAVE_SQUARE:
            /* Peak ±0.25 — PolyBLEP at 50% duty */
            val = phase < 0.5f ? 0.25f : -0.25f;
            val += prv_polyblep(phase, dtp) * 0.25f;
            blp = phase + 0.5f;
            if (blp >= 1.0f) {
                blp -= 1.0f;
            }
            val -= prv_polyblep(blp, dtp) * 0.25f;
            return val;

        case DTR_WAVE_PULSE:
            /* Peak ±0.25 — PolyBLEP at ~31.6% duty */
            val = phase < 0.316f ? 0.25f : -0.25f;
            val += prv_polyblep(phase, dtp) * 0.25f;
            blp = phase + (1.0f - 0.316f);
            if (blp >= 1.0f) {
                blp -= 1.0f;
            }
            val -= prv_polyblep(blp, dtp) * 0.25f;
            return val;

        case DTR_WAVE_SAW:
            /* Peak ±0.327 — PolyBLEP on naive saw */
            val = 0.653f * (phase < 0.5f ? phase : phase - 1.0f);
            val -= prv_polyblep(phase, dtp) * 0.327f;
            return val;

        default:
            /* Triangle, tilted_saw, organ, noise, phaser — no discontinuity */
            return prv_waveform_ns(wave, phase, noise_state);
    }
}

float dtr_synth_waveform_bl(uint8_t wave, float phase, float dtp)
{
    static uint32_t pub_noise_state = 0x12345678;
    return prv_waveform_bl_ns(wave, phase, dtp, &pub_noise_state);
}

/* ------------------------------------------------------------------ */
/*  PCM rendering                                                      */
/* ------------------------------------------------------------------ */

/**
 * Speed value maps to samples per note (higher = slower):
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
    uint32_t noise_state;

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
    noise_state = 0x12345678;

    {
        uint8_t prev_key = 49; /* C-4 (261.6 Hz) */
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
                /* Update previous-note tracking for slide */
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

                /* Apply effects */
                cur_freq   = base_freq;
                sample_vol = vol;

                switch (note->effect) {
                    case DTR_FX_SLIDE: {
                        /* Slide from previous note's pitch/vol to current */
                        float prev_freq;

                        prev_freq  = prev_key > 0 ? prv_pitch_freq(prev_key) : base_freq;
                        cur_freq   = prev_freq + (base_freq - prev_freq) * frac;
                        sample_vol = prev_vol + (vol - prev_vol) * frac;
                        break;
                    }
                    case DTR_FX_VIBRATO: {
                        /* Triangle-wave LFO at 7.5 Hz, ±quarter semitone */
                        float time_s;
                        float ttt;

                        time_s   = (float)idx / (float)DTR_SYNTH_SAMPLE_RATE;
                        ttt      = SDL_fabsf(SDL_fmodf(7.5f * time_s, 1.0f) - 0.5f) - 0.25f;
                        cur_freq = base_freq * (1.0f + 0.059463094359f * ttt);
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
                        /* 4-note group arpeggio.
                         * Cycles through notes (nti & ~3) .. (nti & ~3)+3. */
                        float   time_s;
                        int32_t mrate;
                        int32_t ncyc;
                        int32_t arp_note;

                        time_s = (float)idx / (float)DTR_SYNTH_SAMPLE_RATE;
                        mrate = (sfx->speed <= 8 ? 32 : 16) / (note->effect == DTR_FX_ARPF ? 4 : 8);
                        ncyc  = (int32_t)((float)mrate * 7.5f * time_s);
                        arp_note = (nti & ~3) | (ncyc & 3);
                        if (arp_note < DTR_SYNTH_NOTES && sfx->notes[arp_note].pitch > 0) {
                            cur_freq = prv_pitch_freq(sfx->notes[arp_note].pitch);
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
                if (note->effect == DTR_FX_FADE_IN || note->effect == DTR_FX_FADE_OUT) {
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
                sample    = prv_waveform_bl_ns(
                    wave, cur_phase, freq / (float)DTR_SYNTH_SAMPLE_RATE, &noise_state);

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

            /* Update previous-note tracking for slide */
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
