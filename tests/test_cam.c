/**
 * \file            test_cam.c
 * \brief           Unit tests for camera helper functions
 *
 * Tests camera set/get/reset/follow via the graphics camera API.
 */

#include "graphics.h"
#include "test_harness.h"

#define TW 64
#define TH 64

/* ------------------------------------------------------------------ */
/*  cam.set / cam.get                                                  */
/* ------------------------------------------------------------------ */

static void test_cam_set_get(void)
{
    dtr_graphics_t *gfx;

    gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_camera(gfx, 100, 200);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 100);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 200);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_cam_set_negative(void)
{
    dtr_graphics_t *gfx;

    gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_camera(gfx, -50, -75);
    DTR_ASSERT_EQ_INT(gfx->camera_x, -50);
    DTR_ASSERT_EQ_INT(gfx->camera_y, -75);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_cam_set_zero(void)
{
    dtr_graphics_t *gfx;

    gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_camera(gfx, 100, 100);
    dtr_gfx_camera(gfx, 0, 0);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 0);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 0);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  cam.reset                                                          */
/* ------------------------------------------------------------------ */

static void test_cam_reset(void)
{
    dtr_graphics_t *gfx;

    gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_camera(gfx, 42, 99);
    /* Reset = set to (0,0) */
    dtr_gfx_camera(gfx, 0, 0);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 0);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 0);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_cam_default_zero(void)
{
    dtr_graphics_t *gfx;

    gfx = dtr_gfx_create(TW, TH);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 0);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 0);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  cam.follow (lerp toward target)                                    */
/* ------------------------------------------------------------------ */

static void test_cam_follow_lerp(void)
{
    dtr_graphics_t *gfx;
    double          speed;
    double          cx;
    double          cy;

    gfx = dtr_gfx_create(TW, TH);

    /* Start at origin, follow toward (100, 200) at speed 0.5 */
    speed = 0.5;
    cx    = 0.0 + (100.0 - 0.0) * speed;   /* 50 */
    cy    = 0.0 + (200.0 - 0.0) * speed;   /* 100 */
    dtr_gfx_camera(gfx, (int32_t)cx, (int32_t)cy);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 50);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 100);

    /* Second follow from (50,100) toward (100,200) at speed 0.5 */
    cx = 50.0 + (100.0 - 50.0) * speed;    /* 75 */
    cy = 100.0 + (200.0 - 100.0) * speed;  /* 150 */
    dtr_gfx_camera(gfx, (int32_t)cx, (int32_t)cy);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 75);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 150);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_cam_follow_speed_one(void)
{
    dtr_graphics_t *gfx;
    double          cx;
    double          cy;

    gfx = dtr_gfx_create(TW, TH);

    /* Speed 1.0 should snap to target immediately */
    cx = 0.0 + (80.0 - 0.0) * 1.0;
    cy = 0.0 + (60.0 - 0.0) * 1.0;
    dtr_gfx_camera(gfx, (int32_t)cx, (int32_t)cy);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 80);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 60);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_cam_follow_speed_zero(void)
{
    dtr_graphics_t *gfx;
    double          cx;
    double          cy;

    gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_camera(gfx, 30, 40);

    /* Speed 0.0 should not move */
    cx = 30.0 + (200.0 - 30.0) * 0.0;
    cy = 40.0 + (200.0 - 40.0) * 0.0;
    dtr_gfx_camera(gfx, (int32_t)cx, (int32_t)cy);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 30);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 40);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Camera affects drawing                                             */
/* ------------------------------------------------------------------ */

static void test_cam_offset_drawing(void)
{
    dtr_graphics_t *gfx;

    gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Draw a pixel at (10,10) with camera offset (-5,-5) */
    dtr_gfx_camera(gfx, -5, -5);
    dtr_gfx_pset(gfx, 10, 10, 7);

    /* The pixel should appear at (10 - (-5), 10 - (-5)) = (15, 15) */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[15 * TW + 15], 7);

    /* The raw position (10,10) should still be 0 */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[10 * TW + 10], 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Text width / height                                                */
/* ------------------------------------------------------------------ */

static void test_text_width_empty(void)
{
    dtr_graphics_t *gfx;

    gfx = dtr_gfx_create(TW, TH);
    DTR_ASSERT_EQ_INT(dtr_gfx_text_width(gfx, ""), 0);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_text_width_single_char(void)
{
    dtr_graphics_t *gfx;
    int32_t         w;

    gfx = dtr_gfx_create(TW, TH);
    w   = dtr_gfx_text_width(gfx, "A");
    /* Default font: 4px wide, no trailing gap for single char */
    DTR_ASSERT(w > 0);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_text_width_multichar(void)
{
    dtr_graphics_t *gfx;
    int32_t         w1;
    int32_t         w2;

    gfx = dtr_gfx_create(TW, TH);
    w1  = dtr_gfx_text_width(gfx, "AB");
    w2  = dtr_gfx_text_width(gfx, "ABCD");
    /* Longer string should be wider */
    DTR_ASSERT(w2 > w1);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_text_height_single_line(void)
{
    dtr_graphics_t *gfx;
    int32_t         h;

    gfx = dtr_gfx_create(TW, TH);
    h   = dtr_gfx_text_height(gfx, "hello");
    DTR_ASSERT(h > 0);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_text_height_multiline(void)
{
    dtr_graphics_t *gfx;
    int32_t         h1;
    int32_t         h2;

    gfx = dtr_gfx_create(TW, TH);
    h1  = dtr_gfx_text_height(gfx, "line1");
    h2  = dtr_gfx_text_height(gfx, "line1\nline2");
    /* Two lines should be taller than one */
    DTR_ASSERT(h2 > h1);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("cam (camera helpers) + text metrics");

    /* set / get */
    DTR_RUN_TEST(test_cam_set_get);
    DTR_RUN_TEST(test_cam_set_negative);
    DTR_RUN_TEST(test_cam_set_zero);

    /* reset */
    DTR_RUN_TEST(test_cam_reset);
    DTR_RUN_TEST(test_cam_default_zero);

    /* follow (lerp) */
    DTR_RUN_TEST(test_cam_follow_lerp);
    DTR_RUN_TEST(test_cam_follow_speed_one);
    DTR_RUN_TEST(test_cam_follow_speed_zero);

    /* camera affects drawing */
    DTR_RUN_TEST(test_cam_offset_drawing);

    /* text width / height */
    DTR_RUN_TEST(test_text_width_empty);
    DTR_RUN_TEST(test_text_width_single_char);
    DTR_RUN_TEST(test_text_width_multichar);
    DTR_RUN_TEST(test_text_height_single_line);
    DTR_RUN_TEST(test_text_height_multiline);

    DTR_TEST_END();
}
