/**
 * \file            audio.c
 * \brief           Audio subsystem — SFX and music via SDL_mixer 3.x
 */

#include "audio.h"

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

dtr_audio_t *dtr_audio_create(int32_t channels, int32_t freq, int32_t buffer)
{
    dtr_audio_t * aud;
    SDL_AudioSpec spec;

    (void)buffer;

    aud = DTR_CALLOC(1, sizeof(dtr_audio_t));
    if (aud == NULL) {
        return NULL;
    }

    aud->num_channels  = channels;
    aud->frequency     = freq;
    aud->buffer_size   = buffer;
    aud->master_volume = 1.0f;
    aud->music_volume  = 1.0f;

    for (int32_t idx = 0; idx < CONSOLE_MAX_CHANNELS; ++idx) {
        aud->channel_volume[idx] = 1.0f;
    }

    if (!MIX_Init()) {
        SDL_Log("MIX_Init failed: %s", SDL_GetError());
        DTR_FREE(aud);
        return NULL;
    }

    spec.freq     = freq;
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;

    aud->mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (aud->mixer == NULL) {
        SDL_Log("MIX_CreateMixerDevice failed: %s", SDL_GetError());
        MIX_Quit();
        DTR_FREE(aud);
        return NULL;
    }

    /* Pre-create tracks for SFX channels */
    for (int32_t idx = 0; idx < channels && idx < CONSOLE_MAX_CHANNELS; ++idx) {
        aud->sfx_tracks[idx] = MIX_CreateTrack(aud->mixer);
    }

    /* One dedicated track for music */
    aud->music_track = MIX_CreateTrack(aud->mixer);
    if (aud->music_track != NULL) {
        MIX_SetTrackLoops(aud->music_track, -1);
    }

    aud->initialized = true;
    return aud;
}

void dtr_audio_destroy(dtr_audio_t *aud)
{
    if (aud == NULL) {
        return;
    }

    /* Destroy SFX tracks */
    for (int32_t idx = 0; idx < CONSOLE_MAX_CHANNELS; ++idx) {
        if (aud->sfx_tracks[idx] != NULL) {
            MIX_DestroyTrack(aud->sfx_tracks[idx]);
        }
    }

    /* Destroy music track */
    if (aud->music_track != NULL) {
        MIX_DestroyTrack(aud->music_track);
    }

    /* Free loaded audio data */
    for (int32_t idx = 0; idx < aud->sfx_count; ++idx) {
        if (aud->sfx[idx] != NULL) {
            MIX_DestroyAudio(aud->sfx[idx]);
        }
    }
    for (int32_t idx = 0; idx < aud->music_count; ++idx) {
        if (aud->music[idx] != NULL) {
            MIX_DestroyAudio(aud->music[idx]);
        }
    }

    if (aud->mixer != NULL) {
        MIX_DestroyMixer(aud->mixer);
    }

    MIX_Quit();
    DTR_FREE(aud);
}

/* ------------------------------------------------------------------ */
/*  SFX loading                                                        */
/* ------------------------------------------------------------------ */

bool dtr_audio_load_sfx(dtr_audio_t *aud, const uint8_t *data, size_t len)
{
    SDL_IOStream *io;
    MIX_Audio *   audio;

    if (aud == NULL || aud->sfx_count >= DTR_MAX_SFX) {
        return false;
    }

    io = SDL_IOFromConstMem(data, len);
    if (io == NULL) {
        return false;
    }

    audio = MIX_LoadAudio_IO(aud->mixer, io, true, true);
    if (audio == NULL) {
        return false;
    }

    aud->sfx[aud->sfx_count] = audio;
    ++aud->sfx_count;
    return true;
}

/* ------------------------------------------------------------------ */
/*  SFX playback                                                       */
/* ------------------------------------------------------------------ */

void dtr_sfx_play(dtr_audio_t *aud, int32_t idx, int32_t channel, int32_t length)
{
    MIX_Track *     track;
    SDL_PropertiesID props;

    if (aud == NULL || idx < 0 || idx >= aud->sfx_count) {
        return;
    }
    if (aud->sfx[idx] == NULL) {
        return;
    }
    if (channel < 0 || channel >= aud->num_channels) {
        return;
    }

    track = aud->sfx_tracks[channel];
    if (track == NULL) {
        return;
    }

    MIX_SetTrackAudio(track, aud->sfx[idx]);
    MIX_SetTrackLoops(track, (length > 0) ? length - 1 : 0);

    props = SDL_CreateProperties();
    if (props == 0) {
        return;
    }
    MIX_PlayTrack(track, props);
    SDL_DestroyProperties(props);
}

void dtr_sfx_stop(dtr_audio_t *aud, int32_t channel)
{
    if (aud == NULL) {
        return;
    }
    if (channel < 0 || channel >= CONSOLE_MAX_CHANNELS) {
        /* Stop all */
        for (int32_t idx = 0; idx < CONSOLE_MAX_CHANNELS; ++idx) {
            if (aud->sfx_tracks[idx] != NULL) {
                MIX_StopTrack(aud->sfx_tracks[idx], 0);
            }
        }
        return;
    }
    if (aud->sfx_tracks[channel] != NULL) {
        MIX_StopTrack(aud->sfx_tracks[channel], 0);
    }
}

void dtr_sfx_volume(dtr_audio_t *aud, float vol, int32_t channel)
{
    if (aud == NULL) {
        return;
    }
    if (channel >= 0 && channel < CONSOLE_MAX_CHANNELS) {
        aud->channel_volume[channel] = vol;
        if (aud->sfx_tracks[channel] != NULL) {
            MIX_SetTrackGain(aud->sfx_tracks[channel], vol * aud->master_volume);
        }
    }
}

float dtr_sfx_get_volume(dtr_audio_t *aud, int32_t channel)
{
    if (aud == NULL) {
        return 0.0f;
    }
    if (channel >= 0 && channel < CONSOLE_MAX_CHANNELS) {
        return aud->channel_volume[channel];
    }
    return 0.0f;
}

bool dtr_sfx_playing(dtr_audio_t *aud, int32_t channel)
{
    if (aud == NULL) {
        return false;
    }
    if (channel < 0 || channel >= CONSOLE_MAX_CHANNELS) {
        return false;
    }
    if (aud->sfx_tracks[channel] == NULL) {
        return false;
    }
    return MIX_TrackPlaying(aud->sfx_tracks[channel]);
}

/* ------------------------------------------------------------------ */
/*  Music loading                                                      */
/* ------------------------------------------------------------------ */

bool dtr_audio_load_music(dtr_audio_t *aud, const uint8_t *data, size_t len)
{
    SDL_IOStream *io;
    MIX_Audio *   audio;

    if (aud == NULL || aud->music_count >= DTR_MAX_MUSIC) {
        return false;
    }

    io = SDL_IOFromConstMem(data, len);
    if (io == NULL) {
        return false;
    }

    audio = MIX_LoadAudio_IO(aud->mixer, io, false, true);
    if (audio == NULL) {
        return false;
    }

    aud->music[aud->music_count] = audio;
    ++aud->music_count;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Music playback                                                     */
/* ------------------------------------------------------------------ */

void dtr_mus_play(dtr_audio_t *aud, int32_t idx, int32_t fade_ms, uint32_t channel_mask)
{
    SDL_PropertiesID props;

    (void)channel_mask;

    if (aud == NULL || idx < 0 || idx >= aud->music_count) {
        return;
    }
    if (aud->music[idx] == NULL || aud->music_track == NULL) {
        return;
    }

    MIX_SetTrackAudio(aud->music_track, aud->music[idx]);
    MIX_SetTrackLoops(aud->music_track, -1);

    props = SDL_CreateProperties();
    if (props == 0) {
        return;
    }
    if (fade_ms > 0) {
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER,
                              fade_ms);
    }
    MIX_PlayTrack(aud->music_track, props);
    SDL_DestroyProperties(props);
}

void dtr_mus_stop(dtr_audio_t *aud, int32_t fade_ms)
{
    if (aud == NULL || aud->music_track == NULL) {
        return;
    }

    if (fade_ms > 0) {
        Sint64 fade_frames;

        fade_frames = MIX_TrackMSToFrames(aud->music_track, fade_ms);
        MIX_StopTrack(aud->music_track, fade_frames);
    } else {
        MIX_StopTrack(aud->music_track, 0);
    }
}

void dtr_mus_volume(dtr_audio_t *aud, float vol)
{
    if (aud == NULL) {
        return;
    }
    aud->music_volume = vol;
    if (aud->music_track != NULL) {
        MIX_SetTrackGain(aud->music_track, vol * aud->master_volume);
    }
}

float dtr_mus_get_volume(dtr_audio_t *aud)
{
    if (aud == NULL) {
        return 0.0f;
    }
    return aud->music_volume;
}

bool dtr_mus_playing(dtr_audio_t *aud)
{
    if (aud == NULL || aud->music_track == NULL) {
        return false;
    }
    return MIX_TrackPlaying(aud->music_track);
}

/* ------------------------------------------------------------------ */
/*  Master volume                                                      */
/* ------------------------------------------------------------------ */

void dtr_audio_set_master_volume(dtr_audio_t *aud, float vol)
{
    if (aud == NULL) {
        return;
    }
    if (vol < 0.0f) {
        vol = 0.0f;
    }
    if (vol > 1.0f) {
        vol = 1.0f;
    }
    aud->master_volume = vol;

    /* Re-apply gain to all SFX channels */
    for (int32_t idx = 0; idx < CONSOLE_MAX_CHANNELS; ++idx) {
        if (aud->sfx_tracks[idx] != NULL) {
            MIX_SetTrackGain(aud->sfx_tracks[idx], aud->channel_volume[idx] * vol);
        }
    }

    /* Re-apply gain to music track */
    if (aud->music_track != NULL) {
        MIX_SetTrackGain(aud->music_track, aud->music_volume * vol);
    }
}

float dtr_audio_get_master_volume(dtr_audio_t *aud)
{
    if (aud == NULL) {
        return 0.0f;
    }
    return aud->master_volume;
}
