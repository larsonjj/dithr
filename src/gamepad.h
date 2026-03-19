/**
 * \file            gamepad.h
 * \brief           Gamepad state — buttons, axes, hotplug, rumble
 */

#ifndef DTR_GAMEPAD_H
#define DTR_GAMEPAD_H

#include "console.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DTR_MAX_GAMEPADS 4

/**
 * \brief           State for a single gamepad
 */
typedef struct dtr_gamepad {
    SDL_Gamepad *  handle;
    SDL_JoystickID joy_id;
    bool           connected;
    char           name[128];

    bool  btn_current[DTR_PAD_BTN_COUNT];
    bool  btn_previous[DTR_PAD_BTN_COUNT];
    bool  btn_pressed[DTR_PAD_BTN_COUNT];
    float axes[DTR_PAD_AXIS_COUNT];

    float deadzone;
    bool  supports_rumble;
    bool  supports_trigger_rumble;
} dtr_gamepad_t;

/**
 * \brief           Gamepad subsystem managing up to DTR_MAX_GAMEPADS
 */
struct dtr_gamepad_state {
    dtr_gamepad_t pads[DTR_MAX_GAMEPADS];
    int32_t       count;
};

dtr_gamepad_state_t *dtr_gamepad_create(void);
void                 dtr_gamepad_destroy(dtr_gamepad_state_t *gp);
void                 dtr_gamepad_update(dtr_gamepad_state_t *gp);

/* Hotplug */
void dtr_gamepad_on_added(dtr_gamepad_state_t *gp, SDL_JoystickID id);
void dtr_gamepad_on_removed(dtr_gamepad_state_t *gp, SDL_JoystickID id);
void dtr_gamepad_on_button(dtr_gamepad_state_t *gp, SDL_JoystickID id,
                          SDL_GamepadButton btn, bool down);
void dtr_gamepad_on_axis(dtr_gamepad_state_t *gp, SDL_JoystickID id,
                        SDL_GamepadAxis axis, int16_t value);

/* Queries */
bool        dtr_gamepad_btn(dtr_gamepad_state_t *gp, dtr_pad_btn_t b, int32_t index);
bool        dtr_gamepad_btnp(dtr_gamepad_state_t *gp, dtr_pad_btn_t b, int32_t index);
float       dtr_gamepad_axis(dtr_gamepad_state_t *gp, dtr_pad_axis_t a, int32_t index);
bool        dtr_gamepad_connected(dtr_gamepad_state_t *gp, int32_t index);
int32_t     dtr_gamepad_count(dtr_gamepad_state_t *gp);
const char *dtr_gamepad_name(dtr_gamepad_state_t *gp, int32_t index);
void        dtr_gamepad_set_deadzone(dtr_gamepad_state_t *gp, float val, int32_t index);
float       dtr_gamepad_get_deadzone(dtr_gamepad_state_t *gp, int32_t index);

/* Rumble */
void dtr_gamepad_rumble(dtr_gamepad_state_t *gp,
                        int32_t              index,
                        uint16_t             low,
                        uint16_t             high,
                        uint32_t             duration_ms);
void dtr_gamepad_rumble_triggers(dtr_gamepad_state_t *gp,
                                 int32_t              index,
                                 uint16_t             left,
                                 uint16_t             right,
                                 uint32_t             duration_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_GAMEPAD_H */
