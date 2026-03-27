/**
 * \file            ui.c
 * \brief           Stateless UI layout helpers — rect splitting and stacking
 */

#include "ui.h"

dtr_ui_rect_t
dtr_ui_rect(int32_t pos_x, int32_t pos_y, int32_t width, int32_t height)
{
    dtr_ui_rect_t rect;

    rect.pos_x  = pos_x;
    rect.pos_y  = pos_y;
    rect.width  = width;
    rect.height = height;
    return rect;
}

dtr_ui_rect_t
dtr_ui_inset(dtr_ui_rect_t rect, int32_t inset)
{
    dtr_ui_rect_t out;

    out.pos_x  = rect.pos_x + inset;
    out.pos_y  = rect.pos_y + inset;
    out.width  = rect.width - inset * 2;
    out.height = rect.height - inset * 2;
    if (out.width < 0) {
        out.width = 0;
    }
    if (out.height < 0) {
        out.height = 0;
    }
    return out;
}

dtr_ui_rect_t
dtr_ui_anchor(double anc_x, double anc_y, int32_t width, int32_t height,
              int32_t scr_w, int32_t scr_h)
{
    dtr_ui_rect_t out;

    out.pos_x  = (int32_t)(anc_x * (double)(scr_w - width));
    out.pos_y  = (int32_t)(anc_y * (double)(scr_h - height));
    out.width  = width;
    out.height = height;
    return out;
}

void
dtr_ui_hsplit(dtr_ui_rect_t rect, double frac, int32_t gap,
              dtr_ui_rect_t *left, dtr_ui_rect_t *right)
{
    int32_t usable;
    int32_t left_w;

    usable = rect.width - gap;
    if (usable < 0) {
        usable = 0;
    }
    left_w = (int32_t)(frac * (double)usable);

    left->pos_x  = rect.pos_x;
    left->pos_y  = rect.pos_y;
    left->width  = left_w;
    left->height = rect.height;

    right->pos_x  = rect.pos_x + left_w + gap;
    right->pos_y  = rect.pos_y;
    right->width  = usable - left_w;
    right->height = rect.height;
}

void
dtr_ui_vsplit(dtr_ui_rect_t rect, double frac, int32_t gap,
              dtr_ui_rect_t *top, dtr_ui_rect_t *bottom)
{
    int32_t usable;
    int32_t top_h;

    usable = rect.height - gap;
    if (usable < 0) {
        usable = 0;
    }
    top_h = (int32_t)(frac * (double)usable);

    top->pos_x  = rect.pos_x;
    top->pos_y  = rect.pos_y;
    top->width  = rect.width;
    top->height = top_h;

    bottom->pos_x  = rect.pos_x;
    bottom->pos_y  = rect.pos_y + top_h + gap;
    bottom->width  = rect.width;
    bottom->height = usable - top_h;
}

void
dtr_ui_hstack(dtr_ui_rect_t rect, int32_t count, int32_t gap,
              dtr_ui_rect_t *out)
{
    int32_t usable;
    int32_t cell_w;
    int32_t cursor;
    int32_t idx;

    if (count <= 0) {
        return;
    }
    usable = rect.width - gap * (count - 1);
    if (usable < 0) {
        usable = 0;
    }
    cell_w = usable / count;
    cursor = rect.pos_x;

    for (idx = 0; idx < count; ++idx) {
        out[idx].pos_x  = cursor;
        out[idx].pos_y  = rect.pos_y;
        out[idx].width  = cell_w;
        out[idx].height = rect.height;
        cursor += cell_w + gap;
    }
}

void
dtr_ui_vstack(dtr_ui_rect_t rect, int32_t count, int32_t gap,
              dtr_ui_rect_t *out)
{
    int32_t usable;
    int32_t cell_h;
    int32_t cursor;
    int32_t idx;

    if (count <= 0) {
        return;
    }
    usable = rect.height - gap * (count - 1);
    if (usable < 0) {
        usable = 0;
    }
    cell_h = usable / count;
    cursor = rect.pos_y;

    for (idx = 0; idx < count; ++idx) {
        out[idx].pos_x  = rect.pos_x;
        out[idx].pos_y  = cursor;
        out[idx].width  = rect.width;
        out[idx].height = cell_h;
        cursor += cell_h + gap;
    }
}

dtr_ui_rect_t
dtr_ui_place(dtr_ui_rect_t parent, double anc_x, double anc_y,
             int32_t width, int32_t height)
{
    dtr_ui_rect_t out;

    out.pos_x  = parent.pos_x + (int32_t)(anc_x * (double)(parent.width - width));
    out.pos_y  = parent.pos_y + (int32_t)(anc_y * (double)(parent.height - height));
    out.width  = width;
    out.height = height;
    return out;
}
