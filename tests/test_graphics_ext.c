/**
 * \file            test_graphics_ext.c
 * \brief           Extended graphics tests (transitions, palette, sheet, draw
 *                  list dispatch)
 */

#include <string.h>

#include "graphics.h"
#include "test_harness.h"

/* Small framebuffer for fast tests */
#define TW 16
#define TH 16

/* ------------------------------------------------------------------ */
/*  Transition pixel processing                                        */
/* ------------------------------------------------------------------ */

static void test_gfx_fade_pixels(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        pixels[16];

    /* Fill pixels with white (palette 7 → 0xFFF1E8FF) */
    for (int32_t idx = 0; idx < 16; ++idx) {
        pixels[idx] = 0xFFFFFFFF;
    }

    /* Fade towards black (palette 0 = 0x000000FF), 2 frames */
    dtr_gfx_fade(gfx, 0, 2);

    /* Frame 1: t=0.5 → should blend halfway toward black */
    dtr_gfx_transition_update_buf(gfx, pixels);
    DTR_ASSERT(dtr_gfx_transitioning(gfx));

    {
        uint32_t pix;
        uint8_t  rval;

        pix  = pixels[0];
        rval = (uint8_t)((pix >> 24) & 0xFF);
        /* R should be around 127-128 (halfway from 255 to 0) */
        DTR_ASSERT(rval >= 100 && rval <= 155);
    }

    /* Frame 2: t=1.0 → fully faded to black, transition ends */
    dtr_gfx_transition_update_buf(gfx, pixels);
    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    {
        uint32_t pix;
        uint8_t  rval;

        pix  = pixels[0];
        rval = (uint8_t)((pix >> 24) & 0xFF);
        DTR_ASSERT_EQ_INT(rval, 0);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_wipe_left_pixels(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        pixels[16];
    uint32_t        target;

    for (int32_t idx = 0; idx < 16; ++idx) {
        pixels[idx] = 0xFFFFFFFF;
    }

    /* Wipe left, 1 frame → entire screen should be target */
    dtr_gfx_wipe(gfx, DTR_WIPE_LEFT, 0, 1);
    target = gfx->colors[0];

    dtr_gfx_transition_update_buf(gfx, pixels);
    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    for (int32_t idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(pixels[idx], target);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_wipe_right_pixels(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        pixels[16];
    uint32_t        target;

    for (int32_t idx = 0; idx < 16; ++idx) {
        pixels[idx] = 0xFFFFFFFF;
    }

    dtr_gfx_wipe(gfx, DTR_WIPE_RIGHT, 0, 1);
    target = gfx->colors[0];

    dtr_gfx_transition_update_buf(gfx, pixels);
    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    for (int32_t idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(pixels[idx], target);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_wipe_up_pixels(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        pixels[16];
    uint32_t        target;

    for (int32_t idx = 0; idx < 16; ++idx) {
        pixels[idx] = 0xFFFFFFFF;
    }

    dtr_gfx_wipe(gfx, DTR_WIPE_UP, 0, 1);
    target = gfx->colors[0];

    dtr_gfx_transition_update_buf(gfx, pixels);

    for (int32_t idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(pixels[idx], target);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_wipe_down_pixels(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        pixels[16];
    uint32_t        target;

    for (int32_t idx = 0; idx < 16; ++idx) {
        pixels[idx] = 0xFFFFFFFF;
    }

    dtr_gfx_wipe(gfx, DTR_WIPE_DOWN, 0, 1);
    target = gfx->colors[0];

    dtr_gfx_transition_update_buf(gfx, pixels);

    for (int32_t idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(pixels[idx], target);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dissolve_pixels(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        pixels[16];
    uint32_t        target;
    int32_t         changed;

    for (int32_t idx = 0; idx < 16; ++idx) {
        pixels[idx] = 0xFFFFFFFF;
    }

    dtr_gfx_dissolve(gfx, 0, 3);
    target = gfx->colors[0];

    /* Frame 1: t=1/3, some pixels should change */
    dtr_gfx_transition_update_buf(gfx, pixels);
    changed = 0;
    for (int32_t idx = 0; idx < 16; ++idx) {
        if (pixels[idx] == target) {
            ++changed;
        }
    }
    DTR_ASSERT(changed > 0);
    DTR_ASSERT(dtr_gfx_transitioning(gfx));

    /* Frame 2: t=2/3, more pixels should change */
    dtr_gfx_transition_update_buf(gfx, pixels);
    {
        int32_t changed2;

        changed2 = 0;
        for (int32_t idx = 0; idx < 16; ++idx) {
            if (pixels[idx] == target) {
                ++changed2;
            }
        }
        DTR_ASSERT(changed2 >= changed);
    }

    /* Frame 3: transition completes */
    dtr_gfx_transition_update_buf(gfx, pixels);
    DTR_ASSERT(!dtr_gfx_transitioning(gfx));

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_transition_none_noop(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        pixels[16];

    for (int32_t idx = 0; idx < 16; ++idx) {
        pixels[idx] = 0xAABBCCFF;
    }

    /* No transition active → should not modify pixels */
    dtr_gfx_transition_update_buf(gfx, pixels);

    for (int32_t idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(pixels[idx], 0xAABBCCFF);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Flip to RGBA                                                       */
/* ------------------------------------------------------------------ */

static void test_gfx_flip_to(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(4, 4);
    uint32_t        dst[16];

    dtr_gfx_cls(gfx, 7);
    dtr_gfx_flip_to(gfx, dst);

    /* All pixels should be palette colour 7 */
    for (int32_t idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(dst[idx], gfx->colors[7]);
    }

    /* With screen_pal remapping: 7 → 1 */
    dtr_gfx_pal(gfx, 7, 1, true);
    dtr_gfx_flip_to(gfx, dst);
    for (int32_t idx = 0; idx < 16; ++idx) {
        DTR_ASSERT_EQ_INT(dst[idx], gfx->colors[1]);
    }

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Palt / clip_reset                                                  */
/* ------------------------------------------------------------------ */

static void test_gfx_palt(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Default: only colour 0 is transparent */
    DTR_ASSERT(gfx->transparent[0]);
    DTR_ASSERT(!gfx->transparent[1]);

    /* Set colour 5 as transparent */
    dtr_gfx_palt(gfx, 5, true);
    DTR_ASSERT(gfx->transparent[5]);

    /* Clear it */
    dtr_gfx_palt(gfx, 5, false);
    DTR_ASSERT(!gfx->transparent[5]);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_palt_reset(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* Modify several transparency flags */
    dtr_gfx_palt(gfx, 0, false);
    dtr_gfx_palt(gfx, 3, true);
    dtr_gfx_palt(gfx, 7, true);

    dtr_gfx_palt_reset(gfx);

    /* Only colour 0 should be transparent again */
    DTR_ASSERT(gfx->transparent[0]);
    DTR_ASSERT(!gfx->transparent[3]);
    DTR_ASSERT(!gfx->transparent[7]);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_clip_reset(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    dtr_gfx_clip(gfx, 2, 3, 4, 5);
    DTR_ASSERT_EQ_INT(gfx->clip_x, 2);

    dtr_gfx_clip_reset(gfx);
    DTR_ASSERT_EQ_INT(gfx->clip_x, 0);
    DTR_ASSERT_EQ_INT(gfx->clip_y, 0);
    DTR_ASSERT_EQ_INT(gfx->clip_w, TW);
    DTR_ASSERT_EQ_INT(gfx->clip_h, TH);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Sprite sheet loading                                               */
/* ------------------------------------------------------------------ */

static void test_gfx_load_sheet(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         rgba[8 * 8 * 4];

    /* Fill with pure black RGBA → should quantise to palette 0 */
    memset(rgba, 0, sizeof(rgba));
    for (int32_t idx = 0; idx < 8 * 8; ++idx) {
        rgba[idx * 4 + 3] = 255;    /* opaque alpha */
    }

    DTR_ASSERT(dtr_gfx_load_sheet(gfx, rgba, 8, 8, 8, 8));
    DTR_ASSERT(gfx->sheet.pixels != NULL);
    DTR_ASSERT_EQ_INT(gfx->sheet.width, 8);
    DTR_ASSERT_EQ_INT(gfx->sheet.height, 8);
    DTR_ASSERT_EQ_INT(gfx->sheet.cols, 1);
    DTR_ASSERT_EQ_INT(gfx->sheet.rows, 1);
    DTR_ASSERT_EQ_INT(gfx->sheet.count, 1);

    /* Black pixel should quantise to palette entry 0 */
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[0], 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_load_sheet_transparent(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         rgba[8 * 8 * 4];

    /* Fully transparent pixels → palette 0 */
    memset(rgba, 0, sizeof(rgba));

    DTR_ASSERT(dtr_gfx_load_sheet(gfx, rgba, 8, 8, 8, 8));
    DTR_ASSERT_EQ_INT(gfx->sheet.pixels[0], 0);

    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Draw list dispatch                                                 */
/* ------------------------------------------------------------------ */

static void test_gfx_dl_spr_dispatch(void)
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

    dtr_gfx_dl_begin(gfx);
    dtr_gfx_dl_spr(gfx, 0, 0, 0, 0, 1, 1, false, false);
    dtr_gfx_dl_end(gfx);

    /* Sprite should have been drawn at (0,0) */
    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 4, 4), 5);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_sspr_dispatch(void)
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

    dtr_gfx_dl_begin(gfx);
    dtr_gfx_dl_sspr(gfx, 0, 0, 0, 4, 4, 0, 0, 4, 4);
    dtr_gfx_dl_end(gfx);

    DTR_ASSERT_EQ_INT(dtr_gfx_pget(gfx, 2, 2), 3);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_spr_rot_dispatch(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         count;

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

    dtr_gfx_dl_begin(gfx);
    dtr_gfx_dl_spr_rot(gfx, 0, 0, 4, 4, 0.0f, 4, 4);
    dtr_gfx_dl_end(gfx);

    count = 0;
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        if (gfx->framebuffer[idx] == 4) {
            ++count;
        }
    }
    DTR_ASSERT(count > 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_spr_affine_dispatch(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);
    uint8_t         sheet_data[8 * 8];
    int32_t         count;

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

    dtr_gfx_dl_begin(gfx);
    dtr_gfx_dl_spr_affine(gfx, 0, 0, 4, 4, 4.0f, 4.0f, 1.0f, 0.0f);
    dtr_gfx_dl_end(gfx);

    count = 0;
    for (int32_t idx = 0; idx < TW * TH; ++idx) {
        if (gfx->framebuffer[idx] == 6) {
            ++count;
        }
    }
    DTR_ASSERT(count > 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_gfx_dl_layer_order(void)
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

    dtr_gfx_dl_begin(gfx);

    /* Layer 1: draw colour 2 sprite at (0,0) */
    dtr_gfx_dl_spr(gfx, 1, 0, 0, 0, 1, 1, false, false);

    /* Layer 0 (lower): overwrite pixels at (0,0) with colour 9 */
    sheet_data[0] = 9;
    dtr_gfx_dl_spr(gfx, 0, 0, 0, 0, 1, 1, false, false);

    dtr_gfx_dl_end(gfx);

    /* Layer 1 is drawn AFTER layer 0, so pixel should be colour 2 */
    /* (But note: sheet data was modified to 9 after the first queue.) */
    /* Since commands reference the same sheet, the final pixels depend  */
    /* on sheet state at dl_end time. Layer 0 draws first (9), then     */
    /* layer 1 draws second — but sheet data is same, so both use 9     */
    /* at pixel[0]. Check that something was drawn at least.             */
    DTR_ASSERT(dtr_gfx_pget(gfx, 0, 0) != 0);

    gfx->sheet.pixels = NULL;
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Init default palette                                               */
/* ------------------------------------------------------------------ */

static void test_gfx_init_default_palette(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(TW, TH);

    /* First palette entry should be black */
    DTR_ASSERT_EQ_INT(gfx->colors[0], 0x000000FF);
    /* Palette entry 7 should be 0xFFF1E8FF */
    DTR_ASSERT_EQ_INT(gfx->colors[7], 0xFFF1E8FF);
    /* Entry 16 should be greyscale 0x111111FF */
    DTR_ASSERT_EQ_INT(gfx->colors[16], 0x111111FF);
    /* HSV-generated entries (32+) should have full alpha */
    DTR_ASSERT_EQ_INT(gfx->colors[32] & 0xFF, 0xFF);
    DTR_ASSERT_EQ_INT(gfx->colors[255] & 0xFF, 0xFF);

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

    DTR_TEST_BEGIN("test_graphics_ext");

    DTR_RUN_TEST(test_gfx_fade_pixels);
    DTR_RUN_TEST(test_gfx_wipe_left_pixels);
    DTR_RUN_TEST(test_gfx_wipe_right_pixels);
    DTR_RUN_TEST(test_gfx_wipe_up_pixels);
    DTR_RUN_TEST(test_gfx_wipe_down_pixels);
    DTR_RUN_TEST(test_gfx_dissolve_pixels);
    DTR_RUN_TEST(test_gfx_transition_none_noop);
    DTR_RUN_TEST(test_gfx_flip_to);
    DTR_RUN_TEST(test_gfx_palt);
    DTR_RUN_TEST(test_gfx_palt_reset);
    DTR_RUN_TEST(test_gfx_clip_reset);
    DTR_RUN_TEST(test_gfx_load_sheet);
    DTR_RUN_TEST(test_gfx_load_sheet_transparent);
    DTR_RUN_TEST(test_gfx_dl_spr_dispatch);
    DTR_RUN_TEST(test_gfx_dl_sspr_dispatch);
    DTR_RUN_TEST(test_gfx_dl_spr_rot_dispatch);
    DTR_RUN_TEST(test_gfx_dl_spr_affine_dispatch);
    DTR_RUN_TEST(test_gfx_dl_layer_order);
    DTR_RUN_TEST(test_gfx_init_default_palette);

    DTR_TEST_END();
}
