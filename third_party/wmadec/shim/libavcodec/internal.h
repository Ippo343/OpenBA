/*
 * Banished reimplementation, third_party/wmadec: stands in for FFmpeg's
 * libavcodec/internal.h. wmadec.c needs only AVCodecInternal (declared in the shim
 * avcodec.h) from it. Not an FFmpeg file; see EXTRACT.md.
 */
#ifndef AVCODEC_INTERNAL_H
#define AVCODEC_INTERNAL_H

#include "avcodec.h"
#include "libavutil/internal.h"

#endif /* AVCODEC_INTERNAL_H */
