/*
 * Banished reimplementation, third_party/wmadec: stands in for FFmpeg's
 * libavcodec/decode.h. ff_get_buffer sizes the frame's float planes for
 * frame->nb_samples; wmadec_glue.c implements it over a per-decoder buffer.
 * Not an FFmpeg file; see EXTRACT.md.
 */
#ifndef AVCODEC_DECODE_H
#define AVCODEC_DECODE_H

#include "avcodec.h"

int ff_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags);

#endif /* AVCODEC_DECODE_H */
