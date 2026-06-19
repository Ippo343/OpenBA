// code/Game/Game/serialize_externals.h
// Shared declaration of the canonical single-ExternalPtr<T>::Serialize body.
//
// ExternalPtrSerializeGeneric is the faithful, byte-identical port of the engine's
// ExternalPtr<T>::Serialize template (e.g. FUN_14001b8e0 / FUN_1400196a0 /
// FUN_14000bf40 / FUN_140002340 / FUN_140027ad0). It is parameterised by the
// element typeId used for the post-load IsDerivedFrom validation.
//
//   refField   — pointer to the ExternalPtr field (void** storing External*)
//   serializer — IO::Serializer*
//   typeIdPtr  — pointer to the uint32 typeId for validation, or null. When null
//                (or *typeIdPtr == 0) the body resolves the typeId lazily by
//                typeName, and if that also fails it loads WITHOUT type
//                validation (cascade-safe — never crashes on an unresolved type).
//   typeName   — simple/qualified class name for the lazy-by-name fallback.
//
// Defined in serialize_externals.cpp.

#pragma once
#include <cstdint>

void ExternalPtrSerializeGeneric(void** refField, void* serializer,
                                 const uint32_t* typeIdPtr, const char* typeName);
