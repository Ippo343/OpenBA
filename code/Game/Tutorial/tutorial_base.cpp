// code/Game/Tutorial/tutorial_base.cpp
// Clean reimplementations of the 10 exe-internal helper functions the Tutorial
// module calls via ResolveAddr(Module::Exe, ...) but which were unregistered in
// standalone (the Tutorial ResolveAddr gap). Registering these (standalone/
// reg_tutorial_base.cpp) lets the tutorial reimpls fire real behavior instead of
// silently skipping the guarded call.
//
// Two families:
//   A. ExternalPtr<T> serialize helpers (26fb0 single-style + 4 array variants +
//      27de0 cached-resolve). The single-element body is the canonical
//      ExternalPtr<T>::Serialize — identical to FUN_140002340 / FUN_14000bf40 /
//      FUN_14001b8e0 — so we reuse the proven ExternalPtrSerializeGeneric
//      (code/Game/Game/serialize_externals.cpp). The array helpers loop it.
//   B. TutorialGoalCount::Check data helpers (BST counts, work-capacity, slider cast).
//
// Closure-safety: family A's entire sub-callee closure is Runtime exports
// (FindOrLoad/Filename/Serializer/String — ResolveExport, no registration) +
// engine_funcs. Family B's only exe sub-callees are FUN_140003050 (registered,
// Work wave) and FUN_140004680 (registered, component-nature wave) plus the
// internal compute FUN_14012e5c0 (registered here). All verified in Ghidra.
//
// Decompiled from Application-x64.exe; ABI/control-flow verified instruction-level.

#include "ui/UserInterface.h"
#include "Game/exe_constants.h"   // kRdata_* (B3R Leftovers: the .rdata constants)
#include "system/Filename.h"
#include "system/String.h"
#include "object/TypeInfo.h"
#include "Component/exe_bodies.h"   // 0x140004680 FloorComponent_FindResData
#include "Work/exe_bodies.h"        // 0x140003050 WorkBase_FindComponent_3050
#include "Object/factory_registration.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "Game/serialize_externals.h"
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include "Component/WorkDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Game/Toolbar.h"                // B.3.9 EXA: Toolbar::TypeId()
#include "ui/SpinnerElement.h"           // UI::SpinnerElement::TypeId() (D5, ENDGAME B.3.9 CG3)
#include "Tutorial/exe_bodies.h"
#include "Goal/goal_exe_helpers.h"           // NamedExternalRef, the entity-bucket trees
#include "Entity/CitizenBehavior.h"
#include "Entity/ComponentEntity.h"
#include "Component/ComponentDescription.h"
#include "Component/MapComponent.h"
#include "Component/WorkComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Game/Profession.h"

// ============================================================
// Family A — ExternalPtr serialize helpers
// ============================================================

// Per-element ExternalPtr<T>::Serialize via the canonical generic body.
// typeId resolves lazily by name (cascade-safe: loads without type validation
// if the name is not yet registered — never crashes).
static inline void SerOne(void* field, void* serializer, const char* typeName) {
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(field), serializer,
                                /*typeIdPtr*/ nullptr, typeName);
}

// Array-of-ExternalPtr<T>::Serialize (8-byte stride). param_4 is an unused
// scratch register in the original; ignored here.
//   0x140026fb0  Tutorial steps      (TutorialStep)
//   0x140028cf0  TutorialStep images (TutorialImage)
//   0x140028f40  TutorialStep setups (TutorialSetup)
//   0x140029190  TutorialStep goals  (TutorialGoal)
static void SerArray(void* serializer, int count, void* arrayData, const char* typeName) {
    if (!arrayData || count <= 0) return;
    uint8_t* base = reinterpret_cast<uint8_t*>(arrayData);
    for (int i = 0; i < count; ++i) {
        SerOne(base + static_cast<size_t>(i) * 8, serializer, typeName);
    }
}

extern "C" void TutBase_SerializeStepArray(void* serializer, int count, void* arrayData, void*) {
    SerArray(serializer, count, arrayData, "TutorialStep");
}
extern "C" void TutBase_SerializeImageArray(void* serializer, int count, void* arrayData, void*) {
    SerArray(serializer, count, arrayData, "TutorialImage");
}
extern "C" void TutBase_SerializeSetupArray(void* serializer, int count, void* arrayData, void*) {
    SerArray(serializer, count, arrayData, "TutorialSetup");
}
extern "C" void TutBase_SerializeGoalArray(void* serializer, int count, void* arrayData, void*) {
    SerArray(serializer, count, arrayData, "TutorialGoal");
}

// 0x140027ad0 — single ExternalPtr serialize for a TutorialImage's image ref.
// The original validates against UI::ElementDescription (_typeInfo slot 0x140151a50).
extern "C" void TutBase_SerializeImageRef(void* fieldPtr, void* serializer) {
    SerOne(fieldPtr, serializer, "ElementDescription");
}

/// 0x140027de0 — ExternalPtr<Toolbar>'s resolve by name: resolve + cache a
// Toolbar external referenced by name. Mirrors ResolveExternalByName_Impl
// exactly, validating against the Toolbar type instead.
void NamedExternalRef::ResolveToolbar() {
    ::Object::ExternalFactory* efactory = ::Object::ExternalFactory::_singleton;
    if (!efactory) return;

    // Generation check — cached result still valid if generations match.
    int32_t refGen     = generation;
    int32_t factoryGen = efactory->_generation;
    if (refGen == factoryGen) return;

    System::Filename filename(Name());
    const ::Object::External* found = efactory->Find(filename);

    // STANDALONE DEVIATION, deliberate — retail uses Find() alone and would crash
    // on the null below, so this is not papering over a faithful fault: it is the
    // documented standalone resource-environment gap.
    //
    // Retail reaches here with "Game\\Toolbar.rsc:<item>" already in the
    // ExternalFactory, because UpdateLoop::LoadResources has preloaded the
    // "Game/GameResources.rsc" dependency tree into it. In standalone that preload
    // populates the loop's m_resourceList but does NOT leave these entries in the
    // factory (see standalone/gameupdateloop_stubs.cpp on the non-terminating
    // recursive load), so Find() misses and TutorialImageToolbarIcon::Slot9 built a
    // Sprite of {sheet = null, index = 0} — which reads as VALID to
    // UI::ImageElement::ComputeSize and faults it on *(sheet + 0x20).
    //
    // Measured: Find("Game\\Toolbar.rsc:options") -> null,
    //           Find("Game/Toolbar.rsc:options")  -> null,
    //           FindOrLoad("Game/Toolbar.rsc:options") -> object.
    // The separator matters only to the LOADER: once loaded, Find() hits with the
    // backslash spelling too, so the factory's key comparison normalizes and only
    // the file-open path does not. Hence the fallback normalizes to '/'.
    //
    // The real fix is to make the preload leave these in the factory as retail's
    // does; until then this loads on demand, which is what retail's environment
    // has already done by this point.
    if (!found) {
        const char* text = Name()._data;
        if (text) {
            char normalized[260];
            size_t k = 0;
            for (; text[k] && k < sizeof(normalized) - 1; ++k)
                normalized[k] = (text[k] == '\\') ? '/' : text[k];
            normalized[k] = 0;

            System::String   normStr(normalized);
            System::Filename normFile(normStr);
            found = efactory->FindOrLoad(normFile);
        }
    }

    // Validate Toolbar type (cell 0x1401c6128; the global "TypeId_ToolbarItem" lies — D5).
    const ::Object::External* result = nullptr;
    if (found) {
        uint32_t tid = Toolbar::TypeId();
        if (Object::IsDerivedFromNullSafe(found, tid)) {
            result = found;
        }
    }

    resolved   = result;
    generation = factoryGen;
}

// ============================================================
// Family B — TutorialGoalCount::Check data helpers
// ============================================================

// Shared BST lookup over the CitizenBehavior entity-bucket trees: the bucket's
// entity count, 0 when absent. Faithful port of the original control flow
// (FUN_140092010 / FUN_1400910a0).
template <class Node, class Key>
static inline uint32_t BstCountByKey(const Node* node, const Key* key) {
    while (true) {
        while (true) {
            if (node == nullptr) return 0;
            if (node->key <= key) break;     // not greater → leave inner loop
            node = node->left;               // go left
        }
        if (key <= node->key) break;         // equal → found
        node = node->right;                  // go right
    }
    if (node == nullptr) return 0;
    return static_cast<uint32_t>(node->count);
}

// 0x140092010 — the entities of one template (the +0x160 tree).
uint32_t CitizenBehavior::CountByTemplate(const ComponentDescription* key) const {
    return BstCountByKey(TemplateTreeRoot(this), key);
}

// 0x1400910a0 — the entities of one profession (the +0x100 tree).
uint32_t CitizenBehavior::CountByProfession(const Profession* key) const {
    return BstCountByKey(ProfessionTreeRoot(this), key);
}

// The tree's guards around 0x14012e5c0 that retail lacks (known_port_issues
// 14d row 80): a scaled description with a zero _maxArea, or an entity with no
// MapComponent, answers _defaultWorkers where retail divides by zero /
// dereferences null. Everything else is WorkComponent::CalcMaxWorkers, retail's
// body (Component/workcomponent.cpp).
static int CalcMaxWorkersGuarded(const WorkComponent* work) {
    const Description* base = work->_description;
    const WorkDescription* jobType = Object::IsDerivedFromNullSafe(base, WorkDescription::TypeId())
        ? static_cast<const WorkDescription*>(base) : nullptr;
    if (jobType && jobType->_b34) {
        if (jobType->_i30 == 0) return jobType->_i24;
        if (work->_entity->FindComponent<MapComponent>() == nullptr) return jobType->_i24;
    }
    return work->CalcMaxWorkers();   // 0x14012e5c0
}

// 0x14012e5c0 — WorkComponent::CalcMaxWorkers. This was a SECOND port of it
// (B4 TUT compared both with retail 2026-10-02: the same function; this copy
// only adds the two guards above). The body is the member's; the C name stays
// for the callers in other blocks' files.
// B4 FORWARD: 21 outside callers (Component/citizencomponent.cpp,
// destroycomponent_workerrelease.cpp, huntcomponent.cpp, workplacecomponent.cpp;
// Game/town_workplace_registry.cpp; Work/buildstructure.cpp, cleararea.cpp,
// foresterwork.cpp, harvestarea.cpp, harvestorchard.cpp, herbalistwork.cpp,
// huntwork.cpp, marketwork.cpp, pasturework.cpp, plantarea.cpp,
// plantorchard.cpp, tendarea.cpp, tradework.cpp).
extern "C" int WorkComponent_CalcMaxWorkers(uintptr_t workComp) {
    return CalcMaxWorkersGuarded(reinterpret_cast<const WorkComponent*>(workComp));   // B4 FORWARD: the old signature
}

// 0x14012fbb0 — WorkPlaceComponent::GetMaxWorkers. Finds the owner entity's
// WorkComponent (FindComponent<WorkComponent>, 0x140003050) and returns its
// cached capacity (+0x30), computing it lazily if still 0. The null-component
// return is the tree's (retail reads through it; 14d row 81).
int WorkPlaceComponent::GetMaxWorkers() const {
    WorkComponent* comp = _entity->FindComponent<WorkComponent>();
    if (!comp) return 0;

    if (comp->workState == 0) {
        comp->workState = CalcMaxWorkersGuarded(comp);   // 0x14012e5c0
    }
    return comp->workState;
}
