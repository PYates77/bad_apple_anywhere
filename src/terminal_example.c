#include "libbadapple.h"

#include <stdio.h>
#include <stdlib.h>

static bool display_frame(badapple_ctx_t *ctx, const badapple_frame_info_t *frame)
{
    printf("\033[H"); // jump to top left of terminal window

    for (uint32_t y = 0; y < frame->height; y++) {
        for (uint32_t x = 0; x < frame->width; x++) {
            uint8_t pixel =
                frame->bitmap[y * frame->width + x];

            // just draw a light or dark square for now
            if (pixel > 128) {
                putchar('#');
            } else {
                putchar(' ');
            }
        }

        putchar('\n');
    }

    fflush(stdout);

    return true;
}


int main(void)
{
    badapple_ctx_t ctx = {
        .width = 180,
        .height = 60,
        .fps = 30,
        .cb = display_frame,
    };

    ctx.bitmap = calloc(ctx.width * ctx.height, sizeof(*ctx.bitmap));
    if (ctx.bitmap == NULL) {
        return 1;
    }

    if (!badapple_init(&ctx)) {
        free(ctx.bitmap);
        return 1;
    }

    printf("\033[2J"); // clear scren

    // this takes care of the timing and everything
    badapple_play(&ctx);

    badapple_destroy(&ctx);
    free(ctx.bitmap);
    return 0;
}
