// ============================================================================
// src/engine/audio/audio_pcm.cpp -- the decode-at-load arithmetic (Phase D.4 SDL-4)
// ============================================================================
// audio_pcm.h has the rules and where they were measured. Pure: no OS header, no
// engine type; tests/unit/audio_pcm_test.cpp pins both functions.
// ============================================================================
#include "audio/audio_pcm.h"

#include "platform_audio.h"   // Platform::AudioWaveFormat, kAudioFormatPcm

namespace Audio {

void RewriteFormatAsPcm(Platform::AudioWaveFormat& format) {
    const uint16_t blockAlign = static_cast<uint16_t>(2u * format.channels);
    format.formatTag      = Platform::kAudioFormatPcm;
    format.blockAlign     = blockAlign;
    format.avgBytesPerSec = format.samplesPerSec * blockAlign;
    format.bitsPerSample  = 16;
    format.extraSize      = 0;
}

uint32_t XwmaPcmPlayBegin(uint32_t playBegin, const uint32_t* seekTable, uint32_t packetCount,
                          uint32_t channels, uint32_t frameLength, uint32_t pcmSamples) {
    // k: the last packet whose start (SeekSamples(k) = table[k - 1] / (2 * channels),
    // 0 for packet 0) is at or before PlayBegin. The table ascends.
    uint32_t k = 0;
    if (channels != 0) {
        const uint64_t bytesPerSample = 2ull * channels;
        for (uint32_t i = 1; i < packetCount; ++i) {
            if (seekTable[i - 1] / bytesPerSample > playBegin) break;
            k = i;
        }
    }
    uint64_t start = playBegin;
    if (k > 0) start += frameLength;
    return start > pcmSamples ? pcmSamples : static_cast<uint32_t>(start);
}

}  // namespace Audio
