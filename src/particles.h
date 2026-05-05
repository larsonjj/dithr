/**
 * \file            particles.h
 * \brief           Particle system — fixed-pool emitter with pixel/rect/circle
 *                  primitives, color fade, gravity, drag.
 */

#ifndef DTR_PARTICLES_H
#define DTR_PARTICLES_H

#include "console_defs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CONSOLE_MAX_PARTICLES
#define CONSOLE_MAX_PARTICLES 1024
#endif

/** \brief Particle render kind */
typedef enum dtr_particle_kind {
    DTR_PARTICLE_PIXEL  = 0,
    DTR_PARTICLE_RECT   = 1,
    DTR_PARTICLE_CIRCLE = 2,
} dtr_particle_kind_t;

/** \brief A single particle slot in the pool */
typedef struct dtr_particle {
    float   x;
    float   y;
    float   vx;
    float   vy;
    float   ax;
    float   ay;
    float   drag;     /**< Per-second velocity decay (0 = none, 1 = full) */
    float   life;     /**< Remaining seconds */
    float   max_life; /**< Original life (for fade ratio) */
    uint8_t color0;   /**< Color at birth */
    uint8_t color1;   /**< Color at death (interpolates by life ratio) */
    uint8_t size;     /**< Render size in pixels (≥ 1) */
    uint8_t kind;     /**< dtr_particle_kind_t */
    bool    active;
} dtr_particle_t;

/** \brief Particle pool */
typedef struct dtr_particles {
    dtr_particle_t pool[CONSOLE_MAX_PARTICLES];
    int32_t        high_water; /**< Highest in-use index + 1 */
    int32_t        alive;      /**< Active particle count */
} dtr_particles_t;

/** \brief Spawn config for `dtr_particles_emit` */
typedef struct dtr_particle_spawn {
    float   x;
    float   y;
    float   vx;
    float   vy;
    float   ax;
    float   ay;
    float   drag;
    float   life;
    uint8_t color0;
    uint8_t color1;
    uint8_t size;
    uint8_t kind;
} dtr_particle_spawn_t;

/* ---- Lifecycle --------------------------------------------------------- */

dtr_particles_t *dtr_particles_create(void);
void             dtr_particles_destroy(dtr_particles_t *p);
void             dtr_particles_clear(dtr_particles_t *p);

/* ---- Emit / step ------------------------------------------------------- */

/**
 * \brief           Spawn a single particle.
 * \return          Slot index (>= 0) or -1 if pool is full.
 */
int32_t dtr_particles_emit(dtr_particles_t *p, const dtr_particle_spawn_t *spawn);

/**
 * \brief           Advance every active particle by `dt` seconds, removing
 *                  any whose life dropped to zero.
 */
void dtr_particles_tick(dtr_particles_t *p, float dt);

/* ---- Query ------------------------------------------------------------- */

int32_t dtr_particles_count(const dtr_particles_t *p);

/**
 * \brief           Linear interpolation between two palette indices by t (0..1).
 *                  Used by drawing code to fade color0 → color1 across life.
 *                  Note: this performs a simple integer lerp — for true palette
 *                  blending the cart should pre-pick colors.
 */
uint8_t dtr_particles_lerp_color(uint8_t c0, uint8_t c1, float t);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_PARTICLES_H */
