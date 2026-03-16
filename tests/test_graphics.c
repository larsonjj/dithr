/**
 * \file            test_graphics.c
 * \brief           Unit tests for the graphics subsystem
 */

#include <SDL3/SDL.h>
#include <string.h>

#include "graphics.h"
#include "test_harness.h"

/* Small framebuffer for fast tests */
#define TW 16
#define TH 16

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void test_gfx_create_destroy(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    MVN_ASSERT(gfx != NULL);
    MVN_ASSERT_EQ_INT(gfx->width, TW);
    MVN_ASSERT_EQ_INT(gfx->height, TH);
    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_destroy_null(void)
{
    mvn_gfx_destroy(NULL); /* must not crash */
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  cls / pset / pget                                                  */
/* ------------------------------------------------------------------ */

static void test_gfx_cls(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 5);
    for (int i = 0; i < TW * TH; ++i) {
        MVN_ASSERT_EQ_INT(gfx->framebuffer[i], 5);
    }
    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_pset_pget(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    mvn_gfx_pset(gfx, 3, 4, 9);
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 3, 4), 9);

    /* Unset pixel should be 0 */
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 0, 0), 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_pget_out_of_bounds(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 7);

    /* Out-of-bounds pget returns 0 */
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, -1, 0), 0);
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, TW, 0), 0);
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 0, TH), 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_pset_clipped(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    /* Drawing outside framebuffer should not crash or write */
    mvn_gfx_pset(gfx, -1, -1, 3);
    mvn_gfx_pset(gfx, TW, TH, 3);

    /* No pixel should have changed to 3 */
    for (int i = 0; i < TW * TH; ++i) {
        MVN_ASSERT_EQ_INT(gfx->framebuffer[i], 0);
    }

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Camera                                                             */
/* ------------------------------------------------------------------ */

static void test_gfx_camera(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    /* Camera shifts drawing coordinates */
    mvn_gfx_camera(gfx, 5, 5);
    mvn_gfx_pset(gfx, 7, 7, 2);

    /* Screen position: (7-5, 7-5) = (2, 2) */
    MVN_ASSERT_EQ_INT(gfx->framebuffer[2 * TW + 2], 2);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Clip                                                               */
/* ------------------------------------------------------------------ */

static void test_gfx_clip(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    /* Restrict clip to a 4x4 region at (2, 2) */
    mvn_gfx_clip(gfx, 2, 2, 4, 4);
    MVN_ASSERT_EQ_INT(gfx->clip_x, 2);
    MVN_ASSERT_EQ_INT(gfx->clip_y, 2);
    MVN_ASSERT_EQ_INT(gfx->clip_w, 4);
    MVN_ASSERT_EQ_INT(gfx->clip_h, 4);

    /* Drawing inside clip should work */
    mvn_gfx_pset(gfx, 3, 3, 1);
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 3, 3), 1);

    /* Drawing outside clip should be rejected */
    mvn_gfx_pset(gfx, 0, 0, 5);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[0], 0);

    mvn_gfx_pset(gfx, 6, 6, 5);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 6], 0);

    /* Reset clip */
    mvn_gfx_clip_reset(gfx);
    MVN_ASSERT_EQ_INT(gfx->clip_x, 0);
    MVN_ASSERT_EQ_INT(gfx->clip_w, TW);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Palette                                                            */
/* ------------------------------------------------------------------ */

static void test_gfx_draw_pal(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    /* Remap colour 5 → 9 in the draw palette */
    mvn_gfx_pal(gfx, 5, 9, false);
    mvn_gfx_pset(gfx, 0, 0, 5);

    /* The framebuffer should store the remapped colour */
    MVN_ASSERT_EQ_INT(gfx->framebuffer[0], 9);

    mvn_gfx_pal_reset(gfx);
    mvn_gfx_pset(gfx, 1, 0, 5);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[1], 5);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_transparency(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    /* Colour 0 transparent by default */
    MVN_ASSERT(gfx->transparent[0] == true);
    MVN_ASSERT(gfx->transparent[1] == false);

    mvn_gfx_palt(gfx, 3, true);
    MVN_ASSERT(gfx->transparent[3] == true);

    mvn_gfx_palt_reset(gfx);
    MVN_ASSERT(gfx->transparent[3] == false);
    MVN_ASSERT(gfx->transparent[0] == true); /* 0 remains transparent */

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Color / cursor state                                               */
/* ------------------------------------------------------------------ */

static void test_gfx_color(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    MVN_ASSERT_EQ_INT(gfx->color, 7); /* default */
    mvn_gfx_color(gfx, 12);
    MVN_ASSERT_EQ_INT(gfx->color, 12);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_cursor(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    mvn_gfx_cursor(gfx, 5, 10);
    MVN_ASSERT_EQ_INT(gfx->cursor_x, 5);
    MVN_ASSERT_EQ_INT(gfx->cursor_y, 10);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Primitives: line, rect, rectfill                                   */
/* ------------------------------------------------------------------ */

static void test_gfx_line_horizontal(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    mvn_gfx_line(gfx, 2, 3, 8, 3, 4);

    /* All pixels on row 3 from x=2..8 should be colour 4 */
    for (int x = 2; x <= 8; ++x) {
        MVN_ASSERT_EQ_INT(gfx->framebuffer[3 * TW + x], 4);
    }
    /* Pixel just outside shouldn't be set */
    MVN_ASSERT_EQ_INT(gfx->framebuffer[3 * TW + 1], 0);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[3 * TW + 9], 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_rectfill(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    mvn_gfx_rectfill(gfx, 2, 2, 5, 5, 3);

    /* Pixels inside the filled rect */
    for (int y = 2; y <= 5; ++y) {
        for (int x = 2; x <= 5; ++x) {
            MVN_ASSERT_EQ_INT(gfx->framebuffer[y * TW + x], 3);
        }
    }
    /* Pixel just outside */
    MVN_ASSERT_EQ_INT(gfx->framebuffer[1 * TW + 2], 0);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 2], 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_rect_outline(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    mvn_gfx_rect(gfx, 2, 2, 6, 6, 8);

    /* Corners should be set */
    MVN_ASSERT_EQ_INT(gfx->framebuffer[2 * TW + 2], 8);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[2 * TW + 6], 8);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 2], 8);
    MVN_ASSERT_EQ_INT(gfx->framebuffer[6 * TW + 6], 8);

    /* Interior should be empty */
    MVN_ASSERT_EQ_INT(gfx->framebuffer[4 * TW + 4], 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Sprite flags                                                       */
/* ------------------------------------------------------------------ */

static void test_gfx_sprite_flags(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    /* Default flags are 0 */
    MVN_ASSERT_EQ_INT(mvn_gfx_fget(gfx, 0), 0);

    mvn_gfx_fset(gfx, 0, 0xFF);
    MVN_ASSERT_EQ_INT(mvn_gfx_fget(gfx, 0), 0xFF);

    /* Per-bit set/get */
    mvn_gfx_fset(gfx, 1, 0);
    mvn_gfx_fset_bit(gfx, 1, 2, true);
    MVN_ASSERT(mvn_gfx_fget_bit(gfx, 1, 2));
    MVN_ASSERT(!mvn_gfx_fget_bit(gfx, 1, 0));

    mvn_gfx_fset_bit(gfx, 1, 2, false);
    MVN_ASSERT(!mvn_gfx_fget_bit(gfx, 1, 2));

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Fill pattern                                                       */
/* ------------------------------------------------------------------ */

static void test_gfx_fill_pattern(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    /* Checkerboard pattern: alternating bits in a 4x4 grid */
    mvn_gfx_fillp(gfx, 0x5A5A);

    /* Draw a filled rect — only pattern-enabled pixels should be set */
    mvn_gfx_rectfill(gfx, 0, 0, 3, 3, 2);

    /* Count filled pixels — should be exactly half of 4x4 = 8 */
    int count = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (gfx->framebuffer[y * TW + x] == 2) ++count;
        }
    }
    MVN_ASSERT_EQ_INT(count, 8);

    /* Reset pattern */
    mvn_gfx_fillp(gfx, 0);
    mvn_gfx_cls(gfx, 0);
    mvn_gfx_rectfill(gfx, 0, 0, 3, 3, 2);

    /* All 16 pixels should be filled now */
    count = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (gfx->framebuffer[y * TW + x] == 2) ++count;
        }
    }
    MVN_ASSERT_EQ_INT(count, 16);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Flip (palette → RGBA)                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_flip(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    mvn_gfx_cls(gfx, 0);

    mvn_gfx_pset(gfx, 0, 0, 1);
    mvn_gfx_flip(gfx);

    /* Pixel at (0,0) should be colour 1 from the palette */
    MVN_ASSERT_EQ_INT(gfx->pixels[0], gfx->colors[gfx->screen_pal[1]]);

    /* Pixel at (1,0) should be colour 0 */
    MVN_ASSERT_EQ_INT(gfx->pixels[1], gfx->colors[gfx->screen_pal[0]]);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Reset                                                              */
/* ------------------------------------------------------------------ */

static void test_gfx_reset(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    /* Mutate state */
    mvn_gfx_color(gfx, 12);
    mvn_gfx_camera(gfx, 3, 3);
    mvn_gfx_clip(gfx, 1, 1, 4, 4);
    mvn_gfx_cursor(gfx, 5, 5);
    mvn_gfx_fillp(gfx, 0xAAAA);

    mvn_gfx_reset(gfx);

    MVN_ASSERT_EQ_INT(gfx->color, 7);
    MVN_ASSERT_EQ_INT(gfx->camera_x, 0);
    MVN_ASSERT_EQ_INT(gfx->camera_y, 0);
    MVN_ASSERT_EQ_INT(gfx->clip_x, 0);
    MVN_ASSERT_EQ_INT(gfx->clip_y, 0);
    MVN_ASSERT_EQ_INT(gfx->clip_w, TW);
    MVN_ASSERT_EQ_INT(gfx->clip_h, TH);
    MVN_ASSERT_EQ_INT(gfx->cursor_x, 0);
    MVN_ASSERT_EQ_INT(gfx->cursor_y, 0);
    MVN_ASSERT_EQ_INT(gfx->fill_pattern, 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Polygon                                                            */
/* ------------------------------------------------------------------ */

static void test_gfx_poly_outline(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    int32_t pts[] = { 2, 2, 8, 2, 8, 8, 2, 8 };

    mvn_gfx_cls(gfx, 0);
    mvn_gfx_poly(gfx, pts, 4, 7);

    /* Corners should be drawn */
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 2, 2), 7);
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 8, 2), 7);
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 8, 8), 7);
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 2, 8), 7);

    /* Centre should not be filled */
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 5, 5), 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_polyfill(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    int32_t pts[] = { 2, 2, 8, 2, 8, 8, 2, 8 };

    mvn_gfx_cls(gfx, 0);
    mvn_gfx_polyfill(gfx, pts, 4, 3);

    /* Centre should be filled */
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 5, 5), 3);

    /* Corners should be filled */
    MVN_ASSERT_EQ_INT(mvn_gfx_pget(gfx, 2, 2), 3);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_poly_degenerate(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);
    int32_t pts[] = { 0, 0 };

    mvn_gfx_cls(gfx, 0);

    /* Less than 3 vertices — should not crash */
    mvn_gfx_poly(gfx, pts, 1, 7);
    mvn_gfx_polyfill(gfx, pts, 2, 7);
    mvn_gfx_poly(gfx, NULL, 0, 7);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Custom font                                                        */
/* ------------------------------------------------------------------ */

static void test_gfx_font_custom_and_reset(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    /* Set custom font */
    mvn_gfx_font(gfx, 0, 0, 8, 8, ' ', 95);
    MVN_ASSERT(gfx->custom_font.active);
    MVN_ASSERT_EQ_INT(gfx->custom_font.char_w, 8);
    MVN_ASSERT_EQ_INT(gfx->custom_font.char_h, 8);
    MVN_ASSERT_EQ_INT(gfx->custom_font.first, ' ');
    MVN_ASSERT_EQ_INT(gfx->custom_font.count, 95);

    /* Reset */
    mvn_gfx_font_reset(gfx);
    MVN_ASSERT(!gfx->custom_font.active);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Screen transitions                                                 */
/* ------------------------------------------------------------------ */

static void test_gfx_fade(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    MVN_ASSERT(!mvn_gfx_transitioning(gfx));

    mvn_gfx_fade(gfx, 0, 10);
    MVN_ASSERT(mvn_gfx_transitioning(gfx));
    MVN_ASSERT_EQ_INT(gfx->transition.type, MVN_TRANS_FADE);
    MVN_ASSERT_EQ_INT(gfx->transition.duration, 10);
    MVN_ASSERT_EQ_INT(gfx->transition.color, 0);

    /* Run until completion */
    mvn_gfx_cls(gfx, 0);
    mvn_gfx_flip(gfx);
    for (int i = 0; i < 10; ++i) {
        mvn_gfx_transition_update(gfx);
    }
    MVN_ASSERT(!mvn_gfx_transitioning(gfx));

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_wipe(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    mvn_gfx_wipe(gfx, MVN_WIPE_LEFT, 1, 5);
    MVN_ASSERT(mvn_gfx_transitioning(gfx));
    MVN_ASSERT_EQ_INT(gfx->transition.type, MVN_TRANS_WIPE);
    MVN_ASSERT_EQ_INT(gfx->transition.direction, MVN_WIPE_LEFT);

    /* Run until completion */
    mvn_gfx_cls(gfx, 0);
    mvn_gfx_flip(gfx);
    for (int i = 0; i < 5; ++i) {
        mvn_gfx_transition_update(gfx);
    }
    MVN_ASSERT(!mvn_gfx_transitioning(gfx));

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_dissolve(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    mvn_gfx_dissolve(gfx, 2, 8);
    MVN_ASSERT(mvn_gfx_transitioning(gfx));
    MVN_ASSERT_EQ_INT(gfx->transition.type, MVN_TRANS_DISSOLVE);

    /* Run until completion */
    mvn_gfx_cls(gfx, 0);
    mvn_gfx_flip(gfx);
    for (int i = 0; i < 8; ++i) {
        mvn_gfx_transition_update(gfx);
    }
    MVN_ASSERT(!mvn_gfx_transitioning(gfx));

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_transition_clamp_frames(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    /* frames < 1 should be clamped to 1 */
    mvn_gfx_fade(gfx, 0, 0);
    MVN_ASSERT_EQ_INT(gfx->transition.duration, 1);

    mvn_gfx_wipe(gfx, 0, 0, -5);
    MVN_ASSERT_EQ_INT(gfx->transition.duration, 1);

    mvn_gfx_dissolve(gfx, 0, 0);
    MVN_ASSERT_EQ_INT(gfx->transition.duration, 1);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Draw list                                                          */
/* ------------------------------------------------------------------ */

static void test_gfx_dl_begin_end(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    mvn_gfx_dl_begin(gfx);
    MVN_ASSERT(gfx->draw_list.active);
    MVN_ASSERT_EQ_INT(gfx->draw_list.count, 0);

    mvn_gfx_dl_end(gfx);
    MVN_ASSERT(!gfx->draw_list.active);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_dl_queue_and_sort(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    mvn_gfx_dl_begin(gfx);

    /* Queue sprites at different layers (higher layer first) */
    mvn_gfx_dl_spr(gfx, 10, 0, 0, 0, 1, 1, false, false);
    mvn_gfx_dl_spr(gfx, 1, 0, 0, 0, 1, 1, false, false);
    mvn_gfx_dl_spr(gfx, 5, 0, 0, 0, 1, 1, false, false);
    MVN_ASSERT_EQ_INT(gfx->draw_list.count, 3);

    /* dl_end sorts by layer and flushes */
    mvn_gfx_dl_end(gfx);
    MVN_ASSERT_EQ_INT(gfx->draw_list.count, 0);

    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

static void test_gfx_dl_overflow(void)
{
    mvn_graphics_t *gfx = mvn_gfx_create(TW, TH);

    mvn_gfx_dl_begin(gfx);

    /* Fill to capacity */
    for (int i = 0; i < CONSOLE_MAX_DRAW_CMDS; ++i) {
        mvn_gfx_dl_spr(gfx, 0, 0, 0, 0, 1, 1, false, false);
    }
    MVN_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    /* One more should be silently dropped */
    mvn_gfx_dl_spr(gfx, 0, 0, 0, 0, 1, 1, false, false);
    MVN_ASSERT_EQ_INT(gfx->draw_list.count, CONSOLE_MAX_DRAW_CMDS);

    mvn_gfx_dl_end(gfx);
    mvn_gfx_destroy(gfx);
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== test_graphics ===\n");

    test_gfx_create_destroy();
    test_gfx_destroy_null();
    test_gfx_cls();
    test_gfx_pset_pget();
    test_gfx_pget_out_of_bounds();
    test_gfx_pset_clipped();
    test_gfx_camera();
    test_gfx_clip();
    test_gfx_draw_pal();
    test_gfx_transparency();
    test_gfx_color();
    test_gfx_cursor();
    test_gfx_line_horizontal();
    test_gfx_rectfill();
    test_gfx_rect_outline();
    test_gfx_sprite_flags();
    test_gfx_fill_pattern();
    test_gfx_flip();
    test_gfx_reset();

    /* Phase 10 */
    test_gfx_poly_outline();
    test_gfx_polyfill();
    test_gfx_poly_degenerate();
    test_gfx_font_custom_and_reset();
    test_gfx_fade();
    test_gfx_wipe();
    test_gfx_dissolve();
    test_gfx_transition_clamp_frames();
    test_gfx_dl_begin_end();
    test_gfx_dl_queue_and_sort();
    test_gfx_dl_overflow();

    printf("All graphics tests passed.\n");
    return 0;
}
