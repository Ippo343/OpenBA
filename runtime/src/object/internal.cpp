// ============================================================================
// runtime/src/object/internal.cpp — Object::Internal / InternalBasePtr /
//                                   InternalFactory   (Wave 3, lane B)
// ============================================================================
//
// Byte-faithful port of the decompilation at the retail address named above
// each function (Runtime-x64.dll, image base 0x180000000, read 2026-09-06).
// Layouts, evidence and the vtable dump are in
// runtime/types/object/Internal.h — read that first.
// The full write-up, including the GO/NO-GO verdict, is
// docs/runtime_port/WAVE3_LANE_B.md.
//
// ORIGINAL FILE NAME. The profile build's source-tree strings name
// `Object\InternalFactory.cpp` and the headers `Object/InternalFactory.h` and
// `Object/InternalPtr.h`. There is no `Object\Internal.cpp`. This file's name
// is the wave scaffolding's, not the engine's.
//
// ----------------------------------------------------------------------------
// THE ONE THING THAT MAKES THIS LANE SAFE
// ----------------------------------------------------------------------------
// PLAN.md §4.3 risk 3 says a half-ported class with two vtables in play is the
// swap-remove UAF family again. That failure mode DOES NOT EXIST here, for the
// same reason it did not exist for the Wave 2 allocator, and for one extra
// reason of its own:
//
//   * There is ONE table. Every function below reaches the LIVE
//     `?_singleton@InternalFactory@Object@@0PEAV12@EA` and mutates the DLL's
//     own arrays in place. No replica, no second table, no second heap — the
//     buffers are allocated and freed through the DLL's own
//     System::MemoryAllocator statics.
//   * There is ONE vtable, and it is THEIRS. `??_7Internal@Object@@6B@`
//     (0x180154720) and `??_7Base@Object@@6B@` (0x180154788) are exported data
//     symbols, so the ported constructor stamps the DLL's vtable by name. An
//     object built by our ctor is byte-for-byte indistinguishable from one
//     built by the DLL's, including its virtual dispatch.
//
//     *** THAT SECOND BULLET IS CONDITIONALLY TRUE, AND ITS CONDITION IS NOT
//         VISIBLE FROM HERE. *** (Wave 11 lane OBJLIFE, 2026-09-09.)
//     `VtblInternal()` / `VtblBase()` below reach those two symbols through
//     `ResolveExport`, which consults the PORT REGISTRY FIRST. Both symbols are
//     already MINTED into standalone/rt_data_mint_tables.inc; they are simply
//     not promoted yet. The moment anything registers them — and that is
//     exactly what Wave 11 lane OBJDATA's Class A row set is for —
//     `ResolveExport` starts handing back OUR MINTED VTABLE, and:
//
//         objects built by OUR ctor (the doors we intercept) carry the minted
//         vptr, while objects built by the DLL's ctor (its own in-DLL rel32
//         callers, which we do NOT intercept) carry 0x180154720.
//
//     That is TWO VTABLES IN PLAY ON ONE CLASS — PLAN.md §4.3 risk 3 verbatim,
//     the hazard this very bullet exists to deny. Neither lane creates it
//     alone: it needs the TABLE tier armed AND those two ??_7 rows promoted.
//     Do not arm this tier and promote those rows in the same build without
//     first making these two accessors resolve RAW (theirs when the DLL is
//     present) and fall back to the registry only when it is absent.
//
//     *** THAT CHANGE IS NOW MADE, HERE. *** (Wave 11 follow-up 5, lane TINFO,
//     2026-09-10.) `VtblInternal()` / `VtblBase()` below pin to the DLL's own
//     table while the DLL is present and fall back to the registry only when it
//     is not — so the second bullet above is UNCONDITIONALLY true again, and it
//     stays true whatever the mint registers. The long note beside those two
//     functions has the measurement that showed the hazard was live (123,149
//     objects carrying the minted table in one armed run) and the kill switch
//     that reproduces it.
//
//     ⚠ SCOPE. The advisor amendment names these two accessors, and they are
//     the two this class uses. They are NOT the only stampers of
//     `??_7Base@Object@@6B@` in the tree — TWELVE other files reach that symbol
//     through a registry-first door (`ResolveExport` / `RT_DATA_SLOT` /
//     `RtVtable`) and would split the same way if that row were ever promoted.
//     See docs/runtime_port/WAVE11F_LANE_TINFO.md for the list.
//
// What is left is therefore an ordinary faithfulness question — does our body
// compute the same indices and the same array growth as theirs — and that is
// exactly what the shadow mode below measures.
//
// ----------------------------------------------------------------------------
// THREE ARMING TIERS
// ----------------------------------------------------------------------------
//   the old DLL-era switch            nothing in this file registers.
//
//   (default)                         TIER 1 only — the functions with no
//                                     shared state: both InternalBasePtr
//                                     constructors, Copy, DeletePtr,
//                                     Internal::Persist, and — since Wave 11
//                                     lane OBJINT — InternalFactory::
//                                     ClearPersistState, which touches
//                                     _persistIds and never the handle table.
//
//   the old DLL-era switch     adds observe-only wrappers around
//                                     Internal::Internal, ~Internal,
//                                     ClearPersistState and ResetPersistState.
//                                     OUR CODE NEVER WRITES THE TABLE in this
//                                     mode: it predicts the post-state, calls
//                                     the DLL to do the real work, and asserts
//                                     the prediction held. This is the oracle
//                                     (PLAN.md §4.3 risk 2's the diff oracle
//                                     contract, verbatim). Requires
//                                     the old DLL-era switch to report.
//
//   the old DLL-era switch      TIER 2 IS DEFAULT-ON as of Wave 11
//                                     follow-up lane ARM (2026-09-09); this is
//                                     its KILL SWITCH. The real ports of the
//                                     table mutators drive the live handle
//                                     table on every ordinary run. (
//                                     InternalBasePtr::Serialize LEFT this tier
//                                     in Wave 9: lane S verified it, and
//                                     gated-because-unverified is not the same
//                                     as gated-because-dangerous.)
//                                     TABLE and SHADOW are mutually exclusive;
//                                     asking for both is refused.
//
//   the old DLL-era switch              ALSO ARMS TIER 2, and is why the old
//                                     "*** VERDICT: NO-GO ***" line is gone
//                                     from here (Wave 9 lane Y, 2026-09-08).
//                                     The NO-GO was about TWO writers driving
//                                     one live table; with the DLL absent there
//                                     is one writer and no fallback. See the
//                                     long note in RtObjectInternalRegister.
//
// CheckPtr is REGISTERED UNCONDITIONALLY as of 2026-09-08 (Wave 9 lane Y). It
// used to say "DELIBERATELY NOT REGISTERED in any tier"; the reason for that has
// been fixed at its source rather than worked around — see the long comment on
// RtObject_InternalBasePtr_CheckPtr. It is not part of any tier: it touches no
// shared state, and the cutover needs the symbol to exist.
//
// No C++ exceptions anywhere in this file (PLAN.md §4.3 risk 5).
// ============================================================================

#include <cstdio>
#include <cstring>

#include "../../types/object/Internal.h"
#include "../../funcs/rt_port.h"

namespace RtObject {
namespace {

// ============================================================
// The allocator and PRNG
// ============================================================
// The handle table's buffers used to cross the Runtime-x64.dll boundary in both
// directions, so allocation had to stay on the DLL's heap and the door here was
// the raw one (GetProcAddress). With the DLL gone there is exactly ONE heap --
// our own MemoryAllocator port -- and the registry door is the only door.
//
// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  0x1800c7a50
// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     0x1800c7ad0
// Retail's Array bodies inline `EnterCS(_singleton+0x640); Allocate(...);
// LeaveCS`, which is precisely what these two statics are.
using MemAllocFn = void* (*)(uint64_t, int);
using MemFreeFn  = void  (*)(void*);
using RandFn     = uint32_t (*)(void*);

MemAllocFn g_memAlloc = nullptr;
MemFreeFn  g_memFree  = nullptr;
RandFn     g_rand     = nullptr;

void* MemAlloc(uint64_t bytes, int align) {
    if (!g_memAlloc)
        g_memAlloc = reinterpret_cast<MemAllocFn>(
            ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    return g_memAlloc(bytes, align);
}

void MemFree(void* p) {
    if (!g_memFree)
        g_memFree = reinterpret_cast<MemFreeFn>(
            ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    g_memFree(p);
}

// ?Rand@Random@System@@QEAAIXZ  0x1800c7f90. Wave 2 ported this and proved it
// bit-exact over 16,035,064 calls.
uint32_t RandomRand(void* random) {
    if (!g_rand)
        g_rand = reinterpret_cast<RandFn>(ResolveExport("?Rand@Random@System@@QEAAIXZ"));
    return g_rand(random);
}

// ============================================================
// The live singleton and the two exported vtables
// ============================================================
InternalFactory** g_ppFactory     = nullptr;
void*             g_vtblInternal  = nullptr;
void*             g_vtblBase      = nullptr;

InternalFactory* Factory() {
    if (!g_ppFactory)
        g_ppFactory = reinterpret_cast<InternalFactory**>(
            ResolveExport("?_singleton@InternalFactory@Object@@0PEAV12@EA"));
    return g_ppFactory ? *g_ppFactory : nullptr;
}

// An exported ??_7 symbol's ADDRESS *is* the vtable (slot 0 first), so these
// are used as values, not dereferenced. Verified against the .rdata dump in
// Internal.h: 0x180154720 holds 0x180006440, the scalar deleting destructor.
//
// ============================================================================
// CONDITION (b) — WAVE 11 follow-up 5, lane TINFO. THE SPLIT-VPTR FIX.
// ============================================================================
//
// This is the change the banner at the top of this file described and declined
// to make, and the advisor adjudication in docs/runtime_port/WAVE11.md made
// binding on whoever promotes `??_7Internal@Object@@6B@`:
//
//     may be promoted only when BOTH hold: (a) [ObjTI] ATTENDANCE entries>0,
//     and (b) VtblInternal()/VtblBase() pin to the RAW DLL symbol while the
//     DLL is present, falling back to the registry only when it is absent.
//
// WHY IT MATTERS, MEASURED RATHER THAN ARGUED. `ResolveExport` is registry-
// first, so with the minted row registered these two returned OUR table while
// the DLL's own in-DLL `rel32` callers — which we do not intercept — kept
// stamping 0x180154720. Lane TINFO ran exactly that: `??_7Internal@Object@@6B@`
// armed with the TABLE tier armed, `[RtInternalTable] ctorRuns=123149`. That is
// 123,149 live objects carrying a DIFFERENT vtable from the ones the DLL built,
// on one class, which is PLAN.md §4.3 risk 3 verbatim.
//
// WHAT PINNING COSTS, AND WHY IT IS NOTHING. Every slot of both tables is one
// of our ported bodies either way (`READY slots=4 ported=4 TRAP=0 purecall=0`);
// what changes is WHICH POINTER the object carries, and the only correct answer
// while the DLL is present is the DLL's, because that is the answer every
// object we did not build already has. At cutover there is no DLL, `RtRawVtable`
// comes back null, and the registry answer — the minted table — is the only one
// there is. So the same code is right on both sides of cutover.
//
// behaviour. It exists so the A/B that demonstrates the split is runnable with
// ONE variable, and it is the half you must NOT ship: with it set and the minted
// row registered, this class has two vtables.
void* VtblInternal() {
    if (!g_vtblInternal) {
        g_vtblInternal = ResolveExport("??_7Internal@Object@@6B@");
    }
    return g_vtblInternal;
}
void* VtblBase() {
    if (!g_vtblBase) {
        g_vtblBase = ResolveExport("??_7Base@Object@@6B@");
    }
    return g_vtblBase;
}

// Whether the pin is armed. Read by the exit screen below, which lives at
// global scope with the other read-outs.

// ============================================================
// Collection::Array<T> bodies, as retail instantiates them
// ============================================================
// WAVE2's warning is live here: node/element offsets are a function of
// sizeof(T), so MSVC emits a SEPARATE body per instantiation and one ported
// body cannot serve two element widths. Retail has four distinct ones in this
// class alone, and all four are ported separately below with their own
// addresses. The growth policy is identical in all four and matches
// runtime/types/collection/Collection.h exactly.
//
// The `if (slot != nullptr)` tests are RETAIL's, not ours. They are the
// template's per-element address check surviving into codegen; they can never
// fire for a non-null buffer. Kept because this file is byte-faithful first.

// FUN_18002ffa0 — Array<Internal*>::SetSize(int newSize, const Internal*& fill)
void ArrayPtrSetSizeFill(Collection::Array<Internal*>* a, int newSize, Internal* const* fill) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    Internal** old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<Internal**>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        Internal** slot = a->_data + i;
        if (slot != nullptr)
            *slot = (i < oldSize) ? old[i] : *fill;
    }
    if (old != a->_data && old != nullptr)
        MemFree(old);
}

// FUN_18002fca0 — Array<Internal*>::SetSize(int newSize), no fill value.
// Only the surviving prefix is copied; a grown tail is left UNINITIALISED,
// which is why retail only ever calls this one to SHRINK.
void ArrayPtrSetSize(Collection::Array<Internal*>* a, int newSize) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    Internal** old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<Internal**>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        if (i < oldSize) {
            Internal** slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
    }
    if (old != a->_data && old != nullptr)
        MemFree(old);
}

// ---- 0x18002fca0 — THE ADDRESS DOOR FOR THE BODY ABOVE ---------------------
// WAVE 9 LANE UID. `ArrayPtrSetSize` has BEEN the port of FUN_18002fca0 since
// Wave 3 — the comment above it says so — but nothing ever registered it at
// that address, so `ResolveAddr(Module::Runtime, 0x18002fca0)` fell through to
// the DLL and would be a null call at cutover
// (reference_by_address_door_dies_at_cutover.md). Two lane docs recorded
// 0x18002fca0 as an UNPORTED blocker of `??1DialogInstance@UI@@`
// (WAVE9_LANE_UIF.md row 6, WAVE9_LANE_UIC.md §7); the body was already here.
// That is reference_grep_for_the_port_before_writing_it.md read from the other
// side: the miss was not a duplicate port, it was a HALF-REGISTERED one.
//
// Registered UNGATED, deliberately, and it is NOT part of the `table` tier:
// this body is a container primitive that operates on whatever array its
// caller hands it. It mutates the handle table only when the CALLER passes
// `_singleton->_objects`, which only `~Internal` does — and that one stays
// behind `table`. Registering the primitive therefore arms no handle-table
// mutation of its own.
//
// It is unexported (checked against the shipped export directory with pefile),
// so ADDRESS ONLY, and the void* signature is the shape every address caller
// reaches it through. Element width is 8 and the type is erased in the callee,
// so `Internal*` here is a name, not a constraint.
extern "C" void RtObject_ArrayPtrSetSize_18002fca0(void* arrayPtr, int newSize) {
    ArrayPtrSetSize(static_cast<Collection::Array<Internal*>*>(arrayPtr), newSize);
}

// FUN_18002fd90 — Array<Internal*>::Reserve(int n). Grows CAPACITY only: it
// sets _size to n while it reallocates and restores the old _size on the way
// out, and it returns immediately unless n is strictly greater than _size.
void ArrayPtrReserve(Collection::Array<Internal*>* a, int n) {
    const int oldSize = a->_size;
    if (oldSize >= n)
        return;
    Internal** old = a->_data;
    a->_size = n;
    if (a->_capacity < n) {
        a->_capacity = n;
        if (n == oldSize + 1 && oldSize != 0)
            a->_capacity = n * 3;
        a->_data = static_cast<Internal**>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    if (old != a->_data) {
        for (int i = 0; i < oldSize; ++i) {
            Internal** slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
        if (old != nullptr)
            MemFree(old);
    }
    a->_size = oldSize;
}

// FUN_18002fea0 — Array<int32_t>::SetSize(int newSize). Element width 4,
// alignment 4 (Collection.h: the alignment argument is alignof(T), not
// sizeof(T)); otherwise identical to ArrayPtrSetSize.
void ArrayIntSetSize(Collection::Array<int32_t>* a, int newSize) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    int32_t* old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<int32_t*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 2), 4));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        if (i < oldSize) {
            int32_t* slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
    }
    if (old != a->_data && old != nullptr)
        MemFree(old);
}

// The capacity ArrayPtrSetSizeFill/SetSize will end up with, given a starting
// (size, capacity) and a target size. Used by the shadow oracle so that the
// growth policy — the only non-trivial arithmetic in the whole class — is
// actually checked and not just assumed.
int PredictCapacity(int oldSize, int oldCapacity, int newSize) {
    if (newSize == oldSize)
        return oldCapacity;
    if (oldCapacity >= newSize)
        return oldCapacity;
    if (newSize == oldSize + 1 && oldSize != 0)
        return newSize * 3;
    return newSize;
}

// ---- ENDGAME B.3.1 step 1: the bridge to the real class ---------------------
//
// `Object::InternalBasePtr` (runtime/types/object/Internal.h) now owns every
// body in this family. The `RtObject_InternalBasePtr_*` functions below stay —
// they are the registered symbols and their callers have not moved yet — and
// each is now a ONE-LINE shim onto the class, running over the same eight
// bytes. The cast is the transitional kind B31_RULES sanctions for a
// not-yet-converted POD; it disappears with the POD in step 2.
inline ::Object::InternalBasePtr* AsClass(InternalBasePtr* p) {
    return reinterpret_cast< ::Object::InternalBasePtr*>(p);
}
inline const ::Object::InternalBasePtr* AsClass(const InternalBasePtr* p) {
    return reinterpret_cast<const ::Object::InternalBasePtr*>(p);
}

}  // namespace

// ============================================================================
// TIER 1 — no shared state
// ============================================================================

// ??0InternalBasePtr@Object@@QEAA@XZ   retail 0x1800062c0, profile 0x180023900
// One instruction: `mov qword ptr [rcx], 0`.
// B.3.1: the body is `Object::InternalBasePtr::InternalBasePtr()`; this is the
// registered shim.
void* RtObject_InternalBasePtr_Ctor(InternalBasePtr* self) {
    new (AsClass(self)) ::Object::InternalBasePtr();
    return self;   // MSVC constructors return `this` in RAX.
}

// ??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z
//                                      retail 0x1800062a0, profile 0x1800238e0
// One instruction: `mov qword ptr [rcx], rdx`.
void* RtObject_InternalBasePtr_CtorPtr(InternalBasePtr* self, Internal* p) {
    new (AsClass(self)) ::Object::InternalBasePtr(p);
    return self;
}

// ?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z
//                                      retail 0x1800062b0, profile 0x1800257d0
// One instruction pair: `mov rax,[rdx]; mov [rcx],rax`. NOT refcounted — this
// really is a raw 8-byte copy, and the two InternalBasePtrs are then aliases.
void RtObject_InternalBasePtr_Copy(InternalBasePtr* self, const InternalBasePtr* other) {
    // `Copy` is PROTECTED in retail (`IEAA`), so the shim goes through the
    // class's one-line static forwarder rather than widening its access.
    ::Object::InternalBasePtr::Shim_Copy(AsClass(self), AsClass(other));
}

// ?CheckPtr@InternalBasePtr@Object@@IEBAXXZ
//                                      retail 0x1800c0290, profile 0x18008c5d0
//
// *** A BARE `RET 0` IN BOTH BUILDS. *** It validates nothing. The profile
// build — asserts compiled in, 5,182 exports — has the identical empty body, so
// this was compiled out at the SOURCE level in every shipped configuration, not
// optimised out of retail. `?AssignId@InternalBasePtr@Object@@IEAAXXZ`
// (0x1800c0280) is the same three bytes.
//
// REGISTERED BY DEFAULT since 2026-09-08 (Wave 9 lane Y). This block used to
// say "NOT REGISTERED BY DEFAULT, and the reason is not squeamishness —
// registering it would be an active regression", and the argument was:
//
//   engine/funcs/engine_funcs.cpp inspects the FIRST BYTE of whatever
//   `?CheckPtr@…@IEBAXXZ` resolves to, and sets `g_checkPtrIsNop` only when it
//   is `C3` or `C2 00 00`. That flag makes ~60,000 calls per frame at 800x10
//   disappear. Register this port and the resolved body becomes a GCC-compiled
//   function whose first byte is neither — so the flag goes false and the calls
//   come back. We would have replaced a no-op with a no-op and paid for it.
//
// That was TRUE, and it was a statement about the DETECTOR, not about this
// function. It is now fixed where it was wrong: the detector asks
// `RuntimeExportIsRegistered("?CheckPtr@InternalBasePtr@Object@@IEBAXXZ")` and
// treats our own port as a no-op, which it provably is — the body below is
// empty and we wrote it. The flag stays true, the ~60k calls stay gone, and
// registering costs nothing.
//
// The perf-build half of the old argument is unchanged and is not a reason
// against registering: with BANISHED_CHECKPTR_STATIC_NOP the call sites are
// compiled out entirely, so this body never runs there. Anyone reading a census
// row for it should read this paragraph — it is a REGISTERED SYMBOL, not an
// EXERCISED one, and in the perf build it has no callers by construction
// (reference_uncalled_port_is_not_verified.md, acknowledged rather than hidden).
//
// WHY IT MUST BE REGISTERED AT ALL, given it does nothing: at cutover the symbol
// has to exist. It is on the DLL-absent boot's [RtUnresolved] list, and 2,333 of
// its 3,475 call sites in this tree have no `if (fn_...)` null guard — they go
// through the engine_funcs.h macro, which with a null resolve and a false flag
// calls a null pointer. Leaving it unregistered is an EXEC-of-0 at cutover.
//
// the old DLL-era switch is retained only as a positive assertion in
// the log; it no longer gates anything.
// B.3.1: still empty, and `Object::InternalBasePtr::CheckPtr` it forwards to is
// empty too. NEVER ADD VALIDATION TO EITHER.
void RtObject_InternalBasePtr_CheckPtr(const InternalBasePtr* self) {
    ::Object::InternalBasePtr::Shim_CheckPtr(AsClass(self));
}

// ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z
//                                      retail 0x1800bfda0, profile 0x18008b490
// Empty in both builds. It is slot 3 of ??_7Internal@Object@@6B@ and exists so
// that a subclass has something to override; InternalFactory::PersistObject
// calls it on every object it creates from a stream.
void RtObject_Internal_Persist(Internal*, void* /*IO::Stream&*/) {
}

// ?DeletePtr@InternalBasePtr@Object@@QEAAXXZ
//                                      retail 0x1800c0150, profile 0x18008c5a0
//
//     p = this->_ptr;
//     if (p) { p->vtbl[0](p, 1); this->_ptr = 0; }
//
// Slot 0 with flag 1 is the scalar deleting destructor: it runs ~Internal (and
// therefore the swap-remove) and then frees the block. The dispatch is virtual,
// so it lands in whatever vtable the object carries — which, because our ctor
// stamps the DLL's, is the same code either way.
//
// NOT DIFFABLE: running the original as an oracle would destroy the object
// twice. The evidence for this one is code inspection of five instructions, not
// a measurement, and the write-up says so.
void RtObject_InternalBasePtr_DeletePtr(InternalBasePtr* self) {
    AsClass(self)->DeletePtr();
}

// ============================================================================
// TIER 2 — the table mutators.  NO-GO by default; see the file header.
// ============================================================================

// ??0Internal@Object@@QEAA@XZ          retail 0x1800bfd10, profile 0x18008b370
//
// Retail inlines Base::Base and InternalFactory::AddObject; the profile build
// calls both out of line and proves what the inline is:
//     Base::Base(this);                    // vptr = &Base::_vftable_
//     this->vptr = &Internal::_vftable_;
//     this->_factoryIndex = -1;
//     this->_generation   = 0;
//     AddObject(_singleton, this);         // 0x1800c00c0
// and AddObject is
//     obj->_factoryIndex = _objects._size;
//     obj->_generation   = _random.Rand();
//     _objects.SetSize(_objects._size + 1, obj);
//
// The `-1` / `0` pair is a dead store in both builds — AddObject overwrites
// both before anything can observe them — and is reproduced anyway.
// WAVE 11 LANE OBJLIFE — ATTENDANCE COUNTERS.
//
// A run of the TABLE tier that boots and plays is NOT evidence that these
// bodies ran: a green readiness screen is not a route, and only an attendance
// counter separates "safe" from "never executed"
// (reference_a_green_readiness_screen_is_not_a_route). The shadow tier gets its
// `calls=` from RT_DIFF; the table tier had NOTHING, so an armed run could only
// ever be reported as "did not crash".
//
// Two plain counters, incremented unconditionally in the ported bodies. They
// are not diff sites and cost one increment on a path that already does a
// locked array grow. `RtObjectInternalAttendance()` reads them out.
// All FOUR tier-2 rows, not just this lane's two. The other pair
// (ClearPersistState / ResetPersistState) is lane OBJINT's subject, and this
// file's own banner has said since Wave 9 that they have NEVER EXECUTED — a
// claim nothing in the tree could actually check. Now it can, and OBJINT's
// verdict can cite a counter instead of an absence of evidence.
unsigned long long g_tableCtorRuns  = 0;
unsigned long long g_tableDtorRuns  = 0;
unsigned long long g_tableClearRuns = 0;
unsigned long long g_tableResetRuns = 0;

Internal* RtObject_Internal_Ctor(Internal* self) {
    ++g_tableCtorRuns;
    self->vptr = VtblBase();
    self->vptr = VtblInternal();
    self->factoryIndex = -1;
    self->generation   = 0;

    InternalFactory* f = Factory();
    self->factoryIndex = f->objects._size;
    Internal* elem = self;
    self->generation = RandomRand(&f->random);
    ArrayPtrSetSizeFill(&f->objects, f->objects._size + 1, &elem);
    return self;
}

// ??1Internal@Object@@UEAA@XZ          retail 0x1800bfdb0, profile 0x18008b400
//
//     this->vptr = &Internal::_vftable_;
//     this->_generation = 0;
//     RemoveObject(_singleton, this);          // 0x1800c0110, inlined
//     Base::~Base(this);                       // vptr = &Base::_vftable_
//
// RemoveObject is the swap-remove that
// reference_internalfactory_swapremove_wild_write.md is about:
//     last = _objects._size - 1;  idx = obj->_factoryIndex;
//     if (last != idx) { _objects[idx] = _objects[last];
//                        _objects[idx]->_factoryIndex = idx; }
//     _objects.SetSize(last);
// There is NO validation of `idx` — not here and not in the profile build. If
// `idx` is stale the second store lands wherever the allocator has since put
// that block. That is retail's behaviour and this port keeps it; the fix for
// the family is upstream (unregister before freeing), never a guard here.
void RtObject_Internal_Dtor(Internal* self) {
    ++g_tableDtorRuns;
    self->vptr = VtblInternal();
    self->generation = 0;

    InternalFactory* f = Factory();
    const int last = f->objects._size - 1;
    const int idx  = self->factoryIndex;
    if (last != idx) {
        f->objects._data[idx] = f->objects._data[last];
        f->objects._data[idx]->factoryIndex = idx;
    }
    ArrayPtrSetSize(&f->objects, last);

    self->vptr = VtblBase();
}

// ?ClearPersistState@InternalFactory@Object@@QEAAXXZ
//                                      retail 0x1800bfe30, profile 0x18008c0b0
// Frees the persist-id array and zeroes all three of its fields. Retail inlines
// the free (FUN_18001c750); the profile build calls MemFree — and it calls it
// UNCONDITIONALLY, where retail's inline null-checks first. Same behaviour:
// MemoryAllocator::Free opens with `test rdx,rdx; jz`.
void RtObject_InternalFactory_ClearPersistState(InternalFactory* self) {
    ++g_tableClearRuns;
    // LIVENESS. This body is DEFAULT-ON as of Wave 11 lane OBJINT, and a
    // verified body that nothing calls is dead code that reads exactly like a
    // landed port (reference_uncalled_port_is_not_verified.md). Once per
    // process, on a function the save/load path enters once per transaction.
    static bool first = true;
    if (first) { first = false;
        printf("[RtPort] FIRST LIVE CALL into our ?ClearPersistState@InternalFactory@Object@@ "
               "(default-on since Wave 11 lane OBJINT)\n"); }

    if (self->persistIds._data != nullptr)
        MemFree(self->persistIds._data);
    self->persistIds._data     = nullptr;
    self->persistIds._size     = 0;
    self->persistIds._capacity = 0;
}

// ?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z
//                                      retail 0x1800bfe40, profile 0x18008bfc0
//
//     Serializer ser(stream, 0);              // inlined; see below
//     int count = _objects._size;
//     ser.readerWriter->vtbl[0xa0](encoding, &count, 0);   // WRITE on save, READ on load
//     _persistIds.SetSize(count);
//     _objects.Reserve(count);
//     for (i = 0; i < _persistIds._size; ++i) _persistIds[i] = -1;
//     ~Serializer(ser);                       // encoding->vtbl[0](encoding, 1)
//
// The Serializer construction is reproduced from ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z
// (0x1800b9960), which retail inlines here verbatim: stream+0x08 selects the
// direction (0 == reading) and stream+0x10 is the encoding factory whose vtable
// slot 3 (+0x18) makes a reader and slot 4 (+0x20) makes a writer.
//
// ARITY — RESOLVED 2026-09-08, WAVE 9 LANE Y. This block used to say the arity
// was unknown and "should be disassembled if this function is ever taken live";
// that was WAVE3_LANE_B.md's gate step 2, and it is now closed from the bytes.
// Retail, 0x1800bfe68-0x1800bfe84:
//
//     MOV RCX,[RDX+0x10]     ; arg1 = stream->encoding  (the factory)
//     XOR R8D,R8D            ; arg3 = 0                 <-- SET BEFORE the test
//     MOV RAX,[RCX]          ; factory vtable
//     CMP [RDX+0x8],EBX      ; direction == 0 ?
//     LEA RDX,[RSP+0x28]     ; arg2 = &serializer
//     JNZ +5
//     CALL [RAX+0x18]        ; reading -> CreateReader
//     CALL [RAX+0x20]        ; writing -> CreateWriter
//
// THREE arguments, identical on both branches, and the third is a literal 0.
// The port passed only two, leaving R8 holding whatever the caller left there.
//
// It is not an academic point, and the tree had already proved it: Wave 4's
// IO::Serializer port disassembled the SAME two slots from the out-of-line
// ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z (0x1800b9960) and wrote in
// runtime/src/io/serializer.cpp: "The `int version` argument is NOT ignored …
// R8D is live all the way to the CALL, and the thing being called is
// ??0Binary{Reader,Writer}@IO@@QEAA@AEAVSerializer@1@H@Z — (Serializer&, int).
// BinaryWriter STORES it and writes it to the stream. That pins the arity of
// both factory slots at 3." So the callee reads R8, and a two-argument call
// would have handed BinaryWriter a garbage stream version.
//
// The third argument is the serializer VERSION, and retail's inlined copy here
// passes 0 — this call site does not version its record. Written as a named
// constant so the 0 cannot be mistaken for a placeholder.
void RtObject_InternalFactory_ResetPersistState(InternalFactory* self, void* stream) {
    ++g_tableResetRuns;
    SerializerLite ser;
    ser.stream   = stream;
    ser.readerWriter = nullptr;

    char*  s       = static_cast<char*>(stream);
    void*  factory = *reinterpret_cast<void**>(s + 0x10);
    void** fvtbl   = *reinterpret_cast<void***>(factory);
    const int mode = *reinterpret_cast<int*>(s + 0x08);
    const int kSerializerVersion = 0;   // retail's XOR R8D,R8D at 0x1800bfe6c
    ser.readerWriter = reinterpret_cast<void* (*)(void*, void*, int)>(
        fvtbl[(mode == 0 ? 0x18 : 0x20) / 8])(factory, &ser, kSerializerVersion);

    void*  enc     = ser.readerWriter;
    void** encVtbl = *reinterpret_cast<void***>(enc);

    int count = self->objects._size;
    reinterpret_cast<void (*)(void*, int*, const char*)>(encVtbl[kEncSerializeInt / 8])(
        enc, &count, nullptr);

    ArrayIntSetSize(&self->persistIds, count);
    ArrayPtrReserve(&self->objects, count);

    // The bound is re-read from _persistIds._size on every iteration, as retail
    // does; it cannot change, but this file is byte-faithful first.
    for (int i = 0; i < self->persistIds._size; ++i)
        self->persistIds._data[i] = -1;

    if (enc != nullptr)
        reinterpret_cast<void (*)(void*, int)>(encVtbl[0])(enc, 1);
}

// ?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z
//                                      retail 0x1800c0180, profile 0x18008c460
//
//     bool present = (_ptr != 0);
//     enc->Serialize(present);                     // enc vtbl +0x78
//     if (!present) { _ptr = 0; return; }
//     index = _ptr ? _ptr->_factoryIndex : 0;
//     type  = _ptr ? _ptr->Type() : 0xffffffff;    // Internal vtbl slot 2
//     gen   = _ptr ? _ptr->_generation : 0;
//     enc->Serialize(index);                       // +0xa0, int
//     enc->Serialize(gen);                         // +0x98, uint
//     enc->Serialize(type);                        // +0x98, uint
//     _ptr = InternalFactory::PersistObject(_singleton, ser.stream, gen, index, type);
//
// The redundant `_ptr ? … : …` guards are retail's: `present` already proved
// the pointer non-null, and the compiler kept the tests anyway.
//
// *** THE ENCODING VTABLE OFFSETS ARE BUILD-SPECIFIC. *** These are the RETAIL
// numbers (0x78 / 0x98 / 0xa0, agreeing with the forwarder table in
// runtime/src/io/serializer.cpp). The PROFILE build uses 0x90 / 0xb0 / 0xb8 for
// the same three calls. Transcribing the profile numbers writes the right
// values through the wrong slots and desyncs every save.
//
// NOT DIFFABLE and NOT SHADOWED by the byte oracle: the call advances a stream,
// so a second run would consume or emit the record twice.
//
// The last two sentences of this block used to read "It is registered only
// under the old DLL-era switch and is UNVERIFIED." BOTH halves are stale
// and are corrected here rather than left to mislead (PLAN §6 step 8): Wave 9
// lane S UNGATED the registration (see the registration function below) and
// VERIFIED it with the emission-sequence oracle — 4 emissions on the non-null
// branch and 1 on the null branch, identical to the shipped body.
// B.3.1: the body moved to `Object::InternalBasePtr::Serialize`, whose
// out-of-line half (`Object::InternalBasePtrSerializeBody`) sits at the bottom
// of this file, next to `Factory()` and the `PersistObject` name door it needs.
// This is the registered shim.
void RtObject_InternalBasePtr_Serialize(InternalBasePtr* self, SerializerLite* ser) {
    AsClass(self)->Serialize(*ser);
}

}  // namespace RtObject

// ============================================================================
// ENDGAME B.3.1 step 1 — the real class's out-of-line body.
//
// `?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z`
//   retail 0x1800c0180, profile 0x18008c460.
//
// Moved here from `RtObject_InternalBasePtr_Serialize` unchanged except for the
// member spelling: it needs this file's `Factory()` and the `PersistObject`
// name door, and `IO::Serializer`, `Object::Internal`'s vtable and
// `InternalFactory` are all still unconverted, so it reaches them through
// exactly the doors the C body used (SMARTPTR_INVENTORY.md §4). No new casts.
//
//     bool present = (_ptr != 0);
//     rw->Serialize(present);                      // rw vtbl +0x78
//     if (!present) { _ptr = 0; return; }
//     index = _ptr ? _ptr->_factoryIndex : 0;
//     type  = _ptr ? _ptr->Type() : 0xffffffff;    // Internal vtbl slot 2
//     gen   = _ptr ? _ptr->_generation : 0;
//     rw->Serialize(index);                        // +0xa0, int
//     rw->Serialize(gen);                          // +0x98, uint
//     rw->Serialize(type);                         // +0x98, uint
//     _ptr = InternalFactory::PersistObject(_singleton, ser.stream, gen, index, type);
//
// The redundant `_ptr ? … : …` guards are retail's: `present` already proved
// the pointer non-null and the compiler kept the tests anyway.
//
// *** THE READERWRITER VTABLE OFFSETS ARE BUILD-SPECIFIC. *** These are the
// RETAIL numbers (0x78 / 0x98 / 0xa0, agreeing with the forwarder table in
// runtime/src/io/serializer.cpp). The PROFILE build uses 0x90 / 0xb0 / 0xb8 for
// the same three calls; transcribing those writes the right values through the
// wrong slots and desyncs every save.
//
// NOT DIFFABLE and NOT SHADOWED by the byte oracle: the call advances a stream,
// so a second run would consume or emit the record twice. Wave 9 lane S
// VERIFIED it with the emission-sequence oracle — 4 emissions on the non-null
// branch and 1 on the null branch, identical to the shipped body.
// ============================================================================
namespace Object {

void InternalBasePtrSerializeBody(RtObject::Internal** self,
                                  RtObject::SerializerLite& ser) {
    void*  enc     = ser.readerWriter;
    void** encVtbl = *reinterpret_cast<void***>(enc);

    char present = (*self != nullptr) ? 1 : 0;
    reinterpret_cast<void (*)(void*, char*, const char*)>(
        encVtbl[RtObject::kEncSerializeBool / 8])(enc, &present, nullptr);
    if (present == 0) {
        *self = nullptr;
        return;
    }

    int      index;
    uint32_t type;
    RtObject::Internal* p = *self;
    if (p == nullptr) {
        index = 0;
        type  = 0xffffffffu;
    } else {
        index = p->factoryIndex;
        void** pv = *reinterpret_cast<void***>(p);
        type = reinterpret_cast<uint32_t (*)(RtObject::Internal*)>(
            pv[RtObject::kInternalVtblType / 8])(p);
    }
    const uint32_t gen = (*self != nullptr) ? (*self)->generation : 0u;

    reinterpret_cast<void (*)(void*, int*, const char*)>(
        encVtbl[RtObject::kEncSerializeInt / 8])(enc, &index, nullptr);
    reinterpret_cast<void (*)(void*, uint32_t*, const char*)>(
        encVtbl[RtObject::kEncSerializeUint / 8])(enc, const_cast<uint32_t*>(&gen), nullptr);
    reinterpret_cast<void (*)(void*, uint32_t*, const char*)>(
        encVtbl[RtObject::kEncSerializeUint / 8])(enc, &type, nullptr);

    // The REGISTRY door, not the raw DLL door: PersistObject is ported
    // (internalfactory_persist.cpp) and with the DLL absent the raw door is
    // NULL. First DLL-absent Load Game (2026-09-12) died here, EXEC-of-0, with
    // the InternalFactory in RCX -- the same wall as video/buffers.cpp MemFree.
    using PersistObjectFn = RtObject::Internal* (*)(RtObject::InternalFactory*, void*,
                                                    uint32_t, int, uint32_t);
    PersistObjectFn persist = RT_EXPORT_ONCE(PersistObjectFn,
        "?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z");
    *self = persist(RtObject::Factory(), ser.stream, gen, index, type);
}

}  // namespace Object

// ============================================================================
// Registration
// ============================================================================
// WAVE 11 LANE OBJLIFE — the attendance read-out, called from standalone/main.cpp
// at exit. Prints unconditionally, including 0/0, and says which of the two
// zeroes it is: "the tier was never armed" and "the tier was armed and never
// entered" are opposite findings and a silent log cannot tell them apart.
// ============================================================================
// WAVE 11 FOLLOW-UP 5, lane TINFO — THE SCREEN THAT MAKES CONDITION (b)
// CHECKABLE FROM A LOG
// ============================================================================
// Printed once at exit beside the attendance counters. Per symbol: the pointer
// our constructor actually stamps, the DLL's own, the registry's — so "the
// split is closed" is a line in a log and not a claim about a code path.
//
// It is a GATE and not a tally, by this project's own test: the quantity VARIES
// with the thing it gates. With `??_7Internal@Object@@6B@` registered,
// `the old DLL-era switch` makes both rows read `MINTED (SPLIT)` and
// the default makes them read `DLL`.
// Both symbols are spelled as LITERALS at every resolve call below rather than
// travelling through a loop variable: tools/port_bypass_audit.py cannot classify
// a name that arrives in a variable and files it as UNRESOLVED-NAME, which is
// residue a later lane has to re-derive. Two symbols, so the duplication is
// three lines.
static void RtInternalVtblPinRow(const char* sym, void* stamped, void* port) {
    printf("[RtInternalVtbl] %-26s stamped=%p registry=%p  -> %s\n",
           sym, stamped, port,
           stamped == port ? "the registry's table is the only one"
                           : "NEITHER -- unexpected, investigate");
}

void RtObjectInternalVtblPin() {
    using namespace RtObject;
    // `stamped` comes through the same accessors the CONSTRUCTOR uses, so the
    // report cannot drift from the behaviour it describes.
    RtInternalVtblPinRow("??_7Internal@Object@@6B@", VtblInternal(),
                         ResolveExport("??_7Internal@Object@@6B@"));
    RtInternalVtblPinRow("??_7Base@Object@@6B@", VtblBase(),
                         ResolveExport("??_7Base@Object@@6B@"));
}

void RtObjectInternalAttendance() {
    using namespace RtObject;
    const bool armed = RuntimeExportIsRegistered("??0Internal@Object@@QEAA@XZ");
    printf("[RtInternalTable] ctorRuns=%llu dtorRuns=%llu clearRuns=%llu "
           "resetRuns=%llu  (%s)\n",
           g_tableCtorRuns, g_tableDtorRuns, g_tableClearRuns, g_tableResetRuns,
           g_tableCtorRuns == 0
               ? (armed ? "REGISTERED BUT NEVER ENTERED -- the door exists and "
                          "nothing came through it; do NOT read this run as "
                          "evidence the port works"
                        : "the ctor is NOT registered on this run -- which is a "
                          "REGRESSION, not the default")
               : "our ported bodies drove the LIVE handle table this many times");
    // The persist-state pair is reported SEPARATELY from the ctor/dtor pair on
    // purpose: they live only on the save/load path, so zero here with a large
    // ctorRuns means "this run never saved or loaded", which is a statement
    // about the RUN, not about those two ports. Reading one pair's zero through
    // the other pair's non-zero is how an unexercised body gets written up as
    // exercised.
    if (armed && g_tableClearRuns == 0 && g_tableResetRuns == 0)
        printf("[RtInternalTable]   persist-state pair NEVER ENTERED on this "
               "run -- expected unless the run saved or loaded (they are only "
               "on that path). NOT evidence about those two ports either way; "
               "use BANISHED_AUTOLOAD to reach them.\n");
}

int RtObjectInternalRegister() {
    using namespace RtObject;
    int n = 0;


    // WAVE 9 LANE Y, 2026-09-08 — the tier-2 arming condition.
    //
    // `??0Internal@Object@@QEAA@XZ` and `??1Internal@Object@@UEAA@XZ` are on the
    // DLL-absent boot's [RtUnresolved] list. They are not optional at cutover:
    // Internal is the base of every object in the engine, and an unresolved ctor
    // is a null call on the first one constructed. So the tier arms ITSELF when
    // there is no DLL, and stays opt-in when there is.
    //
    // This is not the NO-GO being waived, it is the NO-GO's own premise expiring.
    // WAVE3_LANE_B.md held the tier back because "our code now drives the LIVE
    // handle table" while the DLL's own Internal ctors drive it too — two writers,
    // one AVL-adjacent array, and a wrong `_factoryIndex` is the swap-remove wild
    // write with no proximate symptom. With Runtime-x64.dll absent THERE IS NO
    // SECOND WRITER, and there is also no DLL body left to fall through to. The
    // hazard and the alternative disappear together.
    //
    // The tier's own gate (WAVE3_LANE_B.md §5) is otherwise satisfied and was
    // already satisfied when it was written: step 1 asked for a green SHADOW run
    // with `calls=` in the thousands, and WAVE3.md records `Internal::Internal` /
    // `~Internal` at 93,387 calls each, 0 divergences, with the ctor and dtor
    // counts identical. Step 2 asked for the encoding-factory arity, which this
    // lane closed from the bytes — see ResetPersistState above. Step 3 is the
    // wave-boundary run, which is a cell for the integrator, not a code change.
    //
    // WHAT IS STILL OWED — REWRITTEN 2026-09-09 (Wave 11 lane OBJINT), because
    // what stood here was the eight-wave-old "the two persist-state shadow sites
    // have NEVER EXECUTED (WAVE3.md, defect E)". THEY HAVE NOW. A
    // `BANISHED_AUTOLOAD` run reached both, `calls=1 divergences=0` each, and
    // defect E is closed. `ClearPersistState` left the tier entirely as a
    // result (see its own block below); `ResetPersistState` did not, and the
    // block on it below says exactly which one line keeps it here.
    //
    // What is owed now is a LIVE TIER-2 LOAD RUN — this tier armed, a save
    // loaded, our `ResetPersistState` driving the real handle table with the
    // DLL present. Lane OBJINT did not have that run and does not claim it.
    // WAVE 11 FOLLOW-UP #1, 2026-09-09 (lane ARM) — THE TIER IS NOW DEFAULT-ON.
    //
    // Until today the gate was `RtOptIn("the old DLL-era switch") || noDll`
    // — opt-IN, which by this project's rule means these rows were NOT ports:
    // a body nothing registers on a default run is indistinguishable from an
    // absent one (reference_half_registered_port_reads_as_absent.md).
    //
    // What changed is not the evidence — it is the SEQUENCING constraint the
    // Wave 11 advisor named. The evidence was already sufficient
    // (WAVE11_LANE_OBJLIFE.md §4.1, WAVE11.md "ADVISOR VERDICT"): a PREDICTIVE
    // oracle (ours predicts, theirs runs) at calls=126764/126758 divergences=0,
    // an attendance counter that discriminates three ways (unregistered /
    // registered-but-never-entered / entered=122030), and an undesigned
    // cross-check where ctor and dtor differ by exactly 6 in two independent
    // runs with different totals. The advisor's only objection was that arming
    // inside a three-lane merge would confound that wave's integration
    // measurement. This lane IS the isolated change it asked for, with its own
    // before/after against the perf baseline — see WAVE11F_LANE_ARM.md.
    //
    // The switch is now a KILL switch: the old DLL-era switch (or
    // "false") leaves the tier unregistered and every call goes to the DLL
    // exactly as before. Verified by running it, not by reading this comment.
    //
    // THE SHADOW TIER STILL WORKS. The two tiers claim the same doors and are
    // mutually exclusive, so a naive default-on would have made
    // the old DLL-era switch refuse everything and silently delete the
    // oracle that produced the evidence above. So: an explicit arm still
    // CONFLICTS with shadow and is refused loudly; the DEFAULT arm simply
    // yields to it.
    //
    // `noDll` still forces the tier on regardless of the kill switch: with
    // Runtime-x64.dll absent there is no DLL body to fall through to, and an
    // unregistered `??0Internal` is a null call on the first object built.
    // ---- Tier 1: no shared state, on by default -----------------------------
    // Both a name and a Ghidra address for every one: half our call sites
    // resolve by name and half by ResolveAddr(Module::Runtime, …) with the
    // other as a fallback (see the same note in runtime/src/system/string.cpp).
    RegisterRuntimeExport("??0InternalBasePtr@Object@@QEAA@XZ",
                          (void*)&RtObject_InternalBasePtr_Ctor); ++n;
    RegisterRuntimeAddr(0x1800062c0, (void*)&RtObject_InternalBasePtr_Ctor);

    RegisterRuntimeExport("??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z",
                          (void*)&RtObject_InternalBasePtr_CtorPtr); ++n;
    RegisterRuntimeAddr(0x1800062a0, (void*)&RtObject_InternalBasePtr_CtorPtr);

    RegisterRuntimeExport("?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z",
                          (void*)&RtObject_InternalBasePtr_Copy); ++n;
    RegisterRuntimeAddr(0x1800062b0, (void*)&RtObject_InternalBasePtr_Copy);

    RegisterRuntimeExport("?DeletePtr@InternalBasePtr@Object@@QEAAXXZ",
                          (void*)&RtObject_InternalBasePtr_DeletePtr); ++n;
    RegisterRuntimeAddr(0x1800c0150, (void*)&RtObject_InternalBasePtr_DeletePtr);

    RegisterRuntimeExport("?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtObject_Internal_Persist); ++n;
    RegisterRuntimeAddr(0x1800bfda0, (void*)&RtObject_Internal_Persist);

    // ---- CheckPtr: DEFAULT-ON as of 2026-09-08 (Wave 9 lane Y) --------------
    //
    // It used to be opt-in behind the old DLL-era switch, for the reason
    // written on the function: registering it made the exe-side byte sniff in
    // engine_funcs.cpp see a GCC prologue instead of a RET, flip
    // `g_checkPtrIsNop` to false, and pay ~60k indirect calls a frame for a
    // no-op. THAT WAS A FACT ABOUT THE DETECTOR, NOT ABOUT THE PORT, and it is
    // now fixed at its source: the detector asks
    // `RuntimeExportIsRegistered("?CheckPtr@...")` first, so our empty body is
    // recognised and the flag stays TRUE. Registering is therefore behaviour-
    // and cost-neutral with the DLL present, and in the perf build (
    // BANISHED_CHECKPTR_STATIC_NOP) the call sites are compiled out regardless.
    //
    // The reason to register rather than leave it: at cutover the symbol has to
    // EXIST. `?CheckPtr@InternalBasePtr@Object@@IEBAXXZ` is on the DLL-absent
    // boot's [RtUnresolved] list (attempt 04, asked at 7ff71e24209f), and 2,333
    // of its 3,475 call sites have no null guard — they call through the macro,
    // which with a null resolve and a false flag calls a null pointer.
    //
    // the old DLL-era switch is kept as a positive assertion for anyone
    // measuring the decision; it no longer gates the registration.
    RegisterRuntimeExport("?CheckPtr@InternalBasePtr@Object@@IEBAXXZ",
                          (void*)&RtObject_InternalBasePtr_CheckPtr); ++n;
    RegisterRuntimeAddr(0x1800c0290, (void*)&RtObject_InternalBasePtr_CheckPtr);

    // ---- InternalBasePtr::Serialize — UNGATED 2026-09-07 (Wave 9) -----------
    // It sat in the TABLE tier below, but not because it touches the handle
    // table: it does not. It was gated because WAVE3.md could not verify it —
    // "has no oracle at all and lands in the save/load path". The lane S prefix
    // oracle now verifies it on BOTH branches against the shipped body:
    //   non-null  4 emissions (bool present, int index, uint gen, uint type)
    //             identical — and those four ARE the whole emission sequence,
    //             since the only thing after them is PersistObject, which both
    //             sides would call identically
    //   null      1 emission identical, full compare, no stop needed
    // Gated-because-unverified is not the same as gated-because-dangerous, and
    // keeping the two in one switch made a verified port look like a NO-GO.
    RegisterRuntimeExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtObject_InternalBasePtr_Serialize); ++n;
    RegisterRuntimeAddr(0x1800c0180, (void*)&RtObject_InternalBasePtr_Serialize);

    // ---- WAVE 9 LANE UID — the address door for FUN_18002fca0 --------------
    // The body is ArrayPtrSetSize, ported since Wave 3 and never registered.
    // Ungated for the reason spelled out at the adapter: it is a container
    // primitive, not a handle-table mutator. Its first address caller is
    // `??1DialogInstance@UI@@` (runtime/src/ui/dialog.cpp, lane UID).
    RegisterRuntimeAddr(0x18002fca0, (void*)&RtObject_ArrayPtrSetSize_18002fca0); ++n;

    // ---- ClearPersistState — UNGATED 2026-09-09 (Wave 11 lane OBJINT) -------
    //
    // It sat in the TABLE tier below. IT DOES NOT BELONG THERE, and the reason
    // is the one Wave 9 gave when it ungated `InternalBasePtr::Serialize`:
    // gated-because-unverified is not the same as gated-because-dangerous, and
    // keeping the two in one switch makes a verified port look like a NO-GO.
    //
    // WHY IT IS NOT DANGEROUS. The tier's hazard, stated in the arming block
    // above, is TWO WRITERS ON THE HANDLE TABLE — our `_objects` mutations
    // racing the DLL's, where a wrong `_factoryIndex` is the swap-remove wild
    // write with no proximate symptom. **This body never touches `_objects`.**
    // It frees `_persistIds._data` and zeroes that array's three fields, and
    // `_persistIds` is touched by exactly two functions in the whole engine —
    // this one and `ResetPersistState`. There is no second writer to race.
    //
    // WHY IT IS NO LONGER UNVERIFIED. WAVE3.md's defect E recorded that this
    // function's shadow HAD NEVER EXECUTED, and that stood for eight waves.
    // Lane OBJINT ran it (`BANISHED_AUTOLOAD`, `calls=1 divergences=0`) and,
    // because a live shadow is structurally unable to see the one thing that
    // could be wrong here, also built a synthetic-subject A/B that watches the
    // ALLOCATOR'S OWN COUNTERS across the call
    // (runtime/src/object/internalfactory_persist_probe.cpp):
    //
    //     ours{blocks=-1 bytes=-72}  theirs{blocks=-1 bytes=-72}   IDENTICAL
    //
    // and its negative control — the same body with the `MemFree` deleted —
    // was CAUGHT, by that screen and only by that screen. That is the leak a
    // green post-state check would have passed
    // (reference_a_written_recipe_can_be_wrong.md).
    //
    // NOT registered in SHADOW mode: the shadow wrapper above claims the same
    // two doors, and a second binding would win and silently replace the
    // oracle with the thing it was measuring.
    RegisterRuntimeExport("?ClearPersistState@InternalFactory@Object@@QEAAXXZ",
                          (void*)&RtObject_InternalFactory_ClearPersistState); ++n;
    RegisterRuntimeAddr(0x1800bfe30, (void*)&RtObject_InternalFactory_ClearPersistState);

    // ---- Tier 2: the real table ports. ---------------------------------------
        RegisterRuntimeExport("??0Internal@Object@@QEAA@XZ",
                              (void*)&RtObject_Internal_Ctor); ++n;
        RegisterRuntimeAddr(0x1800bfd10, (void*)&RtObject_Internal_Ctor);

        RegisterRuntimeExport("??1Internal@Object@@UEAA@XZ",
                              (void*)&RtObject_Internal_Dtor); ++n;
        RegisterRuntimeAddr(0x1800bfdb0, (void*)&RtObject_Internal_Dtor);

        // ---- ResetPersistState — STAYS IN THE TIER, and NOT because it is
        // unverified. Lane OBJINT verified the BODY as thoroughly as
        // ClearPersistState's: the shadow ran (calls=1, divergences=0) and the
        // synthetic A/B found the emission sequence identical (3 emissions —
        // the encoding-factory create, the int count, the destroy) with an
        // identical post-state, its fill control CAUGHT.
        //
        // It stays because of ONE LINE that ClearPersistState does not have:
        //
        //     ArrayPtrReserve(&self->objects, count);
        //
        // `_objects` IS the handle table, and on the LOAD leg `count` comes off
        // the stream and can exceed `_size`, so that call can REALLOC the array
        // the DLL's own `Internal` ctors and dtors are also writing. Reserve
        // never moves an index or a `_factoryIndex`, and the DLL re-reads
        // `_data` through the singleton on every access, so the argument that
        // it is safe is a good one — but it is an ARGUMENT, and the tier exists
        // precisely because this project has been wrong about a two-writer
        // array before with no proximate symptom. Record the distinction
        // rather than collapsing it: this row is VERIFIED-BUT-UNARMED, which is
        // a different state from the UNVERIFIED it was in until today, and the
        // measurement it still owes is a live tier-2 load run, not another
        // oracle.
        RegisterRuntimeExport("?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z",
                              (void*)&RtObject_InternalFactory_ResetPersistState); ++n;
        RegisterRuntimeAddr(0x1800bfe40, (void*)&RtObject_InternalFactory_ResetPersistState);


    printf("[RtPort]   Object::Internal: TABLE mode — our code drives the handle "
           "table. Shadow gate step 1 was met in Wave 3 (93,387 calls each on the "
           "ctor/dtor, 0 divergences); step 2 (the encoding-factory arity) was closed "
           "2026-09-08.\n");

    printf("[RtPort]   Object::Internal: %d exports registered (INCLUDING the table tier)\n", n);
    return n;
}
