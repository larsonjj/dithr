/**
 * \file            audio.c
 * \brief           Audio subsystem — SFX and music via SDL_mixer
 */

#include "audio.h"

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

mvn_audio_t *mvn_audio_create(int32_t channels, int32_t freq, int32_t buffer)
{
    mvn_audio_t * aud;
    SDL_AudioSpec spec;

    aud = MVN_CALLOC(1, sizeof(mvn_audio_t));
    if (aud == NULL) {
        return NULL;
    }

    aud->num_channels  = channels;
    aud->frequency     = freq;
    aud->buffer_size   = buffer;
    aud->master_volume = 1.0f;

    for (int32_t idx = 0; idx < CONSOLE_MAX_CHANNELS; ++idx) {
        aud->channel_volume[idx] = 1.0f;
    }

    spec.freq     = freq;
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;

    if (!Mix_OpenAudio(0, &spec)) {
        SDL_Log("Mix_OpenAudio failed: %s", SDL_GetError());
        MVN_FREE(aud);
        return NULL;
    }

    Mix_AllocateChannels(channels);
    aud->initialized = true;

    return aud;
}

void mvn_audio_destroy(mvn_audio_t *aud)
{
    if (aud == NULL) {
        return;
    }

    for (int32_t idx = 0; idx < aud->sfx_count; ++idx) {
        if (aud->sfx[idx] != NULL) {
            Mix_FreeChunk(aud->sfx[idx]);
        }
    }
    for (int32_t idx = 0; idx < aud->music_count; ++idx) {
        if (aud->music[idx] != NULL) {
            Mix_FreeMusic(aud->music[idx]);
        }
    }

    if (aud->initialized) {
        Mix_CloseAudio();
    }

    MVN_FREE(aud);
}

/* ------------------------------------------------------------------ */
/*  SFX loading                                                        */
/* ------------------------------------------------------------------ */

bool mvn_audio_load_sfx(mvn_audio_t *aud, const uint8_t *data, size_t len)
{
    SDL_IOStream *rw;
    Mix_Chunk *   chunk;

    if (aud->sfx_count >= MVN_MAX_SFX) {
        return false;
    }

    rw = SDL_IOFromConstMem(data, len);
    if (rw == NULL) {
        return false;
    }

    chunk = Mix_LoadWAV_IO(rw, true);
    if (chunk == NULL) {
        return false;
    }

    aud->sfx[aud->sfx_count] = chunk;
    ++aud->sfx_count;
    return true;
}

/* ------------------------------------------------------------------ */
/*  SFX playback                                                       */
/* ------------------------------------------------------------------ */

void mvn_sfx_play(mvn_audio_t *aud, int32_t idx, int32_t channel, int32_t offset, int32_t length)
{
    (void)offset;

    if (aud == NULL || idx < 0 || idx >= aud->sfx_count) {
        return;
    }
    if (aud->sfx[idx] == NULL) {
        return;
    }

    Mix_PlayChannel(channel, aud->sfx[idx], (length > 0) ? length - 1 : 0);
}

void mvn_sfx_stop(mvn_audio_t *aud, int32_t channel)
{
    (void)aud;
    Mix_HaltChannel(channel);
}

void mvn_sfx_volume(mvn_audio_t *aud, float vol, int32_t channel)
{
    if (channel >= 0 && channel < CONSOLE_MAX_CHANNELS) {
        aud->channel_volume[channel] = vol;
    }
    Mix_Volume(channel, (int32_t)(vol * (float)MIX_MAX_VOLUME));
}

float mvn_sfx_get_volume(mvn_audio_t *aud, int32_t channel)
{
    if (channel >= 0 && channel < CONSOLE_MAX_CHANNELS) {
        return aud->channel_volume[channel];
    }
    return 0.0f;
}

bool mvn_sfx_playing(mvn_audio_t *aud, int32_t channel)
{
    (void)aud;
    return Mix_Playing(channel) != 0;
}

/* ------------------------------------------------------------------ */
/*  Music loading                                                      */
/* ------------------------------------------------------------------ */

bool mvn_audio_load_music(mvn_audio_t *aud, const uint8_t *data, size_t len)
{
    SDL_IOStream *rw;
    Mix_Music *   mus;

    if (aud->music_count >= MVN_MAX_MUSIC) {
        return false;
    }

    rw = SDL_IOFromConstMem(data, len);
    if (rw == NULL) {
        return false;
    }

    mus = Mix_LoadMUS_IO(rw, true);
    if (mus == NULL) {
        return false;
    }

    aud->music[aud->music_count] = mus;
    ++aud->music_count;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Music playback                                                     */
/* ------------------------------------------------------------------ */

void mvn_mus_play(mvn_audio_t *aud, int32_t idx, int32_t fade_ms, uint32_t channel_mask)
{
    (void)channel_mask;

    if (aud == NULL || idx < 0 || idx >= aud->music_count) {
        return;
    }
    if (aud->music[idx] == NULL) {
        return;
    }

    if (fade_ms > 0) {
        Mix_FadeInMusic(aud->music[idx], -1, fade_ms);
    } else {
        Mix_PlayMusic(aud->music[idx], -1);
    }
}

void mvn_mus_stop(mvn_audio_t *aud, int32_t fade_ms)
{
    (void)aud;
    if (fade_ms > 0) {
        Mix_FadeOutMusic(fade_ms);
    } else {
        Mix_HaltMusic();
    }
}

void mvn_mus_volume(mvn_audio_t *aud, float vol)
{
    aud->master_volume = vol;
    Mix_VolumeMusic((int32_t)(vol * (float)MIX_MAX_VOLUME));
}

float mvn_mus_get_volume(mvn_audio_t *aud)
{
    return aud->master_volume;
}

bool mvn_mus_playing(mvn_audio_t *aud)
{
    (void)aud;
    return Mix_PlayingMusic() != 0;
}
