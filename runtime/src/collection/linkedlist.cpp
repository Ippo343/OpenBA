// ============================================================================
// runtime/src/collection/linkedlist.cpp — Wave 9 lane UIC
//   Collection::LinkedList<T=8 bytes, DefaultAllocator> — the unexported
//   out-of-line helpers UI::UserInterface::BringDialogToFront calls.
//
//   ENDGAME B.3.2 step 1: the bodies themselves now live on the class template
//   in runtime/types/collection/Collection.h; what is left here is the
//   allocator seam and the four one-line address doors. See "WHERE THE BODIES
//   WENT" below the original write-up.
// ============================================================================
//
// The container layout is NOT recovered here — it was already recovered, twice,
// and this file only ports the bodies that operate on it:
//
//   runtime/types/collection/Collection.h  `Collection::LinkedList`
//       head +0x00, tail +0x08, count +0x10, sizeof 0x18; node
//       {element +0x00, prev +0x08, next +0x10}, sizeof 0x18 for T=8.
//       Pinned there by UI::UserInterface::CleanRemovedDialogs (0x18011f790).
//   runtime/types/ui/UserInterface.h       `_dialogs` at UserInterface+0x00
//
// The three bodies, all UNEXPORTED (checked against the shipped export
// directory with tools/uic_probe.py, which reads the PE export table — none of
// the three has a name, so all three are registered by ADDRESS only and there
// is nothing here for tools/export_name_audit.py to check):
//
//   | retail      | what it is                              | reached from      |
//   |-------------|-----------------------------------------|-------------------|
//   | 0x180009c30 | the allocator-lock Free wrapper         | 0x18007a010 (tail |
//   |             | `if(p){Enter;Free(_singleton,p);Leave;}`| jump), and THREE  |
//   |             | first argument taken and NEVER READ     | existing tree     |
//   |             |                                         | call sites (below)|
//   | 0x18007a010 | LinkedList::Remove(list, &value) —      | BringDialogToFront|
//   |             | find by VALUE, unlink, free the node    | 0x180120440       |
//   | 0x180032750 | LinkedList node alloc + init            | BringDialogToFront|
//
// ---------------------------------------------------------------------------
// WHY THE TWO ALLOCATOR BODIES ARE NOT REWRITTEN HERE
// ---------------------------------------------------------------------------
// 0x180009c30 and 0x180032750 each open-code an allocator call under the
// allocator's own critical section. Both are the SAME INSTRUCTION SEQUENCE as a
// body this tree already ports, and that is established by reading both, not by
// eye:
//
//   0x1800c7ad0  ?MemFree@MemoryAllocator@System@@SAXPEAX@Z   (ported, lane E,
//                registered at runtime/src/system/allocator.cpp:1296/1399)
//     TEST RCX,RCX / JZ ret / PUSH RDI / SUB RSP,0x30 / MOV [RSP+0x20],-2 /
//     MOV [RSP+0x48],RBX / MOV RDI,RCX / MOV RBX,[0x1801fc718] / ADD RBX,0x640 /
//     MOV [RSP+0x40],RBX / MOV RCX,RBX / CALL [0x18014b390] / NOP /
//     MOV RDX,RDI / MOV RCX,[0x1801fc718] / CALL 0x1800c6c00 / NOP /
//     MOV RCX,RBX / CALL [0x18014b388] / ... / RET
//
//   0x180009c30
//     TEST RDX,RDX / JZ ret / MOV [RSP+0x8],RCX / PUSH RDI / SUB RSP,0x30 /
//     MOV [RSP+0x20],-2 / MOV [RSP+0x48],RBX / MOV RDI,RDX / ...IDENTICAL...
//
// The ONLY differences are (a) which register carries the pointer — RCX there,
// RDX here — and (b) one extra `MOV [RSP+0x8],RCX` that homes the first
// argument and never reads it back. So 0x180009c30 IS MemFree with an ignored
// leading argument. It is implemented below as exactly that, which keeps ONE
// body for the pair rather than a second copy that can drift.
//
// The same argument holds for the allocating half:
//
//   0x1800c7a50  ?MemAlloc@…@@SAPEAX_KH@Z (ported, lane E)
//     ... Enter([0x1801fc718]+0x640) ; Allocate(_singleton, RDX=bytes, R8D=align)
//         via CALL 0x1800c6e60 ; Leave ; return RAX
//   0x180032750
//     ... Enter([0x1801fc718]+0x640) ; MOV EDX,0x18 / LEA R8D,[RDX-0x10] (=8) /
//         MOV RCX,[0x1801fc718] / CALL 0x1800c6e60 ; Leave
//
// i.e. its allocation is `MemAlloc(0x18, 8)` spelled inline. Reproduced as that
// call, so the node allocation goes through the ONE ported allocator entry
// point instead of a private second copy of the lock protocol. This matters at
// cutover: 0x1800c7a50/0x1800c7ad0/0x1800c6e60/0x1800c6c00 are the four
// addresses `RegisterAllocatorAddresses()` arms (allocator.cpp, commit 309c015)
// — reaching the heap any other way from here would re-open the wall attempt 13
// closed.
//
// ---------------------------------------------------------------------------
// THIS REGISTRATION CHANGES BEHAVIOUR AT THREE EXISTING CALL SITES
// ---------------------------------------------------------------------------
// `ResolveAddr(Module::Runtime, …)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so registering
// 0x180009c30 redirects these, which reach it by address today:
//
//   runtime/src/graphics/scene.cpp:239   RtFreeLocked
//   runtime/src/input/input.cpp:139      RtInput_KeyMap_ClearAll's freeNode
//
// That redirection is the POINT, not a side effect: all three were an
// unregistered address door, i.e. a null call after cutover.
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// NONE for Remove and for the node allocator, and no attempt is made to
// manufacture one. Both are destructive and stateful in the strict sense:
// replaying Remove unlinks a node that is already unlinked and frees it a
// second time; replaying the allocator leaks a 24-byte node per call and
// returns a different pointer, so a comparison of the return value is
// guaranteed to fail for a reason that says nothing about the port. These ship
// in the disposition lane M established and lanes UIF/Z reused: a NO-ORACLE
// port with NO call into the shipped body at all.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

// ---------------------------------------------------------------------------
// ENDGAME B.3.2 STEP 1 (LIST core) — WHERE THE BODIES WENT
// ---------------------------------------------------------------------------
// The four bodies above are now MEMBERS of
// `Collection::LinkedList<T, Allocator>` in
// runtime/types/collection/Collection.h, each carrying the transcription notes
// that used to sit here next to it. Retail has exactly ONE instantiation of
// that template (T = an 8-byte pointer), and at T = 8 the generic bodies
// compile to the same stores this file used to make by hand — the only change
// is that `0x18`, `+0x8` and `+0x10` are now `sizeof(Node)` and field names.
//
// What is left in this file is the two things the header cannot hold:
//
//   1. `Collection::detail::ListNodeAlloc/ListNodeFree` — the allocator seam,
//      which needs `ResolveAddr` and therefore must stay out of a types header
//      (PLAN.md §4.1 decision 1).
//   2. The four `extern "C"` entry points, now ONE LINE EACH, and their
//      `RegisterRuntimeAddr` rows. Every row that was here is still here:
//      callers reach the class through them until B.3.2 step 2 converts the
//      call sites.
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"

#ifdef BANISHED_UNIT_TEST
#include <cstdlib>
#endif

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — same reasoning array.cpp gives (resolve.h drags in <windows.h>, whose
// <wingdi.h> declares a function called `Rectangle`). Must stay byte-identical
// to resolve.h's declaration; the linker checks the two against each other.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The one retail instantiation. Named here so the shims below and the layout
// asserts read against the same type the header defines.
using List8 = Collection::LinkedList<void*>;
using Node8 = List8::Node;
static_assert(sizeof(Node8) == 0x18, "LinkedList::Node(T=8) is 0x18 bytes");
static_assert(offsetof(Node8, prev) == 0x08, "Node::prev @ +0x08");
static_assert(offsetof(Node8, next) == 0x10, "Node::next @ +0x10");
static_assert(sizeof(List8) == 0x18, "Collection::LinkedList is 0x18 bytes");
static_assert(offsetof(List8, _head)  == 0x00, "LinkedList::_head @ +0x00");
static_assert(offsetof(List8, _tail)  == 0x08, "LinkedList::_tail @ +0x08");
static_assert(offsetof(List8, _count) == 0x10, "LinkedList::_count @ +0x10");

#ifndef BANISHED_UNIT_TEST
namespace {

// The allocator boundary. RETRIES A NULL rather than caching one, which is the
// RtResolveCached contract and is load-bearing rather than defensive: at
// cutover these addresses are supplied by RegisterAllocatorAddresses() inside
// the allocator's own arming path, so a caller that ran before that point would
// otherwise freeze a null in for the life of the process
// (WAVE9.md, "Attempt 14", the second defect the crash was hiding).
void* MemAllocLocked(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

void MemFreeLocked(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn) fn(p);
}

}  // namespace
#endif  // !BANISHED_UNIT_TEST

// ============================================================================
// Collection::detail — the node allocation seam the header declares
// ============================================================================
//
// In the SHIPPING build these are exactly the two calls the bodies above
// argued for: `MemAlloc(sizeof(Node), 8)` and `MemFree(node)`, i.e. the ONE
// ported allocator entry point rather than a private copy of the lock
// protocol. Reaching the heap any other way from here would re-open the wall
// attempt 13 closed (see this file's header block).
//
// In the HOST UNIT TEST build (`make unit`, -DBANISHED_UNIT_TEST) there is no
// address registry and no MemoryAllocator singleton to resolve, so
// `ResolveAddr` answers 0 and the shipping path would allocate nothing at all —
// every list would silently stay empty and every assertion in list_test.cpp
// would be vacuous. B31_RULES class rule 6 allows a test-only allocator for
// exactly this; this is it, and it is the ONLY behavioural difference between
// the two builds in this file.
//
// The test allocator carries a 16-byte header with a state word, so that
// freeing a block twice is DETECTED rather than tolerated: that is what makes
// "Clear frees every node exactly once" a real assertion instead of a count
// that a double free and a leak would cancel out of.
// ============================================================================
namespace Collection {
namespace detail {

#ifdef BANISHED_UNIT_TEST
uint64_t g_listNodeAllocs      = 0;   // read by tests/unit/list_test.cpp
uint64_t g_listNodeFrees       = 0;
uint64_t g_listNodeDoubleFrees = 0;

namespace {
constexpr uint64_t kLive = 0x4c495645ull;   // 'LIVE'
constexpr uint64_t kDead = 0x44454144ull;   // 'DEAD'
}  // namespace

void* ListNodeAlloc(uint64_t bytes, int align) {
    (void)align;                     // malloc is already 16-aligned on x64
    uint8_t* base = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(bytes) + 16));
    if (!base) { return nullptr; }
    *reinterpret_cast<uint64_t*>(base) = kLive;
    ++g_listNodeAllocs;
    return base + 16;
}

void ListNodeFree(void* p) {
    if (!p) { return; }              // retail's own `TEST RDX,RDX / JZ ret`
    uint8_t*  base  = static_cast<uint8_t*>(p) - 16;
    uint64_t& state = *reinterpret_cast<uint64_t*>(base);
    if (state != kLive) { ++g_listNodeDoubleFrees; return; }
    state = kDead;
    ++g_listNodeFrees;
    std::free(base);
}
#else
void* ListNodeAlloc(uint64_t bytes, int align) {
    return MemAllocLocked(bytes, align);
}

void ListNodeFree(void* p) {
    MemFreeLocked(p);
}
#endif

}  // namespace detail
}  // namespace Collection

// ============================================================================
// The four entry points — one line each onto the class
// ============================================================================
// The transcription notes for each moved to the member it became; the retail
// address stays on both sides so a crash log naming either one leads to the
// other.

// ---- 0x180009c30 — the allocator-lock Free wrapper -------------------------
// The first argument is homed at [RSP+0x8] and never read again. Retail passes
// the container there at every call site (`this`), so the parameter is kept and
// named honestly rather than deleted — deleting it would silently change the
// ABI of an address every other caller reaches through a 2-argument typedef
// (runtime/src/graphics/scene.cpp:238 and runtime/src/input/input.cpp:131 both
// declare it that way).
//
// It goes to the seam and NOT to `List8::FreeNode` even though that member is
// the same one line: retail's callers pass a `Set*`, an unrelated container, or
// (avl_trees.cpp:1170) outright junk in that slot, so spelling this as a member
// call would invent a `this` that no caller promises.
extern "C" void RtColl_FreeLocked_180009c30(void* /*ignoredThis*/, void* p) {
    Collection::detail::ListNodeFree(p);
}

// ---- 0x180032750 — LinkedList node allocate + initialise -------------------
// THE FIRST ARGUMENT IS NEVER READ (the whole body touches RCX only as the
// destination of `XOR ECX,ECX`), which is why `NewNode` is a static member.
extern "C" void* RtColl_LinkedListNewNode_180032750(void* /*neverRead*/,
                                                    void* const* value) {
    return List8::NewNode(*value);
}

// ---- 0x18007a010 — LinkedList::Remove(list, &value) ------------------------
extern "C" void RtColl_LinkedListRemove_18007a010(void* listPtr,
                                                  void* const* value) {
    static_cast<List8*>(listPtr)->Remove(*value);
}

// ---- 0x1800325f0 — LinkedList::Clear(list) ---------------------------------
// WAVE 9 LANE UID. The member `??1DialogInstance@UI@@` (0x180114d40) calls on
// its `_children` list at 0x180114e5e.
//
// ORACLE: none, and none is possible — the same reason `Remove` has none. A
// replay walks a chain whose every node has already been freed and frees each
// one a second time. Two things stand in for it, and neither is dressed up as
// more than it is:
//   * A SHAPE CHECK against the shipped bytes, `VerifyListClearShape` in
//     runtime/src/ui/dialog.cpp, run beside the `??1DialogInstance` shape check.
//     The entire failure surface of this body is four displacements — the
//     `next` offset it walks and the three fields it zeroes — and all four are
//     literal displacements in the shipped instruction stream, so all four are
//     compared against the DLL while the DLL is still there.
//   * `LinkedListClearFreesEveryNodeOnce` in tests/unit/list_test.cpp, which
//     runs the member against the counting test allocator above.
extern "C" void RtColl_LinkedListClear_1800325f0(void* listPtr) {
    static_cast<List8*>(listPtr)->Clear();
}

// ============================================================================
// Registration — BY ADDRESS ONLY
// ============================================================================
// None of these is in the shipped export directory, so there is no mangled
// name to register under and nothing for tools/export_name_audit.py to verify.
// The addresses are spelled as hex literals in the RegisterRuntimeAddr calls
// because tools/runtime_census.py counts the literal in the call and cannot see
// through a named constant
// (reference_census_cannot_see_nonliteral_registrations.md).
int RtCollLinkedListRegister() {
    int n = 0;
    // 0x180009c30 is NOT registered here any more. It was registered by THREE
    // files (this one, io/modification.cpp, system/allocator.cpp), the registry's
    // last binding won, and which body survived depended on init order -- lane
    // VID2 found it. runtime/src/system/allocator.cpp is now the single owner and
    // registers it UNCONDITIONALLY from RtSystemPortInit, so this file's callers
    // still reach a correct body. RtColl_FreeLocked_180009c30 is kept because it
    // is the named spelling of `LinkedList::FreeNode` that scene.cpp and
    // input.cpp reach through a 2-argument typedef.
    RegisterRuntimeAddr(0x180032750, (void*)&RtColl_LinkedListNewNode_180032750); ++n;
    RegisterRuntimeAddr(0x18007a010, (void*)&RtColl_LinkedListRemove_18007a010);  ++n;
    // WAVE 9 LANE UID — Clear. Unexported, so ADDRESS ONLY like the two above;
    // verified absent from the shipped export directory with pefile BEFORE the body
    // was written (reference_resolveexport_phantom_names.md). Its only caller today
    // is `??1DialogInstance@UI@@`, which reaches it by address at 0x180114e5e.
    RegisterRuntimeAddr(0x1800325f0, (void*)&RtColl_LinkedListClear_1800325f0);   ++n;
    return n;
}
