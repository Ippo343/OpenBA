#pragma once
// ============================================================================
// runtime/types/system/MemoryAllocator.h — System::MemoryAllocator (Wave 2, lane C)
// ============================================================================
//
// Retail Runtime-x64.dll, image base 0x180000000. Every offset below is
// static_assert'ed at the bottom of this file and every one is pinned by a
// named, decompiled retail function (the function is cited next to the field).
//
// ---------------------------------------------------------------------------
// WHAT IT IS
// ---------------------------------------------------------------------------
// A single-arena BOUNDARY-TAG allocator with 192 segregated free lists.
// It is NOT a bucketed pool allocator: the bins index a first-fit search over
// variable-sized blocks, they do not own fixed-size cells. There is no
// large-allocation path and no fallback to the CRT heap — every allocation of
// every size comes out of the same boundary-tag arena, and when the arena
// cannot satisfy a request the allocator raises a fatal Error box.
//
// Memory comes from VirtualAlloc in two stages: a range is RESERVED whole
// (MEM_RESERVE) and pages are COMMITTED lazily as the high-water mark of used
// bytes advances (CommitToPage) and DECOMMITTED when the tail block is freed
// (ReleaseToPage). So the process's committed footprint tracks the peak of the
// arena, not the reserve.
//
// The allocator object is EMBEDDED AT OFFSET 0 OF Engine::Engine:
// `Engine::Engine::GetMemoryAllocator` (0x18000c8b0) is a bare `return this`.
// `_singleton` (exported data, 0x1801fc718) is set by the ctor and cleared by
// the dtor, so the Engine object's address IS the allocator singleton.
//
// ---------------------------------------------------------------------------
// THE ALLOCATION HEADER — and a correction to the note in
// standalone/filename_runtime_hook.cpp
// ---------------------------------------------------------------------------
// That file's header says "refcount at ptr-4, user pointer at ptr-8". Verified
// against the disassembly: that is TRUE, and it describes System::String, NOT
// this allocator. `System::String::SetLength` (0x1800c8220) is the proof:
//
//     U   = MemoryAllocator::Allocate(_singleton, len + 1 + 8, 4);
//     *(int*)(U + 0) = len;          // String length
//     *(int*)(U + 4) = 1;            // String refcount
//     this->_data    = (char*)U + 8; // the char* a String* points at
//
// So for a String `s`:  s-4 == U+4 == refcount, s-8 == U == the ALLOCATOR
// user pointer — and the allocator's own header sits at U-8 == s-0x10.
//
// The allocator header is 8 bytes and holds NO refcount:
//
//     MemoryTag (8 bytes, immediately below every user pointer)
//     ------------------------------------------------------------------
//     bits  0..40  size of this block in 4-BYTE UNITS, tag included.
//                  Bytes = field * 4. Confirmed by GetAllocationSize
//                  (0x1800c71d0): `return (*(u64*)(p-8) & 0x1ffffffffff) << 2;`
//     bits 41..43  size of the PREVIOUS block in 4-byte units, when that fits
//                  in 3 bits (values 1..7). 0 means "too big for the field":
//                  the previous block's byte size is then stored as a full
//                  qword FOOTER at (tag - 8), i.e. in the last 8 bytes of the
//                  previous block. Meaningful only when bit 44 is set.
//     bit  44      PREV_FREE  (0x0000'1000'0000'0000)
//     bit  45      THIS_FREE  (0x0000'2000'0000'0000)
//     bits 46..63  high 18 bits (bits 23..40) of the free-list PREV delta.
//                  Only meaningful while the block is linked into a free list;
//                  cleared by UnlinkFreeTag and by Allocate.
//
//     FreeTag (the first 8 bytes of the USER AREA of a free block, tag + 8)
//     ------------------------------------------------------------------
//     bits  0..40  free-list NEXT delta, a SIGNED 41-bit byte offset relative
//                  to this FreeTag's own address. 0 == end of list.
//     bits 41..63  low 23 bits of the free-list PREV delta (also a signed
//                  41-bit byte offset relative to this FreeTag).
//
// The free list is therefore DOUBLY linked with 41-bit signed relative
// offsets, and the prev link is SPLIT ACROSS TWO QWORDS: low 23 bits in the
// FreeTag, high 18 bits in the MemoryTag above it. Reassembly (UnlinkFreeTag,
// 0x1800c7410):
//
//     prevDelta = ((int64)tag >> 23) & 0xffffffffff800000   // sign-extended
//               | (freeTag >> 41);
//     nextDelta = (int64)(freeTag << 23) >> 23;             // sign-extended
//
// A block is linked into a free list only when it is at least 0x10 bytes
// (LinkFreeTag's `if (0xf < bytes)`, UnlinkFreeTag(MemoryTag*)'s `if (3 < size)`).
// Free blocks smaller than 16 bytes exist, carry THIS_FREE, and are simply
// unreachable holes until a neighbour coalesces them.
//
// ---------------------------------------------------------------------------
// THE BINS (GetBinForSize 0x1800c78d0 / GetBinForLargeSize 0x1800c7920)
// ---------------------------------------------------------------------------
//     bytes <= 0x200 : bin = (int)bytes >> 2          -> 0 .. 128, exact size
//     bytes >  0x200 : bin = 119 + floor(log2(bytes)) -> 128 .. 191, power-of-2
// The two ranges join continuously at bin 128 (0x200 maps to 128 either way).
// There are 192 bins; only 2..191 are reachable, because the minimum block is
// 16 bytes (bin 4) after Allocate's `if (total < 8) total = 8; total += 8`.
//
// The log2 loop is the classic binary-search bit scan:
//     s=32; n=64; do { if (v>>s) { n-=s; v>>=s; } s>>=1; } while (s);
//     bin = (int)v - n + 0xb6;
// It leaves v == 1 for every input, so it is exactly 119 + floor(log2(bytes));
// it is transcribed literally below rather than replaced by a bit scan, so the
// two builds cannot disagree on a boundary case.
//
// `_minBin` (+0x628) is a search FLOOR, not a hint: GetFreeTagForSize clamps
// its starting bin UP to _minBin. It starts at 0xbf (191, "everything is
// empty") and is lowered by LinkFreeTag; UnlinkFreeTag walks it back up when
// it empties the bin that _minBin names.
//
// ---------------------------------------------------------------------------
// ALLOCATE (0x1800c6e60) — the shape, so the port can be read against it
// ---------------------------------------------------------------------------
//   align8 = (align + 7) & ~7                       // NOTE: 32-bit
//   total  = (size + 7 & ~7) + (int64)(align8 - 8)  // sign-extended
//   if (total < 8) total = 8
//   total += 8                                      // room for the tag
//   tag = GetFreeTagForSize(total) - 8, growing the arena once if needed
//   UnlinkFreeTag(tag + 8, nullptr)
//   range = GetMemoryRange(tag)                     // inlined linear scan
//   pad = alignUp(user, align8) - user              // 32-BIT arithmetic
//   if (pad) split off [tag, tag+pad) as a free block and move tag forward
//   if (blockBytes - total < 0x10) take the whole block
//   else split the tail off as a new free block (and coalesce forward)
//   commit up to the end of the block; _allocatedBytes += blockBytes; ++_allocCount
//   return tag + 8
//
// Two things a reimplementation gets wrong if it "cleans them up":
//   * `pad` is computed in 32-bit registers (`IMUL EAX,EDI` / `SUB EAX,R8D` at
//     0x1800c6f7d) even though the divide is 64-bit. Faithful code keeps the
//     truncation.
//   * `(int64)(align8 - 8)` is SIGN-extended (CDQE at 0x1800c6ea1), so
//     align == 0 subtracts 8 from the request rather than adding 4G.
//
// FREE (0x1800c6c00) coalesces backwards first (via PREV_FREE + the 3-bit
// prevSize or the footer), then forwards, then decommits if the coalesced
// block now reaches the end of its range, then relinks. Note it decrements
// `_allocCount` and `_allocatedBytes` BEFORE coalescing, using the size the
// user block had.
//
// ---------------------------------------------------------------------------
// STATISTICS / DEBUG FIELDS
// ---------------------------------------------------------------------------
//   +0x630 _allocatedBytes  int64, exposed by GetAllocatedSize (0x18007c0b0)
//   +0x638 _allocCount      int, live block count
//   +0x63c _freeTagCount    int, number of linked free blocks
//   0x1801ff300             a FILE-STATIC global (not a member): the largest
//                           `size` argument ever passed to Allocate. Written
//                           unconditionally at the top of Allocate; nothing
//                           reads it in retail. Named g_largestRequestedSize
//                           here; the port maintains it because it is a store
//                           our shadow oracle can observe.
//
// There is no guard byte, no fill pattern and no leak list. Allocate returns
// DIRTY memory (already noted in code/Game/Game/serialize_arrays.cpp:354).
//
// ---------------------------------------------------------------------------
// PORTABILITY (PLAN.md §4.1)
// ---------------------------------------------------------------------------
// This header must not include <windows.h>. The allocator genuinely needs
// VirtualAlloc/VirtualFree and a recursive mutex, so those five operations are
// a function-pointer seam (RtSysMemPlatform below) that the host installs. The
// Win32 implementation belongs in `runtime/src/system/MemoryAllocator_win.cpp`
// — a file this lane is not allowed to create; its exact contents are given in
// docs/runtime_port/WAVE2_ALLOCATOR.md for the lead to land. Until it is
// installed the port refuses to arm and says so, which is why nothing here can
// silently run on a half-built seam.
//
// The CRITICAL_SECTION at +0x640 is declared as an opaque 0x28-byte blob. It
// is 40 bytes on x64 MSVC and the object layout depends on that; the lock/
// unlock seam operates on its address, which is exactly what
// standalone/allocator_lock.cpp already does (`self + 0x640`).
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace rt {
namespace system {

// ---------------------------------------------------------------------------
// Tag bit field constants
// ---------------------------------------------------------------------------
static const uint64_t kTagSizeMask     = 0x000001ffffffffffull; // bits 0..40, /4
static const uint64_t kTagPrevSizeMask = 0x00000e0000000000ull; // bits 41..43, /4
static const uint64_t kTagPrevFree     = 0x0000100000000000ull; // bit 44
static const uint64_t kTagThisFree     = 0x0000200000000000ull; // bit 45
static const uint64_t kTagPrevHiMask   = 0xffffc00000000000ull; // bits 46..63

// Literals as they appear in the disassembly, kept so the port reads like the
// listing rather than like a cleaned-up rewrite.
static const uint64_t kSizeBytesMask   = 0x000007fffffffffcull; // bits 2..42
static const uint64_t kThisFreeShl2    = 0x0000800000000000ull; // kTagThisFree << 2

// ---------------------------------------------------------------------------
// MemoryTag / FreeTag — the two names the DLL's own mangled signatures use
// (`PEAUMemoryTag@12@`, `PEAUFreeTag@12@`). Both are a single packed qword; the
// bit fields are documented at the top of this file. They are deliberately NOT
// C bitfield structs: the code manipulates them with the same masks and shifts
// the listing does, and a bitfield layout would be a second, unverified model
// of the same bits.
//
//   MemoryTag lives at (userPointer - 8) and heads every block.
//   FreeTag   lives at (tag + 8) — the first qword of a FREE block's user area.
//             GetFreeTagForSize/LinkFreeTag/UnlinkFreeTag pass FreeTag*, i.e.
//             the USER pointer of a free block, not its tag.
// ---------------------------------------------------------------------------
struct MemoryTag { uint64_t bits; };
struct FreeTag   { uint64_t link; };
static_assert(sizeof(MemoryTag) == 8, "MemoryTag is one packed qword");
static_assert(sizeof(FreeTag) == 8, "FreeTag is one packed qword");

// ---------------------------------------------------------------------------
// MemoryRange — one VirtualAlloc reservation. 0x18 bytes.
// Pinned by AddNewMemoryRange (0x1800c7690), CommitToPage (0x1800c7bd0),
// ReleaseToPage (0x1800c7c80), GetMemoryRange (0x1800c71f0).
// ---------------------------------------------------------------------------
struct MemoryRange {
    uint8_t* base;       // +0x00 reservation base; also the first MemoryTag
    uint8_t* end;        // +0x08 base + logical size (NOT base + reserved size)
    uint64_t committed;  // +0x10 bytes committed from base
};
static_assert(sizeof(MemoryRange) == 0x18, "MemoryRange is 0x18 bytes");
static_assert(offsetof(MemoryRange, base) == 0x00, "MemoryRange::base");
static_assert(offsetof(MemoryRange, end) == 0x08, "MemoryRange::end");
static_assert(offsetof(MemoryRange, committed) == 0x10, "MemoryRange::committed");

// ---------------------------------------------------------------------------
// MemoryAllocator — 0x670 bytes (operator= at 0x18007c0c0 memcpy's 0x670).
// ---------------------------------------------------------------------------
struct MemoryAllocator {
    MemoryRange* ranges;         // +0x000 ctor, GetMemoryRange, Shutdown
    int32_t      rangeCount;     // +0x008 ctor, AddNewMemoryRange
    int32_t      rangeCapacity;  // +0x00c ExpandMemoryRangeTable
    uint64_t     minRangeSize;   // +0x010 Startup arg1 << 20 (megabytes)
    uint64_t     maxTotalSize;   // +0x018 Startup arg2 << 20; 0 == unlimited
    uint64_t     totalReserved;  // +0x020 AddNewMemoryRange accumulates
    uint64_t     freeLists[192]; // +0x028 FreeTag* per bin, 0x600 bytes
    int32_t      minBin;         // +0x628 search floor; ctor sets 0xbf
    int32_t      pad62c;         // +0x62c never written by any retail function
    int64_t      allocatedBytes; // +0x630 GetAllocatedSize
    int32_t      allocCount;     // +0x638 live blocks
    int32_t      freeTagCount;   // +0x63c linked free blocks
    uint8_t      lock[0x28];     // +0x640 CRITICAL_SECTION (x64 MSVC: 40 bytes)
    uint64_t     pageSize;       // +0x668 GetSystemInfo().dwPageSize
};
static_assert(sizeof(MemoryAllocator) == 0x670, "MemoryAllocator is 0x670 bytes");
static_assert(offsetof(MemoryAllocator, ranges) == 0x000, "ranges");
static_assert(offsetof(MemoryAllocator, rangeCount) == 0x008, "rangeCount");
static_assert(offsetof(MemoryAllocator, rangeCapacity) == 0x00c, "rangeCapacity");
static_assert(offsetof(MemoryAllocator, minRangeSize) == 0x010, "minRangeSize");
static_assert(offsetof(MemoryAllocator, maxTotalSize) == 0x018, "maxTotalSize");
static_assert(offsetof(MemoryAllocator, totalReserved) == 0x020, "totalReserved");
static_assert(offsetof(MemoryAllocator, freeLists) == 0x028, "freeLists");
static_assert(offsetof(MemoryAllocator, minBin) == 0x628, "minBin");
static_assert(offsetof(MemoryAllocator, allocatedBytes) == 0x630, "allocatedBytes");
static_assert(offsetof(MemoryAllocator, allocCount) == 0x638, "allocCount");
static_assert(offsetof(MemoryAllocator, freeTagCount) == 0x63c, "freeTagCount");
static_assert(offsetof(MemoryAllocator, lock) == 0x640, "lock");
static_assert(offsetof(MemoryAllocator, pageSize) == 0x668, "pageSize");

// ---------------------------------------------------------------------------
// Platform seam. See the PORTABILITY note above: the host installs this table,
// the port refuses to arm without it. Flag values are named after the Win32
// constants the retail code passes, because that is what the port must
// reproduce, not because the seam has to be Windows.
// ---------------------------------------------------------------------------
struct RtSysMemPlatform {
    // VirtualAlloc(addr, bytes, MEM_RESERVE,             PAGE_READWRITE)
    void*    (*reserve)(void* addr, uint64_t bytes);
    // VirtualAlloc(addr, bytes, MEM_RESERVE|MEM_COMMIT,  PAGE_READWRITE)
    void*    (*reserveCommit)(void* addr, uint64_t bytes);
    // VirtualAlloc(addr, bytes, MEM_COMMIT,              PAGE_READWRITE)
    void*    (*commit)(void* addr, uint64_t bytes);
    // VirtualFree(addr, bytes, MEM_DECOMMIT)
    void     (*decommit)(void* addr, uint64_t bytes);
    // VirtualFree(addr, 0, MEM_RELEASE)
    void     (*release)(void* addr);
    // GetSystemInfo().dwPageSize
    uint64_t (*pageSize)();
    // InitializeCriticalSectionAndSpinCount(cs, 5000) / Enter / Leave / Delete
    void     (*lockInit)(void* cs);
    void     (*lockEnter)(void* cs);
    void     (*lockLeave)(void* cs);
    void     (*lockDelete)(void* cs);
};

} // namespace system
} // namespace rt

// Installed by the host (standalone/) before RtSystemAllocatorRegister() runs.
// Declared extern "C" so the _win file can be a plain translation unit.
extern "C" void RtSysMemPlatformInstall(const rt::system::RtSysMemPlatform* p);

// The atomic-swap installer. The host detours BOTH MemoryAllocator::Allocate
// (0x1800c6e60) and ::Free (0x1800c6c00) to the two function pointers it is
// handed, and returns true only if BOTH took. It must also RETIRE the
// allocator_lock patches on the same two prologues first — see
// docs/runtime_port/WAVE2_ALLOCATOR.md §4. Without this installed,
// RtSystemAllocatorRegister() registers nothing.

// ---- CUTOVER ARMING (Wave 9 lane E4) ---------------------------------------
//
// The seam above is a DLL-PRESENT concept from top to bottom, and that is why
// the allocator could not arm with the DLL renamed aside: the detour exists
// solely to redirect the DLL's OWN internal callers, and the prologue guard
// reads the DLL's bytes to check nobody else owns them. With no DLL there are
// no internal callers and no bytes, so the guard resolves null, reports
// "unresolved", and REFUSES — correctly by its own logic, and wrongly for the
// situation.
//
// With no DLL there is also no second allocator, so the whole reason the swap
// had to be atomic disappears: registering the exports IS the complete swap.
// This seam is that path. The host hands back the COMPOSED (locked) entry
// points to register for the two member exports, because with no DLL there is
// nothing for allocator_lock.cpp to patch and the critical section has to come
// from somewhere.
//
// Returns false to refuse; RtSystemAllocatorRegister() then registers nothing,
// exactly as it does when the detour installer refuses.
typedef bool (*RtAllocCutoverArmer)(void* ourAllocate, void* ourFree,
                                    void** outComposedAllocate,
                                    void** outComposedFree);
extern "C" void RtSystemAllocatorSetCutoverArmer(RtAllocCutoverArmer fn);

// (The DLL-present detour-installer and lock-retire seams died with the DLL, ENDGAME A.3.)

// runtime/src/system/allocator_win.cpp — installs the Win32 seam and holds the
// six exported Platform* bodies. The portable half calls this once, under
// #if defined(_WIN32); a _posix/_sdl twin provides the same two symbols.
extern "C" void RtSystemAllocatorInstallPlatform();
extern "C" int  RtSystemAllocatorRegisterPlatformExports();

// Shadow oracle (see the long comment in runtime/src/system/allocator.cpp).
extern "C" bool RtAllocShadowStartup(uint64_t minRangeMB, uint64_t maxTotalMB);
extern "C" void RtAllocShadowAlloc(uint64_t size, int align, void* theirs,
                                   const rt::system::MemoryAllocator* theirSelf);
extern "C" void RtAllocShadowFree(void* theirs,
                                  const rt::system::MemoryAllocator* theirSelf);
extern "C" void RtAllocShadowReport(uint64_t* calls, uint64_t* divergences);
// Harness health, NOT port correctness: replay-map overflow and frees that had
// no mapped replica pointer. Either one non-zero invalidates the run.
extern "C" void RtAllocShadowHealth(uint64_t* mapFull, uint64_t* freeUnmapped);
extern "C" void RtAllocShadowCoverage(int* ranges, uint64_t* reserved);
// Arena-growth effectiveness control: how many times the growth path actually
// ran, how many of those took ExpandMemoryRangeTable's copy-and-release branch,
// and how many comparisons landed outside range 0. See WAVE9_LANE_E3.md.
extern "C" void RtAllocShadowGrowth(uint64_t* grow, uint64_t* tableCopy,
                                    uint64_t* crossRange);
// Wave 10 lane SHADOWBISECT — unobserved-mutation ("gap") detector. Pre/post-op
// hooks called from the allocator-lock wrappers inside the critical section;
// a non-zero gap count means the real arena was mutated by a route that does
// not pass through the patched Allocate/Free entry.
extern "C" void RtAllocShadowPreOp(const rt::system::MemoryAllocator* theirSelf);
extern "C" void RtAllocShadowPostOp(const rt::system::MemoryAllocator* theirSelf);
extern "C" void RtAllocShadowGaps(uint64_t* checks, uint64_t* gaps, int64_t* dAlloc,
                                  int64_t* dFreeTag, int64_t* dBytes,
                                  uint64_t* ctlInjected);
