#pragma once
// ============================================================================
// src/platform/common/audio_feed.h -- a voice's buffer queue, OS-free (Phase D.4 SDL-4)
// ============================================================================
// The part of an XAudio2 source voice an SDL3 audio stream does not have: the queue
// of submitted buffers, each with its PlayBegin, its loop count and its
// END_OF_STREAM flag, and the per-buffer notifications. The SDL backend
// (src/platform/sdl/audio_sdl.cpp) owns one per voice and drives it from the
// stream's get-callback; nothing here calls SDL, so tests/unit/audio_feed_test.cpp
// pins it (D4_RULES C7: the unit binary never links SDL).
//
// THE MODEL. Bytes are handed to the sink ON DEMAND: Pull(want) gives the sink at
// least `want` bytes (as many as the queue holds), never more than it asks for
// beyond one frame of rounding. A buffer is FINISHED when its last byte (of its
// last pass) has been handed over; then it leaves the queue and the sink hears
// BufferEnd, and EndOfStreamPut when it carried END_OF_STREAM. Queued() counts the
// buffers submitted and not yet finished, the one playing included
// (XAUDIO2_VOICE_STATE::BuffersQueued; the Stream worker's high-water mark 3 and
// its drain to 0 read it).
//
// Since SDL pulls only what the device needs for its next pass, "handed over" is
// at most one device period ahead of "played": a buffer finishes that much earlier
// than XAudio2's OnBufferEnd would say. OnStreamEnd waits for the played-out point
// (the backend checks the stream's OUTPUT side after the flush).
//
// LOOPS (handins/XWMA.md A4.5, measured on XAudio2 2.7): a buffer looped by
// XAudio2 with LoopBegin 0 replays the whole buffer, so a pass after the first
// starts at byte 0, not at PlayBegin. kLoopInfinite (255, XAUDIO2_LOOP_INFINITE)
// repeats forever; n < 255 plays n extra passes. A zero-length buffer never loops.
//
// THREADS. Not synchronised: the backend holds the stream's lock around Submit /
// Queued (the submitting thread) and the get-callback already runs under it.
// No allocation after construction (a fixed ring, as XAudio2's 64-buffer limit).
// ============================================================================

#include <cstdint>

namespace Platform {

class AudioFeed {
public:
    static constexpr uint32_t kMaxQueued    = 64;    // XAUDIO2_MAX_QUEUED_BUFFERS
    static constexpr uint32_t kLoopInfinite = 255;   // XAUDIO2_LOOP_INFINITE

    // What Pull reports, in order, on the pulling thread.
    class Sink {
    public:
        virtual void Put(const uint8_t* data, uint32_t bytes) = 0;
        virtual void BufferStart() = 0;      // the first byte of a buffer is handed over
        virtual void LoopEnd() = 0;          // a pass ended and the buffer starts again at 0
        virtual void BufferEnd() = 0;        // the buffer is finished and left the queue
        virtual void EndOfStreamPut() = 0;   // ... and it carried END_OF_STREAM
    protected:
        ~Sink() = default;
    };

    // `frameBytes`: one sample frame of the voice's PCM (channels * bytes per sample).
    explicit AudioFeed(uint32_t frameBytes) : _frameBytes(frameBytes ? frameBytes : 1u) {}

    // Queue `bytes` of `data` (rounded down to whole frames), starting at frame
    // `playBegin` (clamped to the end), played `loopCount` extra times. The data is
    // referenced, not copied: it must stay valid until the buffer finishes. False
    // when kMaxQueued buffers are queued already (XAudio2 refuses the submit too).
    bool Submit(const uint8_t* data, uint32_t bytes, uint32_t playBegin, uint32_t loopCount,
                bool endOfStream);

    uint32_t Queued() const { return _count; }

    // Hand at least `want` bytes to the sink (rounded up to whole frames), or all
    // that is queued. A finished buffer at the head is retired even when want == 0.
    void Pull(uint32_t want, Sink& sink);

private:
    struct Buffer {
        const uint8_t* data;
        uint32_t bytes;
        uint32_t cursor;
        uint32_t loopsLeft;
        bool     infinite;
        bool     endOfStream;
        bool     started;
    };
    Buffer   _ring[kMaxQueued] = {};
    uint32_t _head = 0;
    uint32_t _count = 0;
    uint32_t _frameBytes;
};

}  // namespace Platform
