/**
 * \file            test_audio.c
 * \brief           Unit tests for the audio subsystem
 */



#include "audio.h"
#include "test_harness.h"

/* ------------------------------------------------------------------ */
/*  NULL safety                                                        */
/* ------------------------------------------------------------------ */

static void test_audio_destroy_null(void)
{
    dtr_audio_destroy(NULL); /* must not crash */
    DTR_PASS();
}

static void test_sfx_play_null(void)
{
    dtr_sfx_play(NULL, 0, 0, 0);
    DTR_PASS();
}

static void test_sfx_stop_null(void)
{
    dtr_sfx_stop(NULL, 0);
    dtr_sfx_stop(NULL, -1); /* stop-all path */
    DTR_PASS();
}

static void test_sfx_volume_null(void)
{
    dtr_sfx_volume(NULL, 0.5f, 0);
    DTR_PASS();
}

static void test_sfx_get_volume_null(void)
{
    DTR_ASSERT_NEAR(dtr_sfx_get_volume(NULL, 0), 0.0f, 0.001);
    DTR_PASS();
}

static void test_sfx_playing_null(void)
{
    DTR_ASSERT(!dtr_sfx_playing(NULL, 0));
    DTR_PASS();
}

static void test_mus_play_null(void)
{
    dtr_mus_play(NULL, 0, 0, 0);
    DTR_PASS();
}

static void test_mus_stop_null(void)
{
    dtr_mus_stop(NULL, 0);
    DTR_PASS();
}

static void test_mus_volume_null(void)
{
    dtr_mus_volume(NULL, 0.5f);
    DTR_PASS();
}

static void test_mus_get_volume_null(void)
{
    DTR_ASSERT_NEAR(dtr_mus_get_volume(NULL), 0.0f, 0.001);
    DTR_PASS();
}

static void test_mus_playing_null(void)
{
    DTR_ASSERT(!dtr_mus_playing(NULL));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Load with NULL / invalid data                                      */
/* ------------------------------------------------------------------ */

static void test_load_sfx_null_audio(void)
{
    uint8_t dummy = 0;

    DTR_ASSERT(!dtr_audio_load_sfx(NULL, &dummy, 1));
    DTR_PASS();
}

static void test_load_music_null_audio(void)
{
    uint8_t dummy = 0;

    DTR_ASSERT(!dtr_audio_load_music(NULL, &dummy, 1));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Channel bounds                                                     */
/* ------------------------------------------------------------------ */

static void test_sfx_playing_out_of_bounds(void)
{
    DTR_ASSERT(!dtr_sfx_playing(NULL, -1));
    DTR_ASSERT(!dtr_sfx_playing(NULL, CONSOLE_MAX_CHANNELS));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Live audio tests (skipped if device unavailable)                   */
/* ------------------------------------------------------------------ */

static dtr_audio_t *prv_try_create(void)
{
    return dtr_audio_create(4, CONSOLE_AUDIO_FREQ, CONSOLE_AUDIO_BUFFER);
}

static void test_audio_create_destroy(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    DTR_ASSERT(aud->initialized);
    DTR_ASSERT_EQ_INT(aud->num_channels, 4);
    DTR_ASSERT_EQ_INT(aud->frequency, CONSOLE_AUDIO_FREQ);
    DTR_ASSERT(aud->mixer != NULL);
    DTR_ASSERT(aud->music_track != NULL);

    dtr_audio_destroy(aud);
    DTR_PASS();
}

static void test_audio_channel_volume(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* Default volumes should be 1.0 */
    DTR_ASSERT_NEAR(dtr_sfx_get_volume(aud, 0), 1.0f, 0.001);

    dtr_sfx_volume(aud, 0.5f, 0);
    DTR_ASSERT_NEAR(dtr_sfx_get_volume(aud, 0), 0.5f, 0.001);

    /* Out-of-range channel returns 0 */
    DTR_ASSERT_NEAR(dtr_sfx_get_volume(aud, -1), 0.0f, 0.001);
    DTR_ASSERT_NEAR(dtr_sfx_get_volume(aud, CONSOLE_MAX_CHANNELS), 0.0f, 0.001);

    dtr_audio_destroy(aud);
    DTR_PASS();
}

static void test_audio_music_volume(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    DTR_ASSERT_NEAR(dtr_mus_get_volume(aud), 1.0f, 0.001);

    dtr_mus_volume(aud, 0.3f);
    DTR_ASSERT_NEAR(dtr_mus_get_volume(aud), 0.3f, 0.001);

    dtr_audio_destroy(aud);
    DTR_PASS();
}

static void test_audio_sfx_play_invalid_idx(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* No SFX loaded — idx 0 is out-of-range, should not crash */
    dtr_sfx_play(aud, 0, 0, 0);
    dtr_sfx_play(aud, -1, 0, 0);
    dtr_sfx_play(aud, 999, 0, 0);

    dtr_audio_destroy(aud);
    DTR_PASS();
}

static void test_audio_sfx_play_invalid_channel(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* Channel out-of-range — should silently return */
    dtr_sfx_play(aud, 0, -1, 0);
    dtr_sfx_play(aud, 0, 999, 0);

    dtr_audio_destroy(aud);
    DTR_PASS();
}

static void test_audio_music_not_playing(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    DTR_ASSERT(!dtr_mus_playing(aud));
    dtr_mus_stop(aud, 0);   /* stop when nothing playing — safe */
    dtr_mus_stop(aud, 100); /* fade stop when nothing playing — safe */

    dtr_audio_destroy(aud);
    DTR_PASS();
}

static void test_audio_sfx_stop_all(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* Stop-all with channel = -1 */
    dtr_sfx_stop(aud, -1);

    dtr_audio_destroy(aud);
    DTR_PASS();
}

static void test_audio_master_volume(void)
{
    dtr_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* Default master volume should be 1.0 */
    DTR_ASSERT_NEAR(dtr_audio_get_master_volume(aud), 1.0f, 0.001);

    /* Set to 0.5 */
    dtr_audio_set_master_volume(aud, 0.5f);
    DTR_ASSERT_NEAR(dtr_audio_get_master_volume(aud), 0.5f, 0.001);

    /* Clamp below 0 */
    dtr_audio_set_master_volume(aud, -0.5f);
    DTR_ASSERT_NEAR(dtr_audio_get_master_volume(aud), 0.0f, 0.001);

    /* Clamp above 1 */
    dtr_audio_set_master_volume(aud, 2.0f);
    DTR_ASSERT_NEAR(dtr_audio_get_master_volume(aud), 1.0f, 0.001);

    dtr_audio_destroy(aud);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Runner                                                             */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("test_audio");

    /* NULL safety (always run) */
    DTR_RUN_TEST(test_audio_destroy_null);
    DTR_RUN_TEST(test_sfx_play_null);
    DTR_RUN_TEST(test_sfx_stop_null);
    DTR_RUN_TEST(test_sfx_volume_null);
    DTR_RUN_TEST(test_sfx_get_volume_null);
    DTR_RUN_TEST(test_sfx_playing_null);
    DTR_RUN_TEST(test_mus_play_null);
    DTR_RUN_TEST(test_mus_stop_null);
    DTR_RUN_TEST(test_mus_volume_null);
    DTR_RUN_TEST(test_mus_get_volume_null);
    DTR_RUN_TEST(test_mus_playing_null);
    DTR_RUN_TEST(test_load_sfx_null_audio);
    DTR_RUN_TEST(test_load_music_null_audio);
    DTR_RUN_TEST(test_sfx_playing_out_of_bounds);

    /* Live tests (skip if no audio device) */
    DTR_RUN_TEST(test_audio_create_destroy);
    DTR_RUN_TEST(test_audio_channel_volume);
    DTR_RUN_TEST(test_audio_music_volume);
    DTR_RUN_TEST(test_audio_sfx_play_invalid_idx);
    DTR_RUN_TEST(test_audio_sfx_play_invalid_channel);
    DTR_RUN_TEST(test_audio_music_not_playing);
    DTR_RUN_TEST(test_audio_sfx_stop_all);
    DTR_RUN_TEST(test_audio_master_volume);

    DTR_TEST_END();
}
