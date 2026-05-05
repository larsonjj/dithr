/**
 * \file            ui.h
 * \brief           Stateless UI layout helpers — rect splitting and stacking
 */

#ifndef DTR_UI_H
#define DTR_UI_H

#include "console_defs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Forward declaration — full definition is in graphics.h */
typedef struct dtr_graphics dtr_graphics_t;

/**
 * \brief           Axis-aligned rectangle used by all layout functions
 */
typedef struct dtr_ui_rect {
    int32_t pos_x;
    int32_t pos_y;
    int32_t width;
    int32_t height;
} dtr_ui_rect_t;

/* ---- Group stack ------------------------------------------------------- */

/** Maximum nesting depth of ui_group_push calls. */
#define DTR_UI_GROUP_MAX_DEPTH 16

typedef struct dtr_ui_group_entry {
    dtr_ui_rect_t rect;
    bool          clip_applied;
} dtr_ui_group_entry_t;

/**
 * \brief           Stateful UI context — holds a group/clip stack.
 *                  One instance lives on the console alongside the graphics
 *                  subsystem.
 */
typedef struct dtr_ui {
    dtr_graphics_t      *gfx; /**< Weak ref — owned by console */
    int32_t              depth;
    dtr_ui_group_entry_t stack[DTR_UI_GROUP_MAX_DEPTH];
} dtr_ui_t;

/**
 * \brief           Create and return a new UI context.
 * \param[in]       gfx: Graphics instance (weak ref, must outlive the context)
 */
dtr_ui_t *dtr_ui_create(dtr_graphics_t *gfx);

/**
 * \brief           Destroy a UI context created with dtr_ui_create.
 */
void dtr_ui_destroy(dtr_ui_t *ui);

/**
 * \brief           Push a group rect onto the stack.
 * \param[in]       clip: If true, calls gfx.clip to restrict drawing to rect.
 * \return          true on success; false if the stack is full.
 */
bool dtr_ui_group_push(dtr_ui_t *ui, dtr_ui_rect_t rect, bool clip);

/**
 * \brief           Pop the innermost group.  If clip was applied it is reset.
 *                  Safe to call on an empty stack (no-op).
 */
void dtr_ui_group_pop(dtr_ui_t *ui);

/**
 * \brief           Return the current group rect, or a zero-origin full-screen
 *                  rect if the stack is empty.
 */
dtr_ui_rect_t dtr_ui_group_current(const dtr_ui_t *ui);

/**
 * \brief           Clamp child to the intersection with the current group rect.
 *                  If there is no active group the child is returned unchanged.
 */
dtr_ui_rect_t dtr_ui_fit(const dtr_ui_t *ui, dtr_ui_rect_t child);

/* ---- Construction ------------------------------------------------------ */

/**
 * \brief           Create a rect from position and size
 */
dtr_ui_rect_t dtr_ui_rect(int32_t pos_x, int32_t pos_y, int32_t width, int32_t height);

/* ---- Transforms -------------------------------------------------------- */

/**
 * \brief           Shrink a rect on all four sides by n pixels
 */
dtr_ui_rect_t dtr_ui_inset(dtr_ui_rect_t rect, int32_t inset);

/**
 * \brief           Position a rect using normalised 0–1 screen anchors
 * \param[in]       anc_x: Horizontal anchor (0=left, 0.5=center, 1=right)
 * \param[in]       anc_y: Vertical anchor (0=top, 0.5=middle, 1=bottom)
 * \param[in]       scr_w: Screen width
 * \param[in]       scr_h: Screen height
 */
dtr_ui_rect_t dtr_ui_anchor(double  anc_x,
                            double  anc_y,
                            int32_t width,
                            int32_t height,
                            int32_t scr_w,
                            int32_t scr_h);

/* ---- Splitting --------------------------------------------------------- */

/**
 * \brief           Split a rect horizontally at fraction t
 * \param[out]      left: Left portion
 * \param[out]      right: Right portion
 * \param[in]       frac: Split point 0..1
 * \param[in]       gap: Pixel gap between halves
 */
void dtr_ui_hsplit(dtr_ui_rect_t  rect,
                   double         frac,
                   int32_t        gap,
                   dtr_ui_rect_t *left,
                   dtr_ui_rect_t *right);

/**
 * \brief           Split a rect vertically at fraction t
 * \param[out]      top: Top portion
 * \param[out]      bottom: Bottom portion
 */
void dtr_ui_vsplit(dtr_ui_rect_t  rect,
                   double         frac,
                   int32_t        gap,
                   dtr_ui_rect_t *top,
                   dtr_ui_rect_t *bottom);

/* ---- Stacking ---------------------------------------------------------- */

/**
 * \brief           Divide a rect into n equal columns
 * \param[out]      out: Array of at least n rects
 * \param[in]       count: Number of columns
 * \param[in]       gap: Pixel gap between columns
 */
void dtr_ui_hstack(dtr_ui_rect_t rect, int32_t count, int32_t gap, dtr_ui_rect_t *out);

/**
 * \brief           Divide a rect into n equal rows
 * \param[out]      out: Array of at least n rects
 */
void dtr_ui_vstack(dtr_ui_rect_t rect, int32_t count, int32_t gap, dtr_ui_rect_t *out);

/* ---- Placement --------------------------------------------------------- */

/**
 * \brief           Place a child rect within a parent using 0–1 anchors
 * \param[in]       anc_x: Horizontal placement (0=left, 0.5=center, 1=right)
 * \param[in]       anc_y: Vertical placement (0=top, 0.5=middle, 1=bottom)
 */
dtr_ui_rect_t
dtr_ui_place(dtr_ui_rect_t parent, double anc_x, double anc_y, int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_UI_H */
