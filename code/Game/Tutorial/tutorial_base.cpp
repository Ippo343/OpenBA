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

#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
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

// 0x140027de0 — resolve+cache a Toolbar external referenced by name.
// Mirrors ResolveExternalByName_Impl (engine_funcs.h) exactly, validating against
// the Toolbar type instead of BuildComponent.
//   stringField layout: +0x00 String, +0x08 cached generation, +0x10 cached External*.
extern "C" void TutBase_ResolveToolbarRef(void* stringField) {
    if (!stringField) return;
    void* factory = ::Object::ExternalFactory::_singleton;
    if (!factory) return;

    uintptr_t s = reinterpret_cast<uintptr_t>(stringField);

    // Generation check — cached result still valid if generations match.
    int32_t refGen     = *reinterpret_cast<int32_t*>(s + 0x08);
    int32_t factoryGen = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(factory) + 0x08);
    if (refGen == factoryGen) return;

    // +0x00 of the ref triple is the name String (B.3.1 bridge: still raw bytes).
    System::Filename filename(*reinterpret_cast<const System::String*>(s));
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge
    void* found = const_cast<void*>(static_cast<const void*>(efactory->Find(filename)));

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
        const char* name = *reinterpret_cast<const char* const*>(s);
        if (name) {
            char normalized[260];
            size_t k = 0;
            for (; name[k] && k < sizeof(normalized) - 1; ++k)
                normalized[k] = (name[k] == '\\') ? '/' : name[k];
            normalized[k] = 0;

            System::String   normStr(normalized);
            System::Filename normFile(normStr);
            found = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(normFile)));
        }
    }

    // Validate Toolbar type (cell 0x1401c6128; the global "TypeId_ToolbarItem" lies — D5).
    void* result = nullptr;
    if (found) {
        uint32_t tid = Toolbar::TypeId();
        if (!fn_IsDerivedFrom || fn_IsDerivedFrom(found, tid)) {
            result = found;
        }
    }

    *reinterpret_cast<void**>(s + 0x10)  = result;
    *reinterpret_cast<int32_t*>(s + 0x08) = factoryGen;

}

// ============================================================
// Family B — TutorialGoalCount::Check data helpers
// ============================================================

// Shared BST lookup: node[0]=key, node[2]=count, node[4]=left, node[5]=right.
// Faithful port of the original control flow (FUN_140092010 / FUN_1400910a0).
static inline uint32_t BstCountByKey(uintptr_t root, uintptr_t key) {
    uintptr_t* node = reinterpret_cast<uintptr_t*>(root);
    while (true) {
        while (true) {
            if (node == nullptr) return 0;
            if (node[0] <= key) break;       // not greater → leave inner loop
            node = reinterpret_cast<uintptr_t*>(node[4]);  // go left
        }
        if (key <= node[0]) break;           // equal → found
        node = reinterpret_cast<uintptr_t*>(node[5]);      // go right
    }
    if (node == nullptr) return 0;
    return static_cast<uint32_t>(node[2]);
}

// 0x140092010 — count buildings of a type (BST at worldData+0x160).
extern "C" uint32_t TutBase_CountBuildingsByType(uintptr_t worldData, uintptr_t typePtr) {
    return BstCountByKey(*reinterpret_cast<uintptr_t*>(worldData + 0x160), typePtr);
}

// 0x1400910a0 — count professions of a type (BST at worldData+0x100).
extern "C" uint32_t TutBase_CountProfessionsByType(uintptr_t worldData, uintptr_t typePtr) {
    return BstCountByKey(*reinterpret_cast<uintptr_t*>(worldData + 0x100), typePtr);
}

// 0x140013920 — cast an element to SliderElement (IsDerivedFrom check).
// Returns null when the type can't be confirmed — safe for the slider-value
// read path (count stays 0), never feeds a non-slider into Spinner accessors.
extern "C" void* TutBase_CastToSliderElement(void* element) {
    if (!element || !fn_IsDerivedFrom) {
        return nullptr;
    }
    return fn_IsDerivedFrom(element, UI::SpinnerElement::TypeId()) ? element : nullptr;
}

// 0x14012e5c0 — WorkComponent::CalcMaxWorkers. Computes a workplace's worker
// capacity from its WorkDescription (workComp+0x18) and the building footprint.
// Lazily computed and cached at workComp+0x30 by the fbb0 helper below.
//
// This is the ONLY body registered at 0x14012e5c0 (reg_tutorial_base.cpp). It lives in
// the Tutorial file because TutorialGoalCount::Check was the first caller found, but the
// function belongs to WorkComponent and has five other callers: WorkPlaceUI_OnCreate
// (0x140087080), the builder-profession enter/leave pair (0x14012f520 / 0x14012f670),
// HuntComponent::Update (0x140122740) and fbb0 below. It is NOT "terrain speed" — a
// second copy under that name was registered from reg_component_nature.cpp until the
// duplicate was removed.
extern "C" int WorkComponent_CalcMaxWorkers(uintptr_t workComp) {
    uintptr_t obj = *reinterpret_cast<uintptr_t*>(workComp + 0x18);   // WorkDescription

    // Validate WorkDescription; trust on unresolved type-id (matches the
    // original's assumption that +0x18 is always a WorkDescription).
    uintptr_t desc = obj;
    if (obj && fn_IsDerivedFrom) {
        static uint32_t s_wdTid = 0;
        if (s_wdTid == 0) s_wdTid = WorkDescription::TypeId();
        if (s_wdTid != 0) desc = fn_IsDerivedFrom(reinterpret_cast<void*>(obj), s_wdTid) ? obj : 0;
    }
    if (!desc) return 0;   // original would deref null; we no-op instead.

    int baseCap = *reinterpret_cast<int*>(desc + 0x24);

    // Fixed-capacity workplaces (flag at +0x34 == 0): return base capacity.
    if (*reinterpret_cast<char*>(desc + 0x34) == 0) {
        return baseCap;
    }

    // Footprint-scaled: capacity = (footprintArea / divisor) * baseCap, clamped.
    int divisor = *reinterpret_cast<int*>(desc + 0x30);
    if (divisor == 0) return baseCap;   // avoid float div-by-zero UB.

    uint8_t ibp[32] = {};
    typedef void* (*FindComp_t)(uintptr_t entity, void* retIBP);
    FindComp_t findComp = reinterpret_cast<FindComp_t>(ResolveAddr(Module::Exe, 0x140004680));
    if (!findComp) return baseCap;
    findComp(*reinterpret_cast<uintptr_t*>(workComp + 0x10), ibp);
    uintptr_t footprint = *reinterpret_cast<uintptr_t*>(ibp);
    if (!footprint) return baseCap;

    int h = *reinterpret_cast<short*>(footprint + 0x2e);
    int w = *reinterpret_cast<short*>(footprint + 0x2c);
    float uvScale = g_fUVScale ? *g_fUVScale : 0.0f;
    int computed = static_cast<int>(
        (static_cast<float>(h * w) / static_cast<float>(divisor)) * static_cast<float>(baseCap) + uvScale);

    // result = min(computed, baseCap), and if that min < 1, clamp up to 1.
    int result = baseCap;
    if (computed <= result) {
        result = computed;
        if (computed < 1) result = 1;
    }
    return result;
}

// 0x14012fbb0 — WorkPlaceComponent::GetMaxWorkers. Finds the owner entity's
// WorkComponent via FUN_140003050 and returns its cached capacity (+0x30),
// computing it lazily through WorkComponent_CalcMaxWorkers if still 0.
extern "C" int WorkPlaceComponent_GetMaxWorkers(uintptr_t workplaceComp) {

    uint8_t ibp[32] = {};
    typedef void* (*FindComp_t)(uintptr_t entity, void* retIBP);
    FindComp_t findComp = reinterpret_cast<FindComp_t>(ResolveAddr(Module::Exe, 0x140003050));
    if (!findComp) return 0;
    void* self = findComp(*reinterpret_cast<uintptr_t*>(workplaceComp + 0x10), ibp);

    uintptr_t comp = *reinterpret_cast<uintptr_t*>(self);
    if (!comp) return 0;

    if (*reinterpret_cast<int*>(comp + 0x30) == 0) {
        typedef int (*Compute_t)(uintptr_t);
        Compute_t compute = reinterpret_cast<Compute_t>(ResolveAddr(Module::Exe, 0x14012e5c0));
        if (compute) {
            *reinterpret_cast<int*>(comp + 0x30) = compute(comp);
        }
    }
    return *reinterpret_cast<int*>(comp + 0x30);
}
