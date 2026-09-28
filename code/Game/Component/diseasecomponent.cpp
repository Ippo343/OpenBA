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
// real bodies. Unproven feature => NOT in the default-on table. With the gate off the
// component stays byte-for-byte the inert component it was before this wave.
// Trace: BANISHED_CROPDISEASE_TRACE (defaults ON when the feature is live, =0 mutes).

#include "Component/diseasecomponent_funcs.h"
#include "options.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A residual
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/String.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/StringTable.h"    // ENDGAME B.3.6 UI callers (K4)
#include "system/MemoryAllocator.h"
#include "Component/exe_bodies.h"
#include "Component/particlecomponent_funcs.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Player/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/exe_constants.h"
#include <cstring>

namespace {

// ============================================================
// Leaves, called directly — every one is ported elsewhere:
//   0x140004fe0 PastureComponent finder   WorkLeaf_FindComp_4fe0
//   0x140004680 MapComponent finder       FloorComponent_FindResData
//   0x140005200 ParticleComponent finder  NaturalResourceComponent_FindComponentByType
//   0x1400052c0 NaturalResource finder    WorkBase_FindComponent_52c0
//   0x140005500 AnimalComponent finder    ActHunt_FindComponent_5500
//   0x140003ea0 StateMachine finder       PlayerPath_FindComponent_3ea0
//   0x140005160 description finder        DragTool_FindDescription_5160
//   0x14009fc00 GetTilesInArea            WorkLeaf_GetTilesInArea_9fc00
//   0x1400c2ea0 MarkRemoval               ActHunt_MarkEntityRemoval_c2ea0
//   0x140003110 ResizeFill                ActHunt_DynArraySetCount_3110
//   0x140002cc0 EraseByValue              CemeteryListErase_Reimpl
//   0x140002340 ExternalPtr serialize     Reimpl_ExternalPtr_Serialize_StringTable
//   0x140126440 SetAnimIndex              ActCNR_SetAnimIndex_126440
//   0x140005380 / 0x140005440 CropField / Orchard finders WorkAcc_FindComponent_5380/_5440
// ============================================================

// Event-log leaves exported from code/Game/Game/eventlog.cpp.
extern "C" void EventLog_ArrayGrow_Reg(void* arrBase, int newCount, const void* newEntry);
extern "C" void EventLog_YearString_Reg(void* gameWorld, void* retStr);
extern "C" void EventLog_SeasonString_Reg(void* gameWorld, void* eventLog, void* retStr);
extern "C" int  EventLog_TileIndex_Reg(void* unused, void* entityIBP);

// Particle VFX (code/Game/Component/particlecomponent.cpp, 0x140053660):
// ParticleComponent_StartEmitterFade, declared in particlecomponent_funcs.h.

static int  s_trace = -1;

// .rdata constants (Game/exe_constants.h).
static inline float BitsF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
static inline float MonthsPerYear() { return BitsF(kRdata_1401721b8); }   // 12.0f
static inline float SecsPerMonth()  { return kRdata_140172234; }          // 300.0f
static inline float SecsPerYear()   { return BitsF(kRdata_14017225c); }   // 3600.0f
static inline float OneF()          { return kRdata_14017214c; }          // 1.0f (g_fUVScale)

static inline void* GameWorld() {
    return GameUpdateLoop::_singleton;
}
static inline int64_t TimeFromSeconds(float secs) {
    int64_t t = 0;                              // Time ctor writes 8 BYTES
    RtSystem_Time_Ctor(reinterpret_cast<::System::Time*>(&t), secs);
    return t;
}

bool CDTrace() {
    if (s_trace < 0) {
        const char* e = Opt().cropDiseaseTrace;
        s_trace = (e && *e && e[0] != '0') ? 1 : 0;
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
    int64_t entity = *reinterpret_cast<int64_t*>(entityIBPSlot);
    if (entity == 0) { *out = 0; return out; }

    int64_t pastureIBP = 0;
    WorkLeaf_FindComp_4fe0(static_cast<uintptr_t>(entity), &pastureIBP);
    if (pastureIBP != 0) {
        *out = *reinterpret_cast<uint64_t*>(pastureIBP + 0x28);
        return out;
    }

    void* gw = GameWorld();
    void* map = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xF8) : nullptr;

    int64_t mapIBP = 0;
    FloorComponent_FindResData(reinterpret_cast<void*>(entity), &mapIBP);
    const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

    struct { int64_t data; int32_t count; int32_t cap; } list = {0, 0, 0};
    if (map && mc) {
        WorkLeaf_GetTilesInArea_9fc00(static_cast<int*>(map),
                        *(const int16_t*)(mc + 0x28), *(const int16_t*)(mc + 0x2a),
                        *(const int16_t*)(mc + 0x2c), *(const int16_t*)(mc + 0x2e),
                        &list, 0);
    }
    const int64_t listData  = list.data;
    const int32_t listCount = list.count;

    if (listCount > 0) {
        for (int i = 0; i < listCount; ++i) {
            int64_t* slot = reinterpret_cast<int64_t*>(listData + (int64_t)i * 0x10 + 8);
            int64_t natResIBP = 0;
            WorkBase_FindComponent_52c0(static_cast<uintptr_t>(*slot), &natResIBP);
            if (natResIBP != 0) {
                *out = *reinterpret_cast<uint64_t*>(*slot + 0x28);
                ::System::MemoryAllocator::MemFree((void*)listData);
                return out;
            }
        }
    } else {
        // Nothing sprouted: fall back to what the field/orchard is SET to grow.
        int64_t cropIBP = 0;
        WorkAcc_FindComponent_5380(static_cast<uintptr_t>(entity), &cropIBP);   // CropFieldComponent finder
        if (cropIBP != 0) {
            *out = *reinterpret_cast<uint64_t*>(cropIBP + 0x50);
            ::System::MemoryAllocator::MemFree((void*)listData);
            return out;
        }
        int64_t orchardIBP = 0;
        WorkAcc_FindComponent_5440(static_cast<uintptr_t>(entity), &orchardIBP);  // OrchardComponent finder
        if (orchardIBP != 0) {
            *out = *reinterpret_cast<uint64_t*>(orchardIBP + 0x60);
            ::System::MemoryAllocator::MemFree((void*)listData);
            return out;
        }
    }

    ::System::MemoryAllocator::MemFree((void*)listData);
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
    uintptr_t s = (uintptr_t)self;
    void* desc = *reinterpret_cast<void**>(s + 0x18);
    int count = *reinterpret_cast<int32_t*>(s + 0x40);
    uintptr_t geoArr = *reinterpret_cast<uintptr_t*>(s + 0x38);
    if (!desc || !geoArr) return;
    uintptr_t elems = *reinterpret_cast<uintptr_t*>((uintptr_t)desc + 0x28);
    if (!elems) return;

    for (int i = 0; i < count; ++i) {
        float t = reinterpret_cast<const System::Time*>(
            reinterpret_cast<void*>(elems + ((int64_t)i * 5 + 4) * 8))->AsSeconds();   // B.3.4b bridge
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
    void* found = nullptr;
    uint32_t want = DiseaseComponent::TypeId();
    int count = *reinterpret_cast<int32_t*>(entity + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        void* comp = *slot;
        uint32_t id = 0;
        if (comp) id = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
        if (id == want) {
            void** ms = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
            if (*ms && ::Object::IsDerivedFromNullSafe(*ms, want)) found = *ms;
            break;
        }
    }
    new (outIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return outIBP;
}

// ============================================================
// 0x1400481b0 — CheckCure
// ============================================================
extern "C" bool DiseaseComp_CheckCure(DiseaseComponent* self) {

    if (self->isPasture == 1) {
        int64_t pastureIBP = 0;
        WorkLeaf_FindComp_4fe0(reinterpret_cast<uintptr_t>(self->_entity), &pastureIBP);
        // Faithful: retail derefs unguarded — isPasture==1 means the finder
        // already located a PastureComponent at Startup.
        if (*reinterpret_cast<int32_t*>(pastureIBP + 0x38) != 0) return false;
        return true;
    }

    void* gw = GameWorld();
    void* map = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xF8) : nullptr;

    int64_t mapIBP = 0;
    FloorComponent_FindResData(self->_entity, &mapIBP);
    const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

    struct { int64_t data; int32_t count; int32_t cap; } list = {0, 0, 0};
    if (map && mc) {
        WorkLeaf_GetTilesInArea_9fc00(static_cast<int*>(map),
                        *(const int16_t*)(mc + 0x28), *(const int16_t*)(mc + 0x2a),
                        *(const int16_t*)(mc + 0x2c), *(const int16_t*)(mc + 0x2e),
                        &list, 0);
    }

    int living = 0;
    for (int i = 0; i < list.count; ++i) {
        int64_t* slot = reinterpret_cast<int64_t*>(list.data + (int64_t)i * 0x10 + 8);
        int64_t natResIBP = 0;
        WorkBase_FindComponent_52c0(static_cast<uintptr_t>(*slot), &natResIBP);
        if (natResIBP != 0) ++living;
    }
    ::System::MemoryAllocator::MemFree((void*)list.data);
    return living == 0;
}

// ============================================================
// 0x140048320 — Cure
// ============================================================
extern "C" void DiseaseComp_Cure(DiseaseComponent* self) {
    int64_t particleIBP = 0;
    NaturalResourceComponent_FindComponentByType(self->_entity, &particleIBP);
    if (particleIBP) ParticleComponent_StopEmitterFade((void*)particleIBP);

    bool wasInfested = (self->duration != 0);

    self->duration    = 0;
    self->killAccum   = 0;
    self->spreadAccum = 0;
    self->rollAccum   = 0;

    // Record what is growing NOW. The next infection roll compares the field's
    // tiles against this: unchanged => the (shorter) yearsIfSame mean applies.
    int64_t entitySlot = (int64_t)self->_entity;
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
    if (DiseaseComp_CheckCure(self)) {
        if (CDTrace()) {
            printf("[CropDisease] INFECT-SKIPPED self=%p reason=already-cureable "
                   "(no plants / empty herd)\n", (void*)self);
            fflush(stdout);
        }
        return;
    }

    int64_t smcIBP = 0;
    PlayerPath_FindComponent_3ea0(reinterpret_cast<uintptr_t>(self->_entity), &smcIBP);
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
    System::String cropName;
    bool haveName = false;
    {
        uint64_t entityDesc = self->_entity
            ? *reinterpret_cast<uint64_t*>((uintptr_t)self->_entity + 0x28) : 0;
        int64_t found = 0;
        if (entityDesc) DragTool_FindDescription_5160(static_cast<uintptr_t>(entityDesc), &found);
        if (found) {
            uint8_t uiStr[16] = {0};
            ::new (uiStr) UI::String(
                Object::ExternalPtr<UI::StringTable>(  // B.3.6 bridge
                    reinterpret_cast<UI::StringTable*>(*reinterpret_cast<uint64_t*>(found + 0x30))),
                *reinterpret_cast<const System::String*>(found + 0x40));
            ::new (&cropName) System::String(
                reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge
            haveName = true;
        }
    }

    void* eventLog = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0x138) : nullptr;
    if (eventLog && gw) {
        System::String yearStr, seasonStr;
        System::String templateStr;
        EventLog_YearString_Reg(gw, &yearStr);
        EventLog_SeasonString_Reg(gw, eventLog, &seasonStr);

        void* eventsTable = *reinterpret_cast<void**>(eventLog);
        if (eventsTable)
            ::new (&templateStr) System::String(
                reinterpret_cast<const UI::StringTable*>(eventsTable)->GetText(0xC));  // B.3.6 bridge
        else
            templateStr.Set("Crop infestation");

        System::String formatted = System::String::FormatIndexed(templateStr._data,
                                    seasonStr._data, yearStr._data,
                                    haveName ? cropName._data : "");

        void* entityIBP = &self->_entity;
        int tileIdx = EventLog_TileIndex_Reg(nullptr, entityIBP);

        uint8_t entry[0x28] = {0};
        *(uint32_t*)(entry + 0x00) = 0xFFFF6060u;   // severity-3 red
        ::new (static_cast<void*>(entry + 0x08)) System::String(formatted);   // B.3.1 bridge
        *(int32_t*)(entry + 0x10) = 0xC;            // eventType: crop infestation
        *(int32_t*)(entry + 0x14) = 3;              // severity
        *(int32_t*)(entry + 0x18) = tileIdx;
        *(int32_t*)(entry + 0x1c) = 2;              // iconIndex (hardcoded here)
        *(int32_t*)(entry + 0x20) = -1;             // no popup

        int count = *reinterpret_cast<int32_t*>((uintptr_t)eventLog + 0x20);
        EventLog_ArrayGrow_Reg((void*)((uintptr_t)eventLog + 0x18), count + 1, entry);

        reinterpret_cast<System::String*>(entry + 0x08)->~String();   // B.3.1 bridge
    }

    // Drop to 1x speed if the settings' auto-slow byte (+0x17) is on. Retail
    // 0x140048542: MOV RAX,[0x1401c3de0]; CMP byte [RAX+0x17],0 -- the byte of
    // the settings BLOCK (this read indexed the cell pointer, 0xb8 bytes past it).
    if (g_gameSettings != nullptr &&
        reinterpret_cast<const uint8_t*>(g_gameSettings)[0x17] != 0 && gw) {
        void* speedObj = *reinterpret_cast<void**>((uintptr_t)gw + 0x120);
        if (speedObj) *reinterpret_cast<int32_t*>((uintptr_t)speedObj + 0x34) = 1;
    }

    // ---- Start every emitter on the entity's ParticleComponent (the swarm VFX).
    int64_t particleIBP = 0;
    NaturalResourceComponent_FindComponentByType(self->_entity, &particleIBP);
    int emitters = 0;
    if (particleIBP) {
        emitters = *reinterpret_cast<int32_t*>(particleIBP + 0x40);
        for (int i = 0; i < emitters; ++i)
            ParticleComponent_StartEmitterFade(reinterpret_cast<ParticleComponent*>(particleIBP), i);
    }

    // ---- Arm the timers.
    DiseaseDescription* desc = reinterpret_cast<DiseaseDescription*>(self->_description);
    self->duration    = TimeFromSeconds(desc->durationMonths * SecsPerMonth());
    self->killAccum   = 0;
    self->spreadAccum = 0;

    if (CDTrace()) {
        printf("[CropDisease] INFECT self=%p pasture=%d stage=%d durationMonths=%.2f "
               "emitters=%d tile=%d\n",
               (void*)self, (int)self->isPasture, (int)stage,
               (double)desc->durationMonths, emitters,
               EventLog_TileIndex_Reg(nullptr, &self->_entity));
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
    void* gw = GameWorld();
    void* map = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xF8) : nullptr;
    void* rng = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xD8) : nullptr;

    int64_t mapIBP = 0;
    FloorComponent_FindResData(self->_entity, &mapIBP);
    const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

    struct { int64_t data; int32_t count; int32_t cap; } list = {0, 0, 0};
    if (map && mc) {
        WorkLeaf_GetTilesInArea_9fc00(static_cast<int*>(map),
                        *(const int16_t*)(mc + 0x28), *(const int16_t*)(mc + 0x2a),
                        *(const int16_t*)(mc + 0x2c), *(const int16_t*)(mc + 0x2e),
                        &list, 0);
    }

    uint32_t tileCount = (uint32_t)list.count;
    int attempts = 0;
    bool killed = false;
    for (;;) {
        uint32_t idx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(tileCount));   // B.3.4b bridge
        int64_t entity = *reinterpret_cast<int64_t*>(list.data + (int64_t)idx * 0x10 + 8);
        int64_t natResIBP = 0;
        WorkBase_FindComponent_52c0(static_cast<uintptr_t>(entity), &natResIBP);

        if (natResIBP != 0) {
            if (((*reinterpret_cast<uint32_t*>(natResIBP + 0x20) >> 0x14) & 1) == 0) {
                *reinterpret_cast<uint32_t*>(natResIBP + 0x28) = 0;   // yield -> nothing
                *reinterpret_cast<uint8_t*>(natResIBP + 0x2d) = 0;
                *reinterpret_cast<uint32_t*>(natResIBP + 0x20) |= 0x200000u;  // dead
                ActCNR_SetAnimIndex_126440(natResIBP, 2);
                killed = true;
                break;
            }
        }
        if (++attempts > 0x31) break;
    }
    ::System::MemoryAllocator::MemFree((void*)list.data);

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
    void* gw = GameWorld();
    void* rng = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xD8) : nullptr;
    void* entityMgr = gw ? *reinterpret_cast<void**>((uintptr_t)gw + 0xE8) : nullptr;

    int64_t pastureIBP = 0;
    WorkLeaf_FindComp_4fe0(reinterpret_cast<uintptr_t>(self->_entity), &pastureIBP);
    if (*reinterpret_cast<int32_t*>(pastureIBP + 0x38) == 0) return;

    int attempts = 0;
    bool killed = false;
    do {
        uint32_t herd = *reinterpret_cast<uint32_t*>(pastureIBP + 0x38);
        uint32_t idx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(herd));   // B.3.4b bridge
        int64_t member = *reinterpret_cast<int64_t*>(
            *reinterpret_cast<int64_t*>(pastureIBP + 0x30) + (int64_t)idx * 8);
        int64_t entity = *reinterpret_cast<int64_t*>(member + 0x10);

        // Retail inlines this scan; 0x140005500 is the same scan on the same type id.
        int64_t animalIBP = 0;
        ActHunt_FindComponent_5500(static_cast<uintptr_t>(entity), &animalIBP);

        if (((*reinterpret_cast<uint32_t*>(animalIBP + 0x20) >> 0x14) & 1) == 0) {
            int64_t victim = *reinterpret_cast<int64_t*>(member + 0x10);
            if (victim == 0) break;
            if (((*reinterpret_cast<uint32_t*>(victim + 0x40) >> 10) & 1) != 0) break;
            ActHunt_MarkEntityRemoval_c2ea0((void*)victim);
            if (entityMgr) {
                ActHunt_DynArraySetCount_3110(
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
    void* gw = GameWorld();
    if (!gw) return;

    // The noCropDisease debug flag (GameWorld+0xD0 bit 5) — retail's own kill switch.
    if (((*reinterpret_cast<uint32_t*>((uintptr_t)gw + 0xD0) >> 5) & 1) != 0) return;

    DiseaseDescription* desc = reinterpret_cast<DiseaseDescription*>(self->_description);

    void* clock = *reinterpret_cast<void**>((uintptr_t)gw + 0x128);
    float secs = clock ? reinterpret_cast<const System::Time*>((char*)clock + 8)->AsSeconds() : 0.0f;   // B.3.4b bridge
    int month = (int)((secs / SecsPerYear()) * MonthsPerYear());
    if (month > 0xB) month = 0;

    if (month < desc->firstMonth || month > desc->lastMonth) return;

    void* map = *reinterpret_cast<void**>((uintptr_t)gw + 0xF8);
    void* rng = *reinterpret_cast<void**>((uintptr_t)gw + 0xD8);

    int64_t mapIBP = 0;
    FloorComponent_FindResData(self->_entity, &mapIBP);
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
        float roll = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
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
    void* gw = GameWorld();
    if (!gw) return;
    DiseaseDescription* desc = reinterpret_cast<DiseaseDescription*>(self->_description);

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

        int64_t mapIBP = 0;
        FloorComponent_FindResData(self->_entity, &mapIBP);
        const uint8_t* mc = reinterpret_cast<const uint8_t*>(mapIBP);

        int width = *mapInts;
        int centre = ((*(const int16_t*)(mc + 0x2e) >> 1) + *(const int16_t*)(mc + 0x2a))
                     * width
                   + (*(const int16_t*)(mc + 0x2c) >> 1) + *(const int16_t*)(mc + 0x28);

        int tx = centre % width + reinterpret_cast<System::Random*>(rng)->RandTol(static_cast<int32_t>(desc->spreadRadius));   // B.3.4b bridge
        int ty = centre / width + reinterpret_cast<System::Random*>(rng)->RandTol(static_cast<int32_t>(desc->spreadRadius));   // B.3.4b bridge
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

        const char* outcome = "empty-cell";
        if (target != (int64_t)self->_entity && target != 0) {
            int64_t sibIBP = 0;
            DiseaseComp_FindSibling(target, &sibIBP);
            if (sibIBP != 0) {
                int64_t targetSlot = target;
                uint64_t theirId = 0, myId = 0;
                DiseaseComp_GrownIdentity(&theirId, &targetSlot);
                int64_t mySlot = (int64_t)self->_entity;
                DiseaseComp_GrownIdentity(&myId, &mySlot);
                if (theirId == myId) {
                    int64_t victimIBP = 0;
                    DiseaseComp_FindSibling(target, &victimIBP);
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

// ============================================================
// 0x140048c50 — Startup (vtable slot 4)
// ============================================================

// ============================================================
// 0x140048c00 — OnAdded (vtable slot 6): append to the town registry
// ============================================================
// town = *(gw+0xF0); registry DynArray at town+0xF0 {data@+0, count@+8, cap@+0xC}.
// The disassembly reads the count as [RCX+8] off the array base — not a separate
// town field — and calls 0x140003110 (ResizeFill).

// ============================================================
// 0x140048bb0 — OnRemoved (vtable slot 7): erase from registry, then Cure
// ============================================================
// Retail tail-JMPs into Cure (0x140048320) with RCX still == self.

// ============================================================
// 0x140048ca0 — Serialize (vtable slot 3)
// ============================================================

// ============================================================
// ENDGAME B.3.8 CIT — DiseaseComponent as a REAL class (DiseaseComponent.h).
// ============================================================
// Before: a 48-slot hand-built g_DiseaseComponent_LiveVtable filled at every
// create (slots 3 Serialize, 4 Startup, 6 OnAdded, 7 OnRemoved, 9 Update),
// Inert_ScalarDtor in slot 0. After: the compiler's 16-slot vtable and a real
// ctor/dtor chain (B38_RULES D1, D2). Slot 3 is wired by construction: a
// class's table cannot lose its serializer.
#include "Object/factory_registration.h"
#include "Component/DiseaseComponent.h"

// 0x140048db0. Component() is the CALL 0x1401188b0; the 0x140159290 stamp is
// the compiler's; +0x28..+0x40 (4 qwords), +0x48 (dword), +0x50 = 0; flags |= 0x8b.
DiseaseComponent::DiseaseComponent()
    : rollAccum(0), duration(0), killAccum(0), spreadAccum(0),
      isPasture(0), _pad4C{}, curedIdentity(0) {
    OrFlagsInCtor(0x8b);
}

void DiseaseComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    alignas(8) uint8_t ser[24] = {0};
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge
    Reimpl_ExternalPtr_Serialize_StringTable(&curedIdentity, ser);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(isPasture, nullptr);   // B.3.4 bridge
    reinterpret_cast<System::Time*>(&rollAccum)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<System::Time*>(&duration)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<System::Time*>(&killAccum)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<System::Time*>(&spreadAccum)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    // Version-0 saves predate the registry, so re-run OnAdded (vtable +0x30 = slot 6).
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() == 0 && static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        OnAdded();   // slot 6, dispatched virtually as retail does
    }
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
void DiseaseComponent::Startup() {
    int64_t pastureIBP = 0;
    WorkLeaf_FindComp_4fe0(reinterpret_cast<uintptr_t>(_entity), &pastureIBP);
    isPasture = (pastureIBP != 0) ? 1 : 0;

    if (CDTrace()) {
        printf("[CropDisease] STARTUP this=%p pasture=%d desc=%p entity=%p\n",
               (void*)this, (int)isPasture, _description, _entity);
        fflush(stdout);
    }
}
void DiseaseComponent::OnAdded() {
    void* gw = GameWorld();
    if (!gw) return;
    uintptr_t town = *reinterpret_cast<uintptr_t*>((uintptr_t)gw + 0xF0);
    if (!town) return;
    int64_t* reg = reinterpret_cast<int64_t*>(town + 0xF0);

    uint8_t ibp[16] = {0};
    new (ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
    int count = *reinterpret_cast<int32_t*>((uintptr_t)reg + 8);
    ActHunt_DynArraySetCount_3110(reg, count + 1, ibp);

    if (CDTrace()) {
        printf("[CropDisease] REGISTER this=%p registry-count=%d\n",
               (void*)this, count + 1);
        fflush(stdout);
    }
}
void DiseaseComponent::OnRemove() {
    void* gw = GameWorld();
    if (gw) {
        uintptr_t town = *reinterpret_cast<uintptr_t*>((uintptr_t)gw + 0xF0);
        if (town) {
            uint8_t ibp[16] = {0};
            new (ibp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
            CemeteryListErase_Reimpl(reinterpret_cast<int64_t*>(town + 0xF0), ibp);
        }
    }
    DiseaseComp_Cure(this);
}

// [9] 0x140048b90. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void DiseaseComponent::Update(void* time) {
    int64_t* deltaTime = static_cast<int64_t*>(time);
    if (duration > 0) DiseaseComp_ActiveDisease(this, deltaTime);
    else                    DiseaseComp_InfectionTick(this, deltaTime);
}

// create 0x140004db0: MemAlloc(0x58, 8), then the ctor.
void* DiseaseComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(DiseaseComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) DiseaseComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* DiseaseComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) DiseaseComponent());
}

DEFINE_EXE_TYPE(DiseaseComponent, "Component")
