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
#include <type_traits>
#include "object/External.h"   // Object::External (the typed wrapper's base check)

namespace IO { class Serializer; }

// The one body, on the slot as what it holds (an External*).
void ExternalPtrSerializeGeneric(::Object::External** refField, IO::Serializer* serializer,
                                 const uint32_t* typeIdPtr, const char* typeName);
// The typed wrapper over that one body for an ExternalPtr<T> member held as `T*`
// (T derives from Object::External; the slot holds the External* retail serializes).
template <class T, class = std::enable_if_t<std::is_base_of_v<::Object::External, T> &&
                                            !std::is_same_v<T, ::Object::External>>>
inline void ExternalPtrSerializeGeneric(T** refField, IO::Serializer* serializer,
                                        const uint32_t* typeIdPtr, const char* typeName) {
    ExternalPtrSerializeGeneric(reinterpret_cast<::Object::External**>(refField), serializer,   // the typed wrapper hands its T** slot to the one erased External** core (a T** is not an External**; retail shares the body)  // CAST-OK: elem_stride
                                typeIdPtr, typeName);
}
// B.4 GAME-b: the typed per-reference body (serialize_externals.cpp). The Checked
// form adds the factory / type-id preamble and its one-shot warnings (tagged).
void SerializeExternalRef(Object::External*& ref, IO::Serializer& serializer,
                          uint32_t typeId, bool typeIdResolved);
void SerializeExternalRefChecked(Object::External*& ref, IO::Serializer& serializer,
                                 uint32_t typeId, const char* typeName, const char* tag);
