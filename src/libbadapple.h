#ifndef _LIBBADAPPLE_H_
#define _LIBBADAPPLE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct badapple_ctx badapple_ctx_t;
typedef struct badapple_frame_info badapple_frame_info_t;

typedef bool (*badapple_frame_callback_t)(
    badapple_ctx_t *ctx,
    const badapple_frame_info_t *frame);

struct badapple_ctx
{
    /* Requested output format */
    uint32_t width;
    uint32_t height;
    uint32_t fps;

    /*
     * Caller-provided framebuffer.
     * One byte per pixel (8-bit grayscale).
     * Required size:
     *     width * height
     */
    uint8_t *bitmap;

    /* Optional callback used by badapple_play(). */
    badapple_frame_callback_t cb;

    /* Opaque pointer available to the application. */
    void *user_data;

    /* Private */
    void *priv;
};

struct badapple_frame_info
{
    uint32_t frame_number;

    /* Presentation timestamp. */
    uint64_t timestamp_us;

    uint32_t width;
    uint32_t height;

    /*
     * Points to ctx->bitmap.
     *
     * Valid until the next call to badapple_next_frame().
     */
    const uint8_t *bitmap;
};

bool badapple_init(badapple_ctx_t *ctx);

void badapple_destroy(badapple_ctx_t *ctx);

/*
 * Decode the next output frame.
 *
 * Returns false on end-of-file or error.
 */
bool badapple_next_frame(
    badapple_ctx_t *ctx,
    badapple_frame_info_t *frame);

/*
 * Decode and play the video using ctx->cb.
 *
 * Returns false if playback was interrupted by an error or
 * because the callback returned false.
 */
bool badapple_play(badapple_ctx_t *ctx);

#endif /* _LIBBADAPPLE_H_ */
