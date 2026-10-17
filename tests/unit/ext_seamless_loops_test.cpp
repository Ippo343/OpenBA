// ============================================================================
// tests/unit/ext_seamless_loops_test.cpp -- ext.seamless_loops's transform (block SEAMLESS)
// ============================================================================
// src/ext/seamless_loops/seamless_loops.{h,cpp} on synthetic PCM, and the decode's gate
// (Audio::LoopSeamApplies, src/engine/audio/audio_pcm.h): a seam with a codec-style silent tail,
// a steady tone, a buffer too short to fold, the trim bound, mono at 48 kHz, an all-silent buffer,
// and a one-shot buffer the decode never hands over.
// ============================================================================
#include "unit.h"

#include "audio/audio_pcm.h"
#include "ext/seamless_loops/seamless_loops.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

using Ext::SeamlessLoops::Transform;
using Ext::SeamlessLoops::Result;

// Deterministic noise in [-amp, amp] (an LCG; the ambiences are noise-like).
std::vector<int16_t> Noise(uint32_t frames, uint32_t channels, int amp, uint32_t seed = 12345u) {
    std::vector<int16_t> v(static_cast<size_t>(frames) * channels);
    uint32_t s = seed;
    for (auto& x : v) {
        s = s * 1664525u + 1013904223u;
        x = static_cast<int16_t>(static_cast<int>((s >> 16) % static_cast<uint32_t>(2 * amp + 1)) - amp);
    }
    return v;
}

double Rms(const std::vector<int16_t>& v, uint32_t channels, uint32_t lo, uint32_t hi) {
    double s = 0.0;
    for (size_t i = static_cast<size_t>(lo) * channels; i < static_cast<size_t>(hi) * channels; ++i)
        s += static_cast<double>(v[i]) * v[i];
    return std::sqrt(s / (static_cast<double>(hi - lo) * channels));
}

}  // namespace

UNIT_TEST(ext_seamless_loops_trims_a_silent_tail_and_folds_the_seam) {
    const uint32_t rate = 44100, ch = 2, n = 44100 * 4;
    std::vector<int16_t> pcm = Noise(n, ch, 2000);
    const uint32_t tail = 1000;                      // the codec tail: |x| <= 2
    for (uint32_t f = n - tail; f < n; ++f)
        for (uint32_t c = 0; c < ch; ++c) pcm[f * ch + c] = static_cast<int16_t>(pcm[f * ch + c] % 3);
    const double body = Rms(pcm, ch, 0, n - tail);
    const Result r = Transform(pcm.data(), n, ch, rate);
    CHECK(r.applied);
    CHECK_EQ(r.crossfadeFrames, 2205u);              // 50 ms at 44.1 kHz
    // 64-frame windows: 15 windows (960 frames) are wholly silent, the 16th holds 24 noisy frames.
    CHECK_EQ(r.framesTrimmed, 960u);
    CHECK_EQ(r.newFrames, n - 960u - 2205u);
    // Across the wrap (the last and the first 10 ms of the new buffer) the level is the body's.
    const double endLevel = Rms(pcm, ch, r.newFrames - 441, r.newFrames);
    const double startLevel = Rms(pcm, ch, 0, 441);
    CHECK(endLevel > 0.8 * body && endLevel < 1.2 * body);
    CHECK(startLevel > 0.8 * body && startLevel < 1.2 * body);
    // Equal power: the middle of the fold keeps the level (a linear fade would sit near 0.71x).
    const double mid = Rms(pcm, ch, 1102 - 220, 1102 + 220);
    CHECK(mid > 0.85 * body && mid < 1.15 * body);
}

UNIT_TEST(ext_seamless_loops_steady_tone_trims_nothing) {
    const uint32_t rate = 44100, ch = 2, n = 44100 * 2;
    std::vector<int16_t> pcm(static_cast<size_t>(n) * ch);
    for (uint32_t f = 0; f < n; ++f)
        for (uint32_t c = 0; c < ch; ++c)
            pcm[f * ch + c] = static_cast<int16_t>(std::lround(8000.0 * std::sin(2.0 * 3.14159265358979 * 440.0 * f / rate)));
    const Result r = Transform(pcm.data(), n, ch, rate);
    CHECK(r.applied);
    CHECK_EQ(r.framesTrimmed, 0u);
    CHECK_EQ(r.newFrames, n - 2205u);
    for (size_t i = 0; i < pcm.size(); ++i) CHECK(pcm[i] >= -11314 && pcm[i] <= 11314);   // <= sqrt(2) x 8000
}

UNIT_TEST(ext_seamless_loops_too_short_is_untouched) {
    const uint32_t rate = 44100, ch = 2;
    const uint32_t n = 2 * 2205 - 1;                 // under 2 x the crossfade
    std::vector<int16_t> pcm = Noise(n, ch, 1000);
    const std::vector<int16_t> before = pcm;
    const Result r = Transform(pcm.data(), n, ch, rate);
    CHECK(!r.applied);
    CHECK_EQ(r.newFrames, n);
    CHECK(pcm == before);
    // ... and a buffer whose trimmed length falls under 2 x the crossfade is untouched too.
    const uint32_t n2 = 2 * 2205 + 500;
    std::vector<int16_t> pcm2 = Noise(n2, ch, 1000);
    for (uint32_t f = n2 - 1000; f < n2; ++f) pcm2[f * ch] = pcm2[f * ch + 1] = 0;
    const std::vector<int16_t> before2 = pcm2;
    const Result r2 = Transform(pcm2.data(), n2, ch, rate);
    CHECK(!r2.applied);
    CHECK(pcm2 == before2);
}

UNIT_TEST(ext_seamless_loops_trim_is_bounded_to_one_frame) {
    const uint32_t rate = 44100, ch = 2, n = 44100 * 3;
    std::vector<int16_t> pcm = Noise(n, ch, 2000);
    for (uint32_t f = n - 3000; f < n; ++f) pcm[f * ch] = pcm[f * ch + 1] = 0;   // 3000 silent frames
    const Result r = Transform(pcm.data(), n, ch, rate);
    CHECK(r.applied);
    CHECK_EQ(r.framesTrimmed, Ext::SeamlessLoops::kMaxTrimFrames);
}

UNIT_TEST(ext_seamless_loops_mono_48k) {
    const uint32_t rate = 48000, ch = 1, n = 48000 * 2;
    std::vector<int16_t> pcm = Noise(n, ch, 3000, 777u);
    for (uint32_t f = n - 640; f < n; ++f) pcm[f] = 0;   // ten silent windows
    const Result r = Transform(pcm.data(), n, ch, rate);
    CHECK(r.applied);
    CHECK_EQ(r.crossfadeFrames, 2400u);              // 50 ms at 48 kHz
    CHECK_EQ(r.framesTrimmed, 640u);
    CHECK_EQ(r.newFrames, n - 640u - 2400u);
    CHECK(!Transform(pcm.data(), r.newFrames, 3, rate).applied);   // channels outside 1..2: untouched
    CHECK(!Transform(pcm.data(), r.newFrames, 1, 0).applied);      // no rate: untouched
}

UNIT_TEST(ext_seamless_loops_all_silent_trims_nothing) {
    const uint32_t rate = 44100, ch = 2, n = 44100;
    std::vector<int16_t> pcm(static_cast<size_t>(n) * ch, 0);
    const Result r = Transform(pcm.data(), n, ch, rate);
    CHECK_EQ(r.framesTrimmed, 0u);
    for (int16_t x : pcm) CHECK_EQ(x, static_cast<int16_t>(0));
}

UNIT_TEST(ext_seamless_loops_gate_one_shots_untouched) {
    int16_t pcm[4] = {1, 2, 3, 4};
    CHECK(Audio::LoopSeamApplies(true, true, pcm, 2));
    CHECK(!Audio::LoopSeamApplies(true, false, pcm, 2));     // a one-shot buffer: never handed over
    CHECK(!Audio::LoopSeamApplies(false, true, pcm, 2));     // the row off: the seam is null
    CHECK(!Audio::LoopSeamApplies(true, true, nullptr, 2));  // the decode refused the format
    CHECK(!Audio::LoopSeamApplies(true, true, pcm, 0));      // nothing decoded
}
