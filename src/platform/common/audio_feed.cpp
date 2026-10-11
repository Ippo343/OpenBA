// ============================================================================
// src/platform/common/audio_feed.cpp -- a voice's buffer queue, OS-free (Phase D.4 SDL-4)
// ============================================================================
// audio_feed.h has the model (on-demand hand-over, finished = last byte handed
// over, loops restart at byte 0). Pure; tests/unit/audio_feed_test.cpp.
// ============================================================================
#include "audio_feed.h"

namespace Platform {

bool AudioFeed::Submit(const uint8_t* data, uint32_t bytes, uint32_t playBegin,
                       uint32_t loopCount, bool endOfStream) {
    if (_count >= kMaxQueued) return false;
    Buffer& b = _ring[(_head + _count) % kMaxQueued];
    b.data  = data;
    b.bytes = bytes - bytes % _frameBytes;
    const uint64_t begin = static_cast<uint64_t>(playBegin) * _frameBytes;
    b.cursor = begin < b.bytes ? static_cast<uint32_t>(begin) : b.bytes;
    b.infinite    = loopCount == kLoopInfinite;
    b.loopsLeft   = b.infinite ? 0u : loopCount;
    b.endOfStream = endOfStream;
    b.started     = false;
    ++_count;
    return true;
}

void AudioFeed::Pull(uint32_t want, Sink& sink) {
    if (want % _frameBytes) want += _frameBytes - want % _frameBytes;
    while (_count > 0) {
        Buffer& b = _ring[_head];
        const uint32_t left = b.bytes - b.cursor;
        if (left > 0) {
            if (want == 0) break;
            if (!b.started) {
                b.started = true;
                sink.BufferStart();
            }
            const uint32_t n = left < want ? left : want;
            sink.Put(b.data + b.cursor, n);
            b.cursor += n;
            want -= n;
            if (b.cursor < b.bytes) break;           // want is spent
        }
        // The pass is over. Loop (from byte 0) or retire the buffer.
        if (b.bytes > 0 && (b.infinite || b.loopsLeft > 0)) {
            if (!b.infinite) --b.loopsLeft;
            b.cursor = 0;
            sink.LoopEnd();
            continue;
        }
        if (!b.started) {                            // nothing was ever handed over
            b.started = true;
            sink.BufferStart();
        }
        const bool endOfStream = b.endOfStream;
        _head = (_head + 1) % kMaxQueued;
        --_count;
        sink.BufferEnd();
        if (endOfStream) sink.EndOfStreamPut();
    }
}

}  // namespace Platform
