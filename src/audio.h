/**
 * \file            audio.h
 * \brief           Audio subsystem — SFX and music via SDL_mixer
 */

#ifndef MVN_AUDIO_H
#define MVN_AUDIO_H

#include "console.h"

#include <SDL3_mixer/SDL_mixer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MVN_MAX_SFX   256
#define MVN_MAX_MUSIC 64

/**
 * \brief           Audio subsystem state
 */
typedef struct mvn_audio {
    int32_t num_channels;
    int32_t frequency;
    int32_t buffer_size;

    MIX_Mixer *mixer;

    MIX_Audio *sfx[MVN_MAX_SFX];
    int32_t    sfx_count;
    MIX_Track *sfx_tracks[CONSOLE_MAX_CHANNELS];

    MIX_Audio *music[MVN_MAX_MUSIC];
    int32_t    music_count;
    MIX_Track *music_track;

    float master_volume;
    float channel_volume[CONSOLE_MAX_CHANNELS];

    bool initialized;
} mvn_audio_t;

/* Lifecycle */
mvn_audio_t *mvn_audio_create(int32_t channels, int32_t freq, int32_t buffer);
void         mvn_audio_destroy(mvn_audio_t *aud);

/* SFX */
bool  mvn_audio_load_sfx(mvn_audio_t *aud, const uint8_t *data, size_t len);
void  mvn_sfx_play(mvn_audio_t *aud, int32_t idx, int32_t channel, int32_t offset, int32_t length);
void  mvn_sfx_stop(mvn_audio_t *aud, int32_t channel);
void  mvn_sfx_volume(mvn_audio_t *aud, float vol, int32_t channel);
float mvn_sfx_get_volume(mvn_audio_t *aud, int32_t channel);
bool  mvn_sfx_playing(mvn_audio_t *aud, int32_t channel);

/* Music */
bool  mvn_audio_load_music(mvn_audio_t *aud, const uint8_t *data, size_t len);
void  mvn_mus_play(mvn_audio_t *aud, int32_t idx, int32_t fade_ms, uint32_t channel_mask);
void  mvn_mus_stop(mvn_audio_t *aud, int32_t fade_ms);
void  mvn_mus_volume(mvn_audio_t *aud, float vol);
float mvn_mus_get_volume(mvn_audio_t *aud);
bool  mvn_mus_playing(mvn_audio_t *aud);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_AUDIO_H */
