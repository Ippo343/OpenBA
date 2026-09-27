// code/Game/Game/founding_spawn.cpp
// Retail founding-colonist spawn — the port of CitizenComponent_impl_0dd0_sub_1040
// (0x1400a1040) and the per-family arithmetic of CitizenComponent_impl_0dd0
// (0x1400a0dd0), which together create the start-town population.
//
// This REPLACED the hand-rolled SpawnOneCitizen in starttown.cpp (open H of
// notes/NEXT_SESSION_BRIEF.md, section 4), which reproduced most of FUN_140117080's
// effects piecemeal (gender, mesh variant, spawn transform, name, colour, age,
// lifespan) and burned a second RNG stream. Graduated unconditional 2026-09-13 and
// the hand-rolled path deleted; this is now the only founding-colonist spawner.
//
// ---------------------------------------------------------------------------
// Retail sub_1040 (0x1400a1040), signature
//   (startConfig, InternalBasePtr* outCitizenComp, float* basePos, float age,
//    char genderSel, char educated)
// body, in order:
//   1. MemAlloc(0x48, 8); copy the ExternalRef triple at startConfig+0x48
//      ({String@+0x48, int@+0x50, External*@+0x58}) to a local, FUN_14001b1d0 on the
//      copy (re-resolve), ComponentEntity_CopyCtor(mem, resolvedTemplate).
//   2. wrap the entity in an IBP, Entity_FindComponent_Citizen -> out param, CheckPtr.
//   3. desc = *(comp+0x18); IsDerivedFrom(desc, DWORD_1401c7190 = CitizenDescription).
//   4. if (age < desc+0x44)  educated = 0.       // children are never educated
//   5. pos = { RandFloatTol()*jitter + base.x, RandFloatTol()*jitter + base.y, base.z }
//      with jitter = *(float*)(startConfig+0x40).  NB the draw ORDER: the FIRST draw
//      scales Y and the SECOND scales X (the decompile evaluates the Vector3 args
//      right-to-left). Preserved below so the RNG stream matches vanilla.
//   6. laborerProf = *(*(gw+0xb0) + 0x30)         // ProfessionList -> Laborer
//   7. FUN_140117080(comp, &pos, gender = (genderSel == '\0'), isChild = age < desc+0x44,
//                    age /*FLOAT*/, educated, &nullIBP, laborerProf)
//   8. FUN_14001b1d0(startConfig+0x48) again, then
//      FUN_1400c48a0(*(gw+0xe8), &entityIBP, *(int*)(template+0x20))   // EntityList add
//
// Retail impl_0dd0 (0x1400a0dd0) around it:
//   * scans the citizen template's sub-description array (+0x28 / count +0x30) for the
//     CitizenDescription — that is where the age ranges live;
//   * per family: adult A with genderSel='\0', adult B with '\x01', both aged
//       RandFloat() * (desc+0x54 * DAT_140172114 - desc+0x50) + desc+0x50
//     (a FRESH draw each), spouse cross-link +0x250 both ways, then Rand(maxKids+1)
//     children aged RandFloat() * desc+0x44, educated='\0', genderSel alternating from
//     TRUE and persisting ACROSS families, each joined to adult B via helper_68a0.
//   DAT_140172114 = 0.5f (read from the exe at 0x140172114: 00 00 00 3f), the same
//   constant starttown.cpp already names kCenterBias.
// The family loop itself stays in starttown.cpp (it owns the famEntities /
// g_foundingCitizens bookkeeping the founding-housing, laborer-seed and town-registration
// passes consume); this file exports the per-colonist spawn and the two age draws so
// every RNG draw and every description read stays in one place.
//
// ---------------------------------------------------------------------------
// Deviations from retail, all forced by standalone and all logged:
//   a) Entity_FindComponent_Citizen (0x140009790) is NOT registered, and the exe
//      type-id global it reads (DAT_1401c7158) is null on a default run. The finder is
//      reimplemented inline (identical body: scan entity+0x30 array of entity+0x38
//      IBPs, match vtable+0x10 GetTypeId) with the type id taken from the exe global
//      when present and ::Object::GetTypeIdByName("CitizenComponent") otherwise — the
//      same fallback the in-game birth path uses (citizencomponent.cpp).
//   b) IsDerivedFrom against DWORD_1401c7190 fails in standalone (the description's
//      type-id isn't recognised), which would leave retail's `desc` NULL and fault on
//      desc+0x44. Established derive-or-TRUST pattern: keep the check, but fall back to
//      the linked description at comp+0x18 when it fails.
//   c) FUN_14001b1d0 is not registered either; the equivalent is the inline
//      ResolveExternalByName_Impl. It is find-only and caches its result, so a re-resolve
//      that missed would null the template starttown.cpp loaded via FindOrLoad. The
//      caller therefore passes the already-resolved template as a fallback and we restore
//      the cache if the re-resolve comes back empty.
//   d) A garbage jitter radius (NaN / negative / absurd) is clamped to 0, same guard
//      SetCitizenSpawnTransform already applied. The field is read raw from the .rsc.

#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "resolve.h"
#include "Object/factory_registration.h"
#include "object/TypeInfo.h"                 // caller batch CB6: Object::Base::Type()
#include "Component/CitizenComponent.h"     // caller batch CB6: TypeId()
#include "Component/CitizenDescription.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "object/External.h"   // ENDGAME B.3.9 SS2: Object::ExternalFactory::_singleton

// Start-config offsets (mirrors of the constants in starttown.cpp).
static constexpr int SC_JITTER       = 0x40;   // float spawn-jitter radius
static constexpr int SC_CITIZEN_REF  = 0x48;   // ExternalRef {String, int gen, External*}
static constexpr int SC_CITIZEN_TMPL = 0x58;   // = SC_CITIZEN_REF + 0x10 (cached template)
static constexpr int TEMPLATE_CATEGORY = 0x20; // *(template+0x20) = entity-list category
static constexpr int DESC_SUB_ARR    = 0x28;   // description/template sub-block array
static constexpr int DESC_SUB_CNT    = 0x30;   // ... and its count
static constexpr int ENT_COMP_ARR    = 0x30;   // entity component-IBP array
static constexpr int ENT_COMP_CNT    = 0x38;   // ... and its count
static constexpr int ENTITY_SIZE     = 0x48;   // ComponentEntity allocation

static constexpr float kAdultAgeBias = 0.5f;   // DAT_140172114

// CitizenComp_DeferredInit_117080 (0x140117080) — the whole per-citizen init. Called
// through ResolveAddr (project style) rather than directly, and with the age argument
// correctly typed as FLOAT: it was long declared `int` and only worked because every
// existing caller passed literal 0 (same bit pattern as 0.0f). See open H.
typedef void (*fn_DeferredInit_117080_t)(int64_t self, void* pos, uint32_t gender,
                                         char isChild, float age, char educated,
                                         int64_t* parentIBP, int64_t laborerProf);

static int  s_trace = 0;
static bool TraceOn() { return s_trace < 24; }   // first-N

// ---------------------------------------------------------------------------
// Type ids — read directly (D5, ENDGAME B.3.9 CG3): cells 0x1401c7158 and
// 0x1401c7190 through the classes' own X::TypeId(), never the engine_funcs
// g_TypeId_* globals (which read the SAME cells).
// ---------------------------------------------------------------------------
static uint32_t TidCitizenComponent() {
    return CitizenComponent::TypeId();
}
static uint32_t TidCitizenDescription() {
    return CitizenDescription::TypeId();
}

// Entity_FindComponent_Citizen (0x140009790), reimplemented — see deviation (a).
// Writes the found component into `outIBP` through the IBP ctor, exactly as retail's
// finder does (its callers then CheckPtr the slot).
static void FoundingSpawn_FindCitizenComp(void* entity, uintptr_t* outIBP) {
    void* found = nullptr;
    uint32_t want = TidCitizenComponent();
    if (entity && want != 0) {
        uint8_t* e = reinterpret_cast<uint8_t*>(entity);
        int count = *reinterpret_cast<int32_t*>(e + ENT_COMP_CNT);
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + ENT_COMP_ARR);
        for (int i = 0; i < count && arr; ++i) {
            void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
            void* comp = *slot;
            if (!comp) continue;
            // Slot 2 on an entity component-array element (proved Object::Base).
            if (reinterpret_cast<const Object::Base*>(comp)->Type() == want) {   // B.3.8 bridge
                found = comp;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) new (outIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    else                         *outIBP = reinterpret_cast<uintptr_t>(found);
}

// The head of impl_0dd0: find the CitizenDescription in the citizen template's
// sub-description array (raw pointers, no IBPs; match on vtable+0x10 GetTypeId, then
// IsDerivedFrom-validate). Result cached per template — retail re-scans on each
// impl_0dd0 call, but the scan draws no RNG and reads nothing mutable.
extern "C" void* StartTown_FoundingCitizenDescription(void* citizenTemplate) {
    static void* s_tmpl = nullptr;
    static void* s_desc = nullptr;
    static bool  s_cached = false;
    if (s_cached && citizenTemplate == s_tmpl) return s_desc;   // includes the null case
    void* found = nullptr;
    uint32_t want = TidCitizenDescription();
    if (citizenTemplate && want != 0) {
        uint8_t* t = reinterpret_cast<uint8_t*>(citizenTemplate);
        int count = *reinterpret_cast<int32_t*>(t + DESC_SUB_CNT);
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(t + DESC_SUB_ARR);
        for (int i = 0; i < count && arr; ++i) {
            void* desc = *reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
            if (!desc) continue;
            // Slot 2 on a template description-array element (proved Object::Base).
            if (reinterpret_cast<const Object::Base*>(desc)->Type() == want) {   // B.3.8 bridge
                // Retail keeps the hit only if IsDerivedFrom agrees; standalone trusts
                // the GetTypeId match when the derive check can't run (deviation (b)).
                found = desc;
                break;
            }
        }
    }
    s_tmpl = citizenTemplate;
    s_desc = found;
    s_cached = true;
    if (found && s_trace < 24) {
        ++s_trace;
        const uint8_t* d = reinterpret_cast<const uint8_t*>(found);
        fprintf(stderr, "[FoundingSpawn] CitizenDescription=%p tid=%u adultAge(+0x44)=%.2f "
                "ageMin(+0x50)=%.2f ageMax(+0x54)=%.2f -> adult range [%.2f, %.2f)\n",
                found, want, *reinterpret_cast<const float*>(d + 0x44),
                *reinterpret_cast<const float*>(d + 0x50),
                *reinterpret_cast<const float*>(d + 0x54),
                *reinterpret_cast<const float*>(d + 0x50),
                *reinterpret_cast<const float*>(d + 0x54) * kAdultAgeBias);
        fflush(stderr);
    } else if (!found) {
        fprintf(stderr, "[FoundingSpawn] NO CitizenDescription on template=%p (tid=%u) — "
                "ages will fall back to 0\n", citizenTemplate, want);
        fflush(stderr);
    }
    return found;
}

static void* FoundingSpawn_Rng() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return nullptr;
    return reinterpret_cast<GameUpdateLoop*>(*g_pGameWorldSingleton)->m_random;   // B.3.8 bridge
}

// ---------------------------------------------------------------------------
// The two age draws of impl_0dd0. Each consumes exactly one RandFloat, in the same
// place in the stream retail consumes it (immediately before the sub_1040 call it
// feeds), so the caller must not hoist them.
// ---------------------------------------------------------------------------
extern "C" float StartTown_FoundingAdultAge(void* citizenTemplate) {
    void* desc = StartTown_FoundingCitizenDescription(citizenTemplate);
    void* rng  = FoundingSpawn_Rng();
    if (!desc || !rng) return 0.0f;
    const uint8_t* d = reinterpret_cast<const uint8_t*>(desc);
    float ageMin = *reinterpret_cast<const float*>(d + 0x50);
    float ageMax = *reinterpret_cast<const float*>(d + 0x54);
    float r = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
    // FAITHFUL, including the odd shape: the upper bound is HALF the description's max
    // age, so the range is [ageMin, ageMax*0.5). No clamp to the adult threshold —
    // the hand-rolled path added one; retail has none, and if this ever produces an
    // "adult" below desc+0x44 that IS what vanilla does with these .rsc numbers.
    return r * (ageMax * kAdultAgeBias - ageMin) + ageMin;
}

extern "C" float StartTown_FoundingChildAge(void* citizenTemplate) {
    void* desc = StartTown_FoundingCitizenDescription(citizenTemplate);
    void* rng  = FoundingSpawn_Rng();
    if (!desc || !rng) return 0.0f;
    float adultAge = *reinterpret_cast<const float*>(
        reinterpret_cast<const uint8_t*>(desc) + 0x44);
    return reinterpret_cast<System::Random*>(rng)->RandFloat() * adultAge;   // uniform [0, adult threshold)  B.3.4b bridge
}

// Retail's two FUN_14001b1d0 calls (steps 1 and 8) — see deviation (c). Returns the
// template pointer the CopyCtor source and the entity-list category are both read from.
static void* FoundingSpawn_RefreshTemplate(uint8_t* startConfig, void* fallback) {
    if (!startConfig) return fallback;
    if (::Object::ExternalFactory::_singleton)
        ResolveExternalByName_Impl(startConfig + SC_CITIZEN_REF);
    void** cache = reinterpret_cast<void**>(startConfig + SC_CITIZEN_TMPL);
    if (*cache == nullptr && fallback != nullptr) {
        *cache = fallback;                       // restore what FindOrLoad had loaded
        if (s_trace < 24) { ++s_trace;
            fprintf(stderr, "[FoundingSpawn] re-resolve of the citizen ExternalRef missed; "
                    "restored the FindOrLoad template %p\n", fallback);
            fflush(stderr);
        }
    }
    return *cache ? *cache : fallback;
}

// ---------------------------------------------------------------------------
// CitizenComponent_impl_0dd0_sub_1040 (0x1400a1040)
//
// `genderSel` is retail's param_5: '\0' -> gender 1, '\x01' -> gender 0.
// `educated`  is retail's param_6 (forced to 0 for children by step 4).
// `entityList` is the caller's copy of *(gw+0xe8), which is what retail reads.
// Returns the new entity (raw pointer); *outCitizenComp receives the CheckPtr'd
// CitizenComponent the family links are written through.
// ---------------------------------------------------------------------------
extern "C" void* StartTown_SpawnColonistRetail(uint8_t* startConfig, void* citizenTemplate,
                                               void* entityList, const float base[3],
                                               float age, char genderSel, char educated,
                                               uint8_t** outCitizenComp) {
    if (outCitizenComp) *outCitizenComp = nullptr;
    if (!fn_MemAlloc || !fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor) {
        fprintf(stderr, "[FoundingSpawn] BAIL: entity primitives unresolved\n");
        fflush(stderr);
        return nullptr;
    }

    void* rng = FoundingSpawn_Rng();

    // 1. allocate + copy-construct the entity from the (re-resolved) citizen template
    void* tmpl = FoundingSpawn_RefreshTemplate(startConfig, citizenTemplate);
    void* mem  = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
    void* ent  = (mem && tmpl) ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
    if (!ent) {
        fprintf(stderr, "[FoundingSpawn] BAIL: CopyCtor produced no entity "
                "(mem=%p tmpl=%p)\n", mem, tmpl);
        fflush(stderr);
        return nullptr;
    }

    uintptr_t entIBP = 0;
    new (&entIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(ent));   // B.3.3 bridge

    // 2. find the CitizenComponent
    uintptr_t compIBP = 0;
    FoundingSpawn_FindCitizenComp(reinterpret_cast<void*>(entIBP), &compIBP);
    uint8_t* comp = reinterpret_cast<uint8_t*>(compIBP);
    if (!comp) {
        // Never true in retail (the entity is a copy of the citizen template, so it
        // always carries the component). Registering a citizen entity with no
        // CitizenComponent would seed the town list with something no consumer can
        // read, so drop the whole colonist instead.
        fprintf(stderr, "[FoundingSpawn] ABORT: entity %p has no CitizenComponent "
                "(tid=%u) — colonist dropped\n", ent, TidCitizenComponent());
        fflush(stderr);
        return nullptr;
    }

    // 3. the linked CitizenDescription (derive-or-trust, deviation (b))
    void* desc = *reinterpret_cast<void**>(comp + 0x18);
    if (desc && fn_IsDerivedFrom) {
        uint32_t tid = TidCitizenDescription();
        if (tid != 0 && !fn_IsDerivedFrom(desc, tid)) {
            // Retail takes NULL here and then reads desc+0x44 off it — a fault. Keep the
            // linked description and say so once (deviation (b)).
            static bool s_said = false;
            if (!s_said) { s_said = true;
                fprintf(stderr, "[FoundingSpawn] IsDerivedFrom(desc=%p, CitizenDescription=%u) "
                        "false — trusting comp+0x18 (standalone derive gap)\n", desc, tid);
                fflush(stderr);
            }
        }
    }
    const float adultAge = desc ? *reinterpret_cast<float*>(
                                      reinterpret_cast<uint8_t*>(desc) + 0x44)
                                : 0.0f;
    const bool isChild = (age < adultAge);

    // 4. children are never educated
    if (isChild) educated = '\0';

    // 5. spawn position — jittered around the town centre. Draw order preserved:
    //    first RandFloatTol scales Y, second scales X.
    float jitter = *reinterpret_cast<float*>(startConfig + SC_JITTER);
    if (!(jitter == jitter) || jitter < 0.0f || jitter > 1.0e4f) jitter = 0.0f;  // deviation (d)
    float jy = (rng ? fn_Random_RandFloatTol(rng) : 0.0f) * jitter;
    float jx = (rng ? fn_Random_RandFloatTol(rng) : 0.0f) * jitter;
    float pos[3] = { base[0] + jx, base[1] + jy, base[2] };

    // 6. the Laborer profession key (ProfessionList at gw+0xb0, +0x30)
    int64_t laborerProf = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        int64_t profList = reinterpret_cast<int64_t>(reinterpret_cast<GameUpdateLoop*>(*g_pGameWorldSingleton)->m_professionList);   // B.3.8 bridge
        if (profList) laborerProf = *reinterpret_cast<int64_t*>(profList + 0x30);
    }

    // 7. the whole per-citizen init
    int64_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    static fn_DeferredInit_117080_t s_deferredInit = nullptr;
    static bool s_resolved = false;
    if (!s_resolved) {
        s_resolved = true;
        s_deferredInit = reinterpret_cast<fn_DeferredInit_117080_t>(
            ResolveAddr(Module::Exe, 0x140117080));
    }
    if (s_deferredInit) {
        s_deferredInit(static_cast<int64_t>(compIBP), pos,
                       static_cast<uint32_t>(genderSel == '\0'),
                       isChild ? '\x01' : '\0', age, educated, &nullIBP, laborerProf);
    } else {
        fprintf(stderr, "[FoundingSpawn] WARN: 0x140117080 unresolved — colonist gets no "
                "name/age/gender/transform\n");
        fflush(stderr);
    }

    // 8. register with the world entity list under the template's category
    tmpl = FoundingSpawn_RefreshTemplate(startConfig, tmpl);
    int category = tmpl ? *reinterpret_cast<int32_t*>(
                              reinterpret_cast<uint8_t*>(tmpl) + TEMPLATE_CATEGORY)
                        : 0;
    uintptr_t regIBP = entIBP;
    fn_EntityList_RegisterEntity(entityList, &regIBP, category);

    if (outCitizenComp) *outCitizenComp = comp;

    if (TraceOn()) {
        ++s_trace;
        fprintf(stderr, "[FoundingSpawn] colonist ent=%p comp=%p gender=%d %s age=%.2f "
                "(adultAge=%.2f) educated=%d pos=(%.1f,%.1f,%.1f) jitter=%.2f "
                "laborerProf=0x%llx category=%d\n",
                ent, comp, (int)(genderSel == '\0'), isChild ? "child" : "adult", age,
                adultAge, (int)educated, pos[0], pos[1], pos[2], jitter,
                (unsigned long long)laborerProf, category);
        fflush(stderr);
    }
    return ent;
}
