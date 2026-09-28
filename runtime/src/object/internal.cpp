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
