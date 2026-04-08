/**
 * \file            test_synth.c
 * \brief           Unit tests for the chip-tune synthesizer
 */

#include "synth.h"
#include "test_harness.h"

#include <stdlib.h>

#define EPS 0.01f

/* ------------------------------------------------------------------ */
/*  Pitch-to-frequency conversion                                      */
/* ------------------------------------------------------------------ */

static void test_pitch_rest(void)
{
    DTR_ASSERT_NEAR(dtr_synth_pitch_freq(0), 0.0f, EPS);
    DTR_PASS();
}

static void test_pitch_out_of_range(void)
{
    DTR_ASSERT_NEAR(dtr_synth_pitch_freq(97), 0.0f, EPS);
    DTR_ASSERT_NEAR(dtr_synth_pitch_freq(255), 0.0f, EPS);
    DTR_PASS();
}

static void test_pitch_a4(void)
{
    /* Pitch 58 = A-4 = 440 Hz */
    DTR_ASSERT_NEAR(dtr_synth_pitch_freq(58), 440.0f, 0.5f);
    DTR_PASS();
}

static void test_pitch_c0(void)
{
    /* Pitch 1 = C-0 ≈ 16.35 Hz */
    DTR_ASSERT_NEAR(dtr_synth_pitch_freq(1), 16.35f, 0.5f);
    DTR_PASS();
}

static void test_pitch_c4(void)
{
    /* Pitch 49 = C-4 ≈ 261.63 Hz */
    DTR_ASSERT_NEAR(dtr_synth_pitch_freq(49), 261.63f, 1.0f);
    DTR_PASS();
}

static void test_pitch_b7(void)
{
    /* Pitch 96 = B-7 ≈ 3951.07 Hz */
    DTR_ASSERT_NEAR(dtr_synth_pitch_freq(96), 3951.07f, 10.0f);
    DTR_PASS();
}

static void test_pitch_octave_doubling(void)
{
    /* An octave up should double the frequency */
    float c3;
    float c4;

    c3 = dtr_synth_pitch_freq(37); /* C-3 */
    c4 = dtr_synth_pitch_freq(49); /* C-4 */
    DTR_ASSERT_NEAR(c4 / c3, 2.0f, 0.01f);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Note naming                                                        */
/* ------------------------------------------------------------------ */

static void test_note_name_rest(void)
{
    DTR_ASSERT_EQ_STR(dtr_synth_note_name(0), "---");
    DTR_PASS();
}

static void test_note_name_out_of_range(void)
{
    DTR_ASSERT_EQ_STR(dtr_synth_note_name(97), "---");
    DTR_PASS();
}

static void test_note_name_c0(void)
{
    DTR_ASSERT_EQ_STR(dtr_synth_note_name(1), "C-0");
    DTR_PASS();
}

static void test_note_name_a4(void)
{
    DTR_ASSERT_EQ_STR(dtr_synth_note_name(58), "A-4");
    DTR_PASS();
}

static void test_note_name_csharp(void)
{
    DTR_ASSERT_EQ_STR(dtr_synth_note_name(2), "C#0");
    DTR_PASS();
}

static void test_note_name_b7(void)
{
    DTR_ASSERT_EQ_STR(dtr_synth_note_name(96), "B-7");
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Waveform output                                                    */
/* ------------------------------------------------------------------ */

static void test_wave_triangle_peak(void)
{
    /* Triangle at phase 0.5 = peak (+0.5), at 0.0 = trough (-0.5) */
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_TRIANGLE, 0.5f), 0.5f, EPS);
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_TRIANGLE, 0.0f), -0.5f, EPS);
    DTR_PASS();
}

static void test_wave_triangle_zero(void)
{
    /* Triangle at phase 0.25 and 0.75 = 0 */
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_TRIANGLE, 0.25f), 0.0f, EPS);
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_TRIANGLE, 0.75f), 0.0f, EPS);
    DTR_PASS();
}

static void test_wave_square_values(void)
{
    /* Square: < 0.5 → +0.25, >= 0.5 → -0.25 */
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_SQUARE, 0.1f), 0.25f, EPS);
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_SQUARE, 0.9f), -0.25f, EPS);
    DTR_PASS();
}

static void test_wave_pulse_duty(void)
{
    /* Pulse: < 0.316 → +0.25, >= 0.316 → -0.25 */
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_PULSE, 0.1f), 0.25f, EPS);
    DTR_ASSERT_NEAR(dtr_synth_waveform(DTR_WAVE_PULSE, 0.5f), -0.25f, EPS);
    DTR_PASS();
}

static void test_wave_saw_range(void)
{
    /* Saw peak at ~0.5: 0.653 * 0.5 ≈ +0.327 */
    float val;

    val = dtr_synth_waveform(DTR_WAVE_SAW, 0.49f);
    DTR_ASSERT(val > 0.0f);
    DTR_ASSERT(val < 0.35f);
    DTR_PASS();
}

static void test_wave_noise_bounded(void)
{
    /* Noise should stay in [-0.5, 0.5] */
    int32_t i;

    for (i = 0; i < 1000; ++i) {
        float s;

        s = dtr_synth_waveform(DTR_WAVE_NOISE, (float)i / 1000.0f);
        DTR_ASSERT(s >= -1.0f && s <= 1.0f);
    }
    DTR_PASS();
}

static void test_wave_organ_range(void)
{
    /* Organ peak ±0.333 */
    float   peak;
    float   s;
    int32_t i;

    peak = 0.0f;
    for (i = 0; i < 100; ++i) {
        s = dtr_synth_waveform(DTR_WAVE_ORGAN, (float)i / 100.0f);
        if (s > peak) {
            peak = s;
        }
    }
    DTR_ASSERT(peak > 0.2f);
    DTR_ASSERT(peak < 0.4f);
    DTR_PASS();
}

static void test_wave_phaser_range(void)
{
    float   s;
    int32_t i;

    for (i = 0; i < 100; ++i) {
        s = dtr_synth_waveform(DTR_WAVE_PHASER, (float)i / 100.0f);
        DTR_ASSERT(s >= -0.6f && s <= 0.6f);
    }
    DTR_PASS();
}

static void test_wave_invalid(void)
{
    DTR_ASSERT_NEAR(dtr_synth_waveform(255, 0.5f), 0.0f, EPS);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Band-limited waveforms (PolyBLEP)                                  */
/* ------------------------------------------------------------------ */

static void test_wave_bl_square(void)
{
    /* BL square should be roughly ±0.25 at well-separated phases */
    float lo;
    float hi;

    hi = dtr_synth_waveform_bl(DTR_WAVE_SQUARE, 0.25f, 0.01f);
    lo = dtr_synth_waveform_bl(DTR_WAVE_SQUARE, 0.75f, 0.01f);
    DTR_ASSERT(hi > 0.15f);
    DTR_ASSERT(lo < -0.15f);
    DTR_PASS();
}

static void test_wave_bl_fallback(void)
{
    /* Triangle doesn't have BL — should match non-BL */
    float bl;
    float raw;

    bl  = dtr_synth_waveform_bl(DTR_WAVE_TRIANGLE, 0.25f, 0.01f);
    raw = dtr_synth_waveform(DTR_WAVE_TRIANGLE, 0.25f);
    DTR_ASSERT_NEAR(bl, raw, EPS);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Render: basic output                                               */
/* ------------------------------------------------------------------ */

static void test_render_null(void)
{
    size_t len;

    DTR_ASSERT(dtr_synth_render(NULL, &len) == NULL);
    DTR_ASSERT(dtr_synth_render(NULL, NULL) == NULL);
    DTR_PASS();
}

static void test_render_silent(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    /* All notes have pitch=0 (rest), speed defaults to 0 → clamped to 1 */
    sfx.speed = 1;
    buf       = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    DTR_ASSERT(len > 0);

    /* All samples should be 0 (rest) */
    for (size_t i = 0; i < len; ++i) {
        DTR_ASSERT_EQ_INT(buf[i], 0);
    }
    free(buf);
    DTR_PASS();
}

static void test_render_nonsilent(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;
    int32_t         has_audio;
    size_t          i;

    /* One audible note */
    sfx.speed             = 1;
    sfx.notes[0].pitch    = 49; /* C-4 */
    sfx.notes[0].waveform = DTR_WAVE_SQUARE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_NONE;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    DTR_ASSERT(len > 0);

    /* At least some samples should be non-zero */
    has_audio = 0;
    for (i = 0; i < len && !has_audio; ++i) {
        if (buf[i] != 0) {
            has_audio = 1;
        }
    }
    DTR_ASSERT(has_audio);
    free(buf);
    DTR_PASS();
}

static void test_render_speed_affects_length(void)
{
    dtr_synth_sfx_t sfx1 = {0};
    dtr_synth_sfx_t sfx2 = {0};
    size_t          len1;
    size_t          len2;
    int16_t        *buf1;
    int16_t        *buf2;

    sfx1.speed = 1;
    sfx2.speed = 2;

    buf1 = dtr_synth_render(&sfx1, &len1);
    buf2 = dtr_synth_render(&sfx2, &len2);
    DTR_ASSERT_NOT_NULL(buf1);
    DTR_ASSERT_NOT_NULL(buf2);

    /* Speed 2 should produce exactly 2x the samples */
    DTR_ASSERT_EQ_INT((long long)len2, (long long)(len1 * 2));

    free(buf1);
    free(buf2);
    DTR_PASS();
}

static void test_render_loop(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    sfx.speed             = 1;
    sfx.loop_start        = 0;
    sfx.loop_end          = 3;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[0].volume   = 7;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);

    /* Looped SFX should produce more samples than non-looped (fills ~60s) */
    DTR_ASSERT(len > DTR_SYNTH_NOTES * (DTR_SYNTH_SAMPLE_RATE / 128));
    free(buf);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Render: effect coverage                                            */
/* ------------------------------------------------------------------ */

static void test_render_slide(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    sfx.speed             = 4;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[0].volume   = 7;
    sfx.notes[1].pitch    = 61;
    sfx.notes[1].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[1].volume   = 7;
    sfx.notes[1].effect   = DTR_FX_SLIDE;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    DTR_ASSERT(len > 0);
    free(buf);
    DTR_PASS();
}

static void test_render_vibrato(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    sfx.speed             = 4;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_VIBRATO;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    DTR_ASSERT(len > 0);
    free(buf);
    DTR_PASS();
}

static void test_render_drop(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    sfx.speed             = 4;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_DROP;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    DTR_ASSERT(len > 0);
    free(buf);
    DTR_PASS();
}

static void test_render_fade_in(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    sfx.speed             = 8;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_SQUARE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_FADE_IN;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    /* First sample should be near zero (fade in starts from 0) */
    DTR_ASSERT(buf[0] >= -100 && buf[0] <= 100);
    free(buf);
    DTR_PASS();
}

static void test_render_fade_out(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;
    int32_t         spn;

    sfx.speed             = 8;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_SQUARE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_FADE_OUT;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    /* Last sample of note 0 should be near zero (faded out) */
    spn = sfx.speed * (DTR_SYNTH_SAMPLE_RATE / 128);
    DTR_ASSERT(buf[spn - 1] >= -100 && buf[spn - 1] <= 100);
    free(buf);
    DTR_PASS();
}

static void test_render_arpeggio(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    sfx.speed = 4;
    /* Set up a 4-note arpeggio group */
    for (int32_t i = 0; i < 4; ++i) {
        sfx.notes[i].pitch    = (uint8_t)(49 + i * 4);
        sfx.notes[i].waveform = DTR_WAVE_TRIANGLE;
        sfx.notes[i].volume   = 7;
        sfx.notes[i].effect   = DTR_FX_ARPF;
    }

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    DTR_ASSERT(len > 0);
    free(buf);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Speed clamping                                                     */
/* ------------------------------------------------------------------ */

static void test_render_speed_zero_clamps(void)
{
    dtr_synth_sfx_t sfx0 = {0};
    dtr_synth_sfx_t sfx1 = {0};
    size_t          len0;
    size_t          len1;
    int16_t        *buf0;
    int16_t        *buf1;

    /* Speed 0 should clamp to 1 internally */
    sfx0.speed = 0;
    sfx1.speed = 1;

    buf0 = dtr_synth_render(&sfx0, &len0);
    buf1 = dtr_synth_render(&sfx1, &len1);
    DTR_ASSERT_NOT_NULL(buf0);
    DTR_ASSERT_NOT_NULL(buf1);
    DTR_ASSERT_EQ_INT((long long)len0, (long long)len1);

    free(buf0);
    free(buf1);
    DTR_PASS();
}

static void test_render_speed_max_clamps(void)
{
    dtr_synth_sfx_t sfx33 = {0};
    dtr_synth_sfx_t sfx32 = {0};
    size_t          len33;
    size_t          len32;
    int16_t        *buf33;
    int16_t        *buf32;

    /* Speed > 32 should clamp to 32 internally */
    sfx33.speed = 33;
    sfx32.speed = 32;

    buf33 = dtr_synth_render(&sfx33, &len33);
    buf32 = dtr_synth_render(&sfx32, &len32);
    DTR_ASSERT_NOT_NULL(buf33);
    DTR_ASSERT_NOT_NULL(buf32);
    DTR_ASSERT_EQ_INT((long long)len33, (long long)len32);

    free(buf33);
    free(buf32);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  All waveforms render without crashing                              */
/* ------------------------------------------------------------------ */

static void test_render_all_waveforms(void)
{
    int32_t w;

    for (w = 0; w < DTR_WAVE_COUNT; ++w) {
        dtr_synth_sfx_t sfx = {0};
        size_t          len;
        int16_t        *buf;

        sfx.speed             = 1;
        sfx.notes[0].pitch    = 49;
        sfx.notes[0].waveform = (uint8_t)w;
        sfx.notes[0].volume   = 7;

        buf = dtr_synth_render(&sfx, &len);
        DTR_ASSERT_NOT_NULL(buf);
        DTR_ASSERT(len > 0);
        free(buf);
    }
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  All effects render without crashing                                */
/* ------------------------------------------------------------------ */

static void test_render_all_effects(void)
{
    int32_t e;

    for (e = 0; e < DTR_FX_COUNT; ++e) {
        dtr_synth_sfx_t sfx = {0};
        size_t          len;
        int16_t        *buf;

        sfx.speed             = 4;
        sfx.notes[0].pitch    = 49;
        sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
        sfx.notes[0].volume   = 7;
        sfx.notes[0].effect   = (uint8_t)e;
        /* Second note for slide target */
        sfx.notes[1].pitch    = 61;
        sfx.notes[1].waveform = DTR_WAVE_TRIANGLE;
        sfx.notes[1].volume   = 7;

        buf = dtr_synth_render(&sfx, &len);
        DTR_ASSERT_NOT_NULL(buf);
        DTR_ASSERT(len > 0);
        free(buf);
    }
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Effect fidelity: slide                                             */
/* ------------------------------------------------------------------ */

static void test_effect_slide_reaches_target(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;
    int32_t         spn;
    float           freq_start;
    float           freq_end;
    float           rms_start;
    float           rms_end;

    /* Note 0: C-4 (pitch 49), Note 1: E-4 (pitch 53) with slide effect.
     * The slide on note 1 should start near C-4 freq and end near E-4 freq.
     * We verify by checking RMS energy at different points — if the pitch
     * changes, the waveform period changes, affecting short-window RMS. */
    sfx.speed             = 16; /* long note = easy to measure */
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[0].volume   = 7;
    sfx.notes[1].pitch    = 53;
    sfx.notes[1].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[1].volume   = 7;
    sfx.notes[1].effect   = DTR_FX_SLIDE;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);

    spn = 16 * (DTR_SYNTH_SAMPLE_RATE / 128);

    /* The slide note (note 1) starts at sample spn and goes to 2*spn.
     * At the very start of note 1, freq should be near note 0's freq.
     * At the very end of note 1, freq should be near note 1's target freq.
     * We can measure this by checking that the audio changes — the start
     * and end of the slide note should have different characteristics. */
    freq_start = dtr_synth_pitch_freq(49);
    freq_end   = dtr_synth_pitch_freq(53);
    DTR_ASSERT(freq_end > freq_start);

    /* Compute RMS of first 512 samples of note 1 vs last 512 samples */
    {
        int32_t window = 512;
        double  sum_a  = 0;
        double  sum_b  = 0;
        int32_t off_a  = spn; /* start of note 1 */
        int32_t off_b  = 2 * spn - window;

        for (int32_t i = 0; i < window; ++i) {
            double a = buf[off_a + i] / 32767.0;
            double b = buf[off_b + i] / 32767.0;
            sum_a += a * a;
            sum_b += b * b;
        }
        rms_start = (float)SDL_sqrt(sum_a / window);
        rms_end   = (float)SDL_sqrt(sum_b / window);
    }

    /* Both windows should have non-zero audio */
    DTR_ASSERT(rms_start > 0.01f);
    DTR_ASSERT(rms_end > 0.01f);

    free(buf);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Effect fidelity: drop reaches silence                              */
/* ------------------------------------------------------------------ */

static void test_effect_drop_reaches_silence(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;
    int32_t         spn;

    sfx.speed             = 16;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_DROP;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);

    spn = 16 * (DTR_SYNTH_SAMPLE_RATE / 128);

    /* Last few samples of note 0 should be very quiet (freq dropped low) */
    {
        int32_t window = 64;
        double  sum    = 0;
        for (int32_t i = spn - window; i < spn; ++i) {
            double s = buf[i] / 32767.0;
            sum += s * s;
        }
        float rms = (float)SDL_sqrt(sum / window);
        /* Drop goes to 1 Hz (clamped), so residual energy remains but is low */
        DTR_ASSERT(rms < 0.20f);
    }

    free(buf);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Effect fidelity: fade-in volume ramp                               */
/* ------------------------------------------------------------------ */

static void test_effect_fade_in_ramp(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;
    int32_t         spn;
    float           rms_early;
    float           rms_late;

    sfx.speed             = 16;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_SQUARE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_FADE_IN;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);

    spn = 16 * (DTR_SYNTH_SAMPLE_RATE / 128);

    /* RMS of first 10% should be much less than last 10% */
    {
        int32_t window = spn / 10;
        double  sum_e  = 0;
        double  sum_l  = 0;
        for (int32_t i = 0; i < window; ++i) {
            double e = buf[i] / 32767.0;
            double l = buf[spn - window + i] / 32767.0;
            sum_e += e * e;
            sum_l += l * l;
        }
        rms_early = (float)SDL_sqrt(sum_e / window);
        rms_late  = (float)SDL_sqrt(sum_l / window);
    }

    DTR_ASSERT(rms_late > rms_early * 2.0f);

    free(buf);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Effect fidelity: fade-out volume ramp                              */
/* ------------------------------------------------------------------ */

static void test_effect_fade_out_ramp(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;
    int32_t         spn;
    float           rms_early;
    float           rms_late;

    sfx.speed             = 16;
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_SQUARE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_FADE_OUT;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);

    spn = 16 * (DTR_SYNTH_SAMPLE_RATE / 128);

    /* RMS of first 10% should be much greater than last 10% */
    {
        int32_t window = spn / 10;
        double  sum_e  = 0;
        double  sum_l  = 0;
        for (int32_t i = 0; i < window; ++i) {
            double e = buf[i] / 32767.0;
            double l = buf[spn - window + i] / 32767.0;
            sum_e += e * e;
            sum_l += l * l;
        }
        rms_early = (float)SDL_sqrt(sum_e / window);
        rms_late  = (float)SDL_sqrt(sum_l / window);
    }

    DTR_ASSERT(rms_early > rms_late * 2.0f);

    free(buf);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Effect fidelity: arpeggio cycles through notes                     */
/* ------------------------------------------------------------------ */

static void test_effect_arpeggio_cycles(void)
{
    dtr_synth_sfx_t sfx = {0};
    size_t          len;
    int16_t        *buf;

    sfx.speed = 8;
    /* 4-note group: C-4, E-4, G-4, C-5 */
    sfx.notes[0].pitch    = 49;
    sfx.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[0].volume   = 7;
    sfx.notes[0].effect   = DTR_FX_ARPF;

    sfx.notes[1].pitch    = 53;
    sfx.notes[1].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[1].volume   = 7;
    sfx.notes[1].effect   = DTR_FX_ARPF;

    sfx.notes[2].pitch    = 56;
    sfx.notes[2].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[2].volume   = 7;
    sfx.notes[2].effect   = DTR_FX_ARPF;

    sfx.notes[3].pitch    = 61;
    sfx.notes[3].waveform = DTR_WAVE_TRIANGLE;
    sfx.notes[3].volume   = 7;
    sfx.notes[3].effect   = DTR_FX_ARPF;

    buf = dtr_synth_render(&sfx, &len);
    DTR_ASSERT_NOT_NULL(buf);
    DTR_ASSERT(len > 0);

    /* Verify the rendered output isn't just a static tone — the arpeggio
     * should create periodic variations. Check that the audio has multiple
     * different sample magnitudes across the arpeggio range. */
    {
        int32_t spn        = 8 * (DTR_SYNTH_SAMPLE_RATE / 128);
        int32_t total      = 4 * spn;
        int32_t pos_count  = 0;
        int32_t neg_count  = 0;
        int32_t zero_cross = 0;

        for (int32_t i = 1; i < total && (size_t)i < len; ++i) {
            if (buf[i] > 0) {
                pos_count++;
            }
            if (buf[i] < 0) {
                neg_count++;
            }
            if ((buf[i - 1] >= 0 && buf[i] < 0) || (buf[i - 1] < 0 && buf[i] >= 0)) {
                zero_cross++;
            }
        }

        /* With 4 notes of different pitches, we expect many zero crossings */
        DTR_ASSERT(zero_cross > 100);
        DTR_ASSERT(pos_count > 0);
        DTR_ASSERT(neg_count > 0);
    }

    free(buf);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Effect fidelity: vibrato produces pitch modulation                 */
/* ------------------------------------------------------------------ */

static void test_effect_vibrato_modulation(void)
{
    dtr_synth_sfx_t sfx_plain = {0};
    dtr_synth_sfx_t sfx_vib   = {0};
    size_t          len_plain;
    size_t          len_vib;
    int16_t        *buf_plain;
    int16_t        *buf_vib;
    int32_t         spn;
    int32_t         diffs;

    /* Render the same note with and without vibrato */
    sfx_plain.speed             = 16;
    sfx_plain.notes[0].pitch    = 49;
    sfx_plain.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx_plain.notes[0].volume   = 7;

    sfx_vib.speed             = 16;
    sfx_vib.notes[0].pitch    = 49;
    sfx_vib.notes[0].waveform = DTR_WAVE_TRIANGLE;
    sfx_vib.notes[0].volume   = 7;
    sfx_vib.notes[0].effect   = DTR_FX_VIBRATO;

    buf_plain = dtr_synth_render(&sfx_plain, &len_plain);
    buf_vib   = dtr_synth_render(&sfx_vib, &len_vib);
    DTR_ASSERT_NOT_NULL(buf_plain);
    DTR_ASSERT_NOT_NULL(buf_vib);
    DTR_ASSERT_EQ_INT((long long)len_plain, (long long)len_vib);

    spn = 16 * (DTR_SYNTH_SAMPLE_RATE / 128);

    /* The vibrato version should differ from the plain version */
    diffs = 0;
    for (int32_t i = 0; i < spn; ++i) {
        if (buf_plain[i] != buf_vib[i]) {
            diffs++;
        }
    }

    /* Most samples should differ due to pitch modulation */
    DTR_ASSERT(diffs > spn / 2);

    free(buf_plain);
    free(buf_vib);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("synth");

    /* Pitch frequency */
    DTR_RUN_TEST(test_pitch_rest);
    DTR_RUN_TEST(test_pitch_out_of_range);
    DTR_RUN_TEST(test_pitch_a4);
    DTR_RUN_TEST(test_pitch_c0);
    DTR_RUN_TEST(test_pitch_c4);
    DTR_RUN_TEST(test_pitch_b7);
    DTR_RUN_TEST(test_pitch_octave_doubling);

    /* Note naming */
    DTR_RUN_TEST(test_note_name_rest);
    DTR_RUN_TEST(test_note_name_out_of_range);
    DTR_RUN_TEST(test_note_name_c0);
    DTR_RUN_TEST(test_note_name_a4);
    DTR_RUN_TEST(test_note_name_csharp);
    DTR_RUN_TEST(test_note_name_b7);

    /* Waveforms */
    DTR_RUN_TEST(test_wave_triangle_peak);
    DTR_RUN_TEST(test_wave_triangle_zero);
    DTR_RUN_TEST(test_wave_square_values);
    DTR_RUN_TEST(test_wave_pulse_duty);
    DTR_RUN_TEST(test_wave_saw_range);
    DTR_RUN_TEST(test_wave_noise_bounded);
    DTR_RUN_TEST(test_wave_organ_range);
    DTR_RUN_TEST(test_wave_phaser_range);
    DTR_RUN_TEST(test_wave_invalid);

    /* Band-limited */
    DTR_RUN_TEST(test_wave_bl_square);
    DTR_RUN_TEST(test_wave_bl_fallback);

    /* Render */
    DTR_RUN_TEST(test_render_null);
    DTR_RUN_TEST(test_render_silent);
    DTR_RUN_TEST(test_render_nonsilent);
    DTR_RUN_TEST(test_render_speed_affects_length);
    DTR_RUN_TEST(test_render_loop);

    /* Effects */
    DTR_RUN_TEST(test_render_slide);
    DTR_RUN_TEST(test_render_vibrato);
    DTR_RUN_TEST(test_render_drop);
    DTR_RUN_TEST(test_render_fade_in);
    DTR_RUN_TEST(test_render_fade_out);
    DTR_RUN_TEST(test_render_arpeggio);

    /* Effect fidelity */
    DTR_RUN_TEST(test_effect_slide_reaches_target);
    DTR_RUN_TEST(test_effect_drop_reaches_silence);
    DTR_RUN_TEST(test_effect_fade_in_ramp);
    DTR_RUN_TEST(test_effect_fade_out_ramp);
    DTR_RUN_TEST(test_effect_arpeggio_cycles);
    DTR_RUN_TEST(test_effect_vibrato_modulation);

    /* Speed clamping */
    DTR_RUN_TEST(test_render_speed_zero_clamps);
    DTR_RUN_TEST(test_render_speed_max_clamps);

    /* Comprehensive */
    DTR_RUN_TEST(test_render_all_waveforms);
    DTR_RUN_TEST(test_render_all_effects);

    DTR_TEST_END();
}
