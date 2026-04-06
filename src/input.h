/**
 * \file            input.h
 * \brief           Virtual input mapping — action-based input abstraction
 */

#ifndef DTR_INPUT_H
#define DTR_INPUT_H

#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Key constants (matching SDL scancodes conceptually) */
typedef enum dtr_key {
    DTR_KEY_NONE = 0,
    DTR_KEY_UP,
    DTR_KEY_DOWN,
    DTR_KEY_LEFT,
    DTR_KEY_RIGHT,
    DTR_KEY_Z,
    DTR_KEY_X,
    DTR_KEY_C,
    DTR_KEY_V,
    DTR_KEY_SPACE,
    DTR_KEY_ENTER,
    DTR_KEY_ESCAPE,
    DTR_KEY_LSHIFT,
    DTR_KEY_RSHIFT,
    DTR_KEY_A,
    DTR_KEY_B,
    DTR_KEY_D,
    DTR_KEY_E,
    DTR_KEY_F,
    DTR_KEY_G,
    DTR_KEY_H,
    DTR_KEY_I,
    DTR_KEY_J,
    DTR_KEY_K,
    DTR_KEY_L,
    DTR_KEY_M,
    DTR_KEY_N,
    DTR_KEY_O,
    DTR_KEY_P,
    DTR_KEY_Q,
    DTR_KEY_R,
    DTR_KEY_S,
    DTR_KEY_T,
    DTR_KEY_U,
    DTR_KEY_W,
    DTR_KEY_Y,
    DTR_KEY_0,
    DTR_KEY_1,
    DTR_KEY_2,
    DTR_KEY_3,
    DTR_KEY_4,
    DTR_KEY_5,
    DTR_KEY_6,
    DTR_KEY_7,
    DTR_KEY_8,
    DTR_KEY_9,
    DTR_KEY_F1,
    DTR_KEY_F2,
    DTR_KEY_F3,
    DTR_KEY_F4,
    DTR_KEY_F5,
    DTR_KEY_F6,
    DTR_KEY_F7,
    DTR_KEY_F8,
    DTR_KEY_F9,
    DTR_KEY_F10,
    DTR_KEY_F11,
    DTR_KEY_F12,
    DTR_KEY_BACKSPACE,
    DTR_KEY_DELETE,
    DTR_KEY_TAB,
    DTR_KEY_HOME,
    DTR_KEY_END,
    DTR_KEY_PAGEUP,
    DTR_KEY_PAGEDOWN,
    DTR_KEY_LCTRL,
    DTR_KEY_RCTRL,
    DTR_KEY_LALT,
    DTR_KEY_RALT,
    DTR_KEY_GRAVE,
    DTR_KEY_SLASH,
    DTR_KEY_COUNT
} dtr_key_t;

/* Gamepad button constants */
typedef enum dtr_pad_btn {
    DTR_PAD_UP = 0,
    DTR_PAD_DOWN,
    DTR_PAD_LEFT,
    DTR_PAD_RIGHT,
    DTR_PAD_A,
    DTR_PAD_B,
    DTR_PAD_X,
    DTR_PAD_Y,
    DTR_PAD_L1,
    DTR_PAD_R1,
    DTR_PAD_L2,
    DTR_PAD_R2,
    DTR_PAD_L3,
    DTR_PAD_R3,
    DTR_PAD_START,
    DTR_PAD_SELECT,
    DTR_PAD_GUIDE,
    DTR_PAD_BTN_COUNT
} dtr_pad_btn_t;

/* Gamepad axis constants */
typedef enum dtr_pad_axis {
    DTR_PAD_AXIS_LX = 0,
    DTR_PAD_AXIS_LY,
    DTR_PAD_AXIS_RX,
    DTR_PAD_AXIS_RY,
    DTR_PAD_AXIS_L2,
    DTR_PAD_AXIS_R2,
    DTR_PAD_AXIS_COUNT
} dtr_pad_axis_t;

/* Mouse button constants */
typedef enum dtr_mouse_btn {
    DTR_MOUSE_LEFT = 0,
    DTR_MOUSE_MIDDLE,
    DTR_MOUSE_RIGHT,
    DTR_MOUSE_BTN_COUNT
} dtr_mouse_btn_t;

/* ------------------------------------------------------------------------ */
/*  Keyboard state                                                           */
/* ------------------------------------------------------------------------ */

/* Key repeat default timing (seconds) */
#define DTR_KEY_REPEAT_DELAY    0.35f
#define DTR_KEY_REPEAT_INTERVAL 0.04f

struct dtr_key_state {
    bool  current[DTR_KEY_COUNT];
    bool  previous[DTR_KEY_COUNT];
    float hold_time[DTR_KEY_COUNT];
    bool  repeat_fired[DTR_KEY_COUNT];
};

dtr_key_state_t *dtr_key_create(void);
void             dtr_key_destroy(dtr_key_state_t *keys);
void             dtr_key_update(dtr_key_state_t *keys, float delta);
void             dtr_key_set(dtr_key_state_t *keys, dtr_key_t key, bool down);
bool             dtr_key_btn(dtr_key_state_t *keys, dtr_key_t key);
bool             dtr_key_btnp(dtr_key_state_t *keys, dtr_key_t key);
bool             dtr_key_btnr(dtr_key_state_t *keys, dtr_key_t key);
const char      *dtr_key_name(dtr_key_t key);

/**
 * \brief           Map an SDL scancode to our dtr_key_t enum
 */
dtr_key_t dtr_key_from_scancode(SDL_Scancode sc);

/* ------------------------------------------------------------------------ */
/*  Virtual input mapping                                                    */
/* ------------------------------------------------------------------------ */

#define DTR_INPUT_MAX_ACTIONS  32
#define DTR_INPUT_MAX_BINDINGS 8
#define DTR_INPUT_ACTION_LEN   32

/* A single binding can reference a key, pad button, or pad axis */
typedef enum dtr_binding_type {
    DTR_BIND_KEY,
    DTR_BIND_PAD_BTN,
    DTR_BIND_PAD_AXIS,
    DTR_BIND_MOUSE_BTN,
} dtr_binding_type_t;

typedef struct dtr_binding {
    dtr_binding_type_t type;
    int32_t            code;      /**< dtr_key_t, dtr_pad_btn_t, or axis */
    float              threshold; /**< For axis bindings */
} dtr_binding_t;

typedef struct dtr_input_action {
    char          name[DTR_INPUT_ACTION_LEN];
    dtr_binding_t bindings[DTR_INPUT_MAX_BINDINGS];
    int32_t       bind_count;
    bool          current;
    bool          previous;
    float         axis_value;
} dtr_input_action_t;

struct dtr_input_state {
    dtr_input_action_t actions[DTR_INPUT_MAX_ACTIONS];
    int32_t            action_count;
};

dtr_input_state_t *dtr_input_create(void);
void               dtr_input_destroy(dtr_input_state_t *inp);
void  dtr_input_update(dtr_input_state_t *inp, dtr_key_state_t *keys, dtr_gamepad_state_t *pads);
void  dtr_input_map(dtr_input_state_t   *inp,
                    const char          *action,
                    const dtr_binding_t *bindings,
                    int32_t              count);
void  dtr_input_remap(dtr_input_state_t   *inp,
                      const char          *action,
                      const dtr_binding_t *bindings,
                      int32_t              count);
void  dtr_input_clear_action(dtr_input_state_t *inp, const char *action);
void  dtr_input_clear_all(dtr_input_state_t *inp);
bool  dtr_input_btn(dtr_input_state_t *inp, const char *action);
bool  dtr_input_btnp(dtr_input_state_t *inp, const char *action);
float dtr_input_axis(dtr_input_state_t *inp, const char *action);

/**
 * \brief           Parse a binding string like "KEY_LEFT", "PAD_A", "MOUSE_LEFT"
 * \return          true if parsed successfully
 */
bool dtr_input_parse_binding(const char *str, dtr_binding_t *out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_INPUT_H */
