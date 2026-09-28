// ============================================================================
// runtime/src/object/external_compilers.cpp — ENDGAME B.3.9 OBJ (B39_RULES D6 W3)
//
// The reflection records and factory pairs of `Object::ExtensionCompiler` and
// `Object::ExternalCompiler` (runtime/types/object/ExternalCompiler.h). Their
// minted `?_typeInfo@` records are retired in the same commit, so each name has
// exactly one record in `_firstTypeInfo`. Retail's factory pairs are all four
// `XOR EAX,EAX; RET` (the FACTREST rows they replace had no steps).
// ============================================================================

#include "../../funcs/rt_port.h"
#include "../../types/object/ExternalCompiler.h"

namespace Object {

// ?_typeInfo@ExtensionCompiler@Object@@0VBaseTypeInfo@2@A  0x1801f9ba8
DEFINE_TYPE(Object::ExtensionCompiler, "Base", nullptr)
// ?_typeInfo@ExternalCompiler@Object@@0VBaseTypeInfo@2@A   0x1801f9b70
DEFINE_TYPE(Object::ExternalCompiler, "Base", nullptr)

Base* ExtensionCompiler::CreateNewObject()          { return nullptr; }   // 0x18002c260
Base* ExtensionCompiler::CreateInPlaceObject(void*) { return nullptr; }   // 0x18002c270
Base* ExternalCompiler::CreateNewObject()           { return nullptr; }   // 0x18002c0e0
Base* ExternalCompiler::CreateInPlaceObject(void*)  { return nullptr; }   // 0x18002c0f0

}  // namespace Object
