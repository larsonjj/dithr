/**
 * \file            touch.c
 * \brief           Touch state management — multi-finger tracking
 */

#include "touch.h"
#include "console.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

dtr_touch_state_t *dtr_touch_create(int32_t fb_width, int32_t fb_height)
{
    dtr_touch_state_t *touch;

    touch = DTR_CALLOC(1, sizeof(dtr_touch_state_t));
    if (touch == NULL) {
        return NULL;
    }
    touch->fb_width  = fb_width;
    touch->fb_height = fb_height;
    return touch;
}

void dtr_touch_destroy(dtr_touch_state_t *touch)
{
    DTR_FREE(touch);
}

/* ------------------------------------------------------------------ */
/*  Per-frame update                                                   */
/* ------------------------------------------------------------------ */

void dtr_touch_update(dtr_touch_state_t *touch)
{
    int32_t idx;

    if (touch == NULL) {
        return;
    }

    for (idx = 0; idx < DTR_MAX_FINGERS; ++idx) {
        dtr_finger_t *fin;

        fin = &touch->fingers[idx];

        /* Clear per-frame edge flags */
        fin->pressed  = false;
        fin->released = false;
        fin->dx       = 0.0f;
        fin->dy       = 0.0f;

        /* Purge dead slots */
        if (!fin->active && fin->id != 0) {
            fin->id = 0;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Internal: find slot by finger id                                   */
/* ------------------------------------------------------------------ */

static dtr_finger_t *prv_find_finger(dtr_touch_state_t *touch, int64_t finger_id)
{
    int32_t idx;

    for (idx = 0; idx < DTR_MAX_FINGERS; ++idx) {
        if (touch->fingers[idx].active && touch->fingers[idx].id == finger_id) {
            return &touch->fingers[idx];
        }
    }
    return NULL;
}

static dtr_finger_t *prv_alloc_finger(dtr_touch_state_t *touch)
{
    int32_t idx;

    for (idx = 0; idx < DTR_MAX_FINGERS; ++idx) {
        if (!touch->fingers[idx].active) {
            return &touch->fingers[idx];
        }
    }
    return NULL; /* All slots occupied */
}

static void prv_recount(dtr_touch_state_t *touch)
{
    int32_t cnt;
    int32_t idx;

    cnt = 0;
    for (idx = 0; idx < DTR_MAX_FINGERS; ++idx) {
        if (touch->fingers[idx].active) {
            ++cnt;
        }
    }
    touch->count = cnt;
}

/* ------------------------------------------------------------------ */
/*  Event handlers                                                     */
/* ------------------------------------------------------------------ */

void dtr_touch_on_down(dtr_touch_state_t *touch,
                       int64_t            finger_id,
                       float              norm_x,
                       float              norm_y,
                       float              pressure)
{
    dtr_finger_t *fin;

    if (touch == NULL) {
        return;
    }

    fin = prv_alloc_finger(touch);
    if (fin == NULL) {
        return; /* No free slots */
    }

    fin->id       = finger_id;
    fin->x        = norm_x * (float)touch->fb_width;
    fin->y        = norm_y * (float)touch->fb_height;
    fin->dx       = 0.0f;
    fin->dy       = 0.0f;
    fin->pressure = pressure;
    fin->active   = true;
    fin->pressed  = true;
    fin->released = false;

    prv_recount(touch);
}

void dtr_touch_on_motion(dtr_touch_state_t *touch,
                         int64_t            finger_id,
                         float              norm_x,
                         float              norm_y,
                         float              norm_dx,
                         float              norm_dy,
                         float              pressure)
{
    dtr_finger_t *fin;

    if (touch == NULL) {
        return;
    }

    fin = prv_find_finger(touch, finger_id);
    if (fin == NULL) {
        return;
    }

    fin->x        = norm_x * (float)touch->fb_width;
    fin->y        = norm_y * (float)touch->fb_height;
    fin->dx      += norm_dx * (float)touch->fb_width;
    fin->dy      += norm_dy * (float)touch->fb_height;
    fin->pressure = pressure;
}

void dtr_touch_on_up(dtr_touch_state_t *touch, int64_t finger_id)
{
    dtr_finger_t *fin;

    if (touch == NULL) {
        return;
    }

    fin = prv_find_finger(touch, finger_id);
    if (fin == NULL) {
        return;
    }

    fin->active   = false;
    fin->released = true;

    prv_recount(touch);
}

/* ------------------------------------------------------------------ */
/*  Queries                                                            */
/* ------------------------------------------------------------------ */

int32_t dtr_touch_count(dtr_touch_state_t *touch)
{
    if (touch == NULL) {
        return 0;
    }
    return touch->count;
}

bool dtr_touch_active(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return false;
    }
    return touch->fingers[index].active;
}

bool dtr_touch_pressed(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return false;
    }
    return touch->fingers[index].pressed;
}

bool dtr_touch_released(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return false;
    }
    return touch->fingers[index].released;
}

float dtr_touch_x(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return 0.0f;
    }
    return touch->fingers[index].x;
}

float dtr_touch_y(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return 0.0f;
    }
    return touch->fingers[index].y;
}

float dtr_touch_dx(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return 0.0f;
    }
    return touch->fingers[index].dx;
}

float dtr_touch_dy(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return 0.0f;
    }
    return touch->fingers[index].dy;
}

float dtr_touch_pressure(dtr_touch_state_t *touch, int32_t index)
{
    if (touch == NULL || index < 0 || index >= DTR_MAX_FINGERS) {
        return 0.0f;
    }
    return touch->fingers[index].pressure;
}
