/**
 * \file            mouse.c
 * \brief           Mouse state — position, buttons, wheel, visibility
 */

#include "mouse.h"

mvn_mouse_state_t *mvn_mouse_create(void)
{
    mvn_mouse_state_t *mouse;

    mouse = MVN_CALLOC(1, sizeof(mvn_mouse_state_t));
    if (mouse == NULL) {
        return NULL;
    }

    mouse->visible = true;
    mouse->scale_x = 1.0f;
    mouse->scale_y = 1.0f;

    return mouse;
}

void mvn_mouse_destroy(mvn_mouse_state_t *mouse)
{
    MVN_FREE(mouse);
}

void mvn_mouse_update(mvn_mouse_state_t *mouse)
{
    SDL_memcpy(mouse->btn_previous, mouse->btn_current, sizeof(mouse->btn_current));
}

void mvn_mouse_set_mapping(mvn_mouse_state_t *mouse,
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

bool mvn_mouse_btn(mvn_mouse_state_t *mouse, mvn_mouse_btn_t b)
{
    if (b >= 0 && b < MVN_MOUSE_BTN_COUNT) {
        return mouse->btn_current[b];
    }
    return false;
}

bool mvn_mouse_btnp(mvn_mouse_state_t *mouse, mvn_mouse_btn_t b)
{
    if (b >= 0 && b < MVN_MOUSE_BTN_COUNT) {
        return mouse->btn_current[b] && !mouse->btn_previous[b];
    }
    return false;
}

void mvn_mouse_show(mvn_mouse_state_t *mouse)
{
    mouse->visible = true;
    SDL_ShowCursor();
}

void mvn_mouse_hide(mvn_mouse_state_t *mouse)
{
    mouse->visible = false;
    SDL_HideCursor();
}
