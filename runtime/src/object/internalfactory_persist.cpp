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

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/object/Internal.h"
#include "../../types/object/TypeInfo.h"

namespace RtObject {

namespace {

// `?_singleton@Factory@Object@@0PEAV12@EA` — note the `0PEAV12@EA` spelling;
// the obvious `1PEAV12@A` is a phantom that resolves NULL silently
// (reference_resolveexport_phantom_names.md). RtResolveCached caches a hit and
// RETRIES a miss, so a row minted later is picked up.
void* g_factorySingletonCache = nullptr;                 // Runtime 0x1801fc6e8

Factory** FactorySingletonSlot() {
    return reinterpret_cast<Factory**>(RtResolveCached(
        "?_singleton@Factory@Object@@0PEAV12@EA", &g_factorySingletonCache));
}

// `Object::Internal`'s own BaseTypeInfo node, read through the NAME door so a
// promotion of the row is followed. Field +0x00 is the type id.
const uint8_t* InternalTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A");
}

// `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` (0x1800c0390), ported and registered
// by runtime/src/object/type_system.cpp. Reached through the registry so this
// call lands on OUR body with the DLL present or absent.
bool IsDerivedFrom(const void* self, uint32_t typeId) {
    using Fn = bool (*)(const void*, uint32_t);
    static void* cache = nullptr;
    Fn fn = reinterpret_cast<Fn>(
        RtResolveCached("?IsDerivedFrom@Base@Object@@QEBA_NI@Z", &cache));
    return fn(self, typeId);
}

// IO::Stream + 0x08 — `0 == reading`, non-zero == writing. The only field of
// Stream this body touches.
inline int32_t StreamMode(const void* stream) {
    return *reinterpret_cast<const int32_t*>(
        reinterpret_cast<const uint8_t*>(stream) + 0x08);
}

// vtable slot 3 (+0x18) — `virtual void Serialize(IO::Stream&)`.
inline void SerializeVirtual(void* self, void* stream) {
    void* const* vt = *reinterpret_cast<void* const* const*>(self);
    reinterpret_cast<void (*)(void*, void*)>(vt[3])(self, stream);
}

}  // namespace

// ============================================================================
// ?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z
//                                                          retail 0x1800bff10
// ============================================================================
Internal* RtObject_InternalFactory_PersistObject(InternalFactory* self,
                                                 void*        stream,
                                                 uint32_t     generation,
                                                 int          index,
                                                 uint32_t     typeId) {
    int32_t* const persistIds =
        reinterpret_cast<int32_t*>(self->persistIds._data);   // 0x1800bff27
    const int64_t i = index;                                  // MOVSXD RBX,R9D

    int32_t slot = index;

    if (StreamMode(stream) != 0) {
        // ---- WRITE, 0x1800bffd4 --------------------------------------------
        if (persistIds[i] < 0) {
            persistIds[i] = static_cast<int32_t>(i);
            Internal* obj = reinterpret_cast<Internal**>(self->objects._data)[i];
            SerializeVirtual(obj, stream);
        }
        // `slot` stays `index`: EBX is never reassigned on this arm.
    } else {
        // ---- READ, 0x1800bff3d ---------------------------------------------
        slot = persistIds[i];
        if (slot < 0) {
            // ---- the type-tree descent, 0x1800bff5f ------------------------
            Factory** const pSingleton = FactorySingletonSlot();
            TypeNode* node = pSingleton ? (*pSingleton)->root : nullptr;
            TypeNode* found = nullptr;
            while (node != nullptr) {
                if (typeId < node->key)      { node = node->left;  continue; }
                if (node->key >= typeId)     { found = node; break; }   // equal
                node = node->right;
            }
            // A MISS LEAVES `found` NULL AND RETAIL DEREFERENCES IT. See the
            // banner; no guard is added.
            BaseTypeInfo* const info = found->value.info;      // node + 0x10
            // Zero-argument, exactly as type_system.cpp:390 calls it — retail
            // leaves `info` in RCX at the call site but the factory is a
            // static that reads nothing.
            void* const created =
                reinterpret_cast<void* (*)()>(info->factoryFn)();   // CALL [RCX+0x20]

            const uint32_t internalTypeId =
                *reinterpret_cast<const uint32_t*>(InternalTypeInfo());
            Internal* obj = IsDerivedFrom(created, internalTypeId)
                                ? static_cast<Internal*>(created)
                                : nullptr;                     // CMOVNZ RSI,RBX

            // UNGUARDED on purpose — see the banner. Retail reads
            // `_factoryIndex` (+0x08) BEFORE it writes `_generation` (+0x0c);
            // two distinct fields, so the order is unobservable, and it is
            // kept because an unobservable order that is transcribed costs
            // nothing while one that is tidied is a difference nobody can find.
            const int32_t newIndex = obj->factoryIndex;        // 0x1800bffad
            obj->generation = generation;                      // 0x1800bffb0
            // `_persistIds` is RE-LOADED here (0x1800bffb4 / 0x1800bffbc):
            // the factory call can have grown the table.
            reinterpret_cast<int32_t*>(self->persistIds._data)[i] = newIndex;
            slot = reinterpret_cast<int32_t*>(self->persistIds._data)[i];

            SerializeVirtual(obj, stream);                     // 0x1800bffca
        }
    }

    // 0x1800bffea — `_objects._data` is re-read AFTER the Serialize.
    return reinterpret_cast<Internal**>(self->objects._data)[static_cast<int64_t>(slot)];
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
