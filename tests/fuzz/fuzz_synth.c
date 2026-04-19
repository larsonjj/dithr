/**
 * \file            fuzz_synth.c
 * \brief           Fuzz harness for the chip-tune synthesizer
 *
 * Feed arbitrary bytes as a dtr_synth_sfx_t definition and verify
 * the renderer handles any parameter combination without crashing.
 */

#include "console_defs.h"
#include "synth.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* libFuzzer entry point */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    dtr_synth_sfx_t sfx;
    int16_t        *pcm;
    size_t          pcm_len = 0;

    /* Skip very large inputs */
    if (size > 256) {
        return 0;
    }

    memset(&sfx, 0, sizeof(sfx));

    /* Fill notes from fuzz data (each note is 4 bytes) */
    {
        size_t note_bytes = sizeof(sfx.notes);
        size_t copy_len   = size < note_bytes ? size : note_bytes;

        memcpy(&sfx.notes, data, copy_len);
    }

    /* Fill meta fields from remaining bytes */
    if (size > sizeof(sfx.notes)) {
        const uint8_t *meta = data + sizeof(sfx.notes);
        size_t         left = size - sizeof(sfx.notes);

        if (left >= 1) {
            sfx.speed = meta[0];
        }
        if (left >= 2) {
            sfx.loop_start = meta[1];
        }
        if (left >= 3) {
            sfx.loop_end = meta[2];
        }
    }

    pcm = dtr_synth_render(&sfx, &pcm_len);
    if (pcm != NULL) {
        DTR_FREE(pcm);
    }

    return 0;
}
