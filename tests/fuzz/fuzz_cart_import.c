/**
 * \file            fuzz_cart_import.c
 * \brief           Fuzz harness for the cart import / JSON parsing path
 *
 * Feed arbitrary bytes as a cart.json file and verify the importer
 * handles malformed input gracefully (no crashes, no UB).
 */

#include "cart.h"
#include "console_defs.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

/* libFuzzer entry point */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char     tmp_path[256];
    FILE    *fp;
    dtr_cart_t *cart;

    /* Skip very large inputs to keep runs fast */
    if (size > 65536) {
        return 0;
    }

    /* Write fuzz data to a temp file (cart loader reads from disk) */
    SDL_snprintf(tmp_path, sizeof(tmp_path), "/tmp/fuzz_cart_%d.json", getpid());
    fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        return 0;
    }
    fwrite(data, 1, size, fp);
    fclose(fp);

    /* Attempt to load — should either succeed or fail gracefully */
    cart = dtr_cart_load(tmp_path);
    if (cart != NULL) {
        dtr_cart_destroy(cart);
    }

    remove(tmp_path);
    return 0;
}
