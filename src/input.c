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

static const char *KEY_NAMES[MVN_KEY_COUNT] = {
    "NONE",  "UP",     "DOWN",   "LEFT",   "RIGHT", "Z",   "X",   "C",   "V",  "SPACE",
    "ENTER", "ESCAPE", "LSHIFT", "RSHIFT", "A",     "B",   "D",   "E",   "F",  "G",
    "H",     "I",      "J",      "K",      "L",     "M",   "N",   "O",   "P",  "Q",
    "R",     "S",      "T",      "U",      "W",     "Y",   "0",   "1",   "2",  "3",
    "4",     "5",      "6",      "7",      "8",     "9",   "F1",  "F2",  "F3", "F4",
    "F5",    "F6",     "F7",     "F8",     "F9",    "F10", "F11", "F12",
};

/* ------------------------------------------------------------------ */
/*  Scancode mapping                                                   */
/* ------------------------------------------------------------------ */

mvn_key_t mvn_key_from_scancode(SDL_Scancode sc)
{
    switch (sc) {
        case SDL_SCANCODE_UP:
            return MVN_KEY_UP;
        case SDL_SCANCODE_DOWN:
            return MVN_KEY_DOWN;
        case SDL_SCANCODE_LEFT:
            return MVN_KEY_LEFT;
        case SDL_SCANCODE_RIGHT:
            return MVN_KEY_RIGHT;
        case SDL_SCANCODE_Z:
            return MVN_KEY_Z;
        case SDL_SCANCODE_X:
            return MVN_KEY_X;
        case SDL_SCANCODE_C:
            return MVN_KEY_C;
        case SDL_SCANCODE_V:
            return MVN_KEY_V;
        case SDL_SCANCODE_SPACE:
            return MVN_KEY_SPACE;
        case SDL_SCANCODE_RETURN:
            return MVN_KEY_ENTER;
        case SDL_SCANCODE_ESCAPE:
            return MVN_KEY_ESCAPE;
        case SDL_SCANCODE_LSHIFT:
            return MVN_KEY_LSHIFT;
        case SDL_SCANCODE_RSHIFT:
            return MVN_KEY_RSHIFT;
        case SDL_SCANCODE_A:
            return MVN_KEY_A;
        case SDL_SCANCODE_B:
            return MVN_KEY_B;
        case SDL_SCANCODE_D:
            return MVN_KEY_D;
        case SDL_SCANCODE_E:
            return MVN_KEY_E;
        case SDL_SCANCODE_F:
            return MVN_KEY_F;
        case SDL_SCANCODE_G:
            return MVN_KEY_G;
        case SDL_SCANCODE_H:
            return MVN_KEY_H;
        case SDL_SCANCODE_I:
            return MVN_KEY_I;
        case SDL_SCANCODE_J:
            return MVN_KEY_J;
        case SDL_SCANCODE_K:
            return MVN_KEY_K;
        case SDL_SCANCODE_L:
            return MVN_KEY_L;
        case SDL_SCANCODE_M:
            return MVN_KEY_M;
        case SDL_SCANCODE_N:
            return MVN_KEY_N;
        case SDL_SCANCODE_O:
            return MVN_KEY_O;
        case SDL_SCANCODE_P:
            return MVN_KEY_P;
        case SDL_SCANCODE_Q:
            return MVN_KEY_Q;
        case SDL_SCANCODE_R:
            return MVN_KEY_R;
        case SDL_SCANCODE_S:
            return MVN_KEY_S;
        case SDL_SCANCODE_T:
            return MVN_KEY_T;
        case SDL_SCANCODE_U:
            return MVN_KEY_U;
        case SDL_SCANCODE_W:
            return MVN_KEY_W;
        case SDL_SCANCODE_Y:
            return MVN_KEY_Y;
        case SDL_SCANCODE_0:
            return MVN_KEY_0;
        case SDL_SCANCODE_1:
            return MVN_KEY_1;
        case SDL_SCANCODE_2:
            return MVN_KEY_2;
        case SDL_SCANCODE_3:
            return MVN_KEY_3;
        case SDL_SCANCODE_4:
            return MVN_KEY_4;
        case SDL_SCANCODE_5:
            return MVN_KEY_5;
        case SDL_SCANCODE_6:
            return MVN_KEY_6;
        case SDL_SCANCODE_7:
            return MVN_KEY_7;
        case SDL_SCANCODE_8:
            return MVN_KEY_8;
        case SDL_SCANCODE_9:
            return MVN_KEY_9;
        case SDL_SCANCODE_F1:
            return MVN_KEY_F1;
        case SDL_SCANCODE_F2:
            return MVN_KEY_F2;
        case SDL_SCANCODE_F3:
            return MVN_KEY_F3;
        case SDL_SCANCODE_F4:
            return MVN_KEY_F4;
        case SDL_SCANCODE_F5:
            return MVN_KEY_F5;
        case SDL_SCANCODE_F6:
            return MVN_KEY_F6;
        case SDL_SCANCODE_F7:
            return MVN_KEY_F7;
        case SDL_SCANCODE_F8:
            return MVN_KEY_F8;
        case SDL_SCANCODE_F9:
            return MVN_KEY_F9;
        case SDL_SCANCODE_F10:
            return MVN_KEY_F10;
        case SDL_SCANCODE_F11:
            return MVN_KEY_F11;
        case SDL_SCANCODE_F12:
            return MVN_KEY_F12;
        default:
            return MVN_KEY_NONE;
    }
}

/* ------------------------------------------------------------------ */
/*  Keyboard state                                                     */
/* ------------------------------------------------------------------ */

mvn_key_state_t *mvn_key_create(void)
{
    mvn_key_state_t *keys;

    keys = MVN_CALLOC(1, sizeof(mvn_key_state_t));
    return keys;
}

void mvn_key_destroy(mvn_key_state_t *keys)
{
    MVN_FREE(keys);
}

void mvn_key_update(mvn_key_state_t *keys)
{
    SDL_memcpy(keys->previous, keys->current, sizeof(keys->current));
}

void mvn_key_set(mvn_key_state_t *keys, mvn_key_t key, bool down)
{
    if (key > MVN_KEY_NONE && key < MVN_KEY_COUNT) {
        keys->current[key] = down;
    }
}

bool mvn_key_btn(mvn_key_state_t *keys, mvn_key_t key)
{
    if (key > MVN_KEY_NONE && key < MVN_KEY_COUNT) {
        return keys->current[key];
    }
    return false;
}

bool mvn_key_btnp(mvn_key_state_t *keys, mvn_key_t key)
{
    if (key > MVN_KEY_NONE && key < MVN_KEY_COUNT) {
        return keys->current[key] && !keys->previous[key];
    }
    return false;
}

const char *mvn_key_name(mvn_key_t key)
{
    if (key >= 0 && key < MVN_KEY_COUNT) {
        return KEY_NAMES[key];
    }
    return "NONE";
}

/* ------------------------------------------------------------------ */
/*  Virtual input mapping                                              */
/* ------------------------------------------------------------------ */

mvn_input_state_t *mvn_input_create(void)
{
    mvn_input_state_t *inp;

    inp = MVN_CALLOC(1, sizeof(mvn_input_state_t));
    return inp;
}

void mvn_input_destroy(mvn_input_state_t *inp)
{
    MVN_FREE(inp);
}

/**
 * \brief           Find an action by name, or return NULL
 */
static mvn_input_action_t *prv_find_action(mvn_input_state_t *inp, const char *action)
{
    for (int32_t idx = 0; idx < inp->action_count; ++idx) {
        if (SDL_strcmp(inp->actions[idx].name, action) == 0) {
            return &inp->actions[idx];
        }
    }
    return NULL;
}

void mvn_input_map(mvn_input_state_t *  inp,
                   const char *         action,
                   const mvn_binding_t *bindings,
                   int32_t              count)
{
    mvn_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act == NULL) {
        if (inp->action_count >= MVN_INPUT_MAX_ACTIONS) {
            return;
        }
        act = &inp->actions[inp->action_count];
        ++inp->action_count;
        SDL_strlcpy(act->name, action, MVN_INPUT_ACTION_LEN);
    }

    act->bind_count = (count > MVN_INPUT_MAX_BINDINGS) ? MVN_INPUT_MAX_BINDINGS : count;
    SDL_memcpy(act->bindings, bindings, (size_t)act->bind_count * sizeof(mvn_binding_t));
}

void mvn_input_remap(mvn_input_state_t *  inp,
                     const char *         action,
                     const mvn_binding_t *bindings,
                     int32_t              count)
{
    mvn_input_map(inp, action, bindings, count);
}

void mvn_input_clear_action(mvn_input_state_t *inp, const char *action)
{
    mvn_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        act->bind_count = 0;
    }
}

void mvn_input_clear_all(mvn_input_state_t *inp)
{
    inp->action_count = 0;
}

void mvn_input_update(mvn_input_state_t *inp, mvn_key_state_t *keys, mvn_gamepad_state_t *pads)
{
    for (int32_t idx = 0; idx < inp->action_count; ++idx) {
        mvn_input_action_t *act;
        bool                pressed;
        float               axis_val;

        act           = &inp->actions[idx];
        act->previous = act->current;
        pressed       = false;
        axis_val      = 0.0f;

        for (int32_t bnd = 0; bnd < act->bind_count; ++bnd) {
            mvn_binding_t *bind;

            bind = &act->bindings[bnd];
            switch (bind->type) {
                case MVN_BIND_KEY:
                    if (mvn_key_btn(keys, (mvn_key_t)bind->code)) {
                        pressed = true;
                    }
                    break;

                case MVN_BIND_PAD_BTN:
                    if (mvn_gamepad_btn(pads, (mvn_pad_btn_t)bind->code, 0)) {
                        pressed = true;
                    }
                    break;

                case MVN_BIND_PAD_AXIS: {
                    float val;

                    val = mvn_gamepad_axis(pads, (mvn_pad_axis_t)bind->code, 0);
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

bool mvn_input_btn(mvn_input_state_t *inp, const char *action)
{
    mvn_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        return act->current;
    }
    return false;
}

bool mvn_input_btnp(mvn_input_state_t *inp, const char *action)
{
    mvn_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        return act->current && !act->previous;
    }
    return false;
}

float mvn_input_axis(mvn_input_state_t *inp, const char *action)
{
    mvn_input_action_t *act;

    act = prv_find_action(inp, action);
    if (act != NULL) {
        return act->axis_value;
    }
    return 0.0f;
}

/* ------------------------------------------------------------------ */
/*  Binding string resolver                                             */
/* ------------------------------------------------------------------ */

bool mvn_input_parse_binding(const char *str, mvn_binding_t *out)
{
    /* KEY_* → key binding */
    if (SDL_strncmp(str, "KEY_", 4) == 0) {
        const char *name;

        name = str + 4;
        for (int32_t idx = 0; idx < MVN_KEY_COUNT; ++idx) {
            if (SDL_strcmp(KEY_NAMES[idx], name) == 0) {
                out->type      = MVN_BIND_KEY;
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
            out->type = MVN_BIND_PAD_AXIS;
            out->code = MVN_PAD_AXIS_LX;
        } else if (SDL_strcmp(name, "LY") == 0) {
            out->type = MVN_BIND_PAD_AXIS;
            out->code = MVN_PAD_AXIS_LY;
        } else if (SDL_strcmp(name, "RX") == 0) {
            out->type = MVN_BIND_PAD_AXIS;
            out->code = MVN_PAD_AXIS_RX;
        } else if (SDL_strcmp(name, "RY") == 0) {
            out->type = MVN_BIND_PAD_AXIS;
            out->code = MVN_PAD_AXIS_RY;
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
        struct { const char *nm; mvn_pad_btn_t btn; } pad_map[] = {
            { "UP",     MVN_PAD_UP     }, { "DOWN",   MVN_PAD_DOWN   },
            { "LEFT",   MVN_PAD_LEFT   }, { "RIGHT",  MVN_PAD_RIGHT  },
            { "A",      MVN_PAD_A      }, { "B",      MVN_PAD_B      },
            { "X",      MVN_PAD_X      }, { "Y",      MVN_PAD_Y      },
            { "L1",     MVN_PAD_L1     }, { "R1",     MVN_PAD_R1     },
            { "L2",     MVN_PAD_L2     }, { "R2",     MVN_PAD_R2     },
            { "L3",     MVN_PAD_L3     }, { "R3",     MVN_PAD_R3     },
            { "START",  MVN_PAD_START  }, { "SELECT", MVN_PAD_SELECT },
            { "GUIDE",  MVN_PAD_GUIDE  },
        };
        /* clang-format on */

        for (size_t idx = 0; idx < sizeof(pad_map) / sizeof(pad_map[0]); ++idx) {
            if (SDL_strcmp(pad_map[idx].nm, name) == 0) {
                out->type      = MVN_BIND_PAD_BTN;
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
            out->code = MVN_MOUSE_LEFT;
        } else if (SDL_strcmp(name, "MIDDLE") == 0) {
            out->code = MVN_MOUSE_MIDDLE;
        } else if (SDL_strcmp(name, "RIGHT") == 0) {
            out->code = MVN_MOUSE_RIGHT;
        } else {
            return false;
        }
        out->type      = MVN_BIND_MOUSE_BTN;
        out->threshold = 0.0f;
        return true;
    }

    return false;
}
