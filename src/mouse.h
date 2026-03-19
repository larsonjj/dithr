/**
 * \file            mouse.h
 * \brief           Mouse state — position, buttons, wheel, visibility
 */

#ifndef DTR_MOUSE_H
#define DTR_MOUSE_H

#include "console.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Mouse state (positions are in framebuffer coordinates)
 */
struct dtr_mouse_state {
    float x;
    float y;
    float dx;
    float dy;
    float wheel_x;
    float wheel;

    bool btn_current[DTR_MOUSE_BTN_COUNT];
    bool btn_previous[DTR_MOUSE_BTN_COUNT];
    bool btn_pressed[DTR_MOUSE_BTN_COUNT];

    bool visible;

    /* Window → framebuffer mapping */
    float scale_x;
    float scale_y;
    float offset_x;
    float offset_y;
};

dtr_mouse_state_t *dtr_mouse_create(void);
void               dtr_mouse_destroy(dtr_mouse_state_t *mouse);
void               dtr_mouse_update(dtr_mouse_state_t *mouse);
void               dtr_mouse_set_mapping(dtr_mouse_state_t *mouse,
                                         float              scale_x,
                                         float              scale_y,
                                         float              offset_x,
                                         float              offset_y);
bool               dtr_mouse_btn(dtr_mouse_state_t *mouse, dtr_mouse_btn_t b);
bool               dtr_mouse_btnp(dtr_mouse_state_t *mouse, dtr_mouse_btn_t b);
void               dtr_mouse_show(dtr_mouse_state_t *mouse);
void               dtr_mouse_hide(dtr_mouse_state_t *mouse);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_MOUSE_H */
