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
// ARMING. RtSystemAllocatorArm() (below, called once at boot from
// standalone/runtime_init.cpp) installs the platform seam and hands the host's
// cutover armer its chance to refuse; it prints `[RtAlloc] ARMED` when both are
// in place. Every caller reaches the allocator by name.
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

#include "../../types/system/MemoryAllocator.h"
#include "../../types/system/Error.h"   // B3R: FatalOutOfMemory calls Error by name

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

// ?_singleton@MemoryAllocator@System@@0PEAV12@EA (0x1801fc718): BSS in retail,
// zero until the ctor below runs (B3R W0-SM M1: was the mint's storage cell).
System::MemoryAllocator* System::MemoryAllocator::_singleton = nullptr;

// The instrument seat (MemoryAllocator.h): STANDALONE DEVIATION, instrument
// seat, retail has no such branch. Constant-initialised null; armed at boot only.
System::MemoryAllocatorInstrument System::g_memoryAllocatorInstrument = { nullptr, nullptr };

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
// `System::Error::SetText` (0x1800c5fa0) and `Display` (0x1800c6060) are
// runtime/src/system/error.cpp's, called by name (B3R: they were two
// ResolveExport doors; both rows are registered and ported, so the readiness
// guards and the "nothing registered" fallback print were dead arms).
void FatalOutOfMemory(const char* text, bool stopApplication) {
    System::Error::SetText(text, stopApplication);
    if (stopApplication) System::Error::Display();
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

inline MemoryTag* AsTag(uint8_t* p)  { return reinterpret_cast<MemoryTag*>(p); }
inline FreeTag*   AsNode(uint8_t* p) { return reinterpret_cast<FreeTag*>(p); }

}  // namespace

// ===========================================================================
// The members (B3R W0-A0). Each body is the transcription that used to be the
// free function `rtalloc::X(MemoryAllocator* self, …)`; `self` is `this`.
// The parameter types are the ones the DLL's own mangled signatures name:
// PEAUMemoryTag@12@ and PEAUFreeTag@12@. A FreeTag* is the USER pointer of a
// free block (tag + 8); a MemoryTag* is the block header itself.
// ===========================================================================

// --- GetBinForLargeSize (0x1800c7920) / GetBinForSize (0x1800c78d0) ---------
int MemoryAllocator::GetBinForLargeSize(uint64_t n) const {
    int shift = 0x20;
    int bits  = 0x40;
    do {
        uint64_t v = n >> shift;
        if (v != 0) { bits -= shift; n = v; }
        shift >>= 1;
    } while (shift != 0);
    return static_cast<int>(n) - bits + 0xb6;
}

int MemoryAllocator::GetBinForSize(uint64_t n) const {
    if (n < 0x201) return static_cast<int>(static_cast<int32_t>(n)) >> 2;
    return GetBinForLargeSize(n);
}

// --- GetMemoryRange (0x1800c71f0) ------------------------------------------
MemoryRange* MemoryAllocator::GetMemoryRange(void* p) {
    MemoryRange* r = ranges;
    for (int i = 0; i < rangeCount; ++i, ++r) {
        if (r->base <= static_cast<uint8_t*>(p) && static_cast<uint8_t*>(p) < r->end)
            return r;
    }
    return nullptr;
}

// --- GetAllocationSize (0x1800c71d0) ---------------------------------------
uint64_t MemoryAllocator::GetAllocationSize(void* p) {
    return (*reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(p) - 8) & kTagSizeMask) << 2;
}

// --- GetAllocatedSize (0x18007c0b0) ----------------------------------------
int64_t MemoryAllocator::GetAllocatedSize() const { return allocatedBytes; }

// --- GetFreeTagForSize (0x1800c7230) ---------------------------------------
// Returns a FreeTag* (== tag + 8), i.e. the USER pointer of a free block whose
// byte size is >= `want`. First fit within a bin, then the next bin up.
FreeTag* MemoryAllocator::GetFreeTagForSize(uint64_t want) {
    int bin = GetBinForSize(want);
    if (bin < minBin) bin = minBin;

    for (; bin < 0xc0; ++bin) {
        uint8_t* cur = reinterpret_cast<uint8_t*>(freeLists[bin]);
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
void MemoryAllocator::LinkFreeTag(MemoryTag* tagp) {
    uint8_t* tag = reinterpret_cast<uint8_t*>(tagp);
    uint64_t bytes = (Tag(tag) & kTagSizeMask) * 4;
    if (bytes < 0x10) return;

    uint8_t* node = tag + 8;
    int bin = GetBinForSize(bytes);
    uint64_t* head = &freeLists[bin];

    if (*head == 0) {
        Node(node) = 0;
        Tag(tag) &= 0x00003fffffffffffull;          // clear the prev-link high bits
        if (bin < minBin) minBin = bin;
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
    freeTagCount++;
}

// --- UnlinkFreeTag(FreeTag*, FreeTag*) (0x1800c7410) -----------------------
// The second parameter is unused by retail; kept for ABI shape.
void MemoryAllocator::UnlinkFreeTag(FreeTag* nodep, FreeTag* /*unused*/) {
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
            freeLists[bin] = 0;
        } else {
            freeLists[bin] = reinterpret_cast<uint64_t>(next);
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

    int cursor = minBin;
    if (bin == cursor && freeLists[bin] == 0) {
        while (cursor < 0xc0 && freeLists[minBin] == 0) {
            cursor = minBin + 1;
            minBin = cursor;
        }
    }
    freeTagCount--;
}

// --- UnlinkFreeTag(MemoryTag*) (0x1800c7580) -------------------------------
void MemoryAllocator::UnlinkFreeTag(MemoryTag* tagp) {
    uint8_t* tag = reinterpret_cast<uint8_t*>(tagp);
    if (3 < (Tag(tag) & kTagSizeMask)) UnlinkFreeTag(AsNode(tag + 8), nullptr);
}

// --- CommitToPage (0x1800c7bd0) --------------------------------------------
void MemoryAllocator::CommitToPage(MemoryRange* range, void* upTo) {
    uint64_t page = pageSize;
    uint64_t have = range->committed;
    uint64_t need = ((page - reinterpret_cast<uint64_t>(range->base) - 1 +
                      reinterpret_cast<uint64_t>(upTo)) / page) * page;
    if (have <= need && need - have != 0) {
        g_plat.commit(range->base + have, need - have);
        range->committed = need;
    }
}

// --- ReleaseToPage (0x1800c7c80) -------------------------------------------
void MemoryAllocator::ReleaseToPage(MemoryRange* range, void* downTo) {
    uint64_t page = pageSize;
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
bool MemoryAllocator::ExpandMemoryRangeTable() {
    uint64_t page   = pageSize;
    int      oldCap = rangeCapacity;
    void*    src    = ranges;

    rangeCapacity = oldCap + 2;
    uint64_t bytes = ((page - 1 + static_cast<uint64_t>(oldCap + 2) * 0x18) / page) * page;
    void* dst = g_plat.reserveCommit(nullptr, bytes);
    ranges = static_cast<MemoryRange*>(dst);
    if (dst == nullptr) return false;

    std::memcpy(dst, src, static_cast<size_t>(oldCap) * 0x18);
    std::memset(static_cast<uint8_t*>(dst) + static_cast<size_t>(oldCap) * 0x18, 0,
                static_cast<size_t>(rangeCapacity - oldCap) * 0x18);
    if (oldCap != 0) g_plat.release(src);
    return true;
}

// --- AddNewMemoryRange (0x1800c7690) ---------------------------------------
bool MemoryAllocator::AddNewMemoryRange(uint64_t want) {
    uint64_t size = minRangeSize;
    if (size < want) size = want;

    if (maxTotalSize != 0 && maxTotalSize < size + totalReserved)
        return false;

    if (rangeCount == rangeCapacity && !ExpandMemoryRangeTable())
        return false;

    MemoryRange* range = ranges + rangeCount;
    uint64_t page = pageSize;
    rangeCount++;

    uint8_t* base = static_cast<uint8_t*>(g_plat.reserve(nullptr, ((page - 1 + size) / page) * page));
    range->base = base;
    if (base != nullptr) {
        range->committed = 0;
        range->end = base + size;
        CommitToPage(range, base + 0x10);
        Tag(base) = (((size & kSizeBytesMask) | kThisFreeShl2) >> 2) |
                    (Tag(base) & 0xffffe00000000000ull);
        Node(base + 8) = 0;
        Tag(base) &= 0x00003fffffffffffull;
        LinkFreeTag(AsTag(base));
        totalReserved += size;
    } else {
        FatalOutOfMemory("Memory: The application failed to allocate a memory range!", false);
    }
    return base != nullptr;
}

// --- Allocate (0x1800c6e60) ------------------------------------------------
void* MemoryAllocator::Allocate(uint64_t size, int align) {
    if (g_largestRequestedSize < size) g_largestRequestedSize = size;

    const uint32_t align8 = (static_cast<uint32_t>(align) + 7u) & 0xfffffff8u;

    // CDQE: (align8 - 8) is SIGN-extended before the add. align == 0 subtracts 8.
    uint64_t total = static_cast<uint64_t>(static_cast<int64_t>(
                         static_cast<int32_t>(align8 - 8u))) + ((size + 7) & ~7ull);
    if (total < 8) total = 8;
    uint64_t want = total + 8;

    FreeTag* freeTag = GetFreeTagForSize(want);
    if (freeTag == nullptr) {
        AddNewMemoryRange(want);
        freeTag = GetFreeTagForSize(want);
        if (freeTag == nullptr) {
            FatalOutOfMemory(
                "Failed to allocate memory. Either too many packages (mods) are loaded, "
                "no more memory is available, or an error has occured.", true);
        }
    }
    UnlinkFreeTag(freeTag, nullptr);

    uint8_t* tag = reinterpret_cast<uint8_t*>(freeTag) - 8;

    // Inlined GetMemoryRange. Retail does not guard the miss and neither do we.
    MemoryRange* range = nullptr;
    {
        MemoryRange* r = ranges;
        for (int i = 0; i < rangeCount; ++i, ++r) {
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
        LinkFreeTag(AsTag(tag));

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
        CommitToPage(range, tag + (nq + 4) * 4);

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
        LinkFreeTag(AsTag(rem));
    }

    // Inlined CommitToPage over the block we are handing out.
    {
        uint64_t page = pageSize;
        uint64_t have = range->committed;
        uint64_t need = (((Tag(tag) & kTagSizeMask) * 4 -
                          reinterpret_cast<uint64_t>(range->base) - 1 + page +
                          reinterpret_cast<uint64_t>(tag)) / page) * page;
        if (have <= need && need - have != 0) {
            g_plat.commit(range->base + have, need - have);
            range->committed = need;
        }
    }

    allocatedBytes += static_cast<int64_t>((Tag(tag) & kTagSizeMask) * 4);
    allocCount++;
    return tag + 8;
}

// --- Free (0x1800c6c00) ----------------------------------------------------
void MemoryAllocator::Free(void* p) {
    if (p == nullptr) return;

    uint8_t* tag = static_cast<uint8_t*>(p) - 8;

    MemoryRange* range = nullptr;
    {
        MemoryRange* r = ranges;
        for (int i = 0; i < rangeCount; ++i, ++r) {
            if (r->base <= tag && tag < r->end) { range = r; break; }
        }
    }

    allocCount--;
    allocatedBytes -= static_cast<int64_t>((Tag(tag) & kTagSizeMask) * 4);

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
        if (3 < (Tag(blk) & kTagSizeMask)) UnlinkFreeTag(AsNode(blk + 8), nullptr);
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
            if (3 < (Tag(next) & kTagSizeMask)) UnlinkFreeTag(AsNode(next + 8), nullptr);
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
        ReleaseToPage(range, blk + 0x10);

    Tag(blk) = (Tag(blk) & 0xffffe1ffffffffffull) | kTagThisFree;
    LinkFreeTag(AsTag(blk));
}

// --- Startup (0x1800c7620) -------------------------------------------------
bool MemoryAllocator::Startup(uint64_t minRangeMB, uint64_t maxTotalMB) {
    minRangeSize = minRangeMB << 20;
    maxTotalSize = maxTotalMB << 20;
    pageSize = g_plat.pageSize();
    if (!AddNewMemoryRange(0)) {
        FatalOutOfMemory("Memory: The application failed to start the memory allocator!", false);
        return false;
    }
    return true;
}

// --- Shutdown (0x1800c75b0) ------------------------------------------------
void MemoryAllocator::Shutdown() {
    for (int i = 0; i < rangeCount; ++i) g_plat.release(ranges[i].base);
    g_plat.release(ranges);
}

// --- Get (0x1800534e0) -----------------------------------------------------
MemoryAllocator* MemoryAllocator::Get() {
    return _singleton;
}

// --- MemAlloc (0x1800c7a50) / MemFree (0x1800c7ad0) ------------------------
// Both open with the instrument seat (MemoryAllocator.h): STANDALONE DEVIATION,
// instrument seat, retail has no such branch. Null unless BANISHED_REDZONE or
// BANISHED_ALLOC_STATS armed it at boot; the armed hook does the whole call.
void* MemoryAllocator::MemAlloc(uint64_t size, int align) {
    if (System::g_memoryAllocatorInstrument.memAlloc)                // STANDALONE DEVIATION
        return System::g_memoryAllocatorInstrument.memAlloc(size, align);
    MemoryAllocator* self = Get();
    g_plat.lockEnter(self->lock);
    void* r = self->Allocate(size, align);
    g_plat.lockLeave(self->lock);
    return r;
}

void MemoryAllocator::MemFree(void* p) {
    if (System::g_memoryAllocatorInstrument.memFree) {               // STANDALONE DEVIATION
        System::g_memoryAllocatorInstrument.memFree(p);
        return;
    }
    if (p == nullptr) return;
    MemoryAllocator* self = Get();
    g_plat.lockEnter(self->lock);
    self->Free(p);
    g_plat.lockLeave(self->lock);
}

// --- PointerOffset (0x180034e20) -------------------------------------------
int64_t MemoryAllocator::PointerOffset(void* a, void* b) {
    return static_cast<uint8_t*>(a) - static_cast<uint8_t*>(b);
}

// --- The Platform* family (0x1800c7b30 / b50 / b60 / bb0 / c40 / c70) ------
// These six are defined in runtime/src/system/allocator_win.cpp. They are the
// platform seam the ORIGINAL authors drew, and PLAN.md 4.1's `_win` rule falls
// on the same line, so the OS header goes there and nowhere else.

// ===========================================================================
// rtalloc:: — what is left of the free-function door.
// ===========================================================================
namespace rtalloc {

// --- ctor (0x1800c79a0) / dtor (0x1800c7960) -------------------------------
// NOT members: the class declares no ctor/dtor (see MemoryAllocator.h, "NOT
// DECLARED, ON PURPOSE"). Called from runtime/src/engine/singleton_ctors.cpp
// (Ctor_/Dtor_MemoryAllocator, the ??0 / ??1 rows). Both write the class static
// System::MemoryAllocator::_singleton (0x1801fc718).
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
    MemoryAllocator::_singleton = self;
    self->pageSize = 0;                       // inlined PlatformConstruct
}

void Destruct(MemoryAllocator* self) {
    MemoryAllocator::_singleton = nullptr;
    g_plat.lockDelete(self->lock);
}

// --- The cutover armer's pair ---------------------------------------------
// g_armCutover composes the locked entry points from these two `self`-explicit
// bodies. B3R RT-Core: the other fifteen registration thunks went with their
// rows, which had no reader.
void*    Allocate(MemoryAllocator* self, uint64_t size, int align) { return self->Allocate(size, align); }
void     Free(MemoryAllocator* self, void* p)                      { self->Free(p); }

}  // namespace rtalloc

// ============================================================================
// SHADOW ORACLE  (PLAN.md §4.3 risk 2)
// ============================================================================
//
// An allocator cannot be diffed the Wave 1 way: running both implementations
// mutates the heap twice. Shadow mode's contract is "ours computes into a
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

// ---------------------------------------------------------------------------
// Wave 10 lane SHADOWBISECT — the UNOBSERVED-MUTATION ("gap") detector.
// ---------------------------------------------------------------------------
// SHADOWRE's signature (ours live HIGHER, freeTagCount LOWER, allocatedBytes
// HIGHER by a multiple of 32, free_unmapped==0, geometry AGREEING on the first
// divergent call) admits one reading: the REAL arena performed operations the
// shadow never observed. That is a hypothesis about a route, and the way to
// test it directly is to ask the real allocator's own counters whether they
// moved while we were not looking.
//
// Both shadow hooks run POST-op, inside the allocator critical section. So:
//   * at the END of every observed op we snapshot the real allocator's five
//     tuple counters (RtAllocShadowPostOp);
//   * at the START of the next wrapper entry, still inside the same critical
//     section and BEFORE the real body runs (RtAllocShadowPreOp), we compare.
// Everything between those two points is serialized by the CS, so ANY
// difference is a mutation of the real arena that did not come through
// AllocateLocked/FreeLocked — i.e. exactly the bypassing route the hypothesis
// names. If no gap is ever seen, the hypothesis is DEAD and the signature needs
// another explanation.
//
// The detector is independent of the divergence comparison: it reads the real
// allocator only, never the replica.
bool     g_gapValid      = false;
uint64_t g_gapChecks     = 0;    // how many times the comparison actually ran
uint64_t g_gapCount      = 0;    // how many found the real arena moved
int64_t  g_gapDAlloc     = 0;    // summed deltas over all gaps
int64_t  g_gapDFreeTag   = 0;
int64_t  g_gapDBytes     = 0;
int64_t  g_gapDRanges    = 0;
int64_t  g_gapDReserved  = 0;
int      g_gapPrinted    = 0;
uint64_t g_gapCtlInjected = 0;
struct GapSnap { int32_t alloc, freeTag, ranges; int64_t bytes; uint64_t reserved; };
GapSnap g_gapSnap = {};

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

    g_plat.lockInit(g_shadow.lock);
    g_shadow.pageSize = 0;

    g_shadow.minRangeSize = minRangeMB << 20;
    g_shadow.maxTotalSize = maxTotalMB << 20;
    g_shadow.pageSize = g_plat.pageSize();
    if (!g_shadow.AddNewMemoryRange(0)) return false;

    // AFTER range 0 exists, so range 0 matches retail byte for byte and only
    // the growth path can ever notice. See the `grow` note above.

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
    if (!g_shadowArmed) return;

    uint64_t reqSize = size;

    // Growth coverage is measured as a state DELTA across the replica's own
    // Allocate, so nothing is added to the ported bodies.
    const int32_t preRanges = g_shadow.rangeCount;
    const int32_t preCap    = g_shadow.rangeCapacity;

    void* ours = g_shadow.Allocate(reqSize, align);

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
}

extern "C" void RtAllocShadowFree(void* theirs, const MemoryAllocator* theirSelf) {
    if (!g_shadowArmed || theirs == nullptr) return;

    void* ours = ShadowMapTake(theirs);
    if (ours == nullptr) {               // allocated before we armed, or the map
        g_shadowFreeUnmapped++;          // dropped it -- either way, not comparable
        return;
    }
    g_shadow.Free(ours);
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
}

// ---------------------------------------------------------------------------
// Gap detector: pre-op comparison and post-op snapshot (lane SHADOWBISECT).
// Both are called from AllocateLocked/FreeLocked INSIDE the allocator critical
// section, with self already filtered to the observed allocator.
// ---------------------------------------------------------------------------
extern "C" void RtAllocShadowPreOp(const MemoryAllocator* theirSelf) {
    if (!g_shadowArmed || !theirSelf) return;
    if (!g_gapValid) return;
    g_gapChecks++;
    const int64_t dA = (int64_t)theirSelf->allocCount     - (int64_t)g_gapSnap.alloc;
    const int64_t dF = (int64_t)theirSelf->freeTagCount   - (int64_t)g_gapSnap.freeTag;
    const int64_t dB = (int64_t)theirSelf->allocatedBytes - g_gapSnap.bytes;
    const int64_t dR = (int64_t)theirSelf->rangeCount     - (int64_t)g_gapSnap.ranges;
    const int64_t dV = (int64_t)theirSelf->totalReserved  - (int64_t)g_gapSnap.reserved;
    if (dA == 0 && dF == 0 && dB == 0 && dR == 0 && dV == 0) return;

    g_gapCount++;
    g_gapDAlloc += dA; g_gapDFreeTag += dF; g_gapDBytes += dB;
    g_gapDRanges += dR; g_gapDReserved += dV;
    if (g_gapPrinted < 12) {
        g_gapPrinted++;
        fprintf(stderr, "[RtAllocShadow] GAP #%llu at observed-op %llu: the real arena moved "
                        "with no hook entry between: dAllocCount=%lld dFreeTagCount=%lld "
                        "dAllocatedBytes=%lld dRangeCount=%lld dReserved=%lld\n",
                (unsigned long long)g_gapCount, (unsigned long long)g_shadowCalls,
                (long long)dA, (long long)dF, (long long)dB, (long long)dR, (long long)dV);
        std::fflush(stderr);
    }
}

extern "C" void RtAllocShadowPostOp(const MemoryAllocator* theirSelf) {
    if (!g_shadowArmed || !theirSelf) return;
    g_gapSnap.alloc    = theirSelf->allocCount;
    g_gapSnap.freeTag  = theirSelf->freeTagCount;
    g_gapSnap.bytes    = theirSelf->allocatedBytes;
    g_gapSnap.ranges   = theirSelf->rangeCount;
    g_gapSnap.reserved = theirSelf->totalReserved;
    g_gapValid = true;
}

extern "C" void RtAllocShadowGaps(uint64_t* checks, uint64_t* gaps, int64_t* dAlloc,
                                  int64_t* dFreeTag, int64_t* dBytes,
                                  uint64_t* ctlInjected) {
    if (checks) *checks = g_gapChecks;
    if (gaps) *gaps = g_gapCount;
    if (dAlloc) *dAlloc = g_gapDAlloc;
    if (dFreeTag) *dFreeTag = g_gapDFreeTag;
    if (dBytes) *dBytes = g_gapDBytes;
    if (ctlInjected) *ctlInjected = g_gapCtlInjected;
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
// ARMING -- RtSystemAllocatorArm(), called once at boot (standalone/runtime_init.cpp)
// ============================================================================

namespace {

// The host's cutover armer (standalone/alloc_cutover_arm.cpp), handed in by
// RtSystemAllocatorSetCutoverArmer() before RtSystemAllocatorArm() runs.
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

}

extern "C" void RtSystemAllocatorSetCutoverArmer(RtAllocCutoverArmer fn) {
    g_armCutover = fn;
}

// WAVE 22 LANE ALLOC -- SELF-ATTESTATION, and why it is fprintf and not printf.
//
// Every [RtAlloc] line in this file that reports the outcome of arming is a
// `printf`, and the PERF build compiles printf to `((void)0)`
// (standalone/log_strip.h). So the one binary an A/B is measured on is
// precisely the binary that cannot say whether it armed. A perf run with
// whose allocator silently REFUSED (missing platform seam, missing
// cutover armer) is indistinguishable from an armed
// one, and the A/B would then be comparing a build against itself and would
// report `no regression` -- the measurement reading GREEN because the subject
// never ran. That is this project's own recurring defect shape.
//
// stderr survives the strip (see the note in standalone/log_strip.h) and lands
// in standalone_error.log, which tools/stress_run.sh already collects per run.
// One line, once, at init, outside every measurement window.
static void AttestArming(const char* verdict) {
    fprintf(stderr, "[RtAlloc] ATTEST: %s\n", verdict);
    std::fflush(stderr);
}

void RtSystemAllocatorArm() {
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
        AttestArming("REFUSED: platform seam incomplete");
        return;
    }

    // There is no DLL, so no detour is possible OR necessary; the cutover armer
    // is the host's hook (standalone/alloc_cutover_arm.cpp).
    if (g_armCutover == nullptr) {
        printf("[RtPort] MemoryAllocator: REFUSING to arm -- Runtime-x64.dll is absent "
               "and no cutover armer is installed. "
               "standalone/main.cpp must call RtSystemAllocatorSetCutoverArmer().\n");
        AttestArming("REFUSED: cutover, no armer installed");
        return;
    }
    if (!g_armCutover()) {
        printf("[RtPort] MemoryAllocator: cutover armer REFUSED\n");
        AttestArming("REFUSED: cutover armer said no");
        return;
    }
    printf("[RtAlloc] ARMED (cutover: Runtime-x64.dll absent) -- no detour installed "
           "because there are no DLL-internal callers to redirect.\n");
    AttestArming("ARMED (cutover, DLL absent)");
}

// ============================================================================
// The ADDRESS registrations — cutover only, and the "only" is the whole point
// ============================================================================
//
// Wave 9, attempt 13. The DLL-less boot got eight frames past attempt 12 and
// then died `EXEC of 0` at `rtcoll::MemAlloc +0x81`:
//
//     #4 AppendFilename +0x2D  ->  #3 RtColl_ArraySetSizeFill_180026880 +0x39
//     #2 rtcoll::SetSizeCore +0x181  ->  #1 rtcoll::MemAlloc +0x81
//
// Nothing was missing a NAME. `runtime/src/collection/array.cpp` reaches the
// allocator through the two exported statics BY ADDRESS, deliberately (read its
// "THE ALLOCATOR BOUNDARY" banner), and its own comment states this dependency
// in advance: "At cutover ResolveAddr(Module::Runtime, 0x1800c7a50) resolves
// nothing, because there is no DLL. These two accessors MUST be repointed at
// lane E's allocator in the same commit that arms it." That commit is this one.
// No name-based instrument could ever have seen it, because the call site does
// not mention a name — which is also why gate row E named the wrong mechanism
// (it predicted an unminted `?_singleton@`; that symbol IS minted).
//
// The four addresses, and who reaches each:
//
//   0x1800c7a50  ?MemAlloc@ (static, locking)  runtime/src/collection/array.cpp
//                                              runtime/src/graphics/scene.cpp
//   0x1800c7ad0  ?MemFree@  (static, locking)  runtime/src/collection/array.cpp
//                                              runtime/src/ui/widgets_input.cpp
//                                              standalone/filename_runtime_hook.cpp
//   0x1800c6e60  ?Allocate@ (member)           engine/funcs/engine_funcs.cpp:1631
//   0x1800c6c00  ?Free@     (member)           (no live reader today; registered
//                                              so the pair cannot drift apart)
//
// The member pair gets `allocateEntry`/`freeEntry`, which on this path are the
// COMPOSED (locked) stubs — the same pointers `?Allocate@`/`?Free@` are given
// three lines above. The static pair gets rtalloc::MemAlloc/MemFree, which take
// the critical section themselves exactly as retail's statics do. So both doors
// into the free list are locked, which is the invariant WAVE2_ALLOCATOR.md §4
// exists to protect.
//
// *** WHY THIS IS NOT CALLED ON THE DLL-PRESENT PATH ***
//
// Two reasons, and the second is a live hazard, not a preference:
//
//  1. It is unnecessary. With the DLL present the swap is a DETOUR: retail's
//     bytes at 0x1800c6e60/0x1800c6c00 JMP to our composed stubs. An
//     unregistered `ResolveAddr` there hands back the DLL's address, and calling
//     it lands in our port anyway. The heap is entirely ours either way.
//  2. It would collide with standalone/allocator_lock.cpp, which resolves those
//     same two addresses in order to PATCH the prologue it finds there.
//     `ResolveAddr(Module::Runtime, ...)` consults this registry FIRST
//     ([[reference-resolveaddr-consults-port-registry-first]]), so registering
//     them would hand the patcher OUR function to patch. Its memcmp guard would
//     refuse — safely, but the refusal would read as a mysterious prologue
//     mismatch rather than as what it is.
//
// At cutover neither reason applies: there is no DLL to detour and no prologue
// for allocator_lock to have patched (it prints "could not resolve" and stands
// down, which is the correct behaviour and was never the wall).
// 0x180009c30 -- the allocator-lock Free wrapper, and a TRAP worth spelling out
//
// Retail's body is `if (p) { Enter; Free(_singleton, p); Leave; }` -- byte for
// byte the `?MemFree@` static registered above -- but its SIGNATURE is not the
// same. It takes TWO parameters and ignores the first: retail's
// RemoveGlobalObject tail-calls it passing `this` in RCX and the pointer in RDX
// (see runtime/src/graphics/scene.cpp:238, `RtFreeLocked(void*, void*)`).
//
// So registering `rtalloc::MemFree` here -- which is what "it is exactly the
// MemFree static" invites -- would be silently, catastrophically wrong. Our
// MemFree reads its one argument from RCX, which at this call site holds the
// IGNORED `this`. It would free an object pointer instead of the buffer, leave
// the buffer leaked, and corrupt the free list with a block the allocator never
// handed out. Nothing would fault at the call; the damage would surface
// arbitrarily later in an unrelated allocation.
//
// The shim costs one instruction of shuffle and makes the ABI difference
// explicit instead of load-bearing-and-invisible.
// WAVE 10 LANE SHADOWBISECT — this body used to call `rtalloc::MemFree(p)`
// DIRECTLY, and that one C++ call is what made the allocator shadow read red in
// all seven of lane SHADOWRE's cells.
//
// `rtalloc::MemFree` is `Get(); lockEnter; rtalloc::Free(self, p); lockLeave` —
// OUR ported free logic, run against whatever `_singleton` names. With the DLL
// present and the allocator port unarmed, `_singleton` is RETAIL's arena. So a
// registration that is unconditional and default-on was running the ported Free
// on the shipped allocator's heap, never entering the DLL's `Free`
// (0x1800c6c00) — which is where standalone/allocator_lock.cpp's patch, and
// therefore every observer that rides on it, lives.
//
// Two consequences, and the second is the serious one:
//   * MEASUREMENT: the shadow never saw those frees, so its replica kept blocks
//     retail had released — `ours live` higher, `freeTagCount` lower, `bytes`
//     higher by a multiple of 32 (the 0x18-payload LinkedList node), and
//     `free_unmapped` clean because the Free never reached the hook at all.
//     That is SHADOWRE's signature, exactly, and the divergence count is
//     precisely the number of comparisons after the first such free.
//   * SUBJECT: "the allocator port is off" was not true on this path. An
//     unarmed run was already mutating retail's arena with our code, by a route
//     no kill switch could reach and no counter reported.
//
// The fix is the rule this file's own neighbours already state
// (runtime/src/collection/linkedlist.cpp:153, runtime/src/graphics/
// scene_grid.cpp:181): reach the heap through the ONE door
// `RegisterAllocatorAddresses()` arms, never by binding to our body directly.
// Retail's 0x180009c30 IS `Enter; Free(_singleton, p); Leave`, i.e. byte for
// byte `?MemFree@` at 0x1800c7ad0, so resolving that address is also the
// FAITHFUL port and not a workaround:
//   * unarmed, DLL present — the door hands back retail's MemFree, which calls
//     retail's Free through its patched entry: observed, and identical to what
//     retail's own 0x180009c30 would have done;
//   * armed / at cutover — `RegisterAllocatorAddresses()` has registered
//     0x1800c7ad0 -> `rtalloc::MemFree`, so the door hands back exactly the
//     body this code used to call directly. Behaviour there is unchanged.
//
// RETRIES A NULL rather than caching one, for the reason linkedlist.cpp gives:
// this registration is unconditional and can run before the allocator's arming
// path supplies the address, and a caller that ran first would otherwise freeze
// a null in for the life of the process.
//
// B3R W0-A0: the static, called directly. The by-address door above was bound
// to this same function (RegisterAllocatorAddresses), and the instrument seat
// now lives inside it, so the "observed" property the banner asks for holds.
// B3R RT-Core: external linkage, declared in runtime/types/system/
// MemoryAllocator.h, so its readers call it by name.
namespace rtalloc {
void FreeLockedIgnoringSelf(void* /*ignoredThis*/, void* p) {
    MemoryAllocator::MemFree(p);
}
}  // namespace rtalloc

// RegisterAllocatorAddresses() is gone. B3R W2-RT: 0x1800c7a50 / 0x1800c7ad0
// (the two statics) had no reader left -- every reader calls
// MemoryAllocator::MemAlloc / MemFree by name -- and went. B3R W2-EZ: its last
// row, 0x1800c6e60, went with its one reader (engine_funcs.cpp's fn_MemAlloc).
// 0x180009c30 is not registered at all any more (B3R RT-Core): every reader
// calls rtalloc::FreeLockedIgnoringSelf by name.
