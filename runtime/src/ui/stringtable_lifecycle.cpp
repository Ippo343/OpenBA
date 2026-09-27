// ============================================================================
// runtime/src/ui/stringtable_lifecycle.cpp
//   WAVE 28 LANE MISC -- `??1StringTable@UI@@UEAA@XZ` and
//                        `??3StringTable@UI@@SAXPEAX@Z`
// ============================================================================
//
// ENDGAME B.3.9 block UI-D: the `??1` DOOR IS RETIRED — its body is the real
// `UI::StringTable::~StringTable` (stringtable_factory.cpp) and its only
// reader, slot 0 of the minted table, is retired with the row. The banner
// below is the Wave 28 record of both bodies; only `??3` is still here.
//
// These were the WHOLE wave's only DTOR gap row and only OPNEWDEL gap row
// (WAVE28_KICKOFF.md section 1: "gap 153 = 26 BODY + 1 DTOR + 1 OPNEWDEL + ...").
// Both belong to UI::StringTable, whose factory, accessors and nine vtable
// slots were closed across Waves 26 and 27 in stringtable_factory.cpp.
//
// WHY A SEPARATE FILE AND NOT stringtable_factory.cpp.  Purely a merge-hazard
// decision.  Three lanes of this wave are on the factory family and
// stringtable_factory.cpp is that family's file; appending to its tail from a
// private worktree is how Wave 27 collected the merge hazard its kickoff
// warning 10 is about.  Nothing here is conceptually separate from that file
// and its banner should be read first.
//
// ---------------------------------------------------------------------------
// SYMBOL SPELLINGS, CHECKED AGAINST THE EXPORT DIRECTORY, NOT INFERRED
// ---------------------------------------------------------------------------
// pefile over the shipped C:\GOG Games\Banished\Runtime-x64.dll, matched both
// by NAME and by RVA (the kickoff's warning 5 and the `UEBA`-vs-`QEBA` cost
// Wave 27 paid):
//     ??1StringTable@UI@@UEAA@XZ      0x18011d8f0   ord  691
//     ??3StringTable@UI@@SAXPEAX@Z    0x180002220   ord 1013
// `UEAA` is the VIRTUAL spelling and it is the one that exists -- `QEAA` would
// have resolved NULL in silence.  `SAXPEAX` on the operator delete is `static`,
// not "unexported"; this DLL exports its private and static members
// (reference_aeaa_is_private_not_unexported.md).
//
// ---------------------------------------------------------------------------
// `??1StringTable@UI@@UEAA@XZ` 0x18011d8f0 -- read off objdump, 0x37 bytes
// ---------------------------------------------------------------------------
//     18011d906  LEA RAX,[0x180154630] ; MOV [RCX],RAX   re-stamp OUR vptr
//     18011d910  ADD RCX,0x20 ; CALL 0x180004a90         ~Array<TextEntry>
//     18011d91a  MOV RCX,RBX ; JMP 0x1800bc950           TAIL-JMP ~External
//
// Three steps, and all three callees already exist in this tree:
//
//   * 0x180154630 `??_7StringTable@UI@@6B@` is a DATA row the mint already
//     carries; taken through RT_DATA_SLOT so the stamp follows a promotion of
//     that row without this file changing -- the same door
//     stringtable_factory.cpp's ConstructBody uses, so ctor and dtor cannot
//     drift apart.
//
//   * 0x180004a90 `~Array<UI::StringTable::TextEntry>` is NOT exported (pefile
//     returns nothing at that RVA; it is a compiler-emitted internal), and it
//     is PORTED AND REGISTERED BY ADDRESS already -- Wave 27 lane FRONTIER,
//     runtime/src/collection/array_destroy.cpp:751, the twelfth J2 entry, spec
//     kD_Str2_10.  `ResolveAddr(Module::Runtime, ...)` is registry-FIRST
//     (reference_resolveaddr_consults_port_registry_first.md), so this call
//     reaches FRONTIER's body, not the DLL's, the moment both are linked.  It
//     is therefore NOT a new cutover door: the address is already satisfied.
//
//   * 0x1800bc950 IS exported, as `??1External@Object@@UEAA@XZ` (ord 588), and
//     is ported (Wave 9 lane B2).  Reached BY NAME for that reason; several
//     sibling files reach it by address and both doors are registered.
//
// The tail-JMP is reproduced as an ordinary call followed by a return.  That is
// not a liberty: a tail-JMP and a call-then-return differ only in the stack
// frame, and this function returns void, so there is no return value to carry.
//
// ---------------------------------------------------------------------------
// `??3StringTable@UI@@SAXPEAX@Z` 0x180002220 -- and the thing worth reading
// ---------------------------------------------------------------------------
//     180002220  TEST RCX,RCX ; JZ ret                   the null guard
//     18000223b  RBX = *(0x1801fc718) + 0x640            the allocator's lock
//     180002251  CALL [0x18014b390]                      Enter
//     180002262  CALL 0x1800c6c00(*(0x1801fc718), p)     MemoryAllocator::Free
//     18000226b  CALL [0x18014b388]                      Leave
//
// *** THIS BODY IS INSTRUCTION-FOR-INSTRUCTION IDENTICAL TO
// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0, ord 3228). ***
// Not "similar", not "the same shape": the two listings were diffed line by
// line and every instruction, every displacement and every immediate agrees,
// including the `MOV [RSP+0x20],-2` SEH state store and the `REPZ RET`.  MSVC
// emitted the class's `operator delete` as a duplicate of the global free.
//
// That is what licenses the one-line body below, and it is worth saying why the
// licence is narrow.  0x1800c7ad0 re-tests null itself (0x1800c7ad0 `TEST
// RCX,RCX ; JZ`), so the guard is not lost by delegating; and 0x1800c7ad0 is
// reached through `ResolveAddr`, registry-first, so this free lands in our
// arena -- which is exactly what retail's own bytes do, because retail's own
// bytes call the same allocator singleton at 0x1801fc718.
//
// The one thing NOT claimed: this is a duplicate-body finding about the SHIPPED
// retail binary and was not re-checked against Runtime-x64-profile.dll.  A
// profile build may have merged or not merged these two the same way.  Nothing
// here depends on the profile build.
//
// ---------------------------------------------------------------------------
// WHAT THIS CLOSES BESIDES TWO CENSUS ROWS
// ---------------------------------------------------------------------------
// `RtUiStringTable_ScalarDelete` (stringtable_factory.cpp:718), the class's
// vtable slot 0, currently reaches BOTH of these through the DLL:
//     :742  ResolveAddr(Module::Runtime, 0x18011d8f0)      -> the dtor
//     :750  RT_EXPORT_ONCE(FreeFn, "??3StringTable@UI@@SAXPEAX@Z")
// Both of those doors are registry-aware, so registering the two names and the
// two addresses here makes slot 0 self-contained at cutover without touching
// that file.  That is a CUTOVER claim and it is INFERRED from how the two
// resolve doors are documented to behave; it is NOT measured, because nothing
// in an ordinary DLL-present run dispatches slot 0 (see the attendance note).
//
// ---------------------------------------------------------------------------
// ATTENDANCE, AND WHAT A ZERO MEANS
// ---------------------------------------------------------------------------
// Counters, not a verification.  A zero here is the EXPECTED reading with the
// DLL present for the same reason stringtable_factory.cpp's banner gives at
// length: nothing in this tree constructs or destroys a UI::StringTable through
// a door while the DLL owns the class.  A green readiness screen is not a route
// (reference_a_green_readiness_screen_is_not_a_route.md).
//
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/ui/StringTable.h"

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

uint64_t g_deleteEntries = 0;
uint64_t g_deleteNull    = 0;   // retail's own null branch was taken

using VoidPtrFn = void (*)(void*);

void StringTableLifecycleReport() {
    printf("[RtPort] UI::StringTable lifecycle ATTENDANCE: "
                "operator-delete=%llu (null=%llu)\n",
                (unsigned long long)g_deleteEntries,
                (unsigned long long)g_deleteNull);
    fflush(stdout);
}
struct LifecycleReport {
    ~LifecycleReport() { if (g_deleteEntries) StringTableLifecycleReport(); }
};
LifecycleReport g_lifecycleReport;

}  // namespace

// ============================================================================
// ??3StringTable@UI@@SAXPEAX@Z                                  0x180002220
// ============================================================================
extern "C" void RtUiStringTable_OperatorDelete(void* p) {
    ++g_deleteEntries;
    if (p == nullptr) { ++g_deleteNull; return; }        // 180002220 / 18000227b

    // The Enter / Free / Leave triple, byte-identical to 0x1800c7ad0
    // (?MemFree@MemoryAllocator@System@@SAXPEAX@Z) -- see the banner for the
    // instruction-by-instruction diff that licenses delegating to it instead of
    // opening a second set of doors on the allocator singleton, its lock and
    // the two critical-section imports. Registry-first, so this follows the
    // tree's allocator rather than pinning one.
    static VoidPtrFn memFree = nullptr;
    if (!memFree)
        memFree = reinterpret_cast<VoidPtrFn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (memFree) {
        memFree(p);
    } else {
        std::fprintf(stderr, "[RtPort] StringTable::operator delete: 0x1800c7ad0 "
                             "resolved NULL -- %p was NOT freed\n", p);
    }
}

// ============================================================================
// Registration
// ============================================================================
// BOTH doors for each row: the mint and our own call sites try the NAME first
// and the ADDRESS second, a name-only registration leaves any by-address
// consumer on the DLL, and an address-only one is invisible to the census
// (reference_by_address_door_dies_at_cutover.md,
//  reference_census_cannot_see_nonliteral_registrations.md -- which is also why
//  each name is spelled as its own string literal here).
int RtUiStringTableLifecycleDtorRegister() {
    int n = 0;
    RegisterRuntimeExport("??3StringTable@UI@@SAXPEAX@Z", (void*)&RtUiStringTable_OperatorDelete);
    RegisterRuntimeAddr(0x180002220ull, (void*)&RtUiStringTable_OperatorDelete);    ++n;

    return n;
}
