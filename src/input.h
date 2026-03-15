/**
 * \file            input.h
 * \brief           Virtual input mapping — action-based input abstraction
 */

#ifndef MVN_INPUT_H
#define MVN_INPUT_H

#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Key constants (matching SDL scancodes conceptually) */
typedef enum mvn_key {
    MVN_KEY_NONE = 0,
    MVN_KEY_UP,
    MVN_KEY_DOWN,
    MVN_KEY_LEFT,
    MVN_KEY_RIGHT,
    MVN_KEY_Z,
    MVN_KEY_X,
    MVN_KEY_C,
    MVN_KEY_V,
    MVN_KEY_SPACE,
    MVN_KEY_ENTER,
    MVN_KEY_ESCAPE,
    MVN_KEY_LSHIFT,
    MVN_KEY_RSHIFT,
    MVN_KEY_A,
    MVN_KEY_B,
    MVN_KEY_D,
    MVN_KEY_E,
    MVN_KEY_F,
    MVN_KEY_G,
    MVN_KEY_H,
    MVN_KEY_I,
    MVN_KEY_J,
    MVN_KEY_K,
    MVN_KEY_L,
    MVN_KEY_M,
    MVN_KEY_N,
    MVN_KEY_O,
    MVN_KEY_P,
    MVN_KEY_Q,
    MVN_KEY_R,
    MVN_KEY_S,
    MVN_KEY_T,
    MVN_KEY_U,
    MVN_KEY_W,
    MVN_KEY_Y,
    MVN_KEY_0,
    MVN_KEY_1,
    MVN_KEY_2,
    MVN_KEY_3,
    MVN_KEY_4,
    MVN_KEY_5,
    MVN_KEY_6,
    MVN_KEY_7,
    MVN_KEY_8,
    MVN_KEY_9,
    MVN_KEY_F1,
    MVN_KEY_F2,
    MVN_KEY_F3,
    MVN_KEY_F4,
    MVN_KEY_F5,
    MVN_KEY_F6,
    MVN_KEY_F7,
    MVN_KEY_F8,
    MVN_KEY_F9,
    MVN_KEY_F10,
    MVN_KEY_F11,
    MVN_KEY_F12,
    MVN_KEY_COUNT
} mvn_key_t;

/* Gamepad button constants */
typedef enum mvn_pad_btn {
    MVN_PAD_UP = 0,
    MVN_PAD_DOWN,
    MVN_PAD_LEFT,
    MVN_PAD_RIGHT,
    MVN_PAD_A,
    MVN_PAD_B,
    MVN_PAD_X,
    MVN_PAD_Y,
    MVN_PAD_L1,
    MVN_PAD_R1,
    MVN_PAD_L2,
    MVN_PAD_R2,
    MVN_PAD_L3,
    MVN_PAD_R3,
    MVN_PAD_START,
    MVN_PAD_SELECT,
    MVN_PAD_GUIDE,
    MVN_PAD_BTN_COUNT
} mvn_pad_btn_t;

/* Gamepad axis constants */
typedef enum mvn_pad_axis {
    MVN_PAD_AXIS_LX = 0,
    MVN_PAD_AXIS_LY,
    MVN_PAD_AXIS_RX,
    MVN_PAD_AXIS_RY,
    MVN_PAD_AXIS_L2,
    MVN_PAD_AXIS_R2,
    MVN_PAD_AXIS_COUNT
} mvn_pad_axis_t;

/* Mouse button constants */
typedef enum mvn_mouse_btn {
    MVN_MOUSE_LEFT = 0,
    MVN_MOUSE_MIDDLE,
    MVN_MOUSE_RIGHT,
    MVN_MOUSE_BTN_COUNT
} mvn_mouse_btn_t;

/* ------------------------------------------------------------------------ */
/*  Keyboard state                                                           */
/* ------------------------------------------------------------------------ */

typedef struct mvn_key_state {
    bool current[MVN_KEY_COUNT];
    bool previous[MVN_KEY_COUNT];
} mvn_key_state_t;

mvn_key_state_t *mvn_key_create(void);
void             mvn_key_destroy(mvn_key_state_t *keys);
void             mvn_key_update(mvn_key_state_t *keys);
void             mvn_key_set(mvn_key_state_t *keys, mvn_key_t key, bool down);
bool             mvn_key_btn(mvn_key_state_t *keys, mvn_key_t key);
bool             mvn_key_btnp(mvn_key_state_t *keys, mvn_key_t key);
const char *     mvn_key_name(mvn_key_t key);

/**
 * \brief           Map an SDL scancode to our mvn_key_t enum
 */
mvn_key_t mvn_key_from_scancode(SDL_Scancode sc);

/* ------------------------------------------------------------------------ */
/*  Virtual input mapping                                                    */
/* ------------------------------------------------------------------------ */

#define MVN_INPUT_MAX_ACTIONS  32
#define MVN_INPUT_MAX_BINDINGS 8
#define MVN_INPUT_ACTION_LEN   32

/* A single binding can reference a key, pad button, or pad axis */
typedef enum mvn_binding_type {
    MVN_BIND_KEY,
    MVN_BIND_PAD_BTN,
    MVN_BIND_PAD_AXIS,
    MVN_BIND_MOUSE_BTN,
} mvn_binding_type_t;

typedef struct mvn_binding {
    mvn_binding_type_t type;
    int32_t            code;      /**< mvn_key_t, mvn_pad_btn_t, or axis */
    float              threshold; /**< For axis bindings */
} mvn_binding_t;

typedef struct mvn_input_action {
    char          name[MVN_INPUT_ACTION_LEN];
    mvn_binding_t bindings[MVN_INPUT_MAX_BINDINGS];
    int32_t       bind_count;
    bool          current;
    bool          previous;
    float         axis_value;
} mvn_input_action_t;

typedef struct mvn_input_state {
    mvn_input_action_t actions[MVN_INPUT_MAX_ACTIONS];
    int32_t            action_count;
} mvn_input_state_t;

mvn_input_state_t *mvn_input_create(void);
void               mvn_input_destroy(mvn_input_state_t *inp);
void  mvn_input_update(mvn_input_state_t *inp, mvn_key_state_t *keys, mvn_gamepad_state_t *pads);
void  mvn_input_map(mvn_input_state_t *  inp,
                    const char *         action,
                    const mvn_binding_t *bindings,
                    int32_t              count);
void  mvn_input_remap(mvn_input_state_t *  inp,
                      const char *         action,
                      const mvn_binding_t *bindings,
                      int32_t              count);
void  mvn_input_clear_action(mvn_input_state_t *inp, const char *action);
void  mvn_input_clear_all(mvn_input_state_t *inp);
bool  mvn_input_btn(mvn_input_state_t *inp, const char *action);
bool  mvn_input_btnp(mvn_input_state_t *inp, const char *action);
float mvn_input_axis(mvn_input_state_t *inp, const char *action);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_INPUT_H */
