// ============================================================================
// runtime/src/object/internalfactory_persist.cpp — BATCH 5 (2026-09-12)
//   `?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z`
//   retail 0x1800bff10, ordinal 3326, 248 bytes / 57 instructions.
// ============================================================================
//
//     Internal* InternalFactory::PersistObject(IO::Stream& stream,
//                                              unsigned int generation,
//                                              int          index,
//                                              unsigned int typeId)
//
// The save/load hinge for handle-table objects: ONE function that, on the write
// path, marks an object as already-emitted and serialises it, and on the read
// path CREATES the object from its type id and serialises into it. Which of the
// two runs is decided by `stream+0x08` — `0 == reading`, the same field
// `IO::Serializer`'s constructor branches on
// (runtime/types/object/Internal.h pins it, from 0x1800b9960).
//
// ---------------------------------------------------------------------------
// THE TWO ARRAYS, AND WHY `_persistIds` HOLDS TWO DIFFERENT THINGS
// ---------------------------------------------------------------------------
// `this+0x00` is `Array<Internal*> _objects` and `this+0x10` is
// `Array<int32> _persistIds` (runtime/types/object/Internal.h). Retail reads
// only their `_data` pointers here and never their sizes — THERE IS NO BOUNDS
// CHECK ON EITHER ARRAY on either path, and none is added: the caller is the
// serializer, which drives `index` from the stream, and retail relies on the
// table having been sized first.
//
// `_persistIds[i]` is a SENTINEL on the way in and an INDEX on the way out:
//   * negative  -> "slot i has not been visited in this pass"
//   * otherwise -> the index in `_objects` the pass resolved it to
// The write path stores `index` itself (0x1800bffda, `MOV [RAX+RBX*4],EBX`,
// where EBX is the sign-extended `index`); the read path stores the NEW
// object's `_factoryIndex` (0x1800bffb8). That asymmetry is the function, not
// an accident, and it is why the two arms cannot share a line.
//
// ---------------------------------------------------------------------------
// THE READ PATH — a type-tree descent, a factory call, and an UNGUARDED STORE
// ---------------------------------------------------------------------------
//   1800bff51  RAX = *Factory::_singleton                    (0x1801fc6e8)
//   1800bff70  the descent: unsigned compare on the node key, LEFT at +0x38
//              when `typeId < key`, RIGHT at +0x40 when `key < typeId`, and a
//              MISS leaves RAX = RSI = 0 (0x1800bff89 `MOV RAX,RSI`)
//   1800bff8c  RCX = node->value.info      (node + 0x10 == TypeRecord::info)
//   1800bff90  CALL [RCX+0x20]             BaseTypeInfo::factoryFn(this=info)
//   1800bff93  EDX = [0x1801f9c18]         Internal's _typeInfo node, field +0
//   1800bff9f  CALL 0x1800c0390            Base::IsDerivedFrom(obj, typeId)
//   1800bffa6  CMOVNZ RSI,RBX              keep the object ONLY if it is one
//   1800bffad.. the stores, through RSI
//
// *** RSI IS NULL ON EVERY FAILING PATH AND RETAIL STILL WRITES THROUGH IT. ***
// A missing type id, or a factory that returns something that is not an
// `Object::Internal`, leaves RSI zero and `MOV [RSI+0xc],R15D` faults at
// address 0xc. That is retail's behaviour and it is load-bearing: the engine
// relies on a save file naming a registered Internal type, and a guard here
// would not restore the behaviour, it would skip it and hand the caller a
// silently wrong object (CLAUDE.md, "Don't 'fix' a faithful crash with a null
// check"). The descent's miss arm is transcribed the same way — it produces a
// null node and then dereferences it at +0x10, exactly as written.
//
// `0x1801f9c18` is `?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A` + 0x00, the
// type id — the same address `?Type@Internal@Object@@UEBAIXZ` (0x1800064d0) is
// two instructions of, which is how
// runtime/src/object/object_typeinfo_accessors.cpp identifies it. It is read
// HERE through the same RT_DATA_SLOT name door that file uses, so that a
// promotion of the row is followed rather than the DLL's node being pinned.
// (standalone/reg_mint_promoted.cpp already names `0x1800bff92` — an
// instruction inside this body — as one of the five in-DLL readers of that
// node. This is that reader, now ours.)
//
// ---------------------------------------------------------------------------
// BOTH ARMS END IN THE SAME VIRTUAL CALL, SLOT 3
// ---------------------------------------------------------------------------
// `CALL [RAX+0x18]` at 0x1800bffca and 0x1800bffe7 — vtable slot 3 with the
// stream in RDX. On the write path RDX is the untouched argument; on the read
// path retail re-loads it (`MOV RDX,RBP` at 0x1800bffc0) because RDX was used
// by the descent. Same slot, same argument, two loads.
//
// ---------------------------------------------------------------------------
// THE RETURN IS READ FROM `_objects` AFTER THE SERIALIZE, NOT BEFORE
// ---------------------------------------------------------------------------
//   1800bffea  RAX = *this            (_objects._data, RE-LOADED)
//   1800bfff2  MOVSXD RCX,EBX         the slot, which the read path UPDATED at
//                                     0x1800bffc3 from the array it had just
//                                     written
//   1800bfff5  return RAX[RCX]
// The re-load matters: `Serialize` can grow `_objects` and move the buffer.
// Caching `_objects._data` at the top — which is what a tidied version would do
// — would return a pointer into a freed block.
//
// ---------------------------------------------------------------------------
// ORACLE — NONE, and the refusal is structural
// ---------------------------------------------------------------------------
// Both arms CONSUME the stream through a virtual `Serialize`. Running the
// shipped body a second time for comparison would read the next record's bytes
// on the load path and emit the object twice on the save path — a different
// experiment whose first product is a desynced stream (the same reason
// runtime/src/io/serializer.cpp's write-path notes give, and the shape lane D
// recorded on `ClearStorage`'s no-op Serialize, which read ZERO bytes and
// desynced the next record). There is no prefix-stop that helps: the divergence
// would be in the stream, which is the thing under test. **No site is added.**
//
// What a normal boot DOES exercise is the whole path end to end: any save load
// drives this function once per persisted object, and a wrong descent, a wrong
// factory slot or a wrong sentinel shows up immediately as a failed load rather
// than as a silent mismatch.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

// ============================================================================
// ENDGAME B.3.3 step 5 — THE BODY MOVED, THIS IS THE REGISTERED SHIM
// ============================================================================
// Everything above still describes the function; what changed is where it
// lives. `Object::InternalFactory` is a real class as of step 5, so the body is
// `Object::InternalFactory::PersistObject` in
// runtime/src/object/internal_class.cpp — moved VERBATIM, including the
// unguarded miss arm, the two re-loads and the sentinel/index asymmetry. This
// file keeps the two registrations (by name and by address), which is what the
// cutover manifest and tools/runtime_census.py see.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/object/Internal.h"
#include "../../types/object/TypeInfo.h"

namespace RtObject {

// ============================================================================
// ?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z
//                                                          retail 0x1800bff10
// ============================================================================
Internal* RtObject_InternalFactory_PersistObject(InternalFactory* self,
                                                 void*        stream,
                                                 uint32_t     generation,
                                                 int          index,
                                                 uint32_t     typeId) {
    // B.3.3 bridge — `RtObject::InternalFactory` is the layout-asserted alias
    // of `Object::InternalFactory`.
    ::Object::Internal* p =
        reinterpret_cast< ::Object::InternalFactory*>(self)->PersistObject(
            stream, generation, index, typeId);
    return reinterpret_cast<Internal*>(p);
}

}  // namespace RtObject

// ============================================================================
// Registration — BOTH DOORS. `QEAA` is public and the name is ordinal 3326 in
// the retail export directory, verified with pefile before this was written.
// The name is spelled as a LITERAL so tools/runtime_census.py can see it.
// ============================================================================
int RtObjectInternalFactoryPersistRegister() {
    int n = 0;
    RegisterRuntimeExport("?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@"
                          "AEAVStream@IO@@IHI@Z",
                          (void*)&RtObject::RtObject_InternalFactory_PersistObject); ++n;
    RegisterRuntimeAddr(0x1800bff10,
                        (void*)&RtObject::RtObject_InternalFactory_PersistObject);
    return n;
}
