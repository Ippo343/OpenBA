/*
 * third_party/wmadec — the C API over the vendored FFmpeg WMA v2 decoder.
 *
 * Banished reimplementation glue (not an FFmpeg file; LGPL-2.1-or-later like the
 * library it fronts, see EXTRACT.md). The only header the game's tree includes:
 * no FFmpeg type crosses it.
 *
 * One decoder decodes one WMA v2 bitstream: feed it the packets (each exactly
 * `block_align` bytes) in order; every call returns the PCM the packet completed,
 * as planar float in [-1, 1]. The decoder carries state between packets (the bit
 * reservoir and the MDCT overlap), so packets must arrive in stream order;
 * wmav2_reset() drops the reservoir before a discontinuity, as FFmpeg's flush.
 *
 * Thread-safety: a decoder is used by one thread at a time; separate decoders
 * may run concurrently (the shared tables are built under a real once-init).
 */
#ifndef WMAV2_DECODER_H
#define WMAV2_DECODER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Wmav2Decoder Wmav2Decoder;

typedef struct Wmav2Params {
    int            channels;        /* 1 or 2 */
    int            sample_rate;     /* Hz, <= 50000 */
    int            bit_rate;        /* bits per second (WAVEFORMATEX nAvgBytesPerSec * 8) */
    int            block_align;     /* bytes per packet (WAVEFORMATEX nBlockAlign) */
    const uint8_t *extradata;       /* the WMA v2 codec header; xWMA carries none, see wmav2_xwma_extradata */
    int            extradata_size;
} Wmav2Params;

/* The 6-byte codec header FFmpeg's xWMA demuxer (libavformat/xwma.c) synthesises
 * for WAVE_FORMAT_WMAUDIO2 streams, whose RIFF carries none: encoder flags 0x1f
 * (exponent VLC, bit reservoir, variable block length) at offset 4. */
void wmav2_xwma_extradata(uint8_t out[6]);

/* Returns 0 and a decoder in *out, or a negative error (*out untouched). */
int  wmav2_open(const Wmav2Params *params, Wmav2Decoder **out);

/* Decode one packet (`size` >= block_align; only block_align bytes are read).
 * On success returns 0 and sets *planes to `channels` float planes of *samples
 * frames each (owned by the decoder, valid until the next call); *samples may
 * be 0 when the packet only fed the bit reservoir. Negative on a bitstream
 * error: the reservoir is then reset, as FFmpeg does. */
int  wmav2_decode_packet(Wmav2Decoder *dec, const uint8_t *packet, int size,
                         const float *const **planes, int *samples);

/* End of stream: returns the last frame_len frames still held in the MDCT
 * overlap (FFmpeg's EOF flush, an empty packet). Once per stream; a second call
 * returns 0 frames. Same *planes / *samples contract as wmav2_decode_packet. */
int  wmav2_drain(Wmav2Decoder *dec, const float *const **planes, int *samples);

/* Frames per WMA frame (the decoder's granule: 2048 at 44.1 kHz). */
int  wmav2_frame_len(const Wmav2Decoder *dec);

/* Drop the bit reservoir (FFmpeg's flush): call before feeding a packet that
 * does not follow the previous one. */
void wmav2_reset(Wmav2Decoder *dec);

void wmav2_close(Wmav2Decoder *dec);

#ifdef __cplusplus
}
#endif

#endif /* WMAV2_DECODER_H */
