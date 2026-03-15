/**
 * \file            gamepad.c
 * \brief           Gamepad state — buttons, axes, hotplug, rumble
 */

#include "gamepad.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/*  SDL gamepad button → mvn_pad_btn_t                                 */
/* ------------------------------------------------------------------ */

static mvn_pad_btn_t prv_map_button(SDL_GamepadButton btn)
{
    switch (btn) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return MVN_PAD_UP;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return MVN_PAD_DOWN;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return MVN_PAD_LEFT;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return MVN_PAD_RIGHT;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return MVN_PAD_A;
        case SDL_GAMEPAD_BUTTON_EAST:
            return MVN_PAD_B;
        case SDL_GAMEPAD_BUTTON_WEST:
            return MVN_PAD_X;
        case SDL_GAMEPAD_BUTTON_NORTH:
            return MVN_PAD_Y;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return MVN_PAD_L1;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return MVN_PAD_R1;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return MVN_PAD_L3;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return MVN_PAD_R3;
        case SDL_GAMEPAD_BUTTON_START:
            return MVN_PAD_START;
        case SDL_GAMEPAD_BUTTON_BACK:
            return MVN_PAD_SELECT;
        case SDL_GAMEPAD_BUTTON_GUIDE:
            return MVN_PAD_GUIDE;
        default:
            return MVN_PAD_BTN_COUNT;
    }
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

mvn_gamepad_state_t *mvn_gamepad_create(void)
{
    mvn_gamepad_state_t *gp;

    gp = MVN_CALLOC(1, sizeof(mvn_gamepad_state_t));
    if (gp == NULL) {
        return NULL;
    }

    for (int32_t idx = 0; idx < MVN_MAX_GAMEPADS; ++idx) {
        gp->pads[idx].deadzone = 0.15f;
    }

    return gp;
}

void mvn_gamepad_destroy(mvn_gamepad_state_t *gp)
{
    if (gp == NULL) {
        return;
    }

    for (int32_t idx = 0; idx < MVN_MAX_GAMEPADS; ++idx) {
        if (gp->pads[idx].handle != NULL) {
            SDL_CloseGamepad(gp->pads[idx].handle);
        }
    }

    MVN_FREE(gp);
}

/* ------------------------------------------------------------------ */
/*  Update — poll buttons/axes                                         */
/* ------------------------------------------------------------------ */

void mvn_gamepad_update(mvn_gamepad_state_t *gp)
{
    for (int32_t idx = 0; idx < MVN_MAX_GAMEPADS; ++idx) {
        mvn_gamepad_t *pad;

        pad = &gp->pads[idx];
        if (!pad->connected || pad->handle == NULL) {
            continue;
        }

        /* Save previous state */
        SDL_memcpy(pad->btn_previous, pad->btn_current, sizeof(pad->btn_current));

        /* Poll buttons */
        for (int32_t btn = 0; btn < SDL_GAMEPAD_BUTTON_COUNT; ++btn) {
            mvn_pad_btn_t mapped;

            mapped = prv_map_button((SDL_GamepadButton)btn);
            if (mapped < MVN_PAD_BTN_COUNT) {
                pad->btn_current[mapped] =
                    SDL_GetGamepadButton(pad->handle, (SDL_GamepadButton)btn);
            }
        }

        /* Poll axes */
        {
            float dz;

            dz = pad->deadzone;

            pad->axes[MVN_PAD_AXIS_LX] =
                (float)SDL_GetGamepadAxis(pad->handle, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
            pad->axes[MVN_PAD_AXIS_LY] =
                (float)SDL_GetGamepadAxis(pad->handle, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
            pad->axes[MVN_PAD_AXIS_RX] =
                (float)SDL_GetGamepadAxis(pad->handle, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
            pad->axes[MVN_PAD_AXIS_RY] =
                (float)SDL_GetGamepadAxis(pad->handle, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
            pad->axes[MVN_PAD_AXIS_L2] =
                (float)SDL_GetGamepadAxis(pad->handle, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
            pad->axes[MVN_PAD_AXIS_R2] =
                (float)SDL_GetGamepadAxis(pad->handle, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;

            /* Apply deadzone */
            for (int32_t a = 0; a < MVN_PAD_AXIS_COUNT; ++a) {
                if (fabsf(pad->axes[a]) < dz) {
                    pad->axes[a] = 0.0f;
                }
            }

            /* L2/R2 as digital buttons */
            pad->btn_current[MVN_PAD_L2] = (pad->axes[MVN_PAD_AXIS_L2] > 0.5f);
            pad->btn_current[MVN_PAD_R2] = (pad->axes[MVN_PAD_AXIS_R2] > 0.5f);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Hotplug                                                            */
/* ------------------------------------------------------------------ */

void mvn_gamepad_on_added(mvn_gamepad_state_t *gp, SDL_JoystickID id)
{
    for (int32_t idx = 0; idx < MVN_MAX_GAMEPADS; ++idx) {
        mvn_gamepad_t *pad;

        pad = &gp->pads[idx];
        if (pad->connected) {
            continue;
        }

        pad->handle = SDL_OpenGamepad(id);
        if (pad->handle == NULL) {
            return;
        }

        pad->joy_id                  = id;
        pad->connected               = true;
        pad->supports_rumble         = SDL_RumbleGamepad(pad->handle, 0, 0, 0);
        pad->supports_trigger_rumble = SDL_RumbleGamepadTriggers(pad->handle, 0, 0, 0);

        {
            const char *name;

            name = SDL_GetGamepadName(pad->handle);
            if (name != NULL) {
                SDL_strlcpy(pad->name, name, sizeof(pad->name));
            } else {
                SDL_strlcpy(pad->name, "Unknown", sizeof(pad->name));
            }
        }

        ++gp->count;
        SDL_Log("Gamepad %d connected: %s", idx, pad->name);
        return;
    }
}

void mvn_gamepad_on_removed(mvn_gamepad_state_t *gp, SDL_JoystickID id)
{
    for (int32_t idx = 0; idx < MVN_MAX_GAMEPADS; ++idx) {
        mvn_gamepad_t *pad;

        pad = &gp->pads[idx];
        if (!pad->connected || pad->joy_id != id) {
            continue;
        }

        SDL_CloseGamepad(pad->handle);
        pad->handle    = NULL;
        pad->connected = false;
        --gp->count;

        SDL_Log("Gamepad %d disconnected: %s", idx, pad->name);
        return;
    }
}

/* ------------------------------------------------------------------ */
/*  Queries                                                            */
/* ------------------------------------------------------------------ */

bool mvn_gamepad_btn(mvn_gamepad_state_t *gp, mvn_pad_btn_t b, int32_t index)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return false;
    }
    if (!gp->pads[index].connected || b >= MVN_PAD_BTN_COUNT) {
        return false;
    }
    return gp->pads[index].btn_current[b];
}

bool mvn_gamepad_btnp(mvn_gamepad_state_t *gp, mvn_pad_btn_t b, int32_t index)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return false;
    }
    if (!gp->pads[index].connected || b >= MVN_PAD_BTN_COUNT) {
        return false;
    }
    return gp->pads[index].btn_current[b] && !gp->pads[index].btn_previous[b];
}

float mvn_gamepad_axis(mvn_gamepad_state_t *gp, mvn_pad_axis_t a, int32_t index)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return 0.0f;
    }
    if (!gp->pads[index].connected || a >= MVN_PAD_AXIS_COUNT) {
        return 0.0f;
    }
    return gp->pads[index].axes[a];
}

bool mvn_gamepad_connected(mvn_gamepad_state_t *gp, int32_t index)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return false;
    }
    return gp->pads[index].connected;
}

int32_t mvn_gamepad_count(mvn_gamepad_state_t *gp)
{
    return gp->count;
}

const char *mvn_gamepad_name(mvn_gamepad_state_t *gp, int32_t index)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return "";
    }
    return gp->pads[index].name;
}

void mvn_gamepad_set_deadzone(mvn_gamepad_state_t *gp, float val, int32_t index)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return;
    }
    gp->pads[index].deadzone = val;
}

float mvn_gamepad_get_deadzone(mvn_gamepad_state_t *gp, int32_t index)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return 0.0f;
    }
    return gp->pads[index].deadzone;
}

void mvn_gamepad_rumble(mvn_gamepad_state_t *gp,
                        int32_t              index,
                        uint16_t             low,
                        uint16_t             high,
                        uint32_t             duration_ms)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return;
    }
    if (!gp->pads[index].connected || gp->pads[index].handle == NULL) {
        return;
    }
    SDL_RumbleGamepad(gp->pads[index].handle, low, high, duration_ms);
}

void mvn_gamepad_rumble_triggers(mvn_gamepad_state_t *gp,
                                 int32_t              index,
                                 uint16_t             left,
                                 uint16_t             right,
                                 uint32_t             duration_ms)
{
    if (index < 0 || index >= MVN_MAX_GAMEPADS) {
        return;
    }
    if (!gp->pads[index].connected || gp->pads[index].handle == NULL) {
        return;
    }
    SDL_RumbleGamepadTriggers(gp->pads[index].handle, left, right, duration_ms);
}
