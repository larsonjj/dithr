/**
 * \file            tween.c
 * \brief           Tween engine — easing functions and managed value tweens
 */

#include "tween.h"

#include <math.h>
#include <string.h>

/* ---- Easing functions -------------------------------------------------- */

double dtr_ease_apply(dtr_ease_t ease, double time)
{
    double tmp;

    if (time <= 0.0) {
        return 0.0;
    }
    if (time >= 1.0) {
        return 1.0;
    }

    switch (ease) {
        case DTR_EASE_LINEAR:
            return time;

        case DTR_EASE_IN_QUAD:
            return time * time;

        case DTR_EASE_OUT_QUAD:
            return time * (2.0 - time);

        case DTR_EASE_IN_OUT_QUAD:
            if (time < 0.5) {
                return 2.0 * time * time;
            }
            return -1.0 + (4.0 - 2.0 * time) * time;

        case DTR_EASE_IN_CUBIC:
            return time * time * time;

        case DTR_EASE_OUT_CUBIC:
            tmp = time - 1.0;
            return tmp * tmp * tmp + 1.0;

        case DTR_EASE_IN_OUT_CUBIC:
            if (time < 0.5) {
                return 4.0 * time * time * time;
            }
            tmp = 2.0 * time - 2.0;
            return 0.5 * tmp * tmp * tmp + 1.0;

        case DTR_EASE_OUT_BACK:
            tmp = time - 1.0;
            return tmp * tmp * (2.70158 * tmp + 1.70158) + 1.0;

        case DTR_EASE_OUT_ELASTIC:
            return pow(2.0, -10.0 * time) * sin((time - 0.075) * (2.0 * 3.14159265358979) / 0.3) +
                   1.0;

        case DTR_EASE_OUT_BOUNCE:
            if (time < 1.0 / 2.75) {
                return 7.5625 * time * time;
            } else if (time < 2.0 / 2.75) {
                tmp = time - 1.5 / 2.75;
                return 7.5625 * tmp * tmp + 0.75;
            } else if (time < 2.5 / 2.75) {
                tmp = time - 2.25 / 2.75;
                return 7.5625 * tmp * tmp + 0.9375;
            }
            tmp = time - 2.625 / 2.75;
            return 7.5625 * tmp * tmp + 0.984375;

        default:
            return time;
    }
}

dtr_ease_t dtr_ease_from_name(const char *name)
{
    if (name == NULL) {
        return DTR_EASE_LINEAR;
    }
    if (strcmp(name, "linear") == 0) {
        return DTR_EASE_LINEAR;
    }
    if (strcmp(name, "easeIn") == 0) {
        return DTR_EASE_IN_QUAD;
    }
    if (strcmp(name, "easeOut") == 0) {
        return DTR_EASE_OUT_QUAD;
    }
    if (strcmp(name, "easeInOut") == 0) {
        return DTR_EASE_IN_OUT_QUAD;
    }
    if (strcmp(name, "easeInQuad") == 0) {
        return DTR_EASE_IN_QUAD;
    }
    if (strcmp(name, "easeOutQuad") == 0) {
        return DTR_EASE_OUT_QUAD;
    }
    if (strcmp(name, "easeInOutQuad") == 0) {
        return DTR_EASE_IN_OUT_QUAD;
    }
    if (strcmp(name, "easeInCubic") == 0) {
        return DTR_EASE_IN_CUBIC;
    }
    if (strcmp(name, "easeOutCubic") == 0) {
        return DTR_EASE_OUT_CUBIC;
    }
    if (strcmp(name, "easeInOutCubic") == 0) {
        return DTR_EASE_IN_OUT_CUBIC;
    }
    if (strcmp(name, "easeOutBack") == 0) {
        return DTR_EASE_OUT_BACK;
    }
    if (strcmp(name, "easeOutElastic") == 0) {
        return DTR_EASE_OUT_ELASTIC;
    }
    if (strcmp(name, "easeOutBounce") == 0) {
        return DTR_EASE_OUT_BOUNCE;
    }
    return DTR_EASE_LINEAR;
}

/* ---- Managed tween pool ------------------------------------------------ */

void dtr_tween_init(dtr_tween_t *twn)
{
    int32_t idx;

    for (idx = 0; idx < CONSOLE_MAX_TWEENS; ++idx) {
        twn->pool[idx].active   = false;
        twn->pool[idx].resolved = false;
    }
    twn->count = 0;
}

int32_t dtr_tween_add(dtr_tween_t *twn,
                      double       from,
                      double       too,
                      double       duration,
                      dtr_ease_t   ease,
                      double       delay)
{
    int32_t idx;

    for (idx = 0; idx < CONSOLE_MAX_TWEENS; ++idx) {
        if (!twn->pool[idx].active) {
            twn->pool[idx].from     = from;
            twn->pool[idx].too      = too;
            twn->pool[idx].duration = (duration > 0.0) ? duration : 0.001;
            twn->pool[idx].elapsed  = 0.0;
            twn->pool[idx].delay    = delay;
            twn->pool[idx].ease     = ease;
            twn->pool[idx].active   = true;
            twn->pool[idx].resolved = false;

            if (idx >= twn->count) {
                twn->count = idx + 1;
            }
            return idx;
        }
    }
    return -1; /* Pool full */
}

void dtr_tween_tick(dtr_tween_t *twn, double delta)
{
    int32_t idx;

    for (idx = 0; idx < twn->count; ++idx) {
        dtr_tween_entry_t *ent;

        ent = &twn->pool[idx];
        if (!ent->active) {
            continue;
        }

        /* Handle delay */
        if (ent->delay > 0.0) {
            ent->delay -= delta;
            if (ent->delay > 0.0) {
                continue;
            }
            /* Carry over leftover time */
            ent->elapsed = -ent->delay;
            ent->delay   = 0.0;
        } else {
            ent->elapsed += delta;
        }

        /* Clamp to duration */
        if (ent->elapsed > ent->duration) {
            ent->elapsed = ent->duration;
        }
    }
}

double dtr_tween_val(dtr_tween_t *twn, int32_t idx)
{
    dtr_tween_entry_t *ent;
    double             time;

    if (idx < 0 || idx >= CONSOLE_MAX_TWEENS || !twn->pool[idx].active) {
        return 0.0;
    }
    ent = &twn->pool[idx];

    if (ent->delay > 0.0) {
        return ent->from;
    }
    time = ent->elapsed / ent->duration;
    return ent->from + (ent->too - ent->from) * dtr_ease_apply(ent->ease, time);
}

bool dtr_tween_done(dtr_tween_t *twn, int32_t idx)
{
    if (idx < 0 || idx >= CONSOLE_MAX_TWEENS || !twn->pool[idx].active) {
        return true;
    }
    return twn->pool[idx].elapsed >= twn->pool[idx].duration;
}

void dtr_tween_cancel(dtr_tween_t *twn, int32_t idx)
{
    if (idx >= 0 && idx < CONSOLE_MAX_TWEENS) {
        twn->pool[idx].active = false;
    }
}

void dtr_tween_cancel_all(dtr_tween_t *twn)
{
    int32_t idx;

    for (idx = 0; idx < twn->count; ++idx) {
        twn->pool[idx].active = false;
    }
    twn->count = 0;
}
