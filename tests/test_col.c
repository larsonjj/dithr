/**
 * \file            test_col.c
 * \brief           Unit tests for collision helper functions
 *
 * The col namespace functions are pure math (no engine state needed),
 * so we test the underlying logic directly.
 */

#include "test_harness.h"

/* ------------------------------------------------------------------ */
/*  Rect vs rect (AABB)                                                */
/* ------------------------------------------------------------------ */

static int col_rect(int x1, int y1, int w1, int h1,
                    int x2, int y2, int w2, int h2)
{
    return x1 < x2 + w2 && x1 + w1 > x2 &&
           y1 < y2 + h2 && y1 + h1 > y2;
}

static void test_col_rect_overlap(void)
{
    DTR_ASSERT(col_rect(0, 0, 10, 10, 5, 5, 10, 10));
    DTR_PASS();
}

static void test_col_rect_no_overlap(void)
{
    DTR_ASSERT(!col_rect(0, 0, 10, 10, 20, 20, 10, 10));
    DTR_PASS();
}

static void test_col_rect_edge_touch(void)
{
    /* Touching edges: right edge of A == left edge of B → no overlap */
    DTR_ASSERT(!col_rect(0, 0, 10, 10, 10, 0, 10, 10));
    DTR_PASS();
}

static void test_col_rect_contained(void)
{
    /* B is fully inside A */
    DTR_ASSERT(col_rect(0, 0, 100, 100, 10, 10, 5, 5));
    DTR_PASS();
}

static void test_col_rect_zero_size(void)
{
    /* Zero-size rect should not collide */
    DTR_ASSERT(!col_rect(5, 5, 0, 0, 5, 5, 10, 10));
    DTR_PASS();
}

static void test_col_rect_negative_coords(void)
{
    DTR_ASSERT(col_rect(-5, -5, 10, 10, 0, 0, 10, 10));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Circle vs circle                                                   */
/* ------------------------------------------------------------------ */

static int col_circ(double x1, double y1, double r1,
                    double x2, double y2, double r2)
{
    double dx      = x2 - x1;
    double dy      = y2 - y1;
    double dist_sq = dx * dx + dy * dy;
    double rad_sum = r1 + r2;

    return dist_sq <= rad_sum * rad_sum;
}

static void test_col_circ_overlap(void)
{
    DTR_ASSERT(col_circ(0, 0, 10, 5, 0, 10));
    DTR_PASS();
}

static void test_col_circ_no_overlap(void)
{
    DTR_ASSERT(!col_circ(0, 0, 5, 100, 100, 5));
    DTR_PASS();
}

static void test_col_circ_touching(void)
{
    /* Exactly touching (distance == r1+r2) should be true (<=) */
    DTR_ASSERT(col_circ(0, 0, 5, 10, 0, 5));
    DTR_PASS();
}

static void test_col_circ_concentric(void)
{
    DTR_ASSERT(col_circ(50, 50, 10, 50, 50, 5));
    DTR_PASS();
}

static void test_col_circ_zero_radius(void)
{
    /* Zero radius at same point → touching */
    DTR_ASSERT(col_circ(5, 5, 0, 5, 5, 0));
    /* Zero radius at different point → no overlap */
    DTR_ASSERT(!col_circ(0, 0, 0, 5, 5, 0));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Point inside rect                                                  */
/* ------------------------------------------------------------------ */

static int col_point_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

static void test_col_point_rect_inside(void)
{
    DTR_ASSERT(col_point_rect(5, 5, 0, 0, 10, 10));
    DTR_PASS();
}

static void test_col_point_rect_outside(void)
{
    DTR_ASSERT(!col_point_rect(15, 15, 0, 0, 10, 10));
    DTR_PASS();
}

static void test_col_point_rect_on_edge(void)
{
    /* Top-left corner is inclusive */
    DTR_ASSERT(col_point_rect(0, 0, 0, 0, 10, 10));
    /* Bottom-right edge is exclusive (< not <=) */
    DTR_ASSERT(!col_point_rect(10, 10, 0, 0, 10, 10));
    DTR_PASS();
}

static void test_col_point_rect_negative(void)
{
    DTR_ASSERT(col_point_rect(-3, -3, -5, -5, 10, 10));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Point inside circle                                                */
/* ------------------------------------------------------------------ */

static int col_point_circ(double px, double py, double cx, double cy, double r)
{
    double dx = px - cx;
    double dy = py - cy;

    return dx * dx + dy * dy <= r * r;
}

static void test_col_point_circ_inside(void)
{
    DTR_ASSERT(col_point_circ(5, 5, 5, 5, 10));
    DTR_PASS();
}

static void test_col_point_circ_outside(void)
{
    DTR_ASSERT(!col_point_circ(100, 100, 0, 0, 5));
    DTR_PASS();
}

static void test_col_point_circ_on_edge(void)
{
    /* Point exactly on circumference → true (<=) */
    DTR_ASSERT(col_point_circ(10, 0, 0, 0, 10));
    DTR_PASS();
}

static void test_col_point_circ_center(void)
{
    DTR_ASSERT(col_point_circ(50, 50, 50, 50, 1));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Circle vs rect                                                     */
/* ------------------------------------------------------------------ */

static int col_circ_rect(double cx, double cy, double cr,
                         double rx, double ry, double rw, double rh)
{
    double nearest_x = cx;
    double nearest_y = cy;
    double dx;
    double dy;

    if (nearest_x < rx) {
        nearest_x = rx;
    }
    if (nearest_x > rx + rw) {
        nearest_x = rx + rw;
    }
    if (nearest_y < ry) {
        nearest_y = ry;
    }
    if (nearest_y > ry + rh) {
        nearest_y = ry + rh;
    }

    dx = cx - nearest_x;
    dy = cy - nearest_y;
    return dx * dx + dy * dy <= cr * cr;
}

static void test_col_circ_rect_overlap(void)
{
    /* Circle center inside rect */
    DTR_ASSERT(col_circ_rect(5, 5, 3, 0, 0, 10, 10));
    DTR_PASS();
}

static void test_col_circ_rect_no_overlap(void)
{
    DTR_ASSERT(!col_circ_rect(50, 50, 3, 0, 0, 10, 10));
    DTR_PASS();
}

static void test_col_circ_rect_touching_edge(void)
{
    /* Circle just touching the right edge of rect */
    DTR_ASSERT(col_circ_rect(15, 5, 5, 0, 0, 10, 10));
    DTR_PASS();
}

static void test_col_circ_rect_corner(void)
{
    /* Circle near corner but not touching */
    DTR_ASSERT(!col_circ_rect(14, 14, 3, 0, 0, 10, 10));
    /* Circle near corner and touching */
    DTR_ASSERT(col_circ_rect(12, 12, 5, 0, 0, 10, 10));
    DTR_PASS();
}

static void test_col_circ_rect_circle_contains_rect(void)
{
    /* Large circle completely containing the rect */
    DTR_ASSERT(col_circ_rect(5, 5, 100, 0, 0, 10, 10));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("col (collision helpers)");

    /* rect vs rect */
    DTR_RUN_TEST(test_col_rect_overlap);
    DTR_RUN_TEST(test_col_rect_no_overlap);
    DTR_RUN_TEST(test_col_rect_edge_touch);
    DTR_RUN_TEST(test_col_rect_contained);
    DTR_RUN_TEST(test_col_rect_zero_size);
    DTR_RUN_TEST(test_col_rect_negative_coords);

    /* circle vs circle */
    DTR_RUN_TEST(test_col_circ_overlap);
    DTR_RUN_TEST(test_col_circ_no_overlap);
    DTR_RUN_TEST(test_col_circ_touching);
    DTR_RUN_TEST(test_col_circ_concentric);
    DTR_RUN_TEST(test_col_circ_zero_radius);

    /* point in rect */
    DTR_RUN_TEST(test_col_point_rect_inside);
    DTR_RUN_TEST(test_col_point_rect_outside);
    DTR_RUN_TEST(test_col_point_rect_on_edge);
    DTR_RUN_TEST(test_col_point_rect_negative);

    /* point in circle */
    DTR_RUN_TEST(test_col_point_circ_inside);
    DTR_RUN_TEST(test_col_point_circ_outside);
    DTR_RUN_TEST(test_col_point_circ_on_edge);
    DTR_RUN_TEST(test_col_point_circ_center);

    /* circle vs rect */
    DTR_RUN_TEST(test_col_circ_rect_overlap);
    DTR_RUN_TEST(test_col_circ_rect_no_overlap);
    DTR_RUN_TEST(test_col_circ_rect_touching_edge);
    DTR_RUN_TEST(test_col_circ_rect_corner);
    DTR_RUN_TEST(test_col_circ_rect_circle_contains_rect);

    DTR_TEST_END();
}
