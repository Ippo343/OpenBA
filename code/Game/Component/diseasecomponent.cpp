// code/Game/Component/diseasecomponent.cpp
// DiseaseComponent — CROP / ORCHARD INFESTATION and LIVESTOCK DISEASE.
//
// ⚠ This is NOT citizen sickness. Citizen illness is the `Disease` *resource*
// (Object::External, loaded from Game/Disease/Diseases.rsc) driven by
// CitizenComponent::UpdateHealth — see code/Game/Component/citizen_updatehealth.cpp.
// DiseaseComponent is a per-BUILDING component attached to crop fields, orchards
// and pastures. It is retail Banished's crop-infestation disaster: a field becomes
// infested, its plants die one at a time yielding nothing, the infestation spreads
// to nearby fields growing the SAME crop, and the player's response is to harvest /
// clear the field (or change the crop) — which cures it and, because the crop
// changed, makes the next outbreak less likely.
//
// The exe contains no "Infest"/"Blight"/"Pest" string; the mechanic's own retail
// names are the DebugUI elements `cropDiseaseButton` (element id 0x14A) and
// `noCropDiseaseCheck` (id 0x150 -> GameWorld+0xD0 bit 5).
//
// ---------------------------------------------------------------------------
// Ported bodies (all previously MISSING in standalone)
// ---------------------------------------------------------------------------
//   0x1400481b0  CheckCure       — cured when no plants remain / herd empty
//   0x140048320  Cure            — stop VFX, zero timers, record the crop identity
//   0x1400483a0  Infect          — event-log post, start VFX, arm the timers
//   0x140048610  InfectionTick   — seasonal roll (the noCropDisease gate lives here)
//   0x140048840  ActiveDisease   — kill tick + spread tick
//   0x140048b90  Update          — vtable slot 9 dispatch thunk
//   0x140048c50  Startup         — slot 4: isPasture = entity has a PastureComponent
//   0x140048c00  OnAdded         — slot 6: append to the town registry
//   0x140048bb0  OnRemoved       — slot 7: erase from the registry, then Cure
//   0x140048ca0  Serialize       — slot 3
//   0x14009fb60  SameCropProbe   — "is the cured crop still planted on my tiles?"
//   0x140047f80  GrownIdentity   — what a field/orchard/pasture is currently growing
//   0x140047b40  KillOnePlant    — zero one plant's yield + switch to the dead model
//   0x140047d70  KillOneAnimal   — destroy one pasture animal
//   0x1400537f0  ParticleStopFade— fade the infestation VFX out (sign-flipped rate)
//
// ⚠ Ghidra's symbol names for this class are shifted by one vtable slot. Slots were
// identified by READING THE VTABLE BYTES at 0x140159290 (see engine/types/DiseaseComponent.h):
//   Ghidra "_Shutdown"(0x140048ca0) is slot 3 Serialize; "_Serialize"(0x140048c50) is
//   slot 4 Startup; "_Draw"(0x140048c00) is slot 6 OnAdded; "_OnCreate"(0x140048bb0)
//   is slot 7 OnRemoved. The call targets in OnAdded/OnRemoved were confirmed by
//   DISASSEMBLY (0x140003110 ResizeFill / 0x140002cc0 EraseByValue) — the decompiler's
//   `DynArray8_EraseByValue` label also appears on a different address elsewhere.
//
// ---------------------------------------------------------------------------
// GameWorld / map layout used here (all disassembly-verified)
// ---------------------------------------------------------------------------
//   gw+0xD0  debug-flag word: bit0 zeroCost 1 noHunger 2 noCold 3 noHealth
//            4 noSick  **5 noCropDisease**  6 noFire
//   gw+0xD8  System::Random     gw+0xE8 entity manager (+0xD0 destroy queue, +0xD8 count)
//   gw+0xF0  town   (+0xF0 DiseaseComponent registry DynArray: data@+0 count@+8 cap@+0xC)
//   gw+0xF8  tile map   gw+0x120 game-speed obj   gw+0x128 clock   gw+0x138 event log
//   map+0x00 width (int32)      map+0x80 cell array, stride 0x30
//   cell+0x00 zone raster   cell+0x08 entity raster   cell+0x10 grown-resource identity
//   MapComponent +0x28 x  +0x2a y  +0x2c w  +0x2e h   (all int16)
//
// rdata constants (read_memory-verified): 0x1401721b8=12.0f months/yr,
// 0x140172234=300.0f secs/month, 0x14017225c=3600.0f secs/yr, 0x14017214c=1.0f,
// 0x1401722c0=0x80000000 float sign mask.
//
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ⚠ ORGANIC INFESTATION IS UNOBSERVABLE IN A SESSION-LENGTH RUN — BY DESIGN
// ---------------------------------------------------------------------------
// The authored means (CropField.rsc / Orchard.rsc / Pasture.rsc, the only three
// templates carrying a `DiseaseDescription`) are 150 years (same crop) and 200
// years (crop rotated). With rollMonths = 0.25 that is 48 rolls a year, each at
// chance = 1/(48 * years) ~= 1.4e-4. A `[CropDisease] ROLL ... -> no` line
// forever is therefore CORRECT BEHAVIOUR, not a dead roll — do not "fix" it.
// Measure this lane with the forced trigger (BANISHED_FORCE_INFEST) instead.
// Once infested it is very visible: duration 8 months (pasture 12), a kill every
// 0.03 months and a spread attempt every 0.125.
//
// Opt-in.  BANISHED_LIVE_CROPDISEASE=1 graduates vtable slots 3/4/6/7/9 to the
// real bodies. Unproven feature => NOT in kDefaultOnFlags. With the gate off the
// component stays byte-for-byte the inert component it was before this wave.
// Trace: BANISHED_CROPDISEASE_TRACE (defaults ON when the feature is live, =0 mutes).

#include "Component/diseasecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// ============================================================
// Resolved leaves — every one of these is already ported+registered elsewhere
// ============================================================
typedef void* (*fn_Find_t)(int64_t entity, void* outIBP);
typedef void  (*fn_FindV_t)(void* entity, void* outIBP);
typedef void* (*fn_FindDesc_t)(uint64_t descPtr, void* outSlot);
typedef void  (*fn_GetTilesInArea_t)(void* map, int x, int y, int w, int h,
                                     int64_t* outList, char flag);
typedef void  (*fn_MarkRemoval_t)(void* entity);
typedef void  (*fn_ArrResizeFill_t)(int64_t* arr, int newCount, void* ibp);
typedef void  (*fn_ArrErase_t)(int64_t* arr, void* ibp);
typedef void  (*fn_ComponentSerialize_t)(void* self, void* stream);
typedef void  (*fn_ExternalPtrSerialize_t)(void* field, void* serializer);
typedef void  (*fn_SetAnimIndex_t)(int64_t natRes, uint32_t index);

static fn_Find_t                s_findPasture   = nullptr;  // 0x140004fe0
static fn_Find_t                s_findMap       = nullptr;  // 0x140004680
static fn_FindV_t               s_findParticle  = nullptr;  // 0x140005200
static fn_Find_t                s_findNatRes    = nullptr;  // 0x1400052c0
static fn_Find_t                s_findAnimal    = nullptr;  // 0x140005500
static fn_Find_t                s_findSMC       = nullptr;  // 0x140003ea0
static fn_FindDesc_t            s_findDesc      = nullptr;  // 0x140005160
static fn_GetTilesInArea_t      s_getTiles      = nullptr;  // 0x14009fc00
static fn_MarkRemoval_t         s_markRemoval   = nullptr;  // 0x1400c2ea0
static fn_ArrResizeFill_t       s_arrResizeFill = nullptr;  // 0x140003110
static fn_ArrErase_t            s_arrErase      = nullptr;  // 0x140002cc0
static fn_ComponentSerialize_t  s_compSerialize = nullptr;  // 0x1401189b0
static fn_ExternalPtrSerialize_t s_extPtrSerialize = nullptr; // 0x140002340
static fn_SetAnimIndex_t        s_setAnimIndex  = nullptr;  // 0x140126440

// Serializer .rdata dispatch slots (same set DiseaseComponent::Serialize calls).
typedef void (*fn_SerCtor_t)(void* self, void* stream, int mode);   // [0x140152ad8]
typedef void (*fn_SerInt_t)(void* self, void* value, const char* n);// [0x140152ac8]
typedef void (*fn_SerTime_t)(void* time, void* serializer);         // [0x140152aa8]
typedef int  (*fn_SerGetVer_t)(void* self);                         // [0x140152a48]
typedef void (*fn_SerDtor_t)(void* self);                           // [0x140152ad0]
static fn_SerCtor_t   s_serCtor   = nullptr;
static fn_SerInt_t    s_serInt    = nullptr;
static fn_SerTime_t   s_serTime   = nullptr;
static fn_SerGetVer_t s_serGetVer = nullptr;
static fn_SerDtor_t   s_serDtor   = nullptr;

// Event-log leaves exported from code/Game/Game/eventlog.cpp.
extern "C" void EventLog_ArrayGrow_Reg(void* arrBase, int newCount, const void* newEntry);
extern "C" void EventLog_YearString_Reg(void* gameWorld, void* retStr);
extern "C" void EventLog_SeasonString_Reg(void* gameWorld, void* eventLog, void* retStr);
extern "C" int  EventLog_TileIndex_Reg(void* unused, void* entityIBP);

// Particle VFX (code/Game/Component/particlecomponent.cpp, 0x140053660).
extern "C" void ParticleComponent_StartEmitterFade(void* self, int index);

// rdata constants.
static const float* s_pMonthsPerYear = nullptr;  // 0x1401721b8 = 12.0f
static const float* s_pSecsPerMonth  = nullptr;  // 0x140172234 = 300.0f
static const float* s_pSecsPerYear   = nullptr;  // 0x14017225c = 3600.0f
static const float* s_pOne           = nullptr;  // 0x14017214c = 1.0f (g_fUVScale)

static uint32_t s_tidDiseaseComponent = 0;

static bool s_init = false;
static int  s_live = -1;
static int  s_trace = -1;

static inline void* RSlot(uintptr_t slotAddr) {
    uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<void**>(slot) : nullptr;
}
template <typename T> static inline T R(uintptr_t a) {
    return reinterpret_cast<T>(ResolveAddr(Module::Exe, a));
}

static void EnsureInit() {
    if (s_init) return;
    s_init = true;

    s_findPasture   = R<fn_Find_t>(0x140004fe0);
    s_findMap       = R<fn_Find_t>(0x140004680);
    s_findParticle  = R<fn_FindV_t>(0x140005200);
    s_findNatRes    = R<fn_Find_t>(0x1400052c0);
    s_findAnimal    = R<fn_Find_t>(0x140005500);
    s_findSMC       = R<fn_Find_t>(0x140003ea0);
    s_findDesc      = R<fn_FindDesc_t>(0x140005160);
    s_getTiles      = R<fn_GetTilesInArea_t>(0x14009fc00);
    s_markRemoval   = R<fn_MarkRemoval_t>(0x1400c2ea0);
    s_arrResizeFill = R<fn_ArrResizeFill_t>(0x140003110);
    s_arrErase      = R<fn_ArrErase_t>(0x140002cc0);
    s_compSerialize = R<fn_ComponentSerialize_t>(0x1401189b0);
    s_extPtrSerialize = R<fn_ExternalPtrSerialize_t>(0x140002340);
    s_setAnimIndex  = R<fn_SetAnimIndex_t>(0x140126440);

    s_serCtor   = reinterpret_cast<fn_SerCtor_t>(RSlot(0x140152ad8));
    s_serInt    = reinterpret_cast<fn_SerInt_t>(RSlot(0x140152ac8));
    s_serTime   = reinterpret_cast<fn_SerTime_t>(RSlot(0x140152aa8));
    s_serGetVer = reinterpret_cast<fn_SerGetVer_t>(RSlot(0x140152a48));
    s_serDtor   = reinterpret_cast<fn_SerDtor_t>(RSlot(0x140152ad0));

    s_pMonthsPerYear = R<const float*>(0x1401721b8);
    s_pSecsPerMonth  = R<const float*>(0x140172234);
    s_pSecsPerYear   = R<const float*>(0x14017225c);
    s_pOne           = R<const float*>(0x14017214c);

    s_tidDiseaseComponent = ::Object::GetTypeIdByName("DiseaseComponent");
}

// An unresolved .rdata constant silently deletes the test it guards
// (reference_unresolved_rdata_const_deletes_guarded_test), so every read has the
// verified retail literal as its fallback and the boot trace prints what it got.
static inline float MonthsPerYear() { return s_pMonthsPerYear ? *s_pMonthsPerYear : 12.0f; }
static inline float SecsPerMonth()  { return s_pSecsPerMonth  ? *s_pSecsPerMonth  : 300.0f; }
static inline float SecsPerYear()   { return s_pSecsPerYear   ? *s_pSecsPerYear   : 3600.0f; }
static inline float OneF()          { return s_pOne           ? *s_pOne           : 1.0f; }

static inline void* GameWorld() {
    return g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
}
static inline int64_t TimeFromSeconds(float secs) {
    int64_t t = 0;                              // Time ctor writes 8 BYTES
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, secs);
    return t;
}
static inline void CheckPtr(void* p) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
}

bool CDLive() {
    if (s_live < 0) {
        const char* e = std::getenv("BANISHED_LIVE_CROPDISEASE");
        s_live = (e && *e && e[0] != '0' && e[0] != 'f' && e[0] != 'F') ? 1 : 0;
    }
    return s_live == 1;
}
bool CDTrace() {
    if (s_trace < 0) {
        const char* e = std::getenv("BANISHED_CROPDISEASE_TRACE");
        // Default ON while the feature is live: the events are rare (a roll per
        // field per desc.rollMonths, a kill per killMonths) so volume stays low.
        s_trace = e ? ((*e && e[0] != '0') ? 1 : 0) : (CDLive() ? 1 : 0);
    }
    return s_trace == 1;
}

// Roll traces are the only per-field-per-period line; budget them so a large town
// cannot flood the log. Outcome lines (infect/kill/spread/cure) are never budgeted.
static int s_rollBudget = 0;
static const int kRollBudget = 400;

} // namespace

// ============================================================
// 0x14009fb60 — SameCropProbe: is the cured crop still on my tiles?
// ============================================================
// Byte-exact from the disassembly: map width is the int32 at map+0x00 and the
// 0x30-byte cell array base is the pointer at map+0x80 (Ghidra renders these as
// `param_1[0]` / `param_1 + 0x20` on an `int*` — ELEMENTS, not bytes). The compared
// field is cell+0x10, the grown-resource identity raster (cell+0x00 is the zone
// raster and cell+0x08 the entity raster — a different thing entirely).
// Outer loop walks x across `w`, inner loop steps whole rows down `h`.
extern "C" bool DiseaseComp_SameCropProbe(void* map, int x, int y, int w, int h,
                                          int64_t wantIdentity) {
    const uint8_t* m = reinterpret_cast<const uint8_t*>(map);
    int32_t width = *reinterpret_cast<const int32_t*>(m + 0x00);
    uintptr_t cells = *reinterpret_cast<const uintptr_t*>(m + 0x80);

    for (int64_t dx = 0; dx < w; ++dx) {
        uintptr_t cell = cells + 0x10 +
            (uintptr_t)(((int64_t)width * y) + dx + x) * 0x30;
        for (int64_t dy = 0; dy < h; ++dy) {
            if (*reinterpret_cast<const int64_t*>(cell) == wantIdentity) return true;
            cell += (uintptr_t)width * 0x30;
        }
    }
    return false;
}

// ============================================================
// 0x140047f80 — GrownIdentity: what is this entity currently growing?
// ============================================================
// Pasture  -> PastureComponent+0x28 (the livestock kind).
// Field/orchard -> the first tile entity carrying a NaturalResourceComponent,
//                  whose +0x28 is the resource identity.
// Empty footprint -> the CropFieldComponent's +0x50, else the OrchardComponent's
//                  +0x60 (the crop the field is *set* to, before anything sprouts).
// Returns through an out-slot: `entityIBPSlot` points at a slot holding the entity.
extern "C" void* DiseaseComp_GrownIdentity(uint64_t* out, void* entityIBPSlot) {
    EnsureInit();
    int64_t entity = *reinterpret_cast<int64_t*>(entityIBPSlot);
    if (entity == 0) { *out = 0; return out; }

    CheckPtr(entityIBPSlot);
    int64_t pastureIBP = 0;
    if (s_findPasture) s_findPasture(entity, &pastureIBP);
    if (pastureIBP != 0) {
        CheckPtr(&pastureIBP);
        *out = *reinterpret_cast<uint64_t*>(pastureIBP + 0x28);
        return out;
    }

    void* gw = GameWorld();
    void* map = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xF8) : nullptr;

    CheckPtr(entityIBPSlot);
    int64_t mapIBP = 0;
    if (s_findMap) s_findMap(entity, &mapIBP);
    CheckPtr(&mapIBP);
    const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

    struct { int64_t data; int32_t count; int32_t cap; } list = {0, 0, 0};
    if (s_getTiles && map && mc) {
        s_getTiles(map, *(const int16_t*)(mc + 0x28), *(const int16_t*)(mc + 0x2a),
                        *(const int16_t*)(mc + 0x2c), *(const int16_t*)(mc + 0x2e),
                        reinterpret_cast<int64_t*>(&list), 0);
    }
    const int64_t listData  = list.data;
    const int32_t listCount = list.count;

    if (listCount > 0) {
        for (int i = 0; i < listCount; ++i) {
            int64_t* slot = reinterpret_cast<int64_t*>(listData + (int64_t)i * 0x10 + 8);
            CheckPtr(slot);
            int64_t natResIBP = 0;
            if (s_findNatRes) s_findNatRes(*slot, &natResIBP);
            if (natResIBP != 0) {
                CheckPtr(slot);
                *out = *reinterpret_cast<uint64_t*>(*slot + 0x28);
                if (fn_MemFreeStatic) fn_MemFreeStatic((void*)listData);
                return out;
            }
        }
    } else {
        // Nothing sprouted: fall back to what the field/orchard is SET to grow.
        CheckPtr(entityIBPSlot);
        int64_t cropIBP = 0;
        {
            typedef void* (*fnf)(int64_t, void*);
            fnf findCrop = R<fnf>(0x140005380);   // CropFieldComponent finder
            if (findCrop) findCrop(entity, &cropIBP);
        }
        if (cropIBP != 0) {
            CheckPtr(&cropIBP);
            *out = *reinterpret_cast<uint64_t*>(cropIBP + 0x50);
            if (fn_MemFreeStatic) fn_MemFreeStatic((void*)listData);
            return out;
        }
        CheckPtr(entityIBPSlot);
        int64_t orchardIBP = 0;
        {
            typedef void* (*fnf)(int64_t, void*);
            fnf findOrchard = R<fnf>(0x140005440);  // OrchardComponent finder
            if (findOrchard) findOrchard(entity, &orchardIBP);
        }
        if (orchardIBP != 0) {
            CheckPtr(&orchardIBP);
            *out = *reinterpret_cast<uint64_t*>(orchardIBP + 0x60);
            if (fn_MemFreeStatic) fn_MemFreeStatic((void*)listData);
            return out;
        }
    }

    if (fn_MemFreeStatic) fn_MemFreeStatic((void*)listData);
    *out = 0;
    return out;
}

// ============================================================
// 0x1400537f0 — ParticleComponent: fade the emitters OUT
// ============================================================
// Sibling of ParticleComponent_StartEmitterFade (0x140053660). Writes the same
// per-emitter fade-rate field (geoArray + i*0x18 + 0x10) but XORs the float's BIT
// PATTERN with 0x80000000 (the .rdata sign mask at 0x1401722c0) to negate it —
// a *bit* operation, not a numeric cast, whatever the decompiler's `(uint)` says.
extern "C" void ParticleComponent_StopEmitterFade(void* self) {
    EnsureInit();
    uintptr_t s = (uintptr_t)self;
    void* desc = *reinterpret_cast<void**>(s + 0x18);
    int count = *reinterpret_cast<int32_t*>(s + 0x40);
    uintptr_t geoArr = *reinterpret_cast<uintptr_t*>(s + 0x38);
    if (!desc || !geoArr) return;
    uintptr_t elems = *reinterpret_cast<uintptr_t*>((uintptr_t)desc + 0x28);
    if (!elems) return;

    for (int i = 0; i < count; ++i) {
        float t = fn_Time_AsSeconds
            ? fn_Time_AsSeconds(reinterpret_cast<void*>(elems + ((int64_t)i * 5 + 4) * 8))
            : 1.0f;
        float rate = OneF() / t;
        uint32_t bits;
        __builtin_memcpy(&bits, &rate, 4);
        bits ^= 0x80000000u;                       // sign flip => fade OUT
        *reinterpret_cast<uint32_t*>(geoArr + (uintptr_t)i * 0x18 + 0x10) = bits;
    }
}

// ============================================================
// 0x1400050a0 — DiseaseComponent sibling finder
// ============================================================
// The only exe caller is the spread block below. No registered equivalent existed.
extern "C" void* DiseaseComp_FindSibling(int64_t entity, void* outIBP) {
    EnsureInit();
    void* found = nullptr;
    uint32_t want = s_tidDiseaseComponent;
    int count = *reinterpret_cast<int32_t*>(entity + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        CheckPtr(slot);
        void* comp = *slot;
        uint32_t id = 0;
        if (comp) {
            uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
            typedef uint32_t (*fn_GetTypeId_t)(void*);
            fn_GetTypeId_t g = *reinterpret_cast<fn_GetTypeId_t*>(vt + 0x10);
            if (g) id = g(comp);
        }
        if (id == want) {
            void** ms = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
            CheckPtr(ms);
            if (*ms && fn_IsDerivedFrom && fn_IsDerivedFrom(*ms, want)) found = *ms;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outIBP, found);
    return outIBP;
}

// ============================================================
// 0x1400481b0 — CheckCure
// ============================================================
extern "C" bool DiseaseComp_CheckCure(DiseaseComponent* self) {
    EnsureInit();
    void* entitySlot = &self->entity;

    if (self->isPasture == 1) {
        CheckPtr(entitySlot);
        int64_t pastureIBP = 0;
        if (s_findPasture) s_findPasture((int64_t)self->entity, &pastureIBP);
        CheckPtr(&pastureIBP);
        // Faithful: retail derefs unguarded — isPasture==1 means the finder
        // already located a PastureComponent at Startup.
        if (*reinterpret_cast<int32_t*>(pastureIBP + 0x38) != 0) return false;
        return true;
    }

    void* gw = GameWorld();
    void* map = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xF8) : nullptr;

    CheckPtr(entitySlot);
    int64_t mapIBP = 0;
    if (s_findMap) s_findMap((int64_t)self->entity, &mapIBP);
    CheckPtr(&mapIBP);
    const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

    struct { int64_t data; int32_t count; int32_t cap; } list = {0, 0, 0};
    if (s_getTiles && map && mc) {
        s_getTiles(map, *(const int16_t*)(mc + 0x28), *(const int16_t*)(mc + 0x2a),
                        *(const int16_t*)(mc + 0x2c), *(const int16_t*)(mc + 0x2e),
                        reinterpret_cast<int64_t*>(&list), 0);
    }

    int living = 0;
    for (int i = 0; i < list.count; ++i) {
        int64_t* slot = reinterpret_cast<int64_t*>(list.data + (int64_t)i * 0x10 + 8);
        CheckPtr(slot);
        int64_t natResIBP = 0;
        if (s_findNatRes) s_findNatRes(*slot, &natResIBP);
        if (natResIBP != 0) ++living;
    }
    if (fn_MemFreeStatic) fn_MemFreeStatic((void*)list.data);
    return living == 0;
}

// ============================================================
// 0x140048320 — Cure
// ============================================================
extern "C" void DiseaseComp_Cure(DiseaseComponent* self) {
    EnsureInit();
    CheckPtr(&self->entity);
    int64_t particleIBP = 0;
    if (s_findParticle) s_findParticle(self->entity, &particleIBP);
    CheckPtr(&particleIBP);
    if (particleIBP) ParticleComponent_StopEmitterFade((void*)particleIBP);

    bool wasInfested = (self->duration != 0);

    self->duration    = 0;
    self->killAccum   = 0;
    self->spreadAccum = 0;
    self->rollAccum   = 0;

    // Record what is growing NOW. The next infection roll compares the field's
    // tiles against this: unchanged => the (shorter) yearsIfSame mean applies.
    int64_t entitySlot = (int64_t)self->entity;
    uint64_t identity = 0;
    DiseaseComp_GrownIdentity(&identity, &entitySlot);
    self->curedIdentity = identity;

    if (wasInfested && CDTrace()) {
        printf("[CropDisease] CURE self=%p pasture=%d newIdentity=%p\n",
               (void*)self, (int)self->isPasture, (void*)identity);
        fflush(stdout);
    }
}

// ============================================================
// 0x1400483a0 — Infect
// ============================================================
extern "C" void DiseaseComp_Infect(DiseaseComponent* self) {
    EnsureInit();
    if (DiseaseComp_CheckCure(self)) {
        if (CDTrace()) {
            printf("[CropDisease] INFECT-SKIPPED self=%p reason=already-cureable "
                   "(no plants / empty herd)\n", (void*)self);
            fflush(stdout);
        }
        return;
    }

    CheckPtr(&self->entity);
    int64_t smcIBP = 0;
    if (s_findSMC) s_findSMC((int64_t)self->entity, &smcIBP);
    CheckPtr(&smcIBP);
    // State gate: the field must be past stage 1. +0x52 is the override, used
    // unless negative, in which case +0x50 (the live state) is read.
    int8_t stage = *reinterpret_cast<int8_t*>(smcIBP + 0x52);
    if (stage < 0) stage = *reinterpret_cast<int8_t*>(smcIBP + 0x50);
    if (stage <= 1) {
        if (CDTrace()) {
            printf("[CropDisease] INFECT-SKIPPED self=%p reason=stage=%d (<=1)\n",
                   (void*)self, (int)stage);
            fflush(stdout);
        }
        return;
    }

    void* gw = GameWorld();

    // ---- Event-log post: type 0xC severity 3, colour 0xFFFF6060, icon 2, no popup.
    // A third entry-append variant next to AddEvent/AddCitizenEvent: three format
    // args ({0} season {1} year {2} crop name) and hardcoded icon/popup.
    uint8_t cropName[8] = {0};
    bool haveName = false;
    {
        uint64_t entityDesc = self->entity
            ? *reinterpret_cast<uint64_t*>((uintptr_t)self->entity + 0x28) : 0;
        int64_t found = 0;
        if (s_findDesc && entityDesc) s_findDesc(entityDesc, &found);
        if (found && fn_UIString_Ctor && fn_UIString_ToSystemString) {
            uint8_t uiStr[16] = {0};
            fn_UIString_Ctor(uiStr, *reinterpret_cast<uint64_t*>(found + 0x30),
                             reinterpret_cast<const void*>(found + 0x40));
            fn_UIString_ToSystemString(uiStr, cropName);
            haveName = true;
        }
    }

    void* eventLog = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0x138) : nullptr;
    if (eventLog && gw && fn_String_Ctor_Copy && fn_String_Dtor) {
        uint8_t yearStr[8] = {0}, seasonStr[8] = {0};
        uint8_t templateStr[8] = {0}, formatted[8] = {0};
        EventLog_YearString_Reg(gw, yearStr);
        EventLog_SeasonString_Reg(gw, eventLog, seasonStr);

        void* eventsTable = *reinterpret_cast<void**>(eventLog);
        if (eventsTable && fn_StringTable_GetText)
            fn_StringTable_GetText(eventsTable, templateStr, 0xC);
        else if (fn_String_Ctor_CStr)
            fn_String_Ctor_CStr(templateStr, "Crop infestation");

        if (fn_String_FormatIndexed) {
            fn_String_FormatIndexed(formatted, *(const char**)templateStr,
                                    *(const char**)seasonStr, *(const char**)yearStr,
                                    haveName ? *(const char**)cropName : "");
        } else {
            fn_String_Ctor_Copy(formatted, templateStr);
        }

        void* entityIBP = &self->entity;
        int tileIdx = EventLog_TileIndex_Reg(nullptr, entityIBP);

        uint8_t entry[0x28] = {0};
        *(uint32_t*)(entry + 0x00) = 0xFFFF6060u;   // severity-3 red
        fn_String_Ctor_Copy(entry + 0x08, formatted);
        *(int32_t*)(entry + 0x10) = 0xC;            // eventType: crop infestation
        *(int32_t*)(entry + 0x14) = 3;              // severity
        *(int32_t*)(entry + 0x18) = tileIdx;
        *(int32_t*)(entry + 0x1c) = 2;              // iconIndex (hardcoded here)
        *(int32_t*)(entry + 0x20) = -1;             // no popup

        int count = *reinterpret_cast<int32_t*>((uintptr_t)eventLog + 0x20);
        EventLog_ArrayGrow_Reg((void*)((uintptr_t)eventLog + 0x18), count + 1, entry);

        fn_String_Dtor(entry + 0x08);
        fn_String_Dtor(formatted);
        fn_String_Dtor(templateStr);
        fn_String_Dtor(seasonStr);
        fn_String_Dtor(yearStr);
    }

    // Drop to 1x speed if the "pause on important events" setting is on.
    if (g_pGameSettingsPtr && g_pGameSettingsPtr[0x17] != 0 && gw) {
        void* speedObj = *reinterpret_cast<void**>((uintptr_t)gw + 0x120);
        if (speedObj) *reinterpret_cast<int32_t*>((uintptr_t)speedObj + 0x34) = 1;
    }
    if (haveName && fn_String_Dtor) fn_String_Dtor(cropName);

    // ---- Start every emitter on the entity's ParticleComponent (the swarm VFX).
    CheckPtr(&self->entity);
    int64_t particleIBP = 0;
    if (s_findParticle) s_findParticle(self->entity, &particleIBP);
    CheckPtr(&particleIBP);
    int emitters = 0;
    if (particleIBP) {
        emitters = *reinterpret_cast<int32_t*>(particleIBP + 0x40);
        for (int i = 0; i < emitters; ++i)
            ParticleComponent_StartEmitterFade((void*)particleIBP, i);
    }

    // ---- Arm the timers.
    DiseaseDescription* desc = reinterpret_cast<DiseaseDescription*>(self->description);
    self->duration    = TimeFromSeconds(desc->durationMonths * SecsPerMonth());
    self->killAccum   = 0;
    self->spreadAccum = 0;

    if (CDTrace()) {
        printf("[CropDisease] INFECT self=%p pasture=%d stage=%d durationMonths=%.2f "
               "emitters=%d tile=%d\n",
               (void*)self, (int)self->isPasture, (int)stage,
               (double)desc->durationMonths, emitters,
               EventLog_TileIndex_Reg(nullptr, &self->entity));
        fflush(stdout);
    }
}

// ============================================================
// 0x140047b40 — KillOnePlant  (Ghidra: AnimateNaturalResource_impl_7b40)
// ============================================================
// Up to 50 random tries over the field footprint for a NaturalResourceComponent
// that is not already dead (flag bit 0x100000). The kill zeroes +0x28 — the yield
// identity, so the plant produces NOTHING — clears +0x2d, sets the dead bit
// 0x200000 and switches the model to animation index 2 (withered).
extern "C" void DiseaseComp_KillOnePlant(DiseaseComponent* self) {
    EnsureInit();
    void* gw = GameWorld();
    void* map = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xF8) : nullptr;
    void* rng = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xD8) : nullptr;

    CheckPtr(&self->entity);
    int64_t mapIBP = 0;
    if (s_findMap) s_findMap((int64_t)self->entity, &mapIBP);
    CheckPtr(&mapIBP);
    const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

    struct { int64_t data; int32_t count; int32_t cap; } list = {0, 0, 0};
    if (s_getTiles && map && mc) {
        s_getTiles(map, *(const int16_t*)(mc + 0x28), *(const int16_t*)(mc + 0x2a),
                        *(const int16_t*)(mc + 0x2c), *(const int16_t*)(mc + 0x2e),
                        reinterpret_cast<int64_t*>(&list), 0);
    }

    uint32_t tileCount = (uint32_t)list.count;
    int attempts = 0;
    bool killed = false;
    for (;;) {
        uint32_t idx = fn_Random_Rand ? fn_Random_Rand(rng, tileCount) : 0;
        int64_t entity = *reinterpret_cast<int64_t*>(list.data + (int64_t)idx * 0x10 + 8);
        int64_t natResIBP = 0;
        CheckPtr(&entity);
        if (s_findNatRes) s_findNatRes(entity, &natResIBP);

        if (natResIBP != 0) {
            CheckPtr(&natResIBP);
            if (((*reinterpret_cast<uint32_t*>(natResIBP + 0x20) >> 0x14) & 1) == 0) {
                CheckPtr(&natResIBP);
                *reinterpret_cast<uint32_t*>(natResIBP + 0x28) = 0;   // yield -> nothing
                CheckPtr(&natResIBP);
                *reinterpret_cast<uint8_t*>(natResIBP + 0x2d) = 0;
                CheckPtr(&natResIBP);
                *reinterpret_cast<uint32_t*>(natResIBP + 0x20) |= 0x200000u;  // dead
                if (s_setAnimIndex) s_setAnimIndex(natResIBP, 2);
                killed = true;
                break;
            }
        }
        if (++attempts > 0x31) break;
    }
    if (fn_MemFreeStatic) fn_MemFreeStatic((void*)list.data);

    if (CDTrace()) {
        printf("[CropDisease] KILL-PLANT self=%p killed=%s tiles=%u attempts=%d\n",
               (void*)self, killed ? "yes" : "NO-all-already-dead",
               tileCount, attempts);
        fflush(stdout);
    }
}

// ============================================================
// 0x140047d70 — KillOneAnimal
// ============================================================
extern "C" void DiseaseComp_KillOneAnimal(DiseaseComponent* self) {
    EnsureInit();
    void* gw = GameWorld();
    void* rng = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xD8) : nullptr;
    void* entityMgr = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xE8) : nullptr;

    CheckPtr(&self->entity);
    int64_t pastureIBP = 0;
    if (s_findPasture) s_findPasture((int64_t)self->entity, &pastureIBP);
    CheckPtr(&pastureIBP);
    if (*reinterpret_cast<int32_t*>(pastureIBP + 0x38) == 0) return;

    int attempts = 0;
    bool killed = false;
    do {
        CheckPtr(&pastureIBP);
        uint32_t herd = *reinterpret_cast<uint32_t*>(pastureIBP + 0x38);
        CheckPtr(&pastureIBP);
        uint32_t idx = fn_Random_Rand ? fn_Random_Rand(rng, herd) : 0;
        int64_t member = *reinterpret_cast<int64_t*>(
            *reinterpret_cast<int64_t*>(pastureIBP + 0x30) + (int64_t)idx * 8);
        CheckPtr(&member);
        int64_t entity = *reinterpret_cast<int64_t*>(member + 0x10);
        CheckPtr(&entity);

        // Retail inlines this scan; 0x140005500 is the same scan on the same type id.
        int64_t animalIBP = 0;
        if (s_findAnimal) s_findAnimal(entity, &animalIBP);
        CheckPtr(&animalIBP);

        if (((*reinterpret_cast<uint32_t*>(animalIBP + 0x20) >> 0x14) & 1) == 0) {
            CheckPtr(&member);
            int64_t victim = *reinterpret_cast<int64_t*>(member + 0x10);
            if (victim == 0) break;
            CheckPtr(&victim);
            if (((*reinterpret_cast<uint32_t*>(victim + 0x40) >> 10) & 1) != 0) break;
            CheckPtr(&victim);
            if (s_markRemoval) s_markRemoval((void*)victim);
            if (s_arrResizeFill && entityMgr) {
                s_arrResizeFill(
                    reinterpret_cast<int64_t*>((uintptr_t)entityMgr + 0xD0),
                    *reinterpret_cast<int32_t*>((uintptr_t)entityMgr + 0xD8) + 1,
                    &victim);
            }
            killed = true;
            break;
        }
        ++attempts;
    } while (attempts < 0x32);

    if (CDTrace()) {
        printf("[CropDisease] KILL-ANIMAL self=%p killed=%s attempts=%d\n",
               (void*)self, killed ? "yes" : "NO-none-eligible", attempts);
        fflush(stdout);
    }
}

// ============================================================
// 0x140048610 — InfectionTick (healthy path)
// ============================================================
extern "C" void DiseaseComp_InfectionTick(DiseaseComponent* self, int64_t* deltaTime) {
    EnsureInit();
    void* gw = GameWorld();
    if (!gw) return;

    // The noCropDisease debug flag (GameWorld+0xD0 bit 5) — retail's own kill switch.
    if (((*reinterpret_cast<uint32_t*>((uintptr_t)gw + 0xD0) >> 5) & 1) != 0) return;

    DiseaseDescription* desc = reinterpret_cast<DiseaseDescription*>(self->description);

    void* clock = *reinterpret_cast<void**>((uintptr_t)gw + 0x128);
    float secs = (clock && fn_Time_AsSeconds)
        ? fn_Time_AsSeconds((char*)clock + 8) : 0.0f;
    int month = (int)((secs / SecsPerYear()) * MonthsPerYear());
    if (month > 0xB) month = 0;

    if (month < desc->firstMonth || month > desc->lastMonth) return;

    void* map = *reinterpret_cast<void**>((uintptr_t)gw + 0xF8);
    void* rng = *reinterpret_cast<void**>((uintptr_t)gw + 0xD8);

    CheckPtr(&self->entity);
    int64_t mapIBP = 0;
    if (s_findMap) s_findMap((int64_t)self->entity, &mapIBP);
    CheckPtr(&mapIBP);
    const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);
    int16_t fx = *(const int16_t*)(mc + 0x28), fy = *(const int16_t*)(mc + 0x2a);
    int16_t fw = *(const int16_t*)(mc + 0x2c), fh = *(const int16_t*)(mc + 0x2e);

    self->rollAccum += *deltaTime;
    int64_t period = TimeFromSeconds(desc->rollMonths * SecsPerMonth());

    while (period < self->rollAccum) {
        self->rollAccum -= period;

        // Probe TRUE == the crop recorded at the last Cure is still planted, which
        // retail answers with +0x2c (the SHORTER mean interval). Polarity verified
        // against 0x140048610's `if (probe == 0) +0x28 else +0x2c`; see the warning
        // on DiseaseDescription about the inverted _low/_highSpawnChance names.
        bool sameCrop = DiseaseComp_SameCropProbe(map, fx, fy, fw, fh,
                                                  (int64_t)self->curedIdentity);
        float years = sameCrop ? desc->yearsIfSame : desc->yearsIfChanged;
        float chance = OneF() / ((MonthsPerYear() / desc->rollMonths) * years);
        float roll = fn_Random_RandFloat ? fn_Random_RandFloat(rng) : 1.0f;
        bool passed = (roll < chance);

        if (CDTrace() && s_rollBudget < kRollBudget) {
            ++s_rollBudget;
            printf("[CropDisease] ROLL self=%p month=%d sameCrop=%s years=%.2f "
                   "chance=%.6f roll=%.6f -> %s\n",
                   (void*)self, month, sameCrop ? "HIT" : "miss", (double)years,
                   (double)chance, (double)roll, passed ? "INFECT" : "no");
            fflush(stdout);
        }
        if (passed) DiseaseComp_Infect(self);
    }
}

// ============================================================
// 0x140048840 — ActiveDisease (infested path): kill tick + spread tick
// ============================================================
extern "C" void DiseaseComp_ActiveDisease(DiseaseComponent* self, int64_t* deltaTime) {
    EnsureInit();
    void* gw = GameWorld();
    if (!gw) return;
    DiseaseDescription* desc = reinterpret_cast<DiseaseDescription*>(self->description);

    self->duration    -= *deltaTime;
    self->killAccum   += *deltaTime;
    self->spreadAccum += *deltaTime;

    if (self->duration < 0 || DiseaseComp_CheckCure(self)) {
        if (CDTrace()) {
            printf("[CropDisease] RUN-OUT self=%p reason=%s\n", (void*)self,
                   (self->duration < 0) ? "duration-expired" : "no-plants-left");
            fflush(stdout);
        }
        DiseaseComp_Cure(self);
        return;
    }

    // ---- Kill tick.
    int64_t killPeriod = TimeFromSeconds(desc->killMonths * SecsPerMonth());
    while (killPeriod < self->killAccum) {
        self->killAccum -= killPeriod;
        if (self->isPasture == 1) DiseaseComp_KillOneAnimal(self);
        else                      DiseaseComp_KillOnePlant(self);
    }

    // ---- Spread tick.
    int64_t spreadPeriod = TimeFromSeconds(desc->spreadMonths * SecsPerMonth());
    if (spreadPeriod >= self->spreadAccum) return;

    void* rng = *reinterpret_cast<void**>((uintptr_t)gw + 0xD8);
    int* mapInts = *reinterpret_cast<int**>((uintptr_t)gw + 0xF8);

    for (;;) {
        self->spreadAccum -= spreadPeriod;

        CheckPtr(&self->entity);
        int64_t mapIBP = 0;
        if (s_findMap) s_findMap((int64_t)self->entity, &mapIBP);
        CheckPtr(&mapIBP);
        const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

        int width = *mapInts;
        int centre = ((*(const int16_t*)(mc + 0x2e) >> 1) + *(const int16_t*)(mc + 0x2a))
                     * width
                   + (*(const int16_t*)(mc + 0x2c) >> 1) + *(const int16_t*)(mc + 0x28);

        int tx = centre % width + (fn_Random_RandTol ? fn_Random_RandTol(rng, desc->spreadRadius) : 0);
        int ty = centre / width + (fn_Random_RandTol ? fn_Random_RandTol(rng, desc->spreadRadius) : 0);
        if (tx < 0) tx = 0;
        if (ty < 0) ty = 0;
        if (tx >= width) tx = width - 1;
        if (ty >= width) ty = width - 1;

        // Cell array base is at map+0x80 (`piVar4 + 0x20` on an int* = 0x80 bytes).
        uintptr_t cells = *reinterpret_cast<uintptr_t*>((uintptr_t)mapInts + 0x80);
        int64_t* cell = reinterpret_cast<int64_t*>(
            cells + (uintptr_t)(width * ty + tx) * 0x30);

        int64_t target = 0;
        if (cell[0] != 0)      target = cell[0];           // zone raster wins
        else if (cell[1] != 0) target = cell[1];           // else entity raster

        CheckPtr(&self->entity);
        const char* outcome = "empty-cell";
        if (target != (int64_t)self->entity && target != 0) {
            CheckPtr(&target);
            int64_t sibIBP = 0;
            DiseaseComp_FindSibling(target, &sibIBP);
            if (sibIBP != 0) {
                int64_t targetSlot = target;
                uint64_t theirId = 0, myId = 0;
                DiseaseComp_GrownIdentity(&theirId, &targetSlot);
                int64_t mySlot = (int64_t)self->entity;
                DiseaseComp_GrownIdentity(&myId, &mySlot);
                if (theirId == myId) {
                    CheckPtr(&target);
                    int64_t victimIBP = 0;
                    DiseaseComp_FindSibling(target, &victimIBP);
                    CheckPtr(&victimIBP);
                    DiseaseComp_Infect(reinterpret_cast<DiseaseComponent*>(victimIBP));
                    outcome = "SPREAD";
                } else {
                    outcome = "different-crop";
                }
            } else {
                outcome = "not-infestable";
            }
        } else if (target != 0) {
            outcome = "self";
        }

        if (CDTrace()) {
            printf("[CropDisease] SPREAD-TRY self=%p tile=(%d,%d) radius=%d -> %s\n",
                   (void*)self, tx, ty, (int)desc->spreadRadius, outcome);
            fflush(stdout);
        }

        if (self->spreadAccum <= spreadPeriod) return;
    }
}

// ============================================================
// 0x140048b90 — Update (vtable slot 9)
// ============================================================
// Retail is a 16-byte thunk: CMP qword [RCX+0x30],0 / JG ActiveDisease / JMP InfectionTick.
// +0x30 is the infestation duration remaining on a FIELD or PASTURE (not a citizen).
extern "C" void DiseaseComponent_Update(DiseaseComponent* self, int64_t* deltaTime) {
    if (self->duration > 0) DiseaseComp_ActiveDisease(self, deltaTime);
    else                    DiseaseComp_InfectionTick(self, deltaTime);
}

// ============================================================
// 0x140048c50 — Startup (vtable slot 4)
// ============================================================
extern "C" void DiseaseComponent_Startup(DiseaseComponent* self) {
    EnsureInit();
    CheckPtr(&self->entity);
    int64_t pastureIBP = 0;
    if (s_findPasture) s_findPasture((int64_t)self->entity, &pastureIBP);
    self->isPasture = (pastureIBP != 0) ? 1 : 0;

    if (CDTrace()) {
        printf("[CropDisease] STARTUP self=%p pasture=%d desc=%p entity=%p\n",
               (void*)self, (int)self->isPasture, self->description, self->entity);
        fflush(stdout);
    }
}

// ============================================================
// 0x140048c00 — OnAdded (vtable slot 6): append to the town registry
// ============================================================
// town = *(gw+0xF0); registry DynArray at town+0xF0 {data@+0, count@+8, cap@+0xC}.
// The disassembly reads the count as [RCX+8] off the array base — not a separate
// town field — and calls 0x140003110 (ResizeFill).
extern "C" void DiseaseComponent_OnAdded(DiseaseComponent* self) {
    EnsureInit();
    void* gw = GameWorld();
    if (!gw) return;
    uintptr_t town = *reinterpret_cast<uintptr_t*>((uintptr_t)gw + 0xF0);
    if (!town) return;
    int64_t* reg = reinterpret_cast<int64_t*>(town + 0xF0);

    uint8_t ibp[16] = {0};
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(ibp, self);
    int count = *reinterpret_cast<int32_t*>((uintptr_t)reg + 8);
    if (s_arrResizeFill) s_arrResizeFill(reg, count + 1, ibp);

    if (CDTrace()) {
        printf("[CropDisease] REGISTER self=%p registry-count=%d\n",
               (void*)self, count + 1);
        fflush(stdout);
    }
}

// ============================================================
// 0x140048bb0 — OnRemoved (vtable slot 7): erase from registry, then Cure
// ============================================================
// Retail tail-JMPs into Cure (0x140048320) with RCX still == self.
extern "C" void DiseaseComponent_OnRemoved(DiseaseComponent* self) {
    EnsureInit();
    void* gw = GameWorld();
    if (gw) {
        uintptr_t town = *reinterpret_cast<uintptr_t*>((uintptr_t)gw + 0xF0);
        if (town) {
            uint8_t ibp[16] = {0};
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(ibp, self);
            if (s_arrErase) s_arrErase(reinterpret_cast<int64_t*>(town + 0xF0), ibp);
        }
    }
    DiseaseComp_Cure(self);
}

// ============================================================
// 0x140048ca0 — Serialize (vtable slot 3)
// ============================================================
extern "C" void DiseaseComponent_Serialize(DiseaseComponent* self, void* stream) {
    EnsureInit();
    if (s_compSerialize) s_compSerialize(self, stream);
    if (!s_serCtor || !s_serDtor) return;

    uint8_t ser[24] = {0};
    s_serCtor(ser, stream, 1);
    if (s_extPtrSerialize) s_extPtrSerialize(&self->curedIdentity, ser);
    if (s_serInt)  s_serInt(ser, &self->isPasture, nullptr);
    if (s_serTime) {
        s_serTime(&self->rollAccum,   ser);
        s_serTime(&self->duration,    ser);
        s_serTime(&self->killAccum,   ser);
        s_serTime(&self->spreadAccum, ser);
    }
    // Version-0 saves predate the registry, so re-run OnAdded (vtable +0x30 = slot 6).
    if (s_serGetVer && s_serGetVer(ser) == 0 &&
        *reinterpret_cast<int32_t*>((uintptr_t)stream + 8) == 0) {
        void** vt = *reinterpret_cast<void***>(self);
        typedef void (*fn_slot6_t)(void*);
        if (vt && vt[6]) reinterpret_cast<fn_slot6_t>(vt[6])(self);
    }
    s_serDtor(ser);
}

// ============================================================
// Init hook (kept for the existing declaration in diseasecomponent_funcs.h)
// ============================================================
void DiseaseComponent_InitFuncs() {
    EnsureInit();
    if (CDTrace()) {
        printf("[CropDisease] init live=%d monthsPerYear=%.1f secsPerMonth=%.1f "
               "secsPerYear=%.1f one=%.3f tid=%u\n",
               CDLive() ? 1 : 0, (double)MonthsPerYear(), (double)SecsPerMonth(),
               (double)SecsPerYear(), (double)OneF(), s_tidDiseaseComponent);
        fflush(stdout);
    }
}

// ============================================================
// Standalone factory — a REAL vtable, gated on BANISHED_LIVE_CROPDISEASE
// ============================================================
// Modelled on COMPONENT_LIVE_FACTORY_SOA but installs five slots (3 Serialize,
// 4 Startup, 6 OnAdded, 7 OnRemoved, 9 Update) under one gate, because they are
// one feature: OnAdded puts the component in the registry the debug trigger walks,
// Startup sets the pasture discriminator every other body branches on, Update
// drives it, OnRemoved unregisters, Serialize persists it.
//
// The installs MUST live in FinishLiveVtable: InitInertTyped re-stubs the shared
// per-type vtable on every instance creation, so a slot written once at static-init
// (or from a per-instance body) is erased by the next component of any type.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

namespace {
void* g_DiseaseComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

uint32_t DiseaseComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("DiseaseComponent");
    return cached;
}
void DC_SerializeVSlot(void* s, void* stream) {
    DiseaseComponent_Serialize(reinterpret_cast<DiseaseComponent*>(s), stream);
}
void DC_StartupVSlot(void* s)  { DiseaseComponent_Startup(reinterpret_cast<DiseaseComponent*>(s)); }
void DC_OnAddedVSlot(void* s)  { DiseaseComponent_OnAdded(reinterpret_cast<DiseaseComponent*>(s)); }
void DC_OnRemovedVSlot(void* s){ DiseaseComponent_OnRemoved(reinterpret_cast<DiseaseComponent*>(s)); }
void DC_UpdateVSlot(void* s, void* t) {
    DiseaseComponent_Update(reinterpret_cast<DiseaseComponent*>(s),
                            reinterpret_cast<int64_t*>(t));
}
void DiseaseComponent_FinishLiveVtable() {
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_CROPDISEASE")) return;
    g_DiseaseComponent_LiveVtable[3] = reinterpret_cast<void*>(&DC_SerializeVSlot);
    g_DiseaseComponent_LiveVtable[4] = reinterpret_cast<void*>(&DC_StartupVSlot);
    g_DiseaseComponent_LiveVtable[6] = reinterpret_cast<void*>(&DC_OnAddedVSlot);
    g_DiseaseComponent_LiveVtable[7] = reinterpret_cast<void*>(&DC_OnRemovedVSlot);
    g_DiseaseComponent_LiveVtable[9] = reinterpret_cast<void*>(&DC_UpdateVSlot);
}
} // namespace

extern "C" void* DiseaseComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x58, 0x8B,
        g_DiseaseComponent_LiveVtable,
        reinterpret_cast<void*>(&DiseaseComponent_LiveGetTypeId));
    DiseaseComponent_FinishLiveVtable();
    return p;
}
static void* DiseaseComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x58, 0x8B,
        g_DiseaseComponent_LiveVtable,
        reinterpret_cast<void*>(&DiseaseComponent_LiveGetTypeId));
    DiseaseComponent_FinishLiveVtable();
    return mem;
}

REGISTER_TYPE(DiseaseComponent, "Component",
              &DiseaseComponent_Factory, &DiseaseComponent_CreateInPlace);
