/**
 * \file            synth_api.c
 * \brief           JS synth namespace — procedural SFX creation and preview
 */

#include "../audio.h"
#include "../synth.h"
#include "api_common.h"

#include <SDL3/SDL_audio.h>
#include <SDL3_mixer/SDL_mixer.h>

#define CON(ctx) dtr_api_get_console(ctx)
#define AUD(ctx) (CON(ctx)->audio)

/* ------------------------------------------------------------------ */
/*  Real-time synthesis playback state                                 */
/* ------------------------------------------------------------------ */

/**
 * State for real-time sample generation in the audio callback.
 * Instead of pre-rendering the entire SFX to a buffer, we generate
 * samples on-the-fly from the live definition.  When a note changes,
 * the next sample naturally uses the new parameters — zero clicks.
 */
typedef struct dtr_synth_voice {
    SDL_AudioStream   *stream;
    MIX_Track         *track;
    const dtr_synth_sfx_t *def; /* live pointer into store->defs[] */
    float              phase;       /* oscillator phase accumulator 0..1 */
    int32_t            note_idx;    /* current note index in pattern */
    int32_t            note_pos;    /* sample position within current note */
    int32_t            total_pos;   /* total sample position since start */
    bool               playing;
    bool               looping;
    bool               fade_out;
    int32_t            fade_rem;
    uint32_t           noise_state; /* per-voice noise PRNG */
    /* Parameter crossfade — smooths edits during playback */
    uint8_t            latch_pitch; /* latched pitch for current note */
    uint8_t            latch_wave;  /* latched waveform for current note */
    uint8_t            latch_fx;    /* latched effect for current note */
    float              smooth_vol;  /* smoothed volume (0..1), ramped */
    float              last_out;    /* previous output sample (S16 scale) */
    float              xfade_base;  /* output at start of crossfade */
    int32_t            xfade_rem;   /* remaining crossfade samples */
} dtr_synth_voice_t;

/**
 * State for single-note preview (uses pre-rendered buffer — edits
 * don't happen during note preview, so the old approach is fine).
 */
typedef struct dtr_synth_note_voice {
    SDL_AudioStream *stream;
    MIX_Track       *track;
    int16_t         *buf;
    size_t           buf_len;
    size_t           pos;
    bool             playing;
    bool             fade_out;
    int32_t          fade_rem;
    /* Crossfade from previous note to avoid pop on rapid entry */
    float            last_out;    /* last output sample (S16 scale) */
    float            xfade_base;  /* output at start of crossfade */
    int32_t          xfade_rem;   /* remaining crossfade samples */
} dtr_synth_note_voice_t;

/* ------------------------------------------------------------------ */
/*  In-editor synth SFX storage                                        */
/* ------------------------------------------------------------------ */

typedef struct dtr_synth_store {
    dtr_synth_sfx_t       defs[DTR_SYNTH_MAX_SFX];
    dtr_synth_voice_t     play;       /* main pattern playback (real-time) */
    dtr_synth_note_voice_t note;      /* single-note preview (pre-rendered) */
    int32_t               count;
    bool                  initialized;
} dtr_synth_store_t;

/**
 * Single global store (editor is the only user).
 * A per-console approach would require extending dtr_console_t;
 * this keeps the change minimal.
 */
static dtr_synth_store_t g_store;

static dtr_synth_store_t *prv_get_store(void)
{
    if (!g_store.initialized) {
        SDL_memset(&g_store, 0, sizeof(g_store));
        g_store.initialized = true;
    }
    return &g_store;
}

/* ------------------------------------------------------------------ */
/*  Real-time synthesis callback                                       */
/* ------------------------------------------------------------------ */

#define FADE_SAMPLES       128
#define PARAM_XFADE        128  /* crossfade length for mid-note edits */
#define NOTE_XFADE         96   /* crossfade for rapid note preview */
#define FADE_IN_SAMPLES    64   /* fade-in at playback start */
#define VOL_RAMP_SAMPLES   512  /* ~11.6ms at 44100 Hz */
#define VOL_RAMP_STEP      (1.0f / (float)VOL_RAMP_SAMPLES)

/**
 * Compute samples-per-note from speed value.
 */
static int32_t prv_spn(uint8_t speed)
{
    int32_t spd;

    spd = speed;
    if (spd < 1) {
        spd = 1;
    }
    if (spd > 32) {
        spd = 32;
    }
    return (33 - spd) * (DTR_SYNTH_SAMPLE_RATE / 120);
}

/**
 * Simple PRNG for noise waveform (matches synth.c).
 */
static float prv_noise(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return (float)(int32_t)*state / (float)INT32_MAX;
}

/**
 * Generate one sample from the voice's current state, then advance.
 */
static int16_t prv_voice_sample(dtr_synth_voice_t *v)
{
    const dtr_synth_sfx_t  *sfx;
    const dtr_synth_note_t *note;
    int32_t                 spn;
    int32_t                 nti;
    float                   base_freq;
    float                   vol;
    float                   frac;
    float                   cur_freq;
    float                   sample_vol;
    float                   sample;
    float                   output;

    sfx = v->def;
    if (sfx == NULL) {
        return 0;
    }

    spn = prv_spn(sfx->speed);

    /* Determine which note we're on */
    nti      = v->note_idx;
    note     = &sfx->notes[nti];

    /* Latch parameters at the start of each note (no crossfade —
     * note boundaries are natural transitions). */
    if (v->note_pos == 0) {
        v->latch_pitch = note->pitch;
        v->latch_wave  = note->waveform;
        v->latch_fx    = note->effect;
        v->smooth_vol  = (float)note->volume / 7.0f;
    }

    /* Detect mid-note waveform/pitch/effect change from a synth.set()
     * edit.  Start a crossfade from the previous output level to avoid
     * a waveform discontinuity.  Volume is handled separately via
     * smooth_vol ramp below (output-level crossfade causes DC-offset
     * artifacts for pure amplitude changes). */
    if (v->note_pos > 0 &&
        (note->pitch    != v->latch_pitch ||
         note->waveform != v->latch_wave ||
         note->effect   != v->latch_fx)) {
        v->xfade_base  = v->last_out;
        v->xfade_rem   = PARAM_XFADE;
        v->latch_pitch = note->pitch;
        v->latch_wave  = note->waveform;
        v->latch_fx    = note->effect;
    }

    /* Smoothly ramp volume toward target — avoids pop on mid-note
     * volume edits while keeping the waveform shape clean. */
    {
        float target_vol;

        target_vol = (float)note->volume / 7.0f;
        if (v->smooth_vol < target_vol) {
            v->smooth_vol += VOL_RAMP_STEP;
            if (v->smooth_vol > target_vol) {
                v->smooth_vol = target_vol;
            }
        } else if (v->smooth_vol > target_vol) {
            v->smooth_vol -= VOL_RAMP_STEP;
            if (v->smooth_vol < target_vol) {
                v->smooth_vol = target_vol;
            }
        }
    }

    base_freq = dtr_synth_pitch_freq(note->pitch);
    vol       = v->smooth_vol;
    frac      = (float)v->note_pos / (float)spn;

    if (note->pitch == 0 || vol <= 0.0f) {
        sample = 0.0f;
        v->phase = 0.0f;
    } else {
        cur_freq   = base_freq;
        sample_vol = vol;

        /* Effects (mirroring synth.c) */
        switch (note->effect) {
            case DTR_FX_SLIDE_UP:
                cur_freq = base_freq * (1.0f + frac * 0.5f);
                break;
            case DTR_FX_SLIDE_DOWN:
                cur_freq = base_freq * (1.0f - frac * 0.3f);
                if (cur_freq < 20.0f) {
                    cur_freq = 20.0f;
                }
                break;
            case DTR_FX_DROP:
                cur_freq = base_freq * (1.0f - frac * frac * 0.9f);
                if (cur_freq < 20.0f) {
                    cur_freq = 20.0f;
                }
                break;
            case DTR_FX_FADE_IN:
                sample_vol = vol * frac;
                break;
            case DTR_FX_FADE_OUT:
                sample_vol = vol * (1.0f - frac);
                break;
            case DTR_FX_ARPF: {
                int32_t arp_step;

                arp_step = ((v->note_pos * 30) / spn) % 3;
                if (arp_step == 1) {
                    cur_freq = base_freq * SDL_powf(2.0f, 4.0f / 12.0f);
                } else if (arp_step == 2) {
                    cur_freq = base_freq * SDL_powf(2.0f, 7.0f / 12.0f);
                }
                break;
            }
            case DTR_FX_ARPS: {
                int32_t arp_step;

                arp_step = ((v->note_pos * 6) / spn) % 3;
                if (arp_step == 1) {
                    cur_freq = base_freq * SDL_powf(2.0f, 4.0f / 12.0f);
                } else if (arp_step == 2) {
                    cur_freq = base_freq * SDL_powf(2.0f, 7.0f / 12.0f);
                }
                break;
            }
            default:
                break;
        }

        /* Advance phase */
        v->phase += cur_freq / (float)DTR_SYNTH_SAMPLE_RATE;
        v->phase -= SDL_floorf(v->phase);

        /* Generate waveform (band-limited to reduce aliasing) */
        if (note->waveform == DTR_WAVE_NOISE) {
            sample = prv_noise(&v->noise_state);
        } else {
            sample = dtr_synth_waveform_bl(
                note->waveform, v->phase,
                cur_freq / (float)DTR_SYNTH_SAMPLE_RATE);
        }

        sample *= sample_vol * 0.8f;
        if (sample > 1.0f) {
            sample = 1.0f;
        }
        if (sample < -1.0f) {
            sample = -1.0f;
        }
    }

    /* Advance position */
    v->note_pos++;
    v->total_pos++;
    if (v->note_pos >= spn) {
        v->note_pos = 0;
        v->note_idx++;

        /* Handle looping / end */
        if (sfx->loop_end > 0 && sfx->loop_end >= sfx->loop_start &&
            sfx->loop_end < DTR_SYNTH_NOTES) {
            if (v->note_idx > sfx->loop_end) {
                v->note_idx = sfx->loop_start;
            }
        } else if (v->note_idx >= DTR_SYNTH_NOTES) {
            v->playing = false;
        }
    }

    output = sample * 32767.0f;

    /* Crossfade from previous output level to smooth mid-note edits */
    if (v->xfade_rem > 0) {
        float ttt;

        ttt    = (float)v->xfade_rem / (float)PARAM_XFADE;
        output = v->xfade_base * ttt + output * (1.0f - ttt);
        v->xfade_rem--;
    }

    /* Fade-in at playback start to avoid silent→loud pop */
    if (v->total_pos <= FADE_IN_SAMPLES) {
        float fin;

        fin    = (float)v->total_pos / (float)FADE_IN_SAMPLES;
        output *= fin;
    }

    v->last_out = output;

    if (output > 32767.0f) {
        output = 32767.0f;
    }
    if (output < -32767.0f) {
        output = -32767.0f;
    }
    return (int16_t)output;
}

/**
 * AudioStream get-callback for pattern playback (real-time synthesis).
 */
static void SDLCALL
prv_voice_get(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
    dtr_synth_voice_t *v;
    int                need;
    int16_t            tmp[512];

    (void)total;
    v    = (dtr_synth_voice_t *)userdata;
    need = additional / (int)sizeof(int16_t);

    while (need > 0) {
        int chunk;
        int ci;

        chunk = need > 512 ? 512 : need;

        if (!v->playing) {
            SDL_memset(tmp, 0, (size_t)chunk * sizeof(int16_t));
            SDL_PutAudioStreamData(stream, tmp, chunk * (int)sizeof(int16_t));
            need -= chunk;
            continue;
        }

        for (ci = 0; ci < chunk; ++ci) {
            int16_t s;

            if (!v->playing) {
                tmp[ci] = 0;
                continue;
            }

            s = prv_voice_sample(v);

            /* Apply fade-out if requested */
            if (v->fade_out && v->fade_rem > 0) {
                float t;

                t   = (float)v->fade_rem / (float)FADE_SAMPLES;
                s   = (int16_t)((float)s * t);
                v->fade_rem--;
                if (v->fade_rem <= 0) {
                    v->playing  = false;
                    v->fade_out = false;
                    s = 0;
                }
            }

            tmp[ci] = s;
        }

        SDL_PutAudioStreamData(stream, tmp, chunk * (int)sizeof(int16_t));
        need -= chunk;
    }
}

/**
 * AudioStream get-callback for note preview (pre-rendered buffer).
 * Applies a crossfade at the start when a new buffer replaces an
 * in-progress note to avoid a pop from the amplitude discontinuity.
 */
static void SDLCALL
prv_note_get(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
    dtr_synth_note_voice_t *nv;
    int                     need;

    (void)total;
    nv   = (dtr_synth_note_voice_t *)userdata;
    need = additional / (int)sizeof(int16_t);

    if (!nv->playing || nv->buf == NULL || nv->buf_len == 0) {
        int16_t silence[256];

        SDL_memset(silence, 0, sizeof(silence));
        while (need > 0) {
            int chunk;

            chunk = need > 256 ? 256 : need;
            SDL_PutAudioStreamData(stream, silence,
                                   chunk * (int)sizeof(int16_t));
            need -= chunk;
        }
        nv->last_out = 0.0f;
        return;
    }

    while (need > 0) {
        int avail;

        avail = (int)(nv->buf_len - nv->pos);
        if (avail <= 0) {
            nv->playing = false;
            {
                int16_t silence[256];

                SDL_memset(silence, 0, sizeof(silence));
                while (need > 0) {
                    int chunk;

                    chunk = need > 256 ? 256 : need;
                    SDL_PutAudioStreamData(stream, silence,
                                           chunk * (int)sizeof(int16_t));
                    need -= chunk;
                }
            }
            nv->last_out = 0.0f;
            return;
        }

        {
            int chunk;

            chunk = need < avail ? need : avail;

            if (nv->fade_out && nv->fade_rem > 0) {
                int16_t tmp[512];
                int     seg;

                seg = chunk > 512 ? 512 : chunk;
                seg = seg > nv->fade_rem ? nv->fade_rem : seg;
                SDL_memcpy(tmp, nv->buf + nv->pos,
                           (size_t)seg * sizeof(int16_t));
                for (int ri = 0; ri < seg; ++ri) {
                    float t;

                    t      = (float)(nv->fade_rem - ri) / (float)FADE_SAMPLES;
                    tmp[ri] = (int16_t)((float)tmp[ri] * t);
                }
                nv->last_out = (float)tmp[seg - 1];
                SDL_PutAudioStreamData(stream, tmp,
                                       seg * (int)sizeof(int16_t));
                nv->pos      += (size_t)seg;
                need         -= seg;
                nv->fade_rem -= seg;
                if (nv->fade_rem <= 0) {
                    nv->playing  = false;
                    nv->fade_out = false;
                    nv->last_out = 0.0f;
                    {
                        int16_t silence[256];

                        SDL_memset(silence, 0, sizeof(silence));
                        while (need > 0) {
                            int c2;

                            c2 = need > 256 ? 256 : need;
                            SDL_PutAudioStreamData(stream, silence,
                                                   c2 * (int)sizeof(int16_t));
                            need -= c2;
                        }
                    }
                    return;
                }
            } else if (nv->xfade_rem > 0) {
                /* Crossfade from previous note's amplitude */
                int16_t tmp[512];
                int     seg;

                seg = chunk > 512 ? 512 : chunk;
                if (seg > nv->xfade_rem) {
                    seg = nv->xfade_rem;
                }
                SDL_memcpy(tmp, nv->buf + nv->pos,
                           (size_t)seg * sizeof(int16_t));
                for (int ri = 0; ri < seg; ++ri) {
                    float ttt;
                    float out;

                    ttt    = (float)(nv->xfade_rem - ri) / (float)NOTE_XFADE;
                    out    = nv->xfade_base * ttt +
                             (float)tmp[ri] * (1.0f - ttt);
                    if (out > 32767.0f) {
                        out = 32767.0f;
                    }
                    if (out < -32767.0f) {
                        out = -32767.0f;
                    }
                    tmp[ri] = (int16_t)out;
                }
                nv->last_out = (float)tmp[seg - 1];
                SDL_PutAudioStreamData(stream, tmp,
                                       seg * (int)sizeof(int16_t));
                nv->pos       += (size_t)seg;
                need          -= seg;
                nv->xfade_rem -= seg;
            } else {
                nv->last_out = (float)nv->buf[nv->pos + (size_t)chunk - 1];
                SDL_PutAudioStreamData(stream, nv->buf + nv->pos,
                                       chunk * (int)sizeof(int16_t));
                nv->pos += (size_t)chunk;
                need    -= chunk;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Stream/track setup helpers                                         */
/* ------------------------------------------------------------------ */

static void prv_ensure_voice(dtr_synth_voice_t *v, dtr_audio_t *aud)
{
    SDL_AudioSpec spec;

    if (v->stream != NULL || aud == NULL || aud->mixer == NULL) {
        return;
    }

    spec.freq     = DTR_SYNTH_SAMPLE_RATE;
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 1;

    v->stream = SDL_CreateAudioStream(&spec, &spec);
    if (v->stream == NULL) {
        return;
    }
    SDL_SetAudioStreamGetCallback(v->stream, prv_voice_get, v);

    v->track = MIX_CreateTrack(aud->mixer);
    if (v->track != NULL) {
        MIX_SetTrackAudioStream(v->track, v->stream);
    }
}

static void prv_ensure_note_voice(dtr_synth_note_voice_t *nv, dtr_audio_t *aud)
{
    SDL_AudioSpec spec;

    if (nv->stream != NULL || aud == NULL || aud->mixer == NULL) {
        return;
    }

    spec.freq     = DTR_SYNTH_SAMPLE_RATE;
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 1;

    nv->stream = SDL_CreateAudioStream(&spec, &spec);
    if (nv->stream == NULL) {
        return;
    }
    SDL_SetAudioStreamGetCallback(nv->stream, prv_note_get, nv);

    nv->track = MIX_CreateTrack(aud->mixer);
    if (nv->track != NULL) {
        MIX_SetTrackAudioStream(nv->track, nv->stream);
    }
}

static void prv_voice_start(dtr_synth_voice_t *v, const dtr_synth_sfx_t *def)
{
    SDL_LockAudioStream(v->stream);
    SDL_ClearAudioStream(v->stream); /* flush stale data */
    v->def         = def;
    v->phase       = 0.0f;
    v->note_idx    = 0;
    v->note_pos    = 0;
    v->total_pos   = 0;
    v->playing     = true;
    v->looping     = def->loop_end > 0 && def->loop_end >= def->loop_start;
    v->fade_out    = false;
    v->fade_rem    = 0;
    v->noise_state = 0x12345678;
    v->latch_pitch = 0;
    v->latch_wave  = 0;
    v->latch_fx    = 0;
    v->smooth_vol  = 0.0f;
    v->last_out    = 0.0f;
    v->xfade_base  = 0.0f;
    v->xfade_rem   = 0;
    SDL_UnlockAudioStream(v->stream);

    if (v->track != NULL && !MIX_TrackPlaying(v->track)) {
        SDL_PropertiesID props;

        props = SDL_CreateProperties();
        if (props != 0) {
            MIX_SetTrackLoops(v->track, -1);
            MIX_PlayTrack(v->track, props);
            SDL_DestroyProperties(props);
        }
    }
}

static void prv_voice_stop(dtr_synth_voice_t *v)
{
    if (v->stream == NULL) {
        return;
    }
    SDL_LockAudioStream(v->stream);
    if (v->playing) {
        v->fade_out = true;
        v->fade_rem = FADE_SAMPLES;
    }
    SDL_UnlockAudioStream(v->stream);
}

static void prv_note_voice_stop(dtr_synth_note_voice_t *nv)
{
    if (nv->stream == NULL) {
        return;
    }
    SDL_LockAudioStream(nv->stream);
    if (nv->playing) {
        nv->fade_out = true;
        nv->fade_rem = FADE_SAMPLES;
    }
    SDL_UnlockAudioStream(nv->stream);
}

/* ------------------------------------------------------------------ */
/*  synth.set(idx, notesArray, speed, loopStart, loopEnd)              */
/*                                                                     */
/*  notesArray: [{pitch, waveform, volume, effect}, ...]  (32 entries) */
/* ------------------------------------------------------------------ */

static JSValue js_synth_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_synth_store_t *store;
    dtr_synth_sfx_t    tmp; /* build into local copy first */
    int32_t            idx;
    JSValue            arr;
    int32_t            len;

    (void)this_val;
    if (argc < 2) {
        return JS_FALSE;
    }

    store = prv_get_store();
    idx   = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    if (idx < 0 || idx >= DTR_SYNTH_MAX_SFX) {
        return JS_FALSE;
    }

    SDL_memset(&tmp, 0, sizeof(dtr_synth_sfx_t));

    /* Read notes array */
    arr = argv[1];
    {
        JSValue len_val;

        len_val = JS_GetPropertyStr(ctx, arr, "length");
        JS_ToInt32(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);
    }
    if (len > DTR_SYNTH_NOTES) {
        len = DTR_SYNTH_NOTES;
    }

    for (int32_t nti = 0; nti < len; ++nti) {
        JSValue           elem;
        JSValue           val;
        dtr_synth_note_t *note;

        note = &tmp.notes[nti];
        elem = JS_GetPropertyUint32(ctx, arr, (uint32_t)nti);

        val = JS_GetPropertyStr(ctx, elem, "pitch");
        if (!JS_IsUndefined(val)) {
            int32_t v;

            JS_ToInt32(ctx, &v, val);
            note->pitch = (uint8_t)v;
        }
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, elem, "waveform");
        if (!JS_IsUndefined(val)) {
            int32_t v;

            JS_ToInt32(ctx, &v, val);
            note->waveform = (uint8_t)v;
        }
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, elem, "volume");
        if (!JS_IsUndefined(val)) {
            int32_t v;

            JS_ToInt32(ctx, &v, val);
            note->volume = (uint8_t)v;
        }
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, elem, "effect");
        if (!JS_IsUndefined(val)) {
            int32_t v;

            JS_ToInt32(ctx, &v, val);
            note->effect = (uint8_t)v;
        }
        JS_FreeValue(ctx, val);

        JS_FreeValue(ctx, elem);
    }

    tmp.speed      = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 2, 8);
    tmp.loop_start = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 3, 0);
    tmp.loop_end   = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 4, 0);

    /*
     * Copy into the live def atomically under the audio stream lock.
     * Without this, the callback thread can read a half-written def
     * (e.g. after memset zeroed it but before notes were filled in).
     */
    if (store->play.stream != NULL) {
        SDL_LockAudioStream(store->play.stream);
        SDL_memcpy(&store->defs[idx], &tmp, sizeof(dtr_synth_sfx_t));
        SDL_UnlockAudioStream(store->play.stream);
    } else {
        SDL_memcpy(&store->defs[idx], &tmp, sizeof(dtr_synth_sfx_t));
    }

    /* Update count */
    if (idx >= store->count) {
        store->count = idx + 1;
    }

    return JS_TRUE;
}

/* ------------------------------------------------------------------ */
/*  synth.get(idx) → {notes, speed, loopStart, loopEnd}                */
/* ------------------------------------------------------------------ */

static JSValue js_synth_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_synth_store_t *store;
    dtr_synth_sfx_t   *def;
    int32_t            idx;
    JSValue            result;
    JSValue            notes_arr;

    (void)this_val;
    store = prv_get_store();
    idx   = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    if (idx < 0 || idx >= DTR_SYNTH_MAX_SFX) {
        return JS_UNDEFINED;
    }

    def       = &store->defs[idx];
    result    = JS_NewObject(ctx);
    notes_arr = JS_NewArray(ctx);

    for (int32_t nti = 0; nti < DTR_SYNTH_NOTES; ++nti) {
        JSValue note_obj;

        note_obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, note_obj, "pitch", JS_NewInt32(ctx, def->notes[nti].pitch));
        JS_SetPropertyStr(ctx, note_obj, "waveform", JS_NewInt32(ctx, def->notes[nti].waveform));
        JS_SetPropertyStr(ctx, note_obj, "volume", JS_NewInt32(ctx, def->notes[nti].volume));
        JS_SetPropertyStr(ctx, note_obj, "effect", JS_NewInt32(ctx, def->notes[nti].effect));
        JS_SetPropertyUint32(ctx, notes_arr, (uint32_t)nti, note_obj);
    }

    JS_SetPropertyStr(ctx, result, "notes", notes_arr);
    JS_SetPropertyStr(ctx, result, "speed", JS_NewInt32(ctx, def->speed));
    JS_SetPropertyStr(ctx, result, "loopStart", JS_NewInt32(ctx, def->loop_start));
    JS_SetPropertyStr(ctx, result, "loopEnd", JS_NewInt32(ctx, def->loop_end));

    return result;
}

/* ------------------------------------------------------------------ */
/*  synth.play(idx, channel?)                                          */
/*                                                                     */
/*  Starts real-time synthesis playback of the given SFX definition.    */
/*  The audio callback reads directly from store->defs[idx], so edits  */
/*  are heard instantly with no buffer swap and no clicks.              */
/* ------------------------------------------------------------------ */

static JSValue js_synth_play(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_synth_store_t *store;
    dtr_audio_t       *aud;
    int32_t            idx;
    int32_t            channel;
    int32_t            sample_offset;

    (void)this_val;
    store         = prv_get_store();
    aud           = AUD(ctx);
    idx           = dtr_api_opt_int(ctx, argc, argv, 0, -1);
    channel       = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    sample_offset = dtr_api_opt_int(ctx, argc, argv, 2, 0);

    if (idx < 0 || idx >= DTR_SYNTH_MAX_SFX) {
        return JS_UNDEFINED;
    }
    if (aud == NULL || channel < 0 || channel >= aud->num_channels) {
        return JS_UNDEFINED;
    }

    prv_ensure_voice(&store->play, aud);

    /* If already playing this def and sample_offset > 0, it's a mid-play
     * edit — the callback is already reading from the live definition,
     * so nothing to do. */
    if (sample_offset > 0 && store->play.playing &&
        store->play.def == &store->defs[idx]) {
        return JS_UNDEFINED;
    }

    /* Fresh start */
    prv_voice_start(&store->play, &store->defs[idx]);

    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  synth.playNote(pitch, waveform, volume, effect, speed, channel?)    */
/*                                                                     */
/*  Renders and plays a single note for preview.                       */
/* ------------------------------------------------------------------ */

static JSValue
js_synth_play_note(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_synth_store_t *store;
    dtr_audio_t       *aud;
    dtr_synth_sfx_t    tmp;
    int32_t            channel;
    int16_t           *pcm;
    size_t             pcm_len;

    (void)this_val;
    if (argc < 4) {
        return JS_UNDEFINED;
    }

    store   = prv_get_store();
    aud     = AUD(ctx);
    channel = dtr_api_opt_int(ctx, argc, argv, 5, 0);

    if (aud == NULL || channel < 0 || channel >= aud->num_channels) {
        return JS_UNDEFINED;
    }

    /* Build a 1-note SFX */
    SDL_memset(&tmp, 0, sizeof(tmp));
    tmp.notes[0].pitch    = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 0, 0);
    tmp.notes[0].waveform = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 1, 0);
    tmp.notes[0].volume   = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 2, 5);
    tmp.notes[0].effect   = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 3, 0);
    tmp.speed             = (uint8_t)dtr_api_opt_int(ctx, argc, argv, 4, 8);
    tmp.loop_start        = 0;
    tmp.loop_end          = 0;

    pcm = dtr_synth_render(&tmp, &pcm_len);
    if (pcm == NULL) {
        return JS_UNDEFINED;
    }

    /* Truncate to 1 note and apply fade ramps to avoid clicks */
    {
        int32_t spn;
        int32_t ramp;
        int32_t rl;

        spn = (33 - (tmp.speed < 1 ? 1 : (tmp.speed > 32 ? 32 : tmp.speed))) *
              (DTR_SYNTH_SAMPLE_RATE / 120);
        if ((size_t)spn < pcm_len) {
            pcm_len = (size_t)spn;
        }

        ramp = 64;
        rl   = (int32_t)pcm_len;
        if (ramp > rl) {
            ramp = rl;
        }

        /* Fade-in */
        for (int32_t ri = 0; ri < ramp; ++ri) {
            float t;

            t       = (float)(ri + 1) / (float)ramp;
            pcm[ri] = (int16_t)((float)pcm[ri] * t);
        }

        /* Fade-out */
        for (int32_t ri = 0; ri < ramp; ++ri) {
            float t;

            t                   = (float)(ramp - ri) / (float)ramp;
            pcm[rl - ramp + ri] = (int16_t)((float)pcm[rl - ramp + ri] * t);
        }
    }

    /* Free previous note preview PCM safely under the stream lock */
    prv_ensure_note_voice(&store->note, aud);

    {
        int16_t *old_buf;

        SDL_LockAudioStream(store->note.stream);
        SDL_ClearAudioStream(store->note.stream); /* flush stale data */
        /* Capture previous amplitude for crossfade */
        store->note.xfade_base = store->note.last_out;
        store->note.xfade_rem  = store->note.playing ? NOTE_XFADE : 0;
        old_buf              = store->note.buf;
        store->note.buf      = pcm;
        store->note.buf_len  = pcm_len;
        store->note.pos      = 0;
        store->note.playing  = true;
        store->note.fade_out = false;
        store->note.fade_rem = 0;
        SDL_UnlockAudioStream(store->note.stream);

        /* Now safe to free — callback can no longer access old_buf */
        if (old_buf != NULL && old_buf != pcm) {
            DTR_FREE(old_buf);
        }
    }

    if (store->note.track != NULL && !MIX_TrackPlaying(store->note.track)) {
        SDL_PropertiesID props;

        props = SDL_CreateProperties();
        if (props != 0) {
            MIX_SetTrackLoops(store->note.track, -1);
            MIX_PlayTrack(store->note.track, props);
            SDL_DestroyProperties(props);
        }
    }

    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  synth.stop(channel?)                                               */
/* ------------------------------------------------------------------ */

static JSValue js_synth_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_synth_store_t *store;

    (void)this_val;
    (void)ctx;
    (void)argc;
    (void)argv;
    store = prv_get_store();

    prv_voice_stop(&store->play);
    prv_note_voice_stop(&store->note);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  synth.playing() → bool                                             */
/*                                                                     */
/*  Returns true if the pattern voice is currently playing.             */
/* ------------------------------------------------------------------ */

static JSValue js_synth_playing(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)ctx;
    (void)argc;
    (void)argv;
    return JS_NewBool(ctx, prv_get_store()->play.playing);
}

/* ------------------------------------------------------------------ */
/*  synth.count()                                                      */
/* ------------------------------------------------------------------ */

static JSValue js_synth_count(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, prv_get_store()->count);
}

/* ------------------------------------------------------------------ */
/*  synth.noteName(pitch) → "C-4"                                     */
/* ------------------------------------------------------------------ */

static JSValue
js_synth_note_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t pitch;

    (void)this_val;
    pitch = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    return JS_NewString(ctx, dtr_synth_note_name((uint8_t)pitch));
}

/* ------------------------------------------------------------------ */
/*  synth.waveNames() → ["sine","square","pulse",...]                  */
/* ------------------------------------------------------------------ */

static const char *prv_wave_names[] = {
    "sine",
    "square",
    "pulse",
    "triangle",
    "saw",
    "organ",
    "noise",
    "phaser",
};

static JSValue
js_synth_wave_names(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue arr;

    (void)this_val;
    (void)argc;
    (void)argv;

    arr = JS_NewArray(ctx);
    for (int32_t idx = 0; idx < DTR_WAVE_COUNT; ++idx) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, JS_NewString(ctx, prv_wave_names[idx]));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  synth.fxNames() → ["none","slide up","slide down",...]             */
/* ------------------------------------------------------------------ */

static const char *prv_fx_names[] = {
    "none",
    "slide up",
    "slide dn",
    "drop",
    "fade in",
    "fade out",
    "arp fast",
    "arp slow",
};

static JSValue
js_synth_fx_names(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue arr;

    (void)this_val;
    (void)argc;
    (void)argv;

    arr = JS_NewArray(ctx);
    for (int32_t idx = 0; idx < DTR_FX_COUNT; ++idx) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, JS_NewString(ctx, prv_fx_names[idx]));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  Registration                                                       */
/* ------------------------------------------------------------------ */

static const JSCFunctionListEntry js_synth_funcs[] = {
    JS_CFUNC_DEF("set", 5, js_synth_set),
    JS_CFUNC_DEF("get", 1, js_synth_get),
    JS_CFUNC_DEF("play", 3, js_synth_play),
    JS_CFUNC_DEF("playNote", 6, js_synth_play_note),
    JS_CFUNC_DEF("stop", 1, js_synth_stop),
    JS_CFUNC_DEF("playing", 0, js_synth_playing),
    JS_CFUNC_DEF("count", 0, js_synth_count),
    JS_CFUNC_DEF("noteName", 1, js_synth_note_name),
    JS_CFUNC_DEF("waveNames", 0, js_synth_wave_names),
    JS_CFUNC_DEF("fxNames", 0, js_synth_fx_names),
};

void dtr_synth_api_register(JSContext *ctx, JSValue global)
{
    JSValue nsp;

    nsp = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, nsp, js_synth_funcs, countof(js_synth_funcs));
    JS_SetPropertyStr(ctx, global, "synth", nsp);
}
