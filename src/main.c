/**
 * \file            main.c
 * \brief           MVN Console entry point
 */

#include "console.h"

#include <stdio.h>

/**
 * \brief           Application entry point
 */
int main(int argc, char *argv[])
{
    const char *   cart_path;
    mvn_console_t *con;

    cart_path = (argc > 1) ? argv[1] : "cart.json";

    con = mvn_console_create(cart_path);
    if (con == NULL) {
        SDL_Log("Failed to create console");
        return 1;
    }

    mvn_console_run(con);
    mvn_console_destroy(con);

    return 0;
}
