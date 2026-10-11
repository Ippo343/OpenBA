/*
 * Banished reimplementation, third_party/wmadec: stands in for FFmpeg's
 * libavcodec/codec_internal.h. FFCodec keeps the member names wmadec.c's
 * designated initialisers use (.p.*, .priv_data_size, .init, .close, .cb.decode,
 * .flush, .caps_internal); wmadec_glue.c drives the decoder through them.
 * Not an FFmpeg file; see EXTRACT.md.
 */
#ifndef AVCODEC_CODEC_INTERNAL_H
#define AVCODEC_CODEC_INTERNAL_H

#include "avcodec.h"

#define FF_CODEC_CAP_INIT_CLEANUP (1 << 1)

typedef struct FFCodec {
    AVCodec p;
    int caps_internal;
    int priv_data_size;
    int (*init)(AVCodecContext *);
    struct {
        int (*decode)(AVCodecContext *avctx, AVFrame *frame,
                      int *got_frame_ptr, AVPacket *avpkt);
    } cb;
    int (*close)(AVCodecContext *);
    void (*flush)(AVCodecContext *);
} FFCodec;

#define CODEC_LONG_NAME(str) .p.long_name = (str)
#define FF_CODEC_DECODE_CB(func) .cb.decode = (func)

#endif /* AVCODEC_CODEC_INTERNAL_H */
