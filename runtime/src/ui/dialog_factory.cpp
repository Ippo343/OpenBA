// ============================================================================
// runtime/src/ui/dialog_factory.cpp — WAVE 11 LANE OBJINT, THE CREATION-PATH PROBE
//
//   ?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ        retail 0x180038530
//   ?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z retail 0x180038610
// ============================================================================
//
// WHY THIS FILE EXISTS. It is not here to bank a row. `WAVE10E_LANE_FINAL.md`
// left `??_7Dialog@UI@@6B@` unpromotable with a written EXPIRY:
//
//   > The row becomes promotable and required the moment
//   > ?CreateNewObject@Dialog@UI@@ (0x180038530) or
//   > ?CreateInPlaceObject@Dialog@UI@@ (0x180038610) is ported and registered.
//   > From that point every resource-loaded UI::Dialog carries whatever
//   > ResolveExport("??_7Dialog@UI@@6B@") returns.
//
// `WAVE11_KICKOFF.md` repeated it as "mechanical and already written down", and
// the lane brief repeated it again. THIS FILE IS THE EXPERIMENT THAT TESTS IT,
// and the prediction it was written to test is that the expiry is WRONG — that
// porting and registering the factory is NECESSARY BUT NOT SUFFICIENT, for a
// reason that is visible in the binary before the run.
//
// ----------------------------------------------------------------------------
// THE STATIC FACT THAT MOTIVATES THE PREDICTION
// ----------------------------------------------------------------------------
// `?CreateNewObject@Dialog@UI@@` and `?CreateInPlaceObject@Dialog@UI@@` have
// **ZERO CODE XREFS** in Runtime-x64.dll. Every reference to either is DATA:
//
//   0x1801faaa8  ?_typeInfo@Dialog@UI@@ + 0x20   (factoryFn)
//   0x1801faab0  ?_typeInfo@Dialog@UI@@ + 0x28   (inPlaceFactoryFn)
//   0x1801b2a44 / 0x1801b282c   .pdata unwind
//   0x18018e09c / 0x18018dfe0   relocations
//   Entry Point [EXTERNAL]      the export table
//
// So the ONE route into either body is an indirect call through a function
// pointer sitting in the DLL's own `.data`, put there by the DLL's static
// initialiser with an in-module `LEA`. That is the same class of thing as a
// `rel32` and an in-DLL vtable stamp: **it is not a door.** Not the import
// table, not the port registry, not a detour. `RegisterRuntimeExport` on the
// mangled name cannot reach it, and neither can `RegisterRuntimeAddr`.
//
// The reader is generic and our tree already owns a copy of it:
// `?CreateObject@Factory@Object@@` (runtime/src/object/type_system.cpp:352)
// ends in `info->factoryFn()`, and `?AddController@DialogInstance@UI@@`
// (runtime/src/ui/dialog_create.cpp:551) does the same by hand. Both are OUR
// code, so both ARE doors — but only for whatever `BaseTypeInfo*` they happen
// to be holding, and that node comes from the AVL that `Factory::Startup`
// builds by walking `?_firstTypeInfo@BaseTypeInfo@Object@@`, which
// runtime/src/object/type_system.cpp:79 resolves BY NAME. By name means the
// DLL's chain head, so every node in that tree is a DLL node, and every
// `factoryFn` in it is the DLL's raw address.
//
// Three conditions therefore have to hold for our factory to run, and the
// published expiry names only the first:
//
//   1. our body exists and is registered              <- this file
//   2. something calls through a BaseTypeInfo whose factoryFn is OUR pointer
//      -> needs the MINTED ?_typeInfo@Dialog@UI@@ row, not the DLL's node
//   3. the chain/AVL the creator walks contains that minted node
//      -> blocked by the `_firstTypeInfo` split brain (WAVE11.md, class A)
//
// ----------------------------------------------------------------------------
// HOW THE PREDICTION IS MEASURED RATHER THAN ARGUED
// ----------------------------------------------------------------------------
// An argument from xrefs is still an argument. Two counters turn it into a
// reading, and they are deliberately INDEPENDENT of each other:
//
//   [DlgFactory] attendance   incremented at the top of each of our two
//                             bodies. It answers "did anything ever call the
//                             port?" — the attendance counter this project has
//                             twice had to add after mistaking SAFE-to-stamp
//                             for a route (reference_a_green_readiness_screen_
//                             is_not_a_route.md).
//
// ----------------------------------------------------------------------------
// ARMING — NONE. BOTH BODIES REGISTER UNCONDITIONALLY. (corrected 2026-09-12)
// ----------------------------------------------------------------------------
// This file used to be opt-in, on the argument below that nothing could reach
// either body anyway. The cutover boot disproved the premise — the MINTED
// `?_typeInfo@Dialog@UI@@` reaches it and died on the unported-factory trap —
// so the flag and its control are gone. See DIRECTION.md section 2.3.
//
// ----------------------------------------------------------------------------
// THE ABORT-TRAP RISK, ENUMERATED RATHER THAN FOOTNOTED
// ----------------------------------------------------------------------------
// The brief asks which of the 104 minted `BaseTypeInfo` factory abort traps
// this probe could reach. The answer is **NONE, and the reason is structural**:
// the traps (`RtMintTiTrapStub_*` in standalone/rt_data_mint_tables.inc) are
// only ever loaded into a minted `BaseTypeInfo` row, and a minted row is only
// installed by `reg_mint_promoted.cpp`. This file
// registers two FUNCTION symbols and mints no data, promotes no row, and does
// not touch `_firstTypeInfo`. Row 24's own two traps
// (`RtMintTiTrapStub_24_0/_24_1`, rt_data_mint_tables.inc:1816-1817) are the
// only ones this subject could ever be adjacent to, and arming this file makes
// them neither more nor less reachable than they were.
//
// ----------------------------------------------------------------------------
// THE ALLOCATOR IS ON THIS PATH, SAID EXPLICITLY
// ----------------------------------------------------------------------------
// `CreateNewObject` allocates: retail inlines `System::MemAlloc(0x88, 8)`
// (0x1800c7a50) — lock `_singleton@MemoryAllocator + 0x640`, call
// `?Allocate@MemoryAllocator@System@@` (0x1800c6e60), unlock. This port reaches
// it through `ResolveAddr(Module::Runtime, 0x1800c7a50)`, which consults the
// port registry FIRST, so our allocator answers. One heap is used either way.
//
// `CreateInPlaceObject` allocates NOTHING — it is placement construction on a
// caller-owned buffer — so the in-place half of the probe is allocator-free.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"

// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: resolve.h drags in <windows.h>. Byte-identical to
// resolve.h's declarations; the linker checks them.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// `??0Dialog@UI@@QEAA@XZ` (0x180112db0), ported and VERIFIED by Wave 10 lane
// DLG2 (runtime/src/ui/dialog_ctor_dtor.cpp). See the faithfulness note on
// CreateNewObject below for why calling it out-of-line is the right transcription.
extern "C" void* RtUiDialog_Ctor(void* self);

namespace {

// ---- attendance ------------------------------------------------------------
uint64_t g_attendNew     = 0;
uint64_t g_attendInPlace = 0;

// ---- `?MemAlloc@System@@YAPEAX_KH@Z` 0x1800c7a50 ---------------------------
// BY ADDRESS on purpose, and the address is the LOCKED wrapper, not
// `?Allocate@` (0x1800c6e60) which retail calls with the lock already held.
// Retail inlines the whole lock/Allocate/unlock triple here; calling the
// out-of-line wrapper reproduces it exactly, including the lock order.
// `ResolveAddr(Module::Runtime, …)` consults the port registry first, so this
// follows the tree without this file changing
// (reference_resolveaddr_consults_port_registry_first.md).
void* MemAllocLocked(uint64_t size, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    // Re-resolve on a miss rather than freezing a null: a caller that ran
    // before the allocator's arming path supplied this address would otherwise
    // poison the static for the life of the process.
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(size, align) : nullptr;
}

// THE ALLOCATION SHAPE, read off 0x180038561-0x180038571:
//     MOV EDX,0x88          ; size
//     LEA R8D,[RDX + -0x80] ; align = 0x88 - 0x80 = 8   (retail computes it!)
//     MOV RCX,[_singleton@MemoryAllocator]
//     CALL ?Allocate@
// The `LEA R8D,[RDX-0x80]` is a code-size trick for `MOV R8D,8`; it is written
// as the literal 8 here because that is the value, and the derivation would
// read as a relationship between size and alignment that does not exist.
constexpr uint64_t kDialogSize  = 0x88;
constexpr int      kDialogAlign = 8;

}  // namespace

// ============================================================================
// ?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z   0x180038610
// ============================================================================
//
//     Base* Dialog::CreateInPlaceObject(void* p)
//     { if (!p) return nullptr; new (p) Dialog(); return p; }
//
// Retail's body is the `??0Dialog@UI@@` constructor INLINED, and the inlining
// is complete — 0x180038626..0x180038696 is instruction-for-instruction the
// same store sequence the out-of-line ctor at 0x180112db0 performs, including
// the three vtable stamps in order (`??_7Base@Object@@6B@` 0x180154788,
// immediately overwritten UNREAD by `??_7External@Object@@6B@` 0x1801547a0,
// then `??_7Dialog@UI@@6B@` 0x1801562a0), the +0x30 = 0xffffffff sentinel, and
// the +0x44 four-byte hole that no store covers.
//
// TRANSCRIPTION CHOICE, stated because it is a choice. This port calls the
// out-of-line ctor rather than re-transcribing the inlined stores. Behaviour is
// identical (same stores, same order, same hole) and the alternative would
// clone a 40-line body that Wave 10 lane DLG2 already verified against the
// shipped export byte-for-byte over the whole 0x88 extent — cloning it would
// create a second copy to keep in sync and would not be more faithful, only
// longer. This is the same move UI3 made in the other direction when it
// replaced retail's INLINED Dialog temporary with a call to the out-of-line
// export.
//
// THE NULL CHECK IS RETAIL'S, not a guard added here: 0x180038621
// `TEST RCX,RCX / JZ 0x18003869b`, and the null branch returns 0 in EAX.
extern "C" void* RtUiDialog_CreateInPlaceObject(void* p) {
    ++g_attendInPlace;
    if (p == nullptr) return nullptr;      // 180038621 / 18003869b
    RtUiDialog_Ctor(p);
    return p;                              // 180038696 MOV RAX,RCX
}

// ============================================================================
// ?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ           0x180038530
// ============================================================================
//
//     Base* Dialog::CreateNewObject()
//     { void* p = System::MemAlloc(0x88, 8); if (!p) return nullptr;
//       new (p) Dialog(); return p; }
//
// Same inlined ctor as above, preceded by the inlined `MemAlloc`. The null
// check at 0x18003858a is retail's and covers an allocation failure.
//
// THE `MOV [RSP+0x40]` STORES ARE NOT STATE. 0x180038552 and 0x180038582 write
// the lock pointer and then the fresh object into a stack slot; that slot is
// the MSVC exception-unwind frame's live-object record (`MOV [RSP+0x20],-2` at
// the top is the state index). It is dead in a build with no unwind tables and
// is deliberately not reproduced — reproducing it would write a local nothing
// reads and imply state that is not there.
extern "C" void* RtUiDialog_CreateNewObject() {
    ++g_attendNew;
    void* p = MemAllocLocked(kDialogSize, kDialogAlign);
    if (p == nullptr) return nullptr;      // 18003858a / 180038601
    RtUiDialog_Ctor(p);
    return p;                              // 1800385fc MOV RAX,RDI
}

// ============================================================================
// Registration + the attendance report
// ============================================================================
namespace {
// Printed from a static destructor, exactly as `[DlgTrip]`'s TripReport does,
// so the reading lands even on a route that exits without reaching a summary
// hook — and so the two instruments report by the same mechanism.
void DialogFactoryReport() {
    printf("[DlgFactory] attendance: CreateNewObject=%llu CreateInPlaceObject=%llu\n",
                (unsigned long long)g_attendNew,
                (unsigned long long)g_attendInPlace);
}

struct FactoryReport { ~FactoryReport() { DialogFactoryReport(); } };
FactoryReport g_factoryReport;
}  // namespace

// ----------------------------------------------------------------------------
// THE PREDICTION IN THIS FILE'S HEADER IS NOW SETTLED, AND IT WAS RIGHT ABOUT
// THE DLL-PRESENT ROUTE AND WRONG ABOUT THE ONE THAT MATTERS. (2026-09-12)
//
// The header argues that registering these two names cannot redirect anything,
// because the only reference to either body is `?_typeInfo@Dialog@UI@@`'s
// `factoryFn` field, written by the DLL's own static initialiser -- not a door.
// That holds WHILE THE DLL IS PRESENT and nothing below contradicts it.
//
// At CUTOVER there is no DLL and no DLL-written `factoryFn`: the BaseTypeInfo
// the creator walks is the MINTED one (standalone/rt_data_mint.cpp), whose
// `factoryFn` is filled FROM THE PORT REGISTRY at mint time, by name first and
// then by address -- exactly the two doors this function opens. So condition 2
// and condition 3 of the header's three are both satisfied by the mint, and
// registering here is not merely sufficient, it is REQUIRED.
//
// It was required on 2026-09-12: the cutover boot died with
//     [RtMint] FATAL: minted BaseTypeInfo::factoryFn is not ported
//     ?_typeInfo@Dialog@UI@@ slot 0 -> ?CreateNewObject@Dialog@UI@@
// because this registration was behind an opt-in flag the cutover arm did not
// set. The flag is gone (DIRECTION.md section 2.3: a port registers
// unconditionally), and with it the in-place "effectiveness control".
int RtUiDialogFactoryRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&RtUiDialog_CreateInPlaceObject);
    RegisterRuntimeAddr(0x180038610, (void*)&RtUiDialog_CreateInPlaceObject);  ++n;

    RegisterRuntimeExport("?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&RtUiDialog_CreateNewObject);
    RegisterRuntimeAddr(0x180038530, (void*)&RtUiDialog_CreateNewObject);      ++n;
    return n;
}
