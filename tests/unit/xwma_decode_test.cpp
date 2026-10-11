// ============================================================================
// tests/unit/xwma_decode_test.cpp — Audio::XwmaDecoder against XAudio2 2.7 (Phase D.4 SDL A3)
// ============================================================================
// THE FIXTURE (tests/unit/data/, provenance in data/README.md):
//   xwma_fixture.bin  `Audio\Animal\Sheep1.wav` exactly as the game stores it: the
//                     13,452-byte run SoundBuffer::PlatformSerialize reads out of
//                     WinData/data0.pkg at 0x2d802 -- int32 length, int32 table
//                     bytes, the seek table, the 0x28-byte format window, the packets.
//                     Mono, 44.1 kHz, 48 kbit/s, 6 packets of 2230 bytes, 65536 samples.
//   xwma_fixture.pcm  the same packets decoded ONCE by XAudio2 2.7 (one source voice,
//                     one buffer, its XAUDIO2_BUFFER_WMA table, a pass-through XAPO
//                     capturing the decoded float, the mastering voice at 44.1 kHz mono:
//                     no resampling), converted to S16 by round-to-nearest; 65536
//                     little-endian int16.
//
// THE TOLERANCE. FFmpeg's WMA v2 decoder and XAudio2's are different implementations of
// one algorithm; their float output differs by at most 1.53e-5 on this fixture (and on
// two shipped stereo sounds, third_party/wmadec/EXTRACT.md), i.e. under half an S16
// step, so after rounding a sample can differ by one LSB and no more. Measured on this
// fixture: max |diff| 1 LSB, RMS 0.025 LSB, 99.9 % of samples identical. Pinned with a
// 2x margin: max 2 LSB, RMS 0.05 LSB, >= 99.5 % identical. A timing slip of even one
// sample, or a wrong frame drop at either end, fails all three by orders of magnitude.
// ============================================================================
#include "unit.h"
#include "audio/xwma_decode.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

std::string DataPath(const char* name) {
    std::string dir = __FILE__;
    const size_t slash = dir.find_last_of("/\\");
    dir = (slash == std::string::npos) ? std::string(".") : dir.substr(0, slash);
    return dir + "/data/" + name;
}

std::vector<uint8_t> ReadAll(const char* name) {
    std::vector<uint8_t> bytes;
    FILE* f = std::fopen(DataPath(name).c_str(), "rb");
    if (f == nullptr) return bytes;
    uint8_t chunk[4096];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) bytes.insert(bytes.end(), chunk, chunk + n);
    std::fclose(f);
    return bytes;
}

uint32_t ReadU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// The PlatformSerialize run, split into the four pieces SoundBuffer holds.
struct Fixture {
    std::vector<uint8_t>  raw;
    std::vector<uint32_t> table;
    const uint8_t* format = nullptr;
    const uint8_t* packets = nullptr;
    size_t bytes = 0;
    bool ok = false;

    Fixture() : raw(ReadAll("xwma_fixture.bin")) {
        if (raw.size() < 8) return;
        const int32_t length = int32_t(ReadU32(&raw[0]));
        const int32_t tableBytes = int32_t(ReadU32(&raw[4]));
        if (length <= 0 || tableBytes <= 0 || tableBytes % 4 != 0) return;
        if (raw.size() != 8u + size_t(tableBytes) + 0x28u + size_t(length)) return;
        table.resize(size_t(tableBytes) / 4);
        for (size_t i = 0; i < table.size(); ++i) table[i] = ReadU32(&raw[8 + 4 * i]);
        format = raw.data() + 8 + tableBytes;
        packets = format + 0x28;
        bytes = size_t(length);
        ok = true;
    }
    bool Open(Audio::XwmaDecoder& dec) const {
        return ok && dec.Open(format, packets, bytes, table.data(), uint32_t(table.size()));
    }
};

std::vector<int16_t> Reference() {
    const std::vector<uint8_t> raw = ReadAll("xwma_fixture.pcm");
    std::vector<int16_t> pcm(raw.size() / 2);
    for (size_t i = 0; i < pcm.size(); ++i)
        pcm[i] = int16_t(uint16_t(raw[2 * i] | (raw[2 * i + 1] << 8)));
    return pcm;
}

std::vector<int16_t> DecodeWhole(Audio::XwmaDecoder& dec) {
    std::vector<int16_t> pcm(size_t(dec.TotalSamples()) * dec.Channels());
    const size_t n = dec.DecodePackets(0, dec.PacketCount(), pcm.data(), size_t(dec.TotalSamples()));
    pcm.resize(n * dec.Channels());
    return pcm;
}

struct Diff { int maxAbs = 0; double rms = 0.0; double identical = 0.0; };
Diff Compare(const int16_t* a, const int16_t* b, size_t n) {
    Diff d;
    if (n == 0) return d;
    double sq = 0.0;
    size_t same = 0;
    for (size_t i = 0; i < n; ++i) {
        const int e = int(a[i]) - int(b[i]);
        const int ae = e < 0 ? -e : e;
        if (ae > d.maxAbs) d.maxAbs = ae;
        sq += double(e) * e;
        if (e == 0) ++same;
    }
    d.rms = std::sqrt(sq / double(n));
    d.identical = double(same) / double(n);
    return d;
}

}  // namespace

UNIT_TEST(xwma_fixture_opens_as_the_game_stores_it) {
    const Fixture fx;
    CHECK(fx.ok);
    Audio::XwmaDecoder dec;
    CHECK(fx.Open(dec));
    CHECK(dec.IsOpen());
    CHECK_EQ(dec.Channels(), 1u);
    CHECK_EQ(dec.SampleRate(), 44100u);
    CHECK_EQ(dec.BlockAlign(), 2230u);
    CHECK_EQ(dec.PacketCount(), 6u);
    CHECK_EQ(dec.FrameLength(), 2048u);
    CHECK_EQ(dec.TotalSamples(), uint64_t(65536));   // seek table 0x20000 bytes / 2
    CHECK_EQ(dec.SeekSamples(0), uint64_t(0));
    CHECK_EQ(dec.SeekSamples(3), uint64_t(30720));   // 0xf000 / 2
    CHECK_EQ(dec.NextPacket(), 0u);
}

UNIT_TEST(xwma_decode_at_load_matches_xaudio2) {
    const Fixture fx;
    const std::vector<int16_t> ref = Reference();
    CHECK_EQ(ref.size(), size_t(65536));
    Audio::XwmaDecoder dec;
    CHECK(fx.Open(dec));
    const std::vector<int16_t> pcm = DecodeWhole(dec);
    CHECK_EQ(pcm.size(), size_t(65536));
    CHECK_EQ(dec.NextPacket(), 6u);
    CHECK_EQ(dec.PendingSamples(), size_t(0));
    CHECK(!dec.Failed());
    if (pcm.size() != ref.size()) return;
    const Diff d = Compare(pcm.data(), ref.data(), pcm.size());
    std::printf("  xwma vs XAudio2 2.7: max |diff| %d LSB, RMS %.4f LSB, %.2f %% identical\n",
                d.maxAbs, d.rms, 100.0 * d.identical);
    CHECK(d.maxAbs <= 2);
    CHECK(d.rms <= 0.05);
    CHECK(d.identical >= 0.995);
}

// The Stream worker's shape: the pass continues across calls, and output that does
// not fit is carried. Same samples, same order, as the one-call decode.
UNIT_TEST(xwma_chunks_continue_the_pass) {
    const Fixture fx;
    Audio::XwmaDecoder whole;
    CHECK(fx.Open(whole));
    const std::vector<int16_t> one = DecodeWhole(whole);

    for (uint32_t chunk : {1u, 2u, 4u}) {
        Audio::XwmaDecoder dec;
        CHECK(fx.Open(dec));
        std::vector<int16_t> got;
        std::vector<int16_t> buf(1500);           // smaller than a chunk: forces the carry
        uint32_t packet = 0;
        while (packet < dec.PacketCount()) {
            const uint32_t n = std::min(chunk, dec.PacketCount() - packet);
            size_t w = dec.DecodePackets(packet, n, buf.data(), buf.size());
            got.insert(got.end(), buf.begin(), buf.begin() + std::ptrdiff_t(w));
            packet += n;
            CHECK_EQ(dec.NextPacket(), packet);
            while (dec.PendingSamples() > 0) {
                w = dec.DecodePackets(dec.NextPacket(), 0, buf.data(), buf.size());
                got.insert(got.end(), buf.begin(), buf.begin() + std::ptrdiff_t(w));
            }
        }
        CHECK_EQ(got.size(), one.size());
        CHECK(got == one);
        CHECK_EQ(dec.DecodePackets(dec.NextPacket(), 1, buf.data(), buf.size()), size_t(0));
    }
}

// The Stream worker's real shape: no whole track in memory. Open with the table and
// format only; each chunk's packets arrive in a reused buffer (Stream::Block's ring),
// which is overwritten as soon as DecodeChunk returns.
UNIT_TEST(xwma_stream_chunks_from_a_reused_buffer) {
    const Fixture fx;
    Audio::XwmaDecoder whole;
    CHECK(fx.Open(whole));
    const std::vector<int16_t> one = DecodeWhole(whole);

    for (uint32_t chunk : {2u, 16u}) {
        Audio::XwmaDecoder dec;
        CHECK(dec.Open(fx.format, nullptr, 0, fx.table.data(), uint32_t(fx.table.size())));
        CHECK_EQ(dec.TotalSamples(), uint64_t(65536));
        std::vector<uint8_t> block(size_t(chunk) * dec.BlockAlign());
        std::vector<int16_t> buf(dec.MaxSamplesFor(chunk));
        std::vector<int16_t> got;
        uint32_t packet = 0;
        while (packet < dec.PacketCount()) {
            const uint32_t n = std::min(chunk, dec.PacketCount() - packet);
            std::copy(fx.packets + size_t(packet) * dec.BlockAlign(),
                      fx.packets + size_t(packet + n) * dec.BlockAlign(), block.begin());
            const size_t w = dec.DecodeChunk(packet, n, block.data(), buf.data(), buf.size());
            got.insert(got.end(), buf.begin(), buf.begin() + std::ptrdiff_t(w));
            std::fill(block.begin(), block.end(), uint8_t(0xA5));   // the ring slot is reused
            packet += n;
        }
        CHECK_EQ(dec.PendingSamples(), size_t(0));
        CHECK(got == one);
        // Without packets, the contiguous entry point has nothing to read.
        CHECK_EQ(dec.DecodePackets(0, 1, buf.data(), buf.size()), size_t(0));
    }
}

// A pass started at packet k > 0 is a fresh decoder there, as a fresh XAudio2 voice
// would be: TotalSamples - SeekSamples(k) - FrameLength samples, the tail of the
// packet-0 pass (XAudio2 2.7 measured the same, third_party/wmadec/EXTRACT.md rule 4).
UNIT_TEST(xwma_pass_from_a_later_packet_is_the_tail) {
    const Fixture fx;
    Audio::XwmaDecoder dec;
    CHECK(fx.Open(dec));
    const std::vector<int16_t> one = DecodeWhole(dec);

    const uint32_t k = 3;
    const size_t expect = size_t(dec.TotalSamples() - dec.SeekSamples(k) - dec.FrameLength());
    CHECK_EQ(expect, size_t(32768));
    std::vector<int16_t> tail(expect + 4096);
    const size_t n = dec.DecodePackets(k, dec.PacketCount() - k, tail.data(), tail.size());
    CHECK_EQ(n, expect);
    if (n != expect || one.size() < n) return;
    const Diff d = Compare(tail.data(), one.data() + (one.size() - n), n);
    CHECK(d.maxAbs <= 2);
    CHECK(d.identical >= 0.995);

    // Back to packet 0: a new pass, the same samples as the first.
    const std::vector<int16_t> again = DecodeWhole(dec);
    CHECK(again == one);
}

UNIT_TEST(xwma_open_refuses_what_it_cannot_decode) {
    const Fixture fx;
    CHECK(fx.ok);
    if (!fx.ok) return;
    Audio::XwmaDecoder dec;
    std::vector<uint8_t> fmt(fx.format, fx.format + 0x28);

    std::vector<uint8_t> pcmTag = fmt;                 // WAVE_FORMAT_PCM
    pcmTag[0] = 0x01; pcmTag[1] = 0x00;
    CHECK(!dec.Open(pcmTag.data(), fx.packets, fx.bytes, fx.table.data(), uint32_t(fx.table.size())));
    CHECK(!dec.IsOpen());

    std::vector<uint8_t> six = fmt;                    // 6 channels
    six[2] = 6;
    CHECK(!dec.Open(six.data(), fx.packets, fx.bytes, fx.table.data(), uint32_t(fx.table.size())));

    CHECK(!dec.Open(fmt.data(), fx.packets, fx.bytes - 1, fx.table.data(), uint32_t(fx.table.size())));
    CHECK(!dec.Open(fmt.data(), fx.packets, fx.bytes, fx.table.data(), 0));
    CHECK(!dec.IsOpen());
    CHECK_EQ(dec.DecodePackets(0, 1, nullptr, 0), size_t(0));

    CHECK(dec.Open(fmt.data(), fx.packets, fx.bytes, fx.table.data(), uint32_t(fx.table.size())));
    dec.Close();
    CHECK(!dec.IsOpen());
    CHECK_EQ(dec.TotalSamples(), uint64_t(0));
}
