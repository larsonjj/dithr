/**
 * \file            main.c
 * \brief           MVN Console entry point — SDL_MAIN_USE_CALLBACKS model
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include "console.h"

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
/*  SDL_AppInit                                                        */
/* ------------------------------------------------------------------ */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    app_state_t *app;

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
    app->cart_path = (argc > 1) ? argv[1] : "cart.json";
#endif
    SDL_Log("mvngin %s — loading %s", CONSOLE_VERSION, app->cart_path);

    app->con = mvn_console_create(app->cart_path);
    if (app->con == NULL) {
        SDL_Log("Failed to create console");
        SDL_free(app);
        return SDL_APP_FAILURE;
    }

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
