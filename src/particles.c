/**
 * \file            particles.c
 * \brief           Particle system implementation
 */

#include "particles.h"

#include "console.h"

#include <math.h>
#include <string.h>

dtr_particles_t *dtr_particles_create(void)
{
    dtr_particles_t *p;

    p = DTR_CALLOC(1, sizeof(*p));
    return p;
}

void dtr_particles_destroy(dtr_particles_t *p)
{
    if (p != NULL) {
        DTR_FREE(p);
    }
}

void dtr_particles_clear(dtr_particles_t *p)
{
    if (p == NULL) {
        return;
    }
    memset(p->pool, 0, sizeof(p->pool));
    p->high_water = 0;
    p->alive      = 0;
}

int32_t dtr_particles_emit(dtr_particles_t *p, const dtr_particle_spawn_t *spawn)
{
    int32_t i;

    if (p == NULL || spawn == NULL) {
        return -1;
    }

    /* Linear scan for first inactive slot. */
    for (i = 0; i < CONSOLE_MAX_PARTICLES; ++i) {
        dtr_particle_t *pt;

        pt = &p->pool[i];
        if (pt->active) {
            continue;
        }

        pt->x        = spawn->x;
        pt->y        = spawn->y;
        pt->vx       = spawn->vx;
        pt->vy       = spawn->vy;
        pt->ax       = spawn->ax;
        pt->ay       = spawn->ay;
        pt->drag     = spawn->drag;
        pt->life     = spawn->life > 0.0f ? spawn->life : 1.0f;
        pt->max_life = pt->life;
        pt->color0   = spawn->color0;
        pt->color1   = spawn->color1;
        pt->size     = spawn->size > 0 ? spawn->size : 1;
        pt->kind     = spawn->kind;
        pt->active   = true;

        if (i + 1 > p->high_water) {
            p->high_water = i + 1;
        }
        ++p->alive;
        return i;
    }

    return -1;
}

void dtr_particles_tick(dtr_particles_t *p, float dt)
{
    int32_t i;
    int32_t new_high;
    float   drag_factor;

    if (p == NULL || dt <= 0.0f) {
        return;
    }

    new_high = 0;

    for (i = 0; i < p->high_water; ++i) {
        dtr_particle_t *pt;

        pt = &p->pool[i];
        if (!pt->active) {
            continue;
        }

        /* Integrate */
        pt->vx += pt->ax * dt;
        pt->vy += pt->ay * dt;

        if (pt->drag > 0.0f) {
            drag_factor = 1.0f - pt->drag * dt;
            if (drag_factor < 0.0f) {
                drag_factor = 0.0f;
            }
            pt->vx *= drag_factor;
            pt->vy *= drag_factor;
        }

        pt->x += pt->vx * dt;
        pt->y += pt->vy * dt;

        pt->life -= dt;
        if (pt->life <= 0.0f) {
            pt->active = false;
            --p->alive;
            continue;
        }
        new_high = i + 1;
    }

    p->high_water = new_high;
}

int32_t dtr_particles_count(const dtr_particles_t *p)
{
    if (p == NULL) {
        return 0;
    }
    return p->alive;
}

uint8_t dtr_particles_lerp_color(uint8_t c0, uint8_t c1, float t)
{
    float blended;

    if (t <= 0.0f) {
        return c0;
    }
    if (t >= 1.0f) {
        return c1;
    }
    blended = (float)c0 + ((float)c1 - (float)c0) * t;
    return (uint8_t)(blended + 0.5f);
}
