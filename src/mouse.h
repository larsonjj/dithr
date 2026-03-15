/**
 * \file            mouse.h
 * \brief           Mouse state — position, buttons, wheel, visibility
 */

#ifndef MVN_MOUSE_H
#define MVN_MOUSE_H

#include "console.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Mouse state (positions are in framebuffer coordinates)
 */
typedef struct mvn_mouse_state {
    float x;
    float y;
    float dx;
    float dy;
    float wheel_x;
    float wheel;

    bool btn_current[MVN_MOUSE_BTN_COUNT];
    bool btn_previous[MVN_MOUSE_BTN_COUNT];

    bool visible;

    /* Window → framebuffer mapping */
    float scale_x;
    float scale_y;
    float offset_x;
    float offset_y;
} mvn_mouse_state_t;

mvn_mouse_state_t *mvn_mouse_create(void);
void               mvn_mouse_destroy(mvn_mouse_state_t *mouse);
void               mvn_mouse_update(mvn_mouse_state_t *mouse);
void               mvn_mouse_set_mapping(mvn_mouse_state_t *mouse,
                                         float              scale_x,
                                         float              scale_y,
                                         float              offset_x,
                                         float              offset_y);
bool               mvn_mouse_btn(mvn_mouse_state_t *mouse, mvn_mouse_btn_t b);
bool               mvn_mouse_btnp(mvn_mouse_state_t *mouse, mvn_mouse_btn_t b);
void               mvn_mouse_show(mvn_mouse_state_t *mouse);
void               mvn_mouse_hide(mvn_mouse_state_t *mouse);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_MOUSE_H */
