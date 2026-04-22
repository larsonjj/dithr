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

#ifndef CONSOLE_MAX_TWEEN_SEQS
#define CONSOLE_MAX_TWEEN_SEQS 16
#endif

#ifndef CONSOLE_MAX_SEQ_STEPS
#define CONSOLE_MAX_SEQ_STEPS 8
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

    /** Sequences: each is a chain of tween pool indices run in order */
    struct {
        int32_t steps[CONSOLE_MAX_SEQ_STEPS]; /**< Pool indices */
        int32_t step_count;
        bool    active;
        bool    parallel; /**< True = all steps run simultaneously */
    } seqs[CONSOLE_MAX_TWEEN_SEQS];
    int32_t seq_count; /**< High-water mark for sequence scan */
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

/* ---- Sequences & parallels --------------------------------------------- */

/**
 * \brief           Create a sequence of tweens that run one after another.
 * \param[in]       twn: Pool
 * \param[in]       steps: Array of {from, to, dur, ease} tuples (6 doubles each:
 *                         from, to, dur, ease_id, delay, _unused)
 * \param[in]       count: Number of steps (max CONSOLE_MAX_SEQ_STEPS)
 * \return          Sequence index (>= 0) on success, -1 on failure
 *
 * Each step's delay is automatically stacked so they run in order.
 */
int32_t dtr_tween_seq_add(dtr_tween_t      *twn,
                          const double     *from,
                          const double     *too,
                          const double     *dur,
                          const dtr_ease_t *ease,
                          int32_t           count);

/**
 * \brief           Create a parallel group of tweens that all run simultaneously.
 * \return          Sequence index (>= 0) on success, -1 on failure
 */
int32_t dtr_tween_par_add(dtr_tween_t      *twn,
                          const double     *from,
                          const double     *too,
                          const double     *dur,
                          const dtr_ease_t *ease,
                          int32_t           count);

/**
 * \brief           Get current value of a sequence (value of active step)
 */
double dtr_tween_seq_val(dtr_tween_t *twn, int32_t seq_idx);

/**
 * \brief           Check if a sequence/parallel group is completely done
 */
bool dtr_tween_seq_done(dtr_tween_t *twn, int32_t seq_idx);

/**
 * \brief           Cancel a sequence and all its sub-tweens
 */
void dtr_tween_seq_cancel(dtr_tween_t *twn, int32_t seq_idx);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_TWEEN_H */
