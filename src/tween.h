/**
 * \file            tween.h
 * \brief           Tween engine — managed value tweens + standalone easing
 */

#ifndef DTR_TWEEN_H
#define DTR_TWEEN_H

#include "console_defs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CONSOLE_MAX_TWEENS
#define CONSOLE_MAX_TWEENS 64
#endif

/** \brief Easing function identifier */
typedef enum dtr_ease {
    DTR_EASE_LINEAR = 0,
    DTR_EASE_IN_QUAD,
    DTR_EASE_OUT_QUAD,
    DTR_EASE_IN_OUT_QUAD,
    DTR_EASE_IN_CUBIC,
    DTR_EASE_OUT_CUBIC,
    DTR_EASE_IN_OUT_CUBIC,
    DTR_EASE_OUT_BACK,
    DTR_EASE_OUT_ELASTIC,
    DTR_EASE_OUT_BOUNCE,
    DTR_EASE_COUNT,
} dtr_ease_t;

/** \brief A single managed tween */
typedef struct dtr_tween_entry {
    double     from;
    double     too; /**< Target value */
    double     duration;
    double     elapsed;
    double     delay;
    dtr_ease_t ease;
    bool       active;
    bool       resolved; /**< True once Promise was resolved */
} dtr_tween_entry_t;

/** \brief Tween pool */
typedef struct dtr_tween {
    dtr_tween_entry_t pool[CONSOLE_MAX_TWEENS];
    int32_t           count; /**< High-water mark for scan range */
} dtr_tween_t;

/* ---- Easing (stateless) ------------------------------------------------ */

/**
 * \brief           Apply an easing function to a normalised 0–1 value
 */
double dtr_ease_apply(dtr_ease_t ease, double time);

/**
 * \brief           Parse an easing name string into an enum value
 */
dtr_ease_t dtr_ease_from_name(const char *name);

/* ---- Managed tweens ---------------------------------------------------- */

void    dtr_tween_init(dtr_tween_t *twn);
int32_t dtr_tween_add(dtr_tween_t *twn,
                      double       from,
                      double       too,
                      double       duration,
                      dtr_ease_t   ease,
                      double       delay);
void    dtr_tween_tick(dtr_tween_t *twn, double delta);
double  dtr_tween_val(dtr_tween_t *twn, int32_t idx);
bool    dtr_tween_done(dtr_tween_t *twn, int32_t idx);
void    dtr_tween_cancel(dtr_tween_t *twn, int32_t idx);
void    dtr_tween_cancel_all(dtr_tween_t *twn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_TWEEN_H */
