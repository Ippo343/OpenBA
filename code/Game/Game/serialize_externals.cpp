// code/Game/Game/serialize_externals.cpp
// Single ExternalPtr serialize helpers used by MenuScene::Slot5.
//
// Helpers (M3.5 / notes/M35_recon.md):
//   #9  ExternalPtr serialize (+0x70 GraphicsMesh) — exe 0x14001b8e0  (CASCADE)
//   #10 ExternalPtr serialize (+0x80 Weather)      — exe 0x1400196a0  (CASCADE)
//
// Bodies are byte-identical except for the typeId source:
//   #9  reads BaseTypeInfo* from the .rdata slot at 0x140151f20 (GraphicsMesh,
//       same slot as helper #7), then dereferences *(uint*)BaseTypeInfo for typeId.
//   #10 reads typeId directly from a dword at 0x1401c6198 (Weather — exe-resident
//       BaseTypeInfo whose typeId field is patched in by Weather_RegisterType
//       at runtime).
//
// We extract the shared body into ExternalPtrSerializeGeneric, parameterised
// by the typeIdPtr.

#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "system/Filename.h"
#include "system/String.h"
#include "Object/factory_registration.h"
#include "Component/EnvironmentDescription.h"   // caller batch CB6: TypeId()
#include "Game/Toolbar.h"   // Toolbar::TypeId() (ENDGAME B.3.9 TR)
#include "Game/Weather.h"   // Weather::TypeId() (ENDGAME B.3.9 TR)
#include "graphics/GraphicsMesh.h"   // Graphics::GraphicsMesh::TypeId() (ENDGAME B.3.9 TR)
#include "ui/StringTable.h"   // UI::StringTable::TypeId() (D5, ENDGAME B.3.9 CG3)
// B7 LX (row 992): the type of each further instantiation the StringTable core used to serve.
#include "Component/ComponentDescription.h"   // ComponentDescription::TypeId() -- cell 0x1401c7318
#include "Game/Profession.h"                  // Profession::TypeId() -- cell 0x1401c5a98
#include "ui/SpriteSheet.h"
#include "picking/PickingMesh.h"
#include "audio/SoundEffect.h"
#include "pathfinding/PointList.h"
#include "graphics/Particle.h"
#include "graphics/AnimationGroup.h"
#include "ui/Dialog.h"
#include "ui/ElementDescription.h"
#include "ui/DialogControllerConfig.h"
#include "Game/serialize_externals.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A — Object::External / ExternalFactory
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstdio>
#include "Game/exe_bodies.h"

// Shared body — mirrors FUN_14001b8e0 / FUN_1400196a0 line by line.
// Exposed (external linkage) via serialize_externals.h so the Tutorial
// ExternalPtr-array serialize helpers can reuse this exact body.
//
// Cascade safety: if typeIdPtr is null OR *typeIdPtr is 0 (Runtime DLL hasn't
// registered the underlying type yet), we still call FindOrLoad but skip the
// IsDerivedFrom check. The original would deref a NULL BaseTypeInfo and crash;
// our reimpl gracefully falls through with a one-shot warning.
// B.4 GAME-b: the per-reference body is typed (an Object::External* slot and an
// IO::Serializer&) and shared by every ExternalPtr serializer of this family —
// ExternalPtrSerializeGeneric, the 0x140007140 copy (serialize_externals_typed.cpp)
// and the MaterialInstance array (serialize_struct60b_serialize.cpp); they
// differed only in their log tags. Every caller now reaches the External** body
// directly or through the typed T** wrapper (serialize_externals.h); the untyped
// (void**, void*) overload had no caller left and was deleted (B4 S4 SIG).
void SerializeExternalRefChecked(::Object::External*& ref, IO::Serializer& serializer,
                                 uint32_t typeId, const char* typeName, [[maybe_unused]] const char* tag) {
    if (!::Object::ExternalFactory::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[%s<%s>] runtime primitives unwired — "
                   "skipping serialize (cascade-safe no-op)\n",
                   tag, typeName ? typeName : "?");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    bool typeIdResolved = (typeId != 0);
    // The type name is resolved through the factory's lazy lookup when the type
    // id is not registered yet (strip any namespace prefix: "Graphics::X" -> "X").
    if (!typeIdResolved && typeName) {
        const char* simple = typeName;
        for (const char* p = typeName; *p; ++p) {
            if (p[0] == ':' && p[1] == ':') simple = p + 2;
        }
        typeId = ::Object::ResolveTypeIdByNameLazy(simple);
        typeIdResolved = (typeId != 0);
    }
    if (!typeIdResolved) {
        static bool s_typeWarned = false;
        if (!s_typeWarned) {
            printf("[%s<%s>] typeId not yet registered — "
                   "loading without type validation\n",
                   tag, typeName ? typeName : "?");
            fflush(stdout);
            s_typeWarned = true;
        }
    }
    SerializeExternalRef(ref, serializer, typeId, typeIdResolved);
}

// The body FUN_14001b8e0 / FUN_1400196a0 / FUN_140007140 share, one reference.
void SerializeExternalRef(::Object::External*& ref, IO::Serializer& serializer,
                          uint32_t typeId, bool typeIdResolved) {
    ::Object::ExternalFactory* efactory = ::Object::ExternalFactory::_singleton;

    // Loads one path and keeps it when it derives from the type (or the type is
    // not registered yet).
    auto load = [&](const System::Filename& path) -> ::Object::External* {
        ::Object::External* loaded = efactory->FindOrLoadAs<::Object::External>(path);
        if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId)))
            return loaded;
        return nullptr;
    };

    // Encoding vtable +0x10 is slot 2 == IO::Encoding::GetKind() (B34_RULES §1.1).
    const int encState = serializer.GetEncoding().GetKind();
    if (encState == 3) {
        // Path-export (read-only) mode: walk the existing reference only.
        if (!ref) return;
        System::Filename pathBuf = ref->GetPath();
        const ::Object::External* found = efactory->FindOrLoad(pathBuf);
        if (found && typeIdResolved) {
            ::Object::IsDerivedFromOrTrust(found, typeId);
        }
        return;
    }

    const int direction = static_cast<int>(serializer.GetDirection());   // 0 = loading
    System::Filename fnameBuf;
    if (efactory->IsReferenceModeEnabled()) {
        uint32_t refId = 0xFFFFFFFFu;
        if (ref) refId = efactory->GetReferenceId(ref);
        bool needsLoad = false;
        if (!ref || direction != 0 || refId != 0xFFFFFFFFu) {
            serializer.Serialize(refId, nullptr);
            if (refId != 0xFFFFFFFFu) {
                if (direction == 0) {
                    System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                    fnameBuf = tmpFn;
                }
                needsLoad = true;
            }
        } else {
            serializer.Serialize(refId, nullptr);
        }
        if (needsLoad && static_cast<int>(serializer.GetDirection()) == 0)
            ref = load(fnameBuf);
    } else {
        bool hasRef = (ref != nullptr);
        serializer.Serialize(hasRef, nullptr);
        if (hasRef) {
            if (ref) {
                System::Filename pathBuf = ref->GetPath();
                fnameBuf = pathBuf;
            }
            fnameBuf.AsString().Serialize(serializer);
            if (static_cast<int>(serializer.GetDirection()) == 0)
                ref = load(fnameBuf);
        }
    }
}

void ExternalPtrSerializeGeneric(::Object::External** refField, IO::Serializer* serializer,
                                 const uint32_t* typeIdPtr,
                                 const char* typeName) {
    if (!refField || !serializer) return;
    SerializeExternalRefChecked(*refField, *serializer, typeIdPtr ? *typeIdPtr : 0u, typeName,
                                "ExternalPtr::Serialize");
}

void ExternalPtrSerialize_GraphicsMesh(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Graphics::GraphicsMesh::TypeId(), "Graphics::GraphicsMesh", "ExternalPtr::Serialize");
}

void ExternalPtrSerialize_Weather(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Weather::TypeId(), "Weather", "ExternalPtr::Serialize");
}

// 0x14000bf40 -- ExternalPtr<UI::StringTable>::Serialize (the import ?_typeInfo@StringTable@UI@@,
// slot 0x140151488). B7 LX (row 992): it serves THAT instantiation only; the four other
// retail bodies it used to stand for test other types and have their own cores below.
void ExternalPtrSerialize_StringTable(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, UI::StringTable::TypeId(), "StringTable", "ExternalPtr::Serialize");
}

// ---- B7 LX (row 992): one core per retail instantiation, each testing the ONE type cell
// its retail body reads before `CALL [0x140152a98]` (IsDerivedFrom). The exe cells are read
// as a dword; the Runtime types through their import slot (`MOV RCX,[slot]; MOV EDX,[RCX]`).
// 0x140002340 -- ExternalPtr<ComponentDescription>: MOV EDX,[0x1401c7318].
void ExternalPtrSerialize_ComponentDescription(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, ComponentDescription::TypeId(), "ComponentDescription", "ExternalPtr::Serialize");
}
// 0x140018020 -- ExternalPtr<Profession>: MOV EDX,[0x1401c5a98].
void ExternalPtrSerialize_Profession(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Profession::TypeId(), "Profession", "ExternalPtr::Serialize");
}
// 0x14000bd30 -- ExternalPtr<UI::SpriteSheet>: import slot 0x140151480.
void ExternalPtrSerialize_SpriteSheet(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, UI::SpriteSheet::TypeId(), "SpriteSheet", "ExternalPtr::Serialize");
}
// 0x14000b7b0 -- ExternalPtr<Picking::PickingMesh>: import slot 0x140151468.
void ExternalPtrSerialize_PickingMesh(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Picking::PickingMesh::TypeId(), "PickingMesh", "ExternalPtr::Serialize");
}
// 0x1400019a0 -- ExternalPtr<Audio::SoundEffect>: import slot 0x140152a80 (0x140001c70 is
// its array loop).
void ExternalPtrSerialize_SoundEffect(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Audio::SoundEffect::TypeId(), "SoundEffect", "ExternalPtr::Serialize");
}
// 0x14000ab20 -- ExternalPtr<Pathfinding::PointList>: import slot 0x140151448.
void ExternalPtrSerialize_PointList(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Pathfinding::PointList::TypeId(), "PointList", "ExternalPtr::Serialize");
}
// 0x14000a910 -- ExternalPtr<Graphics::Particle>: import slot 0x140151420.
void ExternalPtrSerialize_Particle(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Graphics::Particle::TypeId(), "Particle", "ExternalPtr::Serialize");
}
// 0x140035b40 -- ExternalPtr<Graphics::AnimationGroup>: import slot 0x140152980.
void ExternalPtrSerialize_AnimationGroup(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Graphics::AnimationGroup::TypeId(), "AnimationGroup", "ExternalPtr::Serialize");
}
// 0x140028650 -- ExternalPtr<UI::Dialog>: import slot 0x140151728.
void ExternalPtrSerialize_Dialog(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, UI::Dialog::TypeId(), "Dialog", "ExternalPtr::Serialize");
}
// 0x140027ad0 -- ExternalPtr<UI::ElementDescription>: import slot 0x140151a50.
void ExternalPtrSerialize_ElementDescription(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, UI::ElementDescription::TypeId(), "ElementDescription", "ExternalPtr::Serialize");
}
// 0x140039840 -- ExternalPtr<UI::DialogControllerConfig>: import slot 0x1401529c0.
void ExternalPtrSerialize_DialogControllerConfig(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, UI::DialogControllerConfig::TypeId(), "DialogControllerConfig", "ExternalPtr::Serialize");
}

void ExternalPtrSerialize_Toolbar(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, Toolbar::TypeId(), "Toolbar", "ExternalPtr::Serialize");
}

void ExternalPtrSerialize_EnvironmentDesc(::Object::External*& ref, IO::Serializer& serializer) {
    SerializeExternalRefChecked(ref, serializer, EnvironmentDescription::TypeId(), "EnvironmentDescription", "ExternalPtr::Serialize");
}
