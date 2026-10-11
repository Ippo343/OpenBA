// ============================================================================
// tests/unit/audio_pcm_test.cpp -- the decode-at-load arithmetic (Phase D.4 SDL-4)
// ============================================================================
// src/engine/audio/audio_pcm.{h,cpp}: the PCM WAVEFORMATEX a decoded SoundBuffer
// carries, and the PCM sample a play starts at for retail's PlayBegin -- the rule
// MEASURED against XAudio2 2.7 in handins/XWMA.md A4.3 (k == 0: PlayBegin itself;
// k > 0: PlayBegin + FrameLength; k = the packet whose table range holds it).
// ============================================================================
#include "unit.h"

#include <cstdint>

#include "audio/audio_pcm.h"
#include "platform_audio.h"

UNIT_TEST(audio_pcm_rewrite_format_mono_and_stereo) {
    // The shipped mono class: Sheep1.wav's xWMA header (44.1 kHz, 48 kbit/s,
    // 2230-byte packets).
    Platform::AudioWaveFormat f{Platform::kAudioFormatXwma, 1, 44100, 6000, 2230, 16, 0};
    Audio::RewriteFormatAsPcm(f);
    CHECK_EQ(f.formatTag, Platform::kAudioFormatPcm);
    CHECK_EQ(f.channels, 1);
    CHECK_EQ(f.samplesPerSec, 44100u);
    CHECK_EQ(f.blockAlign, 2);
    CHECK_EQ(f.avgBytesPerSec, 88200u);
    CHECK_EQ(f.bitsPerSample, 16);
    CHECK_EQ(f.extraSize, 0);

    Platform::AudioWaveFormat g{Platform::kAudioFormatXwma, 2, 48000, 4000, 1008, 16, 0};
    Audio::RewriteFormatAsPcm(g);
    CHECK_EQ(g.blockAlign, 4);
    CHECK_EQ(g.avgBytesPerSec, 192000u);
    CHECK_EQ(g.samplesPerSec, 48000u);
}

UNIT_TEST(audio_pcm_play_begin_k0_is_play_begin) {
    // Mono, six packets of 16384 samples (32768 decoded bytes each), as Sheep1:
    // PlayBegin 6690 lies in packet 0, so XAudio2 started at sample 6690 exactly.
    const uint32_t table[6] = {32768, 65536, 98304, 131072, 163840, 196608};
    CHECK_EQ(Audio::XwmaPcmPlayBegin(6690, table, 6, 1, 2048, 98304), 6690u);
    CHECK_EQ(Audio::XwmaPcmPlayBegin(0, table, 6, 1, 2048, 98304), 0u);
    CHECK_EQ(Audio::XwmaPcmPlayBegin(16383, table, 6, 1, 2048, 98304), 16383u);
}

UNIT_TEST(audio_pcm_play_begin_k_positive_adds_a_frame) {
    // Stereo (4 bytes per sample), packets of 16384 samples: PlayBegin 22300 lies in
    // packet 1 (16384 <= 22300 < 32768), as Crowd's measured case, so the start is
    // 22300 + 2048.
    const uint32_t table[4] = {65536, 131072, 196608, 262144};
    CHECK_EQ(Audio::XwmaPcmPlayBegin(22300, table, 4, 2, 2048, 65536), 24348u);
    // The boundary itself belongs to the next packet: SeekSamples(1) == 16384.
    CHECK_EQ(Audio::XwmaPcmPlayBegin(16384, table, 4, 2, 2048, 65536), 18432u);
    // Packet 3 (the last): 49152 <= 50000.
    CHECK_EQ(Audio::XwmaPcmPlayBegin(50000, table, 4, 2, 2048, 65536), 52048u);
}

UNIT_TEST(audio_pcm_play_begin_clamps_to_the_pcm) {
    // A shorter PCM than the table (Tornado.wav: invalid last packets) or a PlayBegin
    // past the end: never past the decoded length.
    const uint32_t table[4] = {65536, 131072, 196608, 262144};
    CHECK_EQ(Audio::XwmaPcmPlayBegin(50000, table, 4, 2, 2048, 51000), 51000u);
    CHECK_EQ(Audio::XwmaPcmPlayBegin(90000, table, 4, 2, 2048, 65536), 65536u);
    CHECK_EQ(Audio::XwmaPcmPlayBegin(100, table, 4, 2, 2048, 0), 0u);
    // No table, no channels: PlayBegin as given, clamped.
    CHECK_EQ(Audio::XwmaPcmPlayBegin(100, nullptr, 0, 2, 2048, 1000), 100u);
    CHECK_EQ(Audio::XwmaPcmPlayBegin(100, table, 4, 0, 2048, 1000), 100u);
}
