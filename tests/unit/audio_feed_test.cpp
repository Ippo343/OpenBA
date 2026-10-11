// ============================================================================
// tests/unit/audio_feed_test.cpp -- a voice's buffer queue (Phase D.4 SDL-4)
// ============================================================================
// src/platform/common/audio_feed.{h,cpp}: the XAudio2 source-voice semantics the SDL
// backend keeps per voice. What the game depends on:
//   * Queued() counts submitted, unfinished buffers, the playing one included (the
//     Stream worker waits below 3 and drains to 0);
//   * one BufferEnd per finished buffer, EndOfStreamPut after an END_OF_STREAM one;
//   * PlayBegin is a frame offset; a loop pass restarts at byte 0 (XWMA A4.5);
//     255 = forever, n = n extra passes.
// No SDL (D4_RULES C7).
// ============================================================================
#include "unit.h"

#include <cstdint>
#include <string>
#include <vector>

#include "audio_feed.h"

namespace {

// Records what Pull reports: the bytes as one string of values, the events as letters
// (S start, L loop end, E buffer end, F end of stream).
class Recorder final : public Platform::AudioFeed::Sink {
public:
    void Put(const uint8_t* data, uint32_t bytes) override {
        for (uint32_t i = 0; i < bytes; ++i) bytesOut.push_back(data[i]);
    }
    void BufferStart() override { events += 'S'; }
    void LoopEnd() override { events += 'L'; }
    void BufferEnd() override { events += 'E'; }
    void EndOfStreamPut() override { events += 'F'; }
    std::vector<uint8_t> bytesOut;
    std::string events;
};

std::vector<uint8_t> Ramp(uint32_t n, uint8_t first = 0) {
    std::vector<uint8_t> v(n);
    for (uint32_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(first + i);
    return v;
}

}  // namespace

UNIT_TEST(audio_feed_one_shot_from_play_begin_then_end_of_stream) {
    // Stereo S16 (4 bytes a frame), 8 frames, PlayBegin frame 3.
    const std::vector<uint8_t> pcm = Ramp(32);
    Platform::AudioFeed feed(4);
    CHECK(feed.Submit(pcm.data(), 32, 3, 0, true));
    CHECK_EQ(feed.Queued(), 1u);

    Recorder r;
    feed.Pull(8, r);                                  // two frames
    CHECK_EQ(r.bytesOut.size(), size_t(8));
    CHECK_EQ(r.bytesOut[0], 12);                      // frame 3
    CHECK(r.events == "S");
    CHECK_EQ(feed.Queued(), 1u);                      // still playing

    feed.Pull(100, r);                                // the rest, and the end
    CHECK_EQ(r.bytesOut.size(), size_t(20));
    CHECK_EQ(r.bytesOut.back(), 31);
    CHECK(r.events == "SEF");
    CHECK_EQ(feed.Queued(), 0u);

    feed.Pull(100, r);                                // nothing more
    CHECK(r.events == "SEF");
}

UNIT_TEST(audio_feed_pull_rounds_up_to_whole_frames) {
    const std::vector<uint8_t> pcm = Ramp(16);
    Platform::AudioFeed feed(4);
    CHECK(feed.Submit(pcm.data(), 18, 0, 0, false));  // 18 -> 16 bytes (whole frames)
    Recorder r;
    feed.Pull(5, r);                                  // 5 -> 8
    CHECK_EQ(r.bytesOut.size(), size_t(8));
    feed.Pull(100, r);
    CHECK_EQ(r.bytesOut.size(), size_t(16));
    CHECK(r.events == "SE");                          // no END_OF_STREAM flag
}

UNIT_TEST(audio_feed_finite_loop_restarts_at_zero) {
    // 4 mono S16 frames, PlayBegin 2, one extra pass: frames 2,3 then 0,1,2,3.
    const std::vector<uint8_t> pcm = Ramp(8);
    Platform::AudioFeed feed(2);
    CHECK(feed.Submit(pcm.data(), 8, 2, 1, true));
    Recorder r;
    feed.Pull(1000, r);
    const std::vector<uint8_t> want = {4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
    CHECK(r.bytesOut == want);
    CHECK(r.events == "SLEF");
    CHECK_EQ(feed.Queued(), 0u);
}

UNIT_TEST(audio_feed_infinite_loop_never_ends) {
    const std::vector<uint8_t> pcm = Ramp(8);
    Platform::AudioFeed feed(2);
    CHECK(feed.Submit(pcm.data(), 8, 0, Platform::AudioFeed::kLoopInfinite, true));
    Recorder r;
    for (int i = 0; i < 10; ++i) feed.Pull(6, r);     // 60 bytes = 7.5 passes
    CHECK_EQ(r.bytesOut.size(), size_t(60));
    CHECK_EQ(r.bytesOut[8], 0);                       // pass 2 starts at byte 0
    CHECK_EQ(r.bytesOut[59], 3);
    CHECK_EQ(feed.Queued(), 1u);
    CHECK(r.events.find('E') == std::string::npos);
    CHECK_EQ(r.events.size(), size_t(8));             // S + 7 loop ends
}

UNIT_TEST(audio_feed_stream_worker_queue_accounting) {
    // The Stream worker: chunks without END_OF_STREAM, then a last one with it. The
    // count includes the one being played and drops by one per finished chunk.
    const std::vector<uint8_t> a = Ramp(8, 0), b = Ramp(8, 100), c = Ramp(8, 200);
    Platform::AudioFeed feed(4);
    CHECK(feed.Submit(a.data(), 8, 0, 0, false));
    CHECK(feed.Submit(b.data(), 8, 0, 0, false));
    CHECK(feed.Submit(c.data(), 8, 0, 0, true));
    CHECK_EQ(feed.Queued(), 3u);
    Recorder r;
    feed.Pull(4, r);
    CHECK_EQ(feed.Queued(), 3u);
    feed.Pull(4, r);                                  // a finishes
    CHECK_EQ(feed.Queued(), 2u);
    feed.Pull(12, r);                                 // b finishes, c half
    CHECK_EQ(feed.Queued(), 1u);
    CHECK_EQ(r.bytesOut[8], 100);
    CHECK_EQ(r.bytesOut[16], 200);
    feed.Pull(4, r);
    CHECK_EQ(feed.Queued(), 0u);
    CHECK(r.events == "SESESEF");
}

UNIT_TEST(audio_feed_empty_and_full) {
    Platform::AudioFeed feed(4);
    Recorder r;
    // A zero-length buffer (an xWMA buffer the decoder refused) finishes at the next
    // pull, even one asking for nothing, and never loops.
    CHECK(feed.Submit(nullptr, 0, 0, Platform::AudioFeed::kLoopInfinite, true));
    feed.Pull(0, r);
    CHECK(r.events == "SEF");
    CHECK_EQ(feed.Queued(), 0u);
    // PlayBegin past the end: nothing to hand over, the buffer just finishes.
    const std::vector<uint8_t> pcm = Ramp(8);
    CHECK(feed.Submit(pcm.data(), 8, 99, 0, false));
    feed.Pull(16, r);
    CHECK(r.bytesOut.empty());
    CHECK(r.events == "SEFSE");
    // XAUDIO2_MAX_QUEUED_BUFFERS: the 65th submit is refused.
    for (uint32_t i = 0; i < Platform::AudioFeed::kMaxQueued; ++i)
        CHECK(feed.Submit(pcm.data(), 8, 0, 0, false));
    CHECK(!feed.Submit(pcm.data(), 8, 0, 0, false));
    CHECK_EQ(feed.Queued(), Platform::AudioFeed::kMaxQueued);
}
