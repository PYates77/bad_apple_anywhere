#include "libbadapple.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ascii_gradient = " .'`^,:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

static bool display_frame(const badapple_frame_info_t *frame)
{
    printf("\033[H"); // jump to top left of terminal window

    for (uint32_t y = 0; y < frame->height; y++) {
        for (uint32_t x = 0; x < frame->width; x++) {
            uint8_t pixel = frame->bitmap[y * frame->width + x];

            // choose a character depding on pixel darkness
            uint32_t index = (pixel * (strlen(ascii_gradient) - 1)) / 255;
            putchar(ascii_gradient[index]);
        }

        putchar('\n');
    }

    fflush(stdout);

    return true;
}


int main(void)
{
    // set up using our display settings
    badapple_ctx_t ctx = {
        .width = 180,
        .height = 60,
        .fps = 30,
        .cb = display_frame,
        .bitmap = NULL,
    };

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
