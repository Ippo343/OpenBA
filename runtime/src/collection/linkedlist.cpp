// ============================================================================
// runtime/src/collection/linkedlist.cpp — Wave 9 lane UIC
//   Collection::LinkedList<T=8 bytes, DefaultAllocator> — the three unexported
//   out-of-line helpers UI::UserInterface::BringDialogToFront calls.
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
// unregistered address door, i.e. a null call after cutover. It is also why
// this file has its own kill switch, `BANISHED_RT_LINKEDLIST=0`, separate from
// BANISHED_RT_COLLECTION — an integration run that regresses in the graphics or
// input path can take this out without giving up the Array ports.
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
// What they do carry is liveness: RT_DIFF_WORK reports how many entered calls
// actually unlinked / actually allocated. `calls=` will read 0 at these sites
// because nothing is compared — `worked=` is the whole signal here, exactly as
// WAVE9_LANE_UIF.md §5 records for its own three no-oracle bodies.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_diff.h"
#include "../../funcs/rt_port.h"

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — same reasoning array.cpp gives (resolve.h drags in <windows.h>, whose
// <wingdi.h> declares a function called `Rectangle`). Must stay byte-identical
// to resolve.h's declaration; the linker checks the two against each other.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// The 8-byte-element instantiation, which is the only one these three bodies
// serve (the element is an Object::InternalPtr, one pointer). Mirrors
// Collection.h's LinkedList<T=8> exactly; asserted rather than assumed.
struct LlNode {
    void*   element;   // +0x00
    LlNode* prev;      // +0x08
    LlNode* next;      // +0x10
};
struct LlList {
    LlNode* head;      // +0x00
    LlNode* tail;      // +0x08
    int32_t count;     // +0x10
    int32_t pad14;     // +0x14  never read or written by any of the three
};
static_assert(sizeof(LlNode) == 0x18, "LinkedList::Node(T=8) is 0x18 bytes");
static_assert(offsetof(LlNode, prev) == 0x08, "Node::prev @ +0x08");
static_assert(offsetof(LlNode, next) == 0x10, "Node::next @ +0x10");
static_assert(sizeof(LlList) == 0x18, "Collection::LinkedList is 0x18 bytes");
static_assert(offsetof(LlList, head)  == 0x00, "LinkedList::_head @ +0x00");
static_assert(offsetof(LlList, tail)  == 0x08, "LinkedList::_tail @ +0x08");
static_assert(offsetof(LlList, count) == 0x10, "LinkedList::_count @ +0x10");

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

// ---- 0x180009c30 — the allocator-lock Free wrapper -------------------------
//
//     TEST RDX,RDX / JZ ret        ; the null test is on the SECOND argument
//     ... Enter / Free(_singleton, RDX) / Leave ...
//
// The first argument is homed at [RSP+0x8] and never read again. Retail passes
// the container there at every call site (`this`), so the parameter is kept and
// named honestly rather than deleted — deleting it would silently change the
// ABI of an address every other caller reaches through a 2-argument typedef
// (runtime/src/graphics/scene.cpp:238 and runtime/src/input/input.cpp:131 both
// declare it that way).
//
// The `TEST RDX,RDX` guard is retail's own and is reproduced by MemFree's
// identical first instruction (`TEST RCX,RCX; JZ ret` at 0x1800c7ad0) — this is
// not a null check added to paper over a crash.
extern "C" void RtColl_FreeLocked_180009c30(void* /*ignoredThis*/, void* p) {
    RT_DIFF_BEGIN("Collection::FreeLocked(0x180009c30)");
    MemFreeLocked(p);
    RT_DIFF_WORK(p != nullptr);
}

// ---- 0x180032750 — LinkedList node allocate + initialise -------------------
//
//     MOV RSI,RDX                  ; RDX = &value.  RCX IS NEVER READ.
//     ... Enter ; node = Allocate(_singleton, 0x18, 8) ; Leave ...
//     TEST RDI,RDI / JZ -> return 0
//     XOR ECX,ECX
//     MOV [RDI],RCX                ; node->element = 0   <- DEAD, and kept
//     MOV RAX,[RSI] / MOV [RDI],RAX; node->element = *value
//     MOV [RDI+0x8],RCX            ; node->prev = 0
//     MOV [RDI+0x10],RCX           ; node->next = 0
//     MOV RAX,RDI
//
// THE FIRST ARGUMENT IS NEVER READ. That is settled by reading the callee, not
// by inferring it from the one call site
// (reference_read_the_callee_before_holding_back_on_an_arg.md): the whole body
// touches RCX only as the destination of `XOR ECX,ECX`, which destroys it
// before any use. It matters because BringDialogToFront calls this WITHOUT
// setting RCX at all — RCX arrives holding whatever the preceding
// `CALL 0x18007a010` left there. A port that read a first parameter here would
// be reading garbage and would look correct for as long as that garbage
// happened to be harmless.
//
// The dead `node->element = 0` before the real store is reproduced. Retail
// emits both; dropping the first is the kind of tidy-up that introduces a
// difference nobody then looks for.
extern "C" void* RtColl_LinkedListNewNode_180032750(void* /*neverRead*/,
                                                    void* const* value) {
    RT_DIFF_BEGIN("Collection::LinkedList::NewNode(0x180032750)");

    LlNode* node = static_cast<LlNode*>(MemAllocLocked(0x18, 8));
    if (!node) { RT_DIFF_WORK(false); return nullptr; }

    node->element = nullptr;     // retail's dead store, kept — see above
    node->element = *value;
    node->prev    = nullptr;
    node->next    = nullptr;

    RT_DIFF_WORK(true);
    return node;
}

// ---- 0x18007a010 — LinkedList::Remove(list, &value) ------------------------
//
// Find the node whose ELEMENT equals *value, unlink it, zero its links,
// decrement the count, and tail-jump into 0x180009c30 to free it.
//
// Three details that a tidier rewrite would get wrong:
//
//  * THE HEAD TEST COMPARES AGAINST THE ORIGINAL HEAD, held in R8 from before
//    the search (`MOV R8,[RCX]` at 0x18007a010, `CMP RDX,R8` at 0x18007a031),
//    not against a re-read of `list->head`. Nothing in this body can change the
//    head in between, so the two agree — but writing `if (n == list->head)`
//    would make that an assumption instead of a transcription.
//  * THE SEARCH IS BY VALUE, not by node. `*value` is loaded ONCE, before the
//    walk (0x18007a01e), and compared against each node's element.
//  * A MISS IS SILENT. If the head is null, or the walk runs off the end, the
//    body simply returns: no free, no count change. BringDialogToFront relies
//    on that — it calls Remove unconditionally on a dialog that may not be in
//    the list yet.
//
// The unlink itself matches, branch for branch, the one Collection.h already
// pins from UI::UserInterface::CleanRemovedDialogs (0x18011f790). That is
// independent corroboration of the field offsets, not the evidence for them.
extern "C" void RtColl_LinkedListRemove_18007a010(void* listPtr,
                                                  void* const* value) {
    RT_DIFF_BEGIN("Collection::LinkedList::Remove(0x18007a010)");

    LlList* list = static_cast<LlList*>(listPtr);
    LlNode* head = list->head;
    if (!head) { RT_DIFF_WORK(false); return; }

    void*   want = *value;
    LlNode* n    = head;
    while (n->element != want) {
        n = n->next;
        if (!n) { RT_DIFF_WORK(false); return; }
    }

    if (n == head)       list->head = n->next;   // the ORIGINAL head — see above
    if (n == list->tail) list->tail = n->prev;
    if (LlNode* prev = n->prev) prev->next = n->next;
    if (LlNode* next = n->next) next->prev = n->prev;
    n->prev = nullptr;
    n->next = nullptr;
    --list->count;

    RT_DIFF_WORK(true);

    // Retail's tail JMP 0x180009c30, with RCX still holding the list. Called
    // directly rather than through ResolveAddr: this IS the body registered at
    // that address a few lines below, so the seam would resolve straight back
    // here at the cost of a lookup.
    RtColl_FreeLocked_180009c30(list, n);
}

// ============================================================================
// Registration — BY ADDRESS ONLY
// ============================================================================
// None of the three is in the shipped export directory, so there is no mangled
// name to register under and nothing for tools/export_name_audit.py to verify.
// The addresses are spelled as hex literals in the RegisterRuntimeAddr calls
// because tools/runtime_census.py counts the literal in the call and cannot see
// through a named constant
// (reference_census_cannot_see_nonliteral_registrations.md).
int RtCollLinkedListRegister() {
    if (!RtWaveEnabled("BANISHED_RT_LINKEDLIST")) return 0;

    int n = 0;
    RegisterRuntimeAddr(0x180009c30, (void*)&RtColl_FreeLocked_180009c30);        ++n;
    RegisterRuntimeAddr(0x180032750, (void*)&RtColl_LinkedListNewNode_180032750); ++n;
    RegisterRuntimeAddr(0x18007a010, (void*)&RtColl_LinkedListRemove_18007a010);  ++n;
    return n;
}
