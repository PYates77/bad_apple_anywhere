#include "libbadapple.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

// ffmpeg includes
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

// ---- Comiple-Time Definitions ----
#ifndef BADAPPLE_FILENAME
#define BADAPPLE_FILENAME "badapple.mp4"
#endif

#ifndef BADAPPLE_DEFAULT_PIXEL_VALUE
#define BADAPPLE_DEFAULT_PIXEL_VALUE (0)
#endif

#ifndef BADAPPLE_SCALE_FLAGS
//#define BADAPPLE_SCALE_FLAGS SWS_POINT // nearest neighbor sampling
#define BADAPPLE_SCALE_FLAGS SWS_BILINEAR
#endif

#define BADAPPLE_SCALE_STRETCH 0
#define BADAPPLE_SCALE_CROP 1
#define BADAPPLE_SCALE_LETTERBOX 2

#ifndef BADAPPLE_SCALE_MODE
//#define BADAPPLE_SCALE_MODE BADAPPLE_SCALE_CROP
#define BADAPPLE_SCALE_MODE BADAPPLE_SCALE_STRETCH
//#define BADAPPLE_SCALE_MODE BADAPPLE_SCALE_LETTERBOX
#endif

typedef struct
{
    AVFormatContext *format_ctx;
    AVCodecContext *codec_ctx;
    AVStream *stream;

    struct SwsContext *sws_ctx;

    AVPacket *packet;
    AVFrame *decode_frame;
    AVFrame *gray_frame;

    uint32_t video_stream_index;

    uint64_t next_output_timestamp_us;
    uint64_t output_period_us;

    uint32_t output_frame_number;
    uint32_t scaled_width;
    uint32_t scaled_height;
    uint32_t crop_x;
    uint32_t crop_y;
    uint32_t offset_x;
    uint32_t offset_y;

} badapple_private_t;

static badapple_private_t *badapple_get_priv(badapple_ctx_t *ctx)
{
    return (badapple_private_t *)ctx->priv;
}
// ----- Private ----
static uint64_t badapple_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000ULL) + ((uint64_t)ts.tv_nsec / 1000ULL);
}

static void badapple_sleep_until(uint64_t target_us)
{
    uint64_t now_us;
    uint64_t delay_us;

    now_us = badapple_time_us();

    if (now_us >= target_us) {
        return;
    }

    delay_us = target_us - now_us;

    struct timespec ts = {
        .tv_sec = delay_us / 1000000ULL,
        .tv_nsec = (delay_us % 1000000ULL) * 1000ULL,
    };

    while (nanosleep(&ts, &ts) < 0) {
        if (errno != EINTR) {
            break;
        }
    }
}

#if (BADAPPLE_SCALE_MODE == BADAPPLE_SCALE_CROP)
static void badapple_crop_dimensions(int src_w, int src_h,
                                     int dst_w, int dst_h,
                                     int *scale_w, int *scale_h)
{
    if ((int64_t)src_w * dst_h > (int64_t)dst_w * src_h) {
        // source is wider than destination
        *scale_h = dst_h;
        *scale_w = src_w * dst_h / src_h;
    } else {
        // source is taller than destination
        *scale_w = dst_w;
        *scale_h = src_h * dst_w / src_w;
    }
}
#elif (BADAPPLE_SCALE_MODE == BADAPPLE_SCALE_LETTERBOX)
static void badapple_letterbox_dimensions(int src_w, int src_h,
                                          int dst_w, int dst_h,
                                          int *scale_w, int *scale_h)
{
    if ((int64_t)src_w * dst_h >
        (int64_t)dst_w * src_h) {
        // source is wider
        *scale_w = dst_w;
        *scale_h = src_h * dst_w / src_w;
    } else {
        // source is taller
        *scale_h = dst_h;
        *scale_w = src_w * dst_h / src_h;
    }
}
#endif /* BADAPPLE_SCALE_MODE */

// ----- Public -------

bool badapple_init(badapple_ctx_t *ctx)
{
    badapple_private_t *priv;
    AVCodecParameters *codec_params;
    const AVCodec *codec;

    priv = calloc(1, sizeof(*priv));

    if (priv == NULL) {
        return false;
    }

    ctx->priv = priv;

    // open video file
    if (avformat_open_input(&priv->format_ctx, BADAPPLE_FILENAME, NULL, NULL) < 0) {
        goto error;
    }

    if (avformat_find_stream_info(priv->format_ctx, NULL) < 0) {
        goto error;
    }

    // find video stream
    for (unsigned int i = 0; i < priv->format_ctx->nb_streams; i++) {
        if (priv->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            priv->video_stream_index = i;
            break;
        }
    }

    priv->stream = priv->format_ctx->streams[priv->video_stream_index];
    codec_params = priv->stream->codecpar;
    codec = avcodec_find_decoder(codec_params->codec_id);

    if (codec == NULL) {
        goto error;
    }

    priv->codec_ctx = avcodec_alloc_context3(codec);

    if (priv->codec_ctx == NULL) {
        goto error;
    }

    if (avcodec_parameters_to_context(priv->codec_ctx, codec_params) < 0) {
        goto error;
    }

    if (avcodec_open2(priv->codec_ctx, codec, NULL) < 0) {
        goto error;
    }

    // allocate decoder objects
    priv->packet = av_packet_alloc();
    priv->decode_frame = av_frame_alloc();
    priv->gray_frame = av_frame_alloc();

    if (priv->packet == NULL ||
        priv->decode_frame == NULL ||
        priv->gray_frame == NULL) {
        goto error;
    }

    // determine scaling if needed
    priv->scaled_width = ctx->width;
    priv->scaled_height = ctx->height;

    priv->offset_x = 0;
    priv->offset_y = 0;

    priv->crop_x = 0;
    priv->crop_y = 0;


#if (BADAPPLE_SCALE_MODE == BADAPPLE_SCALE_CROP)
    badapple_crop_dimensions(priv->codec_ctx->width,
                             priv->codec_ctx->height,
                             ctx->width,
                             ctx->height,
                             &priv->scaled_width,
                             &priv->scaled_height);

    priv->crop_x = (priv->scaled_width - ctx->width) / 2;
    priv->crop_y = (priv->scaled_height - ctx->height) / 2;
#elif (BADAPPLE_SCALE_MODE == BADAPPLE_SCALE_LETTERBOX)
    badapple_letterbox_dimensions(priv->codec_ctx->width,
                                  priv->codec_ctx->height,
                                  ctx->width,
                                  ctx->height,
                                  &priv->scaled_width,
                                  &priv->scaled_height);

    priv->offset_x = (ctx->width - priv->scaled_width) / 2;
    priv->offset_y = (ctx->height - priv->scaled_height) / 2;
#endif

    // allocate grayscale frame buffer
    if (av_image_alloc(priv->gray_frame->data,
                       priv->gray_frame->linesize,
                       priv->scaled_width,
                       priv->scaled_height,
                       AV_PIX_FMT_GRAY8,
                       1) < 0) {
        goto error;
    }

    // configure scaler 
    priv->sws_ctx = sws_getContext(priv->codec_ctx->width,
                                   priv->codec_ctx->height,
                                   priv->codec_ctx->pix_fmt,
                                   priv->scaled_width,
                                   priv->scaled_height,
                                   AV_PIX_FMT_GRAY8,
                                   BADAPPLE_SCALE_FLAGS,
                                   NULL,
                                   NULL,
                                   NULL);

    if (priv->sws_ctx == NULL) {
        goto error;
    }

    // Requested output FPS
    priv->output_period_us =
        1000000ULL / ctx->fps;

    return true;

error:
    badapple_destroy(ctx);

    return false;
}

bool badapple_next_frame(badapple_ctx_t *ctx, badapple_frame_info_t *frame)
{
    badapple_private_t *priv;
    int ret;

    priv = badapple_get_priv(ctx);

    if (priv == NULL) {
        return false;
    }

    while (av_read_frame(priv->format_ctx, priv->packet) >= 0) {
        // skip any streams in the video that we don't want
        if (priv->packet->stream_index != priv->video_stream_index) {
            av_packet_unref(priv->packet);
            continue;
        }

        ret = avcodec_send_packet(
            priv->codec_ctx,
            priv->packet);

        av_packet_unref(priv->packet);

        if (ret < 0) {
            return false;
        }

        while ((ret = avcodec_receive_frame(priv->codec_ctx, priv->decode_frame)) == 0) {
            // determine microsecond timestamp
            AVRational microsecond_timebase = {1, 1000000};
            uint64_t timestamp_us = av_rescale_q(priv->decode_frame->best_effort_timestamp,
                                                 priv->stream->time_base,
                                                 microsecond_timebase);

            // initialize output clock based on the first decoded frame timestamp
            if (priv->output_frame_number == 0) {
                priv->next_output_timestamp_us = timestamp_us;
            }

            // keep discarding frames until we reach the nexst desired timestamp
            // NOTE - this is faster than repeatedly calling seek() because each frame depends on the frame before it
            if (timestamp_us < priv->next_output_timestamp_us) {
                continue;
            }

            // convert source frame into requested output resolution and grayscale format
            sws_scale(priv->sws_ctx,
                      (const uint8_t * const *) priv->decode_frame->data,
                      priv->decode_frame->linesize,
                      0,
                      priv->codec_ctx->height,
                      priv->gray_frame->data,
                      priv->gray_frame->linesize);

            // clear destination for letterbox bars
            memset(ctx->bitmap, BADAPPLE_DEFAULT_PIXEL_VALUE, ctx->width * ctx->height);

            for (uint32_t y = 0; y < priv->scaled_height; y++) {
                size_t bitmap_index = (y + priv->offset_y) * ctx->width + priv->offset_x;
                size_t gray_index = (y + priv->crop_y) * priv->gray_frame->linesize[0] + priv->crop_x;
                memcpy(&ctx->bitmap[bitmap_index],
                       &priv->gray_frame->data[0][gray_index],
                       priv->scaled_width);
            }

            // update frame info metadata for caller
            frame->frame_number = priv->output_frame_number++;
            frame->timestamp_us = priv->next_output_timestamp_us;
            // TODO: width and height redundant? already part of ctx which user specified in the first place?
            frame->width = ctx->width;
            frame->height = ctx->height;
            frame->bitmap = ctx->bitmap;

            priv->next_output_timestamp_us += priv->output_period_us;

            return true;
        }

        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            return false;
        }
    }

    return false;
}

bool badapple_play(badapple_ctx_t *ctx)
{
    badapple_frame_info_t frame;
    uint64_t start_time_us;
    uint64_t first_frame_timestamp_us;

    start_time_us = badapple_time_us();
    first_frame_timestamp_us = 0;

    while (badapple_next_frame(ctx, &frame)) {
        if (frame.frame_number == 0) {
            first_frame_timestamp_us =
                frame.timestamp_us;
        }

        uint64_t presentation_time_us = start_time_us + 
                                        (frame.timestamp_us - first_frame_timestamp_us);

        badapple_sleep_until(presentation_time_us);

        if (ctx->cb != NULL) {
            if (!ctx->cb(ctx, &frame)) {
                return false;
            }
        }
    }

    return true;
}

void badapple_destroy(badapple_ctx_t *ctx)
{
    badapple_private_t *priv;

    if (ctx == NULL) {
        return;
    }

    priv = badapple_get_priv(ctx);

    if (priv == NULL) {
        return;
    }

    if (priv->gray_frame != NULL) {
        av_freep(&priv->gray_frame->data[0]);
    }

    av_frame_free(&priv->gray_frame);
    av_frame_free(&priv->decode_frame);
    av_packet_free(&priv->packet);
    sws_freeContext(priv->sws_ctx);
    avcodec_free_context(&priv->codec_ctx);
    avformat_close_input(&priv->format_ctx);

    free(priv);
    ctx->priv = NULL;
}
