/**
 * \file            gamepad.h
 * \brief           Gamepad state — buttons, axes, hotplug, rumble
 */

#ifndef MVN_GAMEPAD_H
#define MVN_GAMEPAD_H

#include "console.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MVN_MAX_GAMEPADS 4

/**
 * \brief           State for a single gamepad
 */
typedef struct mvn_gamepad {
    SDL_Gamepad *  handle;
    SDL_JoystickID joy_id;
    bool           connected;
    char           name[128];

    bool  btn_current[MVN_PAD_BTN_COUNT];
    bool  btn_previous[MVN_PAD_BTN_COUNT];
    float axes[MVN_PAD_AXIS_COUNT];

    float deadzone;
    bool  supports_rumble;
    bool  supports_trigger_rumble;
} mvn_gamepad_t;

/**
 * \brief           Gamepad subsystem managing up to MVN_MAX_GAMEPADS
 */
struct mvn_gamepad_state {
    mvn_gamepad_t pads[MVN_MAX_GAMEPADS];
    int32_t       count;
};

mvn_gamepad_state_t *mvn_gamepad_create(void);
void                 mvn_gamepad_destroy(mvn_gamepad_state_t *gp);
void                 mvn_gamepad_update(mvn_gamepad_state_t *gp);

/* Hotplug */
void mvn_gamepad_on_added(mvn_gamepad_state_t *gp, SDL_JoystickID id);
void mvn_gamepad_on_removed(mvn_gamepad_state_t *gp, SDL_JoystickID id);

/* Queries */
bool        mvn_gamepad_btn(mvn_gamepad_state_t *gp, mvn_pad_btn_t b, int32_t index);
bool        mvn_gamepad_btnp(mvn_gamepad_state_t *gp, mvn_pad_btn_t b, int32_t index);
float       mvn_gamepad_axis(mvn_gamepad_state_t *gp, mvn_pad_axis_t a, int32_t index);
bool        mvn_gamepad_connected(mvn_gamepad_state_t *gp, int32_t index);
int32_t     mvn_gamepad_count(mvn_gamepad_state_t *gp);
const char *mvn_gamepad_name(mvn_gamepad_state_t *gp, int32_t index);
void        mvn_gamepad_set_deadzone(mvn_gamepad_state_t *gp, float val, int32_t index);
float       mvn_gamepad_get_deadzone(mvn_gamepad_state_t *gp, int32_t index);

/* Rumble */
void mvn_gamepad_rumble(mvn_gamepad_state_t *gp,
                        int32_t              index,
                        uint16_t             low,
                        uint16_t             high,
                        uint32_t             duration_ms);
void mvn_gamepad_rumble_triggers(mvn_gamepad_state_t *gp,
                                 int32_t              index,
                                 uint16_t             left,
                                 uint16_t             right,
                                 uint32_t             duration_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_GAMEPAD_H */
