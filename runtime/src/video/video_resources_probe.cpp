// ============================================================================
// runtime/src/video/video_resources_probe.cpp — Wave 18 lane VRES's oracle
// ============================================================================
//
// SYNTHETIC-SUBJECT REPLAY against the SHIPPED bodies, the shape
// array_destroy_probe.cpp established: build two private, byte-identical
// subjects, run OURS on one and the DLL's body on the other, compare. Two
// subjects, because a destructor cannot be replayed on one without a double
// free.
//
// THE SHIPPED BODY IS REACHED THROUGH ResolveRuntimeRaw, NEVER ResolveAddr.
// ResolveAddr consults the port registry first
// (reference_resolveaddr_consults_port_registry_first.md) and this lane
// registers 0x1800f7290, so ResolveAddr would hand back our own function and
// every comparison would be ours-against-ours and always green
// (reference_shadow_marker_is_not_an_oracle.md is the same failure one level up).
//
// ---------------------------------------------------------------------------
// WHAT IT COVERS — five cases, and the split is deliberate
// ---------------------------------------------------------------------------
// Cases 1-3 drive the THREE ELEMENT-DESTROY HELPERS ??1Device@Video@@ reaches:
// 0x180010cf0 (per DisplayMode), 0x180010da0 (per Adapter) and 0x180010f00
// (per GraphicsInterface). That is where 100% of this lane's parameterization
// lives — every stride, every offset, and the NESTING ORDER — and it is the
// only place a transcription error can hide. None of the three frees the
// element buffer it ITSELF walks, so THAT buffer is fully readable afterwards
// on both sides and the comparison is a real memcmp.
//
// *** EACH LEVEL FREES THE LEVEL BELOW'S BUFFER, so the nested buffers are NOT
// memcmp-able. *** This lane's first draft did it anyway, on the reasoning that
// only the innermost arrays were freed, and the probe refuted that on its own
// first run: two confident MISMATCH lines that were free-list links, then an
// ACCESS_VIOLATION inside CompareBuffers when one of those links was
// dereferenced as a String. Both nested memcmps are gone. What replaces them is
// the byte-delta check below, which is a better instrument for the question
// they were asked: a memory comparator can never see a LEAK, and the
// allocator's own accounting always can.
//
// Cases 4-5 drive ??1Device@Video@@ END TO END with all three arrays EMPTY.
// That covers the four steps the element helpers do not: the DeviceData slot-0
// call, the `deviceData = null` store, the singleton CMOV (case 4 is type != 0,
// case 5 is type == 0, so BOTH arms of the CMOV are driven) and
// DeleteCriticalSection. With the arrays empty the outer body frees nothing, so
// the whole 0x108-byte Device is comparable with no use-after-free.
//
// NOT COVERED, stated rather than left to a green board:
//
//   * The OUTER bodies with a NON-EMPTY array. An outer body's last act is to
//     free the element buffer, so afterwards there is nothing left to compare —
//     reading it would be a use-after-free and the 16-byte header is all zeroes
//     on both sides by construction. array_destroy_probe.cpp declines the same
//     seven bodies for the same reason. They are covered by COMPOSITION: each
//     is `helper(this, _data, _size)` then `ArrayDtorImpl`, the helpers are
//     replayed here and `ArrayDtorImpl` is lane Q's J1, replayed by
//     array_probe.cpp.
//
//   * The `deviceData != null` arm of ??1Device. It is a virtual dispatch to
//     slot 0 with the delete flag set, i.e. it DESTROYS AND FREES a real
//     DeviceData; a synthetic one would need a real DX11 companion and a real
//     swap chain. The arm is one `if` and a vtable index, both read straight off
//     the listing. Case 4/5 drive the null arm only and say so.
//
//   * The ORDER in which blocks are freed. The slots do not alias, so no
//     comparator on the element buffer can see the order — only the allocator
//     can. Reproduced from the disassembly; checkable, if at all, with
//     BANISHED_REDZONE=watch.
//
// ---------------------------------------------------------------------------
// WHAT IS COMPARED, AND WHAT IS DELIBERATELY EXCLUDED
// ---------------------------------------------------------------------------
// reference_oracle_must_not_compare_unwritten_bytes.md: an oracle that memcmps
// bytes NEITHER side writes fails 100% of calls and reads like a catastrophic
// port defect.
//
//   * Element buffers are seeded with a VARYING NON-ZERO pattern, so a wrong
//     stride or a wrong offset writes its zeroes somewhere the pattern notices.
//     An all-zero seed is the one seeding under which a wrong offset is
//     invisible, because both sides read zero there and agree by accident
//     (reference_uniform_population_is_not_a_verification.md).
//
//   * A String slot cannot be memcmp'd. Retail's two arms leave it holding
//     DIFFERENT THINGS: NULL when the string survives, and `str - 8` — the
//     address of the block it just freed — when it does not. Those are
//     per-side allocations and will never agree. Each String slot is therefore
//     compared by CLASSIFICATION (null / this side's own block base /
//     unchanged / other) and every non-slot byte raw. Surviving refcounts are
//     compared as numbers.
//
//   * Inner Array `_data` pointers are per-side allocations too, but both
//     bodies overwrite them with ZERO, so they compare raw and a side that
//     failed to clear them is caught.
//
//   * In cases 4-5 the Device's CRITICAL_SECTION block (+0x60..+0x8F) is
//     EXCLUDED. DeleteCriticalSection leaves per-object debug bookkeeping there
//     that differs between two objects by construction. Everything else in the
//     0x108 bytes is compared, including the array headers and +0x100.
//
// ---------------------------------------------------------------------------
// CONTROLS — scored SEPARATELY, each with its own catch rate
// ---------------------------------------------------------------------------
// EFFECTIVENESS (reference_probe_needs_an_effectiveness_control.md). Every case
// reports how many blocks it actually freed, how many refcounts it decremented
// and how many of those reached zero. If the totals are zero the probe says the
// run PROVES NOTHING, in those words, rather than printing "0 divergences".
//
// NEGATIVE — `BANISHED_RT_VRES_PROBE=neg` runs FOUR deliberately wrong bodies
// through the SAME comparator, and every one must be flagged. Each names the
// mechanism it exercises, so a control that catches via the wrong mechanism is
// visible rather than absorbed into a union
// (reference_union_control_hides_a_blind_screen.md):
//
//   LEAK    — skip the nested recursion in the Adapter/Interface walkers.
//             THIS IS THE EXACT DEFECT reference_a_written_recipe_can_be_wrong
//             is about: a body that only frees the block instead of destroying
//             every element. If the comparator cannot see this one, nothing
//             else in this file is worth reading.
//   NOSTR   — skip the String release. Mechanism: the Body B slot.
//   SHIFT   — Body A's 16-byte header clear lands eight bytes EARLY; the frees
//             stay correct. Mechanism: the OFFSET the clear writes at, which is
//             what a mis-transcribed spec row looks like. It replaced a
//             wrong-stride control that produced a wild free and an
//             ACCESS_VIOLATION rather than a comparable divergence — see the
//             note above FreeInnerVariant.
//   NOZERO  — drop the `slot+8 = 0` half of the header clear. Mechanism: the
//             16-byte width of Body A's clear, half of which a raw memcmp of a
//             zero-seeded buffer would never notice.
//
// A control is scored ONLY on the cases where its wrong body actually RUNS.
// Case 1 runs no Adapter or Interface walker, so LEAK and NOSTR are not
// applicable to it and are not counted there; counting them would score a
// control as "not caught" for a reason that has nothing to do with the
// comparator (reference_control_must_pick_a_compared_subject.md).
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"
#include "../collection/array_internal.h"
#include "../collection/array_destroy_internal.h"

// The door that does NOT consult the port registry. Locally declared for the
// reason array.cpp declares ResolveAddr: resolve.h drags in <windows.h>.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// video_resources_win.cpp — a real CRITICAL_SECTION for the synthetic Devices,
// because retail's ~Device calls DeleteCriticalSection on it.
extern "C" void RtVideoProbeInitDeviceLock(void* device);

using RtVideo::Adapter;
using RtVideo::Device;
using RtVideo::DisplayMode;
using RtVideo::GraphicsInterface;

// The three helper bodies live in video_resources.cpp's anonymous namespace, so
// the probe cannot call them directly. It calls them through the two entry
// points that file exposes for exactly this purpose — the same rule
// array_destroy_internal.h states: a probe that re-derived the body would be
// testing a copy of the port rather than the port.
void RtVideoProbe_DestroyAdapterRange(Adapter* base, int count);
void RtVideoProbe_DestroyInterfaceRange(GraphicsInterface* base, int count);
void Video_Device_Dtor(Device* self);
struct ImageBuffer;
void Video_ImageBuffer_Recreate(::ImageBuffer* self);

namespace {

// ---------------------------------------------------------------------------
// Reporting and the effectiveness counters
// ---------------------------------------------------------------------------
int g_cases = 0, g_mismatch = 0;
int g_freed = 0, g_decremented = 0, g_reachedZero = 0, g_survived = 0;

// The negative-control selector. `kNegNone` is a normal run.
enum Neg { kNegNone = 0, kNegLeak, kNegNoStr, kNegShift, kNegNoZero, kNegCOUNT };
const char* const kNegName[kNegCOUNT] = { "none", "LEAK", "NOSTR", "SHIFT", "NOZERO" };
Neg g_neg = kNegNone;
// Per-control catch tally, scored separately. Index by Neg.
int g_negCases[kNegCOUNT] = { 0 }, g_negCaught[kNegCOUNT] = { 0 };

void Fail(const char* caseName, const char* what) {
    ++g_mismatch;
    printf("[VresProbe] MISMATCH  %-28s %s\n", caseName, what);
}

// ---------------------------------------------------------------------------
// THE BYTE-DELTA CHECK — the instrument that can see a LEAK
//
// A memory comparator structurally CANNOT see the defect this lane most needs
// to exclude. Each of the three helpers frees the buffer the level below it
// walked, so the nested writes land in memory that is freed before the
// comparator can read it, and a body that skipped the recursion entirely leaves
// the OUTER buffer byte-identical. The first draft of this probe tried to
// memcmp those nested buffers anyway and produced two confident wrong answers
// plus a crash.
//
// What DOES see it is the allocator's own accounting: `MemoryAllocator::
// _allocCount` (+0x638), the live block count, which `Free` decrements exactly
// once per call. The drop across a body IS the number of blocks that body
// released, and a body that skipped the recursion releases fewer. That is a
// direct measurement of destroy-vs-merely-free rather than an inference.
//
// It is also this file's effectiveness control: a delta of 0 means the case
// released nothing and proves nothing, whatever the memcmp said.
// ---------------------------------------------------------------------------
// It counts BLOCKS, not BYTES, and that correction was measured rather than
// reasoned. The first version read `GetAllocatedSize` (+0x630) and the positive
// run failed by 24 bytes on one case and -16 on another while every byte of
// both element buffers matched. `Free` decrements `_allocatedBytes` by the
// BLOCK's size, which is the size of the free tag the request landed in — and
// that depends on splitting and coalescing at the address it happened to get,
// so two identically-sized requests on two sides need not account for the same
// number of bytes. `_allocCount` (+0x638) is decremented exactly ONCE per
// `Free`, so it is symmetric by construction: it answers "how many blocks did
// this body release", which is the question, and the byte count never did.
//
// Only accepting it because it went green would have been the wrong move; what
// makes the switch legitimate is that the LEAK control still separates cleanly
// under it, and it separates by a LARGER margin than the noise the byte metric
// carried (see the probe's own output).
int32_t LiveBlocks() {
    using GetFn = uint8_t* (*)();
    uint8_t* a = RT_EXPORT_ONCE(GetFn, "?Get@MemoryAllocator@System@@SAPEAV12@XZ")();
    if (!a) return -1;
    return *reinterpret_cast<const int32_t*>(a + 0x638);   // MemoryAllocator::allocCount
}

bool DeltaCheck(const char* caseName, int32_t freedOurs, int32_t freedTheirs) {
    if (freedOurs != freedTheirs) {
        char msg[176];
        std::snprintf(msg, sizeof(msg),
                      "blocks released: ours=%d theirs=%d (difference %d) — "
                      "the two bodies did not free the same set of blocks",
                      freedOurs, freedTheirs, freedTheirs - freedOurs);
        Fail(caseName, msg);
        return false;
    }
    if (freedTheirs <= 0) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "blocks released is %d — this case PROVES NOTHING about freeing",
                      freedTheirs);
        Fail(caseName, msg);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Subject construction
//
// One arena per side, so the two sides' allocations can never alias and a slot
// holding "this side's own block base" is decidable.
// ---------------------------------------------------------------------------
struct Side {
    void*  blocks[64];
    int    nBlocks;
    void*  Alloc(uint64_t n) {
        void* p = rtcoll::MemAlloc(n, 8);
        if (p && nBlocks < 64) blocks[nBlocks++] = p;
        return p;
    }
    bool Owns(const void* p) const {
        for (int i = 0; i < nBlocks; ++i) if (blocks[i] == p) return true;
        return false;
    }
};

// A synthetic System::String: an 8-byte header block whose last dword is the
// refcount, with the "string" pointer sitting 8 bytes in. That is exactly the
// shape Body B addresses — `[str-4]` is the count, `str-8` the block base.
uint8_t* MakeString(Side& s, int32_t refcount, uint8_t tag) {
    uint8_t* blk = static_cast<uint8_t*>(s.Alloc(24));
    if (!blk) return nullptr;
    std::memset(blk, tag, 24);
    *reinterpret_cast<int32_t*>(blk + 4) = refcount;   // lands at str-4
    return blk + 8;
}

// Fill a byte range with a varying non-zero pattern seeded from `tag`.
void Pattern(void* p, size_t n, uint8_t tag) {
    uint8_t* b = static_cast<uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(0x40 + tag + (i * 7));
}

// ---------------------------------------------------------------------------
// Comparators
// ---------------------------------------------------------------------------
// A String slot's four possible post-states, so the two sides can be compared
// without comparing two different allocations' addresses.
enum SlotClass { kSlotNull, kSlotOwnBlock, kSlotUnchanged, kSlotOther };
SlotClass Classify(const Side& s, const void* was, const void* now) {
    if (now == nullptr) return kSlotNull;
    if (now == was) return kSlotUnchanged;
    if (s.Owns(now)) return kSlotOwnBlock;
    return kSlotOther;
}

// Compare `n` bytes of two element buffers, skipping the String slots (which
// are classified instead). `strOffs`/`nStr` name the slots within one element.
bool CompareBuffers(const char* caseName,
                    const uint8_t* ours, const uint8_t* theirs,
                    const uint8_t* oursPre, const uint8_t* theirsPre,
                    int count, uint32_t stride,
                    const uint32_t* strOffs, int nStr,
                    const Side& sOurs, const Side& sTheirs) {
    bool ok = true;
    for (int i = 0; i < count; ++i) {
        const uint32_t base = static_cast<uint32_t>(i) * stride;
        for (uint32_t b = 0; b < stride; ++b) {
            bool inSlot = false;
            for (int k = 0; k < nStr; ++k)
                if (b >= strOffs[k] && b < strOffs[k] + 8) inSlot = true;
            if (inSlot) continue;
            if (ours[base + b] != theirs[base + b]) {
                char msg[160];
                std::snprintf(msg, sizeof(msg),
                              "elem %d byte +0x%x: ours=%02x theirs=%02x",
                              i, b, ours[base + b], theirs[base + b]);
                Fail(caseName, msg);
                ok = false;
                b = stride;   // one report per element is enough
            }
        }
        for (int k = 0; k < nStr; ++k) {
            const uint32_t off = base + strOffs[k];
            const void* wasO = *reinterpret_cast<void* const*>(oursPre + off);
            const void* wasT = *reinterpret_cast<void* const*>(theirsPre + off);
            const SlotClass co = Classify(sOurs,  wasO, *reinterpret_cast<void* const*>(ours + off));
            const SlotClass ct = Classify(sTheirs, wasT, *reinterpret_cast<void* const*>(theirs + off));
            if (co != ct) {
                char msg[160];
                std::snprintf(msg, sizeof(msg),
                              "elem %d String slot +0x%x: ours=class%d theirs=class%d",
                              i, strOffs[k], static_cast<int>(co), static_cast<int>(ct));
                Fail(caseName, msg);
                ok = false;
            }
            if (co == kSlotUnchanged || ct == kSlotUnchanged) {
                // A surviving string: the refcounts must match as NUMBERS.
                const int32_t ro = *reinterpret_cast<const int32_t*>(
                    static_cast<const uint8_t*>(wasO) - 4);
                const int32_t rt = *reinterpret_cast<const int32_t*>(
                    static_cast<const uint8_t*>(wasT) - 4);
                if (ro != rt) {
                    char msg[160];
                    std::snprintf(msg, sizeof(msg),
                                  "elem %d String +0x%x refcount: ours=%d theirs=%d",
                                  i, strOffs[k], ro, rt);
                    Fail(caseName, msg);
                    ok = false;
                }
            }
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
// The deliberately wrong bodies. They go through the SAME comparator, and each
// one exercises the mechanism it is named for.
// ---------------------------------------------------------------------------
constexpr rtcoll::DestroySpec kN_ModeRefresh =
    { "neg mode refresh", 0x018, 1, { 0x008 } };
constexpr rtcoll::DestroySpec kN_AdapterMulti =
    { "neg adapter multi", 0x038, 1, { 0x018 } };
constexpr rtcoll::DestroySpec kN_AdapterModes =
    { "neg adapter modes", 0x038, 1, { 0x008 } };
constexpr rtcoll::DestroySpec kN_AdapterName =
    { "neg adapter name",  0x038, 1, { 0x000 } };
constexpr rtcoll::DestroySpec kN_IfaceAdapters =
    { "neg iface adapters", 0x018, 1, { 0x008 } };
constexpr rtcoll::DestroySpec kN_IfaceName =
    { "neg iface name",     0x018, 1, { 0x000 } };
// A CONTROL MUST PRODUCE A COMPARABLE DIVERGENCE, NOT UB — and this lane's
// first attempt did not, which is why the third control is spelled this way.
//
// It originally walked Adapters at stride 0x18 instead of 0x38. It was "caught"
// only in the sense that the process died: reading a 0x38-strided buffer at
// 0x18 hands the allocator a pointer made of the probe's own seed pattern, and
// MemoryAllocator::Free faulted on it (Runtime-x64.dll+0xC6C5F, READ of
// 0x7ffffffc). A control that crashes proves nothing about the COMPARATOR — the
// run never reaches it — and it took the remaining controls down with it.
//
// SHIFT exercises the same mechanism (a mis-transcribed offset in Body A)
// without a wild free: the frees are CORRECT and only the 16-byte header clear
// lands eight bytes EARLY. Early, not late — see the note in the body; the
// forward spelling ran off the end of the element buffer and corrupted the
// heap, which is the same defect class as the stride control it replaced.
enum ABad { kABadNone, kABadNoZero, kABadShift };

void FreeInnerVariant(uint8_t* base, int count, const rtcoll::DestroySpec* s, ABad bad) {
    if (bad == kABadNone) { rtcoll::FreeInnerArraysCore(base, count, s); return; }
    if (count <= 0) return;
    for (int64_t i = 0; i < count; ++i) {
        uint8_t* slot = base + i * s->stride + s->offs[0];
        void* p = *reinterpret_cast<void**>(slot);
        if (p) rtcoll::MemFree(p);                  // the free is always correct
        if (bad == kABadNoZero) {
            *reinterpret_cast<uint64_t*>(slot) = 0;
            // the `slot + 8 = 0` store is DELIBERATELY MISSING
        } else {                                    // kABadShift
            // BACKWARD by 8, not forward. The forward spelling overran: for the
            // two 0x18-stride specs, offs 0x08 + 8 writes 0x10..0x1f, and 0x18
            // is the END of the element — so the last element's clear ran eight
            // bytes past the buffer and corrupted the allocator's next tag. The
            // probe caught that too, as a WRITE fault inside MemAlloc two cases
            // later (Runtime-x64.dll+0xC74C3), which is a second instance of
            // this control's own rule: a control must diverge, not corrupt.
            // Every spec here has offs >= 0x08, so `slot - 8` is never before
            // the element base.
            *reinterpret_cast<uint64_t*>(slot - 8) = 0;
            *reinterpret_cast<uint64_t*>(slot)     = 0;
        }
    }
}

ABad ABadMode() {
    if (g_neg == kNegNoZero) return kABadNoZero;
    if (g_neg == kNegShift)  return kABadShift;
    return kABadNone;
}

void NegAdapterRange(Adapter* base, int count) {
    uint8_t* p = reinterpret_cast<uint8_t*>(base);
    const ABad bad = ABadMode();
    for (int i = 0; i < count; ++i) {
        uint8_t* e = p + static_cast<uint32_t>(i) * kN_AdapterMulti.stride;
        FreeInnerVariant(e, 1, &kN_AdapterMulti, bad);
        Adapter* a = reinterpret_cast<Adapter*>(e);
        if (g_neg != kNegLeak)
            FreeInnerVariant(reinterpret_cast<uint8_t*>(a->modeData),
                             a->modeCount, &kN_ModeRefresh, bad);
        FreeInnerVariant(e, 1, &kN_AdapterModes, bad);
        if (g_neg != kNegNoStr) rtcoll::ReleaseStringsCore(e, 1, &kN_AdapterName);
    }
}

void NegInterfaceRange(GraphicsInterface* base, int count) {
    uint8_t* p = reinterpret_cast<uint8_t*>(base);
    for (int i = 0; i < count; ++i) {
        uint8_t* e = p + static_cast<uint32_t>(i) * 0x18u;
        GraphicsInterface* g = reinterpret_cast<GraphicsInterface*>(e);
        if (g_neg != kNegLeak) NegAdapterRange(g->adapterData, g->adapterCount);
        FreeInnerVariant(e, 1, &kN_IfaceAdapters, ABadMode());
        if (g_neg != kNegNoStr) rtcoll::ReleaseStringsCore(e, 1, &kN_IfaceName);
    }
}

// ---------------------------------------------------------------------------
// Subject builders. `Build*` fills ONE side's buffer and returns it.
// ---------------------------------------------------------------------------
const int kModes = 3, kAdapters = 2, kIfaces = 2;

DisplayMode* BuildModes(Side& s, uint8_t tag, int n) {
    DisplayMode* m = static_cast<DisplayMode*>(s.Alloc(sizeof(DisplayMode) * n));
    Pattern(m, sizeof(DisplayMode) * n, tag);
    for (int i = 0; i < n; ++i) {
        m[i].width = 640 + i;
        m[i].height = 480 + i;
        // Element 1 gets a NULL refresh buffer on purpose: Body A's header
        // clear is unconditional, including when the buffer was already null,
        // and that arm is otherwise never driven.
        if (i == 1) { m[i].refreshData = nullptr; m[i].refreshCount = 0; m[i].refreshCapacity = 0; }
        else {
            m[i].refreshData = static_cast<RtVideo::RefreshRate*>(s.Alloc(16));
            Pattern(m[i].refreshData, 16, static_cast<uint8_t>(tag + i));
            m[i].refreshCount = 2;
            m[i].refreshCapacity = 2;
        }
    }
    return m;
}

Adapter* BuildAdapters(Side& s, uint8_t tag, int n) {
    Adapter* a = static_cast<Adapter*>(s.Alloc(sizeof(Adapter) * n));
    Pattern(a, sizeof(Adapter) * n, tag);
    for (int i = 0; i < n; ++i) {
        // Adapter 0's name has refcount 1 (it must be FREED); adapter 1's has
        // refcount 2 (it must SURVIVE with the count decremented). Both arms of
        // Body B are driven, which is the asymmetry a raw memcmp cannot see.
        a[i].name = MakeString(s, i == 0 ? 1 : 2, static_cast<uint8_t>(tag + 0x10 + i));
        a[i].modeData = BuildModes(s, static_cast<uint8_t>(tag + 0x20 + i), kModes);
        a[i].modeCount = kModes;
        a[i].modeCapacity = kModes;
        a[i].multisampleData = static_cast<uint8_t*>(s.Alloc(8));
        Pattern(a[i].multisampleData, 8, static_cast<uint8_t>(tag + 0x30 + i));
        a[i].multisampleCount = 4;
        a[i].multisampleCap = 8;
    }
    return a;
}

GraphicsInterface* BuildIfaces(Side& s, uint8_t tag, int n) {
    GraphicsInterface* g = static_cast<GraphicsInterface*>(s.Alloc(sizeof(GraphicsInterface) * n));
    Pattern(g, sizeof(GraphicsInterface) * n, tag);
    for (int i = 0; i < n; ++i) {
        g[i].name = MakeString(s, i == 0 ? 1 : 2, static_cast<uint8_t>(tag + 0x50 + i));
        g[i].adapterData = BuildAdapters(s, static_cast<uint8_t>(tag + 0x60 + i), kAdapters);
        g[i].adapterCount = kAdapters;
        g[i].adapterCapacity = kAdapters;
    }
    return g;
}

// Count the work a case did, from the PRE-state — after the bodies run the
// slots are the answer and a second pass could not tell what the first one did.
void CountWork(const uint8_t* pre, int count, uint32_t stride,
               const uint32_t* innerOffs, int nInner,
               const uint32_t* strOffs, int nStr) {
    for (int i = 0; i < count; ++i) {
        const uint32_t base = static_cast<uint32_t>(i) * stride;
        for (int k = 0; k < nInner; ++k)
            if (*reinterpret_cast<void* const*>(pre + base + innerOffs[k])) ++g_freed;
        for (int k = 0; k < nStr; ++k) {
            const uint8_t* str = *reinterpret_cast<const uint8_t* const*>(pre + base + strOffs[k]);
            if (!str) continue;
            ++g_decremented;
            if (*reinterpret_cast<const int32_t*>(str - 4) == 1) ++g_reachedZero;
            else ++g_survived;
        }
    }
}

// ---------------------------------------------------------------------------
// Case 1 — 0x180010cf0, the per-DisplayMode helper
// ---------------------------------------------------------------------------
using RangeFn = void (*)(void* ignoredThis, void* base, int count);

void CaseModes() {
    ++g_cases;
    Side so{}, st{};
    DisplayMode* ours   = BuildModes(so, 0x11, kModes);
    DisplayMode* theirs = BuildModes(st, 0x11, kModes);
    const size_t n = sizeof(DisplayMode) * kModes;
    uint8_t preO[sizeof(DisplayMode) * kModes], preT[sizeof(DisplayMode) * kModes];
    std::memcpy(preO, ours, n);
    std::memcpy(preT, theirs, n);

    const uint32_t inner[1] = { offsetof(DisplayMode, refreshData) };
    CountWork(preO, kModes, sizeof(DisplayMode), inner, 1, nullptr, 0);

    // OURS. The DisplayMode walk is not a named entry point in
    // video_resources.cpp — ??1Device calls FreeInnerArraysCore with that spec
    // inline — so the probe drives the SAME core with the SAME spec shape.
    constexpr rtcoll::DestroySpec spec =
        { "DisplayMode: inner Array<RefreshRate> @+0x008, stride 0x018", 0x018, 1, { 0x008 } };
    const int32_t o0 = LiveBlocks();
    FreeInnerVariant(reinterpret_cast<uint8_t*>(ours), kModes, &spec, ABadMode());
    const int32_t freedOurs = o0 - LiveBlocks();

    RangeFn shipped = reinterpret_cast<RangeFn>(ResolveRuntimeRaw(0x180010cf0));
    if (!shipped) { Fail("case1/DisplayMode", "shipped 0x180010cf0 did not resolve"); return; }
    const int32_t t0 = LiveBlocks();
    shipped(nullptr, theirs, kModes);
    const int32_t freedTheirs = t0 - LiveBlocks();

    bool eq = CompareBuffers("case1/DisplayMode",
                                   reinterpret_cast<uint8_t*>(ours),
                                   reinterpret_cast<uint8_t*>(theirs),
                              preO, preT, kModes, sizeof(DisplayMode),
                              nullptr, 0, so, st);
    eq = DeltaCheck("case1/DisplayMode", freedOurs, freedTheirs) && eq;
    // Only NOZERO is APPLICABLE to this case: the other three controls
    // corrupt the Adapter/Interface walkers, which this case does not run, so
    // scoring them here would count a case in which the control's wrong body
    // was never executed as "not caught"
    // (reference_control_must_pick_a_compared_subject.md).
    if (g_neg == kNegNoZero || g_neg == kNegShift) {
        ++g_negCases[g_neg]; if (!eq) ++g_negCaught[g_neg];
    }
    rtcoll::MemFree(ours);
    rtcoll::MemFree(theirs);
}

// ---------------------------------------------------------------------------
// Case 2 — 0x180010da0, the per-Adapter helper (one level of nesting)
// ---------------------------------------------------------------------------
void CaseAdapters() {
    ++g_cases;
    Side so{}, st{};
    Adapter* ours   = BuildAdapters(so, 0x21, kAdapters);
    Adapter* theirs = BuildAdapters(st, 0x21, kAdapters);
    const size_t n = sizeof(Adapter) * kAdapters;
    uint8_t preO[sizeof(Adapter) * kAdapters], preT[sizeof(Adapter) * kAdapters];
    std::memcpy(preO, ours, n);
    std::memcpy(preT, theirs, n);

    const uint32_t inner[2] = { offsetof(Adapter, multisampleData), offsetof(Adapter, modeData) };
    const uint32_t strs[1]  = { offsetof(Adapter, name) };
    CountWork(preO, kAdapters, sizeof(Adapter), inner, 2, strs, 1);
    for (int i = 0; i < kAdapters; ++i) {
        const uint32_t modeInner[1] = { offsetof(DisplayMode, refreshData) };
        CountWork(reinterpret_cast<const uint8_t*>(
                      reinterpret_cast<const Adapter*>(preO)[i].modeData),
                  kModes, sizeof(DisplayMode), modeInner, 1, nullptr, 0);
    }

    const int32_t o0 = LiveBlocks();
    if (g_neg == kNegNone) RtVideoProbe_DestroyAdapterRange(ours, kAdapters);
    else                   NegAdapterRange(ours, kAdapters);
    const int32_t freedOurs = o0 - LiveBlocks();

    RangeFn shipped = reinterpret_cast<RangeFn>(ResolveRuntimeRaw(0x180010da0));
    if (!shipped) { Fail("case2/Adapter", "shipped 0x180010da0 did not resolve"); return; }
    const int32_t t0 = LiveBlocks();
    shipped(nullptr, theirs, kAdapters);
    const int32_t freedTheirs = t0 - LiveBlocks();

    bool eq = CompareBuffers("case2/Adapter",
                             reinterpret_cast<uint8_t*>(ours),
                             reinterpret_cast<uint8_t*>(theirs),
                             preO, preT, kAdapters, sizeof(Adapter),
                             strs, 1, so, st);
    // *** The nested DisplayMode buffers are NOT comparable here. *** This
    // lane's first draft memcmp'd them, on the reasoning that only their
    // refresh buffers were freed. That reasoning is WRONG and the probe proved
    // it on its own first run: 0x180010da0 frees `modeData` ITSELF at step 3,
    // so both sides' DisplayMode buffers are freed memory by the time the
    // comparator sees them, the allocator has written per-side free-list links
    // into them, and the compare reported two confident MISMATCHes that were
    // artefacts. It then dereferenced a free-list link as a String and took the
    // process down inside CompareBuffers. The nested walk is covered by
    // CASE 1, which drives 0x180010cf0 directly on a buffer nothing frees, and
    // by the byte-delta check below.
    eq = eq && DeltaCheck("case2/Adapter", freedOurs, freedTheirs);
    if (g_neg != kNegNone) { ++g_negCases[g_neg]; if (!eq) ++g_negCaught[g_neg]; }
    rtcoll::MemFree(ours);
    rtcoll::MemFree(theirs);
}

// ---------------------------------------------------------------------------
// Case 3 — 0x180010f00, the per-GraphicsInterface helper (two levels)
// ---------------------------------------------------------------------------
void CaseInterfaces() {
    ++g_cases;
    Side so{}, st{};
    GraphicsInterface* ours   = BuildIfaces(so, 0x31, kIfaces);
    GraphicsInterface* theirs = BuildIfaces(st, 0x31, kIfaces);
    const size_t n = sizeof(GraphicsInterface) * kIfaces;
    uint8_t preO[sizeof(GraphicsInterface) * kIfaces], preT[sizeof(GraphicsInterface) * kIfaces];
    std::memcpy(preO, ours, n);
    std::memcpy(preT, theirs, n);

    const uint32_t inner[1] = { offsetof(GraphicsInterface, adapterData) };
    const uint32_t strs[1]  = { offsetof(GraphicsInterface, name) };
    CountWork(preO, kIfaces, sizeof(GraphicsInterface), inner, 1, strs, 1);

    const int32_t o0 = LiveBlocks();
    if (g_neg == kNegNone) RtVideoProbe_DestroyInterfaceRange(ours, kIfaces);
    else                   NegInterfaceRange(ours, kIfaces);
    const int32_t freedOurs = o0 - LiveBlocks();

    RangeFn shipped = reinterpret_cast<RangeFn>(ResolveRuntimeRaw(0x180010f00));
    if (!shipped) { Fail("case3/Interface", "shipped 0x180010f00 did not resolve"); return; }
    const int32_t t0 = LiveBlocks();
    shipped(nullptr, theirs, kIfaces);
    const int32_t freedTheirs = t0 - LiveBlocks();

    bool eq = CompareBuffers("case3/Interface",
                             reinterpret_cast<uint8_t*>(ours),
                             reinterpret_cast<uint8_t*>(theirs),
                             preO, preT, kIfaces, sizeof(GraphicsInterface),
                             strs, 1, so, st);
    // Same correction as case 2, one level up: 0x180010f00 frees `adapterData`
    // itself, so the nested Adapter buffer is freed memory here too and cannot
    // be memcmp'd. The nesting is covered by case 2 (which drives 0x180010da0
    // on a buffer nothing frees) and by the byte delta.
    eq = eq && DeltaCheck("case3/Interface", freedOurs, freedTheirs);
    if (g_neg != kNegNone) { ++g_negCases[g_neg]; if (!eq) ++g_negCaught[g_neg]; }
    rtcoll::MemFree(ours);
    rtcoll::MemFree(theirs);
}

// ---------------------------------------------------------------------------
// Cases 4 and 5 — ??1Device@Video@@ end to end, empty arrays
// ---------------------------------------------------------------------------
using DtorFn = void (*)(void*);

void CaseDevice(int type, const char* caseName) {
    ++g_cases;
    uint8_t ours[sizeof(Device)], theirs[sizeof(Device)];
    Pattern(ours, sizeof(Device), 0x41);
    std::memcpy(theirs, ours, sizeof(Device));

    // Empty arrays on both sides. `_data` null means the outer body frees
    // nothing, so the whole object stays comparable.
    for (size_t off : { offsetof(Device, modeData),
                        offsetof(Device, multisampleData),
                        offsetof(Device, interfaceData) }) {
        std::memset(ours + off, 0, 16);
        std::memset(theirs + off, 0, 16);
    }
    *reinterpret_cast<int32_t*>(ours + offsetof(Device, type)) = type;
    *reinterpret_cast<int32_t*>(theirs + offsetof(Device, type)) = type;
    // deviceData null: the slot-0 arm is not driven, and case 4/5 say so.
    *reinterpret_cast<void**>(ours + offsetof(Device, deviceData)) = nullptr;
    *reinterpret_cast<void**>(theirs + offsetof(Device, deviceData)) = nullptr;
    RtVideoProbeInitDeviceLock(ours);
    RtVideoProbeInitDeviceLock(theirs);

    // The singleton slot. Both bodies write it; save and restore around the
    // case. Ours goes through RT_DATA_SLOT, retail through the absolute
    // address — assert they are the SAME location before trusting the compare.
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* saved = slot ? *slot : nullptr;
    const uintptr_t rawSlot = ResolveRuntimeRaw(0x1801fe7a8);
    if (!slot || reinterpret_cast<uintptr_t>(slot) != rawSlot) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "singleton slot mismatch: RT_DATA_SLOT=%p raw=%p — the compare below "
                      "would not be about the same storage",
                      static_cast<void*>(slot), reinterpret_cast<void*>(rawSlot));
        Fail(caseName, msg);
        return;
    }

    void* sentinel = reinterpret_cast<void*>(static_cast<uintptr_t>(0xD00DF00Du));
    *slot = sentinel;
    Video_Device_Dtor(reinterpret_cast<Device*>(ours));
    void* afterOurs = *slot;

    *slot = sentinel;
    DtorFn shipped = reinterpret_cast<DtorFn>(ResolveRuntimeRaw(0x1800f7290));
    if (!shipped) { Fail(caseName, "shipped 0x1800f7290 did not resolve"); *slot = saved; return; }
    shipped(theirs);
    void* afterTheirs = *slot;
    *slot = saved;

    bool eq = true;
    if (afterOurs != afterTheirs) {
        char msg[160];
        std::snprintf(msg, sizeof(msg), "singleton after: ours=%p theirs=%p (type=%d)",
                      afterOurs, afterTheirs, type);
        Fail(caseName, msg);
        eq = false;
    }
    // The whole Device except the CRITICAL_SECTION block, which
    // DeleteCriticalSection leaves per-object bookkeeping in.
    const size_t lockLo = offsetof(Device, lock), lockHi = lockLo + 0x30;
    for (size_t b = 0; b < sizeof(Device); ++b) {
        if (b >= lockLo && b < lockHi) continue;
        if (ours[b] != theirs[b]) {
            char msg[128];
            std::snprintf(msg, sizeof(msg), "Device byte +0x%zx: ours=%02x theirs=%02x",
                          b, ours[b], theirs[b]);
            Fail(caseName, msg);
            eq = false;
            break;
        }
    }
    (void)eq;
}


// ---------------------------------------------------------------------------
// Cases 6-8 — ?Recreate@ImageBuffer@Video@@, one per flag arm
//
// THIS ROW HAS NO GAMEPLAY ROUTE AND THAT IS NOT THE SAME AS NO ORACLE
// (reference_no_route_is_not_no_oracle.md). Its only caller in the tree is
// runtime/src/save/saveload_update.cpp's OnVideoRestart — device-lost recovery,
// which nothing in the harness drives, so a live run reports calls=0 forever.
//
// So the subject is synthesised. The body's whole observable content is:
//   * WHICH DeviceData vtable slot it asks for the new companion (slot 10),
//   * that it stores the result at ImageBuffer+0x60,
//   * and WHICH companion slot it then calls — 1, 2 or 3 — decided by flag
//     bits 4 and 5, with slot 3 taking a second argument of ZERO.
// A fake DeviceData and a fake companion, both with recording stubs, make all
// of that comparable: run ours on one ImageBuffer, the shipped body on a second
// identical one, and compare the trace plus the stored pointer. A wrong vtable
// INDEX — the single most likely transcription error in a body of this shape,
// and one a byte comparison of the ImageBuffer could never see — shows up as a
// different slot in the trace.
//
// The DEVICE SINGLETON is redirected at a fake Device for the duration and
// restored afterwards, the same save/restore cases 4-5 use. Retail reaches it
// through the absolute address and we through RT_DATA_SLOT; cases 4-5 already
// assert those are the same storage, so this case inherits that check.
// ---------------------------------------------------------------------------
struct FakeTrace { int slot; void* self; void* arg2; int calls; };
FakeTrace g_trace;

void FakeSlot1(void* self) { g_trace.slot = 1; g_trace.self = self; g_trace.arg2 = nullptr; ++g_trace.calls; }
void FakeSlot2(void* self) { g_trace.slot = 2; g_trace.self = self; g_trace.arg2 = nullptr; ++g_trace.calls; }
void FakeSlot3(void* self, void* a2) { g_trace.slot = 3; g_trace.self = self; g_trace.arg2 = a2; ++g_trace.calls; }

// The fake companion: a vtable pointer followed by nothing. Slots 1..3 are the
// recorders; slot 0 is never reached by Recreate.
void*  g_companionVt[4] = { nullptr, (void*)&FakeSlot1, (void*)&FakeSlot2, (void*)&FakeSlot3 };
void*  g_companion[2]   = { (void*)g_companionVt, nullptr };

int  g_createCalls = 0;
void* g_createArg  = nullptr;
void* FakeCreateCompanion(void* dd, void* imageBuffer) {
    (void)dd;
    ++g_createCalls;
    g_createArg = imageBuffer;
    return (void*)g_companion;
}
// Eleven slots so index 10 (the `CALL [RAX+0x50]` Recreate makes) is in range.
void* g_deviceDataVt[11] = { nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr, nullptr,
                             (void*)&FakeCreateCompanion };
void* g_deviceData[2] = { (void*)g_deviceDataVt, nullptr };

using RecreateFn = void (*)(void*);

void CaseRecreate(uint32_t flags, int wantSlot, const char* caseName) {
    ++g_cases;
    // A fake Device: only +0x100 is read, but give it the real size so a stray
    // read lands in our own storage rather than off the end.
    static uint8_t fakeDevice[sizeof(Device)];
    std::memset(fakeDevice, 0, sizeof(fakeDevice));
    *reinterpret_cast<void**>(fakeDevice + offsetof(Device, deviceData)) = (void*)g_deviceData;

    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    if (!slot) { Fail(caseName, "device singleton slot did not resolve"); return; }
    void* saved = *slot;
    *slot = fakeDevice;

    // Two ImageBuffers, seeded identically with a non-zero pattern so the
    // +0x60 store is a CHANGE and not an accident of a zeroed subject.
    uint8_t ours[0x68], theirs[0x68];
    Pattern(ours, sizeof(ours), 0x51);
    std::memcpy(theirs, ours, sizeof(ours));
    *reinterpret_cast<uint32_t*>(ours   + 0x34) = flags;
    *reinterpret_cast<uint32_t*>(theirs + 0x34) = flags;

    g_trace = FakeTrace{}; g_createCalls = 0; g_createArg = nullptr;
    Video_ImageBuffer_Recreate(reinterpret_cast<::ImageBuffer*>(ours));
    const FakeTrace tOurs = g_trace;
    const int createOurs = g_createCalls;
    void* const createArgOurs = g_createArg;
    void* const storedOurs = *reinterpret_cast<void**>(ours + 0x60);

    RecreateFn shipped = reinterpret_cast<RecreateFn>(ResolveRuntimeRaw(0x1800f33f0));
    if (!shipped) { Fail(caseName, "shipped 0x1800f33f0 did not resolve"); *slot = saved; return; }
    g_trace = FakeTrace{}; g_createCalls = 0; g_createArg = nullptr;
    shipped(theirs);
    const FakeTrace tTheirs = g_trace;
    const int createTheirs = g_createCalls;
    void* const createArgTheirs = g_createArg;
    void* const storedTheirs = *reinterpret_cast<void**>(theirs + 0x60);
    *slot = saved;

    char msg[220];
    if (tOurs.slot != tTheirs.slot || tOurs.calls != tTheirs.calls ||
        tOurs.arg2 != tTheirs.arg2) {
        std::snprintf(msg, sizeof(msg),
                      "companion slot: ours=%d(calls=%d arg2=%p) theirs=%d(calls=%d arg2=%p)",
                      tOurs.slot, tOurs.calls, tOurs.arg2,
                      tTheirs.slot, tTheirs.calls, tTheirs.arg2);
        Fail(caseName, msg);
    }
    if (createOurs != createTheirs || createArgOurs != ours || createArgTheirs != theirs) {
        std::snprintf(msg, sizeof(msg),
                      "DeviceData slot 10: ours calls=%d arg=%p (want %p), "
                      "theirs calls=%d arg=%p (want %p)",
                      createOurs, createArgOurs, static_cast<void*>(ours),
                      createTheirs, createArgTheirs, static_cast<void*>(theirs));
        Fail(caseName, msg);
    }
    if (storedOurs != (void*)g_companion || storedTheirs != (void*)g_companion) {
        std::snprintf(msg, sizeof(msg), "+0x60 store: ours=%p theirs=%p want=%p",
                      storedOurs, storedTheirs, static_cast<void*>(g_companion));
        Fail(caseName, msg);
    }
    // Everything else in the ImageBuffer must be untouched, and identical.
    for (size_t b = 0; b < sizeof(ours); ++b) {
        if (b >= 0x60 && b < 0x68) continue;          // the one field it writes
        if (ours[b] != theirs[b]) {
            std::snprintf(msg, sizeof(msg), "ImageBuffer byte +0x%zx: ours=%02x theirs=%02x",
                          b, ours[b], theirs[b]);
            Fail(caseName, msg);
            break;
        }
    }
    // EFFECTIVENESS: the arm under test must be the arm that ran, on BOTH
    // sides. A case that silently exercised a different branch would otherwise
    // agree with itself and prove nothing about this one.
    if (tTheirs.slot != wantSlot) {
        std::snprintf(msg, sizeof(msg),
                      "the SHIPPED body took slot %d, not the slot %d this case is "
                      "about — the case did not test what it says",
                      tTheirs.slot, wantSlot);
        Fail(caseName, msg);
    }
}

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------
bool g_armed = false, g_ran = false;

void RunOne() {
    g_cases = g_mismatch = 0;
    g_freed = g_decremented = g_reachedZero = g_survived = 0;
    CaseModes();
    CaseAdapters();
    CaseInterfaces();
    if (g_neg == kNegNone) {
        // The end-to-end Device cases are NOT run under a negative control: the
        // wrong bodies above are element walkers, and running them against a
        // subject with empty arrays would exercise nothing, which would score a
        // control as "not caught" for a reason that has nothing to do with the
        // comparator (reference_control_must_pick_a_compared_subject.md).
        CaseDevice(1, "case4/Device type=1");
        CaseDevice(0, "case5/Device type=0");
        // The three Recreate arms. Bit 4 wins over bit 5, so case 8 sets BOTH
        // to prove the precedence rather than assuming it.
        CaseRecreate(0x00000000u, 3, "case6/Recreate no bits -> slot 3");
        CaseRecreate(0x00000020u, 2, "case7/Recreate bit 5 -> slot 2");
        CaseRecreate(0x00000030u, 1, "case8/Recreate bits 4+5 -> slot 1");
    }
    printf("[VresProbe] mode=%-6s cases=%d mismatches=%d | work: freed=%d "
                "refcount-- =%d reached0=%d survived=%d\n",
                kNegName[g_neg], g_cases, g_mismatch,
                g_freed, g_decremented, g_reachedZero, g_survived);
    if (g_freed == 0 && g_decremented == 0)
        printf("[VresProbe] *** THIS RUN PROVES NOTHING: no block was freed and no "
                    "refcount was decremented. ***\n");
}

}  // namespace

void RtVideoResourcesProbeArm(bool on) { g_armed = on; }

void RtVideoResourcesProbeRun() {
    if (!g_armed || g_ran) return;
    g_ran = true;

    const char* mode = std::getenv("BANISHED_RT_VRES_PROBE");
    const bool wantNeg = mode && std::strcmp(mode, "neg") == 0;

    printf("[VresProbe] ==== Wave 18 lane VRES synthetic-subject replay ====\n");
    g_neg = kNegNone;
    RunOne();
    const int posMismatch = g_mismatch;

    if (wantNeg) {
        for (int k = kNegLeak; k < kNegCOUNT; ++k) {
            g_neg = static_cast<Neg>(k);
            RunOne();
        }
        g_neg = kNegNone;
        printf("[VresProbe] ---- negative controls, scored SEPARATELY ----\n");
        bool allCaught = true;
        for (int k = kNegLeak; k < kNegCOUNT; ++k) {
            const bool ok = g_negCases[k] > 0 && g_negCaught[k] == g_negCases[k];
            if (!ok) allCaught = false;
            printf("[VresProbe]   %-7s caught %d of %d cases  %s\n",
                        kNegName[k], g_negCaught[k], g_negCases[k],
                        ok ? "GREEN" : "*** NOT CAUGHT — the comparator is blind here ***");
        }
        if (!allCaught)
            printf("[VresProbe] *** A control was missed: the clean result above is "
                        "NOT evidence. ***\n");
    }
    printf("[VresProbe] VERDICT: %s (positive run mismatches=%d)\n",
                posMismatch == 0 ? "ours matches the shipped bodies" : "DIVERGENCE",
                posMismatch);
}
