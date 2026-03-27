/**
 * \file            test_ui.c
 * \brief           Unit tests for stateless UI layout helpers
 */

#include "ui.h"
#include "test_harness.h"

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
    DTR_ASSERT_EQ_INT(out.pos_x, 144);    /* (320-32)/2 = 144 */
    DTR_ASSERT_EQ_INT(out.pos_y, 82);     /* (180-16)/2 = 82 */
    DTR_PASS();
}

static void test_anchor_bottom_right(void)
{
    dtr_ui_rect_t out;

    out = dtr_ui_anchor(1.0, 1.0, 32, 16, 320, 180);
    DTR_ASSERT_EQ_INT(out.pos_x, 288);    /* 320-32 */
    DTR_ASSERT_EQ_INT(out.pos_y, 164);    /* 180-16 */
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
    DTR_ASSERT_EQ_INT(right.pos_x, 64);    /* 10 + 50 + 4 */
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
    DTR_ASSERT_EQ_INT(bot.pos_y, 54);    /* 10 + 40 + 4 */
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
    DTR_ASSERT_EQ_INT(child.pos_x, 40);    /* (100-20)/2 */
    DTR_ASSERT_EQ_INT(child.pos_y, 35);    /* (80-10)/2 */
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
    DTR_ASSERT_EQ_INT(child.pos_x, 90);     /* 10 + (100-20) */
    DTR_ASSERT_EQ_INT(child.pos_y, 80);     /* 10 + (80-10) */
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

    DTR_TEST_END();
}
