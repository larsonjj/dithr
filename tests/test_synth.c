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

    /* Speed clamping */
    DTR_RUN_TEST(test_render_speed_zero_clamps);
    DTR_RUN_TEST(test_render_speed_max_clamps);

    /* Comprehensive */
    DTR_RUN_TEST(test_render_all_waveforms);
    DTR_RUN_TEST(test_render_all_effects);

    DTR_TEST_END();
}
