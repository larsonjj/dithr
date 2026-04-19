/**
 * \file            test_graphics.c
 * \brief           Unit tests for the graphics subsystem
 */

#include "font.h"
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
/*  Spritesheet pixel access (sget / sset)                             */
/* ------------------------------------------------------------------ */

DTR_GFX_TEST(test_gfx_sget_sset_basic, TW, TH)
{
    uint8_t sheet_data[8 * 8];

    memset(sheet_data, 0, sizeof(sheet_data));
    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;

    /* Default pixel should be 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, 0, 0), 0);

    /* Set and get a pixel */
    dtr_gfx_sset(gfx, 3, 4, 7);
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, 3, 4), 7);

    /* Other pixels unchanged */
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, 0, 0), 0);

    gfx->sheet.pixels = NULL;
    DTR_PASS();
}

DTR_GFX_TEST(test_gfx_sget_oob, TW, TH)
{
    uint8_t sheet_data[8 * 8];

    memset(sheet_data, 5, sizeof(sheet_data));
    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;

    /* Out-of-bounds reads return 0 */
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, -1, 0), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, 0, -1), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, 8, 0), 0);
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, 0, 8), 0);

    gfx->sheet.pixels = NULL;
    DTR_PASS();
}

DTR_GFX_TEST(test_gfx_sset_oob, TW, TH)
{
    uint8_t sheet_data[8 * 8];

    memset(sheet_data, 0, sizeof(sheet_data));
    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;

    /* Out-of-bounds writes should not crash or corrupt memory */
    dtr_gfx_sset(gfx, -1, 0, 5);
    dtr_gfx_sset(gfx, 0, -1, 5);
    dtr_gfx_sset(gfx, 8, 0, 5);
    dtr_gfx_sset(gfx, 0, 8, 5);

    /* All pixels should still be 0 */
    for (int i = 0; i < 8 * 8; ++i) {
        DTR_ASSERT_EQ_INT(sheet_data[i], 0);
    }

    gfx->sheet.pixels = NULL;
    DTR_PASS();
}

DTR_GFX_TEST(test_gfx_sget_null_sheet, TW, TH)
{
    /* With no sheet loaded, sget returns 0 and sset doesn't crash */
    DTR_ASSERT_EQ_INT(dtr_gfx_sget(gfx, 0, 0), 0);
    dtr_gfx_sset(gfx, 0, 0, 5);
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
/*  Draw list overflow — each command type caps at max                  */
/* ------------------------------------------------------------------ */

static void test_gfx_dl_overflow_sspr(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_dl_begin(gfx);
    for (int i = 0; i < CONSOLE_MAX_DRAW_CMDS; ++i) {
        dtr_gfx_dl_sspr(gfx, 0, 0, 0, 1, 1, 0, 0, 1, 1);
    }
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    /* Overflow */
    dtr_gfx_dl_sspr(gfx, 0, 0, 0, 1, 1, 0, 0, 1, 1);
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    gfx->draw_list.count = 0;
    dtr_gfx_dl_end(gfx);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_overflow_spr_rot(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_dl_begin(gfx);
    for (int i = 0; i < CONSOLE_MAX_DRAW_CMDS; ++i) {
        dtr_gfx_dl_spr_rot(gfx, 0, 0, 0, 0, 0.0f, 0, 0);
    }
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    dtr_gfx_dl_spr_rot(gfx, 0, 0, 0, 0, 0.0f, 0, 0);
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    gfx->draw_list.count = 0;
    dtr_gfx_dl_end(gfx);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_overflow_spr_affine(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_dl_begin(gfx);
    for (int i = 0; i < CONSOLE_MAX_DRAW_CMDS; ++i) {
        dtr_gfx_dl_spr_affine(gfx, 0, 0, 0, 0, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    dtr_gfx_dl_spr_affine(gfx, 0, 0, 0, 0, 0.0f, 0.0f, 1.0f, 0.0f);
    DTR_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    gfx->draw_list.count = 0;
    dtr_gfx_dl_end(gfx);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Flip with larger framebuffer — exercises SIMD tail handling         */
/* ------------------------------------------------------------------ */

static void test_gfx_flip_odd_size(void)
{
    /* Non-multiple-of-4 pixel count to test SIMD tail loop */
    dtr_graphics_t *gfx = dtr_gfx_create(13, 7);

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_pset(gfx, 0, 0, 1);
    dtr_gfx_pset(gfx, 12, 6, 7);
    dtr_gfx_flip(gfx);

    /* Verify the RGBA output matches palette lookup */
    DTR_ASSERT(gfx->pixels[0] != 0);                   /* color 1 → non-black */
    DTR_ASSERT(gfx->pixels[12 + 6 * 13] != 0);         /* color 7 → non-black */
    DTR_ASSERT_EQ_INT(gfx->pixels[1], gfx->pixels[2]); /* both color 0 → same */

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Polygon with fewer than 3 vertices — edge cases                    */
/* ------------------------------------------------------------------ */

static void test_gfx_polyfill_single_point(void)
{
    dtr_graphics_t *gfx   = dtr_gfx_create(TW, TH);
    int32_t         pts[] = {5, 5};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_polyfill(gfx, pts, 1, 3);

    /* 1 vertex — nothing drawn */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 5, 5), 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_polyfill_two_points(void)
{
    dtr_graphics_t *gfx   = dtr_gfx_create(TW, TH);
    int32_t         pts[] = {2, 2, 10, 2};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_polyfill(gfx, pts, 2, 5);

    /* 2 vertices — draws a line */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 2, 2), 5);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 10, 2), 5);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Circle with negative radius                                        */
/* ------------------------------------------------------------------ */

static void test_gfx_circ_negative_radius(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_cls(gfx, 0);

    /* Negative radius — should not crash */
    dtr_gfx_circ(gfx, 8, 8, -3, 5);
    dtr_gfx_circfill(gfx, 8, 8, -3, 5);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Fill pattern with all patterns active                               */
/* ------------------------------------------------------------------ */

static void test_gfx_fill_pattern_all(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Solid fill pattern (all bits set) — all pixels drawn */
    dtr_gfx_fillp(gfx, 0xFFFF);
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_rectfill(gfx, 0, 0, 3, 3, 5);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 5);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 3, 3), 5);

    /* Checkerboard pattern — alternating pixels drawn */
    dtr_gfx_fillp(gfx, 0x5555);
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_rectfill(gfx, 0, 0, 3, 3, 7);
    /* bit 0 is set → (0,0) drawn */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 7);
    /* bit 1 is not set → (1,0) not drawn */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 1, 0), 0);

    /* Reset to solid */
    dtr_gfx_fillp(gfx, 0);

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

/* Verify that flip_x with a wide tile (16px) produces the same result
 * via the prv_blit_span_flip fast path as the scalar fallback.  Uses a
 * gradient pattern so every column is distinguishable. */
static void test_gfx_spr_flip_x_wide(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(32, 16);
    uint8_t         sheet_data[16 * 8];

    memset(sheet_data, 0, sizeof(sheet_data));
    /* Fill first row with a gradient: col 0→1, 1→2, ... 15→16 */
    for (int c = 0; c < 16; ++c) {
        sheet_data[c] = (uint8_t)(c + 1);
    }

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 16;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 16;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Draw unflipped at row 0 */
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_spr(gfx, 0, 0, 0, 1, 1, false, false);

    /* Draw flipped at row 8 */
    dtr_gfx_spr(gfx, 0, 0, 8, 1, 1, true, false);

    /* Verify every column is mirrored */
    for (int c = 0; c < 16; ++c) {
        uint8_t normal  = dtr_gfx_pget(gfx, c, 0);
        uint8_t flipped = dtr_gfx_pget(gfx, 15 - c, 8);
        DTR_ASSERT_EQ_INT(normal, flipped);
    }

    /* Verify first pixel of each row specifically */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 1);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 15, 8), 1);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 8), 16);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  spr_batch                                                          */
/* ------------------------------------------------------------------ */

/* Verify that spr_batch produces the same output as individual spr calls */
static void test_gfx_spr_batch_matches_individual(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(32, 16);
    uint8_t         sheet_data[8 * 8];
    uint8_t         expected[32 * 16];
    int32_t         batch_data[3 * 5]; /* 3 sprites × 5 ints each */

    /* Sheet: fill with colour 5 */
    memset(sheet_data, 5, sizeof(sheet_data));
    sheet_data[0] = 3; /* top-left pixel distinct */

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Draw 3 sprites individually and save the framebuffer */
    dtr_gfx_cls(gfx, 0);
    dtr_gfx_spr(gfx, 0, 0, 0, 1, 1, false, false);
    dtr_gfx_spr(gfx, 0, 10, 0, 1, 1, true, false);
    dtr_gfx_spr(gfx, 0, 20, 5, 1, 1, false, true);
    memcpy(expected, gfx->framebuffer, sizeof(expected));

    /* Draw the same 3 sprites via batch and compare */
    dtr_gfx_cls(gfx, 0);
    batch_data[0]  = 0;
    batch_data[1]  = 0;
    batch_data[2]  = 0;
    batch_data[3]  = 0;
    batch_data[4]  = 0; /* no flip */
    batch_data[5]  = 0;
    batch_data[6]  = 10;
    batch_data[7]  = 0;
    batch_data[8]  = 1;
    batch_data[9]  = 0; /* flip_x */
    batch_data[10] = 0;
    batch_data[11] = 20;
    batch_data[12] = 5;
    batch_data[13] = 0;
    batch_data[14] = 1; /* flip_y */
    dtr_gfx_spr_batch(gfx, batch_data, 3);

    for (int i = 0; i < 32 * 16; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], expected[i]);
    }

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* Verify batch with NULL/zero input doesn't crash */
static void test_gfx_spr_batch_empty(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_spr_batch(gfx, NULL, 0);
    dtr_gfx_spr_batch(gfx, NULL, 5);

    /* Framebuffer should be untouched */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 0);

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
/*  spr_rot — fill pattern and camera paths                            */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_rot_with_pattern(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         drawn;
    int32_t         blank;

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

    /* Checkerboard fill pattern */
    gfx->fill_pattern = 0x5A5A;
    dtr_gfx_spr_rot(gfx, 0, 4, 4, 0.0f, 4, 4);

    /* Some pixels drawn, some masked by pattern */
    drawn = 0;
    blank = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 5) {
            ++drawn;
        } else {
            ++blank;
        }
    }
    DTR_ASSERT(drawn > 0);
    DTR_ASSERT(blank > 0);

    gfx->fill_pattern = 0;
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_rot_with_camera(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         found;

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

    /* Camera shifts drawing; some pixels should still appear */
    dtr_gfx_camera(gfx, 2, 2);
    dtr_gfx_spr_rot(gfx, 0, 4, 4, 0.5f, 4, 4);

    found = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 4) {
            ++found;
        }
    }
    DTR_ASSERT(found > 0);

    dtr_gfx_camera(gfx, 0, 0);
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_rot_offscreen(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 3, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Entirely off-screen: early reject path */
    dtr_gfx_spr_rot(gfx, 0, -100, -100, 0.0f, 4, 4);

    for (int32_t i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  spr_affine — fill pattern and camera paths                         */
/* ------------------------------------------------------------------ */

static void test_gfx_spr_affine_with_pattern(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         drawn;
    int32_t         blank;

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 8, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    gfx->fill_pattern = 0x5A5A;
    dtr_gfx_spr_affine(gfx, 0, 4, 4, 4.0f, 4.0f, 1.0f, 0.0f);

    drawn = 0;
    blank = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 8) {
            ++drawn;
        } else {
            ++blank;
        }
    }
    DTR_ASSERT(drawn > 0);
    DTR_ASSERT(blank > 0);

    gfx->fill_pattern = 0;
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_affine_with_camera(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         found;

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

    dtr_gfx_camera(gfx, 3, 3);
    dtr_gfx_spr_affine(gfx, 0, 4, 4, 4.0f, 4.0f, 1.0f, 0.0f);

    found = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 2) {
            ++found;
        }
    }
    DTR_ASSERT(found > 0);

    dtr_gfx_camera(gfx, 0, 0);
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_affine_offscreen(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 1, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* Entirely off-screen */
    dtr_gfx_spr_affine(gfx, 0, -100, -100, 4.0f, 4.0f, 1.0f, 0.0f);

    for (int32_t i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_spr_affine_rotated(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         found;

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 10, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* 45-degree rotation: rot_x=cos(45°)≈0.707, rot_y=sin(45°)≈0.707 */
    dtr_gfx_spr_affine(gfx, 0, 8, 8, 4.0f, 4.0f, 0.707f, 0.707f);

    found = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 10) {
            ++found;
        }
    }
    DTR_ASSERT(found > 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  sspr — fill pattern and camera paths                               */
/* ------------------------------------------------------------------ */

static void test_gfx_sspr_with_pattern(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         drawn;
    int32_t         blank;

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 6, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    gfx->fill_pattern = 0x5A5A;
    dtr_gfx_sspr(gfx, 0, 0, 8, 8, 0, 0, 8, 8);

    drawn = 0;
    blank = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 6) {
            ++drawn;
        } else {
            ++blank;
        }
    }
    DTR_ASSERT(drawn > 0);
    DTR_ASSERT(blank > 0);

    gfx->fill_pattern = 0;
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_sspr_with_camera(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    dtr_gfx_cls(gfx, 0);
    memset(sheet_data, 11, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    dtr_gfx_camera(gfx, 4, 4);
    dtr_gfx_sspr(gfx, 0, 0, 8, 8, 0, 0, 8, 8);

    /* Drawn at screen (-4,-4) to (3,3); pixel at world (4,4)→screen (0,0) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 4, 4), 11);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 7, 7), 11);
    /* World (8,8) → screen (4,4): past the 8x8 dest region */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 8, 8), 0);

    dtr_gfx_camera(gfx, 0, 0);
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_sspr_offscreen(void)
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

    dtr_gfx_sspr(gfx, 0, 0, 8, 8, -20, -20, 8, 8);

    for (int32_t i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  map_draw — fill pattern and transparency paths                     */
/* ------------------------------------------------------------------ */

static void test_gfx_map_draw_transparency(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    /* Fill sheet with colour 0 (transparent by default) */
    memset(sheet_data, 0, sizeof(sheet_data));
    /* Set a few pixels to non-transparent colour */
    sheet_data[0] = 5;

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    int32_t tiles[] = {1};

    dtr_gfx_cls(gfx, 7);
    dtr_gfx_map_draw(gfx, tiles, 1, 1, 0, 0, 1, 1, 0, 0, 8, 8);

    /* Pixel (0,0) has colour 5 from sprite */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 5);
    /* Pixel (1,0) was transparent (colour 0), so background 7 shows through */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 1, 0), 7);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_map_draw_with_pattern(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         drawn;
    int32_t         bg;

    memset(sheet_data, 3, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    int32_t tiles[] = {1};

    dtr_gfx_cls(gfx, 0);
    gfx->fill_pattern = 0x5A5A;
    dtr_gfx_map_draw(gfx, tiles, 1, 1, 0, 0, 1, 1, 0, 0, 8, 8);

    drawn = 0;
    bg    = 0;
    for (int32_t y = 0; y < 8; ++y) {
        for (int32_t x = 0; x < 8; ++x) {
            if (gfx->framebuffer[y * TW + x] == 3) {
                ++drawn;
            } else {
                ++bg;
            }
        }
    }
    DTR_ASSERT(drawn > 0);
    DTR_ASSERT(bg > 0);

    gfx->fill_pattern = 0;
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_map_draw_partial_clip(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];

    memset(sheet_data, 4, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* 3x3 tile map drawn at (-4,-4): partially off-screen */
    int32_t tiles[] = {1, 1, 1, 1, 1, 1, 1, 1, 1};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_map_draw(gfx, tiles, 3, 3, 0, 0, 3, 3, -4, -4, 8, 8);

    /* Top-left tile is mostly off-screen, but pixels at (4,4) visible */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 4);
    /* Bottom-right corner of last tile at (19,19): off-screen (fb=16x16) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 15, 15), 4);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  print — camera and clipping paths                                  */
/* ------------------------------------------------------------------ */

static void test_gfx_print_with_camera(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         found;

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_camera(gfx, 2, 0);
    dtr_gfx_print(gfx, "A", 0, 0, 7);

    /* Some pixels should still be visible */
    found = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 7) {
            ++found;
        }
    }
    DTR_ASSERT(found > 0);

    dtr_gfx_camera(gfx, 0, 0);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_print_offscreen(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_print(gfx, "A", -100, -100, 7);

    /* Entirely off-screen: no pixels drawn */
    for (int32_t i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_print_with_pattern(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         drawn;
    int32_t         blank;

    dtr_gfx_cls(gfx, 0);
    gfx->fill_pattern = 0x5A5A;
    dtr_gfx_print(gfx, "W", 0, 0, 5);

    drawn = 0;
    blank = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 5) {
            ++drawn;
        } else {
            ++blank;
        }
    }
    /* Pattern should mask some pixels */
    DTR_ASSERT(drawn > 0);
    DTR_ASSERT(blank > 0);

    gfx->fill_pattern = 0;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Print — custom font actually renders pixels                        */
/* ------------------------------------------------------------------ */

static void test_gfx_print_custom_font_draws(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[16 * 8];
    int32_t         count;

    dtr_gfx_cls(gfx, 0);

    /* Build a 16×8 sheet with two 8×8 character cells.
     * Cell 0 = space (blank), Cell 1 = 'A' (filled). */
    memset(sheet_data, 0, sizeof(sheet_data));
    /* Fill the second cell (columns 8-15) with colour 5 */
    for (int32_t row = 0; row < 8; ++row) {
        for (int32_t col = 8; col < 16; ++col) {
            sheet_data[row * 16 + col] = 5;
        }
    }

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 16;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 2;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 2;

    /* Custom font: 8×8, starting at ASCII ' ' (32), 2 chars */
    dtr_gfx_font(gfx, 0, 0, 8, 8, ' ', 2);

    /* Print '!' which is font index 1 → the filled cell */
    dtr_gfx_print(gfx, "!", 0, 0, 5);

    count = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 5) {
            ++count;
        }
    }
    DTR_ASSERT(count > 0);

    dtr_gfx_font_reset(gfx);
    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Print — out-of-range characters produce no pixels                  */
/* ------------------------------------------------------------------ */

static void test_gfx_print_special_chars(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_cls(gfx, 0);

    /* Characters outside printable ASCII (DEL=127, tab=9) should be skipped */
    dtr_gfx_print(gfx, "\x7F\x01\x09", 0, 0, 7);

    for (int32_t i = 0; i < TW * TH; ++i) {
        DTR_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Print — draw palette remap applies to text colour                  */
/* ------------------------------------------------------------------ */

static void test_gfx_print_pal_remap(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         col7;
    int32_t         col3;

    dtr_gfx_cls(gfx, 0);

    /* Remap colour 7 → 3 for drawing */
    dtr_gfx_pal(gfx, 7, 3, false);
    dtr_gfx_print(gfx, "A", 0, 0, 7);

    col7 = 0;
    col3 = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 7) {
            ++col7;
        }
        if (gfx->framebuffer[i] == 3) {
            ++col3;
        }
    }
    /* Colour 7 should NOT appear; colour 3 should have the glyph pixels */
    DTR_ASSERT_EQ_INT(col7, 0);
    DTR_ASSERT(col3 > 0);

    /* Reset palette */
    dtr_gfx_pal(gfx, 7, 7, false);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Print — text wrapping at framebuffer right edge                    */
/* ------------------------------------------------------------------ */

static void test_gfx_print_right_edge(void)
{
    /* FB width = 16; built-in font is 4px wide + 1px gap = 5px per char.
     * Printing 4 characters = 20px total → last char extends past edge.
     * Characters that start off-screen should be clipped but not crash. */
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         adv;
    int32_t         count;

    dtr_gfx_cls(gfx, 0);
    adv = dtr_gfx_print(gfx, "AAAA", 0, 0, 7);

    DTR_ASSERT(adv > 0);

    count = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 7) {
            ++count;
        }
    }
    /* Some pixels drawn, but not all 4 chars fully visible */
    DTR_ASSERT(count > 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Print — very long multi-line string                                */
/* ------------------------------------------------------------------ */

static void test_gfx_print_multiline_stress(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         count;

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_print(gfx, "A\nB\nC\nD\nE\nF", 0, 0, 7);

    /* 6 lines × DTR_FONT_H=6 = 36px; FB height=16 → last lines clipped */
    count = 0;
    for (int32_t i = 0; i < TW * TH; ++i) {
        if (gfx->framebuffer[i] == 7) {
            ++count;
        }
    }
    /* At least some pixels visible */
    DTR_ASSERT(count > 0);
    /* cursor_y should have advanced through all lines */
    DTR_ASSERT(gfx->cursor_y > TH);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Map draw — viewport offset (src_x/src_y > 0)                      */
/* ------------------------------------------------------------------ */

static void test_gfx_map_draw_viewport_offset(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[16 * 8]; /* 2×1 tiles, each 8×8 */

    /* Tile 0 (cols 0-7) = colour 2, Tile 1 (cols 8-15) = colour 5 */
    for (int32_t row = 0; row < 8; ++row) {
        for (int32_t col = 0; col < 8; ++col) {
            sheet_data[row * 16 + col] = 2;
        }
        for (int32_t col = 8; col < 16; ++col) {
            sheet_data[row * 16 + col] = 5;
        }
    }

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 16;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 2;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 2;

    /* 3×1 map: [tile1, tile2, tile1] (1-based tile IDs) */
    int32_t tiles[] = {1, 2, 1};

    dtr_gfx_cls(gfx, 0);

    /* Draw starting from tile column 1 (skip first tile), 2 tiles wide */
    dtr_gfx_map_draw(gfx, tiles, 3, 1, 1, 0, 2, 1, 0, 0, 8, 8);

    /* First visible tile should be tile 2 (colour 5) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 4, 4), 5);
    /* Second visible tile should be tile 1 (colour 2) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 12, 4), 2);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Map draw — non-square tiles (tile_pw != tile_ph)                   */
/* ------------------------------------------------------------------ */

static void test_gfx_map_draw_nonsquare_tiles(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[16 * 4]; /* 16×4 sheet */

    /* Single 16×4 tile filled with colour 3 */
    memset(sheet_data, 3, sizeof(sheet_data));

    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 16;
    gfx->sheet.height = 4;
    gfx->sheet.tile_w = 16;
    gfx->sheet.tile_h = 4;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    int32_t tiles[] = {1};

    dtr_gfx_cls(gfx, 0);
    dtr_gfx_map_draw(gfx, tiles, 1, 1, 0, 0, 1, 1, 0, 0, 16, 4);

    /* Pixels inside the tile area should be drawn */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 0), 3);
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 15, 3), 3);

    /* Pixel below the tile (row 4) should still be background */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 0, 4), 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Zero-size framebuffer                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_create_zero_size(void)
{
    dtr_graphics_t *gfx;

    /* Zero dimensions — should either return NULL or handle gracefully */
    gfx = dtr_gfx_create(0, 0);
    if (gfx != NULL) {
        dtr_gfx_destroy(gfx);
    }
    /* No crash is the assertion */
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Text measurement                                                   */
/* ------------------------------------------------------------------ */

static void test_gfx_text_width_and_height(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    int32_t         w;
    int32_t         h;

    /* Single character: 4px wide (no trailing spacing) */
    w = dtr_gfx_text_width(gfx, "A");
    DTR_ASSERT_EQ_INT(w, DTR_FONT_W);

    /* Two characters: 4 + 1 + 4 = 9 */
    w = dtr_gfx_text_width(gfx, "AB");
    DTR_ASSERT_EQ_INT(w, DTR_FONT_W * 2 + 1);

    /* Multi-line: widest line wins */
    w = dtr_gfx_text_width(gfx, "AB\nC");
    DTR_ASSERT_EQ_INT(w, DTR_FONT_W * 2 + 1);

    /* Height: 1 line = DTR_FONT_H, 2 lines = 2*DTR_FONT_H */
    h = dtr_gfx_text_height(gfx, "A");
    DTR_ASSERT_EQ_INT(h, DTR_FONT_H);

    h = dtr_gfx_text_height(gfx, "A\nB");
    DTR_ASSERT_EQ_INT(h, DTR_FONT_H * 2);

    /* Empty: width = 0, height = 1 line */
    w = dtr_gfx_text_width(gfx, "");
    DTR_ASSERT_EQ_INT(w, 0);
    h = dtr_gfx_text_height(gfx, "");
    DTR_ASSERT_EQ_INT(h, DTR_FONT_H);

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
    DTR_RUN_TEST(test_gfx_sget_sset_basic);
    DTR_RUN_TEST(test_gfx_sget_oob);
    DTR_RUN_TEST(test_gfx_sset_oob);
    DTR_RUN_TEST(test_gfx_sget_null_sheet);
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
    DTR_RUN_TEST(test_gfx_dl_overflow_sspr);
    DTR_RUN_TEST(test_gfx_dl_overflow_spr_rot);
    DTR_RUN_TEST(test_gfx_dl_overflow_spr_affine);
    DTR_RUN_TEST(test_gfx_flip_odd_size);
    DTR_RUN_TEST(test_gfx_polyfill_single_point);
    DTR_RUN_TEST(test_gfx_polyfill_two_points);
    DTR_RUN_TEST(test_gfx_circ_negative_radius);
    DTR_RUN_TEST(test_gfx_fill_pattern_all);
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
    DTR_RUN_TEST(test_gfx_spr_flip_x_wide);
    DTR_RUN_TEST(test_gfx_spr_batch_matches_individual);
    DTR_RUN_TEST(test_gfx_spr_batch_empty);
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
    DTR_RUN_TEST(test_gfx_spr_rot_with_pattern);
    DTR_RUN_TEST(test_gfx_spr_rot_with_camera);
    DTR_RUN_TEST(test_gfx_spr_rot_offscreen);
    DTR_RUN_TEST(test_gfx_spr_affine_with_pattern);
    DTR_RUN_TEST(test_gfx_spr_affine_with_camera);
    DTR_RUN_TEST(test_gfx_spr_affine_offscreen);
    DTR_RUN_TEST(test_gfx_spr_affine_rotated);
    DTR_RUN_TEST(test_gfx_sspr_with_pattern);
    DTR_RUN_TEST(test_gfx_sspr_with_camera);
    DTR_RUN_TEST(test_gfx_sspr_offscreen);
    DTR_RUN_TEST(test_gfx_map_draw_transparency);
    DTR_RUN_TEST(test_gfx_map_draw_with_pattern);
    DTR_RUN_TEST(test_gfx_map_draw_partial_clip);
    DTR_RUN_TEST(test_gfx_print_with_camera);
    DTR_RUN_TEST(test_gfx_print_offscreen);
    DTR_RUN_TEST(test_gfx_print_with_pattern);
    DTR_RUN_TEST(test_gfx_print_custom_font_draws);
    DTR_RUN_TEST(test_gfx_print_special_chars);
    DTR_RUN_TEST(test_gfx_print_pal_remap);
    DTR_RUN_TEST(test_gfx_print_right_edge);
    DTR_RUN_TEST(test_gfx_print_multiline_stress);
    DTR_RUN_TEST(test_gfx_map_draw_viewport_offset);
    DTR_RUN_TEST(test_gfx_map_draw_nonsquare_tiles);
    DTR_RUN_TEST(test_gfx_create_zero_size);
    DTR_RUN_TEST(test_gfx_text_width_and_height);

    DTR_TEST_END();
}
