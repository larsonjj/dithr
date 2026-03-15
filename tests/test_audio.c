/**
 * \file            test_audio.c
 * \brief           Unit tests for the audio subsystem
 */

#include <SDL3/SDL.h>

#include "audio.h"
#include "test_harness.h"

/* ------------------------------------------------------------------ */
/*  NULL safety                                                        */
/* ------------------------------------------------------------------ */

static void test_audio_destroy_null(void)
{
    mvn_audio_destroy(NULL); /* must not crash */
    MVN_PASS();
}

static void test_sfx_play_null(void)
{
    mvn_sfx_play(NULL, 0, 0, 0);
    MVN_PASS();
}

static void test_sfx_stop_null(void)
{
    mvn_sfx_stop(NULL, 0);
    mvn_sfx_stop(NULL, -1); /* stop-all path */
    MVN_PASS();
}

static void test_sfx_volume_null(void)
{
    mvn_sfx_volume(NULL, 0.5f, 0);
    MVN_PASS();
}

static void test_sfx_get_volume_null(void)
{
    MVN_ASSERT_NEAR(mvn_sfx_get_volume(NULL, 0), 0.0f, 0.001);
    MVN_PASS();
}

static void test_sfx_playing_null(void)
{
    MVN_ASSERT(!mvn_sfx_playing(NULL, 0));
    MVN_PASS();
}

static void test_mus_play_null(void)
{
    mvn_mus_play(NULL, 0, 0, 0);
    MVN_PASS();
}

static void test_mus_stop_null(void)
{
    mvn_mus_stop(NULL, 0);
    MVN_PASS();
}

static void test_mus_volume_null(void)
{
    mvn_mus_volume(NULL, 0.5f);
    MVN_PASS();
}

static void test_mus_get_volume_null(void)
{
    MVN_ASSERT_NEAR(mvn_mus_get_volume(NULL), 0.0f, 0.001);
    MVN_PASS();
}

static void test_mus_playing_null(void)
{
    MVN_ASSERT(!mvn_mus_playing(NULL));
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Load with NULL / invalid data                                      */
/* ------------------------------------------------------------------ */

static void test_load_sfx_null_audio(void)
{
    uint8_t dummy = 0;

    MVN_ASSERT(!mvn_audio_load_sfx(NULL, &dummy, 1));
    MVN_PASS();
}

static void test_load_music_null_audio(void)
{
    uint8_t dummy = 0;

    MVN_ASSERT(!mvn_audio_load_music(NULL, &dummy, 1));
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Channel bounds                                                     */
/* ------------------------------------------------------------------ */

static void test_sfx_playing_out_of_bounds(void)
{
    MVN_ASSERT(!mvn_sfx_playing(NULL, -1));
    MVN_ASSERT(!mvn_sfx_playing(NULL, CONSOLE_MAX_CHANNELS));
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Live audio tests (skipped if device unavailable)                   */
/* ------------------------------------------------------------------ */

static mvn_audio_t *prv_try_create(void)
{
    return mvn_audio_create(4, CONSOLE_AUDIO_FREQ, CONSOLE_AUDIO_BUFFER);
}

static void test_audio_create_destroy(void)
{
    mvn_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    MVN_ASSERT(aud->initialized);
    MVN_ASSERT_EQ_INT(aud->num_channels, 4);
    MVN_ASSERT_EQ_INT(aud->frequency, CONSOLE_AUDIO_FREQ);
    MVN_ASSERT(aud->mixer != NULL);
    MVN_ASSERT(aud->music_track != NULL);

    mvn_audio_destroy(aud);
    MVN_PASS();
}

static void test_audio_channel_volume(void)
{
    mvn_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* Default volumes should be 1.0 */
    MVN_ASSERT_NEAR(mvn_sfx_get_volume(aud, 0), 1.0f, 0.001);

    mvn_sfx_volume(aud, 0.5f, 0);
    MVN_ASSERT_NEAR(mvn_sfx_get_volume(aud, 0), 0.5f, 0.001);

    /* Out-of-range channel returns 0 */
    MVN_ASSERT_NEAR(mvn_sfx_get_volume(aud, -1), 0.0f, 0.001);
    MVN_ASSERT_NEAR(mvn_sfx_get_volume(aud, CONSOLE_MAX_CHANNELS), 0.0f, 0.001);

    mvn_audio_destroy(aud);
    MVN_PASS();
}

static void test_audio_music_volume(void)
{
    mvn_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    MVN_ASSERT_NEAR(mvn_mus_get_volume(aud), 1.0f, 0.001);

    mvn_mus_volume(aud, 0.3f);
    MVN_ASSERT_NEAR(mvn_mus_get_volume(aud), 0.3f, 0.001);

    mvn_audio_destroy(aud);
    MVN_PASS();
}

static void test_audio_sfx_play_invalid_idx(void)
{
    mvn_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* No SFX loaded — idx 0 is out-of-range, should not crash */
    mvn_sfx_play(aud, 0, 0, 0);
    mvn_sfx_play(aud, -1, 0, 0);
    mvn_sfx_play(aud, 999, 0, 0);

    mvn_audio_destroy(aud);
    MVN_PASS();
}

static void test_audio_sfx_play_invalid_channel(void)
{
    mvn_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* Channel out-of-range — should silently return */
    mvn_sfx_play(aud, 0, -1, 0);
    mvn_sfx_play(aud, 0, 999, 0);

    mvn_audio_destroy(aud);
    MVN_PASS();
}

static void test_audio_music_not_playing(void)
{
    mvn_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    MVN_ASSERT(!mvn_mus_playing(aud));
    mvn_mus_stop(aud, 0);   /* stop when nothing playing — safe */
    mvn_mus_stop(aud, 100); /* fade stop when nothing playing — safe */

    mvn_audio_destroy(aud);
    MVN_PASS();
}

static void test_audio_sfx_stop_all(void)
{
    mvn_audio_t *aud;

    aud = prv_try_create();
    if (aud == NULL) {
        printf("  SKIP %s (no audio device)\n", __func__);
        return;
    }

    /* Stop-all with channel = -1 */
    mvn_sfx_stop(aud, -1);

    mvn_audio_destroy(aud);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Runner                                                             */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("test_audio\n");

    /* NULL safety (always run) */
    test_audio_destroy_null();
    test_sfx_play_null();
    test_sfx_stop_null();
    test_sfx_volume_null();
    test_sfx_get_volume_null();
    test_sfx_playing_null();
    test_mus_play_null();
    test_mus_stop_null();
    test_mus_volume_null();
    test_mus_get_volume_null();
    test_mus_playing_null();
    test_load_sfx_null_audio();
    test_load_music_null_audio();
    test_sfx_playing_out_of_bounds();

    /* Live tests (skip if no audio device) */
    test_audio_create_destroy();
    test_audio_channel_volume();
    test_audio_music_volume();
    test_audio_sfx_play_invalid_idx();
    test_audio_sfx_play_invalid_channel();
    test_audio_music_not_playing();
    test_audio_sfx_stop_all();

    printf("test_audio: all passed\n");
    return 0;
}
