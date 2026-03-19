/**
 * \file            mouse.c
 * \brief           Mouse state — position, buttons, wheel, visibility
 */

#include "mouse.h"

dtr_mouse_state_t *dtr_mouse_create(void)
{
    dtr_mouse_state_t *mouse;

    mouse = DTR_CALLOC(1, sizeof(dtr_mouse_state_t));
    if (mouse == NULL) {
        return NULL;
    }

    mouse->visible = true;
    mouse->scale_x = 1.0f;
    mouse->scale_y = 1.0f;

    return mouse;
}

void dtr_mouse_destroy(dtr_mouse_state_t *mouse)
{
    DTR_FREE(mouse);
}

void dtr_mouse_update(dtr_mouse_state_t *mouse)
{
    SDL_memcpy(mouse->btn_previous, mouse->btn_current, sizeof(mouse->btn_current));
    SDL_memset(mouse->btn_pressed, 0, sizeof(mouse->btn_pressed));
}

void dtr_mouse_set_mapping(dtr_mouse_state_t *mouse,
                           float              scale_x,
                           float              scale_y,
                           float              offset_x,
                           float              offset_y)
{
    mouse->scale_x  = scale_x;
    mouse->scale_y  = scale_y;
    mouse->offset_x = offset_x;
    mouse->offset_y = offset_y;
}

bool dtr_mouse_btn(dtr_mouse_state_t *mouse, dtr_mouse_btn_t b)
{
    if (b >= 0 && b < DTR_MOUSE_BTN_COUNT) {
        return mouse->btn_current[b];
    }
    return false;
}

bool dtr_mouse_btnp(dtr_mouse_state_t *mouse, dtr_mouse_btn_t b)
{
    if (b >= 0 && b < DTR_MOUSE_BTN_COUNT) {
        return mouse->btn_pressed[b];
    }
    return false;
}

void dtr_mouse_show(dtr_mouse_state_t *mouse)
{
    mouse->visible = true;
    SDL_ShowCursor();
}

void dtr_mouse_hide(dtr_mouse_state_t *mouse)
{
    mouse->visible = false;
    SDL_HideCursor();
}
