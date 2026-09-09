// ============================================================================
// runtime/src/io/memory_stream.cpp — Wave 9 lane PKG2, rung 4b-ii
// ============================================================================
//
// IO::MemoryStream's six virtual bodies: Write, Read, SetCurrentPos,
// GetCurrentPos, GetSize and Close.
//
// WHY THESE, AND WHY FIRST
//
// `CompressedStream::Open` — 4b's actual target — reads its header and block
// directory THROUGH a Stream, and in `ReadPackfile` that Stream is a
// MemoryStream over the freshly-read file header. So `Open` cannot be diffed at
// all until the stream it reads through is ours on both sides: an oracle that
// runs ours and theirs side by side would otherwise have one of them advancing
// a cursor the other cannot see. That is sequencing by what the ORACLE needs
// rather than by what the call graph shows, and it is the same reasoning the
// whole bottom-up decomposition rests on.
//
// THE LAYOUT WAS ALREADY IN THE TREE. runtime/types/io/Stream.h carries the
// complete, disassembly-pinned MemoryStream — _data +0x18, _ownsData +0x20,
// _cursor +0x24, _size +0x28, stride 0x30 — and the full vtable including
// `[7] 0x1800ae9e0 Close <-- +0x38`. Not one byte of it needed re-deriving.
// This file exists partly as evidence for the rule 4b-i arrived at: grep the
// types header before opening the PE.
//
// All six mangled names were checked against the shipped export table before a
// line was written (reference_resolveexport_phantom_names). Two of them are
// spelled `UEBAI` — const, returning UNSIGNED int — which is why the two
// getters below return uint32_t and not int32_t.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"

// NO INLINE REPLAY IN THESE BODIES, deliberately. RT_DIFF_SCALAR works by
// running the shipped body a second time on the same object and comparing the
// two answers -- which is exactly wrong for four of the six here, because Read,
// Write and SetCurrentPos MUTATE _cursor: a replay would advance it twice and
// manufacture a divergence in a correct port. So the bodies only declare that
// they did work, and the verification lives in memory_stream_probe.cpp, which
// gives each side its OWN stream and steps them in lockstep. The two const
// getters could safely replay, but they are covered by the same probe and are
// left uniform with their siblings rather than opening a raw door for two
// two-instruction functions.
namespace RtIo {

// ============================================================================
// ?Read@MemoryStream@IO@@UEAA_NHPEAX@Z                       retail 0x1800ae950
// ============================================================================
// THE BOUND CHECK IS 32-BIT AND SIGNED, and that is not an accident of the
// decompiler's rendering. Retail sign-extends both the cursor and the count
// into 64-bit registers (`MOVSXD RCX,[RCX+0x24]` / `MOVSXD RDI,EDX`) and then
// deliberately narrows again for the test: `LEA EAX,[RCX + RDI*1]` is a 32-BIT
// add, and `CMP EAX,[RBX+0x28]; JG` is a 32-BIT SIGNED compare. Writing the
// check in 64-bit — the obvious "cleanup" — changes behaviour for a cursor+count
// that overflows int32, because retail's sum WRAPS and then compares small.
// The offset arithmetic that follows really is 64-bit, so the two widths are
// transcribed separately rather than unified.
extern "C" bool RtIo_MemoryStream_Read(MemoryStream* self, int32_t count, void* dst) {
    RT_DIFF_BEGIN("IO::MemoryStream::Read");

    const int64_t cursor = static_cast<int32_t>(self->cursor);   // 1800ae95d MOVSXD
    const int64_t n      = count;                                // 1800ae961 MOVSXD
    const int32_t end    = static_cast<int32_t>(cursor) +
                           static_cast<int32_t>(n);              // 1800ae964 LEA (32-bit)

    if (end > static_cast<int32_t>(self->size)) {                // 1800ae96a CMP/JG
        RT_DIFF_WORK(true);
        return false;                                            // 1800ae991
    }
    std::memcpy(dst, static_cast<const uint8_t*>(self->data) + cursor,
                static_cast<size_t>(n));                         // 1800ae97c
    self->cursor += static_cast<uint32_t>(n);                    // 1800ae981
    RT_DIFF_WORK(true);
    return true;
}

// ============================================================================
// ?Write@MemoryStream@IO@@UEAA_NHPEBX@Z                      retail 0x1800ae8f0
// ============================================================================
// Byte-for-byte the same body as Read with the memcpy operands swapped —
// including the same 32-bit signed bound check, and including the fact that it
// will NOT grow the buffer: a write past _size fails rather than reallocating.
extern "C" bool RtIo_MemoryStream_Write(MemoryStream* self, int32_t count, const void* src) {
    RT_DIFF_BEGIN("IO::MemoryStream::Write");

    const int64_t cursor = static_cast<int32_t>(self->cursor);   // 1800ae8fd
    const int64_t n      = count;                                // 1800ae901
    const int32_t end    = static_cast<int32_t>(cursor) +
                           static_cast<int32_t>(n);              // 1800ae904

    if (end > static_cast<int32_t>(self->size)) {                // 1800ae90a
        RT_DIFF_WORK(true);
        return false;                                            // 1800ae92e
    }
    std::memcpy(static_cast<uint8_t*>(self->data) + cursor, src,
                static_cast<size_t>(n));                         // 1800ae919
    self->cursor += static_cast<uint32_t>(n);                    // 1800ae91e
    RT_DIFF_WORK(true);
    return true;
}

// ============================================================================
// ?SetCurrentPos@MemoryStream@IO@@UEAA_NH@Z                  retail 0x1800ae9b0
// ============================================================================
// Three instructions, and the notable thing is what is ABSENT: there is NO
// bounds check. Retail stores whatever it is given and returns true, so a seek
// past the end succeeds here and the following Read is what fails. A port that
// "helpfully" validated would change which of the two calls reports the error.
extern "C" bool RtIo_MemoryStream_SetCurrentPos(MemoryStream* self, int32_t pos) {
    RT_DIFF_BEGIN("IO::MemoryStream::SetCurrentPos");
    self->cursor = static_cast<uint32_t>(pos);   // 1800ae9b0
    RT_DIFF_WORK(true);
    return true;                                 // 1800ae9b3
}

// ============================================================================
// ?GetCurrentPos@MemoryStream@IO@@UEBAIXZ                    retail 0x1800ae9c0
// ?GetSize@MemoryStream@IO@@UEBAIXZ                          retail 0x1800ae9d0
// ============================================================================
// Two instructions each. `UEBAI` — const, returning unsigned.
extern "C" uint32_t RtIo_MemoryStream_GetCurrentPos(const MemoryStream* self) {
    RT_DIFF_BEGIN("IO::MemoryStream::GetCurrentPos");
    const uint32_t v = self->cursor;             // 1800ae9c0
    RT_DIFF_WORK(true);
    return v;
}

extern "C" uint32_t RtIo_MemoryStream_GetSize(const MemoryStream* self) {
    RT_DIFF_BEGIN("IO::MemoryStream::GetSize");
    const uint32_t v = self->size;               // 1800ae9d0
    RT_DIFF_WORK(true);
    return v;
}

// ============================================================================
// ?Close@MemoryStream@IO@@UEAAXXZ                            retail 0x1800ae9e0
// ============================================================================
// A BARE RET. This is vtable slot 7 (+0x38) — the slot `ReadPackfile` calls
// virtually at 0x1800b6042 on its MemoryStream, which is therefore a provable
// no-op at that site.
//
// It is worth recording HOW this was nearly got wrong, because the shape is the
// one that has produced three Slot6/Slot7 mix-ups on this project already.
// MemoryStream's constructor stores TWO vtables into the same slot, thirteen
// bytes apart:
//     1800ae72d  LEA RAX,[0x1801549a0]   ; the BASE (Stream) table
//     1800ae734  MOV qword ptr [RCX],RAX
//     1800ae740  LEA RAX,[0x1801555b0]   ; the DERIVED table, overwrites it
//     1800ae747  MOV qword ptr [RCX],RAX
// Slot 7 of the BASE table is NULL. Slot 7 of the DERIVED table is this
// function. Stopping at the first store — the one that looks like "the vtable
// store" — yields a confident "slot 7 is a null call at cutover", and the base
// table's slots 2..6 all being one address reads like a plausible stub table,
// which CORROBORATES the wrong reading. A self-consistent wrong answer.
//
// Retail's is a no-op, so ours must be too: a Close that actually released
// anything would double-free against the dtor, which is where the release
// really lives (0x1800ae890).
extern "C" void RtIo_MemoryStream_Close(MemoryStream* self) {
    RT_DIFF_BEGIN("IO::MemoryStream::Close");
    (void)self;
    RT_DIFF_WORK(true);
    // 1800ae9e0: RET — deliberately empty.
}

}  // namespace RtIo

// ============================================================================
// Registration
// ============================================================================
int RtIoMemoryStreamRegister() {
    using namespace RtIo;
    int n = 0;

    RegisterRuntimeExport("?Read@MemoryStream@IO@@UEAA_NHPEAX@Z",
                          (void*)&RtIo_MemoryStream_Read); ++n;
    RegisterRuntimeAddr(0x1800ae950, (void*)&RtIo_MemoryStream_Read);

    RegisterRuntimeExport("?Write@MemoryStream@IO@@UEAA_NHPEBX@Z",
                          (void*)&RtIo_MemoryStream_Write); ++n;
    RegisterRuntimeAddr(0x1800ae8f0, (void*)&RtIo_MemoryStream_Write);

    RegisterRuntimeExport("?SetCurrentPos@MemoryStream@IO@@UEAA_NH@Z",
                          (void*)&RtIo_MemoryStream_SetCurrentPos); ++n;
    RegisterRuntimeAddr(0x1800ae9b0, (void*)&RtIo_MemoryStream_SetCurrentPos);

    RegisterRuntimeExport("?GetCurrentPos@MemoryStream@IO@@UEBAIXZ",
                          (void*)&RtIo_MemoryStream_GetCurrentPos); ++n;
    RegisterRuntimeAddr(0x1800ae9c0, (void*)&RtIo_MemoryStream_GetCurrentPos);

    RegisterRuntimeExport("?GetSize@MemoryStream@IO@@UEBAIXZ",
                          (void*)&RtIo_MemoryStream_GetSize); ++n;
    RegisterRuntimeAddr(0x1800ae9d0, (void*)&RtIo_MemoryStream_GetSize);

    RegisterRuntimeExport("?Close@MemoryStream@IO@@UEAAXXZ",
                          (void*)&RtIo_MemoryStream_Close); ++n;
    RegisterRuntimeAddr(0x1800ae9e0, (void*)&RtIo_MemoryStream_Close);

    printf("[RtPort]   IO::MemoryStream virtuals (lane PKG2 rung 4b-ii): "
                "%d exports registered\n", n);

    // THE CTOR AND DTOR ARE NOT HERE BECAUSE LANE W ALREADY PORTED THEM, not
    // because they are left over. I nearly shipped a comment saying they were
    // "4b-ii's next step" — the run's own diff table is what corrected it, by
    // listing IO::MemoryStream::~MemoryStream sites this file never declared:
    //   0x1800ae710  ??0MemoryStream@IO@@QEAA@XZ   -> RtIo_MemoryStream_Ctor
    //   0x1800ae890  ??1MemoryStream@IO@@UEAA@XZ   -> RtIo_MemoryStream_Dtor
    // both in runtime/src/io/compressedstream.cpp. So with the six bodies above,
    // IO::MemoryStream IS NOW COMPLETE across the two lanes.
    //
    // That is the same transport failure 4b-i hit with Stream.h, twice in one
    // rung: the answer was already in the tree and I went to the binary first.
    // The rule earned in 4b-i — grep the types header before opening the PE —
    // needs its sibling: GREP FOR THE PORT BEFORE PLANNING TO WRITE IT.
    //
    // ONE THING LANE W'S CTOR DOES NOT GIVE US, and it is a live cutover
    // blocker owned by them, not by this rung: the IO::Encoding it allocates
    // carries vtable 0x1801554a8, whose five slots are all unexported .text
    // internals that mint as NAMED TRAPS. Construction no longer calls through
    // null, but the first VIRTUAL call on that Encoding still traps. Nothing in
    // the six bodies above touches the Encoding, so this rung neither fixes nor
    // worsens it.
    //
    // The VTABLE IS NOT REPOINTED by this file either: these register as
    // bodies, so retail's own vtable still names retail's copies and only OUR
    // callers are redirected.
    return n;
}
