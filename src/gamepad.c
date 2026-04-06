/**
 * \file            gamepad.c
 * \brief           Gamepad state — buttons, axes, hotplug, rumble
 */

#include "gamepad.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/*  SDL gamepad button → dtr_pad_btn_t                                 */
/* ------------------------------------------------------------------ */

static dtr_pad_btn_t prv_map_button(SDL_GamepadButton btn)
{
    switch (btn) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return DTR_PAD_UP;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return DTR_PAD_DOWN;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return DTR_PAD_LEFT;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return DTR_PAD_RIGHT;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return DTR_PAD_A;
        case SDL_GAMEPAD_BUTTON_EAST:
            return DTR_PAD_B;
        case SDL_GAMEPAD_BUTTON_WEST:
            return DTR_PAD_X;
        case SDL_GAMEPAD_BUTTON_NORTH:
            return DTR_PAD_Y;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return DTR_PAD_L1;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return DTR_PAD_R1;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return DTR_PAD_L3;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return DTR_PAD_R3;
        case SDL_GAMEPAD_BUTTON_START:
            return DTR_PAD_START;
        case SDL_GAMEPAD_BUTTON_BACK:
            return DTR_PAD_SELECT;
        case SDL_GAMEPAD_BUTTON_GUIDE:
            return DTR_PAD_GUIDE;
        default:
            return DTR_PAD_BTN_COUNT;
    }
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

dtr_gamepad_state_t *dtr_gamepad_create(void)
{
    dtr_gamepad_state_t *gp;

    gp = DTR_CALLOC(1, sizeof(dtr_gamepad_state_t));
    if (gp == NULL) {
        return NULL;
    }

    for (int32_t idx = 0; idx < DTR_MAX_GAMEPADS; ++idx) {
        gp->pads[idx].deadzone = 0.15f;
    }

    return gp;
}

void dtr_gamepad_destroy(dtr_gamepad_state_t *gp)
{
    if (gp == NULL) {
        return;
    }

    for (int32_t idx = 0; idx < DTR_MAX_GAMEPADS; ++idx) {
        if (gp->pads[idx].handle != NULL) {
            SDL_CloseGamepad(gp->pads[idx].handle);
        }
    }

    DTR_FREE(gp);
}

/* ------------------------------------------------------------------ */
/*  Update — poll buttons/axes                                         */
/* ------------------------------------------------------------------ */

void dtr_gamepad_update(dtr_gamepad_state_t *gp)
{
    for (int32_t idx = 0; idx < DTR_MAX_GAMEPADS; ++idx) {
        dtr_gamepad_t *pad;

        pad = &gp->pads[idx];
        if (!pad->connected || pad->handle == NULL) {
            continue;
        }

        /* Save previous state and clear press latches */
        SDL_memcpy(pad->btn_previous, pad->btn_current, sizeof(pad->btn_current));
        SDL_memset(pad->btn_pressed, 0, sizeof(pad->btn_pressed));

        /* Apply deadzone to axes */
        {
            float dz;

            dz = pad->deadzone;

            for (int32_t a = 0; a < DTR_PAD_AXIS_COUNT; ++a) {
                if (fabsf(pad->axes[a]) < dz) {
                    pad->axes[a] = 0.0f;
                }
            }

            /* L2/R2 as digital buttons */
            pad->btn_current[DTR_PAD_L2] = (pad->axes[DTR_PAD_AXIS_L2] > 0.5f);
            pad->btn_current[DTR_PAD_R2] = (pad->axes[DTR_PAD_AXIS_R2] > 0.5f);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Hotplug                                                            */
/* ------------------------------------------------------------------ */

void dtr_gamepad_on_added(dtr_gamepad_state_t *gp, SDL_JoystickID id)
{
    for (int32_t idx = 0; idx < DTR_MAX_GAMEPADS; ++idx) {
        dtr_gamepad_t *pad;

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

void dtr_gamepad_on_removed(dtr_gamepad_state_t *gp, SDL_JoystickID id)
{
    for (int32_t idx = 0; idx < DTR_MAX_GAMEPADS; ++idx) {
        dtr_gamepad_t *pad;

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

void dtr_gamepad_on_button(dtr_gamepad_state_t *gp,
                           SDL_JoystickID       id,
                           SDL_GamepadButton    btn,
                           bool                 down)
{
    for (int32_t idx = 0; idx < DTR_MAX_GAMEPADS; ++idx) {
        dtr_gamepad_t *pad;
        dtr_pad_btn_t  mapped;

        pad = &gp->pads[idx];
        if (!pad->connected || pad->joy_id != id) {
            continue;
        }

        mapped = prv_map_button(btn);
        if (mapped < DTR_PAD_BTN_COUNT) {
            pad->btn_current[mapped] = down;
            if (down) {
                pad->btn_pressed[mapped] = true;
            }
        }
        return;
    }
}

void dtr_gamepad_on_axis(dtr_gamepad_state_t *gp,
                         SDL_JoystickID       id,
                         SDL_GamepadAxis      axis,
                         int16_t              value)
{
    static const dtr_pad_axis_t axis_map[] = {
        [SDL_GAMEPAD_AXIS_LEFTX]         = DTR_PAD_AXIS_LX,
        [SDL_GAMEPAD_AXIS_LEFTY]         = DTR_PAD_AXIS_LY,
        [SDL_GAMEPAD_AXIS_RIGHTX]        = DTR_PAD_AXIS_RX,
        [SDL_GAMEPAD_AXIS_RIGHTY]        = DTR_PAD_AXIS_RY,
        [SDL_GAMEPAD_AXIS_LEFT_TRIGGER]  = DTR_PAD_AXIS_L2,
        [SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = DTR_PAD_AXIS_R2,
    };

    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) {
        return;
    }

    for (int32_t idx = 0; idx < DTR_MAX_GAMEPADS; ++idx) {
        dtr_gamepad_t *pad;

        pad = &gp->pads[idx];
        if (!pad->connected || pad->joy_id != id) {
            continue;
        }

        pad->axes[axis_map[axis]] = (float)value / 32767.0f;
        return;
    }
}

/* ------------------------------------------------------------------ */
/*  Queries                                                            */
/* ------------------------------------------------------------------ */

bool dtr_gamepad_btn(dtr_gamepad_state_t *gp, dtr_pad_btn_t b, int32_t index)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return false;
    }
    if (!gp->pads[index].connected || b >= DTR_PAD_BTN_COUNT) {
        return false;
    }
    return gp->pads[index].btn_current[b];
}

bool dtr_gamepad_btnp(dtr_gamepad_state_t *gp, dtr_pad_btn_t b, int32_t index)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return false;
    }
    if (!gp->pads[index].connected || b >= DTR_PAD_BTN_COUNT) {
        return false;
    }
    return gp->pads[index].btn_pressed[b];
}

float dtr_gamepad_axis(dtr_gamepad_state_t *gp, dtr_pad_axis_t a, int32_t index)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return 0.0f;
    }
    if (!gp->pads[index].connected || a >= DTR_PAD_AXIS_COUNT) {
        return 0.0f;
    }
    return gp->pads[index].axes[a];
}

bool dtr_gamepad_connected(dtr_gamepad_state_t *gp, int32_t index)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return false;
    }
    return gp->pads[index].connected;
}

int32_t dtr_gamepad_count(dtr_gamepad_state_t *gp)
{
    return gp->count;
}

const char *dtr_gamepad_name(dtr_gamepad_state_t *gp, int32_t index)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return "";
    }
    return gp->pads[index].name;
}

void dtr_gamepad_set_deadzone(dtr_gamepad_state_t *gp, float val, int32_t index)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return;
    }
    gp->pads[index].deadzone = val;
}

float dtr_gamepad_get_deadzone(dtr_gamepad_state_t *gp, int32_t index)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return 0.0f;
    }
    return gp->pads[index].deadzone;
}

void dtr_gamepad_rumble(dtr_gamepad_state_t *gp,
                        int32_t              index,
                        uint16_t             low,
                        uint16_t             high,
                        uint32_t             duration_ms)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return;
    }
    if (!gp->pads[index].connected || gp->pads[index].handle == NULL) {
        return;
    }
    SDL_RumbleGamepad(gp->pads[index].handle, low, high, duration_ms);
}

void dtr_gamepad_rumble_triggers(dtr_gamepad_state_t *gp,
                                 int32_t              index,
                                 uint16_t             left,
                                 uint16_t             right,
                                 uint32_t             duration_ms)
{
    if (index < 0 || index >= DTR_MAX_GAMEPADS) {
        return;
    }
    if (!gp->pads[index].connected || gp->pads[index].handle == NULL) {
        return;
    }
    SDL_RumbleGamepadTriggers(gp->pads[index].handle, left, right, duration_ms);
}
