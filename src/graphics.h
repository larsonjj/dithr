/**
 * \file            graphics.h
 * \brief           Framebuffer, palette, sprites, and all drawing operations
 */

#ifndef MVN_GRAPHICS_H
#define MVN_GRAPHICS_H

#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ------------------------------------------------------------------------ */
/*  Sprite sheet                                                             */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Sprite sheet stored as palette-indexed pixels
 */
typedef struct mvn_sprite_sheet {
    uint8_t *pixels;                     /**< Palette-indexed pixel data */
    int32_t  width;                      /**< Sheet width in pixels */
    int32_t  height;                     /**< Sheet height in pixels */
    int32_t  tile_w;                     /**< Single tile width */
    int32_t  tile_h;                     /**< Single tile height */
    int32_t  cols;                       /**< Tiles per row */
    int32_t  rows;                       /**< Tile rows */
    int32_t  count;                      /**< Total tile count */
    uint8_t  flags[CONSOLE_MAX_SPRITES]; /**< Per-sprite flag bitmask */
} mvn_sprite_sheet_t;

/* ------------------------------------------------------------------------ */
/*  Screen transitions                                                       */
/* ------------------------------------------------------------------------ */

/**
 * \\brief           Transition type enumeration
 */
typedef enum mvn_transition_type {
    MVN_TRANS_NONE     = 0,
    MVN_TRANS_FADE     = 1,
    MVN_TRANS_WIPE     = 2,
    MVN_TRANS_DISSOLVE = 3,
} mvn_transition_type_t;

/**
 * \\brief           Direction for wipe transitions
 */
typedef enum mvn_wipe_dir {
    MVN_WIPE_LEFT  = 0,
    MVN_WIPE_RIGHT = 1,
    MVN_WIPE_UP    = 2,
    MVN_WIPE_DOWN  = 3,
} mvn_wipe_dir_t;

/**
 * \\brief           Screen transition state
 */
typedef struct mvn_transition {
    mvn_transition_type_t type;
    int32_t               frame;     /**< Current frame of the transition */
    int32_t               duration;  /**< Total frames for the transition */
    uint8_t               color;     /**< Target colour for fade/dissolve */
    mvn_wipe_dir_t        direction; /**< Wipe direction */
} mvn_transition_t;

/* ------------------------------------------------------------------------ */
/*  Custom font                                                              */
/* ------------------------------------------------------------------------ */

/**
 * \\brief           Custom monospaced font loaded from the sprite sheet
 */
typedef struct mvn_custom_font {
    bool    active;   /**< True when a custom font is in use */
    int32_t sx;       /**< Source X in sprite sheet pixels */
    int32_t sy;       /**< Source Y in sprite sheet pixels */
    int32_t char_w;   /**< Character width in pixels */
    int32_t char_h;   /**< Character height in pixels */
    int32_t cols;     /**< Characters per row in the font grid */
    char    first;    /**< First ASCII character in the grid */
    int32_t count;    /**< Total number of characters */
} mvn_custom_font_t;

/* ------------------------------------------------------------------------ */
/*  Draw list (sprite batch)                                                 */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Draw command types
 */
typedef enum mvn_draw_cmd_type {
    MVN_DRAW_SPR        = 0,
    MVN_DRAW_SSPR       = 1,
    MVN_DRAW_SPR_ROT    = 2,
    MVN_DRAW_SPR_AFFINE = 3,
} mvn_draw_cmd_type_t;

/**
 * \brief           A single queued draw command
 */
typedef struct mvn_draw_cmd {
    mvn_draw_cmd_type_t type;
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
} mvn_draw_cmd_t;

/**
 * \brief           Draw list state
 */
typedef struct mvn_draw_list {
    mvn_draw_cmd_t cmds[CONSOLE_MAX_DRAW_CMDS];
    int32_t        count;   /**< Number of queued commands */
    bool           active;  /**< True between dl_begin and dl_end */
} mvn_draw_list_t;

/* ------------------------------------------------------------------------ */
/*  Graphics state                                                           */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Complete graphics subsystem state
 */
struct mvn_graphics {
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
    mvn_sprite_sheet_t sheet;

    /* Custom font (loaded from sprite sheet region) */
    mvn_custom_font_t custom_font;

    /* Screen transition */
    mvn_transition_t transition;

    /* Draw list (sprite batch) */
    mvn_draw_list_t draw_list;

    /* Framebuffer dimensions (may be overridden by cart) */
    int32_t width;
    int32_t height;
};

/* ------------------------------------------------------------------------ */
/*  Lifecycle                                                                */
/* ------------------------------------------------------------------------ */

mvn_graphics_t *mvn_gfx_create(int32_t width, int32_t height);
void            mvn_gfx_destroy(mvn_graphics_t *gfx);
void            mvn_gfx_reset(mvn_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Core drawing                                                             */
/* ------------------------------------------------------------------------ */

void    mvn_gfx_cls(mvn_graphics_t *gfx, uint8_t col);
void    mvn_gfx_pset(mvn_graphics_t *gfx, int32_t x, int32_t y, uint8_t col);
uint8_t mvn_gfx_pget(mvn_graphics_t *gfx, int32_t x, int32_t y);

/* ------------------------------------------------------------------------ */
/*  Primitives                                                               */
/* ------------------------------------------------------------------------ */

void mvn_gfx_line(mvn_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col);
void mvn_gfx_rect(mvn_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col);
void mvn_gfx_rectfill(mvn_graphics_t *gfx,
                      int32_t         x0,
                      int32_t         y0,
                      int32_t         x1,
                      int32_t         y1,
                      uint8_t         col);
void mvn_gfx_circ(mvn_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col);
void mvn_gfx_circfill(mvn_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col);
void mvn_gfx_tri(mvn_graphics_t *gfx,
                 int32_t         x0,
                 int32_t         y0,
                 int32_t         x1,
                 int32_t         y1,
                 int32_t         x2,
                 int32_t         y2,
                 uint8_t         col);
void mvn_gfx_trifill(mvn_graphics_t *gfx,
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
void mvn_gfx_poly(mvn_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col);

/**
 * \\brief           Fill a convex polygon using fan-of-triangles
 * \\param[in]       pts: Flat array [x0,y0, x1,y1, ...] of vertex coordinates
 * \\param[in]       count: Number of vertices (pts has count*2 elements)
 * \\param[in]       col: Palette colour
 */
void mvn_gfx_polyfill(mvn_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col);

/* ------------------------------------------------------------------------ */
/*  Text                                                                     */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Print a string and return the x advance in pixels
 */
int32_t mvn_gfx_print(mvn_graphics_t *gfx, const char *str, int32_t x, int32_t y, uint8_t col);
/**
 * \\brief           Set a custom font from a sprite sheet region
 * \\param[in]       sx: Source X of the font grid on the sheet
 * \\param[in]       sy: Source Y of the font grid on the sheet
 * \\param[in]       char_w: Width of each character cell
 * \\param[in]       char_h: Height of each character cell
 * \\param[in]       first: First ASCII character in the grid (e.g. ' ')
 * \\param[in]       count: Number of characters in the grid
 */
void mvn_gfx_font(mvn_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         char_w,
                  int32_t         char_h,
                  char            first,
                  int32_t         count);

/**
 * \\brief           Reset to the built-in 4x6 font
 */
void mvn_gfx_font_reset(mvn_graphics_t *gfx);
/* ------------------------------------------------------------------------ */
/*  Sprites                                                                  */
/* ------------------------------------------------------------------------ */

void mvn_gfx_spr(mvn_graphics_t *gfx,
                 int32_t         idx,
                 int32_t         x,
                 int32_t         y,
                 int32_t         w_tiles,
                 int32_t         h_tiles,
                 bool            flip_x,
                 bool            flip_y);
void mvn_gfx_sspr(mvn_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         sw,
                  int32_t         sh,
                  int32_t         dx,
                  int32_t         dy,
                  int32_t         dw,
                  int32_t         dh);
void mvn_gfx_spr_rot(mvn_graphics_t *gfx,
                     int32_t         idx,
                     int32_t         x,
                     int32_t         y,
                     float           angle,
                     int32_t         cx,
                     int32_t         cy);
void mvn_gfx_spr_affine(mvn_graphics_t *gfx,
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

uint8_t mvn_gfx_fget(mvn_graphics_t *gfx, int32_t idx);
bool    mvn_gfx_fget_bit(mvn_graphics_t *gfx, int32_t idx, int32_t flag);
void    mvn_gfx_fset(mvn_graphics_t *gfx, int32_t idx, uint8_t mask);
void    mvn_gfx_fset_bit(mvn_graphics_t *gfx, int32_t idx, int32_t flag, bool val);

/* ------------------------------------------------------------------------ */
/*  Palette                                                                  */
/* ------------------------------------------------------------------------ */

void mvn_gfx_pal(mvn_graphics_t *gfx, uint8_t src, uint8_t dst, bool screen);
void mvn_gfx_pal_reset(mvn_graphics_t *gfx);
void mvn_gfx_palt(mvn_graphics_t *gfx, uint8_t col, bool trans);
void mvn_gfx_palt_reset(mvn_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  State                                                                    */
/* ------------------------------------------------------------------------ */

void mvn_gfx_camera(mvn_graphics_t *gfx, int32_t x, int32_t y);
void mvn_gfx_clip(mvn_graphics_t *gfx, int32_t x, int32_t y, int32_t w, int32_t h);
void mvn_gfx_clip_reset(mvn_graphics_t *gfx);
void mvn_gfx_fillp(mvn_graphics_t *gfx, uint16_t pattern);
void mvn_gfx_color(mvn_graphics_t *gfx, uint8_t col);
void mvn_gfx_cursor(mvn_graphics_t *gfx, int32_t x, int32_t y);

/* ------------------------------------------------------------------------ */
/*  Flip — convert palette framebuffer to RGBA for SDL texture               */
/* ------------------------------------------------------------------------ */

void mvn_gfx_flip(mvn_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Screen transitions                                                       */
/* ------------------------------------------------------------------------ */

void mvn_gfx_fade(mvn_graphics_t *gfx, uint8_t color, int32_t frames);
void mvn_gfx_wipe(mvn_graphics_t *gfx, int32_t direction, uint8_t color, int32_t frames);
void mvn_gfx_dissolve(mvn_graphics_t *gfx, uint8_t color, int32_t frames);
bool mvn_gfx_transitioning(mvn_graphics_t *gfx);
void mvn_gfx_transition_update(mvn_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Draw list (sprite batch)                                                 */
/* ------------------------------------------------------------------------ */

void mvn_gfx_dl_begin(mvn_graphics_t *gfx);
void mvn_gfx_dl_spr(mvn_graphics_t *gfx,
                    int32_t         layer,
                    int32_t         idx,
                    int32_t         x,
                    int32_t         y,
                    int32_t         w,
                    int32_t         h,
                    bool            flip_x,
                    bool            flip_y);
void mvn_gfx_dl_sspr(mvn_graphics_t *gfx,
                     int32_t         layer,
                     int32_t         sx,
                     int32_t         sy,
                     int32_t         sw,
                     int32_t         sh,
                     int32_t         dx,
                     int32_t         dy,
                     int32_t         dw,
                     int32_t         dh);
void mvn_gfx_dl_spr_rot(mvn_graphics_t *gfx,
                        int32_t         layer,
                        int32_t         idx,
                        int32_t         x,
                        int32_t         y,
                        float           angle,
                        int32_t         cx,
                        int32_t         cy);
void mvn_gfx_dl_spr_affine(mvn_graphics_t *gfx,
                           int32_t         layer,
                           int32_t         idx,
                           int32_t         x,
                           int32_t         y,
                           float           origin_x,
                           float           origin_y,
                           float           rot_x,
                           float           rot_y);
void mvn_gfx_dl_end(mvn_graphics_t *gfx);

/* ------------------------------------------------------------------------ */
/*  Default palette initialisation                                           */
/* ------------------------------------------------------------------------ */

void mvn_gfx_init_default_palette(mvn_graphics_t *gfx);

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
bool mvn_gfx_load_sheet(mvn_graphics_t *gfx,
                        const uint8_t * rgba,
                        int32_t         width,
                        int32_t         height,
                        int32_t         tile_w,
                        int32_t         tile_h);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_GRAPHICS_H */
