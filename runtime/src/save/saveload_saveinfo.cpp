// ============================================================================
// runtime/src/save/saveload_saveinfo.cpp — Wave 24 lane INTERNAL8
// ============================================================================
//
// Two of the eight real bodies inside the census's 45 `internal` UNKNOWN rows
// (WAVE23_LANE_INTERNAL.md §4B/B4):
//
//   0x1800c3db0   the qsort comparator UpdateList sorts the save list with
//   0x1800c3de0   Save::SaveLoad::SaveInfo::SetThumbnail(row, px, w, h)
//
// Both were already fully DESCRIBED by our own tree — saveload_update.cpp's
// "THE UpdateList DEPENDENCY SET" banner transcribes the comparator byte for
// byte and names SetThumbnail's three stores — and both were still address
// doors into the shipped DLL. That is the whole of the `internal` mystery: a
// row printed UNKNOWN because the census reads names and a comment is not a
// name.
//
// EXTENTS, AND HOW THEY WERE BOUNDED
// ---------------------------------------------------------------------------
//   0x1800c3db0   34 bytes, 0x1800c3db0..0x1800c3dd1, then 0xCC padding.
//   0x1800c3de0  332 bytes, 0x1800c3de0..0x1800c3f2b (tail JMP).
//
// NEITHER came from `.pdata`, and for the comparator `.pdata` could not have
// answered at all. 52.9% of this DLL's `.text` exports have NO `.pdata` entry
// (leaf functions) and this one is a leaf with no unwind data and no Ghidra
// function object either. Its extent is read out of the IMAGE BYTES: 34 bytes
// of instructions terminating in `C3`, followed by a run of `CC`. SetThumbnail
// is bounded by Ghidra's function body, which ends at the tail
// `JMP qword ptr [0x18014b388]` (LeaveCriticalSection).
//
// `.pdata` UNDERSTATES an extent — MSVC chains fragments through
// UNW_FLAG_CHAININFO, and the ENTRY fragment is the one that reads
// `chained=False`, so a chained function looks complete when looked up by its
// own address (a 20.4x understatement was measured on a row in this same
// bucket). Understating makes work look cheap and truncates a port silently.
// Neither body here is chained, but the bound is taken from an instrument that
// cannot understate rather than from one that can.
// ============================================================================

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/save/SaveLoad.h"
#include "../../types/video/ImageBuffer.h"
#include "../../types/video/Device.h"   // ENDGAME B.3.9 SS1: Video::Device::_singleton
#include "saveload_internal.h"
#include "../../types/system/MemoryAllocator.h"

using RtSave::SaveInfo;

// Video::Device's recursive render lock, already ported in
// runtime/src/video/buffers_win.cpp and already used from this directory
// (saveload_update.cpp:94). Declared, not re-implemented.
void RtVideoDeviceLock(void** deviceSingleton);
void RtVideoDeviceUnlock(void** deviceSingleton);

namespace {

// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z, 0x1800c7a50.
//
// SetThumbnail does NOT call this symbol — it OPEN-CODES the triple
// `EnterCriticalSection(*0x1801fc718 + 0x640); Allocate(*0x1801fc718, 0x68, 8);
// LeaveCriticalSection(...)`. Lane Q disassembled 0x1800c7a50 and proved that
// triple IS its body, instruction for instruction; array.cpp:155 and
// message_callbacks.cpp:91 already reach the allocator this way for exactly
// the same open-coded shape. Calling the composed symbol keeps this file free
// of <windows.h> (PLAN §4.1) at the cost of nothing observable.
//
// B3R RT-IO: the static member by name; the atomic by-address slot (and its
// readiness test, always true: 0x1800c7a50 has always been registered) is gone.
void* SaveInfoMemAlloc(uint64_t bytes, int align) {
    return ::System::MemoryAllocator::MemAlloc(bytes, align);
}

// ??0ImageBuffer@Video@@QEAA@XZ, 0x1800f2580. ENDGAME B.3.9 VS: `::new (p) Video::ImageBuffer()` -- the ??0 door and the minted row are retired
// (the by-address door lookup and its atomic slot went with it).
void ImageBufferCtor(void* self) {
    ::new (self) Video::ImageBuffer();
}

// ?_singleton@Device@Video@@0PEAV12@EA, 0x1801fe7a8 — the SLOT, not the Device.
// ENDGAME B.3.9 SS1 (B39_RULES D7): the class static's address IS this cell.
void** DeviceSingletonSlot() {
    return reinterpret_cast<void**>(&::Video::Device::_singleton);
}

// ---------------------------------------------------------------------------
// 0x1800c3db0 as a plain function, so the sweep and the registered entry point
// drive the SAME code. A probe that re-derived the body would be testing a
// copy of the port.
//
// The bytes, read out of the image (Ghidra has no function object here):
//
//     83 79 40 00  CMP dword ptr [RCX+0x40],0x0
//     7c 16        JL  .greater             ; a.saveIndex < 0 -> return 1
//     83 7a 40 00  CMP dword ptr [RDX+0x40],0x0
//     7c 10        JL  .greater             ; b.saveIndex < 0 -> return 1
//     48 8b 41 30  MOV RAX,[RCX+0x30]       ; a.sortKey
//     48 8b 4a 30  MOV RCX,[RDX+0x30]       ; b.sortKey
//     48 3b c8     CMP RCX,RAX              ; UNSIGNED, b vs a
//     77 03        JA  .greater
//     1b c0        SBB EAX,EAX              ; CF set iff b<a  ->  -1 ; else 0
//     c3           RET
//   .greater:
//     b8 01 00 00 00 / c3                   ; return 1
//
// THREE THINGS A "TIDIER" SPELLING GETS WRONG, all of them observable:
//
//  1. The compare is UNSIGNED (`JA`, and `SBB` on CF). sortKey is a uint64 and
//     the top bit is live in a FILETIME-shaped value; a signed compare would
//     reverse the order of any pair straddling 2^63.
//  2. It is NOT A TOTAL ORDER. A row with saveIndex < 0 compares GREATER in
//     BOTH positions — cmp(blank, x) == 1 and cmp(x, blank) == 1 — so a blank
//     row and any other row are each "after" the other. That is a genuine
//     violation of the strict weak ordering qsort requires, and it is retail's
//     behaviour. UpdateList gets away with it because the only such row is the
//     "new save" slot and state 1 deliberately sorts the sub-range that
//     EXCLUDES it (the base+0x58 / count-1 shift in saveload_update.cpp). The
//     comparator is reproduced as written, violation included; "fixing" it
//     would change the row order the player sees on any input where retail's
//     inconsistency actually fires.
//  3. Ordering is DESCENDING by sortKey (newest first), because the compare is
//     `b vs a` and not `a vs b`.
// ---------------------------------------------------------------------------
int SaveInfoCompareImpl(const void* a, const void* b) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);

    if (*reinterpret_cast<const int32_t*>(pa + 0x40) < 0) return 1;
    if (*reinterpret_cast<const int32_t*>(pb + 0x40) < 0) return 1;

    const uint64_t ka = *reinterpret_cast<const uint64_t*>(pa + 0x30);
    const uint64_t kb = *reinterpret_cast<const uint64_t*>(pb + 0x30);
    if (kb > ka) return 1;     // JA  — UNSIGNED
    if (kb < ka) return -1;    // SBB EAX,EAX with CF set
    return 0;                  // SBB EAX,EAX with CF clear
}

}  // namespace

// ============================================================================
// 0x1800c3db0 — the qsort comparator. PURE.
// ============================================================================
// It is passed to qsort as DATA, so retail never "calls" it from a site we can
// wrap; the sweep below is the verification, not a call-site oracle.
int Save::SaveLoad::SaveInfo::Compare(const void* a, const void* b) {
    return SaveInfoCompareImpl(a, b);
}

// The registered address door (0x1800c3db0).
extern "C" int RtSave_SaveInfo_Compare(const void* a, const void* b) {
    return Save::SaveLoad::SaveInfo::Compare(a, b);
}

// ============================================================================
// 0x1800c3de0 — SaveInfo::SetThumbnail(row, pixels, w, h)
// ----------------------------------------------------------------------------
// void (SaveInfo* row /*RCX*/, void* pixels /*RDX*/, int w /*R8D*/, int h /*R9D*/)
//
// Retail, in order:
//   1. allocate 0x68 bytes / align 8 under the ALLOCATOR's lock and
//      placement-construct a Video::ImageBuffer in it. 0x68 is exactly
//      sizeof(Video::ImageBuffer) as this tree's header already asserts, which
//      is an independent corroboration of the layout and not a coincidence.
//   2. publish it at row+0x18 (`imageBuffer`) and call vtable slot 9,
//      ?Create@ImageBuffer@Video@@ — eleven arguments, four in registers and
//      seven on the stack.
//   3. under VIDEO DEVICE's lock (a different lock from step 1), call the
//      ImageBufferData at +0x60 through ITS vtable slot 5 with (pixels, w, h).
//   4. store pixels/w/h at row+0x28/+0x20/+0x24 — the three stores that are the
//      independent confirmation of this header's thumbPixels/Width/Height
//      names, since OnVideoRestart reads the same three back as upload args.
//
// *** THE CTOR'S RETURN VALUE IS NOT USED HERE, DELIBERATELY. *** Retail does
// `RDI = alloc; if (RDI) { CALL ctor; RDI = RAX; }` — an MSVC constructor
// returns `this` in RAX, so RAX == RDI and the move is a no-op. Our port of
// that ctor is a plain `void (ImageBuffer*)`, and GCC does NOT guarantee `this`
// in RAX on return from it. Reading RAX through a `void*(*)(void*)` typedef
// here would be reading a register the callee does not set — the same class of
// defect as an fn_* typedef that omits a hidden return slot
// (reference_hidden_retslot_wild_write.md), and it would silently work while
// the DLL body is in use and break at cutover. So the allocation pointer is
// carried in a local. The two are provably the same value.
//
// Retail does NOT null-check `row`, `row->imageBuffer` after Create, or the
// ImageBufferData at +0x60, and neither does this. The one `if` it does have —
// `TEST RDI,RDI` after the allocation — is reproduced, and note what it guards:
// on an allocation failure retail skips only the CONSTRUCTOR and then stores
// null at row+0x18 and DEREFERENCES IT one instruction later at
// `MOV RAX,[RDI]`. That is a fault on OOM in the shipped binary. Transcribed
// as written (CLAUDE.md: do not "fix" a faithful crash).
// ============================================================================
void Save::SaveLoad::SaveInfo::SetThumbnail(void* pixels, int32_t w, int32_t h) {
    void* mem = SaveInfoMemAlloc(0x68, 8);
    if (mem != nullptr) ImageBufferCtor(mem);
    ImageBuffer* img = static_cast<ImageBuffer*>(mem);   // == RAX; see the banner

    imageBuffer = img;

    // vtable slot 9 (+0x48):
    //   ?Create@ImageBuffer@Video@@UEAA_N W4ImageType W4Usage W4Format I H H
    //                                     W4FilterMode W4AddressMode 4 4
    //                                     PEAV?$Array@V?$Array@E@Collection@@@
    // Argument values are immediates in the shipped body and are transcribed,
    // not interpreted: type=0, usage=2, format=5, 0, w, h, filter=0,
    // addressU=addressV=addressW=1, no initial data.
    // ENDGAME B.3.5: `img` is a PROVED `Video::ImageBuffer` — it is the 0x68-byte
    // allocation `ImageBufferCtor` (0x1800f2580) just constructed two lines up,
    // so the receiver is known from the code that produced the pointer and not
    // from the variable's name. Slot 9 is therefore the virtual, called as one.
    // The live object wears the MINTED `??_7ImageBuffer@Video@@6B@`, whose slot 9
    // holds the registered `Video_ImageBuffer_Create` body; `Create` is `= 0` on
    // the class for the reason ImageBuffer.h's slot-9 banner gives, and that
    // changes nothing at a call site.
    reinterpret_cast<Video::ImageBuffer*>(img)->Create(
        0, 2, 5, 0u, w, h, 0, 1, 1, 1, nullptr);         // B.3.5 bridge

    // ---- the VIDEO DEVICE lock, open-coded in retail ----------------------
    // 0x1800c3eb4..0x1800c3eca is EnterCriticalSection(dev+0x60) /
    // GetCurrentThreadId / [dev+0x88]=tid / INC [dev+0x8c] — the OWNER-FIRST
    // inline copy, and the unlock at 0x1800c3eee RE-READS the singleton before
    // decrementing. That is exactly RtVideoDeviceLock/Unlock, which take the
    // SLOT for that reason. This is a different lock from the allocator's, and
    // running the two in the wrong nesting would be a deadlock, not a style
    // difference — the allocator's is already released by step 1.
    void** devSlot = DeviceSingletonSlot();
    RtVideoDeviceLock(devSlot);

    // ImageBufferData vtable slot 5 (+0x28) — the pixel upload. PURECALL in
    // the base Video table; the live body is the DX11 override.
    // `img->platformData` is the companion object the ctor/`Create` pair built
    // for THIS ImageBuffer, so the receiver is proved. Slot 5 of the abstract
    // `Video::ImageBufferData` is `bool Update(void*, int, int)`
    // (CompanionData.h); retail ignores the result and so does this. The
    // CONCRETE `*Data` subclass and the dx11 seam are a declared leave for step 5
    // (B35_RULES 1.1 item 4 / section 11) — only the call converts.
    void* data = img->platformData;                    // +0x60
    reinterpret_cast<Video::ImageBufferData*>(data)->Update(pixels, w, h);  // B.3.5 bridge

    thumbPixels = pixels;   // +0x28
    thumbWidth  = w;        // +0x20
    thumbHeight = h;        // +0x24

    RtVideoDeviceUnlock(devSlot);

    // The Create arguments are verified by TRANSCRIPTION against the
    // immediates in the listing.
}

// The registered address door (0x1800c3de0): `(row, pixels, w, h)`.
extern "C" void RtSave_SaveInfo_SetThumbnail(SaveInfo* row, void* pixels, int w, int h) {
    row->Save::SaveLoad::SaveInfo::SetThumbnail(pixels, w, h);
}

// ============================================================================
// THE INPUT SWEEP — a REAL two-sided oracle for 0x1800c3db0
// ============================================================================
//
// Probe shape (kickoff §1c): an INPUT SWEEP against the shipped body. It is
// available here and nowhere else in this lane because the comparator is the
// only PURE row: no state, no allocation, two const pointers in and an int out.
//
// WHY A SWEEP AND NOT A CALL-SITE ORACLE. The comparator is passed to qsort as
// DATA. There is no call site of ours to wrap, and a port with zero callers
// reports `calls=0` and reads CLEAN (reference_uncalled_port_is_not_verified.md).
// Waiting for a caller would have produced exactly that
// (project_pure_functions_verify_by_sweep.md — GetAlignedPosition reported
// calls=0 in a 16.9M-call in-game run and was verified by sweep instead).
//
// THEIRS is reached through ResolveRuntimeRaw, NOT ResolveAddr. ResolveAddr
// consults the port registry FIRST, and this file REGISTERS 0x1800c3db0 — so
// using it would compare our body with itself and pass unconditionally. That
// exact defect kept a Wave 5 probe dark for five waves.
//
// CONTROLS, and what each is for:
//   * EFFECTIVENESS — `g_decisive` counts the cases that reached the sortKey
//     comparison at all. Every case with a negative saveIndex returns 1 from
//     the first two instructions; a sweep made only of those would be 100%
//     green while testing two CMPs. If g_decisive is 0 the sweep proves
//     nothing and says so in those words.
//   * NEGATIVE / mutation — a deliberately WRONG comparator (SIGNED compare,
//     which is the single most likely way to get this body wrong) is pushed
//     through the SAME comparison path. It must be CAUGHT. The mutation
//     changes a value the probe actually compares (the return code), which is
//     the requirement a corrupted-but-uncompared control fails
//     (reference_control_must_pick_a_compared_subject.md).
//   * A second negative confirms the sweep is not merely "returns nonzero on
//     any change": the wrong comparator agrees with retail on every pair whose
//     keys are both below 2^63, so the sweep also reports HOW MANY of the
//     mutant's answers differed. If that count is 0 the negative control is
//     inconclusive rather than passing, and it is reported that way.
//
// It runs under the old DLL-era switch. Lane INTERNAL8 has no
// game-directory lock; the intended driver is lane DIFF's armed sweep, which
// sets nothing else this file needs.
// ============================================================================
namespace {

struct SweepRow {   // a 0x58-byte SaveInfo stand-in; only +0x30 and +0x40 matter
    uint8_t bytes[0x58];
};

void MakeRow(SweepRow* r, uint64_t sortKey, int32_t saveIndex) {
    // A varying non-zero pattern everywhere else: an all-zero subject is the
    // one seed under which a wrong offset is invisible, because both sides
    // read zero there and agree by accident.
    for (size_t i = 0; i < sizeof(r->bytes); ++i)
        r->bytes[i] = static_cast<uint8_t>(0x5A + (i & 0x1f));
    *reinterpret_cast<uint64_t*>(r->bytes + 0x30) = sortKey;
    *reinterpret_cast<int32_t*>(r->bytes + 0x40) = saveIndex;
}

// The MUTANT: identical except the key compare is SIGNED. Used as the negative
// control. It is NOT registered and nothing else calls it.
int SaveInfoCompareSignedMutant(const void* a, const void* b) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    if (*reinterpret_cast<const int32_t*>(pa + 0x40) < 0) return 1;
    if (*reinterpret_cast<const int32_t*>(pb + 0x40) < 0) return 1;
    const int64_t ka = *reinterpret_cast<const int64_t*>(pa + 0x30);
    const int64_t kb = *reinterpret_cast<const int64_t*>(pb + 0x30);
    if (kb > ka) return 1;
    if (kb < ka) return -1;
    return 0;
}

}  // namespace
