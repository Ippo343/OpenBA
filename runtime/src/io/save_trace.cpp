// ============================================================================
// runtime/src/io/save_trace.cpp — Wave 9 lane S: the save-stream A/B oracle
// ============================================================================
// See save_trace.h for why this exists and what it does not cover.
// ============================================================================

#include "save_trace.h"
#include "../../types/io/Serializer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace RtSaveTrace {
namespace {

using rt::io::Serializer;
using rt::io::Stream;

// One typed emission. `value` holds the emitted scalar zero-extended, or the
// element count for an array slot -- enough to localise a divergence without
// dereferencing anything the callee owns.
struct Emission {
    int      slot;      // rw_vtbl byte offset
    uint64_t a, b, c;   // RDX, R8, R9 as they arrived
    uint64_t value;     // decoded payload (see kSlots)
};

// The recorder a run writes into. Two exist per Compare -- one per side --
// which is the whole point: the sinks must be independent or this degenerates
// into the shared-stream problem the family was held back for.
struct Recorder {
    std::vector<Emission> log;
    bool overflowed = false;
};

Recorder* g_active = nullptr;      // set for the duration of one side's call
size_t    g_maxEmissions = 200000; // a runaway port must not eat the heap

// ---------------------------------------------------------------------------
// The slot table. Kinds decide how much of the argument list is a value.
//
// Offsets are rw_vtbl's, i.e. RETAIL. Copying them from a profile decompile is
// the mistake runtime/src/io/rt_io_port.cpp's warning block exists to prevent
// (bool +0x78 not +0x90, uint +0x98 not +0xb0, int +0xa0 not +0xb8), and this
// file would silently mis-attribute every emission if it repeated it.
// ---------------------------------------------------------------------------
enum Kind : uint8_t { kNone, kScalar, kArray, kBytes, kVersion, kDtor, kSkip };

struct SlotInfo { int off; Kind kind; uint8_t size; const char* name; };

const SlotInfo kSlots[] = {
    { rt::io::rw_vtbl::kDtor,           kDtor,    0, "dtor" },
    { rt::io::rw_vtbl::kGetVersion,     kVersion, 0, "GetVersion" },
    { rt::io::rw_vtbl::kSkip,           kSkip,    0, "Skip" },
    { rt::io::rw_vtbl::kArrayChar,      kArray,   1, "char[]" },
    { rt::io::rw_vtbl::kArrayBool,      kArray,   1, "bool[]" },
    { rt::io::rw_vtbl::kArrayFloat,     kArray,   4, "float[]" },
    { rt::io::rw_vtbl::kArrayUInt64,    kArray,   8, "uint64[]" },
    { rt::io::rw_vtbl::kArrayInt64,     kArray,   8, "int64[]" },
    { rt::io::rw_vtbl::kArrayUInt,      kArray,   4, "uint[]" },
    { rt::io::rw_vtbl::kArrayInt,       kArray,   4, "int[]" },
    { rt::io::rw_vtbl::kArrayUShort,    kArray,   2, "ushort[]" },
    { rt::io::rw_vtbl::kArrayShort,     kArray,   2, "short[]" },
    { rt::io::rw_vtbl::kArrayUChar,     kArray,   1, "uchar[]" },
    { rt::io::rw_vtbl::kArraySChar,     kArray,   1, "schar[]" },
    { rt::io::rw_vtbl::kScalarChar,     kScalar,  1, "char" },
    { rt::io::rw_vtbl::kScalarBool,     kScalar,  1, "bool" },
    { rt::io::rw_vtbl::kScalarFloat,    kScalar,  4, "float" },
    { rt::io::rw_vtbl::kScalarUInt64,   kScalar,  8, "uint64" },
    { rt::io::rw_vtbl::kScalarInt64,    kScalar,  8, "int64" },
    { rt::io::rw_vtbl::kScalarUInt,     kScalar,  4, "uint" },
    { rt::io::rw_vtbl::kScalarInt,      kScalar,  4, "int" },
    { rt::io::rw_vtbl::kScalarUShort,   kScalar,  2, "ushort" },
    { rt::io::rw_vtbl::kScalarShort,    kScalar,  2, "short" },
    { rt::io::rw_vtbl::kScalarUChar,    kScalar,  1, "uchar" },
    { rt::io::rw_vtbl::kScalarSChar,    kScalar,  1, "schar" },
    { rt::io::rw_vtbl::kSerializeBytes, kBytes,   0, "bytes" },
};

const SlotInfo* InfoForSlot(int off) {
    for (const SlotInfo& s : kSlots)
        if (s.off == off) return &s;
    return nullptr;
}

// Encoding emissions carry the +0x1000 tag; report them as such rather than
// looking them up in the ReaderWriter table, where the offsets mean other things.
const char* SlotName(int off) {
    if (off & 0x1000) return "Encoding";
    const SlotInfo* i = InfoForSlot(off);
    return i ? i->name : "?";
}

uint64_t ReadValue(const void* p, uint8_t size) {
    if (!p || !size) return 0;
    uint64_t v = 0;
    std::memcpy(&v, p, size);
    return v;
}

// ---------------------------------------------------------------------------
// The recording thunks.
//
// Microsoft x64 passes the first four arguments in RCX/RDX/R8/R9 regardless of
// their declared types, so ONE 4-argument signature captures every slot in the
// table above -- all of which are `this` plus at most three. The return lands
// in RAX, which is right for the two slots anything reads (`GetVersion`, and
// the Encoding factories below).
// ---------------------------------------------------------------------------
using ThunkFn = uint64_t (*)(void*, uint64_t, uint64_t, uint64_t);

uint64_t RecordEmission(int slot, uint64_t a, uint64_t b, uint64_t c) {
    Recorder* r = g_active;
    if (!r) return 0;                       // a stray call outside a Compare
    if (r->log.size() >= g_maxEmissions) { r->overflowed = true; return 0; }

    const SlotInfo* info = InfoForSlot(slot);
    uint64_t value = 0;
    if (info) {
        switch (info->kind) {
            case kScalar:
                // (T& value, const char* name) -> RDX is the value pointer.
                value = ReadValue(reinterpret_cast<const void*>(a), info->size);
                break;
            case kArray:
            case kBytes:
                // (int n, T* data, const char* name) -> RDX is the count. The
                // elements are deliberately NOT read: on the write path they
                // belong to the object under test and a length mismatch is
                // already a divergence, so reading them adds risk, not signal.
                value = a;
                break;
            default:
                break;
        }
    }
    r->log.push_back(Emission{ slot, a, b, c, value });
    return 0;
}

// One thunk per slot, generated so each knows its own offset.
template <int SlotOff>
uint64_t Thunk(void*, uint64_t a, uint64_t b, uint64_t c) {
    return RecordEmission(SlotOff, a, b, c);
}

// ---------------------------------------------------------------------------
// The synthetic sink: Stream -> Encoding -> ReaderWriter, all ours.
// ---------------------------------------------------------------------------
constexpr int kRwSlots  = 0xD0 / 8;   // one past kSerializeBytes
constexpr int kEncSlots = 0x80 / 8;   // ALL bound — see RecordEncoding

struct Sink {
    void*  rwVtable[kRwSlots];
    void*  rwObject[2];               // [0] = vtable ptr; [1] = spare
    void*  encVtable[kEncSlots];
    void*  encObject[2];
    Stream stream;
    Serializer serializer;
};

// The Encoding's two factory slots. Retail picks +0x18 when the stream reads
// and +0x20 when it writes (`CMP [stream+8],0; JNZ -> +0x20`); we always drive
// the writing side, so +0x20 is the one that fires. Both return our stub
// ReaderWriter so a body that takes the other branch still records rather than
// dereferencing null.
Sink* g_sink[2] = { nullptr, nullptr };
int   g_sinkSide = 0;

// EVERY Encoding slot is bound, not just the two the disassembly of
// Audio::Scene::Serialize showed (+0x18 read / +0x20 write).
//
// The first run of this harness crashed with EXEC-of-0 on this vtable: retail
// reaches an Encoding slot the caller's own disassembly never named, because
// the bulk element pass (0x180052640) derives its own reader/writer from the
// Stream rather than reusing the caller's. A null slot faults; a bound one
// RECORDS, which turns "something crashed" into a line naming the slot. That is
// the whole reason to bind the unknown ones rather than the known ones.
//
// Encoding emissions are tagged +0x1000 so they cannot collide with a
// ReaderWriter slot in the trace, and every one returns the stub ReaderWriter:
// harmless for a slot that wanted something else, and correct for the factory
// pair, which is the only place a return value is consumed.
uint64_t RecordEncoding(int slot) {
    Recorder* r = g_active;
    Sink* s = g_sink[g_sinkSide];
    if (r && r->log.size() < g_maxEmissions)
        r->log.push_back(Emission{ 0x1000 | slot, 0, 0, 0, 0 });
    return s ? reinterpret_cast<uint64_t>(&s->rwObject[0]) : 0;
}

template <int SlotOff>
uint64_t EncThunk(void*, uint64_t, uint64_t, uint64_t) {
    return RecordEncoding(SlotOff);
}

void BuildSink(Sink& s) {
    std::memset(&s, 0, sizeof(s));

    for (int i = 0; i < kRwSlots; ++i) s.rwVtable[i] = nullptr;
    // Fill only the slots the table names; anything else stays null and would
    // fault loudly rather than silently recording a slot we have not mapped.
#define RT_BIND(off) s.rwVtable[(off) / 8] = reinterpret_cast<void*>(&Thunk<off>)
    RT_BIND(0x00); RT_BIND(0x08); RT_BIND(0x10); RT_BIND(0x18);
    RT_BIND(0x20); RT_BIND(0x28); RT_BIND(0x30); RT_BIND(0x38);
    RT_BIND(0x40); RT_BIND(0x48); RT_BIND(0x50); RT_BIND(0x58);
    RT_BIND(0x60); RT_BIND(0x68); RT_BIND(0x70); RT_BIND(0x78);
    RT_BIND(0x80); RT_BIND(0x88); RT_BIND(0x90); RT_BIND(0x98);
    RT_BIND(0xA0); RT_BIND(0xA8); RT_BIND(0xB0); RT_BIND(0xB8);
    RT_BIND(0xC0); RT_BIND(0xC8);
#undef RT_BIND
    s.rwObject[0] = s.rwVtable;

#define RT_ENC(off) s.encVtable[(off) / 8] = reinterpret_cast<void*>(&EncThunk<off>)
    RT_ENC(0x00); RT_ENC(0x08); RT_ENC(0x10); RT_ENC(0x18);
    RT_ENC(0x20); RT_ENC(0x28); RT_ENC(0x30); RT_ENC(0x38);
    RT_ENC(0x40); RT_ENC(0x48); RT_ENC(0x50); RT_ENC(0x58);
    RT_ENC(0x60); RT_ENC(0x68); RT_ENC(0x70); RT_ENC(0x78);
#undef RT_ENC
    s.encObject[0] = s.encVtable;

    s.stream.vtable    = nullptr;   // retail never dispatches on it here
    s.stream.direction = 1;         // non-zero == WRITING (Serializer.h)
    s.stream.encoding  = reinterpret_cast<rt::io::Encoding*>(&s.encObject[0]);

    s.serializer.stream       = &s.stream;
    s.serializer.readerWriter = reinterpret_cast<rt::io::ReaderWriter*>(&s.rwObject[0]);
}

int g_compared = 0;
int g_diverged = 0;

}  // namespace

bool Enabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_RT_SAVE_TRACE");
        on = (e && *e && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return on != 0;
}

bool Compare(const char* name, void* obj,
             void (*ours)(void*, void*), void (*theirs)(void*, void*)) {
    if (!Enabled() || !obj || !ours || !theirs) return true;
    if (ours == theirs) {
        // The vacuous pass this project keeps paying for: if the port is not
        // registered, both pointers are the shipped body and the comparison
        // proves nothing while reporting green
        // (reference_uncalled_port_is_not_verified.md).
        std::fprintf(stderr, "[RtSaveTrace] FAIL %s: ours == theirs (%p) — the port is "
                             "NOT registered, so this would compare the DLL with itself\n",
                     name, reinterpret_cast<void*>(ours));
        ++g_diverged;
        return false;
    }

    static Sink sinkA, sinkB;   // static: ~1 KB each, and Compare is not reentrant
    Recorder recA, recB;
    BuildSink(sinkA);
    BuildSink(sinkB);
    g_sink[0] = &sinkA;
    g_sink[1] = &sinkB;

    g_sinkSide = 0; g_active = &recA; ours(obj, &sinkA.serializer);
    g_sinkSide = 1; g_active = &recB; theirs(obj, &sinkB.serializer);
    g_active = nullptr;

    ++g_compared;

    if (recA.overflowed || recB.overflowed) {
        std::fprintf(stderr, "[RtSaveTrace] FAIL %s: emission cap (%zu) hit — "
                             "ours=%zu theirs=%zu\n",
                     name, g_maxEmissions, recA.log.size(), recB.log.size());
        ++g_diverged;
        return false;
    }

    const size_t n = recA.log.size() < recB.log.size() ? recA.log.size() : recB.log.size();
    for (size_t i = 0; i < n; ++i) {
        const Emission& x = recA.log[i];
        const Emission& y = recB.log[i];
        if (x.slot != y.slot || x.value != y.value) {
            std::fprintf(stderr,
                "[RtSaveTrace] DIVERGE %s at emission %zu: ours %s(+0x%x)=%llu  "
                "theirs %s(+0x%x)=%llu   (%zu vs %zu emissions total)\n",
                name, i,
                SlotName(x.slot), x.slot & 0xfff, (unsigned long long)x.value,
                SlotName(y.slot), y.slot & 0xfff, (unsigned long long)y.value,
                recA.log.size(), recB.log.size());
            ++g_diverged;
            return false;
        }
    }
    if (recA.log.size() != recB.log.size()) {
        std::fprintf(stderr,
            "[RtSaveTrace] DIVERGE %s: same prefix but ours emitted %zu and theirs %zu — "
            "a length difference IS the desync\n",
            name, recA.log.size(), recB.log.size());
        ++g_diverged;
        return false;
    }

    std::fprintf(stderr, "[RtSaveTrace] pass %s: %zu emissions identical\n",
                 name, recA.log.size());
    return true;
}

void Summary() {
    if (!Enabled()) return;
    std::fprintf(stderr, "[RtSaveTrace] ==== %d compared, %d diverged ====\n",
                 g_compared, g_diverged);
    std::fflush(stderr);
}

}  // namespace RtSaveTrace
