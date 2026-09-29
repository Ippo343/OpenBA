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
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>
#include "Component/MapComponent.h"
#include "Component/DiseaseComponent.h"
#include "Component/DiseaseDescription.h"
#include "Component/PastureComponent.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/ParticleComponent.h"
#include "Component/ParticleDescription.h"
#include "Component/AnimalComponent.h"
#include "Component/CropFieldComponent.h"
#include "Component/OrchardComponent.h"
#include "Component/StateMachineComponent.h"
#include "Component/ComponentDescription.h"
#include "Game/PlayerToolManager.h"
#include "Game/GameSettings.h"

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

static inline GameUpdateLoop* GameWorld() {
    return GameUpdateLoop::_singleton;
}
static inline int64_t TimeFromSeconds(float secs) {
    ::System::Time t = {0};                     // Time ctor writes 8 BYTES
    RtSystem_Time_Ctor(&t, secs);
    return t._ticks;
}

// One entry of the tile list 0x14009fc00 fills (16 bytes: the cell index, a
// zero dword, the entity on the cell), and the list itself (data / count /
// capacity), freed by the caller.
struct TileInfo {
    int32_t          cell;     // +0x00
    int32_t          pad04;    // +0x04
    ComponentEntity* entity;   // +0x08
};
static_assert(sizeof(TileInfo) == 0x10, "TileInfo is 16 bytes");
struct TileInfoList {
    TileInfo* data;    // +0x00
    int32_t   count;   // +0x08
    int32_t   cap;     // +0x0C
};

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
                                          const ComponentDescription* wantIdentity) {
    const uint8_t* m = reinterpret_cast<const uint8_t*>(map);
    int32_t width = *reinterpret_cast<const int32_t*>(m + 0x00);
    uintptr_t cells = *reinterpret_cast<const uintptr_t*>(m + 0x80);

    for (int64_t dx = 0; dx < w; ++dx) {
        uintptr_t cell = cells + 0x10 +
            (uintptr_t)(((int64_t)width * y) + dx + x) * 0x30;
        for (int64_t dy = 0; dy < h; ++dy) {
            if (*reinterpret_cast<ComponentDescription* const*>(cell) == wantIdentity) return true;
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
// Retail returns an ExternalPtr<ComponentDescription> by value (RCX = the return
// slot) from an InternalPtr<ComponentEntity> (RDX): it reads no DiseaseComponent
// field, so it stays a free function (B1).
extern "C" ComponentDescription** DiseaseComp_GrownIdentity(ComponentDescription** out,
                                                           ComponentEntity* const* entitySlot) {
    ComponentEntity* entity = *entitySlot;
    if (entity == nullptr) { *out = nullptr; return out; }

    PastureComponent* pasture = entity->FindComponent<PastureComponent>();   // 0x140004fe0
    if (pasture != nullptr) {
        // PastureComponent +0x28 is `void* field28` (COMP-d's header): LEFT.
        *out = reinterpret_cast<ComponentDescription*>(pasture->field28);
        return out;
    }

    GameUpdateLoop* gw = GameWorld();
    void* map = gw ? gw->m_game : nullptr;

    MapComponent* mc = entity->FindComponent<MapComponent>();   // 0x140004680

    // Retail loads the footprint with MOVSX (0x140048031..3f): signed shorts.
    TileInfoList list = {nullptr, 0, 0};
    if (map && mc) {
        WorkLeaf_GetTilesInArea_9fc00(static_cast<int*>(map),
                        static_cast<int16_t>(mc->tileX0), static_cast<int16_t>(mc->tileY0),
                        static_cast<int16_t>(mc->tileX1), static_cast<int16_t>(mc->tileY1),
                        &list, 0);
    }
    TileInfo* const listData  = list.data;
    const int32_t   listCount = list.count;

    if (listCount > 0) {
        for (int i = 0; i < listCount; ++i) {
            ComponentEntity* tileEntity = listData[i].entity;
            if (tileEntity->FindComponent<NaturalResourceComponent>() != nullptr) {   // 0x1400052c0
                *out = tileEntity->_description;
                ::System::MemoryAllocator::MemFree(listData);
                return out;
            }
        }
    } else {
        // Nothing sprouted: fall back to what the field/orchard is SET to grow.
        CropFieldComponent* crop = entity->FindComponent<CropFieldComponent>();   // 0x140005380
        if (crop != nullptr) {
            *out = crop->cropDescription;
            ::System::MemoryAllocator::MemFree(listData);
            return out;
        }
        OrchardComponent* orchard = entity->FindComponent<OrchardComponent>();   // 0x140005440
        if (orchard != nullptr) {
            // OrchardComponent +0x60 is `void* field60` (COMP-d's header): LEFT.
            *out = reinterpret_cast<ComponentDescription*>(orchard->field60);
            ::System::MemoryAllocator::MemFree(listData);
            return out;
        }
    }

    ::System::MemoryAllocator::MemFree(listData);
    *out = nullptr;
    return out;
}

// ============================================================
// 0x1400537f0 — ParticleComponent: fade the emitters OUT
// ============================================================
// Sibling of ParticleComponent_StartEmitterFade (0x140053660). Writes the same
// per-emitter fade-rate field (geoArray + i*0x18 + 0x10) but XORs the float's BIT
// PATTERN with 0x80000000 (the .rdata sign mask at 0x1401722c0) to negate it —
// a *bit* operation, not a numeric cast, whatever the decompiler's `(uint)` says.
// A ParticleComponent member (RCX = the component; declared in ParticleComponent.h).
// Its +0x38 / +0x40 are `void*` in COMP-d's header and the description's emitter
// array is `void*` in ParticleDescription.h: those reads are LEFT.
void ParticleComponent::StopEmitterFade() {
    Description* base = _description;
    ParticleDescription* desc = static_cast<ParticleDescription*>(base);
    int count = *reinterpret_cast<int32_t*>(&field40);
    uintptr_t geoArr = reinterpret_cast<uintptr_t>(field38);
    if (!desc || !geoArr) return;
    uintptr_t elems = reinterpret_cast<uintptr_t>(desc->_emitters);
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
// 0x1400481b0 — CheckCure
// ============================================================
bool DiseaseComponent::CheckCure() {

    if (isPasture == 1) {
        PastureComponent* pasture = _entity->FindComponent<PastureComponent>();   // 0x140004fe0
        // Faithful: retail derefs unguarded — isPasture==1 means the finder
        // already located a PastureComponent at Startup. The herd count is
        // PastureComponent +0x38, `void* field38` in COMP-d's header: LEFT.
        if (*reinterpret_cast<int32_t*>(&pasture->field38) != 0) return false;
        return true;
    }

    GameUpdateLoop* gw = GameWorld();
    void* map = gw ? gw->m_game : nullptr;

    MapComponent* mc = _entity->FindComponent<MapComponent>();   // 0x140004680

    // Retail loads the footprint with MOVSX (0x14004824d..5b): signed shorts.
    TileInfoList list = {nullptr, 0, 0};
    if (map && mc) {
        WorkLeaf_GetTilesInArea_9fc00(static_cast<int*>(map),
                        static_cast<int16_t>(mc->tileX0), static_cast<int16_t>(mc->tileY0),
                        static_cast<int16_t>(mc->tileX1), static_cast<int16_t>(mc->tileY1),
                        &list, 0);
    }

    int living = 0;
    for (int i = 0; i < list.count; ++i) {
        if (list.data[i].entity->FindComponent<NaturalResourceComponent>() != nullptr)   // 0x1400052c0
            ++living;
    }
    ::System::MemoryAllocator::MemFree(list.data);
    return living == 0;
}

// ============================================================
// 0x140048320 — Cure
// ============================================================
void DiseaseComponent::Cure() {
    ParticleComponent* particle = _entity->FindComponent<ParticleComponent>();   // 0x140005200
    if (particle) particle->StopEmitterFade();

    bool wasInfested = (duration._ticks != 0);

    duration._ticks    = 0;
    killAccum._ticks   = 0;
    spreadAccum._ticks = 0;
    rollAccum._ticks   = 0;

    // Record what is growing NOW. The next infection roll compares the field's
    // tiles against this: unchanged => the (shorter) yearsIfSame mean applies.
    ComponentDescription* identity = nullptr;
    DiseaseComp_GrownIdentity(&identity, &_entity);
    curedIdentity = identity;

    if (wasInfested && CDTrace()) {
        printf("[CropDisease] CURE self=%p pasture=%d newIdentity=%p\n",
               (void*)this, (int)isPasture, (void*)identity);
        fflush(stdout);
    }
}

// ============================================================
// 0x1400483a0 — Infect
// ============================================================
void DiseaseComponent::Infect() {
    if (CheckCure()) {
        if (CDTrace()) {
            printf("[CropDisease] INFECT-SKIPPED self=%p reason=already-cureable "
                   "(no plants / empty herd)\n", (void*)this);
            fflush(stdout);
        }
        return;
    }

    uintptr_t smcIBP = reinterpret_cast<uintptr_t>(
        _entity->FindComponent<StateMachineComponent>());   // 0x140003ea0
    // State gate: the field must be past stage 1. +0x52 is the override, used
    // unless negative, in which case +0x50 (the live state) is read. The two
    // bytes are inside StateMachineComponent::stateFlags (COMP-e's header): LEFT.
    int8_t stage = *reinterpret_cast<int8_t*>(smcIBP + 0x52);
    if (stage < 0) stage = *reinterpret_cast<int8_t*>(smcIBP + 0x50);
    if (stage <= 1) {
        if (CDTrace()) {
            printf("[CropDisease] INFECT-SKIPPED self=%p reason=stage=%d (<=1)\n",
                   (void*)this, (int)stage);
            fflush(stdout);
        }
        return;
    }

    GameUpdateLoop* gw = GameWorld();

    // ---- Event-log post: type 0xC severity 3, colour 0xFFFF6060, icon 2, no popup.
    // A third entry-append variant next to AddEvent/AddCitizenEvent: three format
    // args ({0} season {1} year {2} crop name) and hardcoded icon/popup.
    System::String cropName;
    bool haveName = false;
    {
        ComponentDescription* entityDesc = _entity ? _entity->_description : nullptr;
        int64_t found = 0;
        if (entityDesc) DragTool_FindDescription_5160(reinterpret_cast<uintptr_t>(entityDesc), &found);
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

    // GameUpdateLoop::m_eventNotifier (+0x138) is still void* (GAME-a): the
    // notifier's table and entry array below are LEFT.
    void* eventLog = gw ? gw->m_eventNotifier : nullptr;
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

        void* entityIBP = &_entity;
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
    if (g_gameSettings != nullptr && g_gameSettings->autoSlow != 0 && gw) {
        PlayerToolManager* speedObj = gw->m_toolManager;   // +0x120
        if (speedObj) speedObj->field_0x34 = 1;            // 0x14004854f MOV dword [RCX+0x34],1
    }

    // ---- Start every emitter on the entity's ParticleComponent (the swarm VFX).
    ParticleComponent* particle = _entity->FindComponent<ParticleComponent>();   // 0x140005200
    int emitters = 0;
    if (particle) {
        // ParticleComponent +0x40 (the emitter count) is `void* field40` in
        // COMP-d's header: LEFT.
        emitters = *reinterpret_cast<int32_t*>(&particle->field40);
        for (int i = 0; i < emitters; ++i)
            ParticleComponent_StartEmitterFade(particle, i);
    }

    // ---- Arm the timers. Retail takes the description through IsDerivedFrom
    // DiseaseDescription (0x1400485be) and reads +0x3c unguarded.
    Description* base = _description;
    DiseaseDescription* desc = static_cast<DiseaseDescription*>(base);
    duration._ticks    = TimeFromSeconds(desc->durationMonths * SecsPerMonth());
    killAccum._ticks   = 0;
    spreadAccum._ticks = 0;

    if (CDTrace()) {
        printf("[CropDisease] INFECT self=%p pasture=%d stage=%d durationMonths=%.2f "
               "emitters=%d tile=%d\n",
               (void*)this, (int)isPasture, (int)stage,
               (double)desc->durationMonths, emitters,
               EventLog_TileIndex_Reg(nullptr, &_entity));
        fflush(stdout);
    }
}

// ============================================================
// 0x140047b40 — KillOnePlant  (Ghidra: AnimateNaturalResource_impl_7b40)
// ============================================================
// Up to 50 random tries over the field footprint for a NaturalResourceComponent
// that is not already dead (flag bit 0x100000). The kill zeroes +0x28 (retail
// 0x140047cd6 `MOV dword [RAX+0x28],R14D`, R14 = 0: resourceScale = 0.0f, the
// same bits), so the plant produces NOTHING, clears +0x2d, sets the dead bit
// 0x200000 and switches the model to animation index 2 (withered).
void DiseaseComponent::KillOnePlant() {
    GameUpdateLoop* gw = GameWorld();
    void* map = gw ? gw->m_game : nullptr;
    System::Random* rng = gw ? gw->m_random : nullptr;

    MapComponent* mc = _entity->FindComponent<MapComponent>();   // 0x140004680

    // Retail loads the footprint with MOVSX: signed shorts.
    TileInfoList list = {nullptr, 0, 0};
    if (map && mc) {
        WorkLeaf_GetTilesInArea_9fc00(static_cast<int*>(map),
                        static_cast<int16_t>(mc->tileX0), static_cast<int16_t>(mc->tileY0),
                        static_cast<int16_t>(mc->tileX1), static_cast<int16_t>(mc->tileY1),
                        &list, 0);
    }

    uint32_t tileCount = (uint32_t)list.count;
    int attempts = 0;
    bool killed = false;
    for (;;) {
        uint32_t idx = rng->Rand(static_cast<uint32_t>(tileCount));
        ComponentEntity* entity = list.data[idx].entity;
        NaturalResourceComponent* natRes =
            entity->FindComponent<NaturalResourceComponent>();   // 0x1400052c0 (inlined)

        if (natRes != nullptr) {
            if (((natRes->_flags >> 0x14) & 1) == 0) {
                natRes->resourceScale = 0.0f;   // yield -> nothing
                // +0x2d is the high byte of NaturalResourceComponent::resourceFlags
                // (COMP-d's header): LEFT.
                *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(natRes) + 0x2d) = 0;
                natRes->_flags |= 0x200000u;  // dead
                ActCNR_SetAnimIndex_126440(reinterpret_cast<int64_t>(natRes), 2);
                killed = true;
                break;
            }
        }
        if (++attempts > 0x31) break;
    }
    ::System::MemoryAllocator::MemFree(list.data);

    if (CDTrace()) {
        printf("[CropDisease] KILL-PLANT self=%p killed=%s tiles=%u attempts=%d\n",
               (void*)this, killed ? "yes" : "NO-all-already-dead",
               tileCount, attempts);
        fflush(stdout);
    }
}

// ============================================================
// 0x140047d70 — KillOneAnimal
// ============================================================
void DiseaseComponent::KillOneAnimal() {
    GameUpdateLoop* gw = GameWorld();
    System::Random* rng = gw ? gw->m_random : nullptr;
    // GameUpdateLoop::m_entityList (+0xE8) is still void* (GAME-a): the destroy
    // queue at +0xD0 / +0xD8 below is LEFT.
    void* entityMgr = gw ? gw->m_entityList : nullptr;

    PastureComponent* pasture = _entity->FindComponent<PastureComponent>();   // 0x140004fe0
    // The herd (+0x30 data, +0x38 count) is `void*` in COMP-d's header: LEFT.
    if (*reinterpret_cast<int32_t*>(&pasture->field38) == 0) return;

    int attempts = 0;
    bool killed = false;
    do {
        uint32_t herd = *reinterpret_cast<uint32_t*>(&pasture->field38);
        uint32_t idx = rng->Rand(static_cast<uint32_t>(herd));
        Component* member = reinterpret_cast<Component**>(pasture->field30)[idx];

        // Retail inlines this scan; 0x140005500 is the same scan on the same type id.
        AnimalComponent* animal = member->_entity->FindComponent<AnimalComponent>();

        if (((animal->_flags >> 0x14) & 1) == 0) {
            ComponentEntity* victim = member->_entity;
            if (victim == nullptr) break;
            if (((victim->_entityFlags >> 10) & 1) != 0) break;
            ActHunt_MarkEntityRemoval_c2ea0(victim);
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
               (void*)this, killed ? "yes" : "NO-none-eligible", attempts);
        fflush(stdout);
    }
}

// ============================================================
// 0x140048610 — InfectionTick (healthy path)
// ============================================================
void DiseaseComponent::InfectionTick(System::Time* deltaTime) {
    GameUpdateLoop* gw = GameWorld();
    if (!gw) return;

    // The noCropDisease debug flag (GameWorld+0xD0 bit 5) — retail's own kill switch.
    if (((gw->m_gameFlags >> 5) & 1) != 0) return;

    Description* base = _description;
    DiseaseDescription* desc = static_cast<DiseaseDescription*>(base);

    // GameUpdateLoop::m_dayCounter (+0x128) is still void* (GAME-a): LEFT.
    void* clock = gw->m_dayCounter;
    float secs = clock ? reinterpret_cast<const System::Time*>((char*)clock + 8)->AsSeconds() : 0.0f;   // B.3.4b bridge
    int month = (int)((secs / SecsPerYear()) * MonthsPerYear());
    if (month > 0xB) month = 0;

    if (month < desc->firstMonth || month > desc->lastMonth) return;

    void* map = gw->m_game;
    System::Random* rng = gw->m_random;

    MapComponent* mc = _entity->FindComponent<MapComponent>();   // 0x140004680
    // Retail loads the footprint with MOVSX: signed shorts.
    int16_t fx = static_cast<int16_t>(mc->tileX0), fy = static_cast<int16_t>(mc->tileY0);
    int16_t fw = static_cast<int16_t>(mc->tileX1), fh = static_cast<int16_t>(mc->tileY1);

    rollAccum._ticks += deltaTime->_ticks;
    int64_t period = TimeFromSeconds(desc->rollMonths * SecsPerMonth());

    while (period < rollAccum._ticks) {
        rollAccum._ticks -= period;

        // Probe TRUE == the crop recorded at the last Cure is still planted, which
        // retail answers with +0x2c (the SHORTER mean interval). Polarity verified
        // against 0x140048610's `if (probe == 0) +0x28 else +0x2c`; see the warning
        // on DiseaseDescription about the inverted _low/_highSpawnChance names.
        bool sameCrop = DiseaseComp_SameCropProbe(map, fx, fy, fw, fh, curedIdentity);
        float years = sameCrop ? desc->yearsIfSame : desc->yearsIfChanged;
        float chance = OneF() / ((MonthsPerYear() / desc->rollMonths) * years);
        float roll = rng->RandFloat();
        bool passed = (roll < chance);

        if (CDTrace() && s_rollBudget < kRollBudget) {
            ++s_rollBudget;
            printf("[CropDisease] ROLL self=%p month=%d sameCrop=%s years=%.2f "
                   "chance=%.6f roll=%.6f -> %s\n",
                   (void*)this, month, sameCrop ? "HIT" : "miss", (double)years,
                   (double)chance, (double)roll, passed ? "INFECT" : "no");
            fflush(stdout);
        }
        if (passed) Infect();
    }
}

// ============================================================
// 0x140048840 — ActiveDisease (infested path): kill tick + spread tick
// ============================================================
void DiseaseComponent::ActiveDisease(System::Time* deltaTime) {
    GameUpdateLoop* gw = GameWorld();
    if (!gw) return;
    Description* base = _description;
    DiseaseDescription* desc = static_cast<DiseaseDescription*>(base);

    duration._ticks    -= deltaTime->_ticks;
    killAccum._ticks   += deltaTime->_ticks;
    spreadAccum._ticks += deltaTime->_ticks;

    if (duration._ticks < 0 || CheckCure()) {
        if (CDTrace()) {
            printf("[CropDisease] RUN-OUT self=%p reason=%s\n", (void*)this,
                   (duration._ticks < 0) ? "duration-expired" : "no-plants-left");
            fflush(stdout);
        }
        Cure();
        return;
    }

    // ---- Kill tick.
    int64_t killPeriod = TimeFromSeconds(desc->killMonths * SecsPerMonth());
    while (killPeriod < killAccum._ticks) {
        killAccum._ticks -= killPeriod;
        if (isPasture == 1) KillOneAnimal();
        else                KillOnePlant();
    }

    // ---- Spread tick.
    int64_t spreadPeriod = TimeFromSeconds(desc->spreadMonths * SecsPerMonth());
    if (spreadPeriod >= spreadAccum._ticks) return;

    System::Random* rng = gw->m_random;
    // GameUpdateLoop::m_game (+0xF8) is still void* (GAME-a) and Map's cells are
    // untyped: the width and the cell reads below are LEFT.
    int* mapInts = reinterpret_cast<int*>(gw->m_game);

    for (;;) {
        spreadAccum._ticks -= spreadPeriod;

        MapComponent* mc = _entity->FindComponent<MapComponent>();   // 0x140004680

        // Retail loads the footprint with MOVSX: signed shorts.
        int width = *mapInts;
        int centre = ((static_cast<int16_t>(mc->tileY1) >> 1) + static_cast<int16_t>(mc->tileY0))
                     * width
                   + (static_cast<int16_t>(mc->tileX1) >> 1) + static_cast<int16_t>(mc->tileX0);

        int tx = centre % width + rng->RandTol(static_cast<int32_t>(desc->spreadRadius));
        int ty = centre / width + rng->RandTol(static_cast<int32_t>(desc->spreadRadius));
        if (tx < 0) tx = 0;
        if (ty < 0) ty = 0;
        if (tx >= width) tx = width - 1;
        if (ty >= width) ty = width - 1;

        // Cell array base is at map+0x80 (`piVar4 + 0x20` on an int* = 0x80 bytes).
        uintptr_t cells = *reinterpret_cast<uintptr_t*>((uintptr_t)mapInts + 0x80);
        ComponentEntity* const* cell = reinterpret_cast<ComponentEntity* const*>(
            cells + (uintptr_t)(width * ty + tx) * 0x30);

        ComponentEntity* target = nullptr;
        if (cell[0] != nullptr)      target = cell[0];     // zone raster wins
        else if (cell[1] != nullptr) target = cell[1];     // else entity raster

        const char* outcome = "empty-cell";
        if (target != _entity && target != nullptr) {
            if (target->FindComponent<DiseaseComponent>() != nullptr) {   // 0x1400050a0
                ComponentDescription* theirId = nullptr;
                ComponentDescription* myId = nullptr;
                DiseaseComp_GrownIdentity(&theirId, &target);
                DiseaseComp_GrownIdentity(&myId, &_entity);
                if (theirId == myId) {
                    target->FindComponent<DiseaseComponent>()->Infect();   // 0x1400050a0, 0x1400483a0
                    outcome = "SPREAD";
                } else {
                    outcome = "different-crop";
                }
            } else {
                outcome = "not-infestable";
            }
        } else if (target != nullptr) {
            outcome = "self";
        }

        if (CDTrace()) {
            printf("[CropDisease] SPREAD-TRY self=%p tile=(%d,%d) radius=%d -> %s\n",
                   (void*)this, tx, ty, (int)desc->spreadRadius, outcome);
            fflush(stdout);
        }

        if (spreadAccum._ticks <= spreadPeriod) return;
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
    : rollAccum{0}, duration{0}, killAccum{0}, spreadAccum{0},
      isPasture(0), _pad4C{}, curedIdentity(nullptr) {
    OrFlagsInCtor(0x8b);
}

void DiseaseComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    IO::Stream& s = *reinterpret_cast<IO::Stream*>(stream);   // B.3.8 bridge (Persist takes void*)
    IO::Serializer ser(s, 1);
    Reimpl_ExternalPtr_Serialize_StringTable(&curedIdentity, &ser);   // 0x140002340 ExternalPtr<ComponentDescription>
    ser.Serialize(isPasture, nullptr);
    rollAccum.Serialize(ser);     // 0x140152aa8 Time::Serialize
    duration.Serialize(ser);
    killAccum.Serialize(ser);
    spreadAccum.Serialize(ser);
    // Version-0 saves predate the registry, so re-run OnAdded (vtable +0x30 = slot 6).
    if (ser.GetVersion() == 0 && static_cast<int>(s.GetDirection()) == 0) {   // (IO::Stream::GetDirection, retail 0x180006200)
        OnAdded();   // slot 6, dispatched virtually as retail does
    }
}
void DiseaseComponent::Startup() {
    isPasture = (_entity->FindComponent<PastureComponent>() != nullptr) ? 1 : 0;   // 0x140004fe0

    if (CDTrace()) {
        printf("[CropDisease] STARTUP this=%p pasture=%d desc=%p entity=%p\n",
               (void*)this, (int)isPasture, _description, _entity);
        fflush(stdout);
    }
}
void DiseaseComponent::OnAdded() {
    GameUpdateLoop* gw = GameWorld();
    if (!gw) return;
    // GameUpdateLoop::m_citizenList (+0xF0) is still void* (GAME-a): the town's
    // registry array at +0xF0 is LEFT.
    uintptr_t town = reinterpret_cast<uintptr_t>(gw->m_citizenList);
    if (!town) return;
    int64_t* reg = reinterpret_cast<int64_t*>(town + 0xF0);

    ::Object::InternalPtr<DiseaseComponent> ibp(this);
    int count = *reinterpret_cast<int32_t*>((uintptr_t)reg + 8);
    ActHunt_DynArraySetCount_3110(reg, count + 1, &ibp);

    if (CDTrace()) {
        printf("[CropDisease] REGISTER this=%p registry-count=%d\n",
               (void*)this, count + 1);
        fflush(stdout);
    }
}
void DiseaseComponent::OnRemove() {
    GameUpdateLoop* gw = GameWorld();
    if (gw) {
        // m_citizenList is void* (GAME-a): the registry erase stays by offset (LEFT).
        uintptr_t town = reinterpret_cast<uintptr_t>(gw->m_citizenList);
        if (town) {
            ::Object::InternalPtr<DiseaseComponent> ibp(this);
            CemeteryListErase_Reimpl(reinterpret_cast<int64_t*>(town + 0xF0), &ibp);
        }
    }
    Cure();
}

// [9] 0x140048b90. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void DiseaseComponent::Update(void* time) {
    System::Time* deltaTime = static_cast<System::Time*>(time);
    if (duration._ticks > 0) ActiveDisease(deltaTime);
    else                     InfectionTick(deltaTime);
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

// ---- B4 FORWARD: 2 outside caller files (Dialog/debugui.cpp,
// standalone/disaster_harness.cpp); the closing wave rewires them.
extern "C" bool DiseaseComp_CheckCure(DiseaseComponent* self) { return self->CheckCure(); }
extern "C" void DiseaseComp_Infect(DiseaseComponent* self) { self->Infect(); }
