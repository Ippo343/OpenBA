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
//   * There is ONE vtable per class, the compiler's (ENDGAME B.3.9 OBJ): the
//     `??0Internal` / `??1Internal` doors that hand-stamped the exported
//     `??_7Internal@Object@@6B@` / `??_7Base@Object@@6B@` tables are gone with
//     those minted rows; every Internal is built by `Object::Internal()`.
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
// ENDGAME B.3.3 step 5 — the allocator, the PRNG and the live singleton MOVED
// ============================================================
// `MemAlloc`/`MemFree`/`RandomRand` and the `?_singleton@InternalFactory@...`
// door used to live here. They are now `Object::detail::InternalMemAlloc` /
// `InternalMemFree` / `InternalRand` and `Object::InternalFactory::Get()`
// (runtime/src/object/internal_class.cpp), which is the file that can link into
// `make unit`. The doors are the SAME mangled names resolved through the SAME
// registry; only the spelling moved.
//
//   ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  0x1800c7a50
//   ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     0x1800c7ad0
//   ?Rand@Random@System@@QEAAIXZ                   0x1800c7f90
//   ?_singleton@InternalFactory@Object@@0PEAV12@EA 0x1801fc6d8

// ============================================================
// The two exported vtables -- GONE (ENDGAME B.3.9 OBJ)
// ============================================================
// `VtblInternal()` / `VtblBase()` served the `??0Internal` / `??1Internal`
// doors' hand stamps of the minted tables. The doors went with the minted
// `??_7Internal@Object@@6B@` row and its RT_DELDTOR row; every Internal is
// built by `Object::Internal::Internal()` and wears the compiler's table.


// ============================================================
// Collection::Array<T> bodies, as retail instantiates them — MOVED
// ============================================================
// All four (FUN_18002ffa0, FUN_18002fca0, FUN_18002fd90, FUN_18002fea0) are now
// `Object::detail::ArrayPtrSetSizeFill` / `ArrayPtrSetSize` / `ArrayPtrReserve`
// / `ArrayIntSetSize` in runtime/src/object/internal_class.cpp, moved VERBATIM
// with the element spelling changed from `RtObject::Internal*` to
// `Object::Internal*` — the same eight bytes. They moved because
// `InternalFactory::AddObject`/`RemoveObject`/`ResetPersistState` are the only
// callers and those are now the real class's members. WAVE2's warning stands
// where they live now: one ported body cannot serve two element widths.

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

// ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z (0x1800bfda0, slot 3):
// ENDGAME B.3.9 OBJ -- the slot door went with the minted table, its only
// reader; the body is `Object::Internal::Persist` (internal_class.cpp).

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
// ??0Internal@Object@@QEAA@XZ (0x1800bfd10) / ??1Internal@Object@@UEAA@XZ
// (0x1800bfdb0): ENDGAME B.3.9 OBJ -- the two doors went with the minted
// `??_7Internal@Object@@6B@` row, its RT_DELDTOR row 0x180006440 (the ??1
// door's caller) and import slot 0x140152768 (the ??0 door's). The bodies are
// `Object::Internal::Internal()` / `~Internal()` (internal_class.cpp); the
// [RtInternalTable] / [RtInternalVtbl] instruments went with them (D9).

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
// B.3.4 block 3: the parameter is the REAL `IO::Serializer` now — it was
// `RtObject::SerializerLite`, a fourth declaration of the same 16 bytes, which
// died with this block. Same bytes, same ABI, one spelling.
void RtObject_InternalBasePtr_Serialize(InternalBasePtr* self, ::IO::Serializer* ser) {
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

// B.3.4 block 3 — the parameter is the REAL `IO::Serializer`, and the three
// slot constants are `rt::io::rw_vtbl::k*` instead of the identical
// `RtObject::kEncSerialize*` that died with `SerializerLite`. Same three retail
// offsets (0x78 / 0x98 / 0xa0), one spelling. B3X W0: the minted tables are
// retired (B.3.9), so the ReaderWriter and the object's Type() are called by
// name -- the same slots.
void InternalBasePtrSerializeBody(RtObject::Internal** self,
                                  ::IO::Serializer& ser) {
    ::IO::ReaderWriter* const enc = ser._readerWriter;

    // A one-byte `char` through the bool slot (+0x78), as retail passes it.
    char present = (*self != nullptr) ? 1 : 0;
    enc->SerializeBool(*reinterpret_cast<bool*>(&present), nullptr);
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
        type = reinterpret_cast<const Base*>(p)->Type();   // slot 2 +0x10 (B.3.x bridge)
    }
    const uint32_t gen = (*self != nullptr) ? (*self)->generation : 0u;

    enc->SerializeInt(index, nullptr);                          // +0xa0
    enc->SerializeUInt(const_cast<uint32_t&>(gen), nullptr);    // +0x98
    enc->SerializeUInt(type, nullptr);                          // +0x98

    // ?PersistObject@InternalFactory@Object@@ (0x1800bff10), the member by
    // name (B3R RT-Core) on `Object::InternalFactory::Get()`.
    *self = reinterpret_cast<RtObject::Internal*>(
        ::Object::InternalFactory::Get()->PersistObject(ser._stream, gen, index, type));
}

}  // namespace Object

// ============================================================================
// Registration
// ============================================================================
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

    // B.3.4 STREAMS (6/6) — the symbol's FIRST registration. BY NAME *AND* BY
    // ADDRESS, because `ResolveExport` falls through silently for an unported
    // symbol while `ResolveAddr(Module::Runtime, ..)` is registry-first: a
    // caller arriving by address is invisible to a name-only registration
    // (reference_count_the_doors_not_the_calls.md). The mangled name is spelled
    // on ONE line — tools/runtime_census.py cannot see a name split across a
    // string-literal concatenation — and was read off the export directory at
    // the VA whose bytes the body transcribes (ord 1716), not assembled by
    // analogy with its `UEAA` neighbours (reference_resolveexport_phantom_names.md).

    // ---- CheckPtr: STILL REGISTERED, for a reason B.3.3 step 6D changed ------
    //
    // The old reason — "2,333 of its 3,475 exe-side call sites have no null
    // guard, so an unregistered symbol is an EXEC-of-0 at cutover" — is gone with
    // the call sites: step 6D deleted all 4,570 of them, plus the exe-side
    // pointer, the `g_checkPtrIsNop` byte sniff and the perf build's
    // `BANISHED_CHECKPTR_STATIC_NOP`. Nothing in the exe reaches this symbol any
    // more, by name or by address.
    //
    // It stays registered because the mangled name is the ABI door for
    // `Object::InternalBasePtr::CheckPtr()`, a real (empty) member of the real
    // class, exercised by tests/unit/object_ptr_test.cpp through `Shim_CheckPtr`.
    // The full argument is on the function above.

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

    // ---- WAVE 9 LANE UID — the address door for FUN_18002fca0 --------------
    // The body is ArrayPtrSetSize, ported since Wave 3 and never registered.
    // Ungated for the reason spelled out at the adapter: it is a container
    // primitive, not a handle-table mutator. Its first address caller is
    // `??1DialogInstance@UI@@` (runtime/src/ui/dialog.cpp, lane UID).

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

    // ---- Tier 2: the real table ports. ---------------------------------------
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


    // ENDGAME B.3.9 OBJ (B39_RULES D2): retail's factory pair is the class's
    // own statics (internal_class.cpp, both `XOR EAX,EAX; RET`), registered by
    // name AND VA here instead of by the FACTREST interpreter row. The in-place
    // one is the SHARED `?CreateInPlaceObject@Internal@Object@@` every
    // Internal-rooted class's record names; it stays registered here.

    printf("[RtPort]   Object::Internal: TABLE mode — our code drives the handle "
           "table. Shadow gate step 1 was met in Wave 3 (93,387 calls each on the "
           "ctor/dtor, 0 divergences); step 2 (the encoding-factory arity) was closed "
           "2026-09-08.\n");

    printf("[RtPort]   Object::Internal: %d exports registered (INCLUDING the table tier)\n", n);
    return n;
}
