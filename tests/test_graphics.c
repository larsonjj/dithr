/**
 * \file            test_graphics.c
 * \brief           Unit tests for the graphics subsystem
 */

#include "graphics.h"
#include "test_harness.h"

#include <string.h>

/* Small framebuffer for fast tests */
#define TW 16
#define TH 16

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void test_gfx_create_destroy(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    DTR_ASSERT(gfx != NULL);
    DTR_ASSERT_EQ_INT(gfx->width, TW);
    DTR_ASSERT_EQ_INT(gfx->height, TH);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_destroy_null(void)
{
    dtr_gfx_destroy(NULL); /* must not crash */
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  cls / pset / pget                                                  */
/* ------------------------------------------------------------------ */

static void test_gfx_cls(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 5);
    for (int i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 5);
    }
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_pset_pget(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_pset(gfx, 3, 4, 9);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 3, 4), 9);

    /* Unset pixel should be 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_pget_out_of_bounds(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 7);

    /* Out-of-bounds pget returns 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, -1, 0), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, TW, 0), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, TH), 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_pset_clipped(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Drawing outside framebuffer should not crash or write */
    dtr_gfx_pset(gfx, -1, -1, 3);
    dtr_gfx_pset(gfx, TW, TH, 3);

    /* No pixel should have changed to 3 */
    for (int i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Camera                                                             */
/* ------------------------------------------------------------------ */

static void test_gfx_camera(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Camera shifts drawing coordinates */
    dtr_gfx_camera(gfx, 5, 5);
    dtr_gfx_pset(gfx, 7, 7, 2);

    /* Screen position: (7-5, 7-5) = (2, 2) */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[2 * TW + 2], 2);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Clip                                                               */
/* ------------------------------------------------------------------ */

static void test_gfx_clip(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Restrict clip to a 4x4 region at (2, 2) */
    dtr_gfx_clip(gfx, 2, 2, 4, 4);
    DTR_ASSERT_EQ_INT(gfx->clip_x, 2);
    DTR_ASSERT_EQ_INT(gfx->clip_y, 2);
    DTR_ASSERT_EQ_INT(gfx->clip_w, 4);
    DTR_ASSERT_EQ_INT(gfx->clip_h, 4);

    /* Drawing inside clip should work */
    dtr_gfx_pset(gfx, 3, 3, 1);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 3, 3), 1);

    /* Drawing outside clip should be rejected */
    dtr_gfx_pset(gfx, 0, 0, 5);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[0], 0);

    dtr_gfx_pset(gfx, 6, 6, 5);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 6], 0);

    /* Reset clip */
    dtr_gfx_clip_reset(gfx);
    DTR_ASSERT_EQ_INT(gfx->clip_x, 0);
    DTR_ASSERT_EQ_INT(gfx->clip_w, TW);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Palette                                                            */
/* ------------------------------------------------------------------ */

static void test_gfx_draw_pal(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Remap colour 5 → 9 in the draw palette */
    dtr_gfx_pal(gfx, 5, 9, false);
    dtr_gfx_pset(gfx, 0, 0, 5);

    /* The framebuffer should store the remapped colour */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[0], 9);

    dtr_gfx_pal_reset(gfx);
    dtr_gfx_pset(gfx, 1, 0, 5);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[1], 5);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_transparency(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Colour 0 transparent by default */
    DTR_ASSERT(gfx->transparent[0] == true);
    DTR_ASSERT(gfx->transparent[1] == false);

    dtr_gfx_palt(gfx, 3, true);
    DTR_ASSERT(gfx->transparent[3] == true);

    dtr_gfx_palt_reset(gfx);
    DTR_ASSERT(gfx->transparent[3] == false);
    DTR_ASSERT(gfx->transparent[0] == true); /* 0 remains transparent */

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Color / cursor state                                               */
/* ------------------------------------------------------------------ */

static void test_gfx_color(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    DTR_ASSERT_EQ_INT(gfx->color, 7); /* default */
    dtr_gfx_color(gfx, 12);
    DTR_ASSERT_EQ_INT(gfx->color, 12);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_cursor(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_cursor(gfx, 5, 10);
    DTR_ASSERT_EQ_INT(gfx->cursor_x, 5);
    DTR_ASSERT_EQ_INT(gfx->cursor_y, 10);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Primitives: line, rect, rectfill                                   */
/* ------------------------------------------------------------------ */

static void test_gfx_line_horizontal(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_line(gfx, 2, 3, 8, 3, 4);

    /* All pixels on row 3 from x=2..8 should be colour 4 */
    for (int x = 2; x <= 8; ++x) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[3 * TW + x], 4);
    }
    /* Pixel just outside shouldn't be set */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[3 * TW + 1], 0);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[3 * TW + 9], 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_line_vertical(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_line(gfx, 5, 2, 5, 10, 6);

    /* All pixels in column 5 from y=2..10 should be colour 6 */
    for (int y = 2; y <= 10; ++y) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[y * TW + 5], 6);
    }
    /* Pixels just outside */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[1 * TW + 5], 0);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[11 * TW + 5], 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_line_diagonal_clip(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Draw a diagonal line that extends well outside the framebuffer */
    dtr_gfx_line(gfx, -10, -10, TW + 10, TH + 10, 3);

    /* Should not corrupt memory — just verify no crash and some pixels drawn */
    int count = 0;
    for (int i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 3) {
            ++count;
        }
    }
    DTR_ASSERT(count > 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_line_single_point(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Line from a point to itself — should draw a single pixel */
    dtr_gfx_line(gfx, 7, 7, 7, 7, 5);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[7 * TW + 7], 5);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_rectfill(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_rectfill(gfx, 2, 2, 5, 5, 3);

    /* Pixels inside the filled rect */
    for (int y = 2; y <= 5; ++y) {
        for (int x = 2; x <= 5; ++x) {
            DTR_ASSERT_EQ_INT(gfx->framebuffer[y * TW + x], 3);
        }
    }
    /* Pixel just outside */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[1 * TW + 2], 0);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 2], 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_rect_outline(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_rect(gfx, 2, 2, 6, 6, 8);

    /* Corners should be set */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[2 * TW + 2], 8);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[2 * TW + 6], 8);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 2], 8);
    DTR_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 6], 8);

    /* Interior should be empty */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[4 * TW + 4], 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Circle                                                             */
/* ------------------------------------------------------------------ */

static void test_gfx_circ_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_circ(gfx, 8, 8, 3, 2);

    /* Top, bottom, left, right of circle should be drawn */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 5), 2);  /* top */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 11), 2); /* bottom */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 5, 8), 2);  /* left */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 11, 8), 2); /* right */

    /* Centre should not be filled (outline only) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 8), 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_circfill_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_circfill(gfx, 8, 8, 3, 4);

    /* Centre should be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 8), 4);

    /* Cardinal edges should be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 5), 4);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 5, 8), 4);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_circ_at_boundary(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Circle centred at corner — should clip without crashing */
    dtr_gfx_circ(gfx, 0, 0, 5, 1);
    dtr_gfx_circfill(gfx, TW - 1, TH - 1, 5, 2);

    /* Just verify no crash and some pixels were drawn */
    int count = 0;
    for (int i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] != 0) {
            ++count;
        }
    }
    DTR_ASSERT(count > 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_circ_zero_radius(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Radius 0 — should draw a single pixel */
    dtr_gfx_circ(gfx, 8, 8, 0, 6);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 8), 6);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Rect boundary tests                                                */
/* ------------------------------------------------------------------ */

static void test_gfx_rectfill_clipped(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Rect that extends beyond all edges — should not crash */
    dtr_gfx_rectfill(gfx, -5, -5, TW + 5, TH + 5, 1);

    /* Entire framebuffer should be filled */
    for (int i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 1);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_rect_fully_outside(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Rect completely outside the framebuffer — should draw nothing */
    dtr_gfx_rectfill(gfx, TW + 1, TH + 1, TW + 10, TH + 10, 5);

    for (int i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Sprite flags                                                       */
/* ------------------------------------------------------------------ */

static void test_gfx_sprite_flags(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Default flags are 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_fget(gfx, 0), 0);

    dtr_gfx_fset(gfx, 0, 0xFF);
    DTR_ASSERT_EQ_INT(dtr_gfx_fget(gfx, 0), 0xFF);

    /* Per-bit set/get */
    dtr_gfx_fset(gfx, 1, 0);
    dtr_gfx_fset_bit(gfx, 1, 2, true);
    DTR_ASSERT(dtr_gfx_fget_bit(gfx, 1, 2));
    DTR_ASSERT(!dtr_gfx_fget_bit(gfx, 1, 0));

    dtr_gfx_fset_bit(gfx, 1, 2, false);
    DTR_ASSERT(!dtr_gfx_fget_bit(gfx, 1, 2));

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_sprite_flags_oob(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Out-of-bounds index should return 0 and not crash */
    DTR_ASSERT_EQ_INT(dtr_gfx_fget(gfx, -1), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_fget(gfx, CONSOLE_MAX_SPRITES), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_fget(gfx, CONSOLE_MAX_SPRITES + 100), 0);

    /* Out-of-bounds set should not crash */
    dtr_gfx_fset(gfx, -1, 0xFF);
    dtr_gfx_fset(gfx, CONSOLE_MAX_SPRITES, 0xFF);

    /* Out-of-bounds bit flag should return false */
    DTR_ASSERT(!dtr_gfx_fget_bit(gfx, 0, -1));
    DTR_ASSERT(!dtr_gfx_fget_bit(gfx, 0, CONSOLE_SPRITE_FLAGS));

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Fill pattern                                                       */
/* ------------------------------------------------------------------ */

static void test_gfx_fill_pattern(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Checkerboard pattern: alternating bits in a 4x4 grid */
    dtr_gfx_fillp(gfx, 0x5A5A);

    /* Draw a filled rect — only pattern-enabled pixels should be set */
    dtr_gfx_rectfill(gfx, 0, 0, 3, 3, 2);

    /* Count filled pixels — should be exactly half of 4x4 = 8 */
    int count = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (gfx->framebuffer[y * TW + x] == 2)
                ++count;
        }
    }
    DTR_ASSERT_EQ_INT(count, 8);

    /* Reset pattern */
    dtr_gfx_fillp(gfx, 0);
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_rectfill(gfx, 0, 0, 3, 3, 2);

    /* All 16 pixels should be filled now */
    count = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (gfx->framebuffer[y * TW + x] == 2)
                ++count;
        }
    }
    DTR_ASSERT_EQ_INT(count, 16);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Flip (palette → RGBA)                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_flip(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_pset(gfx, 0, 0, 1);
    dtr_gfx_flip(gfx);

    /* Pixel at (0,0) should be colour 1 from the palette */
    DTR_ASSERT_EQ_INT(gfx->pixels[0], gfx->colors[gfx->screen_pal[1]]);

    /* Pixel at (1,0) should be colour 0 */
    DTR_ASSERT_EQ_INT(gfx->pixels[1], gfx->colors[gfx->screen_pal[0]]);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Reset                                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_reset(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Mutate state */
    dtr_gfx_color(gfx, 12);
    dtr_gfx_camera(gfx, 3, 3);
    dtr_gfx_clip(gfx, 1, 1, 4, 4);
    dtr_gfx_cursor(gfx, 5, 5);
    dtr_gfx_fillp(gfx, 0xAAAA);

    dtr_gfx_reset(gfx);

    DTR_ASSERT_EQ_INT(gfx->color, 7);
    DTR_ASSERT_EQ_INT(gfx->camera_x, 0);
    DTR_ASSERT_EQ_INT(gfx->camera_y, 0);
    DTR_ASSERT_EQ_INT(gfx->clip_x, 0);
    DTR_ASSERT_EQ_INT(gfx->clip_y, 0);
    DTR_ASSERT_EQ_INT(gfx->clip_w, TW);
    DTR_ASSERT_EQ_INT(gfx->clip_h, TH);
    DTR_ASSERT_EQ_INT(gfx->cursor_x, 0);
    DTR_ASSERT_EQ_INT(gfx->cursor_y, 0);
    DTR_ASSERT_EQ_INT(gfx->fill_pattern, 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Polygon                                                            */
/* ------------------------------------------------------------------ */

static void test_gfx_poly_outline(void)
{
    dtr_graphics_t *gfx   = dtr_gfx_create(TW, TH);
    int32_t         pts[] = {2, 2, 8, 2, 8, 8, 2, 8};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_poly(gfx, pts, 4, 7);

    /* Corners should be drawn */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 2, 2), 7);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 2), 7);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 8), 7);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 2, 8), 7);

    /* Centre should not be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 5, 5), 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_polyfill(void)
{
    dtr_graphics_t *gfx   = dtr_gfx_create(TW, TH);
    int32_t         pts[] = {2, 2, 8, 2, 8, 8, 2, 8};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_polyfill(gfx, pts, 4, 3);

    /* Centre should be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 5, 5), 3);

    /* Corners should be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 2, 2), 3);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_poly_degenerate(void)
{
    dtr_graphics_t *gfx   = dtr_gfx_create(TW, TH);
    int32_t         pts[] = {0, 0, 1, 1};

    dtr_gfx_cls(gfx, 0);

    /* Less than 3 vertices — should not crash */
    dtr_gfx_poly(gfx, pts, 1, 7);
    dtr_gfx_polyfill(gfx, pts, 2, 7);
    dtr_gfx_poly(gfx, NULL, 0, 7);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Custom font                                                        */
/* ------------------------------------------------------------------ */

static void test_gfx_font_custom_and_reset(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Set custom font */
    dtr_gfx_font(gfx, 0, 0, 8, 8, ' ', 95);
    DTR_ASSERT(gfx->custom_font.active);
    DTR_ASSERT_EQ_INT(gfx->custom_font.char_w, 8);
    DTR_ASSERT_EQ_INT(gfx->custom_font.char_h, 8);
    DTR_ASSERT_EQ_INT(gfx->custom_font.first, ' ');
    DTR_ASSERT_EQ_INT(gfx->custom_font.count, 95);

    /* Reset */
    dtr_gfx_font_reset(gfx);
    DTR_ASSERT(!gfx->custom_font.active);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Screen transitions                                                 */
/* ------------------------------------------------------------------ */

static void test_gfx_fade(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    dtr_gfx_fade(gfx, 0, 10);
    DTR_ASSERT(dtr_gfx_transitioning(gfx));
    DTR_ASSERT_EQ_INT(gfx->transition.type, DTR_TRANS_FADE);
    DTR_ASSERT_EQ_INT(gfx->transition.duration, 10);
    DTR_ASSERT_EQ_INT(gfx->transition.color, 0);

    /* Run until completion */
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_flip(gfx);
    for (int i = 0; i < 10; ++i) {
        dtr_gfx_transition_update(gfx);
    }
    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_wipe(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_wipe(gfx, DTR_WIPE_LEFT, 1, 5);
    DTR_ASSERT(dtr_gfx_transitioning(gfx));
    DTR_ASSERT_EQ_INT(gfx->transition.type, DTR_TRANS_WIPE);
    DTR_ASSERT_EQ_INT(gfx->transition.direction, DTR_WIPE_LEFT);

    /* Run until completion */
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_flip(gfx);
    for (int i = 0; i < 5; ++i) {
        dtr_gfx_transition_update(gfx);
    }
    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dissolve(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_dissolve(gfx, 2, 8);
    DTR_ASSERT(dtr_gfx_transitioning(gfx));
    DTR_ASSERT_EQ_INT(gfx->transition.type, DTR_TRANS_DISSOLVE);

    /* Run until completion */
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_flip(gfx);
    for (int i = 0; i < 8; ++i) {
        dtr_gfx_transition_update(gfx);
    }
    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_transition_clamp_frames(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* frames < 1 should be clamped to 1 */
    dtr_gfx_fade(gfx, 0, 0);
    DTR_ASSERT_EQ_INT(gfx->transition.duration, 1);

    dtr_gfx_wipe(gfx, 0, 0, -5);
    DTR_ASSERT_EQ_INT(gfx->transition.duration, 1);

    dtr_gfx_dissolve(gfx, 0, 0);
    DTR_ASSERT_EQ_INT(gfx->transition.duration, 1);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Draw list                                                          */
/* ------------------------------------------------------------------ */

static void test_gfx_dl_begin_end(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_dl_begin(gfx);
    DTR_ASSERT(gfx->draw_list.active);
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, 0);

    dtr_gfx_dl_end(gfx);
    DTR_ASSERT(!gfx->draw_list.active);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_queue_and_sort(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_dl_begin(gfx);

    /* Queue sprites at different layers (higher layer first) */
    dtr_gfx_dl_spr(gfx, 10, 0, 0, 0, 1, 1, false, false);
    dtr_gfx_dl_spr(gfx, 1, 0, 0, 0, 1, 1, false, false);
    dtr_gfx_dl_spr(gfx, 5, 0, 0, 0, 1, 1, false, false);
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, 3);

    /* dl_end sorts by layer and flushes */
    dtr_gfx_dl_end(gfx);
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_overflow(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_dl_begin(gfx);

    /* Fill to capacity */
    for (int i = 0; i < CONSOLE_MAX_DRAW_CMDS; ++i) {
        dtr_gfx_dl_spr(gfx, 0, 0, 0, 0, 1, 1, false, false);
    }
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    /* One more should be silently dropped */
    dtr_gfx_dl_spr(gfx, 0, 0, 0, 0, 1, 1, false, false);
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    /* Clear queue before dl_end to avoid rendering 1024 sprites */
    gfx->draw_list.count = 0;
    dtr_gfx_dl_end(gfx);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Triangle outline                                                   */
/* ------------------------------------------------------------------ */

static void test_gfx_tri_outline(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_tri(gfx, 2, 2, 10, 2, 6, 10, 5);

    /* Vertices should be drawn */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 2, 2), 5);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 10, 2), 5);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 6, 10), 5);

    /* Interior should not be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 6, 5), 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_trifill(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_trifill(gfx, 2, 2, 12, 2, 7, 12, 3);

    /* Interior should be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 7, 5), 3);

    /* Vertices should be filled */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 2, 2), 3);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 12, 2), 3);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_trifill_clipped(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Triangle extending outside — should not crash */
    dtr_gfx_trifill(gfx, -5, -5, TW + 5, 0, TW / 2, TH + 5, 2);

    int count = 0;
    for (int i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 2)
            ++count;
    }
    DTR_ASSERT(count > 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Print (built-in font)                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_print_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         advance;

    dtr_gfx_cls(gfx, 0);
    advance = dtr_gfx_print(gfx, "AB", 0, 0, 7);

    /* Each built-in char is 4px wide + 1px spacing = 5 per char, 2 chars = 10 */
    DTR_ASSERT(advance > 0);

    /* Some pixels should have been drawn */
    int count = 0;
    for (int i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 7)
            ++count;
    }
    DTR_ASSERT(count > 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_print_newline(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    dtr_gfx_print(gfx, "A\nB", 0, 0, 7);

    /* Cursor Y should have advanced past the first line */
    DTR_ASSERT(gfx->cursor_y > 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_print_empty(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         advance;

    dtr_gfx_cls(gfx, 0);
    advance = dtr_gfx_print(gfx, "", 0, 0, 7);
    DTR_ASSERT_EQ_INT(advance, 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Sprite drawing                                                     */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);

    /* Set up a minimal 8x8 sprite sheet with one 8x8 tile */
    memset(sheet_data, 0, sizeof(sheet_data));
    sheet_data[0] = 5; /* top-left pixel */

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Draw sprite 0 at (0, 0) */
    dtr_gfx_spr(gfx, 0, 0, 0, 1, 1, false, false);

    /* Top-left pixel should be colour 5 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 5);

    /* Clean up */
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_null_sheet(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Drawing with no sprite sheet should not crash */
    dtr_gfx_spr(gfx, 0, 0, 0, 1, 1, false, false);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_out_of_range(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 5, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Negative index and beyond-count index: should not crash or draw */
    dtr_gfx_spr(gfx, -1, 0, 0, 1, 1, false, false);
    dtr_gfx_spr(gfx, 1, 0, 0, 1, 1, false, false);

    for (int i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_flip(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 0, sizeof(sheet_data));

    /* Only top-left pixel has colour */
    sheet_data[0] = 3;

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Flip X: top-left pixel should appear at top-right (x=7) */
    dtr_gfx_spr(gfx, 0, 0, 0, 1, 1, true, false);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 7, 0), 3);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);

    /* Flip Y: top-left pixel should appear at bottom-left (y=7) */
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_spr(gfx, 0, 0, 0, 1, 1, false, true);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 7), 3);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  sspr (stretch sprite)                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_sspr_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 4, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Copy 4x4 source region → 4x4 destination */
    dtr_gfx_sspr(gfx, 0, 0, 4, 4, 2, 2, 4, 4);

    /* Destination pixels should have colour 4 */
    for (int y = 2; y < 6; ++y) {
        for (int x = 2; x < 6; ++x) {
            DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, x, y), 4);
        }
    }

    /* Outside should remain 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 1, 2), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_sspr_null_sheet(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Should not crash with no sheet */
    dtr_gfx_sspr(gfx, 0, 0, 4, 4, 0, 0, 4, 4);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  spr_rot — rotated sprite drawing                                   */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_rot_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         found;

    dtr_gfx_cls(gfx, 0);

    /* Fill sheet with colour 3 */
    memset(sheet_data, 3, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Rotate 0 radians around center (4,4): should draw sprite normally */
    dtr_gfx_spr_rot(gfx, 0, 4, 4, 0.0f, 4, 4);

    /* At least some pixels should be drawn with colour 3 */
    found = 0;
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        if (gfx->framebuffer[idx] == 3) {
            ++found;
        }
    }
    DTR_ASSERT(found > 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_rot_null_sheet(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Should not crash with no sheet */
    dtr_gfx_spr_rot(gfx, 0, 4, 4, 1.0f, 4, 4);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_rot_out_of_range(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 2, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Invalid index: negative and beyond count */
    dtr_gfx_spr_rot(gfx, -1, 4, 4, 0.0f, 4, 4);
    dtr_gfx_spr_rot(gfx, 99, 4, 4, 0.0f, 4, 4);

    /* Framebuffer should be untouched */
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[idx], 0);
    }

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  spr_affine — affine-transformed sprite drawing                     */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_affine_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         found;

    dtr_gfx_cls(gfx, 0);

    /* Fill sheet with colour 6 */
    memset(sheet_data, 6, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Identity affine: rot_x=1, rot_y=0 → no rotation */
    dtr_gfx_spr_affine(gfx, 0, 4, 4, 4.0f, 4.0f, 1.0f, 0.0f);

    /* Some pixels should have been drawn */
    found = 0;
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        if (gfx->framebuffer[idx] == 6) {
            ++found;
        }
    }
    DTR_ASSERT(found > 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_affine_null_sheet(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Should not crash with no sheet */
    dtr_gfx_spr_affine(gfx, 0, 4, 4, 4.0f, 4.0f, 1.0f, 0.0f);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_affine_zero_det(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 7, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Zero determinant (rot_x=0, rot_y=0) → nothing should draw */
    dtr_gfx_spr_affine(gfx, 0, 4, 4, 4.0f, 4.0f, 0.0f, 0.0f);

    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[idx], 0);
    }

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Tilemap                                                            */
/* ------------------------------------------------------------------ */

static void test_gfx_tilemap_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* 2x2 tile map, 8x8 tiles */
    uint8_t tiles[]  = {0, 1, 2, 0};
    uint8_t colors[] = {1, 2, 3};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_tilemap(gfx, tiles, 2, 2, 8, 8, colors, 3);

    /* Tile (0,0) = index 0 → colour 1 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 1);
    /* Tile (1,0) = index 1 → colour 2 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 0), 2);
    /* Tile (0,1) = index 2 → colour 3 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 8), 3);
    /* Tile (1,1) = index 0 → colour 1 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 8), 1);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Screen palette (display pal)                                       */
/* ------------------------------------------------------------------ */

static void test_gfx_screen_pal(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* Remap colour 1 → 5 in screen palette */
    dtr_gfx_pal(gfx, 1, 5, true);
    DTR_ASSERT_EQ_INT(gfx->screen_pal[1], 5);

    /* Reset should restore identity */
    dtr_gfx_pal_reset(gfx);
    DTR_ASSERT_EQ_INT(gfx->screen_pal[1], 1);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Hex sheet loading                                                  */
/* ------------------------------------------------------------------ */

static void test_gfx_load_sheet_hex_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;

    /* 4×2 sheet, tile 2×2: "00010203 04050607" = 8 pixels, 16 hex chars */
    ret = dtr_gfx_load_sheet_hex(gfx, "0001020304050607", 16, 4, 2, 2, 2);
    DTR_ASSERT(ret);
    DTR_ASSERT_EQ_INT(gfx->sheet.width, 4);
    DTR_ASSERT_EQ_INT(gfx->sheet.height, 2);
    DTR_ASSERT_EQ_INT(gfx->sheet.tile_w, 2);
    DTR_ASSERT_EQ_INT(gfx->sheet.tile_h, 2);
    DTR_ASSERT_EQ_INT(gfx->sheet.cols, 2);
    DTR_ASSERT_EQ_INT(gfx->sheet.rows, 1);
    DTR_ASSERT_EQ_INT(gfx->sheet.count, 2);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[0], 0x00);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[1], 0x01);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[7], 0x07);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_sheet_hex_bad_length(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;

    /* Too short */
    ret = dtr_gfx_load_sheet_hex(gfx, "0001", 4, 4, 2, 2, 2);
    DTR_ASSERT(!ret);

    /* Too long */
    ret = dtr_gfx_load_sheet_hex(gfx, "000102030405060708", 18, 4, 2, 2, 2);
    DTR_ASSERT(!ret);

    /* Odd length */
    ret = dtr_gfx_load_sheet_hex(gfx, "00010", 5, 4, 2, 2, 2);
    DTR_ASSERT(!ret);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_sheet_hex_bad_char(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;

    /* 'g' is not a valid hex char */
    ret = dtr_gfx_load_sheet_hex(gfx, "00010203040506g7", 16, 4, 2, 2, 2);
    DTR_ASSERT(!ret);

    /* Sheet pixels should not have been modified on failure */
    DTR_ASSERT(gfx->sheet.pixels == NULL);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_sheet_hex_null(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;

    ret = dtr_gfx_load_sheet_hex(gfx, NULL, 0, 4, 2, 2, 2);
    DTR_ASSERT(!ret);

    /* Zero dimensions */
    ret = dtr_gfx_load_sheet_hex(gfx, "0001020304050607", 16, 0, 2, 2, 2);
    DTR_ASSERT(!ret);

    ret = dtr_gfx_load_sheet_hex(gfx, "0001020304050607", 16, 4, 0, 2, 2);
    DTR_ASSERT(!ret);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_sheet_hex_uppercase(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;

    /* Mixed case: 0A0B0C0D */
    ret = dtr_gfx_load_sheet_hex(gfx, "0A0b0C0d", 8, 2, 2, 2, 2);
    DTR_ASSERT(ret);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[0], 0x0A);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[1], 0x0B);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[2], 0x0C);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[3], 0x0D);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_sheet_hex_trailing_whitespace(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;

    /* 4×2 sheet with trailing newline — should be stripped */
    ret = dtr_gfx_load_sheet_hex(gfx, "0001020304050607\n", 17, 4, 2, 2, 2);
    DTR_ASSERT(ret);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[0], 0x00);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[7], 0x07);

    /* Trailing \r\n */
    ret = dtr_gfx_load_sheet_hex(gfx, "ff00ff00ff00ff00\r\n", 18, 4, 2, 2, 2);
    DTR_ASSERT(ret);
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[0], 0xFF);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Flags hex loading                                                  */
/* ------------------------------------------------------------------ */

static void test_gfx_load_flags_hex_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;
    char            hex[CONSOLE_MAX_SPRITES * 2 + 1];

    /* Build a hex string: flag[0]=0xAB, flag[1]=0xCD, rest=00 */
    memset(hex, '0', CONSOLE_MAX_SPRITES * 2);
    hex[0]                       = 'a';
    hex[1]                       = 'b';
    hex[2]                       = 'c';
    hex[3]                       = 'd';
    hex[CONSOLE_MAX_SPRITES * 2] = '\0';

    ret = dtr_gfx_load_flags_hex(gfx, hex, CONSOLE_MAX_SPRITES * 2);
    DTR_ASSERT(ret);
    DTR_ASSERT_EQ_INT(gfx->sheet.flags[0], 0xAB);
    DTR_ASSERT_EQ_INT(gfx->sheet.flags[1], 0xCD);
    DTR_ASSERT_EQ_INT(gfx->sheet.flags[2], 0x00);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_flags_hex_bad_length(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    DTR_ASSERT(!dtr_gfx_load_flags_hex(gfx, "00", 2));
    DTR_ASSERT(!dtr_gfx_load_flags_hex(gfx, NULL, 0));

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_flags_hex_trailing_newline(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    bool            ret;
    char            hex[CONSOLE_MAX_SPRITES * 2 + 2];

    memset(hex, '0', CONSOLE_MAX_SPRITES * 2);
    hex[0]                           = 'f';
    hex[1]                           = 'f';
    hex[CONSOLE_MAX_SPRITES * 2]     = '\n';
    hex[CONSOLE_MAX_SPRITES * 2 + 1] = '\0';

    ret = dtr_gfx_load_flags_hex(gfx, hex, CONSOLE_MAX_SPRITES * 2 + 1);
    DTR_ASSERT(ret);
    DTR_ASSERT_EQ_INT(gfx->sheet.flags[0], 0xFF);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Sprite + fill pattern                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_fill_pattern(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);

    /* Fill the entire tile with colour 3 */
    memset(sheet_data, 3, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Checkerboard fill pattern — half of pixels should be masked */
    dtr_gfx_fillp(gfx, 0x5A5A);

    dtr_gfx_spr(gfx, 0, 0, 0, 1, 1, false, false);

    /* Count drawn pixels in the 8x8 tile area */
    int count = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (dtr_gfx_pget(gfx, x, y) == 3)
                ++count;
        }
    }
    /* Checkerboard pattern should mask exactly half the pixels */
    DTR_ASSERT_EQ_INT(count, 32);

    dtr_gfx_fillp(gfx, 0);
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Sprite + camera (exercises fast path)                              */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_with_camera(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 0, sizeof(sheet_data));
    sheet_data[0] = 5; /* top-left pixel */

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Camera offset shifts the draw position */
    dtr_gfx_camera(gfx, 3, 2);

    /* Draw at world (3, 2) → screen (0, 0) */
    dtr_gfx_spr(gfx, 0, 3, 2, 1, 1, false, false);

    /* pget also applies camera, so use framebuffer directly */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[0 * TW + 0], 5);
    /* Screen position (3, 2) should be untouched */
    DTR_ASSERT_EQ_INT(gfx->framebuffer[2 * TW + 3], 0);

    dtr_gfx_camera(gfx, 0, 0);
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Sprite fast-path clipping (1x1 tile partially off-screen)          */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_fast_path_clip(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 7, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Draw 1x1 sprite at x=TW-4 — only 4 columns should be visible */
    dtr_gfx_spr(gfx, 0, TW - 4, 0, 1, 1, false, false);

    /* Visible columns: x = TW-4 .. TW-1 */
    for (int y = 0; y < 8; ++y) {
        for (int x = TW - 4; x < TW; ++x) {
            DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, x, y), 7);
        }
    }

    /* Column just before the sprite should be untouched */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, TW - 5, 0), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  sspr — scale up (4x4 → 8x8)                                       */
/* ------------------------------------------------------------------ */

static void test_gfx_sspr_scale_up(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 6, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;

    /* Scale 4x4 source → 8x8 destination at (0, 0) */
    dtr_gfx_sspr(gfx, 0, 0, 4, 4, 0, 0, 8, 8);

    /* All 8x8 destination pixels should be colour 6 */
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, x, y), 6);
        }
    }

    /* Pixel just outside should be 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 0), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  sspr — scale down (8x8 → 4x4)                                     */
/* ------------------------------------------------------------------ */

static void test_gfx_sspr_scale_down(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 9, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;

    /* Scale 8x8 source → 4x4 destination at (2, 2) */
    dtr_gfx_sspr(gfx, 0, 0, 8, 8, 2, 2, 4, 4);

    /* All 4x4 destination pixels should be colour 9 */
    for (int y = 2; y < 6; ++y) {
        for (int x = 2; x < 6; ++x) {
            DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, x, y), 9);
        }
    }

    /* Just outside should be 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 1, 2), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 6, 2), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  sspr — clip (destination extends past framebuffer edge)            */
/* ------------------------------------------------------------------ */

static void test_gfx_sspr_clip(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 2, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;

    /* Destination extends 4 pixels past right edge — should not crash */
    dtr_gfx_sspr(gfx, 0, 0, 8, 8, TW - 4, 0, 8, 8);

    /* Visible part: x = TW-4 .. TW-1 */
    for (int y = 0; y < 8; ++y) {
        for (int x = TW - 4; x < TW; ++x) {
            DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, x, y), 2);
        }
    }

    /* Just before the visible region */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, TW - 5, 0), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  sspr — zero/negative dimensions (safety)                           */
/* ------------------------------------------------------------------ */

static void test_gfx_sspr_zero_dims(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 4, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;

    /* Zero source width — should be a no-op */
    dtr_gfx_sspr(gfx, 0, 0, 0, 4, 0, 0, 4, 4);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);

    /* Zero destination height — should be a no-op */
    dtr_gfx_sspr(gfx, 0, 0, 4, 4, 0, 0, 4, 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  map_draw (batch tilemap blit from sprite sheet)                    */
/* ------------------------------------------------------------------ */

static void test_gfx_map_draw_basic(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* 16x16 sheet, one 8x8 tile filled with colour 5 */
    uint8_t sheet_data[16 * 16];

    memset(sheet_data, 0, sizeof(sheet_data));
    /* Fill tile 0 (top-left 8x8 region) with colour 5 */
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            sheet_data[r * 16 + c] = 5;
        }
    }
    /* Fill tile 1 (top-right 8x8 region) with colour 7 */
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            sheet_data[r * 16 + 8 + c] = 7;
        }
    }

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 16;
    gfx->sheet.height = 16;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 2;
    gfx->sheet.rows   = 2;
    gfx->sheet.count  = 4;

    /* 2x1 map: tile indices 1-based (1 = sprite 0, 2 = sprite 1) */
    int32_t tiles[] = {1, 2};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_map_draw(gfx, tiles, 2, 1, 0, 0, 2, 1, 0, 0, 8, 8);

    /* Tile 0 → colour 5 at (0,0) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 5);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 7, 7), 5);
    /* Tile 1 → colour 7 at (8,0) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 0), 7);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 15, 7), 7);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_map_draw_empty_tiles(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    memset(sheet_data, 3, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Tile value 0 = empty, should not draw anything */
    int32_t tiles[] = {0, 0, 0, 0};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_map_draw(gfx, tiles, 2, 2, 0, 0, 2, 2, 0, 0, 8, 8);

    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 8), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_map_draw_null_inputs(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    dtr_gfx_cls(gfx, 0);

    /* NULL tiles — should not crash */
    dtr_gfx_map_draw(gfx, NULL, 2, 2, 0, 0, 2, 2, 0, 0, 8, 8);

    /* NULL sheet — should not crash */
    int32_t tiles[] = {1};
    dtr_gfx_map_draw(gfx, tiles, 1, 1, 0, 0, 1, 1, 0, 0, 8, 8);

    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_map_draw_with_camera(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    memset(sheet_data, 9, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    int32_t tiles[] = {1};

    /* Camera offset shifts tile left by 4 pixels */
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_camera(gfx, 4, 0);
    dtr_gfx_map_draw(gfx, tiles, 1, 1, 0, 0, 1, 1, 0, 0, 8, 8);

    /* Tile at world (0,0) drawn at screen (-4,0); pget uses world coords.
     * World x=4 → screen x=0 → visible, filled with colour 9.
     * World x=7 → screen x=3 → visible.
     * World x=8 → screen x=4 → past the tile → should be 0.            */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 4, 0), 9);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 7, 0), 9);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 0), 0);

    dtr_gfx_camera(gfx, 0, 0);
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    DTR_TEST_BEGIN("test_graphics");

    DTR_RUN_TEST(test_gfx_create_destroy);
    DTR_RUN_TEST(test_gfx_destroy_null);
    DTR_RUN_TEST(test_gfx_cls);
    DTR_RUN_TEST(test_gfx_pset_pget);
    DTR_RUN_TEST(test_gfx_pget_out_of_bounds);
    DTR_RUN_TEST(test_gfx_pset_clipped);
    DTR_RUN_TEST(test_gfx_camera);
    DTR_RUN_TEST(test_gfx_clip);
    DTR_RUN_TEST(test_gfx_draw_pal);
    DTR_RUN_TEST(test_gfx_transparency);
    DTR_RUN_TEST(test_gfx_color);
    DTR_RUN_TEST(test_gfx_cursor);
    DTR_RUN_TEST(test_gfx_line_horizontal);
    DTR_RUN_TEST(test_gfx_line_vertical);
    DTR_RUN_TEST(test_gfx_line_diagonal_clip);
    DTR_RUN_TEST(test_gfx_line_single_point);
    DTR_RUN_TEST(test_gfx_rectfill);
    DTR_RUN_TEST(test_gfx_rect_outline);
    DTR_RUN_TEST(test_gfx_circ_basic);
    DTR_RUN_TEST(test_gfx_circfill_basic);
    DTR_RUN_TEST(test_gfx_circ_at_boundary);
    DTR_RUN_TEST(test_gfx_circ_zero_radius);
    DTR_RUN_TEST(test_gfx_rectfill_clipped);
    DTR_RUN_TEST(test_gfx_rect_fully_outside);
    DTR_RUN_TEST(test_gfx_sprite_flags);
    DTR_RUN_TEST(test_gfx_sprite_flags_oob);
    DTR_RUN_TEST(test_gfx_fill_pattern);
    DTR_RUN_TEST(test_gfx_flip);
    DTR_RUN_TEST(test_gfx_reset);
    DTR_RUN_TEST(test_gfx_poly_outline);
    DTR_RUN_TEST(test_gfx_polyfill);
    DTR_RUN_TEST(test_gfx_poly_degenerate);
    DTR_RUN_TEST(test_gfx_font_custom_and_reset);
    DTR_RUN_TEST(test_gfx_fade);
    DTR_RUN_TEST(test_gfx_wipe);
    DTR_RUN_TEST(test_gfx_dissolve);
    DTR_RUN_TEST(test_gfx_transition_clamp_frames);
    DTR_RUN_TEST(test_gfx_dl_begin_end);
    DTR_RUN_TEST(test_gfx_dl_queue_and_sort);
    DTR_RUN_TEST(test_gfx_dl_overflow);
    DTR_RUN_TEST(test_gfx_tri_outline);
    DTR_RUN_TEST(test_gfx_trifill);
    DTR_RUN_TEST(test_gfx_trifill_clipped);
    DTR_RUN_TEST(test_gfx_print_basic);
    DTR_RUN_TEST(test_gfx_print_newline);
    DTR_RUN_TEST(test_gfx_print_empty);
    DTR_RUN_TEST(test_gfx_spr_basic);
    DTR_RUN_TEST(test_gfx_spr_null_sheet);
    DTR_RUN_TEST(test_gfx_spr_out_of_range);
    DTR_RUN_TEST(test_gfx_spr_flip);
    DTR_RUN_TEST(test_gfx_sspr_basic);
    DTR_RUN_TEST(test_gfx_sspr_null_sheet);
    DTR_RUN_TEST(test_gfx_spr_rot_basic);
    DTR_RUN_TEST(test_gfx_spr_rot_null_sheet);
    DTR_RUN_TEST(test_gfx_spr_rot_out_of_range);
    DTR_RUN_TEST(test_gfx_spr_affine_basic);
    DTR_RUN_TEST(test_gfx_spr_affine_null_sheet);
    DTR_RUN_TEST(test_gfx_spr_affine_zero_det);
    DTR_RUN_TEST(test_gfx_tilemap_basic);
    DTR_RUN_TEST(test_gfx_screen_pal);
    DTR_RUN_TEST(test_gfx_load_sheet_hex_basic);
    DTR_RUN_TEST(test_gfx_load_sheet_hex_bad_length);
    DTR_RUN_TEST(test_gfx_load_sheet_hex_bad_char);
    DTR_RUN_TEST(test_gfx_load_sheet_hex_null);
    DTR_RUN_TEST(test_gfx_load_sheet_hex_uppercase);
    DTR_RUN_TEST(test_gfx_load_sheet_hex_trailing_whitespace);
    DTR_RUN_TEST(test_gfx_load_flags_hex_basic);
    DTR_RUN_TEST(test_gfx_load_flags_hex_bad_length);
    DTR_RUN_TEST(test_gfx_load_flags_hex_trailing_newline);
    DTR_RUN_TEST(test_gfx_spr_fill_pattern);
    DTR_RUN_TEST(test_gfx_spr_with_camera);
    DTR_RUN_TEST(test_gfx_spr_fast_path_clip);
    DTR_RUN_TEST(test_gfx_sspr_scale_up);
    DTR_RUN_TEST(test_gfx_sspr_scale_down);
    DTR_RUN_TEST(test_gfx_sspr_clip);
    DTR_RUN_TEST(test_gfx_sspr_zero_dims);
    DTR_RUN_TEST(test_gfx_map_draw_basic);
    DTR_RUN_TEST(test_gfx_map_draw_empty_tiles);
    DTR_RUN_TEST(test_gfx_map_draw_null_inputs);
    DTR_RUN_TEST(test_gfx_map_draw_with_camera);

    DTR_TEST_END();
}
