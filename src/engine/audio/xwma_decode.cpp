// ============================================================================
// src/engine/audio/xwma_decode.cpp — Audio::XwmaDecoder (Phase D.4 SDL A3)
// ============================================================================
// The xWMA layer over third_party/wmadec's C API. xwma_decode.h states the
// contract; third_party/wmadec/EXTRACT.md the XAudio2 2.7 measurements behind
// the two numbers this file applies:
//   * the frames dropped at a pass start: 2 x frame_len from packet 0,
//     1 x frame_len from any later packet (the decoder's warm-up, and at packet
//     0 the encoder's priming frame too);
//   * the samples a pass yields: the seek table's total from packet 0; from
//     packet k, the total minus the table's position before k minus frame_len.
// Everything else is the decoder's own output, converted to S16.
// ============================================================================
#include "audio/xwma_decode.h"

#include <cmath>
#include <cstring>

#include "wmav2_decoder.h"

namespace Audio {

namespace {

uint16_t ReadU16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t ReadU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// [-1, 1] float -> S16: scale by 32768, round to nearest (even), saturate.
int16_t ToS16(float x) {
    const float v = std::nearbyint(x * 32768.0f);
    if (v >= 32767.0f) return 32767;
    if (v <= -32768.0f) return -32768;
    if (v != v) return 0;   // NaN: the decoder does not produce one; never cast it
    return static_cast<int16_t>(v);
}

}  // namespace

XwmaDecoder::~XwmaDecoder() { Close(); }

void XwmaDecoder::Close() {
    if (_decoder != nullptr) {
        wmav2_close(_decoder);
        _decoder = nullptr;
    }
    _packets = nullptr;
    _bytes = 0;
    _seekTable = nullptr;
    _packetCount = 0;
    _channels = _sampleRate = _bitRate = _blockAlign = _frameLength = 0;
    _decoderFresh = false;
    _passActive = false;
    _failed = false;
    _nextPacket = 0;
    _skip = _remaining = 0;
    _pending.clear();
    _pendingRead = 0;
}

bool XwmaDecoder::OpenDecoder() {
    if (_decoder != nullptr) {
        wmav2_close(_decoder);
        _decoder = nullptr;
    }
    uint8_t extradata[6];
    wmav2_xwma_extradata(extradata);
    Wmav2Params params;
    params.channels = static_cast<int>(_channels);
    params.sample_rate = static_cast<int>(_sampleRate);
    params.bit_rate = static_cast<int>(_bitRate);
    params.block_align = static_cast<int>(_blockAlign);
    params.extradata = extradata;
    params.extradata_size = static_cast<int>(sizeof(extradata));
    if (wmav2_open(&params, &_decoder) < 0) {
        _decoder = nullptr;
        return false;
    }
    _frameLength = static_cast<uint32_t>(wmav2_frame_len(_decoder));
    _decoderFresh = true;
    return true;
}

bool XwmaDecoder::Open(const uint8_t* format, const uint8_t* packets, size_t bytes,
                       const uint32_t* seekTable, uint32_t packetCount) {
    Close();
    if (format == nullptr || seekTable == nullptr || packetCount == 0) return false;
    if (packets == nullptr && bytes != 0) return false;   // stream use: no packets, no bytes

    // WAVEFORMATEX: wFormatTag +0, nChannels +2, nSamplesPerSec +4,
    // nAvgBytesPerSec +8, nBlockAlign +12.
    const uint16_t tag        = ReadU16(format + 0);
    const uint16_t channels   = ReadU16(format + 2);
    const uint32_t rate       = ReadU32(format + 4);
    const uint32_t avgBytes   = ReadU32(format + 8);
    const uint16_t blockAlign = ReadU16(format + 12);
    if (tag != kFormatTagWma2 || channels < 1 || channels > 2 || rate == 0 || avgBytes == 0 ||
        blockAlign == 0)
        return false;
    if (packets != nullptr && bytes / blockAlign < packetCount) return false;

    _packets = packets;
    _bytes = bytes;
    _seekTable = seekTable;
    _packetCount = packetCount;
    _channels = channels;
    _sampleRate = rate;
    _bitRate = avgBytes * 8;
    _blockAlign = blockAlign;
    // FFmpeg's own init is the format check that matters (rate, bit rate and
    // block size have to give a frame layout it can decode).
    if (!OpenDecoder()) {
        Close();
        return false;
    }
    return true;
}

uint64_t XwmaDecoder::TotalSamples() const {
    if (!IsOpen()) return 0;
    return _seekTable[_packetCount - 1] / (2u * _channels);
}

uint64_t XwmaDecoder::SeekSamples(uint32_t packet) const {
    if (!IsOpen() || packet == 0) return 0;
    if (packet > _packetCount) packet = _packetCount;
    return _seekTable[packet - 1] / (2u * _channels);
}

size_t XwmaDecoder::MaxSamplesFor(uint32_t packetCount) const {
    return PendingSamples() + (static_cast<size_t>(packetCount) * 15u + 2u) * _frameLength;
}

bool XwmaDecoder::StartPass(uint32_t firstPacket) {
    _passActive = false;
    _failed = false;
    _pending.clear();
    _pendingRead = 0;
    // A pass starts from a decoder that has seen no packet: the one Open made,
    // or a new one (the MDCT overlap and the bit reservoir start empty).
    if (!_decoderFresh && !OpenDecoder()) {
        _failed = true;
        return false;
    }
    _decoderFresh = false;

    _skip = static_cast<uint64_t>(_frameLength) * (firstPacket == 0 ? 2u : 1u);
    const uint64_t total = TotalSamples();
    const uint64_t done = SeekSamples(firstPacket) + (firstPacket == 0 ? 0u : _frameLength);
    _remaining = total > done ? total - done : 0;
    _nextPacket = firstPacket;
    _passActive = true;
    return true;
}

void XwmaDecoder::Append(const float* const* planes, int frames) {
    uint64_t n = frames > 0 ? static_cast<uint64_t>(frames) : 0;
    uint64_t from = 0;
    if (_skip > 0) {
        const uint64_t drop = n < _skip ? n : _skip;
        _skip -= drop;
        from = drop;
    }
    uint64_t keep = n - from;
    if (keep > _remaining) keep = _remaining;
    _remaining -= keep;
    if (keep == 0) return;

    // Compact what has already been returned before growing.
    if (_pendingRead > 0) {
        _pending.erase(_pending.begin(), _pending.begin() + static_cast<std::ptrdiff_t>(_pendingRead));
        _pendingRead = 0;
    }
    const size_t base = _pending.size();
    _pending.resize(base + static_cast<size_t>(keep) * _channels);
    int16_t* dst = _pending.data() + base;
    for (uint64_t s = 0; s < keep; ++s)
        for (uint32_t c = 0; c < _channels; ++c)
            *dst++ = ToS16(planes[c][from + s]);
}

size_t XwmaDecoder::DecodePackets(uint32_t firstPacket, uint32_t packetCount,
                                  int16_t* out, size_t outSamples) {
    // Opened for the stream (no packets): only a pure continuation, which hands
    // back what is pending, is meaningful here.
    if (_packets == nullptr && packetCount != 0) return 0;
    const uint8_t* first =
        _packets ? _packets + static_cast<size_t>(firstPacket) * _blockAlign : nullptr;
    return DecodeRange(firstPacket, packetCount, first, out, outSamples);
}

size_t XwmaDecoder::DecodeChunk(uint32_t firstPacket, uint32_t packetCount, const uint8_t* chunk,
                                int16_t* out, size_t outSamples) {
    if (chunk == nullptr && packetCount != 0) return 0;
    return DecodeRange(firstPacket, packetCount, chunk, out, outSamples);
}

size_t XwmaDecoder::DecodeRange(uint32_t firstPacket, uint32_t packetCount, const uint8_t* first,
                                int16_t* out, size_t outSamples) {
    if (!IsOpen() || firstPacket >= _packetCount) {
        // Only a continuation with nothing left to decode may still hand back
        // what is pending.
        if (!(IsOpen() && _passActive && firstPacket == _nextPacket)) return 0;
        packetCount = 0;
    } else if (!_passActive || firstPacket != _nextPacket) {
        if (!StartPass(firstPacket)) return 0;
    }
    if (packetCount > _packetCount - firstPacket) packetCount = _packetCount - firstPacket;

    for (uint32_t i = 0; i < packetCount; ++i) {
        const uint32_t packet = firstPacket + i;
        const float* const* planes = nullptr;
        int frames = 0;
        const uint8_t* data = first + static_cast<size_t>(i) * _blockAlign;
        if (wmav2_decode_packet(_decoder, data, static_cast<int>(_blockAlign), &planes, &frames) < 0)
            _failed = true;
        else
            Append(planes, frames);
        _nextPacket = packet + 1;

        if (_nextPacket == _packetCount) {
            // The pass's last packet: the frame left in the bit reservoir, then
            // the MDCT overlap (wmav2_drain returns 0 frames once both are out).
            for (int step = 0; step < 2; ++step) {
                if (wmav2_drain(_decoder, &planes, &frames) < 0) { _failed = true; break; }
                if (frames == 0) break;
                Append(planes, frames);
            }
        }
    }

    const size_t available = PendingSamples();
    const size_t n = available < outSamples ? available : outSamples;
    if (n > 0 && out != nullptr) {
        std::memcpy(out, _pending.data() + _pendingRead, n * _channels * sizeof(int16_t));
        _pendingRead += n * _channels;
        if (_pendingRead == _pending.size()) {
            _pending.clear();
            _pendingRead = 0;
        }
        return n;
    }
    return 0;
}

}  // namespace Audio
