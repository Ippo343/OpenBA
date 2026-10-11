/*
 * Banished reimplementation, third_party/wmadec: stands in for FFmpeg's
 * libavcodec/avcodec.h (and the codec.h / codec_id.h / packet.h / frame.h /
 * samplefmt.h / channel_layout.h / avutil.h it pulls in). It declares ONLY the
 * types, fields and constants the vendored wmadec.c / wma.c touch; the field NAMES
 * match FFmpeg's so the upstream sources compile unmodified, the layouts do not
 * (nothing outside this library ever sees these structs). Not an FFmpeg file; see
 * EXTRACT.md.
 */
#ifndef AVCODEC_AVCODEC_H
#define AVCODEC_AVCODEC_H

#include <stdint.h>

#include "libavutil/attributes.h"
#include "libavutil/error.h"
#include "libavutil/log.h"
#include "libavutil/mem.h"
#include "libavcodec/defs.h"           /* AV_INPUT_BUFFER_PADDING_SIZE */

#define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))

enum AVMediaType { AVMEDIA_TYPE_UNKNOWN = -1, AVMEDIA_TYPE_AUDIO = 1 };

enum AVCodecID {
    AV_CODEC_ID_NONE  = 0,
    AV_CODEC_ID_WMAV1 = 0x15007,       /* FFmpeg's values, for reference only */
    AV_CODEC_ID_WMAV2 = 0x15008,
};

enum AVSampleFormat { AV_SAMPLE_FMT_NONE = -1, AV_SAMPLE_FMT_FLTP = 8 };

#define AV_CODEC_CAP_DR1          (1 << 1)
#define AV_CODEC_CAP_DELAY        (1 << 5)
#define AV_CODEC_FLAG_BITEXACT    (1 << 23)

typedef struct AVCodec {
    const char      *name;
    const char      *long_name;
    enum AVMediaType type;
    enum AVCodecID   id;
    int              capabilities;
} AVCodec;

typedef struct AVChannelLayout {
    int nb_channels;
} AVChannelLayout;

typedef struct AVCodecInternal {
    int skip_samples;
} AVCodecInternal;

typedef struct AVCodecContext {
    const AVCodec          *codec;
    void                   *priv_data;
    AVCodecInternal        *internal;
    int64_t                 bit_rate;
    int                     flags;
    uint8_t                *extradata;
    int                     extradata_size;
    int                     sample_rate;
    AVChannelLayout         ch_layout;
    enum AVSampleFormat     sample_fmt;
    int                     block_align;
    int                     delay;
} AVCodecContext;

typedef struct AVPacket {
    const uint8_t *data;
    int            size;
} AVPacket;

typedef struct AVFrame {
    uint8_t **extended_data;           /* float planes (AV_SAMPLE_FMT_FLTP), one per channel */
    int       nb_samples;
    int64_t   pts;
} AVFrame;

#endif /* AVCODEC_AVCODEC_H */
