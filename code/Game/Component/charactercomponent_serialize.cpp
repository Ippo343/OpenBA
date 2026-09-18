// code/Game/Component/charactercomponent_serialize.cpp
// Slot-3 (+0x18) stream Serialize for CharacterComponent.
//
//   CharacterComponent::Serialize   retail 0x140109ab0   object size 0x80
//
// Save/load wave (2026-08-27). Retail body (byte-faithful, from decompile):
//   Component_Serialize(self, stream);
//   Serializer ser(stream, /*version*/0);
//   Vector3::Serialize(self+0x2c, ser);                  // world position (12 bytes)
//   Serialize_Float(ser, self+0x3c);                     // (order per retail: 0x3c before 0x38)
//   Serialize_Float(ser, self+0x38);
//   Serialize_Bool (ser, self+0x28);
//   Serialize_Int  (ser, self+0x60);                     // action-stack top
//   count = self+0x50; Serialize_Int(ser, &count);
//   if (loading) ResizeIBPArray(self+0x48, count);        // action-stack array (8-byte IBPs)
//   for i in [0,count): IBP::Serialize(&arr[i], ser); validate Action, null on mismatch.
//   IBP::Serialize(self+0x58, ser); validate Action, null on mismatch.
//   IBP::Serialize(self+0x68, ser); validate ModelComponent (0x1401c7a50), null on mismatch.
//   IBP::Serialize(self+0x70, ser); validate PickingComponent, null on mismatch.
//   if (loading) {
//       obj = self+0x18 (owner entity backref);
//       self+0x78 = IsDerivedFrom(obj, CharacterDescription/0x1401c70b0) ? obj : null;
//       ibp = InternalBasePtr(self);
//       self+0x40 = CharMgr_sub_83f0(gameWorld+0x88, &ibp);   // re-register char index
//   }
//
// This mirrors CharacterComponent_BodyInit's own field derivation
// (charactercomponent.cpp) — on load retail re-derives +0x78/+0x40 exactly the
// same way Startup does, so we reuse CharMgr_sub_83f0 (already a real C++
// function, charmanager.cpp) rather than re-deriving it.
//
// Sub-callees: all through already-resolved engine_funcs.h globals
// (g_fn_Serializer_Ctor/Dtor/GetVersion is NOT used here — version is fixed 0
// and retail never re-reads it) plus Math::Vector3::Serialize,
// fn_Serializer_Serialize_Float, g_fn_Serialize_Bool, g_fn_Serialize_Int,
// fn_InternalBasePtr_Serialize, fn_IsDerivedFrom, fn_InternalBasePtr_Ctor, and
// the array-resize helper FUN_1400032e0 (already ported/registered elsewhere —
// see animalwave_serialize.cpp's s_Resize8 use of the same address).
// Component_SerializeReimpl is the already-ported Component base (+0x18 default).
//
// Validation type ids resolved locally (self-contained, matching the house
// pattern in animalwave_serialize.cpp): 0x1401c7a50 = ModelComponent,
// g_BaseTypeInfo_PickingComponent's raw slot, TypeInfo_Action's raw slot, and
// 0x1401c70b0 = CharacterDescription. These are the SAME raw .rdata addresses
// already resolved (under different local names) by other files
// (action_pf_ctors.cpp, boatcomponent_route.cpp, animalcomponent.cpp) — we
// resolve our own copies rather than share their statics, per house convention.

#include "resolve.h"
#include "engine_funcs.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

extern void Component_SerializeReimpl(void* self, void* stream);

// FUN_1400032e0 was Collection::Array<void*>::SetSize(n) monomorphised into
// the exe (ENDGAME B.3.2 step 2) — converted at its call site below.

// charmanager.cpp — already a real C++ function (not a resolved pointer).
extern "C" int64_t CharMgr_sub_83f0(float* mgrF, int64_t* ibp);

namespace {

const uint32_t* s_tidModelComponent = nullptr;      // 0x1401c7a50
const uint32_t* s_tidPickingComponent = nullptr;    // g_BaseTypeInfo_PickingComponent slot
const uint32_t* s_tidAction = nullptr;               // TypeInfo_Action slot
const uint32_t* s_tidCharacterDescription = nullptr; // 0x1401c70b0
bool s_resolved = false;

void Resolve() {
    if (s_resolved) return;
    s_tidModelComponent = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a50));
    // Verified against disassembly (the MOV EDX,[addr] immediately preceding each
    // CALL [0x140152a98]/IsDerivedFrom site): Action = 0x1401c5da8 (array loop +
    // +0x58), PickingComponent = 0x1401c7c10 (+0x70).
    s_tidPickingComponent = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7c10));
    s_tidAction = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5da8));
    s_tidCharacterDescription = reinterpret_cast<const uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c70b0));
    s_resolved = true;
}

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

inline bool Loading(void* stream) {
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(stream) + 8) == 0;
}

void SerializeValidatedIBP(void* ibp, void* ser, const uint32_t* tid) {
    if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(ibp, ser);
    void* obj = *reinterpret_cast<void**>(ibp);
    if (obj && fn_IsDerivedFrom && tid && *tid && !fn_IsDerivedFrom(obj, *tid))
        *reinterpret_cast<uint64_t*>(ibp) = 0;
}

} // namespace

extern "C" void CharacterComponent_Serialize(void* self, void* stream) {
    if (!self || !stream) return;
    Resolve();
    Component_SerializeReimpl(self, stream);

    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    // ?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800150e0
    reinterpret_cast<Math::Vector3*>(At(self, 0x2c))->Serialize(ser);
    if (fn_Serializer_Serialize_Float) {
        fn_Serializer_Serialize_Float(ser, reinterpret_cast<float*>(At(self, 0x3c)), nullptr);
        fn_Serializer_Serialize_Float(ser, reinterpret_cast<float*>(At(self, 0x38)), nullptr);
    }
    if (g_fn_Serialize_Bool) g_fn_Serialize_Bool(ser, At(self, 0x28), nullptr);
    if (g_fn_Serialize_Int)  g_fn_Serialize_Int(ser, At(self, 0x60), nullptr);

    int count = *reinterpret_cast<int*>(At(self, 0x50));
    if (g_fn_Serialize_Int) g_fn_Serialize_Int(ser, &count, nullptr);
    // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
    if (Loading(stream)) GameColl::AsArray<GameColl::Ptr8>(At(self, 0x48)).SetSize(count);

    uint8_t* member = *reinterpret_cast<uint8_t**>(At(self, 0x48));
    for (int i = 0; i < count && member; ++i, member += 8)
        SerializeValidatedIBP(member, ser, s_tidAction);

    SerializeValidatedIBP(At(self, 0x58), ser, s_tidAction);
    SerializeValidatedIBP(At(self, 0x68), ser, s_tidModelComponent);
    SerializeValidatedIBP(At(self, 0x70), ser, s_tidPickingComponent);

    if (Loading(stream)) {
        void* owner = *reinterpret_cast<void**>(At(self, 0x18));
        bool isDesc = owner && fn_IsDerivedFrom && s_tidCharacterDescription &&
                     *s_tidCharacterDescription &&
                     fn_IsDerivedFrom(owner, *s_tidCharacterDescription);
        *reinterpret_cast<void**>(At(self, 0x78)) = isDesc ? owner : nullptr;

        if (fn_InternalBasePtr_Ctor) {
            int64_t selfIbp = 0;
            new (&selfIbp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
            void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
            float* charMgr = gw ? reinterpret_cast<float*>(
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gw) + 0x88)) : nullptr;
            if (charMgr) {
                int64_t ibpArg = selfIbp;
                *reinterpret_cast<int32_t*>(At(self, 0x40)) =
                    static_cast<int32_t>(CharMgr_sub_83f0(charMgr, &ibpArg));
            }
        }
    }

    g_fn_Serializer_Dtor(ser);
}
