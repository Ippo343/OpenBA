#pragma once
// ============================================================================
// runtime/types/object/internal_class.h - ENDGAME B.3.3 step 5
// ============================================================================
//
// `Object::Internal` and `Object::InternalFactory` as REAL classes, under their
// retail names. The POD descriptions Wave 3 lane B measured stay in
// runtime/types/object/Internal.h and this file is cross-asserted against them
// field for field, so the two descriptions cannot drift (B31_RULES step 1: the
// old PODs keep working until their last caller converts).
//
// WHY THIS IS A SEPARATE HEADER AND NOT MORE OF Internal.h
// -------------------------------------------------------
// It needs `Object::Base`, `Object::BaseTypeInfo` and `DECLARE_TYPE`, i.e.
// TypeInfo.h - and `runtime/types/object/External.h` includes TypeInfo.h from
// INSIDE `namespace RtObject` (External.h:347, where it replaced a local copy of
// the TypeNode block). With `#pragma once` that first inclusion decides the
// namespace for the whole program: a translation unit that includes External.h
// before Internal.h gets `RtObject::Object::Base` and nothing at global scope,
// which is exactly what tests/unit/object_ptr_test.cpp does.
//
// Putting the include here instead of in Internal.h keeps that pre-existing
// include-order hazard where it already was: Internal.h's graph is unchanged,
// and only the files that want the real classes include this header, all of them
// at global scope. THE UNDERLYING FIX IS TO MOVE External.h:347's
// `#include "TypeInfo.h"` above its `namespace RtObject {` at line 37 - a
// one-line change in a file this step does not own, written up in the step's
// report for whoever owns External.h.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <new>
#include <type_traits>

#include "TypeInfo.h"                 // Object::Base, BaseTypeInfo, DECLARE_TYPE
#include "Internal.h"                 // the alias PODs this file is asserted against
#include "../collection/Collection.h"
#include "../system/Value.h"          // ::Random (0x9c4 bytes, index + mt[624])

namespace Object {

// ############################################################################
// ENDGAME B.3.3 step 5 — `Object::Internal` and `Object::InternalFactory` as
// REAL classes, under their retail names.
//
// MEASURED VTABLE. `??_7Internal@Object@@6B@` @ 0x180154720, read out of .rdata
// on 2026-09-18 (Runtime-x64.dll, image base 0x180000000) — FOUR slots, bounded
// by `??_7Achievement@0@6B@` at 0x180154740:
//
//   | slot | +off | address     | body                                   | name |
//   |------|------|-------------|----------------------------------------|------|
//   | 0    | 0x00 | 0x180006440 | VECTOR deleting dtor: `TEST DL,0x2`,   | NO RETAIL NAME in the |
//   |      |      |             | elem size 0x10, `LEA R9,[0x1800bfdb0]` | export table — it is  |
//   |      |      |             | (= ~Internal), free via 0x180006a10    | Base's `DeletingDtor` |
//   |      |      |             |                                        | slot, overridden      |
//   | 1    | 0x08 | 0x1800064c0 | `MOV RAX,[0x1801f9c20]; RET`           | `?TypeName@Internal@Object@@UEBAPEBDXZ` |
//   | 2    | 0x10 | 0x1800064d0 | `MOV EAX,[0x1801f9c18]; RET`           | `?Type@Internal@Object@@UEBAIXZ` |
//   | 3    | 0x18 | 0x1800bfda0 | `RET` — EMPTY in retail AND profile    | `?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z` |
//
// Slots 1 and 2 are exactly what `DECLARE_TYPE` emits over the class's own
// `_typeInfo`, and the rest of retail's reflection block for this class is in
// the export table at the addresses named on each member below. So `Internal`
// takes `DECLARE_TYPE(Internal)`; `DEFINE_TYPE` lives in
// runtime/src/object/internal_class.cpp.
//
// NO VIRTUAL DESTRUCTOR (B33_RULES §1.3, decision 2): GCC would spend two slots
// and shift `Type`, `TypeName` and `Persist` by one, and every one of the ~83
// files still stamping a MINTED table would dispatch one slot out. `~Internal()`
// is protected and non-virtual; deletion is `Object::Destroy(p)`.
//
// THE MINTED TABLE STAYS. `RT_DELDTOR(0x180006440, 0x1800bfdb0, 0x010,
// "Object::Internal")` (deleting_dtor.cpp:294) both dispatches slot 0 AND FILLS
// slot 0 of the minted `??_7Internal@Object@@6B@` (rt_data_mint_tables.inc
// kVSlots76, 4 slots). Retiring it publishes a trap table into live objects
// (B33_RULES §6.1), so nothing here removes a row or a registration; the real
// class coexists with minted-vtable objects exactly as
// tests/unit/object_vtable_abi_test.cpp proves it can.
// ############################################################################

class Internal;
class InternalFactory;

// ------------------------------------------------------------
// The seam this family needs from outside itself. Defined in
// runtime/src/object/internal_class.cpp, which has a malloc/no-registry branch
// under -DBANISHED_UNIT_TEST exactly as runtime/src/object/type_reflection.cpp
// and runtime/src/collection/linkedlist.cpp do.
//
//   ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  0x1800c7a50
//   ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     0x1800c7ad0  (== 0x180006a10)
//   ?Rand@Random@System@@QEAAIXZ                   0x1800c7f90
// ------------------------------------------------------------
namespace detail {
void*    InternalMemAlloc(uint64_t bytes, int align);
void     InternalMemFree(void* p);
uint32_t InternalRand(::Random* random);

// The four `Collection::Array` instantiations retail emits for this class, one
// body per element width, each at its own retail address. They are the Wave 3
// transcriptions moved here from internal.cpp unchanged — they are NOT
// `Collection::Array<T>::SetSize`, because retail's inlined growth here goes
// through the MemoryAllocator statics above rather than the container's own
// allocator policy, and this step does not move that door.
//
//   FUN_18002ffa0  Array<Internal*>::SetSize(int, const Internal*&)
//   FUN_18002fca0  Array<Internal*>::SetSize(int)          — shrink-only in retail
//   FUN_18002fd90  Array<Internal*>::Reserve(int)
//   FUN_18002fea0  Array<int32>::SetSize(int)
void ArrayPtrSetSizeFill(Collection::Array<Internal*>* a, int newSize, Internal* const* fill);
void ArrayPtrSetSize(Collection::Array<Internal*>* a, int newSize);
void ArrayPtrReserve(Collection::Array<Internal*>* a, int n);
void ArrayIntSetSize(Collection::Array<int32_t>* a, int newSize);
}  // namespace detail

// ============================================================
// Object::Internal — 16 bytes: Base's vptr plus two dwords.
// ============================================================
class Internal : public Base {
    DECLARE_TYPE(Internal)

public:
    // ??0Internal@Object@@QEAA@XZ          retail 0x1800bfd10 / profile 0x18008b370
    //     Base::Base(this); vptr = &Internal::_vftable_;
    //     _factoryIndex = -1; _generation = 0;
    //     InternalFactory::_singleton->AddObject(this);
    // The `-1` / `0` pair is a dead store in BOTH builds — AddObject overwrites
    // both before anything can observe them — and is reproduced anyway.
    Internal();

    // ??0Internal@Object@@QEAA@AEBV01@@Z   retail 0x1800064e0
    //     LEA RAX,[0x180154788]; MOV [RCX],RAX      (Base's vptr)
    //     LEA RAX,[0x180154720]; MOV [RCX],RAX      (Internal's vptr)
    //     +0x08 and +0x0c copied verbatim.
    // *** IT DOES NOT CALL AddObject. *** The copy inherits the ORIGINAL's
    // `_factoryIndex`, so two live objects share one table slot and the second
    // `~Internal` swap-removes a slot that no longer names it. That is retail's
    // behaviour, it is the shape
    // reference_internalfactory_swapremove_wild_write.md describes, and it is
    // transcribed, not fixed.
    Internal(const Internal& other);

    // ??4Internal@Object@@QEAAAEAV01@AEBV01@@Z  retail 0x180006430
    // Four instructions: the two dwords, no vptr store, no table call.
    Internal& operator=(const Internal& other);

    // ---- vtable slot 0 — 0x180006440 ------------------------------------
    Base* DeletingDtor(unsigned flags) override;

    // ---- vtable slot 3 — 0x1800bfda0, `RET` in retail AND in the profile
    // build. It exists so a subclass has something to override, and
    // `InternalFactory::PersistObject` calls it on every object it creates from
    // a stream. `IO::Stream` is not a type in this tree yet (Wave 4 owns it), so
    // the parameter is the `void*` every C caller already passes.
    virtual void Persist(void* stream);

    int32_t  GetFactoryIndex() const { return _factoryIndex; }
    uint32_t GetGeneration()   const { return _generation; }

    // The one-line forwarder the registered `RtObject_Internal_Dtor` shim needs:
    // `~Internal()` is protected so that `delete p` cannot compile on a class
    // whose deletion must go through slot 0. Nothing else may call this.
    static void Shim_Dtor(Internal* self) { self->~Internal(); }

    // ---- the 16 bytes, in retail order ----
    // PUBLIC for the same reason `Object::BaseTypeInfo`'s are: every
    // unconverted body in the tree reads them at these offsets through a cast,
    // and `offsetof` on them has to be well formed for the pins below. Retail's
    // are private (`_factoryIndex`, `_generation` in the profile asserts).
    int32_t  _factoryIndex;   // +0x08  own slot in InternalFactory::_objects
    uint32_t _generation;     // +0x0c  InternalFactory::_random.Rand() at birth

protected:
    // ??1Internal@Object@@UEAA@XZ          retail 0x1800bfdb0 / profile 0x18008b400
    //     vptr = &Internal::_vftable_; _generation = 0;
    //     InternalFactory::_singleton->RemoveObject(this);
    //     Base::~Base(this);                    // vptr = &Base::_vftable_
    // The two vptr stores are what a real destructor chain emits for us.
    // NON-VIRTUAL: slot 0 is `DeletingDtor`, decision 2.
    ~Internal();
};

static_assert(sizeof(Internal) == 0x10, "Object::Internal is 16 bytes");
static_assert(sizeof(Internal) == sizeof(RtObject::Internal),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(Internal, _factoryIndex) == 0x08, "Internal::_factoryIndex at +0x08");
static_assert(offsetof(Internal, _generation)   == 0x0c, "Internal::_generation at +0x0c");
static_assert(offsetof(Internal, _factoryIndex) == offsetof(RtObject::Internal, factoryIndex),
              "alias _factoryIndex");
static_assert(offsetof(Internal, _generation)   == offsetof(RtObject::Internal, generation),
              "alias _generation");

// ============================================================
// Object::InternalFactory — 0x9e8 bytes (0x9e4 of fields, alignof 8).
//
// The engine-wide handle table. Layout and evidence: the `RtObject` POD above,
// which this class is cross-asserted against field for field.
//
// NO DESTRUCTOR IS DECLARED, deliberately. `??1InternalFactory@Object@@QEAA@XZ`
// (retail 0x18002f860) is a NON-VIRTUAL member whose port lives in
// runtime/src/object/factory_dtors.cpp — a file this step does not own — and
// `??0InternalFactory@Object@@QEAA@XZ` (0x1800c0030) is registered from
// runtime/src/engine/singleton_ctors.cpp. Declaring either here would give the
// class a second, divergent spelling of a body that is already registered at
// those two addresses. The singleton is heap-allocated once and torn down
// through those doors; nothing constructs one of these by value.
// ============================================================
class InternalFactory {
public:
    // ?Get@InternalFactory@Object@@SAPEAV12@XZ   retail 0x18002f850 —
    // `MOV RAX,[0x1801fc6d8]; RET`, i.e. the exported data symbol
    // `?_singleton@InternalFactory@Object@@0PEAV12@EA` read straight out.
    static InternalFactory* Get();

    // ?Startup@InternalFactory@Object@@QEAA_NXZ  retail 0x1800c0020 —
    // `MOV AL,1; RET`. It really does nothing but succeed.
    bool Startup();
    // ?Shutdown@InternalFactory@Object@@QEAAXXZ  retail 0x1800c0010 — bare RET.
    void Shutdown();

    // ?AddObject@InternalFactory@Object@@QEAAXPEAVInternal@2@@Z   0x1800c00c0
    //     obj->_factoryIndex = _objects._size;
    //     obj->_generation   = _random.Rand();
    //     _objects.SetSize(_objects._size + 1, &obj);
    void AddObject(Internal* obj);

    // ?RemoveObject@InternalFactory@Object@@QEAAXPEAVInternal@2@@Z 0x1800c0110
    //     last = _objects._size - 1;  idx = obj->_factoryIndex;
    //     if (last != idx) { _objects[idx] = _objects[last];
    //                        _objects[idx]->_factoryIndex = idx; }
    //     _objects.SetSize(last);          // tail JMP to 0x18002fca0
    // NO VALIDATION OF `idx`, here or in the profile build. A stale index lands
    // the second store wherever the allocator has since put that block; the fix
    // for that family is upstream (unregister before freeing), never a guard
    // here.
    void RemoveObject(Internal* obj);

    // ?ClearPersistState@InternalFactory@Object@@QEAAXXZ   0x1800bfe30
    void ClearPersistState();

#if !defined(BANISHED_UNIT_TEST)
    // ?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z 0x1800bfe40
    void ResetPersistState(void* stream);

    // ?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z
    //                                                      0x1800bff10
    Internal* PersistObject(void* stream, uint32_t generation, int index,
                            uint32_t typeId);
#endif

    Collection::Array<Internal*>&       GetObjects()          { return _objects; }
    const Collection::Array<Internal*>& GetObjects() const    { return _objects; }
    Collection::Array<int32_t>&         GetPersistIds()       { return _persistIds; }
    ::Random&                           GetRandom()           { return _random; }

    // PUBLIC for the same reason `Object::Factory::_types` is: the unconverted
    // C bodies and the exe-side audit (code/Game/Object/internal_factory_audit.h)
    // reach these directly, and `offsetof` on them has to be well formed.
    Collection::Array<Internal*> _objects;      // +0x00
    Collection::Array<int32_t>   _persistIds;   // +0x10
    ::Random                     _random;       // +0x20
};

static_assert(sizeof(InternalFactory) == 0x9e8,
              "Object::InternalFactory: 0x9e4 bytes of fields, padded to 0x9e8 by alignof 8");
static_assert(sizeof(InternalFactory) == sizeof(RtObject::InternalFactory),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(InternalFactory, _objects)    == 0x00, "_objects at +0x00");
static_assert(offsetof(InternalFactory, _persistIds) == 0x10, "_persistIds at +0x10");
static_assert(offsetof(InternalFactory, _random)     == 0x20, "_random at +0x20");
static_assert(offsetof(InternalFactory, _objects) ==
                  offsetof(RtObject::InternalFactory, objects),    "alias _objects");
static_assert(offsetof(InternalFactory, _persistIds) ==
                  offsetof(RtObject::InternalFactory, persistIds), "alias _persistIds");
static_assert(offsetof(InternalFactory, _random) ==
                  offsetof(RtObject::InternalFactory, random),     "alias _random");

}  // namespace Object
