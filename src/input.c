/**
 * \file            input.c
 * \brief           Keyboard state and virtual input mapping
 */

#include "input.h"

#include "gamepad.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/*  Key name table                                                     */
/* ------------------------------------------------------------------ */

static const char *KEY_NAMES[DTR_KEY_COUNT] = {
    "NONE",   "UP",       "DOWN",     "LEFT",   "RIGHT",     "Z",      "X",     "C",     "V",
    "SPACE",  "ENTER",    "ESCAPE",   "LSHIFT", "RSHIFT",    "A",      "B",     "D",     "E",
    "F",      "G",        "H",        "I",      "J",         "K",      "L",     "M",     "N",
    "O",      "P",        "Q",        "R",      "S",         "T",      "U",     "W",     "Y",
    "0",      "1",        "2",        "3",      "4",         "5",      "6",     "7",     "8",
    "9",      "F1",       "F2",       "F3",     "F4",        "F5",     "F6",    "F7",    "F8",
    "F9",     "F10",      "F11",      "F12",    "BACKSPACE", "DELETE", "TAB",   "HOME",  "END",
    "PAGEUP", "PAGEDOWN", "LCTRL",    "RCTRL",  "LALT",      "RALT",   "GRAVE", "SLASH", "LGUI",
    "RGUI",   "LBRACKET", "RBRACKET",
};

/* ------------------------------------------------------------------ */
/*  Scancode mapping                                                   */
/* ------------------------------------------------------------------ */

dtr_key_t dtr_key_from_scancode(SDL_Scancode sc)
{
    switch (sc) {
        case SDL_SCANCODE_UP:
            return DTR_KEY_UP;
        case SDL_SCANCODE_DOWN:
            return DTR_KEY_DOWN;
        case SDL_SCANCODE_LEFT:
            return DTR_KEY_LEFT;
        case SDL_SCANCODE_RIGHT:
            return DTR_KEY_RIGHT;
        case SDL_SCANCODE_Z:
            return DTR_KEY_Z;
        case SDL_SCANCODE_X:
            return DTR_KEY_X;
        case SDL_SCANCODE_C:
            return DTR_KEY_C;
        case SDL_SCANCODE_V:
            return DTR_KEY_V;
        case SDL_SCANCODE_SPACE:
            return DTR_KEY_SPACE;
        case SDL_SCANCODE_RETURN:
            return DTR_KEY_ENTER;
        case SDL_SCANCODE_ESCAPE:
            return DTR_KEY_ESCAPE;
        case SDL_SCANCODE_LSHIFT:
            return DTR_KEY_LSHIFT;
        case SDL_SCANCODE_RSHIFT:
            return DTR_KEY_RSHIFT;
        case SDL_SCANCODE_A:
            return DTR_KEY_A;
        case SDL_SCANCODE_B:
            return DTR_KEY_B;
        case SDL_SCANCODE_D:
            return DTR_KEY_D;
        case SDL_SCANCODE_E:
            return DTR_KEY_E;
        case SDL_SCANCODE_F:
            return DTR_KEY_F;
        case SDL_SCANCODE_G:
            return DTR_KEY_G;
        case SDL_SCANCODE_H:
            return DTR_KEY_H;
        case SDL_SCANCODE_I:
            return DTR_KEY_I;
        case SDL_SCANCODE_J:
            return DTR_KEY_J;
        case SDL_SCANCODE_K:
            return DTR_KEY_K;
        case SDL_SCANCODE_L:
            return DTR_KEY_L;
        case SDL_SCANCODE_M:
            return DTR_KEY_M;
        case SDL_SCANCODE_N:
            return DTR_KEY_N;
        case SDL_SCANCODE_O:
            return DTR_KEY_O;
        case SDL_SCANCODE_P:
            return DTR_KEY_P;
        case SDL_SCANCODE_Q:
            return DTR_KEY_Q;
        case SDL_SCANCODE_R:
            return DTR_KEY_R;
        case SDL_SCANCODE_S:
            return DTR_KEY_S;
        case SDL_SCANCODE_T:
            return DTR_KEY_T;
        case SDL_SCANCODE_U:
            return DTR_KEY_U;
        case SDL_SCANCODE_W:
            return DTR_KEY_W;
        case SDL_SCANCODE_Y:
            return DTR_KEY_Y;
        case SDL_SCANCODE_0:
            return DTR_KEY_0;
        case SDL_SCANCODE_1:
            return DTR_KEY_1;
        case SDL_SCANCODE_2:
            return DTR_KEY_2;
        case SDL_SCANCODE_3:
            return DTR_KEY_3;
        case SDL_SCANCODE_4:
            return DTR_KEY_4;
        case SDL_SCANCODE_5:
            return DTR_KEY_5;
        case SDL_SCANCODE_6:
            return DTR_KEY_6;
        case SDL_SCANCODE_7:
            return DTR_KEY_7;
        case SDL_SCANCODE_8:
            return DTR_KEY_8;
        case SDL_SCANCODE_9:
            return DTR_KEY_9;
        case SDL_SCANCODE_F1:
            return DTR_KEY_F1;
        case SDL_SCANCODE_F2:
            return DTR_KEY_F2;
        case SDL_SCANCODE_F3:
            return DTR_KEY_F3;
        case SDL_SCANCODE_F4:
            return DTR_KEY_F4;
        case SDL_SCANCODE_F5:
            return DTR_KEY_F5;
        case SDL_SCANCODE_F6:
            return DTR_KEY_F6;
        case SDL_SCANCODE_F7:
            return DTR_KEY_F7;
        case SDL_SCANCODE_F8:
            return DTR_KEY_F8;
        case SDL_SCANCODE_F9:
            return DTR_KEY_F9;
        case SDL_SCANCODE_F10:
            return DTR_KEY_F10;
        case SDL_SCANCODE_F11:
            return DTR_KEY_F11;
        case SDL_SCANCODE_F12:
            return DTR_KEY_F12;
        case SDL_SCANCODE_BACKSPACE:
            return DTR_KEY_BACKSPACE;
        case SDL_SCANCODE_DELETE:
            return DTR_KEY_DELETE;
        case SDL_SCANCODE_TAB:
            return DTR_KEY_TAB;
        case SDL_SCANCODE_HOME:
            return DTR_KEY_HOME;
        case SDL_SCANCODE_END:
            return DTR_KEY_END;
        case SDL_SCANCODE_PAGEUP:
            return DTR_KEY_PAGEUP;
        case SDL_SCANCODE_PAGEDOWN:
            return DTR_KEY_PAGEDOWN;
        case SDL_SCANCODE_LCTRL:
            return DTR_KEY_LCTRL;
        case SDL_SCANCODE_RCTRL:
            return DTR_KEY_RCTRL;
        case SDL_SCANCODE_LALT:
            return DTR_KEY_LALT;
        case SDL_SCANCODE_RALT:
            return DTR_KEY_RALT;
        case SDL_SCANCODE_GRAVE:
            return DTR_KEY_GRAVE;
        case SDL_SCANCODE_SLASH:
            return DTR_KEY_SLASH;
        case SDL_SCANCODE_LGUI:
            return DTR_KEY_LGUI;
        case SDL_SCANCODE_RGUI:
            return DTR_KEY_RGUI;
        case SDL_SCANCODE_LEFTBRACKET:
            return DTR_KEY_LBRACKET;
        case SDL_SCANCODE_RIGHTBRACKET:
            return DTR_KEY_RBRACKET;
        default:
            return DTR_KEY_NONE;
    }
}

/* ------------------------------------------------------------------ */
/*  Keyboard state                                                     */
/* ------------------------------------------------------------------ */

dtr_key_state_t *dtr_key_create(void)
{
    dtr_key_state_t *keys;

    keys = DTR_CALLOC(1, sizeof(dtr_key_state_t));
    return keys;
}

void dtr_key_destroy(dtr_key_state_t *keys)
{
    DTR_FREE(keys);
}

void dtr_key_update(dtr_key_state_t *keys, float delta)
{
    SDL_memcpy(keys->previous, keys->current, sizeof(keys->current));
    SDL_memset(keys->repeat_fired, 0, sizeof(keys->repeat_fired));

    for (int32_t idx = 1; idx < DTR_KEY_COUNT; ++idx) {
        if (keys->current[idx]) {
            float prev_time;

            prev_time = keys->hold_time[idx];
            keys->hold_time[idx] += delta;

            if (prev_time >= DTR_KEY_REPEAT_DELAY) {
                /* Already past initial delay — check repeat interval */
                float prev_rep;
                float curr_rep;

                prev_rep = prev_time - DTR_KEY_REPEAT_DELAY;
                curr_rep = keys->hold_time[idx] - DTR_KEY_REPEAT_DELAY;

                if ((int)(curr_rep / DTR_KEY_REPEAT_INTERVAL) >
                    (int)(prev_rep / DTR_KEY_REPEAT_INTERVAL)) {
                    keys->repeat_fired[idx] = true;
                }
            } else if (keys->hold_time[idx] >= DTR_KEY_REPEAT_DELAY) {
                /* Just crossed the initial delay threshold */
                keys->repeat_fired[idx] = true;
            }
        } else {
            keys->hold_time[idx] = 0.0f;
        }
    }
}

void dtr_key_set(dtr_key_state_t *keys, dtr_key_t key, bool down)
{
    if (key > DTR_KEY_NONE && key < DTR_KEY_COUNT) {
        keys->current[key] = down;
    }
}

bool dtr_key_btn(dtr_key_state_t *keys, dtr_key_t key)
{
    if (key > DTR_KEY_NONE && key < DTR_KEY_COUNT) {
        return keys->current[key];
    }
    return false;
}

bool dtr_key_btnp(dtr_key_state_t *keys, dtr_key_t key)
{
    if (key > DTR_KEY_NONE && key < DTR_KEY_COUNT) {
        return keys->current[key] && !keys->previous[key];
    }
    return false;
}

bool dtr_key_btnr(dtr_key_state_t *keys, dtr_key_t key)
{
    if (key > DTR_KEY_NONE && key < DTR_KEY_COUNT) {
        if (keys->current[key] && !keys->previous[key]) {
            return true; /* initial press */
        }
        return keys->repeat_fired[key];
    }
    return false;
}

const char *dtr_key_name(dtr_key_t key)
{
    if (key >= 0 && key < DTR_KEY_COUNT) {
        return KEY_NAMES[key];
    }
    return "NONE";
}

/* ------------------------------------------------------------------ */
/*  Virtual input mapping                                              */
/* ------------------------------------------------------------------ */

dtr_input_state_t *dtr_input_create(void)
{
    dtr_input_state_t *inp;

    inp = DTR_CALLOC(1, sizeof(dtr_input_state_t));
    return inp;
}

void dtr_input_destroy(dtr_input_state_t *inp)
{
    DTR_FREE(inp);
}

/**
 * \brief           Find an action by name, or return NULL
 */
static dtr_input_action_t *prv_find_action(dtr_input_state_t *inp, const char *action)
{
    for (int32_t idx = 0; idx < inp->action_count; ++idx) {
        if (SDL_strcmp(inp->actions[idx].name, action) == 0) {
            return &inp->actions[idx];
        }
    }
    return NULL;
}

void dtr_input_map(dtr_input_state_t   *inp,
                   const char          *action,
                   const dtr_binding_t *bindings,
                   int32_t              count)
{
    dtr_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act == NULL) {
        if (inp->action_count >= DTR_INPUT_MAX_ACTIONS) {
            return;
        }
        act = &inp->actions[inp->action_count];
        ++inp->action_count;
        SDL_strlcpy(act->name, action, DTR_INPUT_ACTION_LEN);
    }

    act->bind_count = (count > DTR_INPUT_MAX_BINDINGS) ? DTR_INPUT_MAX_BINDINGS : count;
    SDL_memcpy(act->bindings, bindings, (size_t)act->bind_count * sizeof(dtr_binding_t));
}

void dtr_input_remap(dtr_input_state_t   *inp,
                     const char          *action,
                     const dtr_binding_t *bindings,
                     int32_t              count)
{
    dtr_input_map(inp, action, bindings, count);
}

void dtr_input_clear_action(dtr_input_state_t *inp, const char *action)
{
    dtr_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        act->bind_count = 0;
    }
}

void dtr_input_clear_all(dtr_input_state_t *inp)
{
    inp->action_count = 0;
}

void dtr_input_update(dtr_input_state_t *inp, dtr_key_state_t *keys, dtr_gamepad_state_t *pads)
{
    for (int32_t idx = 0; idx < inp->action_count; ++idx) {
        dtr_input_action_t *act;
        bool                pressed;
        float               axis_val;

        act           = &inp->actions[idx];
        act->previous = act->current;
        pressed       = false;
        axis_val      = 0.0f;

        for (int32_t bnd = 0; bnd < act->bind_count; ++bnd) {
            dtr_binding_t *bind;

            bind = &act->bindings[bnd];
            switch (bind->type) {
                case DTR_BIND_KEY:
                    if (dtr_key_btn(keys, (dtr_key_t)bind->code)) {
                        pressed = true;
                    }
                    break;

                case DTR_BIND_PAD_BTN:
                    if (pads != NULL && dtr_gamepad_btn(pads, (dtr_pad_btn_t)bind->code, 0)) {
                        pressed = true;
                    }
                    break;

                case DTR_BIND_PAD_AXIS: {
                    float val;

                    if (pads == NULL)
                        break;
                    val = dtr_gamepad_axis(pads, (dtr_pad_axis_t)bind->code, 0);
                    if (fabsf(val) > bind->threshold) {
                        axis_val = val;
                        pressed  = true;
                    }
                    break;
                }

                default:
                    break;
            }
        }

        act->current    = pressed;
        act->axis_value = axis_val;
    }
}

bool dtr_input_btn(dtr_input_state_t *inp, const char *action)
{
    dtr_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        return act->current;
    }
    return false;
}

bool dtr_input_btnp(dtr_input_state_t *inp, const char *action)
{
    dtr_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        return act->current && !act->previous;
    }
    return false;
}

float dtr_input_axis(dtr_input_state_t *inp, const char *action)
{
    dtr_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        return act->axis_value;
    }
    return 0.0f;
}

/* ------------------------------------------------------------------ */
/*  Binding string resolver                                             */
/* ------------------------------------------------------------------ */

bool dtr_input_parse_binding(const char *str, dtr_binding_t *out)
{
    /* KEY_* → key binding */
    if (SDL_strncmp(str, "KEY_", 4) == 0) {
        const char *name;

        name = str + 4;
        for (int32_t idx = 0; idx < DTR_KEY_COUNT; ++idx) {
            if (SDL_strcmp(KEY_NAMES[idx], name) == 0) {
                out->type      = DTR_BIND_KEY;
                out->code      = idx;
                out->threshold = 0.0f;
                return true;
            }
        }
        return false;
    }

    /* PAD_AXIS_* → axis binding */
    if (SDL_strncmp(str, "PAD_AXIS_", 9) == 0) {
        const char *name;

        name = str + 9;
        if (SDL_strcmp(name, "LX") == 0) {
            out->type = DTR_BIND_PAD_AXIS;
            out->code = DTR_PAD_AXIS_LX;
        } else if (SDL_strcmp(name, "LY") == 0) {
            out->type = DTR_BIND_PAD_AXIS;
            out->code = DTR_PAD_AXIS_LY;
        } else if (SDL_strcmp(name, "RX") == 0) {
            out->type = DTR_BIND_PAD_AXIS;
            out->code = DTR_PAD_AXIS_RX;
        } else if (SDL_strcmp(name, "RY") == 0) {
            out->type = DTR_BIND_PAD_AXIS;
            out->code = DTR_PAD_AXIS_RY;
        } else {
            return false;
        }
        out->threshold = 0.25f;
        return true;
    }

    /* PAD_* → pad button binding */
    if (SDL_strncmp(str, "PAD_", 4) == 0) {
        const char *name;

        name = str + 4;
        /* clang-format off */
        struct { const char *nm; dtr_pad_btn_t btn; } pad_map[] = {
            { "UP",     DTR_PAD_UP     }, { "DOWN",   DTR_PAD_DOWN   },
            { "LEFT",   DTR_PAD_LEFT   }, { "RIGHT",  DTR_PAD_RIGHT  },
            { "A",      DTR_PAD_A      }, { "B",      DTR_PAD_B      },
            { "X",      DTR_PAD_X      }, { "Y",      DTR_PAD_Y      },
            { "L1",     DTR_PAD_L1     }, { "R1",     DTR_PAD_R1     },
            { "L2",     DTR_PAD_L2     }, { "R2",     DTR_PAD_R2     },
            { "L3",     DTR_PAD_L3     }, { "R3",     DTR_PAD_R3     },
            { "START",  DTR_PAD_START  }, { "SELECT", DTR_PAD_SELECT },
            { "GUIDE",  DTR_PAD_GUIDE  },
        };
        /* clang-format on */

        for (size_t idx = 0; idx < sizeof(pad_map) / sizeof(pad_map[0]); ++idx) {
            if (SDL_strcmp(pad_map[idx].nm, name) == 0) {
                out->type      = DTR_BIND_PAD_BTN;
                out->code      = (int32_t)pad_map[idx].btn;
                out->threshold = 0.0f;
                return true;
            }
        }
        return false;
    }

    /* MOUSE_* → mouse button binding */
    if (SDL_strncmp(str, "MOUSE_", 6) == 0) {
        const char *name;

        name = str + 6;
        if (SDL_strcmp(name, "LEFT") == 0) {
            out->code = DTR_MOUSE_LEFT;
        } else if (SDL_strcmp(name, "MIDDLE") == 0) {
            out->code = DTR_MOUSE_MIDDLE;
        } else if (SDL_strcmp(name, "RIGHT") == 0) {
            out->code = DTR_MOUSE_RIGHT;
        } else {
            return false;
        }
        out->type      = DTR_BIND_MOUSE_BTN;
        out->threshold = 0.0f;
        return true;
    }

    return false;
}
