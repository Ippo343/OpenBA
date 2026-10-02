// code/Game/Game/serialize_externals_typed.cpp
//
// M3.5 Path B session 3 — additional typed-ExternalPtr serialize helper.
//
// Helper:
//   FUN_140007140 — typed ExternalPtr serialize (CASCADE, ~120 instr)
//
// Body is byte-identical to FUN_14001b8e0 (helper #9, GraphicsMesh — see
// code/Game/Game/serialize_externals.cpp) except for the typeInfo source
// slot. FUN_140007140 reads BaseTypeInfo* from .rdata slot 0x140151350,
// which is Graphics::MaterialInstance (confirmed via shared xrefs:
// CharacterComponent_impl_08c0 FindOrLoads "Terrain/Selection.rsc" and
// then calls Graphics::MaterialInstance::Setup on the result, validating
// against the same slot).
//
// The body is shared with code/Game/Game/serialize_externals.cpp (B.4 GAME-b).

#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "graphics/Material.h"   // ENDGAME B.3.9 TI-REST: Graphics::MaterialInstance::TypeId()
#include "system/Filename.h"
#include "system/String.h"
#include "Object/factory_registration.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstdio>
#include "Game/exe_bodies.h"
#include "Game/serialize_externals.h"

namespace {
// B.4 GAME-b: the body is the shared typed SerializeExternalRef (serialize_externals.cpp);
// this copy differed only in its log tag. The C entry points' two LEFT casts:
// the slot and the serializer arrive as void* from callers holding their own types.
void ForwardRef7140(void* refField, void* serializer, uint32_t typeId, const char* typeName) {
    if (!refField || !serializer) return;
    SerializeExternalRefChecked(*reinterpret_cast<::Object::External**>(refField),
                                *reinterpret_cast<IO::Serializer*>(serializer),
                                typeId, typeName, "ExternalPtr::Serialize_7140");
}
} // namespace

extern "C" void Reimpl_ExternalPtr_Serialize_Typed_7140(
    void* refField, void* serializer) {
    ForwardRef7140(refField, serializer, Graphics::MaterialInstance::GetTypeInfo().typeId,
                   "Graphics::MaterialInstance");
}

extern "C" void Reimpl_ExternalPtr_Serialize_MapGenerator(void* refField, void* serializer) {
    ForwardRef7140(refField, serializer, 0u, "MapGenerator");
}
extern "C" void Reimpl_ExternalPtr_Serialize_TerrainGenerator(void* refField, void* serializer) {
    ForwardRef7140(refField, serializer, 0u, "TerrainGenerator");
}
extern "C" void Reimpl_ExternalPtr_Serialize_ClimateGenerator(void* refField, void* serializer) {
    ForwardRef7140(refField, serializer, 0u, "ClimateGenerator");
}
