// ============================================================================
// runtime/src/system/allocator.cpp — System::MemoryAllocator (Wave 2, lane C)
// ============================================================================
//
// Retail: Runtime-x64.dll, image base 0x180000000. The complete reverse
// engineering — tag bit layout, bins, the free-list encoding, the arena
// lifecycle — is in runtime/types/system/MemoryAllocator.h. Read that first;
// this file is the transcription and the shadow oracle, not the explanation.
//
// Address map (all verified against the export table with pefile, not guessed):
//
//   0x1800c6c00  ?Free@MemoryAllocator@System@@QEAAXPEAX@Z
//   0x1800c6e60  ?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z
//   0x1800c71d0  ?GetAllocationSize@…@@QEAA_KPEAX@Z
//   0x1800c71f0  ?GetMemoryRange@…@@AEAAPEAUMemoryRange@12@PEAX@Z
//   0x1800c7230  ?GetFreeTagForSize@…@@AEAAPEAUFreeTag@12@_K@Z
//   0x1800c7310  ?LinkFreeTag@…@@AEAAXPEAUMemoryTag@12@@Z
//   0x1800c7410  ?UnlinkFreeTag@…@@AEAAXPEAUFreeTag@12@0@Z
//   0x1800c7580  ?UnlinkFreeTag@…@@AEAAXPEAUMemoryTag@12@@Z
//   0x1800c75b0  ?Shutdown@…@@QEAAXXZ
//   0x1800c7620  ?Startup@…@@QEAA_N_K0@Z
//   0x1800c7690  ?AddNewMemoryRange@…@@AEAA_N_K@Z
//   0x1800c77e0  ?ExpandMemoryRangeTable@…@@AEAA_NXZ
//   0x1800c78d0  ?GetBinForSize@…@@AEBAH_K@Z
//   0x1800c7920  ?GetBinForLargeSize@…@@AEBAH_K@Z
//   0x1800c7960  ??1MemoryAllocator@System@@QEAA@XZ
//   0x1800c79a0  ??0MemoryAllocator@System@@QEAA@XZ
//   0x1800c7a50  ?MemAlloc@…@@SAPEAX_KH@Z
//   0x1800c7ad0  ?MemFree@…@@SAXPEAX@Z
//   0x1800c7b30  ?PlatformConstruct   0x1800c7b50 ?PlatformDestruct
//   0x1800c7b60  ?PlatformAllocate    0x1800c7bb0 ?PlatformFree
//   0x1800c7bd0  ?CommitToPage        0x1800c7c40 ?PlatformStartup
//   0x1800c7c70  ?PlatformShutdown    0x1800c7c80 ?ReleaseToPage
//   0x1800534e0  ?Get@…@@SAPEAV12@XZ
//   0x18007c0b0  ?GetAllocatedSize@…@@QEBA_JXZ
//   0x18007c0c0  ??4MemoryAllocator@System@@QEAAAEAV01@AEBV01@@Z   (memcpy 0x670)
//   0x180034d60  FUN_180034d60 — SetPrevTagInfo, an out-of-line private helper
//   0x180034e20  ?PointerOffset@…@@CA_JPEAX0@Z  (`return a - b`)
//   0x1801fc718  ?_singleton@MemoryAllocator@System@@0PEAV12@EA
//   0x1801ff300  file-static: largest `size` ever passed to Allocate
//
// ---------------------------------------------------------------------------
// WHY THIS LANE IS OPT-IN AND SHIPS SWITCHED OFF
// ---------------------------------------------------------------------------
// RegisterRuntimeExport redirects OUR ~1,180 call sites and nothing else. The
// DLL's own internal calls to MemAlloc/Free are direct rel32 CALLs. Registering
// MemAlloc without detouring Allocate/Free would mean: we hand out blocks from
// our arena, the DLL frees them into its own boundary tags, and the corruption
// is silent and total. So RtSystemAllocatorRegister() below REFUSES to register
// anything unless it can perform the ATOMIC SWAP — detour Allocate and Free so
// every caller in the process comes to us, in the same breath as the export
// registration. Two host-installed seams are required for that, and neither
// exists in the tree yet, so today this function logs why and returns 0.
//
// ---------------------------------------------------------------------------
// FAITHFULNESS NOTES (things a "cleanup" would break)
// ---------------------------------------------------------------------------
//  * `align8 - 8` is SIGN-extended to 64 bits (CDQE at 0x1800c6ea1). align == 0
//    therefore SUBTRACTS 8 from the request; it does not add 4 GB.
//  * The alignment padding is computed with a 64-bit DIV but a 32-bit IMUL/SUB
//    (0x1800c6f7d..0x1800c6f80). The truncation is reproduced.
//  * Allocate's and Free's range lookup is an inlined copy of GetMemoryRange
//    that returns null on miss and is then dereferenced unguarded. Retail does
//    not check it and neither do we (CLAUDE.md: do not "fix" a faithful crash).
//  * Free decrements _allocCount/_allocatedBytes BEFORE coalescing.
//  * GetBinForLargeSize' bit-scan loop is transcribed literally rather than
//    replaced with a clz, so no boundary case can disagree between builds.
//  * No exceptions anywhere (PLAN.md §4.3 risk 5).
// ============================================================================

#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/MemoryAllocator.h"

using rt::system::MemoryAllocator;
using rt::system::MemoryRange;
using rt::system::RtSysMemPlatform;
using rt::system::MemoryTag;
using rt::system::FreeTag;
using rt::system::kTagSizeMask;
using rt::system::kTagPrevSizeMask;
using rt::system::kTagPrevFree;
using rt::system::kTagThisFree;
using rt::system::kTagPrevHiMask;
using rt::system::kSizeBytesMask;
using rt::system::kThisFreeShl2;

// ---------------------------------------------------------------------------
// Platform seam (see MemoryAllocator.h). Null until the host installs it.
// ---------------------------------------------------------------------------
// Deliberately NOT in an anonymous namespace: allocator_win.cpp's
// PlatformAllocate/PlatformFree reach the same table, because retail's
// Platform* bodies ARE this seam rather than a layer sitting above it.
RtSysMemPlatform g_plat = {};
bool             g_platInstalled = false;

extern "C" void RtSysMemPlatformInstall(const RtSysMemPlatform* p) {
    if (!p) { g_platInstalled = false; return; }
    g_plat = *p;
    g_platInstalled = p->reserve && p->reserveCommit && p->commit && p->decommit &&
                      p->release && p->pageSize && p->lockInit && p->lockEnter &&
                      p->lockLeave && p->lockDelete;
}

// ---------------------------------------------------------------------------
// Tag access helpers. Every offset in this allocator is a BYTE offset; the tag
// size field is in 4-byte units.
// ---------------------------------------------------------------------------
namespace {

inline uint64_t& Tag(uint8_t* t)             { return *reinterpret_cast<uint64_t*>(t); }
inline uint64_t  TagC(const uint8_t* t)      { return *reinterpret_cast<const uint64_t*>(t); }
inline uint64_t& Node(uint8_t* freeTag)      { return *reinterpret_cast<uint64_t*>(freeTag); }

// The file-static at Runtime 0x1801ff300: largest `size` ever requested.
// Nothing in retail reads it; it is maintained because it is an observable
// store the shadow oracle can compare.
uint64_t g_largestRequestedSize = 0;

// Retail's fatal path. This comment used to end "after cutover this becomes our
// own Error class" -- Wave 9 lane ENG, iteration 3, is when that happened, and
// the door had to change with it.
//
// `?SetText@` now goes through the PORT-AWARE `ResolveExport`. It was
// `RtOriginal<>` (GetProcAddress, registry-bypassing), which was correct while
// nothing was ported and became a live BYPASS the moment
// runtime/src/system/error.cpp registered the name -- caught by
// tools/port_bypass_audit.py in the same commit that created it
// (`BYPASS ... allocator.cpp:128 ?SetText@Error@System@@ [live path]`), which is
// exactly the class reference_bypassed_port_needs_a_source_checker.md exists for.
//
// `?Display@` STAYS a raw door on purpose. It has no port
// (error.cpp declines it: nothing on any path this wave reaches calls it, since
// Engine::Startup only ever passes stopApplication=false). Converting a door on
// a symbol with NO port changes only which residue list it prints in
// (reference_converting_a_door_is_not_satisfying_it.md) -- it would still be
// null at cutover, and the `if (display)` below already handles that.
typedef void (*ErrorSetTextFn)(const char* text, bool stopApplication);
typedef void (*ErrorDisplayFn)();

void FatalOutOfMemory(const char* text, bool stopApplication) {
    ErrorSetTextFn setText =
        reinterpret_cast<ErrorSetTextFn>(ResolveExport("?SetText@Error@System@@SAXPEBD_N@Z"));
    ErrorDisplayFn display = RtOriginal<ErrorDisplayFn>("?Display@Error@System@@SAXXZ");
    if (setText) setText(text, stopApplication);
    if (stopApplication && display) display();
    if (!setText) {
        fprintf(stderr, "[RtAlloc] FATAL: %s\n", text);
        std::fflush(stderr);
    }
}

// ---------------------------------------------------------------------------
// GetBinForLargeSize (0x1800c7920) / GetBinForSize (0x1800c78d0)
// ---------------------------------------------------------------------------
int GetBinForLargeSize(uint64_t n) {
    int shift = 0x20;
    int bits  = 0x40;
    do {
        uint64_t v = n >> shift;
        if (v != 0) { bits -= shift; n = v; }
        shift >>= 1;
    } while (shift != 0);
    return static_cast<int>(n) - bits + 0xb6;
}

int GetBinForSize(uint64_t n) {
    if (n < 0x201) return static_cast<int>(static_cast<int32_t>(n)) >> 2;
    return GetBinForLargeSize(n);
}

// ---------------------------------------------------------------------------
// FUN_180034d60 — record "my previous block is free, and here is its size".
// Called on the tag that FOLLOWS a newly created free block.
// ---------------------------------------------------------------------------
void SetPrevTagInfo(uint8_t* next, const uint8_t* prev) {
    Tag(next) |= kTagPrevFree;
    uint64_t v = Tag(next);
    if ((TagC(prev) & kTagSizeMask) < 5) {
        uint64_t enc = static_cast<uint64_t>((next - prev) >> 2) << 41;
        Tag(next) = ((enc ^ v) & kTagPrevSizeMask) ^ v;
    } else {
        *reinterpret_cast<int64_t*>(next - 8) = next - prev;
        Tag(next) = v & ~kTagPrevSizeMask;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// The ported class. Free functions with the retail raw ABI (`self` explicit),
// exactly like Wave 1 — never C++ methods, so nothing can add a hidden this
// adjustment or a return slot we did not ask for.
// ---------------------------------------------------------------------------
namespace rtalloc {

// The parameter types are the ones the DLL's own mangled signatures name:
// PEAUMemoryTag@12@ and PEAUFreeTag@12@. A FreeTag* is the USER pointer of a
// free block (tag + 8); a MemoryTag* is the block header itself.
void  LinkFreeTag(MemoryAllocator* self, MemoryTag* tagp);
void  UnlinkFreeTag(MemoryAllocator* self, FreeTag* nodep, FreeTag* unused);
void  UnlinkFreeTagT(MemoryAllocator* self, MemoryTag* tagp);
void  CommitToPage(MemoryAllocator* self, MemoryRange* range, void* upTo);
void  ReleaseToPage(MemoryAllocator* self, MemoryRange* range, void* downTo);
bool  ExpandMemoryRangeTable(MemoryAllocator* self);
bool  AddNewMemoryRange(MemoryAllocator* self, uint64_t want);
FreeTag* GetFreeTagForSize(MemoryAllocator* self, uint64_t want);

inline MemoryTag* AsTag(uint8_t* p)  { return reinterpret_cast<MemoryTag*>(p); }
inline FreeTag*   AsNode(uint8_t* p) { return reinterpret_cast<FreeTag*>(p); }
void* Allocate(MemoryAllocator* self, uint64_t size, int align);
void  Free(MemoryAllocator* self, void* p);

// --- GetMemoryRange (0x1800c71f0) ------------------------------------------
MemoryRange* GetMemoryRange(MemoryAllocator* self, void* p) {
    MemoryRange* r = self->ranges;
    for (int i = 0; i < self->rangeCount; ++i, ++r) {
        if (r->base <= static_cast<uint8_t*>(p) && static_cast<uint8_t*>(p) < r->end)
            return r;
    }
    return nullptr;
}

// --- GetAllocationSize (0x1800c71d0) ---------------------------------------
uint64_t GetAllocationSize(MemoryAllocator* /*self*/, void* p) {
    return (*reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(p) - 8) & kTagSizeMask) << 2;
}

// --- GetAllocatedSize (0x18007c0b0) ----------------------------------------
int64_t GetAllocatedSize(const MemoryAllocator* self) { return self->allocatedBytes; }

// --- GetFreeTagForSize (0x1800c7230) ---------------------------------------
// Returns a FreeTag* (== tag + 8), i.e. the USER pointer of a free block whose
// byte size is >= `want`. First fit within a bin, then the next bin up.
FreeTag* GetFreeTagForSize(MemoryAllocator* self, uint64_t want) {
    int bin = GetBinForSize(want);
    if (bin < self->minBin) bin = self->minBin;

    for (; bin < 0xc0; ++bin) {
        uint8_t* cur = reinterpret_cast<uint8_t*>(self->freeLists[bin]);
        if (cur == nullptr) continue;
        for (;;) {
            if (((TagC(cur - 8) & kTagSizeMask) << 2) >= want) return AsNode(cur);
            int64_t next = static_cast<int64_t>(Node(cur) << 23) >> 23;
            if (next == 0) break;
            cur = cur + next;
            if (cur == nullptr) break;   // retail tests this; unreachable in practice
        }
    }
    return nullptr;
}

// --- LinkFreeTag (0x1800c7310) ---------------------------------------------
// Push `tag` onto the head of its bin. Blocks under 16 bytes are never linked.
void LinkFreeTag(MemoryAllocator* self, MemoryTag* tagp) {
    uint8_t* tag = reinterpret_cast<uint8_t*>(tagp);
    uint64_t bytes = (Tag(tag) & kTagSizeMask) * 4;
    if (bytes < 0x10) return;

    uint8_t* node = tag + 8;
    int bin = GetBinForSize(bytes);
    uint64_t* head = &self->freeLists[bin];

    if (*head == 0) {
        Node(node) = 0;
        Tag(tag) &= 0x00003fffffffffffull;          // clear the prev-link high bits
        if (bin < self->minBin) self->minBin = bin;
    } else {
        uint8_t* oldHead = reinterpret_cast<uint8_t*>(*head);
        Node(node) = static_cast<uint64_t>(oldHead - node) & kTagSizeMask;
        Tag(tag) &= 0x00003fffffffffffull;

        uint64_t back = static_cast<uint64_t>(node - oldHead);
        Node(oldHead) &= kTagSizeMask;
        Node(oldHead) |= back << 41;
        Tag(oldHead - 8) &= 0x00003fffffffffffull;
        Tag(oldHead - 8) |= (back & 0xffffffffff800000ull) << 23;
    }
    *head = reinterpret_cast<uint64_t>(node);
    self->freeTagCount++;
}

// --- UnlinkFreeTag(FreeTag*, FreeTag*) (0x1800c7410) -----------------------
// The second parameter is unused by retail; kept for ABI shape.
void UnlinkFreeTag(MemoryAllocator* self, FreeTag* nodep, FreeTag* /*unused*/) {
    uint8_t* node = reinterpret_cast<uint8_t*>(nodep);
    uint64_t bytes = (TagC(node - 8) & kTagSizeMask) * 4;
    int bin = GetBinForSize(bytes);

    const uint64_t tagv  = Tag(node - 8);
    const uint64_t nodev = Node(node);

    uint64_t pdelta = (static_cast<uint64_t>(static_cast<int64_t>(tagv) >> 23) &
                       0xffffffffff800000ull) | (nodev >> 41);
    uint8_t* prev = pdelta ? node + static_cast<int64_t>(pdelta) : nullptr;

    int64_t ndelta = static_cast<int64_t>(nodev << 23) >> 23;
    uint8_t* next = ndelta ? node + ndelta : nullptr;

    if (prev == nullptr) {
        if (next == nullptr) {
            self->freeLists[bin] = 0;
        } else {
            self->freeLists[bin] = reinterpret_cast<uint64_t>(next);
            Node(next) &= kTagSizeMask;
            Tag(next - 8) &= 0x00003fffffffffffull;
        }
    } else {
        Node(prev) &= 0xfffffe0000000000ull;        // drop prev's next field
        if (next != nullptr) {
            Node(prev) |= static_cast<uint64_t>(next - prev) & kTagSizeMask;
            uint64_t back = static_cast<uint64_t>(prev - next);
            Node(next) &= kTagSizeMask;
            Node(next) |= back << 41;
            Tag(next - 8) &= 0x00003fffffffffffull;
            Tag(next - 8) |= (back & 0xffffffffff800000ull) << 23;
        }
    }

    Node(node) = 0;
    Tag(node - 8) &= 0x00003fffffffffffull;

    int cursor = self->minBin;
    if (bin == cursor && self->freeLists[bin] == 0) {
        while (cursor < 0xc0 && self->freeLists[self->minBin] == 0) {
            cursor = self->minBin + 1;
            self->minBin = cursor;
        }
    }
    self->freeTagCount--;
}

// --- UnlinkFreeTag(MemoryTag*) (0x1800c7580) -------------------------------
void UnlinkFreeTagT(MemoryAllocator* self, MemoryTag* tagp) {
    uint8_t* tag = reinterpret_cast<uint8_t*>(tagp);
    if (3 < (Tag(tag) & kTagSizeMask)) UnlinkFreeTag(self, AsNode(tag + 8), nullptr);
}

// --- CommitToPage (0x1800c7bd0) --------------------------------------------
void CommitToPage(MemoryAllocator* self, MemoryRange* range, void* upTo) {
    uint64_t page = self->pageSize;
    uint64_t have = range->committed;
    uint64_t need = ((page - reinterpret_cast<uint64_t>(range->base) - 1 +
                      reinterpret_cast<uint64_t>(upTo)) / page) * page;
    if (have <= need && need - have != 0) {
        g_plat.commit(range->base + have, need - have);
        range->committed = need;
    }
}

// --- ReleaseToPage (0x1800c7c80) -------------------------------------------
void ReleaseToPage(MemoryAllocator* self, MemoryRange* range, void* downTo) {
    uint64_t page = self->pageSize;
    uint64_t need = ((page - reinterpret_cast<uint64_t>(range->base) - 1 +
                      reinterpret_cast<uint64_t>(downTo)) / page) * page;
    if (need < range->committed) {
        g_plat.decommit(range->base + need, range->committed - need);
        range->committed = need;
    }
}

// --- ExpandMemoryRangeTable (0x1800c77e0) ----------------------------------
// Grows the range table by TWO entries at a time, into its own committed
// VirtualAlloc reservation — never through the arena itself.
bool ExpandMemoryRangeTable(MemoryAllocator* self) {
    uint64_t page   = self->pageSize;
    int      oldCap = self->rangeCapacity;
    void*    src    = self->ranges;

    self->rangeCapacity = oldCap + 2;
    uint64_t bytes = ((page - 1 + static_cast<uint64_t>(oldCap + 2) * 0x18) / page) * page;
    void* dst = g_plat.reserveCommit(nullptr, bytes);
    self->ranges = static_cast<MemoryRange*>(dst);
    if (dst == nullptr) return false;

    std::memcpy(dst, src, static_cast<size_t>(oldCap) * 0x18);
    std::memset(static_cast<uint8_t*>(dst) + static_cast<size_t>(oldCap) * 0x18, 0,
                static_cast<size_t>(self->rangeCapacity - oldCap) * 0x18);
    if (oldCap != 0) g_plat.release(src);
    return true;
}

// --- AddNewMemoryRange (0x1800c7690) ---------------------------------------
bool AddNewMemoryRange(MemoryAllocator* self, uint64_t want) {
    uint64_t size = self->minRangeSize;
    if (size < want) size = want;

    if (self->maxTotalSize != 0 && self->maxTotalSize < size + self->totalReserved)
        return false;

    if (self->rangeCount == self->rangeCapacity && !ExpandMemoryRangeTable(self))
        return false;

    MemoryRange* range = self->ranges + self->rangeCount;
    uint64_t page = self->pageSize;
    self->rangeCount++;

    uint8_t* base = static_cast<uint8_t*>(g_plat.reserve(nullptr, ((page - 1 + size) / page) * page));
    range->base = base;
    if (base != nullptr) {
        range->committed = 0;
        range->end = base + size;
        CommitToPage(self, range, base + 0x10);
        Tag(base) = (((size & kSizeBytesMask) | kThisFreeShl2) >> 2) |
                    (Tag(base) & 0xffffe00000000000ull);
        Node(base + 8) = 0;
        Tag(base) &= 0x00003fffffffffffull;
        LinkFreeTag(self, AsTag(base));
        self->totalReserved += size;
    } else {
        FatalOutOfMemory("Memory: The application failed to allocate a memory range!", false);
    }
    return base != nullptr;
}

// --- Allocate (0x1800c6e60) ------------------------------------------------
void* Allocate(MemoryAllocator* self, uint64_t size, int align) {
    if (g_largestRequestedSize < size) g_largestRequestedSize = size;

    const uint32_t align8 = (static_cast<uint32_t>(align) + 7u) & 0xfffffff8u;

    // CDQE: (align8 - 8) is SIGN-extended before the add. align == 0 subtracts 8.
    uint64_t total = static_cast<uint64_t>(static_cast<int64_t>(
                         static_cast<int32_t>(align8 - 8u))) + ((size + 7) & ~7ull);
    if (total < 8) total = 8;
    uint64_t want = total + 8;

    FreeTag* freeTag = GetFreeTagForSize(self, want);
    if (freeTag == nullptr) {
        AddNewMemoryRange(self, want);
        freeTag = GetFreeTagForSize(self, want);
        if (freeTag == nullptr) {
            FatalOutOfMemory(
                "Failed to allocate memory. Either too many packages (mods) are loaded, "
                "no more memory is available, or an error has occured.", true);
        }
    }
    UnlinkFreeTag(self, freeTag, nullptr);

    uint8_t* tag = reinterpret_cast<uint8_t*>(freeTag) - 8;

    // Inlined GetMemoryRange. Retail does not guard the miss and neither do we.
    MemoryRange* range = nullptr;
    {
        MemoryRange* r = self->ranges;
        for (int i = 0; i < self->rangeCount; ++i, ++r) {
            if (r->base <= tag && tag < r->end) { range = r; break; }
        }
    }

    // Alignment padding: 64-bit DIV, 32-bit IMUL/SUB. The truncation is retail's.
    uint8_t* user = tag + 8;
    uint64_t quot = (static_cast<uint64_t>(static_cast<int64_t>(
                         static_cast<int32_t>(align8 - 1u))) +
                     reinterpret_cast<uint64_t>(user)) /
                    static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(align8)));
    int32_t pad = static_cast<int32_t>(static_cast<uint32_t>(quot) * align8) -
                  static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uint64_t>(user)));

    if (pad == 0) {
        Tag(tag) &= 0xffffc1ffffffffffull;   // clear prevSize, PREV_FREE, THIS_FREE
    } else {
        const uint64_t orig     = Tag(tag);
        const uint64_t origBytes = (orig & kTagSizeMask) * 4;
        const int64_t  padS     = pad;       // MOVSXD

        // The head of the block becomes a `pad`-byte free block …
        Tag(tag) = (((static_cast<uint64_t>(padS) & kSizeBytesMask) | kThisFreeShl2) >> 2) |
                   (orig & 0xffffe00000000000ull);
        LinkFreeTag(self, AsTag(tag));

        // … and the aligned remainder becomes the block we hand out.
        uint8_t* newTag = tag + (Tag(tag) & kTagSizeMask) * 4;
        Tag(newTag) &= kTagPrevHiMask;
        Tag(newTag) |= ((origBytes - static_cast<uint64_t>(padS)) >> 2) & kTagSizeMask;
        SetPrevTagInfo(newTag, tag);
        Tag(newTag) |= kTagPrevFree;
        want -= static_cast<uint64_t>(padS);
        tag = newTag;
    }

    const uint64_t tv     = Tag(tag);
    const uint64_t sizeQ  = tv & kTagSizeMask;

    if (sizeQ * 4 - want < 0x10) {
        // Take the whole block: the remainder would be unusable.
        uint8_t* next = tag + sizeQ * 4;
        if (next != range->end && (Tag(next) & kTagPrevFree) != 0)
            Tag(next) &= ~kTagPrevFree;
    } else {
        // Split. The XOR dance is retail's way of writing the size field.
        uint64_t nv = (((want >> 2) ^ tv) & kTagSizeMask) ^ tv;
        Tag(tag) = nv;
        uint64_t nq = nv & kTagSizeMask;
        CommitToPage(self, range, tag + (nq + 4) * 4);

        uint8_t* rem = tag + nq * 4;
        uint64_t remBytes = (sizeQ * 4 - (Tag(tag) & kTagSizeMask) * 4) & kSizeBytesMask;
        Tag(rem) = ((remBytes | kThisFreeShl2) >> 2) | (Tag(rem) & 0xffffe00000000000ull);

        uint8_t* next = rem + (Tag(rem) & kTagSizeMask) * 4;
        if (next != range->end) {
            uint64_t nextv = Tag(next);
            if ((nextv & kTagPrevFree) != 0) {
                uint64_t s = nextv | kTagPrevFree;
                Tag(next) = s;
                if ((Tag(rem) & kTagSizeMask) < 5) {
                    uint64_t enc = static_cast<uint64_t>((next - rem) >> 2) << 41;
                    Tag(next) = ((enc ^ s) & kTagPrevSizeMask) ^ s;
                } else {
                    *reinterpret_cast<int64_t*>(next - 8) = next - rem;
                    Tag(next) = nextv & ~kTagPrevSizeMask;
                }
            }
        }
        LinkFreeTag(self, AsTag(rem));
    }

    // Inlined CommitToPage over the block we are handing out.
    {
        uint64_t page = self->pageSize;
        uint64_t have = range->committed;
        uint64_t need = (((Tag(tag) & kTagSizeMask) * 4 -
                          reinterpret_cast<uint64_t>(range->base) - 1 + page +
                          reinterpret_cast<uint64_t>(tag)) / page) * page;
        if (have <= need && need - have != 0) {
            g_plat.commit(range->base + have, need - have);
            range->committed = need;
        }
    }

    self->allocatedBytes += static_cast<int64_t>((Tag(tag) & kTagSizeMask) * 4);
    self->allocCount++;
    return tag + 8;
}

// --- Free (0x1800c6c00) ----------------------------------------------------
void Free(MemoryAllocator* self, void* p) {
    if (p == nullptr) return;

    uint8_t* tag = static_cast<uint8_t*>(p) - 8;

    MemoryRange* range = nullptr;
    {
        MemoryRange* r = self->ranges;
        for (int i = 0; i < self->rangeCount; ++i, ++r) {
            if (r->base <= tag && tag < r->end) { range = r; break; }
        }
    }

    self->allocCount--;
    self->allocatedBytes -= static_cast<int64_t>((Tag(tag) & kTagSizeMask) * 4);

    uint64_t tv  = Tag(tag);
    uint8_t* blk = tag;

    // Backward coalesce.
    if ((tv & kTagPrevFree) != 0) {
        uint64_t back;
        if ((tv & kTagPrevSizeMask) == 0)
            back = *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(p) - 0x10);  // footer
        else
            back = static_cast<uint64_t>(static_cast<uint32_t>(tv >> 0x27) & 0x1c);

        blk = tag - back;
        if (3 < (Tag(blk) & kTagSizeMask)) UnlinkFreeTag(self, AsNode(blk + 8), nullptr);
        uint64_t pv = Tag(blk);
        Tag(blk) = ((((Tag(tag) + pv) & 0x3fffffffffffffffull) ^ pv) & kTagSizeMask) ^ pv;
    }

    uint64_t bv   = Tag(blk);
    uint8_t* next = blk + (bv & kTagSizeMask) * 4;

    if (next != range->end) {
        uint64_t nv = Tag(next);
        if ((nv & kTagThisFree) == 0) {
            // Neighbour is live: just tell it its predecessor is free now.
            uint64_t s = nv | kTagPrevFree;
            Tag(next) = s;
            if ((Tag(blk) & kTagSizeMask) < 5) {
                uint64_t enc = static_cast<uint64_t>((next - blk) >> 2) << 41;
                Tag(next) = ((enc ^ s) & kTagPrevSizeMask) ^ s;
            } else {
                *reinterpret_cast<int64_t*>(next - 8) = next - blk;
                Tag(next) = (nv & ~kTagPrevSizeMask) | kTagPrevFree;
            }
        } else {
            // Forward coalesce, then repeat the bookkeeping on the block after.
            Tag(blk) = ((((nv + bv) & 0x3fffffffffffffffull) ^ bv) & kTagSizeMask) ^ bv;
            if (3 < (Tag(next) & kTagSizeMask)) UnlinkFreeTag(self, AsNode(next + 8), nullptr);
            next = next + (Tag(next) & kTagSizeMask) * 4;
            if (next != range->end) {
                uint64_t n2 = Tag(next);
                if ((n2 & kTagPrevFree) != 0) {
                    uint64_t s = n2 | kTagPrevFree;
                    Tag(next) = s;
                    if ((Tag(blk) & kTagSizeMask) < 5) {
                        uint64_t enc = static_cast<uint64_t>((next - blk) >> 2) << 41;
                        Tag(next) = ((enc ^ s) & kTagPrevSizeMask) ^ s;
                    } else {
                        *reinterpret_cast<int64_t*>(next - 8) = next - blk;
                        Tag(next) = (n2 & ~kTagPrevSizeMask) | kTagPrevFree;
                    }
                }
            }
        }
    }

    if (blk + (Tag(blk) & kTagSizeMask) * 4 == range->end)
        ReleaseToPage(self, range, blk + 0x10);

    Tag(blk) = (Tag(blk) & 0xffffe1ffffffffffull) | kTagThisFree;
    LinkFreeTag(self, AsTag(blk));
}

// --- Startup (0x1800c7620) -------------------------------------------------
bool Startup(MemoryAllocator* self, uint64_t minRangeMB, uint64_t maxTotalMB) {
    self->minRangeSize = minRangeMB << 20;
    self->maxTotalSize = maxTotalMB << 20;
    self->pageSize = g_plat.pageSize();
    if (!AddNewMemoryRange(self, 0)) {
        FatalOutOfMemory("Memory: The application failed to start the memory allocator!", false);
        return false;
    }
    return true;
}

// --- Shutdown (0x1800c75b0) ------------------------------------------------
void Shutdown(MemoryAllocator* self) {
    for (int i = 0; i < self->rangeCount; ++i) g_plat.release(self->ranges[i].base);
    g_plat.release(self->ranges);
}

// --- ctor (0x1800c79a0) / dtor (0x1800c7960) -------------------------------
// _singleton is the DLL's exported data slot while the DLL is loaded; after
// cutover it becomes ours. Both are reached through SingletonSlot() below.
MemoryAllocator** SingletonSlot();

void Construct(MemoryAllocator* self) {
    self->ranges = nullptr;
    self->rangeCount = 0;
    self->rangeCapacity = 0;
    self->minRangeSize = 0;
    self->maxTotalSize = 0;
    self->totalReserved = 0;
    self->minBin = 0xbf;
    self->allocatedBytes = 0;
    self->allocCount = 0;
    self->freeTagCount = 0;
    g_plat.lockInit(self->lock);
    std::memset(self->freeLists, 0, sizeof(self->freeLists));
    if (MemoryAllocator** slot = SingletonSlot()) *slot = self;
    self->pageSize = 0;                       // inlined PlatformConstruct
}

void Destruct(MemoryAllocator* self) {
    if (MemoryAllocator** slot = SingletonSlot()) *slot = nullptr;
    g_plat.lockDelete(self->lock);
}

// --- the _singleton slot ---------------------------------------------------
MemoryAllocator** SingletonSlot() {
    MemoryAllocator** slot = RT_DATA_SLOT(MemoryAllocator**,
        "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
    return slot;
}

// --- Get (0x1800534e0) -----------------------------------------------------
MemoryAllocator* Get() {
    MemoryAllocator** slot = SingletonSlot();
    return slot ? *slot : nullptr;
}

// --- MemAlloc (0x1800c7a50) / MemFree (0x1800c7ad0) ------------------------
void* MemAlloc(uint64_t size, int align) {
    MemoryAllocator* self = Get();
    g_plat.lockEnter(self->lock);
    void* r = Allocate(self, size, align);
    g_plat.lockLeave(self->lock);
    return r;
}

void MemFree(void* p) {
    if (p == nullptr) return;
    MemoryAllocator* self = Get();
    g_plat.lockEnter(self->lock);
    Free(self, p);
    g_plat.lockLeave(self->lock);
}

// --- GetBinForSize / GetBinForLargeSize, member-shaped (RCX = this) --------
// The file-scope helpers above take the size in RCX because that is what the
// port's own call sites need; the EXPORTS are const members and put `this`
// there. Registering the file-scope forms directly would shift every argument.
int GetBinForSizeM(const MemoryAllocator* /*self*/, uint64_t n)      { return GetBinForSize(n); }
int GetBinForLargeSizeM(const MemoryAllocator* /*self*/, uint64_t n) { return GetBinForLargeSize(n); }

// --- The Platform* family (0x1800c7b30 / b50 / b60 / bb0 / c40 / c70) ------
// These six live in runtime/src/system/allocator_win.cpp. They are the platform
// seam the ORIGINAL authors drew, and PLAN.md 4.1's `_win` rule falls on the
// same line, so the OS header goes there and nowhere else. Nothing in this file
// declares them, so nothing here can be tempted to reimplement them.

// --- PointerOffset (0x180034e20) -------------------------------------------
int64_t PointerOffset(void* a, void* b) {
    return static_cast<uint8_t*>(a) - static_cast<uint8_t*>(b);
}

}  // namespace rtalloc

// ============================================================================
// SHADOW ORACLE  (PLAN.md §4.3 risk 2, rt_diff.h "shadow mode")
// ============================================================================
//
// An allocator cannot be diffed the Wave 1 way: running both implementations
// mutates the heap twice. RT_DIFF_SHADOW's contract is "ours computes into a
// private replica, theirs executes for real, and the port asserts the replica
// still matches". Concretely, here:
//
//   * The replica is a SECOND, PRIVATE arena, reserved from the platform seam,
//     driven by OUR ported code and touched by nothing else in the process.
//   * The replica is armed at MemoryAllocator::Startup and given the SAME
//     (minRangeMB, maxTotalMB) — so both arenas begin in the identical virgin
//     state and every subsequent range is reserved in the same order at the
//     same size.
//   * Every Allocate the DLL performs is REPLAYED into the replica with the
//     same (size, align); every Free is replayed on the replica pointer that
//     corresponds to the freed DLL pointer (tracked in the map below, which
//     lives in its own VirtualAlloc reservation and never touches either heap).
//   * The invariant asserted per call is the tuple
//         (rangeIndex, offsetWithinRange, allocatedBytesDelta,
//          allocCount, freeTagCount, minBin)
//     Absolute addresses cannot match — the two arenas are at different
//     VirtualAlloc addresses — but the OFFSET must, because the arenas are
//     byte-identical in layout. That single comparison exercises the entire
//     machine: bin selection, the first-fit walk, the alignment split, the
//     tail split, forward and backward coalescing, and the free-list encoding.
//     If any of those diverged, the offsets desynchronise on a later call and
//     never recover.
//
// This proves the port without ever letting it own a byte the process uses.
// It requires the host to feed it the events, which is what
// RtAllocShadow{Startup,Alloc,Free} are for; the driver belongs in
// standalone/ (see docs/runtime_port/WAVE2_ALLOCATOR.md).
//
// STATUS: the machinery below is written and compiles. It HAS NOT RUN — the
// host-side driver and the platform seam do not exist in the tree yet, so no
// shadow numbers are reported anywhere in this lane. Do not read the presence
// of this code as evidence the port is verified.
// ============================================================================

namespace {

MemoryAllocator  g_shadow;             // the replica allocator object
bool             g_shadowArmed = false;
uint64_t         g_shadowCalls = 0;
uint64_t         g_shadowDivergences = 0;

// theirs -> ours pointer map, in its own reservation.
struct ShadowEntry { void* theirs; void* ours; };
const size_t kShadowSlots = 1u << 21;          // 2M slots, 32 MB
ShadowEntry* g_shadowMap = nullptr;

inline size_t ShadowHash(void* p) {
    uint64_t x = reinterpret_cast<uint64_t>(p) >> 4;
    x *= 0x9E3779B97F4A7C15ull; x ^= x >> 29;
    return static_cast<size_t>(x);
}

// PROBE DEFECT COUNTERS (Wave 9 lane E). A silently-dropped insert makes the
// matching Free unreplayable, which desynchronises the replica for a reason
// that has nothing to do with the port -- and the design's own "a divergence
// can never recover" property then turns one harness hiccup into a wall of
// red. Count both so a run can be told apart from a defect.
uint64_t g_shadowMapFull    = 0;   // insert found no free slot at all
uint64_t g_shadowFreeUnmapped = 0; // Free arrived for a pointer we never mapped

// Negative-control switches — see RtAllocShadowStartup. Off unless the env var
// asks for them; they exist to prove the oracle can fail.
bool g_shadowNegCtlMinBin = false;
bool g_shadowNegCtlSize   = false;
bool g_shadowNegCtlGrow   = false;

// ---------------------------------------------------------------------------
// ARENA-GROWTH COVERAGE (Wave 9 lane E3)
// ---------------------------------------------------------------------------
// WAVE9_LANE_E2.md §5.1: 12,750,873 shadowed operations ran with `ranges=1` on
// every cell, so `AddNewMemoryRange` called from inside `Allocate`, the
// range-table realloc-and-copy, the multi-range scans and every `rangeIndex!=0`
// comparison were UNEXERCISED. `divergences=0` over a path that never ran is
// the `calls=0`-reads-as-clean shape.
//
// These three count what actually happened rather than what was attempted, and
// are derived from state DELTAS observed in the shadow layer — nothing is added
// to the ported bodies, which stay byte-faithful:
//
//   grow       replica rangeCount increased during an Allocate  (the growth
//              path ran; the arming call is excluded by construction, it
//              happens before the first comparison)
//   tableCopy  replica rangeCapacity increased from a NON-ZERO capacity, i.e.
//              ExpandMemoryRangeTable's `oldCap != 0` branch: the memcpy of the
//              old table AND the release of it. Capacity goes 0->2->4->6, so
//              this needs a THIRD range, not a second.
//   crossRange comparisons whose block landed in a range with index > 0.
uint64_t g_shadowGrow       = 0;
uint64_t g_shadowTableCopy  = 0;
uint64_t g_shadowCrossRange = 0;

// LINEAR PROBING WITH BACKWARD-SHIFT DELETION (Knuth 6.4 algorithm R).
//
// Wave 9 lane E2. The first version emptied a slot by writing nullptr, which is
// the classic open-addressing defect: nullptr is also the probe terminator, so
// deleting an entry TRUNCATES the probe chain of every key that collided into a
// later slot. Those keys then become unfindable and their Free is dropped, and
// because a dropped Free makes the replica keep a block the real arena released,
// the two desynchronise by exactly one and — by this design's own "a divergence
// never recovers" property — every subsequent comparison is red.
//
// That is exactly what the second measurement showed: free_unmapped=207 and
// then 1,765,043 "divergences" whose first instance was a drift of ONE
// (ours live=13702 vs theirs live=13701) while the range offsets, which are the
// thing that actually proves the port, agreed to the byte. A harness bug wearing
// a catastrophic port defect's clothes.
//
// Backward-shift deletion keeps the no-tombstone invariant: after erasing slot
// i, later entries whose home slot is not cyclically inside (i, j] are shifted
// back so no chain is ever broken. No tombstones means the table cannot silt up
// over a run's ~900k insert/erase pairs either, which a tombstone scheme would.
void ShadowMapErase(size_t i) {
    const size_t mask = kShadowSlots - 1;
    size_t j = i;
    for (;;) {
        g_shadowMap[i].theirs = nullptr;
        g_shadowMap[i].ours   = nullptr;
        for (;;) {
            j = (j + 1) & mask;
            if (g_shadowMap[j].theirs == nullptr) return;   // chain ends: done
            const size_t h = ShadowHash(g_shadowMap[j].theirs) & mask;
            // Can g_shadowMap[j] move back to i? Only if its home slot h does
            // NOT lie cyclically within (i, j].
            const bool inside = (i <= j) ? (i < h && h <= j) : (i < h || h <= j);
            if (!inside) break;
        }
        g_shadowMap[i] = g_shadowMap[j];
        i = j;
    }
}

void ShadowMapInsert(void* theirs, void* ours) {
    if (!g_shadowMap || theirs == nullptr) return;   // nullptr is the empty marker
    const size_t mask = kShadowSlots - 1;
    size_t i = ShadowHash(theirs) & mask;
    for (size_t probe = 0; probe < kShadowSlots; probe++) {
        ShadowEntry& e = g_shadowMap[(i + probe) & mask];
        if (e.theirs == nullptr || e.theirs == theirs) { e.theirs = theirs; e.ours = ours; return; }
    }
    g_shadowMapFull++;   // table genuinely full — impossible at this load factor
}

void* ShadowMapTake(void* theirs) {
    if (!g_shadowMap || theirs == nullptr) return nullptr;
    const size_t mask = kShadowSlots - 1;
    size_t i = ShadowHash(theirs) & mask;
    for (size_t probe = 0; probe < kShadowSlots; probe++) {
        const size_t s = (i + probe) & mask;
        if (g_shadowMap[s].theirs == nullptr) return nullptr;   // not present
        if (g_shadowMap[s].theirs == theirs) {
            void* o = g_shadowMap[s].ours;
            ShadowMapErase(s);
            return o;
        }
    }
    return nullptr;
}

// (rangeIndex, offsetInRange) for a user pointer, or (-1, 0) if not in a range.
void RangeCoords(const MemoryAllocator* a, void* p, int* idx, uint64_t* off) {
    *idx = -1; *off = 0;
    uint8_t* t = static_cast<uint8_t*>(p) - 8;
    for (int i = 0; i < a->rangeCount; ++i) {
        if (a->ranges[i].base <= t && t < a->ranges[i].end) {
            *idx = i; *off = static_cast<uint64_t>(t - a->ranges[i].base); return;
        }
    }
}

}  // namespace

extern "C" bool RtAllocShadowStartup(uint64_t minRangeMB, uint64_t maxTotalMB) {
#if defined(_WIN32)
    if (!g_platInstalled) RtSystemAllocatorInstallPlatform();
#endif
    if (!g_platInstalled) {
        fprintf(stderr, "[RtAllocShadow] platform seam not installed — not arming\n");
        return false;
    }
    g_shadowMap = static_cast<ShadowEntry*>(
        g_plat.reserveCommit(nullptr, static_cast<uint64_t>(kShadowSlots) * sizeof(ShadowEntry)));
    if (!g_shadowMap) return false;
    std::memset(g_shadowMap, 0, kShadowSlots * sizeof(ShadowEntry));

    // Construct WITHOUT touching the real _singleton slot.
    std::memset(&g_shadow, 0, sizeof(g_shadow));
    g_shadow.minBin = 0xbf;

    // ---- NEGATIVE CONTROL (Wave 9 lane E2) --------------------------------
    // A probe that has never reported a divergence has not been shown to be
    // ABLE to. BANISHED_RT_ALLOC_SHADOW_NEGCTL deliberately breaks the replica
    // and the run must go red; if it does not, the green runs mean nothing.
    // Two modes, because the predicate has two independent halves and a
    // control that only exercises one is not a control for the other
    // (reference_shadow_predicate_must_name_its_failing_term.md):
    //   minbin — start the replica's search floor one bin low. Fires the
    //            COUNTER half (minBin/allocCount/freeTagCount/allocatedBytes).
    //   size   — inflate every 100,000th replica request by 8 bytes. Fires the
    //            GEOMETRY half (rangeIndex, offsetWithinRange), which is the
    //            term that actually proves the allocator's placement logic, and
    //            which no counter perturbation can reach.
    //   grow   — (lane E3) enlarge every range AFTER the first by one page.
    //            Fires the GROWTH half and NOTHING ELSE: range 0 is created
    //            below at retail's exact size before the perturbation is
    //            applied, so the replica is byte-identical until the first
    //            Allocate-driven AddNewMemoryRange — at which point
    //            `totalReserved` differs by 0x1000 on that very call and can
    //            never converge back (AddNewMemoryRange only ever adds to it),
    //            and `rangeIndex`/`offsetWithinRange` follow once the longer
    //            range 1 fills one allocation later than theirs.
    //
    //            It is deliberately SILENT when the growth path does not run.
    //            A green `=grow` run therefore means one of two things and the
    //            `grow=` counter is what tells them apart: grow=0 means the
    //            cell never reached the path (the run proves nothing), grow>0
    //            with divergences=0 means the ORACLE IS BLIND to growth and
    //            every green growth run must be thrown away.
    if (const char* nc = getenv("BANISHED_RT_ALLOC_SHADOW_NEGCTL")) {
        if (nc[0] == 'm') { g_shadowNegCtlMinBin = true; g_shadow.minBin = 0xbe; }
        else if (nc[0] == 's') { g_shadowNegCtlSize = true; }
        else if (nc[0] == 'g') { g_shadowNegCtlGrow = true; }
        fprintf(stderr, "[RtAllocShadow] NEGATIVE CONTROL ACTIVE (%s) — this run is "
                        "REQUIRED to go red; a clean result here invalidates every "
                        "other run.\n", nc);
    }
    g_plat.lockInit(g_shadow.lock);
    g_shadow.pageSize = 0;

    g_shadow.minRangeSize = minRangeMB << 20;
    g_shadow.maxTotalSize = maxTotalMB << 20;
    g_shadow.pageSize = g_plat.pageSize();
    if (!rtalloc::AddNewMemoryRange(&g_shadow, 0)) return false;

    // AFTER range 0 exists, so range 0 matches retail byte for byte and only
    // the growth path can ever notice. See the `grow` note above.
    if (g_shadowNegCtlGrow) g_shadow.minRangeSize += 0x1000;

    g_shadowArmed = true;
    fprintf(stderr, "[RtAllocShadow] armed (min=%lluMB max=%lluMB)\n",
                 (unsigned long long)minRangeMB, (unsigned long long)maxTotalMB);
    std::fflush(stderr);
    return true;
}

// Called AFTER the DLL's Allocate returned `theirs`, with `theirSelf` being the
// live allocator whose counters we compare against.
extern "C" void RtAllocShadowAlloc(uint64_t size, int align, void* theirs,
                                   const MemoryAllocator* theirSelf) {
    RT_DIFF_BEGIN("System::MemoryAllocator::Allocate [shadow]");
    if (!g_shadowArmed) return;

    uint64_t reqSize = size;
    if (g_shadowNegCtlSize && (g_shadowCalls % 100000) == 99999) reqSize = size + 8;

    // Growth coverage is measured as a state DELTA across the replica's own
    // Allocate, so nothing is added to the ported bodies.
    const int32_t preRanges = g_shadow.rangeCount;
    const int32_t preCap    = g_shadow.rangeCapacity;

    void* ours = rtalloc::Allocate(&g_shadow, reqSize, align);

    if (g_shadow.rangeCount   > preRanges) g_shadowGrow++;
    if (g_shadow.rangeCapacity > preCap && preCap != 0) g_shadowTableCopy++;

    ShadowMapInsert(theirs, ours);
    g_shadowCalls++;

    int ti, oi; uint64_t to, oo;
    RangeCoords(theirSelf, theirs, &ti, &to);
    RangeCoords(&g_shadow, ours,   &oi, &oo);
    if (oi > 0) g_shadowCrossRange++;

    // rangeCount and totalReserved joined the tuple in lane E3. Without them a
    // growth that happened at a DIFFERENT allocation, or reserved a DIFFERENT
    // number of bytes, is only caught indirectly and only once a later
    // allocation happens to land in the wrong range. They are the two fields
    // AddNewMemoryRange writes that nothing else in the tuple observes.
    const bool equal = (ti == oi) && (to == oo) &&
                       (g_shadow.allocCount    == theirSelf->allocCount) &&
                       (g_shadow.freeTagCount  == theirSelf->freeTagCount) &&
                       (g_shadow.allocatedBytes== theirSelf->allocatedBytes) &&
                       (g_shadow.minBin        == theirSelf->minBin) &&
                       (g_shadow.rangeCount    == theirSelf->rangeCount) &&
                       (g_shadow.totalReserved == theirSelf->totalReserved);
    if (!equal) g_shadowDivergences++;

    char a[160], b[160];
    std::snprintf(a, sizeof(a), "r%d+0x%llx live=%d free=%d bytes=%lld bin=%d nr=%d res=%llu",
                  oi, (unsigned long long)oo, g_shadow.allocCount, g_shadow.freeTagCount,
                  (long long)g_shadow.allocatedBytes, g_shadow.minBin,
                  g_shadow.rangeCount, (unsigned long long)g_shadow.totalReserved);
    std::snprintf(b, sizeof(b), "r%d+0x%llx live=%d free=%d bytes=%lld bin=%d nr=%d res=%llu",
                  ti, (unsigned long long)to, theirSelf->allocCount, theirSelf->freeTagCount,
                  (long long)theirSelf->allocatedBytes, theirSelf->minBin,
                  theirSelf->rangeCount, (unsigned long long)theirSelf->totalReserved);
    RT_DIFF_SHADOW(equal, a, b);
}

extern "C" void RtAllocShadowFree(void* theirs, const MemoryAllocator* theirSelf) {
    RT_DIFF_BEGIN("System::MemoryAllocator::Free [shadow]");
    if (!g_shadowArmed || theirs == nullptr) return;

    void* ours = ShadowMapTake(theirs);
    if (ours == nullptr) {               // allocated before we armed, or the map
        g_shadowFreeUnmapped++;          // dropped it -- either way, not comparable
        return;
    }
    rtalloc::Free(&g_shadow, ours);
    g_shadowCalls++;

    const bool equal = (g_shadow.allocCount     == theirSelf->allocCount) &&
                       (g_shadow.freeTagCount   == theirSelf->freeTagCount) &&
                       (g_shadow.allocatedBytes == theirSelf->allocatedBytes) &&
                       (g_shadow.minBin         == theirSelf->minBin) &&
                       (g_shadow.rangeCount     == theirSelf->rangeCount) &&
                       (g_shadow.totalReserved  == theirSelf->totalReserved);
    if (!equal) g_shadowDivergences++;

    char a[160], b[160];
    std::snprintf(a, sizeof(a), "live=%d free=%d bytes=%lld bin=%d nr=%d res=%llu",
                  g_shadow.allocCount, g_shadow.freeTagCount,
                  (long long)g_shadow.allocatedBytes, g_shadow.minBin,
                  g_shadow.rangeCount, (unsigned long long)g_shadow.totalReserved);
    std::snprintf(b, sizeof(b), "live=%d free=%d bytes=%lld bin=%d nr=%d res=%llu",
                  theirSelf->allocCount, theirSelf->freeTagCount,
                  (long long)theirSelf->allocatedBytes, theirSelf->minBin,
                  theirSelf->rangeCount, (unsigned long long)theirSelf->totalReserved);
    RT_DIFF_SHADOW(equal, a, b);
}

extern "C" void RtAllocShadowReport(uint64_t* calls, uint64_t* divergences) {
    if (calls) *calls = g_shadowCalls;
    if (divergences) *divergences = g_shadowDivergences;
}

// Harness-health counters, distinct from port divergences. A non-zero value in
// either means the MEASUREMENT is compromised, not that the port is wrong.
// COVERAGE CONTROL. `divergences=0` over N calls says nothing about the paths
// those N calls never took. The arena-growth path (AddNewMemoryRange ->
// ExpandMemoryRangeTable -> a second MemoryRange) runs at most a handful of
// times per run and is the one that would silently never execute. Reporting the
// replica's final range count and reserved bytes beside the real allocator's
// says whether it ran at all, and whether it agreed.
extern "C" void RtAllocShadowCoverage(int* ranges, uint64_t* reserved) {
    if (ranges)   *ranges   = g_shadow.rangeCount;
    if (reserved) *reserved = g_shadow.totalReserved;
}

// EFFECTIVENESS CONTROL for the arena-growth cell (lane E3). These count what
// the growth path actually DID, not how many requests were made at it:
//   grow > 0        the second range was created from inside Allocate
//   tableCopy > 0   ExpandMemoryRangeTable's memcpy+release branch ran (needs a
//                   THIRD range: capacity goes 0->2->4)
//   crossRange > 0  allocations were compared in a range with index != 0
// A growth cell whose `grow` is 0 measured nothing about growth however many
// millions of operations it compared.
extern "C" void RtAllocShadowGrowth(uint64_t* grow, uint64_t* tableCopy,
                                    uint64_t* crossRange) {
    if (grow)       *grow       = g_shadowGrow;
    if (tableCopy)  *tableCopy  = g_shadowTableCopy;
    if (crossRange) *crossRange = g_shadowCrossRange;
}

extern "C" void RtAllocShadowHealth(uint64_t* mapFull, uint64_t* freeUnmapped) {
    if (mapFull) *mapFull = g_shadowMapFull;
    if (freeUnmapped) *freeUnmapped = g_shadowFreeUnmapped;
}

// ============================================================================
// REGISTRATION — the atomic swap, and why it does not happen today
// ============================================================================
//
// The host must supply an installer that detours BOTH Allocate and Free before
// a single export is registered. That installer lives in standalone/ because
// standalone/runtime_detour.h pulls in <windows.h>, which PLAN.md §4.1 forbids
// under runtime/src. Until it is installed this function registers NOTHING.
// ============================================================================

namespace {

// ---------------------------------------------------------------------------
// Prologue guard — refuse to arm if anything has already patched the two
// functions we are about to detour.
// ---------------------------------------------------------------------------
// standalone/allocator_lock.cpp is DEFAULT-ON and patches exactly these two
// prologues (13 bytes at Allocate, 12 at Free) to add the critical section that
// retail's raw Allocate/Free do not take. In the current boot order
// RuntimePortInit() runs BEFORE InstallAllocatorLock(), so if we detoured first
// the lock installer's own memcmp guard would refuse and the ported allocator
// would run with NO LOCK AT ALL -- reintroducing the free-list race that lock
// was written to fix, announced by a single line in standalone_error.log.
//
// So: before touching anything, compare both prologues against the bytes the
// shipped DLL actually has. This is deliberately broader than "is the allocator
// lock installed" -- it catches ANY foreign patch on these two entry points,
// including a future one nobody has written yet, and it needs no cooperation
// from the patching file. Bytes taken from memory, not from a mnemonic listing:
// this build emits redundant REX prefixes, so PUSH RBX is `40 53`, not `53`.
//
// Retail Allocate (0x1800c6e60):
//   40 53          PUSH RBX          41 57       PUSH R15
//   57             PUSH RDI          48 83 EC 20 SUB RSP,0x20
//   41 54          PUSH R12
//   41 56          PUSH R14
// Retail Free (0x1800c6c00):
//   48 85 D2             TEST RDX,RDX
//   0F 84 4F 02 00 00    JZ Free+0x258      (rel32: same bytes under ASLR)
//   48 8B C4             MOV RAX,RSP
const uint8_t kAllocatePrologue[13] = {
    0x40,0x53,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x20
};
const uint8_t kFreePrologue[12] = {
    0x48,0x85,0xD2, 0x0F,0x84,0x4F,0x02,0x00,0x00, 0x48,0x8B,0xC4
};

// Returns nullptr when both prologues are untouched, else the name of the one
// that is not.
const char* FindPatchedPrologue() {
    const void* alloc = ResolveExportRaw("?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z");
    const void* freeF = ResolveExportRaw("?Free@MemoryAllocator@System@@QEAAXPEAX@Z");
    if (!alloc || !freeF) return "unresolved (Runtime-x64.dll not loaded?)";
    if (std::memcmp(alloc, kAllocatePrologue, sizeof(kAllocatePrologue)) != 0)
        return "MemoryAllocator::Allocate (0x1800c6e60)";
    if (std::memcmp(freeF, kFreePrologue, sizeof(kFreePrologue)) != 0)
        return "MemoryAllocator::Free (0x1800c6c00)";
    return nullptr;
}

RtAllocDetourInstaller g_installDetours = nullptr;
RtAllocCutoverArmer    g_armCutover     = nullptr;

// Which member of the platform seam is still null. Returns nullptr when the
// table is complete. Naming it matters: `g_platInstalled` is one bool over ten
// function pointers, and lane X spent a probe on a half-built seam because the
// refusal said only "not installed".
const char* PlatformSeamMissing() {
    if (!g_plat.reserve)       return "reserve (VirtualAlloc MEM_RESERVE)";
    if (!g_plat.reserveCommit) return "reserveCommit (VirtualAlloc RESERVE|COMMIT)";
    if (!g_plat.commit)        return "commit (VirtualAlloc MEM_COMMIT)";
    if (!g_plat.decommit)      return "decommit (VirtualFree MEM_DECOMMIT)";
    if (!g_plat.release)       return "release (VirtualFree MEM_RELEASE)";
    if (!g_plat.pageSize)      return "pageSize (GetSystemInfo)";
    if (!g_plat.lockInit)      return "lockInit (InitializeCriticalSectionAndSpinCount)";
    if (!g_plat.lockEnter)     return "lockEnter (EnterCriticalSection)";
    if (!g_plat.lockLeave)     return "lockLeave (LeaveCriticalSection)";
    if (!g_plat.lockDelete)    return "lockDelete (DeleteCriticalSection)";
    return nullptr;
}

// Is the shipped DLL there at all? Asked STRUCTURALLY rather than from an env
// var: both raw lookups resolving null means the module is not mapped.
// ResolveExportRaw deliberately bypasses the port registry, so this cannot be
// fooled by our own registrations.
bool RuntimeDllPresent() {
    return ResolveExportRaw("?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z") != nullptr ||
           ResolveExportRaw("?Free@MemoryAllocator@System@@QEAAXPEAX@Z") != nullptr;
}

// Defined below, after RtSystemAllocatorRegister, next to the export list it
// owns; both arming paths call it.
int RegisterAllocatorExports(void* allocateEntry, void* freeEntry);
}

extern "C" void RtSystemAllocatorSetDetourInstaller(RtAllocDetourInstaller fn) {
    g_installDetours = fn;
}

extern "C" void RtSystemAllocatorSetCutoverArmer(RtAllocCutoverArmer fn) {
    g_armCutover = fn;
}

int RtSystemAllocatorRegister() {
#if defined(_WIN32)
    if (!g_platInstalled) RtSystemAllocatorInstallPlatform();
#endif
    if (!g_platInstalled) {
        const char* missing = PlatformSeamMissing();
        printf("[RtPort] MemoryAllocator: REFUSING to arm -- platform seam incomplete, "
               "first missing entry is '%s' (runtime/src/system/allocator_win.cpp did "
               "not install it). A half-built seam is worse than none: rtalloc::Construct "
               "would call a null lockInit. See docs/runtime_port/WAVE2_ALLOCATOR.md\n",
               missing ? missing : "(table complete but g_platInstalled is false)");
        return 0;
    }

    // ---- CUTOVER: no DLL, so no detour is possible OR necessary -------------
    //
    // Everything below this block is DLL-present machinery. The detour exists
    // only to redirect the DLL's own internal callers; the prologue guard reads
    // the DLL's bytes. With the module gone, FindPatchedPrologue() resolves null
    // and reports "unresolved", so the guard REFUSES -- correct by its own logic
    // and wrong for the situation. That is exactly why BANISHED_RT_ALLOC=1 did
    // not take on the DLL-absent boot: the arming path was wired, and its safety
    // guard was the thing blocking it.
    //
    // With no DLL there is no second allocator, so the atomicity requirement
    // ("register nothing unless the swap took") has nothing left to protect
    // against: registering the exports IS the complete swap. The one thing that
    // does NOT come for free is the critical section, which normally arrives via
    // allocator_lock.cpp's patches or the composed detour stub -- neither of
    // which exists here. The host hands the composed (locked) entry points back
    // through the cutover armer, and they are what gets registered.
    if (!RuntimeDllPresent()) {
        if (g_armCutover == nullptr) {
            printf("[RtPort] MemoryAllocator: REFUSING to arm -- Runtime-x64.dll is absent "
                   "and no cutover armer is installed. Registering the raw bodies alone "
                   "would leave ?Allocate@/?Free@ running with NO critical section, which "
                   "is the free-list race allocator_lock.cpp exists to prevent. "
                   "standalone/main.cpp must call RtSystemAllocatorSetCutoverArmer().\n");
            return 0;
        }
        void* composedAlloc = nullptr;
        void* composedFree  = nullptr;
        if (!g_armCutover(reinterpret_cast<void*>(&rtalloc::Allocate),
                          reinterpret_cast<void*>(&rtalloc::Free),
                          &composedAlloc, &composedFree) ||
            composedAlloc == nullptr || composedFree == nullptr) {
            printf("[RtPort] MemoryAllocator: cutover armer REFUSED -- registering nothing\n");
            return 0;
        }
        const int n = RegisterAllocatorExports(composedAlloc, composedFree);
        printf("[RtAlloc] ARMED (cutover: Runtime-x64.dll absent) -- %d exports registered, "
               "no detour installed because there are no DLL-internal callers to redirect. "
               "?Allocate@/?Free@ point at the COMPOSED entry points, which take the "
               "allocator critical section.\n", n);
        return n;
    }

    // HARD REFUSE if anything already owns these two prologues. Proceeding would
    // silently drop the allocator lock; see the comment on FindPatchedPrologue.
    if (const char* patched = FindPatchedPrologue()) {
        printf("[RtPort] MemoryAllocator: REFUSING to arm -- %s is already "
                    "patched. standalone/allocator_lock.cpp is default-on and holds "
                    "these two prologues; detouring over it would leave the ported "
                    "allocator running WITHOUT the critical section that Runtime's "
                    "own direct calls to Free rely on. Retire that patch first "
                    "(docs/runtime_port/WAVE2_ALLOCATOR.md section 4).\n", patched);
        return 0;
    }

    if (g_installDetours == nullptr) {
        printf("[RtPort] MemoryAllocator: no detour installer -- registering the exports "
                    "alone would let us allocate while the DLL frees. REFUSING. "
                    "See docs/runtime_port/WAVE2_ALLOCATOR.md\n");
        return 0;
    }
    // Atomic swap: the DLL's own callers first, then ours. If the detours do
    // not take, register nothing at all.
    if (!g_installDetours(reinterpret_cast<void*>(&rtalloc::Allocate),
                          reinterpret_cast<void*>(&rtalloc::Free))) {
        printf("[RtPort] MemoryAllocator: detour install FAILED -- registering nothing\n");
        return 0;
    }

    const int n = RegisterAllocatorExports(reinterpret_cast<void*>(&rtalloc::Allocate),
                                           reinterpret_cast<void*>(&rtalloc::Free));
    printf("[RtAlloc] ARMED (atomic swap taken, Runtime-x64.dll present) -- %d exports "
           "registered; the DLL's own Allocate/Free now JMP to our composed stubs.\n", n);
    return n;
}

// The registration block, shared by both arming paths. `allocateEntry` and
// `freeEntry` are what the two MEMBER exports point at: the raw ported bodies on
// the DLL-present path (where the composed stub is reached through the detour,
// and RtAllocSwapPostRegister repoints these afterwards), or the composed locked
// entry points on the cutover path, where there is no detour and no
// allocator_lock patch, so the critical section has to come from here.
namespace {
int RegisterAllocatorExports(void* allocateEntry, void* freeEntry) {
    RegisterRuntimeExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z",
                          reinterpret_cast<void*>(&rtalloc::MemAlloc));
    RegisterRuntimeExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z",
                          reinterpret_cast<void*>(&rtalloc::MemFree));
    RegisterRuntimeExport("?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z", allocateEntry);
    RegisterRuntimeExport("?Free@MemoryAllocator@System@@QEAAXPEAX@Z", freeEntry);
    RegisterRuntimeExport("?Get@MemoryAllocator@System@@SAPEAV12@XZ",
                          reinterpret_cast<void*>(&rtalloc::Get));
    RegisterRuntimeExport("?GetAllocationSize@MemoryAllocator@System@@QEAA_KPEAX@Z",
                          reinterpret_cast<void*>(&rtalloc::GetAllocationSize));
    RegisterRuntimeExport("?GetAllocatedSize@MemoryAllocator@System@@QEBA_JXZ",
                          reinterpret_cast<void*>(&rtalloc::GetAllocatedSize));
    RegisterRuntimeExport("?GetMemoryRange@MemoryAllocator@System@@AEAAPEAUMemoryRange@12@PEAX@Z",
                          reinterpret_cast<void*>(&rtalloc::GetMemoryRange));
    RegisterRuntimeExport("?GetFreeTagForSize@MemoryAllocator@System@@AEAAPEAUFreeTag@12@_K@Z",
                          reinterpret_cast<void*>(&rtalloc::GetFreeTagForSize));
    RegisterRuntimeExport("?LinkFreeTag@MemoryAllocator@System@@AEAAXPEAUMemoryTag@12@@Z",
                          reinterpret_cast<void*>(&rtalloc::LinkFreeTag));
    RegisterRuntimeExport("?UnlinkFreeTag@MemoryAllocator@System@@AEAAXPEAUFreeTag@12@0@Z",
                          reinterpret_cast<void*>(&rtalloc::UnlinkFreeTag));
    RegisterRuntimeExport("?UnlinkFreeTag@MemoryAllocator@System@@AEAAXPEAUMemoryTag@12@@Z",
                          reinterpret_cast<void*>(&rtalloc::UnlinkFreeTagT));
    RegisterRuntimeExport("?CommitToPage@MemoryAllocator@System@@AEAAXPEAUMemoryRange@12@PEAX@Z",
                          reinterpret_cast<void*>(&rtalloc::CommitToPage));
    RegisterRuntimeExport("?ReleaseToPage@MemoryAllocator@System@@AEAAXPEAUMemoryRange@12@PEAX@Z",
                          reinterpret_cast<void*>(&rtalloc::ReleaseToPage));
    RegisterRuntimeExport("?AddNewMemoryRange@MemoryAllocator@System@@AEAA_N_K@Z",
                          reinterpret_cast<void*>(&rtalloc::AddNewMemoryRange));
    RegisterRuntimeExport("?ExpandMemoryRangeTable@MemoryAllocator@System@@AEAA_NXZ",
                          reinterpret_cast<void*>(&rtalloc::ExpandMemoryRangeTable));
    RegisterRuntimeExport("?Startup@MemoryAllocator@System@@QEAA_N_K0@Z",
                          reinterpret_cast<void*>(&rtalloc::Startup));
    RegisterRuntimeExport("?Shutdown@MemoryAllocator@System@@QEAAXXZ",
                          reinterpret_cast<void*>(&rtalloc::Shutdown));
    RegisterRuntimeExport("?PointerOffset@MemoryAllocator@System@@CA_JPEAX0@Z",
                          reinterpret_cast<void*>(&rtalloc::PointerOffset));
    RegisterRuntimeExport("?GetBinForSize@MemoryAllocator@System@@AEBAH_K@Z",
                          reinterpret_cast<void*>(&rtalloc::GetBinForSizeM));
    RegisterRuntimeExport("?GetBinForLargeSize@MemoryAllocator@System@@AEBAH_K@Z",
                          reinterpret_cast<void*>(&rtalloc::GetBinForLargeSizeM));
    // The six Platform* exports are registered by the _win half, which owns
    // their bodies.
    const int platformExports = RtSystemAllocatorRegisterPlatformExports();
    return 21 + platformExports;
}
}  // namespace
