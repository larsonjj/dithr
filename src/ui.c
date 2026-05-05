/**
 * \file            ui.c
 * \brief           Stateless UI layout helpers — rect splitting and stacking
 */

#include "ui.h"

#include "graphics.h"

#include <SDL3/SDL.h>

/* ---- Group stack ------------------------------------------------------- */

dtr_ui_t *dtr_ui_create(dtr_graphics_t *gfx)
{
    dtr_ui_t *ui;

    ui = SDL_malloc(sizeof(dtr_ui_t));
    if (ui == NULL) {
        return NULL;
    }
    ui->gfx   = gfx;
    ui->depth = 0;
    return ui;
}

void dtr_ui_destroy(dtr_ui_t *ui)
{
    SDL_free(ui);
}

bool dtr_ui_group_push(dtr_ui_t *ui, dtr_ui_rect_t rect, bool clip)
{
    dtr_ui_group_entry_t *entry;

    if (ui == NULL || ui->depth >= DTR_UI_GROUP_MAX_DEPTH) {
        return false;
    }

    entry               = &ui->stack[ui->depth];
    entry->rect         = rect;
    entry->clip_applied = clip;
    ui->depth++;

    if (clip && ui->gfx != NULL) {
        dtr_gfx_clip(ui->gfx, rect.pos_x, rect.pos_y, rect.width, rect.height);
    }
    return true;
}

void dtr_ui_group_pop(dtr_ui_t *ui)
{
    dtr_ui_group_entry_t *entry;

    if (ui == NULL || ui->depth <= 0) {
        return;
    }

    ui->depth--;
    entry = &ui->stack[ui->depth];

    if (entry->clip_applied && ui->gfx != NULL) {
        if (ui->depth > 0) {
            /* Restore parent clip */
            dtr_ui_rect_t pr = ui->stack[ui->depth - 1].rect;
            dtr_gfx_clip(ui->gfx, pr.pos_x, pr.pos_y, pr.width, pr.height);
        } else {
            dtr_gfx_clip_reset(ui->gfx);
        }
    }
}

dtr_ui_rect_t dtr_ui_group_current(const dtr_ui_t *ui)
{
    if (ui == NULL || ui->depth <= 0) {
        return dtr_ui_rect(0, 0, 0, 0);
    }
    return ui->stack[ui->depth - 1].rect;
}

dtr_ui_rect_t dtr_ui_fit(const dtr_ui_t *ui, dtr_ui_rect_t child)
{
    dtr_ui_rect_t parent;
    dtr_ui_rect_t out;
    int32_t       x0;
    int32_t       y0;
    int32_t       x1;
    int32_t       y1;
    int32_t       px0;
    int32_t       py0;
    int32_t       px1;
    int32_t       py1;

    if (ui == NULL || ui->depth <= 0) {
        return child;
    }

    parent = ui->stack[ui->depth - 1].rect;

    x0  = child.pos_x;
    y0  = child.pos_y;
    x1  = child.pos_x + child.width;
    y1  = child.pos_y + child.height;
    px0 = parent.pos_x;
    py0 = parent.pos_y;
    px1 = parent.pos_x + parent.width;
    py1 = parent.pos_y + parent.height;

    if (x0 < px0) {
        x0 = px0;
    }
    if (y0 < py0) {
        y0 = py0;
    }
    if (x1 > px1) {
        x1 = px1;
    }
    if (y1 > py1) {
        y1 = py1;
    }

    out.pos_x  = x0;
    out.pos_y  = y0;
    out.width  = x1 - x0 > 0 ? x1 - x0 : 0;
    out.height = y1 - y0 > 0 ? y1 - y0 : 0;
    return out;
}

void dtr_ui_panel(dtr_ui_t     *ui,
                  dtr_ui_rect_t rect,
                  int32_t       sx,
                  int32_t       sy,
                  int32_t       sw,
                  int32_t       sh,
                  int32_t       border)
{
    dtr_ui_rect_t clamped;

    if (ui == NULL) {
        return;
    }
    clamped = dtr_ui_fit(ui, rect);
    dtr_gfx_nine_slice(ui->gfx,
                       sx,
                       sy,
                       sw,
                       sh,
                       clamped.pos_x,
                       clamped.pos_y,
                       clamped.width,
                       clamped.height,
                       border);
}

dtr_ui_rect_t dtr_ui_rect(int32_t pos_x, int32_t pos_y, int32_t width, int32_t height)
{
    dtr_ui_rect_t rect;

    rect.pos_x  = pos_x;
    rect.pos_y  = pos_y;
    rect.width  = width;
    rect.height = height;
    return rect;
}

dtr_ui_rect_t dtr_ui_inset(dtr_ui_rect_t rect, int32_t inset)
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

dtr_ui_rect_t dtr_ui_anchor(double  anc_x,
                            double  anc_y,
                            int32_t width,
                            int32_t height,
                            int32_t scr_w,
                            int32_t scr_h)
{
    dtr_ui_rect_t out;

    out.pos_x  = (int32_t)(anc_x * (double)(scr_w - width));
    out.pos_y  = (int32_t)(anc_y * (double)(scr_h - height));
    out.width  = width;
    out.height = height;
    return out;
}

void dtr_ui_hsplit(dtr_ui_rect_t  rect,
                   double         frac,
                   int32_t        gap,
                   dtr_ui_rect_t *left,
                   dtr_ui_rect_t *right)
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

void dtr_ui_vsplit(dtr_ui_rect_t  rect,
                   double         frac,
                   int32_t        gap,
                   dtr_ui_rect_t *top,
                   dtr_ui_rect_t *bottom)
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

void dtr_ui_hstack(dtr_ui_rect_t rect, int32_t count, int32_t gap, dtr_ui_rect_t *out)
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

void dtr_ui_vstack(dtr_ui_rect_t rect, int32_t count, int32_t gap, dtr_ui_rect_t *out)
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
dtr_ui_place(dtr_ui_rect_t parent, double anc_x, double anc_y, int32_t width, int32_t height)
{
    dtr_ui_rect_t out;

    out.pos_x  = parent.pos_x + (int32_t)(anc_x * (double)(parent.width - width));
    out.pos_y  = parent.pos_y + (int32_t)(anc_y * (double)(parent.height - height));
    out.width  = width;
    out.height = height;
    return out;
}
