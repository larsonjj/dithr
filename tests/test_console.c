/**
 * \file            test_console.c
 * \brief           Unit tests for the console lifecycle
 */

#include "console.h"
#include "input.h"
#include "mouse.h"
#include "test_harness.h"

#include <SDL3/SDL.h>

/* ------------------------------------------------------------------ */
/*  NULL safety                                                        */
/* ------------------------------------------------------------------ */

static void test_console_destroy_null(void)
{
    dtr_console_destroy(NULL); /* must not crash */
    DTR_PASS();
}

static void test_console_reload_null(void)
{
    DTR_ASSERT(!dtr_console_reload(NULL));
    DTR_PASS();
}

static void test_console_reload_assets_null(void)
{
    DTR_ASSERT(!dtr_console_reload_assets(NULL));
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Lifecycle: create with nonexistent cart (uses defaults)             */
/* ------------------------------------------------------------------ */

static void test_console_create_no_cart(void)
{
    dtr_console_t *con;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    /* Verify subsystem pointers were populated */
    DTR_ASSERT(con->runtime != NULL);
    DTR_ASSERT(con->graphics != NULL);
    DTR_ASSERT(con->keys != NULL);
    DTR_ASSERT(con->mouse != NULL);
    DTR_ASSERT(con->gamepads != NULL);
    DTR_ASSERT(con->input != NULL);
    DTR_ASSERT(con->events != NULL);
    DTR_ASSERT(con->cart != NULL);
    DTR_ASSERT(con->postfx != NULL);

    /* Verify default framebuffer dimensions */
    DTR_ASSERT(con->fb_width > 0);
    DTR_ASSERT(con->fb_height > 0);

    /* Window and renderer should exist */
    DTR_ASSERT(con->window != NULL);
    DTR_ASSERT(con->renderer != NULL);
    DTR_ASSERT(con->screen_tex != NULL);

    /* State flags */
    DTR_ASSERT(!con->paused);
    DTR_ASSERT(!con->fullscreen);

#if DEV_BUILD
    DTR_ASSERT(con->watch_asset_mtime >= 0);
    DTR_ASSERT_EQ_INT(con->reload_pending_kind, DTR_RELOAD_PENDING_NONE);
#endif

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  iterate: single frame must not crash                               */
/* ------------------------------------------------------------------ */

static void test_console_iterate_once(void)
{
    dtr_console_t *con;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    con->running = true;
    dtr_console_iterate(con);

    /* Frame count should have advanced */
    DTR_ASSERT(con->frame_count >= 1);

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  event: quit event sets running to false                            */
/* ------------------------------------------------------------------ */

static void test_console_event_quit(void)
{
    dtr_console_t *con;
    SDL_Event      ev;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    con->running = true;

    SDL_memset(&ev, 0, sizeof(ev));
    ev.type = SDL_EVENT_QUIT;
    dtr_console_event(con, &ev);

    DTR_ASSERT(!con->running);

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  reload_assets: safe on default console (no assets to load)         */
/* ------------------------------------------------------------------ */

static void test_console_reload_assets_empty(void)
{
    dtr_console_t *con;
    bool           ok;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    ok = dtr_console_reload_assets(con);
    DTR_ASSERT(ok);

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  event: key down sets key state                                     */
/* ------------------------------------------------------------------ */

static void test_console_event_key_down(void)
{
    dtr_console_t *con;
    SDL_Event      ev;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    SDL_memset(&ev, 0, sizeof(ev));
    ev.type         = SDL_EVENT_KEY_DOWN;
    ev.key.scancode = SDL_SCANCODE_Z;
    ev.key.repeat   = false;
    dtr_console_event(con, &ev);

    /* The key should now be pressed */
    DTR_ASSERT(dtr_key_btn(con->keys, DTR_KEY_Z));

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  event: key up clears key state                                     */
/* ------------------------------------------------------------------ */

static void test_console_event_key_up(void)
{
    dtr_console_t *con;
    SDL_Event      ev;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    /* Press */
    SDL_memset(&ev, 0, sizeof(ev));
    ev.type         = SDL_EVENT_KEY_DOWN;
    ev.key.scancode = SDL_SCANCODE_Z;
    ev.key.repeat   = false;
    dtr_console_event(con, &ev);
    DTR_ASSERT(dtr_key_btn(con->keys, DTR_KEY_Z));

    /* Release */
    SDL_memset(&ev, 0, sizeof(ev));
    ev.type         = SDL_EVENT_KEY_UP;
    ev.key.scancode = SDL_SCANCODE_Z;
    dtr_console_event(con, &ev);
    DTR_ASSERT(!dtr_key_btn(con->keys, DTR_KEY_Z));

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  event: escape toggles pause                                        */
/* ------------------------------------------------------------------ */

static void test_console_event_escape_pause(void)
{
    dtr_console_t *con;
    SDL_Event      ev;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    DTR_ASSERT(!con->paused);

    SDL_memset(&ev, 0, sizeof(ev));
    ev.type         = SDL_EVENT_KEY_DOWN;
    ev.key.scancode = SDL_SCANCODE_ESCAPE;
    ev.key.repeat   = false;
    dtr_console_event(con, &ev);

    DTR_ASSERT(con->paused);

    /* Pressing escape again unpauses */
    dtr_console_event(con, &ev);
    DTR_ASSERT(!con->paused);

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  event: mouse button events                                         */
/* ------------------------------------------------------------------ */

static void test_console_event_mouse_button(void)
{
    dtr_console_t *con;
    SDL_Event      ev;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    /* Press left mouse button */
    SDL_memset(&ev, 0, sizeof(ev));
    ev.type          = SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.x      = 10.0f;
    ev.button.y      = 20.0f;
    dtr_console_event(con, &ev);

    DTR_ASSERT(dtr_mouse_btn(con->mouse, DTR_MOUSE_LEFT));

    /* Release left mouse button */
    SDL_memset(&ev, 0, sizeof(ev));
    ev.type          = SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.button = SDL_BUTTON_LEFT;
    dtr_console_event(con, &ev);

    DTR_ASSERT(!dtr_mouse_btn(con->mouse, DTR_MOUSE_LEFT));

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  iterate: multiple frames advance frame count                       */
/* ------------------------------------------------------------------ */

static void test_console_iterate_multi_frame(void)
{
    dtr_console_t *con;

    con = dtr_console_create("__nonexistent_cart__.json");
    DTR_ASSERT(con != NULL);

    con->running = true;
    dtr_console_iterate(con);
    dtr_console_iterate(con);
    dtr_console_iterate(con);

    DTR_ASSERT(con->frame_count >= 3);

    dtr_console_destroy(con);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    DTR_TEST_BEGIN("console");

    /* NULL safety — no SDL init required */
    DTR_RUN_TEST(test_console_destroy_null);
    DTR_RUN_TEST(test_console_reload_null);
    DTR_RUN_TEST(test_console_reload_assets_null);

    /* Full lifecycle tests — need SDL with dummy drivers */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "  SKIP SDL_Init failed (no dummy drivers?): %s\n", SDL_GetError());
        DTR_TEST_END();
    }

    DTR_RUN_TEST(test_console_create_no_cart);
    DTR_RUN_TEST(test_console_iterate_once);
    DTR_RUN_TEST(test_console_event_quit);
    DTR_RUN_TEST(test_console_reload_assets_empty);
    DTR_RUN_TEST(test_console_event_key_down);
    DTR_RUN_TEST(test_console_event_key_up);
    DTR_RUN_TEST(test_console_event_escape_pause);
    DTR_RUN_TEST(test_console_event_mouse_button);
    DTR_RUN_TEST(test_console_iterate_multi_frame);

    SDL_Quit();
    DTR_TEST_END();
}
