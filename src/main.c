/**
 * \file            main.c
 * \brief           MVN Console entry point — SDL_MAIN_USE_CALLBACKS model
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include "console.h"
#include "audio.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* ------------------------------------------------------------------ */
/*  App state                                                          */
/* ------------------------------------------------------------------ */

typedef struct app_state {
    mvn_console_t *con;
    const char *   cart_path;
} app_state_t;

/* ------------------------------------------------------------------ */
/*  CLI options                                                        */
/* ------------------------------------------------------------------ */

typedef struct cli_opts {
    const char *cart_path;
    int32_t     scale;      /* 0 = use cart default */
    bool        fullscreen;
    bool        mute;
} cli_opts_t;

static void prv_print_usage(void)
{
    SDL_Log(
        "Usage: mvngin [options] [cart.json]\n"
        "\n"
        "Options:\n"
        "  --help        Show this help and exit\n"
        "  --version     Show version and exit\n"
        "  --fullscreen  Start in fullscreen mode\n"
        "  --scale N     Window scale factor (1-10)\n"
        "  --mute        Start with audio muted\n"
    );
}

/**
 * \brief           Parse CLI arguments into opts.
 * \return          true to continue, false to exit (--help / --version).
 */
static bool prv_parse_cli(int argc, char **argv, cli_opts_t *opts)
{
    opts->cart_path  = "cart.json";
    opts->scale      = 0;
    opts->fullscreen = false;
    opts->mute       = false;

    for (int i = 1; i < argc; ++i) {
        if (SDL_strcmp(argv[i], "--help") == 0 || SDL_strcmp(argv[i], "-h") == 0) {
            prv_print_usage();
            return false;
        }
        if (SDL_strcmp(argv[i], "--version") == 0 || SDL_strcmp(argv[i], "-v") == 0) {
            SDL_Log("mvngin %s", CONSOLE_VERSION);
            return false;
        }
        if (SDL_strcmp(argv[i], "--fullscreen") == 0 || SDL_strcmp(argv[i], "-f") == 0) {
            opts->fullscreen = true;
            continue;
        }
        if (SDL_strcmp(argv[i], "--mute") == 0 || SDL_strcmp(argv[i], "-m") == 0) {
            opts->mute = true;
            continue;
        }
        if ((SDL_strcmp(argv[i], "--scale") == 0 || SDL_strcmp(argv[i], "-s") == 0)
            && i + 1 < argc) {
            int val = SDL_atoi(argv[++i]);
            if (val >= 1 && val <= 10) {
                opts->scale = val;
            } else {
                SDL_Log("Warning: --scale value out of range (1-10), ignoring");
            }
            continue;
        }
        /* Positional: cart path (first non-flag argument) */
        if (argv[i][0] != '-') {
            opts->cart_path = argv[i];
            continue;
        }
        SDL_Log("Warning: unknown option '%s'", argv[i]);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  SDL_AppInit                                                        */
/* ------------------------------------------------------------------ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state_t *app;
    cli_opts_t   opts;

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);

    app = SDL_calloc(1, sizeof(app_state_t));
    if (app == NULL) {
        return SDL_APP_FAILURE;
    }

#ifdef MVN_WASM_CART_PATH
    (void)argc;
    (void)argv;
    app->cart_path = MVN_WASM_CART_PATH;
#else
    if (!prv_parse_cli(argc, argv, &opts)) {
        SDL_free(app);
        return SDL_APP_SUCCESS;
    }
    app->cart_path = opts.cart_path;
#endif
    SDL_Log("mvngin %s — loading %s", CONSOLE_VERSION, app->cart_path);

    app->con = mvn_console_create(app->cart_path);
    if (app->con == NULL) {
        SDL_Log("Failed to create console");
        SDL_free(app);
        return SDL_APP_FAILURE;
    }

#ifndef MVN_WASM_CART_PATH
    /* Apply CLI overrides after console creation */
    if (opts.scale > 0) {
        int32_t w = app->con->fb_width * opts.scale;
        int32_t h = app->con->fb_height * opts.scale;
        SDL_SetWindowSize(app->con->window, w, h);
        app->con->win_width  = w;
        app->con->win_height = h;
    }
    if (opts.fullscreen) {
        app->con->fullscreen = true;
        SDL_SetWindowFullscreen(app->con->window, true);
    }
    if (opts.mute && app->con->audio != NULL) {
        app->con->audio->master_volume = 0.0f;
    }
#endif

    *appstate = app;
    return SDL_APP_CONTINUE;
}

/* ------------------------------------------------------------------ */
/*  SDL_AppEvent                                                       */
/* ------------------------------------------------------------------ */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state_t *app = (app_state_t *)appstate;

    mvn_console_event(app->con, event);

    if (!app->con->running) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

/* ------------------------------------------------------------------ */
/*  SDL_AppIterate                                                     */
/* ------------------------------------------------------------------ */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state_t *app = (app_state_t *)appstate;

    mvn_console_iterate(app->con);

    /* Handle restart request from sys.restart() */
    if (app->con->restart) {
        mvn_console_destroy(app->con);
        app->con = mvn_console_create(app->cart_path);
        if (app->con == NULL) {
            SDL_Log("Failed to recreate console on restart");
            return SDL_APP_FAILURE;
        }
    }

    if (!app->con->running) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

/* ------------------------------------------------------------------ */
/*  SDL_AppQuit                                                        */
/* ------------------------------------------------------------------ */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    app_state_t *app = (app_state_t *)appstate;

    (void)result;

    if (app != NULL) {
        mvn_console_destroy(app->con);
        SDL_free(app);
    }
}
