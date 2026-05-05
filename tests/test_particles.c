/**
 * \file            test_particles.c
 * \brief           Unit tests for the particle system (pure C, no JS)
 */

#include "particles.h"
#include "test_harness.h"

#include <math.h>

static void test_create_destroy(void)
{
    dtr_particles_t *p;

    p = dtr_particles_create();
    DTR_ASSERT_NOT_NULL(p);
    DTR_ASSERT_EQ_INT(dtr_particles_count(p), 0);
    dtr_particles_destroy(p);
    DTR_PASS();
}

static void test_emit_increments_count(void)
{
    dtr_particles_t     *p;
    dtr_particle_spawn_t s = {0};
    int32_t              idx;

    p      = dtr_particles_create();
    s.life = 1.0f;
    s.size = 1;

    idx = dtr_particles_emit(p, &s);
    DTR_ASSERT(idx >= 0);
    DTR_ASSERT_EQ_INT(dtr_particles_count(p), 1);

    dtr_particles_emit(p, &s);
    dtr_particles_emit(p, &s);
    DTR_ASSERT_EQ_INT(dtr_particles_count(p), 3);

    dtr_particles_destroy(p);
    DTR_PASS();
}

static void test_tick_integration(void)
{
    dtr_particles_t     *p;
    dtr_particle_spawn_t s = {0};
    int32_t              idx;

    p      = dtr_particles_create();
    s.x    = 0.0f;
    s.y    = 0.0f;
    s.vx   = 10.0f;
    s.vy   = -20.0f;
    s.ay   = 9.8f;
    s.life = 1.0f;
    s.size = 1;

    idx = dtr_particles_emit(p, &s);
    DTR_ASSERT(idx >= 0);

    /* Tick 1 second: x = vx*t = 10, y = vy*t + 0.5*ay*t^2 = -20 + 4.9 ≈ -15.1
     * (Euler integration so y will be slightly different, just sanity check)  */
    dtr_particles_tick(p, 0.5f);
    DTR_ASSERT(p->pool[idx].x > 4.0f && p->pool[idx].x < 6.0f);

    dtr_particles_destroy(p);
    DTR_PASS();
}

static void test_tick_expires(void)
{
    dtr_particles_t     *p;
    dtr_particle_spawn_t s = {0};

    p      = dtr_particles_create();
    s.life = 0.5f;
    s.size = 1;
    dtr_particles_emit(p, &s);
    dtr_particles_emit(p, &s);

    DTR_ASSERT_EQ_INT(dtr_particles_count(p), 2);
    dtr_particles_tick(p, 1.0f);
    DTR_ASSERT_EQ_INT(dtr_particles_count(p), 0);

    dtr_particles_destroy(p);
    DTR_PASS();
}

static void test_pool_full(void)
{
    dtr_particles_t     *p;
    dtr_particle_spawn_t s = {0};
    int32_t              i;
    int32_t              ok;

    p      = dtr_particles_create();
    s.life = 10.0f;
    s.size = 1;
    ok     = 0;

    for (i = 0; i < CONSOLE_MAX_PARTICLES + 16; ++i) {
        if (dtr_particles_emit(p, &s) >= 0) {
            ++ok;
        }
    }
    DTR_ASSERT_EQ_INT(ok, CONSOLE_MAX_PARTICLES);
    DTR_ASSERT_EQ_INT(dtr_particles_count(p), CONSOLE_MAX_PARTICLES);

    dtr_particles_destroy(p);
    DTR_PASS();
}

static void test_clear(void)
{
    dtr_particles_t     *p;
    dtr_particle_spawn_t s = {0};

    p      = dtr_particles_create();
    s.life = 1.0f;
    s.size = 1;
    dtr_particles_emit(p, &s);
    dtr_particles_emit(p, &s);
    DTR_ASSERT_EQ_INT(dtr_particles_count(p), 2);

    dtr_particles_clear(p);
    DTR_ASSERT_EQ_INT(dtr_particles_count(p), 0);
    DTR_ASSERT_EQ_INT(p->high_water, 0);

    dtr_particles_destroy(p);
    DTR_PASS();
}

static void test_lerp_color(void)
{
    DTR_ASSERT_EQ_INT(dtr_particles_lerp_color((uint8_t)0, (uint8_t)100, 0.0f), 0);
    DTR_ASSERT_EQ_INT(dtr_particles_lerp_color((uint8_t)0, (uint8_t)100, 1.0f), 100);
    DTR_ASSERT_EQ_INT(dtr_particles_lerp_color((uint8_t)0, (uint8_t)100, 0.5f), 50);
    DTR_ASSERT_EQ_INT(dtr_particles_lerp_color((uint8_t)7, (uint8_t)7, 0.5f), 7);
    DTR_ASSERT_EQ_INT(dtr_particles_lerp_color((uint8_t)0, (uint8_t)255, -1.0f), 0);
    DTR_ASSERT_EQ_INT(dtr_particles_lerp_color((uint8_t)0, (uint8_t)255, 2.0f), 255);
    DTR_PASS();
}

static void test_drag_decays_velocity(void)
{
    dtr_particles_t     *p;
    dtr_particle_spawn_t s = {0};
    int32_t              idx;

    p      = dtr_particles_create();
    s.vx   = 100.0f;
    s.drag = 1.0f; /* full decay per second */
    s.life = 10.0f;
    s.size = 1;

    idx = dtr_particles_emit(p, &s);
    DTR_ASSERT(idx >= 0);

    dtr_particles_tick(p, 0.5f);
    /* vx should be ~50 after half a second of full drag */
    DTR_ASSERT(p->pool[idx].vx > 40.0f && p->pool[idx].vx < 60.0f);

    dtr_particles_destroy(p);
    DTR_PASS();
}

static void test_emit_null_safe(void)
{
    /* NULL pool / NULL spawn must return -1 without crashing */
    DTR_ASSERT_EQ_INT(dtr_particles_emit(NULL, NULL), -1);
    DTR_ASSERT_EQ_INT(dtr_particles_count(NULL), 0);
    dtr_particles_clear(NULL); /* no-op */
    dtr_particles_tick(NULL, 0.1f);
    dtr_particles_destroy(NULL);
    DTR_PASS();
}

int main(void)
{
    DTR_TEST_BEGIN("test_particles");

    DTR_RUN_TEST(test_create_destroy);
    DTR_RUN_TEST(test_emit_increments_count);
    DTR_RUN_TEST(test_tick_integration);
    DTR_RUN_TEST(test_tick_expires);
    DTR_RUN_TEST(test_pool_full);
    DTR_RUN_TEST(test_clear);
    DTR_RUN_TEST(test_lerp_color);
    DTR_RUN_TEST(test_drag_decays_velocity);
    DTR_RUN_TEST(test_emit_null_safe);

    DTR_TEST_END();
}
