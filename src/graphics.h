/**
 * \file            graphics.h
 * \brief           Framebuffer, palette, sprites, and all drawing operations
 */

#ifndef DTR_GRAPHICS_H
#define DTR_GRAPHICS_H

#include "console_defs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ------------------------------------------------------------------------ */
/*  Sprite sheet                                                             */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Sprite sheet stored as palette-indexed pixels
 */
typedef struct dtr_sprite_sheet {
    uint8_t *pixels;                     /**< Palette-indexed pixel data */
    int32_t  width;                      /**< Sheet width in pixels */
    int32_t  height;                     /**< Sheet height in pixels */
    int32_t  tile_w;                     /**< Single tile width */
    int32_t  tile_h;                     /**< Single tile height */
    int32_t  cols;                       /**< Tiles per row */
    int32_t  rows;                       /**< Tile rows */
    int32_t  count;                      /**< Total tile count */
    uint8_t  flags[CONSOLE_MAX_SPRITES]; /**< Per-sprite flag bitmask */
} dtr_sprite_sheet_t;

/* ------------------------------------------------------------------------ */
/*  Screen transitions                                                       */
/* ------------------------------------------------------------------------ */

/**
 * \\brief           Transition type enumeration
 */
typedef enum dtr_transition_type {
    DTR_TRANS_NONE     = 0,
    DTR_TRANS_FADE     = 1,
    DTR_TRANS_WIPE     = 2,
    DTR_TRANS_DISSOLVE = 3,
} dtr_transition_type_t;

/**
 * \\brief           Direction for wipe transitions
 */
typedef enum dtr_wipe_dir {
    DTR_WIPE_LEFT  = 0,
    DTR_WIPE_RIGHT = 1,
    DTR_WIPE_UP    = 2,
    DTR_WIPE_DOWN  = 3,
} dtr_wipe_dir_t;

/**
 * \\brief           Screen transition state
 */
typedef struct dtr_transition {
    dtr_transition_type_t type;
    int32_t               frame;     /**< Current frame of the transition */
    int32_t               duration;  /**< Total frames for the transition */
    uint8_t               color;     /**< Target colour for fade/dissolve */
    dtr_wipe_dir_t        direction; /**< Wipe direction */
} dtr_transition_t;

/* ------------------------------------------------------------------------ */
/*  Custom font                                                              */
/* ------------------------------------------------------------------------ */

/**
 * \\brief           Custom monospaced font loaded from the sprite sheet
 */
typedef struct dtr_custom_font {
    bool    active;   /**< True when a custom font is in use */
    int32_t sx;       /**< Source X in sprite sheet pixels */
    int32_t sy;       /**< Source Y in sprite sheet pixels */
    int32_t char_w;   /**< Character width in pixels */
    int32_t char_h;   /**< Character height in pixels */
    int32_t cols;     /**< Characters per row in the font grid */
    char    first;    /**< First ASCII character in the grid */
    int32_t count;    /**< Total number of characters */
} dtr_custom_font_t;

/* ------------------------------------------------------------------------ */
/*  Draw list (sprite batch)                                                 */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Draw command types
 */
typedef enum dtr_draw_cmd_type {
    DTR_DRAW_SPR        = 0,
    DTR_DRAW_SSPR       = 1,
    DTR_DRAW_SPR_ROT    = 2,
    DTR_DRAW_SPR_AFFINE = 3,
} dtr_draw_cmd_type_t;

/**
 * \brief           A single queued draw command
 */
typedef struct dtr_draw_cmd {
    dtr_draw_cmd_type_t type;
    int32_t             layer;     /**< Sort key — lower layers drawn first */
    int32_t             order;     /**< Insertion order for stable sort */
    union {
        struct {
            int32_t idx, x, y, w, h;
            bool    fx, fy;
        } spr;
        struct {
            int32_t sx, sy, sw, sh, dx, dy, dw, dh;
        } sspr;
        struct {
            int32_t idx, x, y, cx, cy;
            float   angle;
        } spr_rot;
        struct {
            int32_t idx, x, y;
            float   ox, oy, rx, ry;
        } spr_affine;
    } u;
} dtr_draw_cmd_t;

/**
 * \brief           Draw list state
 */
typedef struct dtr_draw_list {
    dtr_draw_cmd_t cmds[CONSOLE_MAX_DRAW_CMDS];
    int32_t        count;   /**< Number of queued commands */
    bool           active;  /**< True between dl_begin and dl_end */
} dtr_draw_list_t;

/* ------------------------------------------------------------------------ */
/*  Graphics state                                                           */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Complete graphics subsystem state
 */
struct dtr_graphics {
    /* Palette-indexed framebuffer */
    uint8_t framebuffer[CONSOLE_FB_WIDTH * CONSOLE_FB_HEIGHT];

    /* RGBA pixel buffer for uploading to texture */
    uint32_t pixels[CONSOLE_FB_WIDTH * CONSOLE_FB_HEIGHT];

    /* Draw palette — maps source index → draw index */
    uint8_t draw_pal[CONSOLE_PALETTE_SIZE];

    /* Screen palette — maps draw index → display index */
    uint8_t screen_pal[CONSOLE_PALETTE_SIZE];

    /* RGBA colours for each palette slot */
    uint32_t colors[CONSOLE_PALETTE_SIZE];

    /* Transparency flags (one per palette slot) */
    bool transparent[CONSOLE_PALETTE_SIZE];

    /* Current drawing colour index */
    uint8_t color;

    /* Text cursor position */
    int32_t cursor_x;
    int32_t cursor_y;

    /* Fill pattern (4x4 bitmask, 16 bits) */
    uint16_t fill_pattern;

    /* Camera offset */
    int32_t camera_x;
    int32_t camera_y;

    /* Clip rectangle */
    int32_t clip_x;
    int32_t clip_y;
    int32_t clip_w;
    int32_t clip_h;

    /* Sprite sheet */
    dtr_sprite_sheet_t sheet;

    /* Custom font (loaded from sprite sheet region) */
    dtr_custom_font_t custom_font;

    /* Screen transition */
    dtr_transition_t transition;

    /* Draw list (sprite batch) */
    dtr_draw_list_t draw_list;

    /* Framebuffer dimensions (may be overridden by cart) */
    int32_t width;
    int32_t height;
};

/* ------------------------------------------------------------------------ */
/*  Lifecycle                                                                */
/* ------------------------------------------------------------------------ */

dtr_graphics_t *dtr_gfx_create(int32_t width, int32_t height);
void            dtr_gfx_destroy(dtr_graphics_t *gfx);
void            dtr_gfx_reset(dtr_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Core drawing                                                             */
/* ------------------------------------------------------------------------ */

void    dtr_gfx_cls(dtr_graphics_t *gfx, uint8_t col);
void    dtr_gfx_pset(dtr_graphics_t *gfx, int32_t x, int32_t y, uint8_t col);
uint8_t dtr_gfx_pget(dtr_graphics_t *gfx, int32_t x, int32_t y);

/* ------------------------------------------------------------------------ */
/*  Primitives                                                               */
/* ------------------------------------------------------------------------ */

void dtr_gfx_line(dtr_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col);
void dtr_gfx_rect(dtr_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col);
void dtr_gfx_rectfill(dtr_graphics_t *gfx,
                      int32_t         x0,
                      int32_t         y0,
                      int32_t         x1,
                      int32_t         y1,
                      uint8_t         col);
void dtr_gfx_tilemap(dtr_graphics_t *gfx,
                     const uint8_t  *tiles,
                     int32_t         map_w,
                     int32_t         map_h,
                     int32_t         tile_w,
                     int32_t         tile_h,
                     const uint8_t  *colors,
                     int32_t         num_colors);
void dtr_gfx_circ(dtr_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col);
void dtr_gfx_circfill(dtr_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col);
void dtr_gfx_tri(dtr_graphics_t *gfx,
                 int32_t         x0,
                 int32_t         y0,
                 int32_t         x1,
                 int32_t         y1,
                 int32_t         x2,
                 int32_t         y2,
                 uint8_t         col);
void dtr_gfx_trifill(dtr_graphics_t *gfx,
                     int32_t         x0,
                     int32_t         y0,
                     int32_t         x1,
                     int32_t         y1,
                     int32_t         x2,
                     int32_t         y2,
                     uint8_t         col);

/**
 * \\brief           Draw a polygon outline from an array of (x,y) pairs
 * \\param[in]       pts: Flat array [x0,y0, x1,y1, ...] of vertex coordinates
 * \\param[in]       count: Number of vertices (pts has count*2 elements)
 * \\param[in]       col: Palette colour
 */
void dtr_gfx_poly(dtr_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col);

/**
 * \\brief           Fill a convex polygon using fan-of-triangles
 * \\param[in]       pts: Flat array [x0,y0, x1,y1, ...] of vertex coordinates
 * \\param[in]       count: Number of vertices (pts has count*2 elements)
 * \\param[in]       col: Palette colour
 */
void dtr_gfx_polyfill(dtr_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col);

/* ------------------------------------------------------------------------ */
/*  Text                                                                     */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Print a string and return the x advance in pixels
 */
int32_t dtr_gfx_print(dtr_graphics_t *gfx, const char *str, int32_t x, int32_t y, uint8_t col);
/**
 * \\brief           Set a custom font from a sprite sheet region
 * \\param[in]       sx: Source X of the font grid on the sheet
 * \\param[in]       sy: Source Y of the font grid on the sheet
 * \\param[in]       char_w: Width of each character cell
 * \\param[in]       char_h: Height of each character cell
 * \\param[in]       first: First ASCII character in the grid (e.g. ' ')
 * \\param[in]       count: Number of characters in the grid
 */
void dtr_gfx_font(dtr_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         char_w,
                  int32_t         char_h,
                  char            first,
                  int32_t         count);

/**
 * \\brief           Reset to the built-in 4x6 font
 */
void dtr_gfx_font_reset(dtr_graphics_t *gfx);
/* ------------------------------------------------------------------------ */
/*  Sprites                                                                  */
/* ------------------------------------------------------------------------ */

void dtr_gfx_spr(dtr_graphics_t *gfx,
                 int32_t         idx,
                 int32_t         x,
                 int32_t         y,
                 int32_t         w_tiles,
                 int32_t         h_tiles,
                 bool            flip_x,
                 bool            flip_y);
void dtr_gfx_sspr(dtr_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         sw,
                  int32_t         sh,
                  int32_t         dx,
                  int32_t         dy,
                  int32_t         dw,
                  int32_t         dh);
void dtr_gfx_spr_rot(dtr_graphics_t *gfx,
                     int32_t         idx,
                     int32_t         x,
                     int32_t         y,
                     float           angle,
                     int32_t         cx,
                     int32_t         cy);
void dtr_gfx_spr_affine(dtr_graphics_t *gfx,
                        int32_t         idx,
                        int32_t         x,
                        int32_t         y,
                        float           origin_x,
                        float           origin_y,
                        float           rot_x,
                        float           rot_y);

/* ------------------------------------------------------------------------ */
/*  Sprite flags                                                             */
/* ------------------------------------------------------------------------ */

uint8_t dtr_gfx_fget(dtr_graphics_t *gfx, int32_t idx);
bool    dtr_gfx_fget_bit(dtr_graphics_t *gfx, int32_t idx, int32_t flag);
void    dtr_gfx_fset(dtr_graphics_t *gfx, int32_t idx, uint8_t mask);
void    dtr_gfx_fset_bit(dtr_graphics_t *gfx, int32_t idx, int32_t flag, bool val);

/* ------------------------------------------------------------------------ */
/*  Palette                                                                  */
/* ------------------------------------------------------------------------ */

void dtr_gfx_pal(dtr_graphics_t *gfx, uint8_t src, uint8_t dst, bool screen);
void dtr_gfx_pal_reset(dtr_graphics_t *gfx);
void dtr_gfx_palt(dtr_graphics_t *gfx, uint8_t col, bool trans);
void dtr_gfx_palt_reset(dtr_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  State                                                                    */
/* ------------------------------------------------------------------------ */

void dtr_gfx_camera(dtr_graphics_t *gfx, int32_t x, int32_t y);
void dtr_gfx_clip(dtr_graphics_t *gfx, int32_t x, int32_t y, int32_t w, int32_t h);
void dtr_gfx_clip_reset(dtr_graphics_t *gfx);
void dtr_gfx_fillp(dtr_graphics_t *gfx, uint16_t pattern);
void dtr_gfx_color(dtr_graphics_t *gfx, uint8_t col);
void dtr_gfx_cursor(dtr_graphics_t *gfx, int32_t x, int32_t y);

/* ------------------------------------------------------------------------ */
/*  Flip — convert palette framebuffer to RGBA for SDL texture               */
/* ------------------------------------------------------------------------ */

void dtr_gfx_flip_to(dtr_graphics_t *gfx, uint32_t *dst);
void dtr_gfx_flip(dtr_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Screen transitions                                                       */
/* ------------------------------------------------------------------------ */

void dtr_gfx_fade(dtr_graphics_t *gfx, uint8_t color, int32_t frames);
void dtr_gfx_wipe(dtr_graphics_t *gfx, int32_t direction, uint8_t color, int32_t frames);
void dtr_gfx_dissolve(dtr_graphics_t *gfx, uint8_t color, int32_t frames);
bool dtr_gfx_transitioning(dtr_graphics_t *gfx);
void dtr_gfx_transition_update_buf(dtr_graphics_t *gfx, uint32_t *pixels);
void dtr_gfx_transition_update(dtr_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Draw list (sprite batch)                                                 */
/* ------------------------------------------------------------------------ */

void dtr_gfx_dl_begin(dtr_graphics_t *gfx);
void dtr_gfx_dl_spr(dtr_graphics_t *gfx,
                    int32_t         layer,
                    int32_t         idx,
                    int32_t         x,
                    int32_t         y,
                    int32_t         w,
                    int32_t         h,
                    bool            flip_x,
                    bool            flip_y);
void dtr_gfx_dl_sspr(dtr_graphics_t *gfx,
                     int32_t         layer,
                     int32_t         sx,
                     int32_t         sy,
                     int32_t         sw,
                     int32_t         sh,
                     int32_t         dx,
                     int32_t         dy,
                     int32_t         dw,
                     int32_t         dh);
void dtr_gfx_dl_spr_rot(dtr_graphics_t *gfx,
                        int32_t         layer,
                        int32_t         idx,
                        int32_t         x,
                        int32_t         y,
                        float           angle,
                        int32_t         cx,
                        int32_t         cy);
void dtr_gfx_dl_spr_affine(dtr_graphics_t *gfx,
                           int32_t         layer,
                           int32_t         idx,
                           int32_t         x,
                           int32_t         y,
                           float           origin_x,
                           float           origin_y,
                           float           rot_x,
                           float           rot_y);
void dtr_gfx_dl_end(dtr_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Default palette initialisation                                           */
/* ------------------------------------------------------------------------ */

void dtr_gfx_init_default_palette(dtr_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Sprite sheet loading                                                     */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Load sprite sheet from raw RGBA data and quantise to palette
 * \param[in]       gfx: Graphics state
 * \param[in]       rgba: RGBA pixel data
 * \param[in]       width: Image width
 * \param[in]       height: Image height
 * \param[in]       tile_w: Tile width
 * \param[in]       tile_h: Tile height
 */
bool dtr_gfx_load_sheet(dtr_graphics_t *gfx,
                        const uint8_t * rgba,
                        int32_t         width,
                        int32_t         height,
                        int32_t         tile_w,
                        int32_t         tile_h);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_GRAPHICS_H */
