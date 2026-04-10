/**
 * \file            touch.h
 * \brief           Touch state — multi-finger position, pressure, gestures
 */

#ifndef DTR_TOUCH_H
#define DTR_TOUCH_H

#include "console_defs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DTR_MAX_FINGERS 10

/**
 * \brief           State for a single tracked finger
 */
typedef struct dtr_finger {
    int64_t id;       /**< SDL finger ID (0 = slot unused) */
    float   x;        /**< Framebuffer X coordinate */
    float   y;        /**< Framebuffer Y coordinate */
    float   dx;       /**< X movement since last frame */
    float   dy;       /**< Y movement since last frame */
    float   pressure; /**< 0..1 */
    bool    active;   /**< Currently touching */
    bool    pressed;  /**< Just pressed this frame */
    bool    released; /**< Just released this frame */
} dtr_finger_t;

/**
 * \brief           Touch subsystem state
 */
typedef struct dtr_touch_state {
    dtr_finger_t fingers[DTR_MAX_FINGERS];
    int32_t      count; /**< Number of active fingers */

    /* Framebuffer dimensions for coordinate mapping */
    int32_t fb_width;
    int32_t fb_height;
} dtr_touch_state_t;

dtr_touch_state_t *dtr_touch_create(int32_t fb_width, int32_t fb_height);
void               dtr_touch_destroy(dtr_touch_state_t *touch);

/**
 * \brief           End-of-frame update — clear per-frame latches, reset deltas
 */
void dtr_touch_update(dtr_touch_state_t *touch);

/**
 * \brief           Finger down event
 */
void dtr_touch_on_down(dtr_touch_state_t *touch,
                       int64_t            finger_id,
                       float              norm_x,
                       float              norm_y,
                       float              pressure);

/**
 * \brief           Finger motion event
 */
void dtr_touch_on_motion(dtr_touch_state_t *touch,
                         int64_t            finger_id,
                         float              norm_x,
                         float              norm_y,
                         float              norm_dx,
                         float              norm_dy,
                         float              pressure);

/**
 * \brief           Finger up event
 */
void dtr_touch_on_up(dtr_touch_state_t *touch, int64_t finger_id);

/* Query helpers */
int32_t dtr_touch_count(dtr_touch_state_t *touch);
bool    dtr_touch_active(dtr_touch_state_t *touch, int32_t index);
bool    dtr_touch_pressed(dtr_touch_state_t *touch, int32_t index);
bool    dtr_touch_released(dtr_touch_state_t *touch, int32_t index);
float   dtr_touch_x(dtr_touch_state_t *touch, int32_t index);
float   dtr_touch_y(dtr_touch_state_t *touch, int32_t index);
float   dtr_touch_dx(dtr_touch_state_t *touch, int32_t index);
float   dtr_touch_dy(dtr_touch_state_t *touch, int32_t index);
float   dtr_touch_pressure(dtr_touch_state_t *touch, int32_t index);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_TOUCH_H */
