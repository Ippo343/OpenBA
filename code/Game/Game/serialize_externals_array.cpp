// code/Game/Game/serialize_externals_array.cpp
// FUN_1400049c0 — Object::ExternalPtr<ComponentDescription>[] array serializer.
//
// Faithful, offset-for-offset port of the exe array-serializer at 0x1400049c0.
// It loops `count` times over an 8-byte-stride array of External* (ExternalPtr
// fields) rooted at `arrayBase`, serializing each element IN PLACE. The
// per-element body is byte-identical to the engine's single ExternalPtr<T>::
// Serialize template (see ExternalPtrSerializeGeneric in serialize_externals.cpp,
// which is the proven line-by-line port of FUN_140002340 et al.):
//
//   pEVar5 = Serializer::GetEncoding(s);  iVar3 = (*(vtbl+0x10))(pEVar5);
//   if (iVar3 == 3)   -> path-export (read-only) mode:
//        self = arr[i]; if (self) { p = External_GetPath(self);
//                                    o = FindOrLoad(*singleton, p);
//                                    if (o) IsDerivedFrom(o, DWORD_1401c7318); }
//   else              -> ctor Filename; refMode ? (refId path) : (bool + path);
//        on load (*(*serializer+8)==0) FindOrLoad(path) -> IsDerivedFrom(c7318)
//        -> store resolved External* back into arr[i] (*plVar1).
//
// The 4th exe arg (param_4) is a dead init: it only seeds the HIGH 32 bits of the
// refId scratch (local_res20) once before the loop; every use reads only the LOW
// 32 (the uint refId), so it is inert. Caller passes nullptr. We keep the 4-arg
// signature and ignore it — ExternalPtrSerializeGeneric uses a fresh local refId.
//
// Validation type = "ComponentDescription" (DWORD_1401c7318, base Object::External;
// written by ComponentDescription_Register @0x14014ea60). The engine_funcs global
// g_TypeId_ComponentDescription_FC points at that BaseTypeInfo; *g_TypeId_ComponentDescription_FC is
// the typeId dword the disasm reads (MOV EDX,[0x1401c7318]).
//
// Standalone impact: this exe addr was NULL, so Create{Removal,Bridge,Fenced}
// Description and animaldescription Serialize bailed and never deserialized their
// ExternalPtr arrays. Registering it (standalone/address_registry.cpp) restores
// those reads. Reuses the exact resolved helpers of the single-element sibling —
// no new resolution mechanism.

#include "engine_funcs.h"
#include "resolve.h"
#include "Game/serialize_externals.h"
#include "Component/ComponentDescription.h"
#include <cstdint>

// FUN_1400049c0(Serializer* s, int count, longlong arrayBase, void* deadInit)
extern "C" void Reimpl_ExternalPtrArray_Serialize(
    void* serializer, int count, long long arrayBase, void* /*deadInit*/) {


    if (count <= 0 || !arrayBase || !serializer) return;

    // Validation typeId = ComponentDescription (cell 0x1401c7318), read directly
    // through the class's own TypeId() (D5, ENDGAME B.3.9 CG3) rather than the
    // engine_funcs g_TypeId_ComponentDescription_FC global that reads the same cell.
    const uint32_t tid = ComponentDescription::TypeId();
    const uint32_t* typeIdPtr = tid ? &tid : nullptr;

    void** base = reinterpret_cast<void**>(arrayBase);
    for (int i = 0; i < count; ++i) {
        // &arr[i] — 8-byte stride, matching (param_3 + i*8) in the disasm.
        ExternalPtrSerializeGeneric(base + i, serializer, typeIdPtr,
                                    "ComponentDescription");
    }
}
