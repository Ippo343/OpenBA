// ============================================================================
// runtime/src/ui/spritesheet_new.cpp — Wave 24 lane SMALLBODY
//   ??2SpriteSheet@UI@@SAPEAX_K@Z    retail 0x180002e60  ord 895   99 bytes
// ============================================================================
//
// The `UI::SpriteSheet` class `operator new`. One of the nine rows in this
// lane's brief, and the only one of the nine that admits a TWO-SIDED oracle
// needing neither a game route nor a fabricated subject — see ORACLE below.
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED BEFORE A LINE WAS WRITTEN
// ---------------------------------------------------------------------------
// `reference_grep_for_the_port_before_writing_it.md` (three lanes in one wave
// re-ported what the tree already had) and
// `reference_half_registered_port_reads_as_absent.md` (a body ported but never
// REGISTERED is indistinguishable from an absent one to the census). So the
// grep was by ADDRESS *and* by NAME, scoped to the source roots:
//
//   grep -rn '??2SpriteSheet@UI@@SAPEAX' runtime/ code/ standalone/ engine/ dx11/
//     -> exactly ONE hit, and it is a CONSUMER:
//        runtime/src/save/saveload_draw.cpp:601
//           void* p = RT_EXPORT_ONCE(OpNewFn, "??2SpriteSheet@UI@@SAPEAX_K@Z")(0x58);
//   grep -rn '180002e60' runtime/ code/ standalone/ engine/ dx11/   -> NOTHING
//
// The greps were SCOPED to the source roots, not run at the repo root:
// `.claude/worktrees/` holds sibling lane checkouts and an unscoped grep
// returns a confident wrong number with no symptom (WAVE24_KICKOFF §1a, which
// measured that exact inflation at 22x).
//
// The one hit is also this port's ROUTE: `saveload_draw.cpp` builds the
// save/load screen's one-sprite sheet and asks for 0x58 bytes. So this row is
// not merely registerable — it is ENTERED, by our own tree, on the save/load
// path, which is what the attendance counter below reports.
//
// ---------------------------------------------------------------------------
// THE BODY, AND WHY IT IS NOT ASSUMED FROM ITS SIBLING
// ---------------------------------------------------------------------------
// This is instruction-for-instruction `??2ToolTipDialog@UI@@SAPEAX_K@Z`
// (0x18004cfd0), which runtime/src/ui/tooltipdialog.cpp already ports. That
// file's own banner carries the warning that made this lane check rather than
// copy:
//
//     "Reading one and assuming the other matches by resemblance is exactly how
//      this family has been got wrong before (runtime/src/video/context_ctor.cpp
//      records the same check, and found a sibling using align 4)."
//
// So the two were DISASSEMBLED SIDE BY SIDE for this lane, not compared by
// shape. Both are 99 bytes (0x180002e60..0x180002ec3 and
// 0x18004cfd0..0x18004d033), both end on the same `int3` pad, and every
// mnemonic and every immediate matches — including the one that matters,
// `MOV R8D,0x8`. The only bytes that differ are the rel32 and RIP-relative
// displacement fields, which encode a DISTANCE from the instruction and so MUST
// differ between two copies at different addresses
// (reference_position_only_diffs_hide_one_body.md).
//
//     PUSH RDI / SUB RSP,0x30 / MOV [RSP+0x20],-2       MSVC unwind marker
//     MOV RBX,RCX                                       save `bytes`
//     RDI = [0x1801fc718] + 0x640                       the allocator's lock
//     CALL [0x18014b390]                                EnterCriticalSection
//     MOV R8D,0x8 / MOV RDX,RBX / MOV RCX,[0x1801fc718]
//     CALL 0x1800c6e60                                  Allocate(alloc,bytes,8)
//     CALL [0x18014b388]                                LeaveCriticalSection
//     MOV RAX,RBX(=saved result) / RET
//
// The two KERNEL32 slots were named from the DLL's OWN import table, not
// guessed: 0x18014b390 is `KERNEL32.dll!EnterCriticalSection` and 0x18014b388
// is `KERNEL32.dll!LeaveCriticalSection`.
//
// Enter/Allocate/Leave around 0x1801fc718 IS
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) with the
// alignment frozen to the immediate 8 — the identity tooltipdialog.cpp proved
// instruction for instruction and context_ctor.cpp uses too. The LOCK is
// therefore NOT reproduced here: it lives INSIDE MemAlloc, which is the door
// being called. Reproducing it here as well would take the allocator's
// critical section twice.
//
// MemAlloc is reached BY ADDRESS because `ResolveAddr` consults the PORT
// REGISTRY FIRST (reference_resolveaddr_consults_port_registry_first.md), so
// this goes through our allocator body. That is the same door
// tooltipdialog.cpp:184 and context_ctor.cpp already use, and it is what keeps
// this body from being the wrapper that calls a ported allocator DIRECTLY —
// the defect `reference_off_by_default_was_not_true.md` records, where an
// UNARMED run mutated retail's arena because one wrapper bypassed the door.
//
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../types/ui/SpriteSheet.h"   // ~SpriteSheet's body lives here
#include "../../types/system/MemoryAllocator.h"
#include "../../types/collection/Collection.h"

namespace {

// ---------------------------------------------------------------------------
// ATTENDANCE — entries, and NOT a verification of anything.
// ---------------------------------------------------------------------------
// Counted unconditionally on every build, exactly as tooltipdialog.cpp does. A green
// readiness screen is not a route (reference_a_green_readiness_screen_is_not_a_
// route.md); a zero here means the row is registered and DEAD.
uint64_t g_newEntries  = 0;
uint64_t g_newBytes    = 0;   // control: the size retail's caller asks for (0x58)
uint64_t g_newNull     = 0;   // control: the allocator refused

struct SpriteSheetNewSummary {
    ~SpriteSheetNewSummary() {
        if (g_newEntries == 0) {
            printf("[RtPort] UI::SpriteSheet::operator new ATTENDANCE: not "
                   "entered in this run.\n");
            fflush(stdout);
            return;
        }
        printf("[RtPort] UI::SpriteSheet::operator new ATTENDANCE (entries, "
               "not comparisons): entries=%llu lastBytes=%llu nulls=%llu\n",
               (unsigned long long)g_newEntries,
               (unsigned long long)g_newBytes,
               (unsigned long long)g_newNull);
        fflush(stdout);
    }
};
SpriteSheetNewSummary g_summary;

// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`, 0x1800c7a50. See the banner:
// the Enter/Allocate/Leave triple this body open-codes IS this function with the
// alignment frozen to 8. Called directly (B3R RT-UI).
void* MemAlloc(uint64_t bytes, int align) {
    return System::MemoryAllocator::MemAlloc(bytes, align);
}

// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`, 0x1800c7ad0 — the matching
// lock/Free/unlock triple.
void MemFree(void* p) {
    System::MemoryAllocator::MemFree(p);
}

}  // namespace

// ============================================================================
// ??2SpriteSheet@UI@@SAPEAX_K@Z                            retail 0x180002e60
// ============================================================================
extern "C" void* RtUi_SpriteSheet_OperatorNew(uint64_t bytes) {

    ++g_newEntries;
    g_newBytes = bytes;                       // retail's caller passes 0x58

    // ---- the body: MOV R8D,0x8 ; CALL 0x1800c6e60, via MemAlloc -------------
    void* p = MemAlloc(bytes, 8);             // the whole shipped body
    if (p == nullptr) ++g_newNull;

    return p;
}

// ============================================================================
// UI leftovers batch (docs/runtime_port/closure/WORKLIST.md) — ~SpriteSheet
// and its class's three per-TU `operator delete` copies.
// ============================================================================

// The exported `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` wrapper, MemFree
// above, is `static`-cached per translation unit already; these three rows
// all reuse that one cache rather than each keeping their own, since it is
// the SAME shipped body (0x1800c7ad0) underneath every one of them.

// `??3SpriteSheet@UI@@SAXPEAX@Z` (retail 0x180002ee0, 93 bytes) — decompiled:
// `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }`, i.e. exactly
// MemFree with the null test open-coded rather than delegated. Byte-identical
// in shape to the ten other per-TU copies runtime/src/object/deleting_dtor.cpp
// already catalogued.
extern "C" void RtUi_SpriteSheet_OperatorDelete(void* p) { MemFree(p); }
// `??3ToolTipDialog@UI@@SAXPEAX@Z` (retail 0x18004d050, 93 bytes). Same body.
extern "C" void RtUi_ToolTipDialog_OperatorDelete(void* p) { MemFree(p); }
// `??3DialogInstance@UI@@SAXPEAX@Z` (retail 0x180045490, 93 bytes). Same body.
extern "C" void RtUi_DialogInstance_OperatorDelete(void* p) { MemFree(p); }

// ============================================================================
// `??1SpriteSheet@UI@@UEAA@XZ` — retail 0x18011d670, ord 686. The REAL
// destructor (ENDGAME B.3.9 UI-D: the registered door is retired).
// Releases the two refcounted String slots at +0x40 (`m_groupName`) then
// +0x38 (`m_textureName`) — in THAT order, per the disassembly — destroys the
// `m_sprites` array (+0x20, Array<SpriteInfo>) through the already-ported
// `~Array<SpriteEntry:0x38>` at 0x180004f50
// (runtime/src/collection/array_destroy.cpp); the restamp before and
// `Object::External::~External` after are the compiler's.
// ============================================================================
UI::SpriteSheet::~SpriteSheet() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    auto release = [](uint8_t* slot) {
        uint8_t* str = *reinterpret_cast<uint8_t**>(slot);
        if (!str) return;
        int32_t* rc = reinterpret_cast<int32_t*>(str - 4);
        if (--*rc == 0) {
            uint8_t* blk = str - 8;
            *reinterpret_cast<uint8_t**>(slot) = blk;
            if (blk) MemFree(blk);
        } else {
            *reinterpret_cast<uint8_t**>(slot) = nullptr;
        }
    };
    release(p + 0x40);   // m_groupName
    release(p + 0x38);   // m_textureName

    // ENDGAME B.3.2 step 2 — left as a free-function call, deliberately. This
    // is array_destroy.cpp's J2_ENTRY(180004f50, ReleaseStringsCounted,
    // kD_Str2_38) — an already-ported, but NOT `rtcoll::ArraySpec`-shaped,
    // destroy: it drives `ReleaseStringsCounted` off a `DestroySpec`
    // (`kD_Str2_38`), a different type and dispatch from the
    // `rtcoll::ArraySpec::destroy` that `Collection::Array<T>::~Array()`'s
    // template body calls. There is no `kSpec_*` row for this address the
    // class could take as an `ExplicitSpec`, so per the batch rule that a
    // "DtorElem" helper may not be convertible at all, `m_sprites`
    // (SpriteSheet.h +0x20, `Collection::Array<SpriteInfo>`) stays reached
    // as a free-function call, by name, rather than forced onto `.~Array()`.
    rtcoll::RtColl_ArrayDtorElem_180004f50(p + 0x20);   // m_sprites, Array<SpriteInfo>
}

