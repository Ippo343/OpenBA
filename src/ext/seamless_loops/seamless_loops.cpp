// src/ext/seamless_loops/seamless_loops.cpp -- ext.seamless_loops (block SEAMLESS)
//
// NOT RETAIL. seamless_loops.h has the contract and the reasons for the constants.

#include "ext/seamless_loops/seamless_loops.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace Ext {
namespace SeamlessLoops {

namespace {

// Mean square of frames [lo, hi) over every channel.
double MeanSquare(const int16_t* pcm, uint32_t channels, uint32_t lo, uint32_t hi) {
    if (hi <= lo) return 0.0;
    double s = 0.0;
    for (uint64_t i = static_cast<uint64_t>(lo) * channels; i < static_cast<uint64_t>(hi) * channels; ++i) {
        const double v = pcm[i];
        s += v * v;
    }
    return s / (static_cast<double>(hi - lo) * channels);
}

// The quiet level: the kQuietPercentile-th percentile of the 10 ms window RMS over the body, the body
// being everything before the last kMaxTrimFrames + one window (where a tail can be).
float QuietLevel(const int16_t* pcm, uint32_t frames, uint32_t channels, uint32_t rate) {
    const uint32_t w = std::max<uint32_t>(1, rate / 100);
    const uint32_t bodyEnd = frames > kMaxTrimFrames + w ? frames - kMaxTrimFrames - w : 0;
    std::vector<float> rms;
    rms.reserve(bodyEnd / w + 1);
    for (uint32_t lo = 0; lo + w <= bodyEnd; lo += w)
        rms.push_back(static_cast<float>(std::sqrt(MeanSquare(pcm, channels, lo, lo + w))));
    if (rms.empty()) return 0.0f;
    const size_t k = rms.size() * kQuietPercentile / 100;
    std::nth_element(rms.begin(), rms.begin() + static_cast<std::ptrdiff_t>(k), rms.end());
    return rms[k];
}

int16_t ToS16(float v) {
    // Clamp, then round half away from zero (|v| <= sqrt 2 x 32768 fits an int32_t).
    const float c = v > 32767.0f ? 32767.0f : (v < -32768.0f ? -32768.0f : v);
    return static_cast<int16_t>(static_cast<int32_t>(c < 0.0f ? c - 0.5f : c + 0.5f));
}

}  // namespace

uint32_t CrossfadeFrames(uint32_t rate) {
    return static_cast<uint32_t>(static_cast<uint64_t>(rate) * kCrossfadeMs / 1000u);
}

Result Transform(int16_t* pcm, uint32_t frames, uint32_t channels, uint32_t rate) {
    Result r;
    r.newFrames = frames;
    if (pcm == nullptr || channels < 1 || channels > 2 || rate == 0) return r;
    const uint32_t x = CrossfadeFrames(rate);
    r.crossfadeFrames = x;
    if (x == 0 || frames < 2 * x) return r;

    // 1. The codec tail.
    const float quiet = QuietLevel(pcm, frames, channels, rate);
    r.quietLevel = quiet;
    const double limit = static_cast<double>(kTailRatio) * quiet;
    const double limitSq = limit * limit;
    uint32_t end = frames;
    while (end >= kTailWindowFrames && frames - end + kTailWindowFrames <= kMaxTrimFrames &&
           MeanSquare(pcm, channels, end - kTailWindowFrames, end) < limitSq)
        end -= kTailWindowFrames;
    if (end < 2 * x) return r;   // (an all-silent buffer has quiet 0: nothing is below it, nothing trimmed)

    // 2. The fold: out[j] = cos(t) * pcm[m + j] + sin(t) * pcm[j], t = (j + 0.5) / x * pi / 2, j < x, and
    // the buffer becomes m = end - x frames long. In place: frame j is read before it is written, and
    // frame m + j (>= x > j) is never written.
    const uint32_t m = end - x;
    const float halfPi = 1.57079632679489662f;
    for (uint32_t j = 0; j < x; ++j) {
        const float t = (static_cast<float>(j) + 0.5f) / static_cast<float>(x) * halfPi;
        const float fadeIn = std::sin(t), fadeOut = std::cos(t);
        for (uint32_t c = 0; c < channels; ++c) {
            const size_t head = static_cast<size_t>(j) * channels + c;
            const size_t tail = static_cast<size_t>(m + j) * channels + c;
            pcm[head] = ToS16(fadeOut * static_cast<float>(pcm[tail]) + fadeIn * static_cast<float>(pcm[head]));
        }
    }
    r.applied = true;
    r.framesTrimmed = frames - end;
    r.newFrames = m;
    return r;
}

uint32_t ApplyToLoopingBuffer(int16_t* pcm, uint32_t frames, uint32_t channels, uint32_t rate,
                              const char* name) {
    const Result r = Transform(pcm, frames, channels, rate);
    std::fprintf(stderr,
                 "[SeamlessLoops] %s: %s frames=%u ch=%u rate=%u trimmed=%u (%.1f ms) crossfade=%u "
                 "quiet=%.0f -> %u frames\n",
                 name != nullptr && name[0] != '\0' ? name : "(unnamed)",
                 r.applied ? "applied" : "skipped", frames, channels, rate, r.framesTrimmed,
                 rate ? r.framesTrimmed * 1000.0 / rate : 0.0, r.crossfadeFrames, r.quietLevel, r.newFrames);
    return r.newFrames;
}

}  // namespace SeamlessLoops
}  // namespace Ext
