/**
 * \file            test_ui.c
 * \brief           Unit tests for stateless UI layout helpers
 */

#include "graphics.h"
#include "test_harness.h"
#include "ui.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  ui.rect                                                            */
/* ------------------------------------------------------------------ */

static void test_rect_basic(void)
{
    dtr_ui_rect_t rect;

    rect = dtr_ui_rect(10, 20, 100, 50);
    DTR_ASSERT_EQ_INT(rect.pos_x, 10);
    DTR_ASSERT_EQ_INT(rect.pos_y, 20);
    DTR_ASSERT_EQ_INT(rect.width, 100);
    DTR_ASSERT_EQ_INT(rect.height, 50);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui.inset                                                           */
/* ------------------------------------------------------------------ */

static void test_inset_basic(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out;

    rect = dtr_ui_rect(10, 10, 100, 80);
    out  = dtr_ui_inset(rect, 5);
    DTR_ASSERT_EQ_INT(out.pos_x, 15);
    DTR_ASSERT_EQ_INT(out.pos_y, 15);
    DTR_ASSERT_EQ_INT(out.width, 90);
    DTR_ASSERT_EQ_INT(out.height, 70);
    DTR_PASS();
}

static void test_inset_clamps_to_zero(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out;

    rect = dtr_ui_rect(0, 0, 10, 6);
    out  = dtr_ui_inset(rect, 20);
    DTR_ASSERT_EQ_INT(out.width, 0);
    DTR_ASSERT_EQ_INT(out.height, 0);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui.anchor                                                          */
/* ------------------------------------------------------------------ */

static void test_anchor_top_left(void)
{
    dtr_ui_rect_t out;

    out = dtr_ui_anchor(0.0, 0.0, 32, 16, 320, 180);
    DTR_ASSERT_EQ_INT(out.pos_x, 0);
    DTR_ASSERT_EQ_INT(out.pos_y, 0);
    DTR_ASSERT_EQ_INT(out.width, 32);
    DTR_ASSERT_EQ_INT(out.height, 16);
    DTR_PASS();
}

static void test_anchor_center(void)
{
    dtr_ui_rect_t out;

    out = dtr_ui_anchor(0.5, 0.5, 32, 16, 320, 180);
    DTR_ASSERT_EQ_INT(out.pos_x, 144); /* (320-32)/2 = 144 */
    DTR_ASSERT_EQ_INT(out.pos_y, 82);  /* (180-16)/2 = 82 */
    DTR_PASS();
}

static void test_anchor_bottom_right(void)
{
    dtr_ui_rect_t out;

    out = dtr_ui_anchor(1.0, 1.0, 32, 16, 320, 180);
    DTR_ASSERT_EQ_INT(out.pos_x, 288); /* 320-32 */
    DTR_ASSERT_EQ_INT(out.pos_y, 164); /* 180-16 */
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui.hsplit                                                          */
/* ------------------------------------------------------------------ */

static void test_hsplit_even(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t left;
    dtr_ui_rect_t right;

    rect = dtr_ui_rect(0, 0, 100, 50);
    dtr_ui_hsplit(rect, 0.5, 0, &left, &right);
    DTR_ASSERT_EQ_INT(left.pos_x, 0);
    DTR_ASSERT_EQ_INT(left.width, 50);
    DTR_ASSERT_EQ_INT(right.pos_x, 50);
    DTR_ASSERT_EQ_INT(right.width, 50);
    DTR_ASSERT_EQ_INT(left.height, 50);
    DTR_ASSERT_EQ_INT(right.height, 50);
    DTR_PASS();
}

static void test_hsplit_with_gap(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t left;
    dtr_ui_rect_t right;

    rect = dtr_ui_rect(10, 0, 104, 50);
    dtr_ui_hsplit(rect, 0.5, 4, &left, &right);
    DTR_ASSERT_EQ_INT(left.pos_x, 10);
    DTR_ASSERT_EQ_INT(left.width, 50);
    DTR_ASSERT_EQ_INT(right.pos_x, 64); /* 10 + 50 + 4 */
    DTR_ASSERT_EQ_INT(right.width, 50);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui.vsplit                                                          */
/* ------------------------------------------------------------------ */

static void test_vsplit_even(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t top;
    dtr_ui_rect_t bot;

    rect = dtr_ui_rect(0, 0, 100, 80);
    dtr_ui_vsplit(rect, 0.5, 0, &top, &bot);
    DTR_ASSERT_EQ_INT(top.pos_y, 0);
    DTR_ASSERT_EQ_INT(top.height, 40);
    DTR_ASSERT_EQ_INT(bot.pos_y, 40);
    DTR_ASSERT_EQ_INT(bot.height, 40);
    DTR_PASS();
}

static void test_vsplit_with_gap(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t top;
    dtr_ui_rect_t bot;

    rect = dtr_ui_rect(0, 10, 100, 84);
    dtr_ui_vsplit(rect, 0.5, 4, &top, &bot);
    DTR_ASSERT_EQ_INT(top.pos_y, 10);
    DTR_ASSERT_EQ_INT(top.height, 40);
    DTR_ASSERT_EQ_INT(bot.pos_y, 54); /* 10 + 40 + 4 */
    DTR_ASSERT_EQ_INT(bot.height, 40);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui.hstack                                                          */
/* ------------------------------------------------------------------ */

static void test_hstack_basic(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out[3];

    rect = dtr_ui_rect(0, 0, 90, 30);
    dtr_ui_hstack(rect, 3, 0, out);
    DTR_ASSERT_EQ_INT(out[0].pos_x, 0);
    DTR_ASSERT_EQ_INT(out[0].width, 30);
    DTR_ASSERT_EQ_INT(out[1].pos_x, 30);
    DTR_ASSERT_EQ_INT(out[1].width, 30);
    DTR_ASSERT_EQ_INT(out[2].pos_x, 60);
    DTR_ASSERT_EQ_INT(out[2].width, 30);
    DTR_PASS();
}

static void test_hstack_with_gap(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out[3];

    rect = dtr_ui_rect(0, 0, 96, 30);
    dtr_ui_hstack(rect, 3, 3, out);
    DTR_ASSERT_EQ_INT(out[0].pos_x, 0);
    DTR_ASSERT_EQ_INT(out[0].width, 30);
    DTR_ASSERT_EQ_INT(out[1].pos_x, 33);
    DTR_ASSERT_EQ_INT(out[1].width, 30);
    DTR_ASSERT_EQ_INT(out[2].pos_x, 66);
    DTR_ASSERT_EQ_INT(out[2].width, 30);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui.vstack                                                          */
/* ------------------------------------------------------------------ */

static void test_vstack_basic(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out[4];

    rect = dtr_ui_rect(0, 0, 100, 80);
    dtr_ui_vstack(rect, 4, 0, out);
    DTR_ASSERT_EQ_INT(out[0].pos_y, 0);
    DTR_ASSERT_EQ_INT(out[0].height, 20);
    DTR_ASSERT_EQ_INT(out[3].pos_y, 60);
    DTR_ASSERT_EQ_INT(out[3].height, 20);
    DTR_PASS();
}

static void test_vstack_with_gap(void)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out[3];

    rect = dtr_ui_rect(0, 0, 100, 66);
    dtr_ui_vstack(rect, 3, 2, out);
    DTR_ASSERT_EQ_INT(out[0].pos_y, 0);
    /* usable = 66 - 2*2 = 62, cell_h = 62/3 = 20 */
    DTR_ASSERT_EQ_INT(out[0].height, 20);
    DTR_ASSERT_EQ_INT(out[1].pos_y, 22);
    DTR_ASSERT_EQ_INT(out[2].pos_y, 44);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui.place                                                           */
/* ------------------------------------------------------------------ */

static void test_place_center(void)
{
    dtr_ui_rect_t parent;
    dtr_ui_rect_t child;

    parent = dtr_ui_rect(0, 0, 100, 80);
    child  = dtr_ui_place(parent, 0.5, 0.5, 20, 10);
    DTR_ASSERT_EQ_INT(child.pos_x, 40); /* (100-20)/2 */
    DTR_ASSERT_EQ_INT(child.pos_y, 35); /* (80-10)/2 */
    DTR_ASSERT_EQ_INT(child.width, 20);
    DTR_ASSERT_EQ_INT(child.height, 10);
    DTR_PASS();
}

static void test_place_bottom_right(void)
{
    dtr_ui_rect_t parent;
    dtr_ui_rect_t child;

    parent = dtr_ui_rect(10, 10, 100, 80);
    child  = dtr_ui_place(parent, 1.0, 1.0, 20, 10);
    DTR_ASSERT_EQ_INT(child.pos_x, 90); /* 10 + (100-20) */
    DTR_ASSERT_EQ_INT(child.pos_y, 80); /* 10 + (80-10) */
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Group stack                                                        */
/* ------------------------------------------------------------------ */

static void test_group_push_pop_basic(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;
    dtr_ui_rect_t   rect;
    dtr_ui_rect_t   cur;
    bool            ok;

    gfx = dtr_gfx_create(320, 180);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    rect = dtr_ui_rect(10, 20, 100, 50);
    ok   = dtr_ui_group_push(ui, rect, false);
    DTR_ASSERT(ok);
    cur = dtr_ui_group_current(ui);
    DTR_ASSERT_EQ_INT(cur.pos_x, 10);
    DTR_ASSERT_EQ_INT(cur.pos_y, 20);
    DTR_ASSERT_EQ_INT(cur.width, 100);
    DTR_ASSERT_EQ_INT(cur.height, 50);

    dtr_ui_group_pop(ui);
    /* After pop, stack is empty — current returns zero rect */
    cur = dtr_ui_group_current(ui);
    DTR_ASSERT_EQ_INT(cur.width, 0);
    DTR_ASSERT_EQ_INT(cur.height, 0);

    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_group_pop_empty_is_safe(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;

    gfx = dtr_gfx_create(320, 180);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    /* Should not crash */
    dtr_ui_group_pop(ui);
    dtr_ui_group_pop(ui);

    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_group_nested(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;
    dtr_ui_rect_t   outer;
    dtr_ui_rect_t   inner;
    dtr_ui_rect_t   cur;

    gfx = dtr_gfx_create(320, 180);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    outer = dtr_ui_rect(0, 0, 100, 80);
    inner = dtr_ui_rect(10, 10, 50, 40);

    dtr_ui_group_push(ui, outer, false);
    dtr_ui_group_push(ui, inner, false);
    cur = dtr_ui_group_current(ui);
    DTR_ASSERT_EQ_INT(cur.pos_x, 10);
    DTR_ASSERT_EQ_INT(cur.width, 50);

    dtr_ui_group_pop(ui);
    cur = dtr_ui_group_current(ui);
    DTR_ASSERT_EQ_INT(cur.pos_x, 0);
    DTR_ASSERT_EQ_INT(cur.width, 100);

    dtr_ui_group_pop(ui);

    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_group_depth_limit(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;
    dtr_ui_rect_t   rect;
    bool            ok;

    gfx = dtr_gfx_create(320, 180);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    rect = dtr_ui_rect(0, 0, 10, 10);
    for (int32_t i = 0; i < DTR_UI_GROUP_MAX_DEPTH; ++i) {
        ok = dtr_ui_group_push(ui, rect, false);
        DTR_ASSERT(ok);
    }
    /* One more should fail */
    ok = dtr_ui_group_push(ui, rect, false);
    DTR_ASSERT(!ok);

    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_group_fit_clamps_child(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;
    dtr_ui_rect_t   parent;
    dtr_ui_rect_t   child;
    dtr_ui_rect_t   fit;

    gfx = dtr_gfx_create(320, 180);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    parent = dtr_ui_rect(10, 10, 80, 60);
    child  = dtr_ui_rect(0, 0, 200, 200); /* extends outside parent */

    dtr_ui_group_push(ui, parent, false);
    fit = dtr_ui_fit(ui, child);

    /* Clamped to intersection */
    DTR_ASSERT_EQ_INT(fit.pos_x, 10);
    DTR_ASSERT_EQ_INT(fit.pos_y, 10);
    DTR_ASSERT_EQ_INT(fit.width, 80);
    DTR_ASSERT_EQ_INT(fit.height, 60);

    dtr_ui_group_pop(ui);
    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_group_fit_no_group_passthrough(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;
    dtr_ui_rect_t   child;
    dtr_ui_rect_t   fit;

    gfx = dtr_gfx_create(320, 180);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    child = dtr_ui_rect(5, 5, 40, 30);
    fit   = dtr_ui_fit(ui, child);

    /* No active group — child returned unchanged */
    DTR_ASSERT_EQ_INT(fit.pos_x, 5);
    DTR_ASSERT_EQ_INT(fit.pos_y, 5);
    DTR_ASSERT_EQ_INT(fit.width, 40);
    DTR_ASSERT_EQ_INT(fit.height, 30);

    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

static void test_group_fit_zero_intersection(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;
    dtr_ui_rect_t   parent;
    dtr_ui_rect_t   child;
    dtr_ui_rect_t   fit;

    gfx = dtr_gfx_create(320, 180);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    parent = dtr_ui_rect(100, 100, 50, 50);
    child  = dtr_ui_rect(0, 0, 20, 20); /* completely outside parent */

    dtr_ui_group_push(ui, parent, false);
    fit = dtr_ui_fit(ui, child);

    DTR_ASSERT_EQ_INT(fit.width, 0);
    DTR_ASSERT_EQ_INT(fit.height, 0);

    dtr_ui_group_pop(ui);
    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  ui_panel                                                           */
/* ------------------------------------------------------------------ */

/* panel with no active group must not crash, and with a sheet loaded
   it should draw without exploding. */
static void test_panel_no_crash(void)
{
    dtr_graphics_t *gfx;
    dtr_ui_t       *ui;
    uint8_t         sheet_data[64];

    gfx = dtr_gfx_create(64, 64);
    DTR_ASSERT_NOT_NULL(gfx);
    ui = dtr_ui_create(gfx);
    DTR_ASSERT_NOT_NULL(ui);

    memset(sheet_data, 5, sizeof(sheet_data));
    gfx->sheet.pixels = sheet_data;
    gfx->sheet.width  = 8;
    gfx->sheet.height = 8;
    gfx->sheet.tile_w = 8;
    gfx->sheet.tile_h = 8;
    gfx->sheet.cols   = 1;
    gfx->sheet.rows   = 1;
    gfx->sheet.count  = 1;

    /* No active group — rect passes through unchanged */
    dtr_ui_panel(ui, dtr_ui_rect(0, 0, 32, 32), 0, 0, 8, 8, 2);

    /* With a group that clips the rect */
    dtr_ui_group_push(ui, dtr_ui_rect(0, 0, 16, 16), false);
    dtr_ui_panel(ui, dtr_ui_rect(0, 0, 32, 32), 0, 0, 8, 8, 2);
    dtr_ui_group_pop(ui);

    gfx->sheet.pixels = NULL;
    dtr_ui_destroy(ui);
    dtr_gfx_destroy(gfx);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("UI Layout");

    DTR_RUN_TEST(test_rect_basic);
    DTR_RUN_TEST(test_inset_basic);
    DTR_RUN_TEST(test_inset_clamps_to_zero);
    DTR_RUN_TEST(test_anchor_top_left);
    DTR_RUN_TEST(test_anchor_center);
    DTR_RUN_TEST(test_anchor_bottom_right);
    DTR_RUN_TEST(test_hsplit_even);
    DTR_RUN_TEST(test_hsplit_with_gap);
    DTR_RUN_TEST(test_vsplit_even);
    DTR_RUN_TEST(test_vsplit_with_gap);
    DTR_RUN_TEST(test_hstack_basic);
    DTR_RUN_TEST(test_hstack_with_gap);
    DTR_RUN_TEST(test_vstack_basic);
    DTR_RUN_TEST(test_vstack_with_gap);
    DTR_RUN_TEST(test_place_center);
    DTR_RUN_TEST(test_place_bottom_right);

    /* Group stack */
    DTR_RUN_TEST(test_group_push_pop_basic);
    DTR_RUN_TEST(test_group_pop_empty_is_safe);
    DTR_RUN_TEST(test_group_nested);
    DTR_RUN_TEST(test_group_depth_limit);
    DTR_RUN_TEST(test_group_fit_clamps_child);
    DTR_RUN_TEST(test_group_fit_no_group_passthrough);
    DTR_RUN_TEST(test_group_fit_zero_intersection);

    /* ui_panel */
    DTR_RUN_TEST(test_panel_no_crash);

    DTR_TEST_END();
}
