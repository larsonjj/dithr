/**
 * \file            audio.h
 * \brief           Audio subsystem — SFX and music via SDL_mixer
 */

#ifndef DTR_AUDIO_H
#define DTR_AUDIO_H

#include "console.h"

#include <SDL3_mixer/SDL_mixer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DTR_MAX_SFX   256
#define DTR_MAX_MUSIC 64

/**
 * \brief           Audio subsystem state
 */
struct dtr_audio {
    int32_t num_channels;
    int32_t frequency;
    int32_t buffer_size;

    MIX_Mixer *mixer;

    MIX_Audio *sfx[DTR_MAX_SFX];
    int32_t    sfx_count;
    MIX_Track *sfx_tracks[CONSOLE_MAX_CHANNELS];

    MIX_Audio *music[DTR_MAX_MUSIC];
    int32_t    music_count;
    MIX_Track *music_track;

    float master_volume;
    float music_volume;
    float channel_volume[CONSOLE_MAX_CHANNELS];

    int32_t current_music_idx; /**< Index of currently playing music (-1 = none) */

    bool initialized;
};

/* Lifecycle */
dtr_audio_t *dtr_audio_create(int32_t channels, int32_t freq, int32_t buffer);
void         dtr_audio_destroy(dtr_audio_t *aud);

/* SFX */
bool  dtr_audio_load_sfx(dtr_audio_t *aud, const uint8_t *data, size_t len);
void  dtr_sfx_play(dtr_audio_t *aud, int32_t idx, int32_t channel, int32_t length);
void  dtr_sfx_stop(dtr_audio_t *aud, int32_t channel);
void  dtr_sfx_volume(dtr_audio_t *aud, float vol, int32_t channel);
float dtr_sfx_get_volume(dtr_audio_t *aud, int32_t channel);
bool  dtr_sfx_playing(dtr_audio_t *aud, int32_t channel);

/* Music */
bool  dtr_audio_load_music(dtr_audio_t *aud, const uint8_t *data, size_t len);
void  dtr_mus_play(dtr_audio_t *aud, int32_t idx, int32_t fade_ms, uint32_t channel_mask);
void  dtr_mus_stop(dtr_audio_t *aud, int32_t fade_ms);
void  dtr_mus_volume(dtr_audio_t *aud, float vol);
float dtr_mus_get_volume(dtr_audio_t *aud);
bool  dtr_mus_playing(dtr_audio_t *aud);
int32_t dtr_mus_current(dtr_audio_t *aud);

/* Master volume */
void  dtr_audio_set_master_volume(dtr_audio_t *aud, float vol);
float dtr_audio_get_master_volume(dtr_audio_t *aud);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_AUDIO_H */
