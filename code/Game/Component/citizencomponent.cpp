// code/Game/Component/citizencomponent.cpp
// Clean reimplementation of CitizenComponent methods from Application-x64.exe.
//
// CitizenComponent (0x290 bytes) is the largest AI component — full citizen state
// (health, happiness, inventory, mesh, etc). Contains a MeshInstance sub-object
// at +0xB8 (0x120 bytes).
//
// Two reimplemented functions:
//   Update (0x140112e70) — main lifecycle tick (711 instructions!)
//   ShouldSwitchWorkplace (0x1401109E0) — workplace switch decision
//
// Vtable at 0x140162580.
// Decompiled from Application-x64.exe using Ghidra.

// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "Component/StorageComponent.h"   // B.3.8 CB1a (TypeId)
#include "graphics/Mesh.h"
#include "Component/citizencomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "Component/InteractComponent.h"       // ENDGAME B.3.9 C1: 0x1401c7900 cell class
#include "Component/ResidenceComponent.h"      // ENDGAME B.3.9 C1: 0x1401c7cf0 cell class
#include "Component/ResidenceDescription.h"    // ENDGAME B.3.9 C1: 0x1401c7d28 cell class
#include "Component/WorkPlaceComponent.h"      // ENDGAME B.3.9 C1: 0x1401c80e0 cell class
#include "Component/WorkComponent.h"           // ENDGAME B.3.9 C1: 0x1401c8070 cell class
#include "Component/RawMaterialDescription.h"  // ENDGAME B.3.9 C1: 0x1401c7cb8 cell class
#include "Component/CitizenComponent.h"        // ENDGAME B.3.9 C1: 0x1401c7158 cell class
#include "Component/CharacterComponent.h"      // ENDGAME B.3.9 TR
#include "Component/StatusIconComponent.h"     // ENDGAME B.3.9 TR
#include "Component/AttendanceDescription.h"   // ENDGAME B.3.9 CB: 0x1401c3f78 cell class
#include "Component/AttendanceComponent.h"     // B.4 W1 COMP-b: typed attendance links
#include "Component/CitizenDescription.h"      // B.4 W1 COMP-b: description fields by name
#include "Component/ModelComponent.h"          // B.4 W1 COMP-b: CharacterComponent::modelComponent
#include "Entity/ComponentEntity.h"            // B.4 W1 COMP-b: FindComponent<T>
#include "Game/Profession.h"                   // B.4 W1 COMP-b: profession fields by name
#include "Game/Disease.h"                      // B.4 W1 COMP-b: disease fields by name
#include "Game/ProfessionList.h"               // B.4 W1 COMP-b: m_professionList fields
#include "Component/ComponentDescription.h"    // B.4 W1 COMP-b: the entity's description
#include "options.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Tutorial/exe_bodies.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include <cstdint>
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/String.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/StringTable.h"    // ENDGAME B.3.6 UI callers (K4)
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "scene_watch.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Entity/componententity_funcs.h"
#include "Game/exe_constants.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// ============================================================
// Resolved helpers for Update
// ============================================================

// CitizenComponent_TryEnrolInSchool(self, outIBP) -> char (bool)
// At 0x140111840. Ghidra called it TryFindPartner for months; it is school enrolment
// (desc+0x20 -> AttendanceComponent queue -> citizen+0x208). Renamed in Ghidra and here
// 2026-08-15 — the misnomer had already propagated into four files. The resolve site
// below is the authority on the address; the old "0x14010fe40" note here was a different
// function entirely (helper_fe40).

// CitizenComponent_PostLifeEvent(self, eventType, severity, strParam)
// At 0x140113af0 — posts a life-event message to the event log (gw+0x138).
// Formerly misnamed "AssignState" (it assigns nothing). Ported in
// citizen_lifeevent.cpp, registered by reg_eventlog_citizen.cpp.

// CitizenComponent_HandleLifeEvent(self, eventType, strParam) -> char (bool)
// At 0x140112450

// CitizenComponent_UpdateHealth(self, pTime) -> char (bool: 0=dead, nonzero=alive)
// At 0x140111b90

// CitizenComponent_ConsumeFood(self)
// At 0x140111890

// CitizenComponent_ConsumeFuel(self)
// At 0x140111a70

// CitizenComponent_FindCareBuilding(self) — the SICK citizen's hospital finder
// (desc+0x30 -> citizen+0x228), gated on the disease pointer citizen+0x68. Not a
// workplace finder despite the old name; see the resolve site below for the address.

// CitizenComponent_FindStorage(self)
// At 0x140115770


// --- Citizen status-icon driver (standalone wiring) ---
// The 5 health-flag call sites in Update (bit 0 no-workplace, 1 hungry, 2 cold,
// 3 unemployed/homeless, 5 has-profession) call CitizenStatusIcon_SetFlag, which in the
// original is FUN_14012c3f0 (the StatusIcon flag setter). That exe fn IS registered
// now (CitizenComp_FUN_14012c3f0, reg_component_people.cpp) — it is generic, keyed on
// the StatusIconComponent layout (mask +0xf0 / scene id +0x100), which is why the
// building-side callers pass a building's StatusIconComponent to the same address. We
// still route these five call sites to StatusIcon_Toggle — the port of FUN_14012c3f0 in
// statusiconcomponent.cpp — so the citizen's own StatusIconComponent (cached at
// CitizenComponent+0x280) actually shows hunger/cold/unemployed/homeless icons.
// Gated default-off (BANISHED_LIVE_CITIZEN_STATUSICON) so existing LIVE_CITIZEN(_UPDATE)
// runs stay byte-identical when unset. Depends on BANISHED_LIVE_STATUSICON + _DRAW for
// the citizen StatusIconComponent to be live + renderable (same infra as the building
// occupancy warning), and on LIVE_CITIZEN(_UPDATE) for the driving fields to tick.
class StatusIconComponent;
void StatusIcon_Toggle(StatusIconComponent* self, int bit, bool setFlag);

// Adapter for FUN_14012c3f0 (healthComp*, flagIndex, flagValue). Null-guards the cached StatusIconComponent IBP
// (CitizenComponent+0x280): a partial-world citizen lacking a live status-icon component
// has it null, and StatusIcon_Toggle dereferences self+0x10 immediately. The original
// never sees a null here (every citizen carries the component) — this guard is a
// standalone-only safety, not a behavior change.
static void CitizenStatusIcon_SetFlag(StatusIconComponent* icon, int flagIndex, char value) {
    if (icon)
        StatusIcon_Toggle(icon, flagIndex, value != 0);
}


// CitizenComponent_AssignWorkplace(self, workplaceIBP, slotIndex)
// At 0x140114ca0

// CitizenComponent_AssignHousing(self, housingPtr)
// At 0x140111ee0 (writes citizen+0x248). The address in this comment used to read
// 0x140114f80, which never matched the binding 12 lines below or the three other files
// that resolve this function (citizencomponent_lifecycle.cpp, citizen_assign_citizens.cpp
// x2) — all four agree on 0x140111ee0. Corrected 2026-08-15: a stale ADDRESS in a comment
// is the same trap as a wrong binding, just slower-acting.

// CitizenComponent_sub_ffc0 — find unassigned housing
// At 0x14010ffc0

// CitizenComponent_sub_0470 — find assigned housing
// At 0x140110470

// CitizenComponent_helper_ff80 — find new workplace (after ShouldSwitch)
// At 0x14010ff80

// CitizenComponent_Update_sub_0e70 — find work (workplace search)
// At 0x140110e70

// CitizenComponent_sub_1fa0 — release current workplace assignment
// At 0x140111fa0

// CitizenComponent_helper_fe40 — find partner
// At 0x14010fe40

// CitizenComponent_helper_1b90 (0x140111b90–0x140111ecb) — CHILDBIRTH. It opens with a
// scan over the citizen list (partner link +0x250 empty, age +0x3c <= desc+0x4c) but
// that scan only BIASES THE NEWBORN'S GENDER toward the scarcer sex; the body then
// allocates the child entity. Update calls it on the COMMON branch of the delivery roll
// (see the LAB_140113873 block) — the rare branch is death in childbirth.
// NOT UpdateHealth — UpdateHealth is 0x1401127d0, ported in citizen_updatehealth.cpp
// (disambiguated 2026-07-19; branch semantics corrected 2026-07-26).

// CitizenComponent_helper_a4f0 — check storage has capacity for item type
// At 0x14012a4f0

// Entity_FindComponent_Storage(entity, outIBP) -> InternalBasePtr*
// At 0x140008090 (or 0x140006270 depending on which variant)

// UI::StringTable::GetText — use the typedef from engine_funcs.h
// Signature: (self, retval, textIndex) -> void
// We use the engine_funcs.h extern directly: fn_StringTable_GetText

// (removed: UIString_ToSysString — handled inline via dispatch slot)

// Float constants from .rdata (Game/exe_constants.h): 0x14017225c hunger divisor,
// 0x140172234 thirst/time multiplier, 0x140172114 hunger threshold fraction,
// 0x140172100 thirst threshold fraction, 0x1401722c0 happiness-timer XOR mask.
static inline float RdataF(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}


// ============================================================
// CitizenComponent::Update (vtable slot [9], 0x140112e70)
// ============================================================
// Main citizen lifecycle tick. 711 instructions in original.
// Handles aging, hunger/thirst accumulation, health/death checks,
// food/fuel consumption, workplace assignment, housing, partner
// finding, and child-bearing logic.
//
// Parameters: RCX = this (CitizenComponent*), RDX = Time* (pointer to Time).
// RDX is a pointer to the elapsed System::Time (EntityList bucket tick 0x1400c42af).
//
// Phase 3 / Keystone D — CitizenComponent UPDATE live-wiring (work/economy loop).
// The per-tick lifecycle Update drives the world-state that gates the citizen Work
// state: a citizen only gets Work (id 5) as a candidate once it is HOUSED and/or
// EMPLOYED — both assigned here.
// Crash-safe guards bail until the body is populated + the world singleton is live.
void CitizenComponent::Update(void* timeArg) {
    // GUARD: Update derefs the CitizenDescription (+0x288, set by BodyInit), the
    // owner entity (+0x10), and the GameWorld weather (+0x130). Bail until ready.
    if (description == nullptr) return;
    if (_entity == nullptr) return;
    if (GameUpdateLoop::_singleton == nullptr) return;

    const System::Time* deltaTime = static_cast<const System::Time*>(timeArg);
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    // +0x130 is not the Weather resource (B4_RULES correction 5): a void* member, its
    // pointee read stays raw.
    uint8_t* weatherData = reinterpret_cast<uint8_t*>(gameWorld->m_weather);
    CitizenDescription* desc = description;

    // Convert Time to seconds
    float dt_sec = deltaTime->AsSeconds();
    int64_t dt_ticks = deltaTime->_ticks;

    // Check if citizen is adult (age >= adult threshold) and not in education
    float age = this->age;
    float adultAge = desc->adultAge;
    bool isAdult;
    if (age < adultAge || school != nullptr) {
        isAdult = false;
    } else {
        isAdult = true;
    }

    // --- Aging: increment age ---
    float agingRate = desc->agingRate;
    float newAge = age + dt_sec * agingRate;
    this->age = newAge;

    // --- Accumulate timers ---
    foodTimer += dt_ticks;        // food timer
    happinessTimer += dt_ticks;   // happiness timer
    clothingTimer -= dt_ticks;    // clothing wear timer
    if (school != nullptr) {
        schoolTime += dt_ticks;   // school timer
    }
    field_0x200 += dt_ticks;      // general timer
    field_0x60 += dt_ticks;       // work timer

    // Clothing worn out: back to no clothing
    if (clothingTimer < 0) {
        clothingLevel = 0;
        clothingTimer = 0;
    }

    // --- Adult transition: post became-adult event (10 = found partner, 9 = alone) ---
    if (!isAdult && adultAge <= newAge && school == nullptr) {
        int eventType;
        if (!field_0x220) {
            // Not yet had partner — try to find one
            char found = TryEnrolInSchool(nullptr);
            eventType = 10;
            if (found == 0) {
                eventType = 9;
            }
        } else {
            eventType = 9;
        }
        PostLifeEvent(eventType, 0, nullptr);
    }

    // --- Education cap: update max education from description ---
    uint32_t gameFlags = gameWorld->m_gameFlags;
    if ((gameFlags >> 9 & 1) != 0) {
        bool canEducate = (adultAge <= newAge && school == nullptr)
                          || (school != nullptr);
        if (canEducate) {
            int maxEdu = desc->maxEducation;
            if (education < maxEdu) {
                education = maxEdu;
            }
        }
    }

    // --- Mortality check: if sick + has a care building, zero hunger and coldness ---
    // (retail: one qword store of 0 over +0x44/+0x48; H7: the two named fields)
    if (disease != nullptr && careBuilding != nullptr) {
        hunger = 0.0f;
        coldness = 0.0f;
    }

    // --- Hunger accumulation ---
    if ((gameFlags >> 1 & 1) == 0) {
        float hungerRate = desc->hungerRate;
        float hungerDiv = RdataF(kRdata_14017225c);
        hunger += (dt_sec * hungerRate) / hungerDiv;
    } else {
        hunger = 0.0f;
    }

    // --- Thirst accumulation ---
    float fVar14 = kRdata_140172234;
    float thirstThreshTemp = desc->warmTemperature;
    float weatherTemp = *reinterpret_cast<float*>(weatherData + 0x18);
    if ((gameFlags >> 2 & 1) != 0 || thirstThreshTemp < weatherTemp || thirstThreshTemp == weatherTemp) {
        coldness = 0.0f;
    } else {
        // Thirst rate depends on clothing level (field at +0x1E4)
        float thirstRate = desc->coldRate[clothingLevel];
        coldness += (dt_sec * thirstRate) / fVar14;
    }

    // --- Handle life event from profession name ---
    Profession* profRef = pendingProfession;
    if (profRef != nullptr) {
        // Build UI::String from profession name, convert to System::String
        uint8_t localStr[16] = {};
        ::new (localStr) UI::String(
            Object::ExternalPtr<UI::StringTable>(profRef->stringTable),
            *reinterpret_cast<const System::String*>(&profRef->stringTool));
        // UI::String -> System::String, then HandleLifeEvent(7, str). The original
        // converts via dispatch slot [0x140151308] (UI::String::operator System::String,
        // CALL @1401130ab). [WRONG-SLOT FIX] the previous port read slot 0x140152878,
        // which is Achievement::GoalList::AreGoalsMetExcept — unregistered in
        // standalone, so the conversion silently no-opped and localRes8 stayed
        // empty. Use the resolved engine export directly (retval into localRes8).
        System::String localRes8;
        ::new (&localRes8) System::String(
            reinterpret_cast<const UI::String*>(localStr)->operator System::String());  // B.3.6 bridge

        // Copy construct System::String from localStr result
        System::String localRes18(localRes8);
        char* strPtr = localRes18._data;

        char eventResult = HandleLifeEvent(7, strPtr);

        if (eventResult != 0) return;
    }

    // --- Hunger cap: clamp and check death ---
    float hungerCap = desc->maxHunger;
    if (hungerCap <= hunger) {
        hunger = desc->maxHunger;
        if (character->currentAction + 1 < 1) {
            char r = HandleLifeEvent(0, nullptr);
            if (r != 0) return;
        }
    }

    // --- Thirst cap: clamp and check death ---
    float thirstCap = desc->maxColdness;
    if (thirstCap <= coldness) {
        coldness = desc->maxColdness;
        if (character->currentAction + 1 < 1) {
            char r = HandleLifeEvent(1, nullptr);
            if (r != 0) return;
        }
    }

    // --- Old age death check ---
    if (maxAge <= this->age) {
        char r = HandleLifeEvent(2, nullptr);
        if (r != 0) return;
    }

    // --- Health update ---
    char healthOk = UpdateDisease(deltaTime);
    if (healthOk == 0) {
        // Citizen died from health — get death cause string
        System::String localRes8;
        Disease* profData = disease;
        if (profData) {
            void* stringTable = profData->_stringTable;
            {
                // GetText(self, retval, textIndex) — Ghidra drops R8 (textIndex)
                // textIndex is likely 0 for single-entry profession name tables
                ::new (&localRes8) System::String(
                    reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(0));  // B.3.6 bridge
                char* deathStr = localRes8._data;
                char r = HandleLifeEvent(3, deathStr);
                if (r != 0) return;
            }
        }
    }

    // --- Sick? find a care building (+0x68 is the DISEASE pointer, not a profession or
    // a housed flag — see the note on FindCareBuilding) ---
    if (disease != nullptr) {
        happinessFlags |= 1;
        if (careBuilding == nullptr) {
            FindCareBuilding();
        }
    }

    // --- Find storage if needed ---
    if (field_0x230 == nullptr) {
        FindStorage();
    }

    // --- Health status flags (via the StatusIconComponent at +0x280) ---
    ComponentEntity* workplaceRef = home;   // +0x238: the home

    CitizenStatusIcon_SetFlag(statusIcon, 0, workplaceRef == nullptr);

    // Hunger warning flag
    float hungerThreshLow = desc->_fcc;
    float hungerThreshHigh = desc->maxHunger;
    float fVar4 = kRdata_140172114;
    float hungerWarnLevel = (hungerThreshHigh - hungerThreshLow) * fVar4 + hungerThreshLow;
    float curHunger = hunger;
    CitizenStatusIcon_SetFlag(statusIcon, 1, hungerWarnLevel < curHunger);

    // Thirst warning flag
    float thirstThreshLow = desc->_fec;
    float thirstThreshHigh = desc->maxColdness;
    float fThirstFrac = kRdata_140172100;
    float thirstWarnLevel = (thirstThreshHigh - thirstThreshLow) * fThirstFrac + thirstThreshLow;
    float curThirst = coldness;
    CitizenStatusIcon_SetFlag(statusIcon, 2, thirstWarnLevel < curThirst);

    // Unemployed flag
    char isUnemployed;
    Profession* profData1F0 = profession;
    // [STANDALONE GUARD] +0x1F0 = profession object, null until assigned at gameplay
    // (same field CitizenAI CheckEducation null-guards). In retail EVERY citizen —
    // children included — is stamped with the default profession at spawn
    // (FUN_140117080 → ProfessionList+0x30), and that object's +0x4C bool governs the
    // "unemployed" icon (flag 3). A null pointer here is a standalone-only gap, NOT a
    // genuine "no profession". The old fallback defaulted profByte=0 (== unemployed),
    // which mislabelled EVERY not-yet-employed citizen. Children/students can never hold
    // a workplace (+0x248 stays 0 for them), so that fallback pinned a permanent "?"
    // no-job badge on every child. Fall back on age instead: adults with no profession
    // stamped still read as unemployed (they legitimately need a job), but children
    // (age < adultAge) and students (in-school, +0x208 != 0) never do — which is what
    // isAdult already encodes (computed at the top of Update). With a real profession
    // present (stamped SA) this branch is skipped, so retail behaviour is
    // byte-identical.
    char profByte = (profData1F0 != nullptr) ? profData1F0->field4C
                                             : (isAdult ? 0 : 1);
    if (profByte == 0 && workplace == nullptr) {
        isUnemployed = 1;
    } else {
        isUnemployed = 0;
    }
    CitizenStatusIcon_SetFlag(statusIcon, 3, isUnemployed);

    // Flag 5: sick (the disease pointer)
    CitizenStatusIcon_SetFlag(statusIcon, 5, disease != nullptr);

    // --- Hunger/thirst urgency bit flags at +0x54 and +0x88 ---
    desc = description;  // re-read (may have changed)
    hungerThreshLow = desc->_fcc;
    hungerThreshHigh = desc->maxHunger;
    if ((hungerThreshHigh - hungerThreshLow) * fVar4 + hungerThreshLow < hunger) {
        statusFlags |= 1;
        happinessFlags |= 1;
    }
    thirstThreshLow = desc->_fec;
    thirstThreshHigh = desc->maxColdness;
    if ((thirstThreshHigh - thirstThreshLow) * fThirstFrac + thirstThreshLow < coldness) {
        statusFlags |= 2;
        happinessFlags |= 1;
    }

    // --- Food consumption loop ---
    {
        float eatInterval = desc->foodInterval * fVar14;
        System::Time eatTime{};
        RtSystem_Time_Ctor(&eatTime, eatInterval);
        int64_t eatTicks = eatTime._ticks;
        if (eatTicks > 0) {
            while (eatTicks < foodTimer) {
                foodTimer -= eatTicks;
                UpdateHealth();
            }
        }
    }

    // --- Happiness loop ---
    {
        float fuelInterval = desc->happinessInterval * fVar14;
        System::Time fuelTime{};
        RtSystem_Time_Ctor(&fuelTime, fuelInterval);
        int64_t fuelTicks = fuelTime._ticks;
        if (fuelTicks > 0) {
            while (fuelTicks < happinessTimer) {
                happinessTimer -= fuelTicks;
                UpdateHappiness();
            }
        }
    }

    // --- The home's ResidenceComponent ---
    ResidenceComponent* localRes10 = nullptr;
    if (workplaceRef != nullptr) {
        localRes10 = home->FindComponent<ResidenceComponent>();
    }

    // [PORT FIX] The original reassigns pCVar1 = param_1 + 0x278 (CharacterComponent)
    // here — the prior pCVar1 (+0x280, StatusIcon/health) was only for the HealthSetFlag
    // calls above. The workplace/housing-assignment gates below test the citizen's
    // CharacterComponent ACTION-STACK top (+0x60): `+0x60 + 1 < 1` ⇔ stack empty (idle)
    // ⇔ eligible to be (re)assigned. The reimpl had kept pCVar1 = +0x280, so every gate
    // read the inert StatusIcon's +0x60 (== 0 in standalone) and ALWAYS skipped assignment
    // → citizens never got housed/employed. Restore the original component.

    // --- Validate the home ---
    if (character->currentAction + 1 < 1 && workplaceRef != nullptr) {
        char valid = CanReach(workplaceRef);
        if (valid == 0) {
            // [DIAG] This is the ONLY writer that clears a citizen's home (+0x238) in
            // normal play, and in the stress town it fired for ~8% of the housed
            // population per meal cycle; the homeless then never eat and starve. Print
            // both cells' kind (+0x29) and region id (+0x26) so a stale region grid, a
            // wrong footprint cell and a citizen standing off-map stop looking alike.
            static int s_hiN = 0;
            if (s_hiN < 80) { ++s_hiN;
                int* mapInfo = reinterpret_cast<int*>(gameWorld->m_game);
                const uint8_t* tiles = mapInfo ? *reinterpret_cast<uint8_t* const*>(
                    reinterpret_cast<const uint8_t*>(mapInfo) + 0x80) : nullptr;
                int mapW = mapInfo ? *mapInfo : 0, half = mapW >> 1;
                CharacterComponent* ch = character;
                int64_t model = ch ? reinterpret_cast<int64_t>(ch->modelComponent) : 0;
                float cx = model ? *reinterpret_cast<float*>(model + 0x158) : -9999.f;
                float cy = model ? *reinterpret_cast<float*>(model + 0x15c) : -9999.f;
                int tx = (int)(cx + (float)half), ty = (int)(cy + (float)half);
                if (tx < 0) tx = 0; if (tx >= mapW) tx = mapW - 1;
                if (ty < 0) ty = 0; if (ty >= mapW) ty = mapW - 1;
                uint32_t czCell = (uint32_t)(mapW * ty + tx);
                ComponentEntity* homeIBP = workplaceRef;
                int homeCell = Pf_EntityAnchorCell_7fb0(&homeIBP, nullptr);
                unsigned sk = 0, sr = 0, dk = 0, dr = 0;
                if (tiles && mapW > 0) {
                    const uint8_t* ts = tiles + (size_t)czCell * 0x30;
                    sk = ts[0x29]; sr = *reinterpret_cast<const uint16_t*>(ts + 0x26);
                    if (homeCell >= 0 && homeCell < mapW * mapW) {
                        const uint8_t* td = tiles + (size_t)homeCell * 0x30;
                        dk = td[0x29]; dr = *reinterpret_cast<const uint16_t*>(td + 0x26);
                    }
                }
                const char* nm = *reinterpret_cast<const char**>(&name);
                fprintf(stderr, "[HomeInvalid] citizen=%p name=%s pos=(%.2f,%.2f) cell=%u(%d,%d) kind=%u region=%u"
                                " | home=%p cell=%d(%d,%d) kind=%u region=%u | age=%.2f hunger=%.1f\n",
                        (void*)this, nm ? nm : "?", cx, cy, czCell, tx, ty, sk, sr,
                        (void*)workplaceRef, homeCell, homeCell >= 0 && mapW ? homeCell % mapW : -1,
                        homeCell >= 0 && mapW ? homeCell / mapW : -1, dk, dr,
                        this->age, hunger);
                // First three hits: ASCII window of the tile grid around the citizen so a
                // fragmented flood shows itself. Row = y, col = x; 'C' citizen, 'H' home cell;
                // kinds outside the walk mask print as '#' (5-8) or '~' (12+); walkable cells
                // print their region id in base-36 (mod 36).
                if (s_hiN <= 3 && tiles && mapW > 0) {
                    const int R = 24;
                    fprintf(stderr, "[HomeInvalid] region window x=%d..%d y=%d..%d (kind/region)\n",
                            tx - R, tx + R, ty - 12, ty + 12);
                    for (int yy = ty - 12; yy <= ty + 12; ++yy) {
                        char line[2 * R + 2]; int li = 0;
                        for (int xx = tx - R; xx <= tx + R; ++xx) {
                            char c = ' ';
                            if (xx >= 0 && xx < mapW && yy >= 0 && yy < mapW) {
                                const uint8_t* t = tiles + (size_t)(yy * mapW + xx) * 0x30;
                                unsigned k = t[0x29], r = *reinterpret_cast<const uint16_t*>(t + 0x26);
                                if (xx == tx && yy == ty) c = 'C';
                                else if (homeCell >= 0 && xx == homeCell % mapW && yy == homeCell / mapW) c = 'H';
                                else if (((1u << (k & 0x1f)) & 0xe1fu) == 0) c = (k >= 5 && k <= 8) ? '#' : '~';
                                else if (r == 0) c = '.';
                                else { unsigned d = r % 36; c = d < 10 ? (char)('0' + d) : (char)('a' + d - 10); }
                            }
                            line[li++] = c;
                        }
                        line[li] = 0;
                        fprintf(stderr, "[HomeInvalid] y=%3d |%s|\n", yy, line);
                    }
                    // Kind map of the same window.
                    for (int yy = ty - 12; yy <= ty + 12; ++yy) {
                        char line[2 * R + 2]; int li = 0;
                        for (int xx = tx - R; xx <= tx + R; ++xx) {
                            char c = ' ';
                            if (xx >= 0 && xx < mapW && yy >= 0 && yy < mapW) {
                                const uint8_t* t = tiles + (size_t)(yy * mapW + xx) * 0x30;
                                unsigned k = t[0x29];
                                c = k < 10 ? (char)('0' + k) : (char)('a' + k - 10);
                                if (xx == tx && yy == ty) c = 'C';
                            }
                            line[li++] = c;
                        }
                        line[li] = 0;
                        fprintf(stderr, "[HomeInvalid] kind y=%3d |%s|\n", yy, line);
                    }
                }
                fflush(stderr);
            }
            SetHome(nullptr, -1);
        }
    }

    // --- Check: should a child / student follow its parent home ---
    newAge = this->age;
    adultAge = description->adultAge;
    bool isChildOrStudent = (newAge < adultAge) || (school != nullptr);
    if (isChildOrStudent) {
        if (character->currentAction + 1 < 1) {
            // No home, check the parent (+0x258)
            if (workplaceRef == nullptr && _parent != nullptr) {
                if (_parent->home != nullptr) {
                    char valid = CanReach(_parent->home);
                    if (valid != 0) {
                        int slotIdx = _parent->homeSlot;
                        SetHome(_parent->home, slotIdx);
                    }
                }
            }
        }
    }

    // --- Adult section: housing, workplace, partner ---
    // NOTE: skipped for children — start-town citizens currently spawn with age=0
    // (< adultAge), so this whole assignment section no-ops until they age up or are
    // spawned as adults. See project_economy_loop memory.
    // The life-planning section (housing, workplace, partner) is gated on THREE things,
    // and the census can only see two of them. The third is the AI action index at
    // character+0x60: retail runs the whole section only while it is -1 (no action in
    // flight).
    if (isChildOrStudent || (character->currentAction + 1 >= 1))
        goto LAB_140113873;

    // --- Workplace assignment ---
    if (workplace == nullptr) {
        ComponentEntity* housingResult = nullptr;
        profData1F0 = profession;
        // [STANDALONE GUARD] +0x1F0 profession-list null until assigned → treat the
        // +0x4C byte as 0 (unassigned profession) — same guard as the unemployed flag.
        char profByte634 = (profData1F0 != nullptr) ? profData1F0->field4C : 0;
        if (profByte634 == 0) {
            // Unranked profession
            housingResult = FindWorkplace();
        } else {
            housingResult = FindRankedWorkplace();
        }
        SetWorkplace(housingResult);
    } else {
        // Check if should switch workplace (already reimplemented)
        bool shouldSwitch = ShouldSwitchWorkplace();
        if (shouldSwitch) {
            SetWorkplace(FindNewWorkplace());
        }
    }

    // --- Find a home if none ---
    if (workplaceRef == nullptr) {
        happinessFlags |= 0x20;
        goto FindWorkLabel;
    } else if (_parent != nullptr) {
FindWorkLabel:
        {
            int32_t slotIdx = -1;
            ComponentEntity* found = FindHome(&slotIdx, workplaceRef == nullptr);
            if (found != nullptr) {
                if (_parent != nullptr) {
                    happinessFlags |= 0x2000;
                }
                SetParent(nullptr);

                SetHome(found, slotIdx);

                // Also move the spouse
                if (spouse != nullptr) {
                    spouse->SetHome(found, slotIdx);
                }

                // Re-find the ResidenceComponent of the new home
                workplaceRef = home;
                if (workplaceRef != nullptr) {
                    localRes10 = workplaceRef->FindComponent<ResidenceComponent>();
                }
            }
        }
    }

    // --- Shared home: find a home of our own if the residence holds more households ---
    workplaceRef = home;
    if (workplaceRef != nullptr) {
        ResidenceComponent* storageVal = localRes10;
        // Faithful-equivalent guard: the original reads residence+0x58 (the household
        // slot count) UNGUARDED, relying on the invariant that an assigned home (+0x238)
        // always carries a ResidenceComponent (c7cf0). In standalone a force-assigned /
        // partially-built home can lack it (FindComponent -> null), so guard the null
        // before the read.
        if (storageVal != nullptr && storageVal->residentSlotCount > 1) {
            int32_t slotIdx = -1;
            ComponentEntity* found = FindHome(&slotIdx, false);
            if (found != nullptr) {
                SetHome(found, slotIdx);
                if (spouse != nullptr) {
                    spouse->SetHome(found, slotIdx);
                }
            }
        }
    }

    // --- Partner finding: check if eligible ---
    if (_parent == nullptr && workplaceRef != nullptr) {
        if (spouse == nullptr) {
            desc = description;
            float partnerAge = desc->marriageAge;
            if (partnerAge <= this->age) {
                CitizenComponent* partner = FindSpouse();
                spouse = partner;
                if (partner != nullptr) {
                    happinessFlags |= 0x200;
                    spouse->happinessFlags |= 0x200;

                    // The spouse leaves its parent
                    spouse->SetParent(nullptr);

                    // Set the spouse's spouse reference to this citizen
                    spouse->spouse = this;

                    // The spouse moves into this citizen's home
                    spouse->SetHome(workplaceRef, homeSlot);
                }
            }
        }
    }

LAB_140113873:
    // --- Reproduction section (women of childbearing age) ---
    if (gender == 0) {
        desc = description;
        adultAge = desc->adultAge;
        newAge = this->age;
        if (adultAge <= newAge && school == nullptr) {
            float hungerWarn = (desc->maxHunger - desc->_fcc) * fVar4 + desc->_fcc;
            float thirstWarn = (desc->maxColdness - desc->_fec) * fThirstFrac + desc->_fec;

            bool isHungryOrThirsty = (hungerWarn < hunger) || (thirstWarn < coldness);

            // Reset the pregnancy timer if hungry/thirsty and it was positive
            if (isHungryOrThirsty && pregnancyTimer > 0.0f) {
                pregnancyTimer = 0.0f;
            }

            float happiness = pregnancyTimer;
            if (happiness <= 0.0f) {
                if (happiness < 0.0f) {
                    float newHap = happiness + dt_sec;
                    pregnancyTimer = newHap;
                    if (newHap >= 0.0f) {
                        pregnancyTimer = 0.0f;
                    }
                }
            } else {
                float newHap = happiness - dt_sec;
                pregnancyTimer = newHap;
                if (newHap <= 0.0f) {
                    // ---------------------------------------------------------------
                    // GESTATION TIMER EXPIRED — this is the DELIVERY, not a "try for a
                    // baby" roll. [NAMING CORRECTION 2026-07-26, from the 0x140113923
                    // disassembly.] The old comments here had the two branches exactly
                    // backwards and cost a whole session's premise ("zero births").
                    //
                    //   +0x4C > 0   gestation, counting down (seeded to desc+0x58 = 900
                    //               by the conception gate below: married + housed +
                    //               childbearing age + household has room)
                    //   +0x4C == 0  fires HERE
                    //   +0x4C < 0   post-partum recovery, counting back up to 0
                    //
                    // The roll decides whether the MOTHER SURVIVES the birth:
                    //   pDeath = (health/maxHealth) * (desc+0x64 - desc+0x68) + desc+0x68
                    // measured live: desc+0x68 = 0.0250 (at health 0), desc+0x64 = 0.0063
                    // (at health 10) — i.e. a sick mother is ~4x likelier to die. That
                    // monotonicity is the proof this is a mortality roll and not a
                    // fertility one; a fertility chance that FELL with health would be
                    // nonsense. So +0x50 is HEALTH (max = desc+0x70 = 10), not happiness.
                    //
                    //   roll < pDeath  (rare)   -> HandleLifeEvent(6, null), which
                    //                              is the DEATH handler (tombstone +
                    //                              queue-for-removal). Died in childbirth.
                    //   roll >= pDeath (common) -> GiveBirth (allocates the newborn
                    //                              entity), then seeds the post-partum
                    //                              cooldown.
                    // ---------------------------------------------------------------
                    float health = this->health;
                    float maxHealth = desc->maxHealth;
                    float deathChanceAtMaxHealth = desc->_f64;
                    float deathChanceAtZeroHealth = desc->_f68;

                    float roll = gameWorld->m_random->RandFloat();

                    float pMaternalDeath = (health / maxHealth) *
                                           (deathChanceAtMaxHealth - deathChanceAtZeroHealth) +
                                           deathChanceAtZeroHealth;
                    if (pMaternalDeath <= roll) {
                        // Mother survives: deliver the baby.
                        GiveBirth();

                        // Post-partum recovery interval.
                        desc = description;
                        float roll2 = GameUpdateLoop::_singleton->m_random->RandFloat();
                        float timerBase = desc->_f5c;
                        float timerRange = desc->_f60;
                        // The post-birth cooldown is stored NEGATIVE: retail is
                        //   XORPS XMM0, [0x1401722c0]   (0x80000000 x4 = the float sign mask)
                        // i.e. `+0x4c = -(rand*range + base)`, which the `fVar14 < 0` branch
                        // above then counts back up toward 0. The decompiler renders that as
                        // `(uint)(...) ^ DAT_1401722c0` and this port had taken it literally:
                        // a numeric float->int truncation, so e.g. 12.7 became 0x0000000C,
                        // and flipping bit 31 of THAT yields -1.7e-44 — a cooldown that
                        // expires on the very next tick. Flip the sign bit of the float's
                        // BITS, not of a truncated integer.
                        float cooldown = roll2 * timerRange + timerBase;
                        uint32_t bits;
                        std::memcpy(&bits, &cooldown, sizeof(bits));
                        bits ^= kRdata_1401722c0;
                        std::memcpy(&pregnancyTimer, &bits, sizeof(bits));
                    } else {
                        // Mother died in childbirth — event type 6. 0x140116dd0 is the
                        // DEATH handler (tombstone + queue-for-removal), so no baby is
                        // produced on this branch and Update returns immediately.
                        char r = HandleLifeEvent(6, nullptr);
                        if (r != 0) return;
                    }
                }
            }

            // --- CONCEPTION GATE: married + housed + idle timer + childbearing age +
            //     household has room  ->  seed the gestation timer (+0x4C = desc+0x58) ---
            workplaceRef = home;
            if (spouse != nullptr && workplaceRef != nullptr && pregnancyTimer == 0.0f) {
                desc = description;
                float childMinAge = desc->minPregnancyAge;
                float childMaxAge = desc->maxPregnancyAge;
                newAge = this->age;
                if (childMinAge <= newAge && newAge <= childMaxAge) {
                    // Is the household full?
                    ResidenceComponent* storComp = home->FindComponent<ResidenceComponent>();
                    {
                        // [NAMING CORRECTION 2026-07-26] This is not a storage query at all.
                        // 0x14012a4f0 is `ResidenceDescription(+0x18)->+0x24 <=
                        // residence->slot[idx].count` — i.e. **is this household FULL?** —
                        // and the index is the citizen's own residence slot (+0x240), not an
                        // item type. So the timer starts when the household has ROOM, which is
                        // the sensible reading; the old `hasCapacity` name inverted it and made
                        // this branch look like a bug. desc+0x24 is the per-household OCCUPANT
                        // cap (distinct from desc+0x20, the household SLOT count).
                        int slotIdx = homeSlot;
                        bool householdFull = storComp->IsFamilyFull(slotIdx);   // 0x14012a4f0
                        if (!householdFull) {
                            // Room for another occupant — start the pregnancy timer.
                            pregnancyTimer = desc->pregnancyTime;
                        }
                    }
                }
            }
        }
    }
}

// ============================================================
// BST node layout for the profession registry tree
// (gameWorld+0xF0 -> +0x100 = tree root)
// ============================================================
// node[0] = key (uint64_t, matches citizen->data1F0)
// node[1] = array_ptr (pointer to array of entity IBPs)
// node[2] = count (int, number of jobs)
// node[3] = padding
// node[4] = left child
// node[5] = right child

// ============================================================
// BST node layout for the region map tree
// (gameWorld+0x118 -> +0x30 = tree root)
// ============================================================
// node[0] = key (int32_t, region ID)
// node[1] = value (pointer to region descriptor)
// node[2] = padding
// node[3] = left child (at int offset +6 = byte offset +0x18)
// node[4] = right child (at int offset +8 = byte offset +0x20)

// ============================================================
// BST lookup: profession registry (uint64 key)
// Returns count of jobs, or 0 if not found.
// ============================================================
static int LookupProfessionJobCount(uintptr_t registryBase, uint64_t key) {
    uintptr_t* node = *(uintptr_t**)(registryBase + 0x100);
    while (node) {
        uint64_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[4]; // left
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[5]; // right
        } else {
            // Found
            return (int)node[2];
        }
    }
    return 0; // not found
}

// Get job array entry from profession registry
static uintptr_t GetJobEntity(uintptr_t registryBase, uint64_t key, int64_t index) {
    uintptr_t* node = *(uintptr_t**)(registryBase + 0x100);
    while (node) {
        uint64_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[4];
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[5];
        } else {
            // Found — read array[index]
            uintptr_t* arr = (uintptr_t*)node[1];
            return arr[index];
        }
    }
    return 0;
}

// ============================================================
// BST lookup: region map (int32 key)
// Returns pointer to region descriptor, or 0 if not found.
// Tree nodes: int key at +0, value ptr at +8, left at +0x18, right at +0x20
// ============================================================
static uintptr_t LookupRegionByID(uintptr_t areaMap, int32_t regionId) {
    int* node = *(int**)(areaMap + 0x30);
    while (node) {
        int nodeKey = node[0];
        if (regionId < nodeKey) {
            node = *(int**)(node + 6); // left (+0x18)
        } else if (regionId > nodeKey) {
            node = *(int**)(node + 8); // right (+0x20)
        } else {
            // Found — value at node+8 (qword)
            return *(uintptr_t*)(node + 2);
        }
    }
    return 0; // not found
}

// ============================================================
// CitizenComponent_ShouldSwitchWorkplace
// Original: 0x1401109E0 (CitizenComponent_helper_09e0)
// ============================================================
bool CitizenComponent::ShouldSwitchWorkplace() {
    void* gameWorld = GameUpdateLoop::_singleton;
    uintptr_t gwAddr = (uintptr_t)gameWorld;

    // Read citizen's profession (the profession registry's key)
    uint64_t profKey = reinterpret_cast<uint64_t>(profession);   // CAST-OK: ptr_int
    // [STANDALONE GUARD] profession-list null until assigned → no workplace to switch.
    if (profKey == 0) return false;

    // Lookup job count from profession registry
    uintptr_t registry = *(uintptr_t*)(gwAddr + 0xF0);
    int jobCount = LookupProfessionJobCount(registry, profKey);

    // Read workplace entity flag at +0x4C
    uint8_t workplaceFlag = profession->field4C;

    if (workplaceFlag != 0) {
        // ====== ASSIGNED PATH ======
        // Citizen has a specific workplace assignment.
        // Check if another workplace in a lower-priority region has vacancy.
        uintptr_t areaMap = *(uintptr_t*)(gwAddr + 0x118);

        // Get current workplace IBP and find StorageLocationComponent
        WorkComponent* storageLocComp = workplace->FindComponent<WorkComponent>();
        int32_t regionId = storageLocComp->workId;

        // Look up current region in region map BST
        uintptr_t regionDesc = LookupRegionByID(areaMap, regionId);

        // *** FIX: null check for region lookup failure ***
        int currentPriority;
        if (regionDesc == 0) {
            // Region not in tree — don't switch, stay at current workplace.
            // Original game crashes here (null deref). Returning false keeps
            // citizens working instead of endlessly reshuffling.
            return false;
        } else {
            currentPriority = *(int32_t*)(regionDesc + 0x24);
        }

        // Check if current workplace is valid (reachable)
        char valid = CanReach(workplace);
        if (!valid) {
            return true; // workplace not reachable -> should switch
        }

        // Iterate all available jobs looking for a better workplace
        for (int64_t i = 0; i < (int64_t)jobCount; i++) {
            uintptr_t entityIBP = GetJobEntity(registry, profKey, i);

            // CheckPtr + FindWorkplaceComponent
            uintptr_t workplaceCompIBP = 0;
            AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entityIBP), &workplaceCompIBP);

            // CheckPtr + FindStorageLocationComponent
            uintptr_t otherStorageLocIBP = 0;
            WorkBase_FindComponent_3050(entityIBP, &otherStorageLocIBP);

            // Skip if this is the current workplace
            if (entityIBP == reinterpret_cast<uintptr_t>(workplace))
                continue;

            // Check workplace is active (+0x50 flag)
            uintptr_t wpComp = workplaceCompIBP;
            if (*(uint8_t*)(wpComp + 0x50) == 0)
                continue;

            // Check has vacancy
            if (!fn_CitizenComponent_HasVacancy(workplaceCompIBP))
                continue;

            // Look up this workplace's region (re-read areaMap fresh, matches original asm)
            int32_t otherRegionId = *(int32_t*)(otherStorageLocIBP + 0x28);
            uintptr_t freshAreaMap = *(uintptr_t*)((uintptr_t)GameUpdateLoop::_singleton + 0x118);
            uintptr_t otherRegionDesc = LookupRegionByID(freshAreaMap, otherRegionId);

            // Null check on region lookup (original would crash)
            if (otherRegionDesc == 0)
                continue;

            // If other region has lower priority value, switch
            if (*(int32_t*)(otherRegionDesc + 0x24) < currentPriority) {
                // Double-check vacancy
                if (fn_CitizenComponent_HasVacancy(workplaceCompIBP))
                    return true;
            }
        }

        return false;
    } else {
        // ====== UNASSIGNED PATH ======
        // Citizen has no specific workplace. Check if worker distribution is unbalanced.
        int maxGap = 0;
        uint32_t minGap = 0xFFFF;

        for (int64_t i = 0; i < (int64_t)jobCount; i++) {
            uintptr_t entityIBP = GetJobEntity(registry, profKey, i);

            // Find WorkplaceComponent on this entity
            uintptr_t entity = entityIBP;
            int compCount = *(int32_t*)(entity + 0x38);
            uintptr_t compArray = *(uintptr_t*)(entity + 0x30);

            // Scan for WorkplaceComponent by type ID
            uintptr_t wpComp = 0;
            uint32_t wpTypeId = WorkPlaceComponent::TypeId();
            for (int ci = 0; ci < compCount; ci++) {
                uintptr_t* compSlot = (uintptr_t*)(compArray + ci * 8);
                uintptr_t comp = *compSlot;
                uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
                if (typeId == wpTypeId) {
                    // Verify with IsDerivedFrom
                    compSlot = (uintptr_t*)(compArray + ci * 8);
                    uintptr_t finalComp = *compSlot;
                    if (Object::IsDerivedFromNullSafe((void*)finalComp, wpTypeId)) {
                        wpComp = finalComp;
                    }
                    break;
                }
            }

            // Construct IBP for found component
            uintptr_t wpIBP = 0;
            new (&wpIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>((void*)wpComp));   // B.3.3 bridge

            uintptr_t wpCompVal = wpIBP;
            if (*(uint8_t*)(wpCompVal + 0x50) == 0)
                continue; // not active

            // Check workplace validity
            char isValid = CanReach(reinterpret_cast<ComponentEntity*>(entityIBP));
            if (!isValid)
                continue;

            // Get worker slot count via StorageLocationComponent
            uintptr_t wpBase = wpIBP;
            uintptr_t parentEntity = *(uintptr_t*)(wpBase + 0x10);

            uintptr_t slcIBP = 0;
            void* slcResult = WorkBase_FindComponent_3050(parentEntity, &slcIBP);
            uintptr_t slc = *(uintptr_t*)slcResult;

            // Calculate worker slots (lazy-compute cached at +0x30)
            if (*(int32_t*)(slc + 0x30) == 0) {
                *(int32_t*)(slc + 0x30) = fn_CalcWorkerSlots((void*)slc);
            }
            int workerSlots = *(int32_t*)(slc + 0x30);

            // Get current worker count from WorkplaceComponent
            int currentWorkers = *(int32_t*)(wpIBP + 0x60);

            int gap = workerSlots - currentWorkers;
            if (gap > maxGap) maxGap = gap;
            if (gap < (int)minGap) minGap = (uint32_t)gap;
        }

        return (maxGap - (int)minGap) > 1;
    }
}

// ============================================================
// Callee reimpls (Component reg wave, batch "people")
// Faithful ports of the CitizenComponent exe helpers the Update reimpl above
// resolves+calls. Sub-deps + exe-internal globals resolved via guarded
// ResolveAddr (same pattern as the rest of code/Game). Registered in
// reg_component_people.cpp. Bodies are non-static for extern registration.
// ============================================================

// --- exe-internal globals / sub-functions used by the reimpls below ---
// ENDGAME B.3.9 C1 (B39_RULES.md D5): the eight cells below are registered type-id
// cells (INVENTORY_C Table 1), read BY CELL as the class's static TypeId(). Two of
// the old local names lied about the class at their cell: 0x1401c8070 is
// WorkComponent (not "StorageLocationComponent") and 0x1401c7158 is CitizenComponent
// (not "CitizenAIComponent") -- both already converted below.

// Arg 5 is the citizen AGE and is a FLOAT in retail (stored to citizen+0x3c, a float
// field). It was declared `int` here until 2026-07-26 — harmless only because the birth
// path passes literal 0 (bit-identical to 0.0f). Stack slot 5 is typed the same either
// way in the MS x64 ABI, so the fix is representational, but every new caller (the
// founding spawn port) must pass a real float.
// Callees for the citizen-lifecycle cascade (AssignHousing / sub_1930 / sub_40f0 / Find*):
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call sites below.

// .rdata float/double constants used by ConsumeFood / ConsumeFuel come from
// Game/exe_constants.h (kRdata_140172180/1a8/174/194/168/0ec, kRdata_14017214c).


// (FindCompByTypeId retired: its callers use ComponentEntity::FindComponent<T>().)

// CitizenComponent::FindSpouse (0x14010fe40):
// find a citizen of the other gender (+0x38) with no spouse (+0x250), older than the
// description's marriage age (+0x48) and within its age window (+0x4c) of this one.
// Retail returns the InternalPtr through the RDX slot; the member returns what it holds.
// The citizen list is GameUpdateLoop::m_citizenList, still a void* member (LEFT: its
// entries are read raw).
CitizenComponent* CitizenComponent::FindSpouse() {
    uint8_t* gw = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);
    // The old door read the registered 8-byte cell 0x1401722b0 as a uint32_t: its low
    // dword (little-endian) of 0x7fffffffffffffff.
    uint32_t mask = static_cast<uint32_t>(kRdata_1401722b0);
    int64_t list = *reinterpret_cast<int64_t*>(gw + 0xf0);
    int64_t n = *reinterpret_cast<int*>(list + 0xa8);
    for (int64_t i = 0; i < n; i++) {
        int64_t cand = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(list + 0xa0) + i * 8);
        if (*reinterpret_cast<int*>(cand + 0x38) != gender) {
            if (*reinterpret_cast<int64_t*>(cand + 0x250) == 0) {
                float* minAge = &description->marriageAge;
                float candAge = *reinterpret_cast<float*>(cand + 0x3c);
                if (*minAge <= candAge && candAge != *minAge) {
                    float diff = candAge - age;
                    // abs(diff) via mask, then compare to window max (+0x4c)
                    uint32_t bits = *reinterpret_cast<uint32_t*>(&diff) & mask;   // CAST-OK: float_bits
                    float adiff = *reinterpret_cast<float*>(&bits);   // CAST-OK: float_bits
                    if (adiff <= description->_f4c) {
                        return reinterpret_cast<CitizenComponent*>(cand);
                    }
                }
            }
        }
    }
    return nullptr;
}

// CitizenComponent::SetParent (0x140111fa0): leave the current parent's children ring
// (+0x258) and join the new one's. Detach from old, attach to new.
void CitizenComponent::SetParent(CitizenComponent* parent) {
    if (parent != _parent) {
        if (_parent != nullptr) {
            _parent->RemoveChild(this);
        }
        if (parent != nullptr) {
            parent->AddChild(this);
        }
    }
}

// CitizenComponent::FindWorkplace (0x14010ffc0):
// find the best workplace for the citizen's profession (one whose field4C is clear),
// choosing the one with the most free worker slots that the citizen can reach; scans
// the town manager's profession tree keyed by the citizen's profession (+0x1f0).
// The tree hangs off GameUpdateLoop::m_citizenList (a void* member): its nodes are
// read raw (LEFT); each entry is a ComponentEntity.
ComponentEntity* CitizenComponent::FindWorkplace() {
    uint8_t* gw = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);
    ComponentEntity* best = nullptr;
    int64_t tree = *reinterpret_cast<int64_t*>(gw + 0xf0);
    uint64_t key = reinterpret_cast<uint64_t>(profession);   // CAST-OK: ptr_int

    // tree lookup helper (red-black ordered by uint64 at node[0]); node layout:
    //   node[0]=key, node[1]=valueArrayPtr, node[2]=count(low32), node[4]=left, node[5]=right
    auto findNode = [&](uint64_t k) -> uint64_t* {
        uint64_t* p = *reinterpret_cast<uint64_t**>(tree + 0x100);
        while (p != nullptr) {
            if (k < p[0]) p = reinterpret_cast<uint64_t*>(p[4]);
            else if (k <= p[0]) return p;
            else p = reinterpret_cast<uint64_t*>(p[5]);
        }
        return nullptr;
    };

    uint64_t* keyNode = findNode(key);
    int outerCount = keyNode ? (int)keyNode[2] : 0;
    int bestSlots = 0;
    for (int64_t i = 0; i < outerCount; i++) {
        uint64_t* node = findNode(key);
        if (!node) break;
        ComponentEntity* entry = *reinterpret_cast<ComponentEntity**>(node[1] + i * 8);

        // the entry's WorkPlaceComponent (DAT_1401c80e0)
        WorkPlaceComponent* wp = entry->FindComponent<WorkPlaceComponent>();
        int curWorkers = wp->workerCount;

        // the WorkComponent (DAT_1401c8070) on wp's entity (+0x10)
        WorkComponent* sloc = wp->_entity->FindComponent<WorkComponent>();
        int maxSlots = sloc->workState;
        if (maxSlots == 0) {
            maxSlots = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(sloc));
            sloc->workState = maxSlots;
        }

        if (curWorkers < maxSlots) {
            if (wp->isAccepting) {
                char ok = CanReach(entry);
                if (ok != '\0') {
                    WorkComponent* s2 = wp->_entity->FindComponent<WorkComponent>();
                    int s2slots = s2->workState;
                    if (s2slots == 0) {
                        s2slots = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(s2));
                        s2->workState = s2slots;
                    }
                    int gap = s2slots - wp->workerCount;
                    if (bestSlots < gap) {
                        best = entry;
                        bestSlots = gap;
                    }
                }
            }
        }
    }
    return best;
}

// CitizenComponent::FindRankedWorkplace (0x140110470):
// find the best workplace for a profession whose field4C is set: builds a candidate
// array of reachable workplaces with a free slot, sorts it (0x1400302a0), then returns
// the first whose free-slot count exceeds the workplace's current workers. Same
// scanning structure as FindWorkplace plus a grow-array.
ComponentEntity* CitizenComponent::FindRankedWorkplace() {
    uint8_t* gw = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);
    int64_t tree = *reinterpret_cast<int64_t*>(gw + 0xf0);
    uint64_t key = reinterpret_cast<uint64_t>(profession);   // CAST-OK: ptr_int

    auto findNode = [&](uint64_t k) -> uint64_t* {
        uint64_t* p = *reinterpret_cast<uint64_t**>(tree + 0x100);
        while (p != nullptr) {
            if (k < p[0]) p = reinterpret_cast<uint64_t*>(p[4]);
            else if (k <= p[0]) return p;
            else p = reinterpret_cast<uint64_t*>(p[5]);
        }
        return nullptr;
    };

    uint64_t* keyNode = findNode(key);
    int outerCount = keyNode ? (int)keyNode[2] : 0;

    // dynamic candidate array
    ComponentEntity** cand = nullptr;
    int candCount = 0;
    int candCap = 0;

    for (int64_t i = 0; i < outerCount; i++) {
        uint64_t* node = findNode(key);
        if (!node) break;
        ComponentEntity* entry = *reinterpret_cast<ComponentEntity**>(node[1] + i * 8);

        WorkPlaceComponent* wp = entry->FindComponent<WorkPlaceComponent>();
        int curWorkers = wp->workerCount;

        WorkComponent* sloc = wp->_entity->FindComponent<WorkComponent>();
        int maxSlots = sloc->workState;
        if (maxSlots == 0) {
            maxSlots = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(sloc));
            sloc->workState = maxSlots;
        }

        if (curWorkers < maxSlots) {
            if (wp->isAccepting) {
                char ok = CanReach(entry);
                if (ok != '\0') {
                    // append entry to candidate array (grow with 3x policy)
                    int newCount = candCount + 1;
                    if (candCap < newCount) {
                        candCap = (candCount != 0) ? newCount * 3 : newCount;
                        ComponentEntity** nb = static_cast<ComponentEntity**>(
                            System::MemoryAllocator::MemAlloc((uint64_t)candCap * 8, 8));
                        for (int k = 0; k < candCount; k++) nb[k] = cand[k];
                        if (cand) System::MemoryAllocator::MemFree(cand);
                        cand = nb;
                    }
                    cand[candCount] = entry;
                    candCount = newCount;
                }
            }
        }
    }

    if (candCount == 0) {
        if (cand) System::MemoryAllocator::MemFree(cand);
        return nullptr;
    }
    if (candCount > 1) {
        CitizenComp_SortByProfession_02a0(cand, candCount, this);
    }
    for (int k = 0; k < candCount; k++) {
        ComponentEntity* entry = cand[k];
        WorkPlaceComponent* wp = entry->FindComponent<WorkPlaceComponent>();
        WorkComponent* sloc = wp->_entity->FindComponent<WorkComponent>();
        int maxSlots = sloc->workState;
        if (maxSlots == 0) {
            maxSlots = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(sloc));
            sloc->workState = maxSlots;
        }
        if (wp->workerCount < maxSlots) {
            System::MemoryAllocator::MemFree(cand);
            return entry;
        }
    }
    System::MemoryAllocator::MemFree(cand);
    return nullptr;
}

// CitizenComponent::FindNewWorkplace (0x14010ff80): the ranked finder when the
// profession's field4C (+0x4c) is set, else the plain one.
ComponentEntity* CitizenComponent::FindNewWorkplace() {
    if (profession->field4C) {
        return FindRankedWorkplace();
    }
    return FindWorkplace();
}

// CitizenComponent::CanFindHousing (0x140115be0):
// returns true if the tile of the home (+0x238) or of the workplace (+0x248) has its
// +0x22 flag set, else samples the citizen's current world position against the same
// grid. The MapComponent footprint is read as int16 (the tree's width; MapComponent.h
// declares uint16 — LEFT for its owner, see the hand-in).
char CitizenComponent::CanFindHousing() {
    int* grid = *reinterpret_cast<int**>(reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton) + 0xf8);

    auto tileBuildable = [&](int64_t res8) -> bool {
        // [STANDALONE GUARD] FUN_140004680 (position-component find on the workplace/
        // residence entity) returns null in standalone when the component is inert /
        // its type-id global is unfilled. The real game always resolves it. Bail
        // "not buildable" rather than deref res8+0x2e (p[0x17]). Surfaced by the
        // BANISHED_FORCE_WORK harness setting +0x238 to a real building entity.
        if (res8 == 0) return false;
        int64_t base = *reinterpret_cast<int64_t*>(grid + 0x20);
        int dim = **reinterpret_cast<int**>(reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton) + 0xf8);
        int16_t* p = reinterpret_cast<int16_t*>(res8);
        int64_t idx = (((int64_t)p[0x17] >> 1) + (int64_t)p[0x15]) * dim +
                      (((int64_t)p[0x16] >> 1) + (int64_t)p[0x14]);
        return *reinterpret_cast<char*>(base + 0x22 + idx * 0x30) != '\0';
    };

    if (home != nullptr) {
        int64_t res8 = 0;
        FloorComponent_FindResData(home, &res8);
        if (tileBuildable(res8)) return 1;
    }
    if (workplace != nullptr) {
        int64_t res8 = 0;
        FloorComponent_FindResData(workplace, &res8);
        if (tileBuildable(res8)) return 1;
    }
    CharacterComponent* mc = character;
    if (mc == nullptr) return 0;
    // ModelComponent's transform is raw storage (its owner's H5 member): read raw.
    int64_t geom = reinterpret_cast<int64_t>(mc->modelComponent);
    // [STANDALONE BRING-UP GUARD] mc+0x68 is the citizen's geometry/transform IBP,
    // populated only once the CharacterComponent position pipeline is live (Brick 4).
    // Until then it is null; bail "can't find housing" rather than deref geom+0x158.
    if (geom == 0) return 0;
    int dim = grid[0];
    int x = (int)(*reinterpret_cast<float*>(geom + 0x158) + (float)(dim >> 1));
    int y = (int)(*reinterpret_cast<float*>(geom + 0x15c) + (float)(dim >> 1));
    if (x < 0) x = 0;
    if (dim <= x) x = dim - 1;
    if (y < 0) y = 0;
    if (dim <= y) y = dim - 1;
    int64_t cell = ((int64_t)dim * y + x) * 0x30;
    return *reinterpret_cast<char*>(cell + 0x22 + *reinterpret_cast<int64_t*>(grid + 0x20)) != '\0';
}

// The standalone type-name -> CRC registry lookup (also declared further down, next to
// the storage finder that uses it). Needed here for the newborn-component fallback.

// CitizenComponent::GiveBirth (0x140111b90):
// procreation — count unmarried citizens within the description's age window to bias
// the child's gender, allocate a new ComponentEntity copy of this citizen's entity
// description, find its CitizenComponent (DAT_1401c7158), initialise it at the
// parent's world position with this citizen as parent, register it, post life event 8
// and flag both parents (+0x88 |= 0x100).
void CitizenComponent::GiveBirth() {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    int64_t list = reinterpret_cast<int64_t>(gw->m_citizenList);   // void* member: entries read raw

    int eligible = 0, mature = 0;
    int total = *reinterpret_cast<int*>(list + 0xa8);
    for (int i = 0; i < total; i++) {
        int64_t cand = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(list + 0xa0) + (int64_t)i * 8);
        if (*reinterpret_cast<int64_t*>(cand + 0x250) == 0) {
            float maxAge = description->_f4c;
            float candAge = *reinterpret_cast<float*>(cand + 0x3c);
            if (candAge <= maxAge) {
                if (*reinterpret_cast<int*>(cand + 0x38) == 1) mature++;
                eligible++;
            }
        }
    }

    System::Random* randPtr = gw->m_random;
    uint32_t gender;
    if (eligible != 0) {
        float r = randPtr->RandFloat();
        gender = (uint32_t)((float)mature / (float)eligible <= r);
    } else {
        gender = randPtr->Rand(2u);
    }

    // allocate + copy-construct the child entity
    void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
    ComponentEntity* child = nullptr;
    if (mem) {
        child = ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(mem), _entity->_description);
    }

    // Find the newborn's CitizenComponent — retail reads the type id from DAT_1401c7158,
    // which is BaseTypeInfo("CitizenComponent"), NOT CitizenAIComponent (see the corrected
    // note in standalone/reg_citizenai_typeids.cpp).
    //
    // [STANDALONE GAP → CRASH FIX] That .rdata global used to be filled only behind the
    // opt-in BANISHED_LIVE_CITIZENAI_CITID gate (GRADUATED 2026-08-26 — now default-on,
    // kill-switch =0). With the kill-switch set it still reads 0, the find
    // matches nothing, and the deferred init below was called with a NULL component:
    // ACCESS_VIOLATION reading 0x10 inside the deferred init's first `*(self+0x10)`
    // (observed on the very first in-game birth, ~frame 5000, release build 2026-07-25).
    // ENDGAME B.3.9 C1: 0x1401c7158 IS CitizenComponent (INVENTORY_C Table 1, not
    // "CitizenAIComponent" as the old local name said).
    CitizenComponent* newborn = child->FindComponent<CitizenComponent>();
    if (newborn == nullptr) {
        // Never true in retail (the newborn is a copy of a citizen template, so it always
        // carries the component). Abort the birth rather than hand the deferred init a null
        // `this` — a crash here kills the run on the first childbirth.
        static int s_warned = 0;
        if (s_warned < 4) { ++s_warned;
            fprintf(stderr, "[Birth] ABORT: newborn entity %p has no CitizenComponent "
                    "(tid=%u) — skipping deferred init\n", (void*)child, CitizenComponent::TypeId());
            fflush(stderr);
        }
        return;
    }

    Profession* spawnArg = gw->m_professionList->field_0x30;

    // parent world position from ModelComponent (+0x278 -> +0x68 -> +0x158..0x160);
    // ModelComponent's transform is raw storage (its owner's H5 member): read raw.
    CharacterComponent* mc = character;
    int64_t geom = reinterpret_cast<int64_t>(mc->modelComponent);
    float pos[3];
    pos[0] = *reinterpret_cast<float*>(geom + 0x158);
    pos[1] = *reinterpret_cast<float*>(geom + 0x15c);
    pos[2] = *reinterpret_cast<float*>(geom + 0x160);

    newborn->Initialize(pos, gender, '\x01', 0, '\0', this, spawnArg);

    ComponentEntity* addArg = child;
    {
        ComponentDescription* desc = _entity->_description;
        EntityList_RegisterEntity_Impl(gw->m_entityList, &addArg, desc->_category);
    }
    // Post "child born" (event type 8) for the newborn.
    newborn->PostLifeEvent(8, 0, nullptr);

    happinessFlags |= 0x100;
    if (spouse != nullptr) {
        spouse->happinessFlags |= 0x100;
    }
}

// CitizenComponent::EatFrom (0x1401152e0):
// NOTE the retail label "ReleaseWorkplace" is a MISNOMER — this is EatFromStorage
// (Economy Brick M-D): while hunger (+0x44) > 0, pick a stocked edible from the
// target's StorageComponent (Entity_impl_ef40), SubtractItem, bump the town
// per-category consumed counter (townMgr+0x170[cat*0x20+0xC]), OR the material's
// diet bits (+0x5c) into the citizen's +0x54 state mask, hunger -= g_fUVScale (1.0f).
// [SCOPED STANDALONE FALLBACK] self-contained StorageComponent find on an entity:
// an exact type-id match only (no IsDerivedFrom), kept from the time the 6270 port's
// type-id cell was unregistered in standalone. Used by the eat chain (EatFrom +
// Entity_impl_ef40).
static StorageComponent* StandaloneFindStorageComp(ComponentEntity* entity) {
    if (!entity) return nullptr;
    uint32_t tidStorage = StorageComponent::TypeId();
    if (tidStorage == 0) return nullptr;
    int cnt = entity->_componentCount;
    Object::InternalPtr<Component>* arr = entity->_components;
    for (int i = 0; i < cnt && arr; ++i) {
        Component* comp = arr[i].Get();
        if (!comp) continue;
        if (comp->Type() == tidStorage)
            return static_cast<StorageComponent*>(comp);
    }
    return nullptr;
}
void CitizenComponent::EatFrom(ComponentEntity* target) {
    if (target == nullptr) return;

    ComponentEntity* entity = target;
    // Disasm 0x140115311: CALL 0x140006270 — the StorageComponent finder
    // (DAT_1401c7e78), NOT 0x140008090 (that one is the ResidenceComponent finder,
    // DAT_1401c7cf0 — the previous code here used it by mistake, so the eat loop
    // found no storage on storage-only food buildings).
    StorageComponent* store = entity->FindComponent<StorageComponent>();
    // Scoped standalone fallback — see StandaloneFindStorageComp.
    if (store == nullptr) store = StandaloneFindStorageComp(entity);
    if (store == nullptr) {
        // Retail assumes the eat target always carries a StorageComponent (it would
        // fault inside SubtractItem otherwise). In standalone the target may lack a
        // live storage until the world-side M-D brick lands — skip with a trace.
        static int s_noStore = 0;
        if (s_noStore < 4) { ++s_noStore;
            fprintf(stderr, "[EatMsg] EatFromStorage: target=%p has NO StorageComponent — skip\n",
                    (void*)entity);
            fflush(stderr);
        }
        return;
    }

    // consumedTypes (+0x98) is H5 raw storage (Collection::Array data): read raw.
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    float uv = kRdata_14017214c;
    float hours = hunger;
    do {
        if (hours <= 0.0f) return;
        ComponentEntity* arg = target;
        int64_t res10 = 0;
        CitizenComp_Entity_impl_ef40(entity, &res10, &arg, 0);
        int64_t work = res10;
        if (res10 == 0) return;

        // add `work` to the citizen's consumed-type list (+0x98 array, +0xa0 count, +0xa4 cap)
        int count = consumedTypeCount;
        happinessFlags |= 0x10000;
        bool present = false;
        if (0 < count) {
            int64_t* arr = *reinterpret_cast<int64_t**>(self + 0x98);
            for (int i = 0; i < count; i++) {
                if (arr[i] == res10) { present = true; break; }
            }
        }
        if (!present) {
            int newCount = count + 1;
            int64_t* old = *reinterpret_cast<int64_t**>(self + 0x98);
            consumedTypeCount = newCount;
            if (consumedTypeCapacity < newCount) {
                consumedTypeCapacity = (count != 0) ? newCount * 3 : newCount;
                int64_t* nb = static_cast<int64_t*>(
                    System::MemoryAllocator::MemAlloc((uint64_t)consumedTypeCapacity * 8, 8));
                *reinterpret_cast<int64_t**>(self + 0x98) = nb;
            }
            int64_t* dst = *reinterpret_cast<int64_t**>(self + 0x98);
            int start = (old == dst) ? count : 0;
            for (int i = start; i < newCount; i++) {
                dst[i] = (i < count) ? old[i] : res10;
            }
            if (old != dst && old) System::MemoryAllocator::MemFree(old);
        }

        // subtract one unit from the storage + bump the town per-category
        // consumed counter (townMgr = *(gameWorld+0xf0), counters at +0x170,
        // 0x20-byte stride, count at +0xC). m_citizenList is a void* member: raw.
        ActRm_StorageSubtract_d150(reinterpret_cast<uintptr_t>(store), work, 1);
        int64_t worldGrid = reinterpret_cast<int64_t>(GameUpdateLoop::_singleton->m_citizenList);
        {
            uint32_t idx = ActRm_GetResourceCategory_88f90(store, work);
            if (-1 < (int)idx) {
                int* p = reinterpret_cast<int*>((int64_t)(int)idx * 0x20 + 0xc +
                                                *reinterpret_cast<int64_t*>(worldGrid + 0x170));
                *p = *p + 1;
            }
        }

        // find the RawMaterialDescription (DAT_1401c7cb8) on the work entity, OR its
        // capability bits into the citizen's +0x54 state mask.
        // Typeid via the registered storage (filled from GetTypeIdByName after
        // Engine::Startup in standalone); skip the scan while it is unfilled
        // (0 / 0xFFFFFFFF) so a zero id can't false-match an inert stub component.
        void* mat = nullptr;
        uint32_t matTid = RawMaterialDescription::TypeId();
        int rows = (matTid != 0 && matTid != 0xFFFFFFFFu)
                       ? *reinterpret_cast<int*>(work + 0x30) : 0;
        for (int i = 0; i < rows; i++) {
            int64_t comp = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(work + 0x28) + (int64_t)i * 8);
            uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
            if (tid == matTid) {
                void* obj = reinterpret_cast<void*>(comp);
                mat = Object::IsDerivedFromNullSafe(obj, matTid) ? obj : nullptr;
                break;
            }
        }
        uint32_t* state = &statusFlags;
        if ((*state & 8) == 0) *state = (*state & 0xfffffe8f) | 8;
        if (mat) {
            uint32_t f = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(mat) + 0x5c);
            if (f & 1)        *state |= 0x10;
            if (f >> 1 & 1)   *state |= 0x20;
            if (f >> 2 & 1)   *state |= 0x40;
            if (f >> 4 & 1)   *state |= 0x100;
        }
        hours = hunger - uv;
        hunger = hours;
    } while (0.0f <= hours);
    hunger = 0.0f;
}

// CitizenComponent::FindHome (0x140110e70):
// find a home (a residence household slot) for the citizen. First honors the spouse's
// home (+0x250 -> +0x238) if reachable and a single-household residence; else scans the
// town's residence registry (townMgr+0x20) for a free household slot reachable from the
// citizen's position, preferring a single-household residence over a shared one.
// NOTE: this is a long, deeply engine-coupled scan; ported structurally from Ghidra.
// `slot` receives the household slot; retail returns the InternalPtr through RDX.
// The registry hangs off GameUpdateLoop::m_citizenList (a void* member): its entries
// are read raw (LEFT).
ComponentEntity* CitizenComponent::FindHome(int32_t* slot, bool anyHousehold) {
    int* param_3 = slot;
    char param_4 = anyHousehold;
    int bestSlot = -1;
    CitizenComponent* self250 = spouse;
    *param_3 = -1;

    if (self250 != nullptr) {
        self250 = spouse;
        if (self250->home != nullptr) {
            char ok = CanReach(spouse->home);
            if (ok != '\0') {
                ResidenceComponent* st = spouse->home->FindComponent<ResidenceComponent>();
                if (st->residentSlotCount < 2) {
                    *param_3 = spouse->homeSlot;
                    return spouse->home;
                }
            }
        }
    }

    // ---- Faithful 1:1 port of the townMgr scan (0x140110e70) ----
    // townMgr = *(gameWorld+0xf0); registry array *(townMgr+0x20), count *(townMgr+0x28).
    // Citizen fairness pre-scan uses the citizen list *(townMgr+0xa0)/*(townMgr+0xa8).
    int64_t townMgr = reinterpret_cast<int64_t>(GameUpdateLoop::_singleton->m_citizenList);

    // Component finder on an entry entity (comp list +0x30 array / +0x38 count,
    // GetTypeId via vtable+0x10, then IsDerivedFrom). Matches Entity_FindComponent_*.
    auto FindEntityComp = [](int64_t entity, uint32_t tid) -> void* {
        if (tid == 0) return nullptr;
        int cnt = *reinterpret_cast<int*>(entity + 0x38);
        int64_t arr = *reinterpret_cast<int64_t*>(entity + 0x30);
        for (int i = 0; i < cnt; i++) {
            void** ibp = reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
            int64_t comp = *reinterpret_cast<int64_t*>(ibp);
            if (comp == 0) continue;
            uint32_t ctid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
            if (ctid == tid) {
                void** ibp2 = reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
                void* resolved = *ibp2;
                return Object::IsDerivedFromNullSafe(resolved, tid) ? resolved : nullptr;
            }
        }
        return nullptr;
    };

    int64_t bestPartial = 0;  // lVar19 (reused below as partial-best accumulator)

    // Pre-scan (only when the citizen already holds a workplace): fairness gate for
    // the "extra work" second call. Bail (return null) if any adult coworker is idle
    // or if our workplace is nearly full while a coworker's still has room.
    if (home != nullptr) {
        ResidenceComponent* ownStore = home->FindComponent<ResidenceComponent>();
        int citCount = *reinterpret_cast<int*>(townMgr + 0xa8);
        for (int64_t i = 0; i < citCount; i++) {
            int64_t other = *reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(townMgr + 0xa0) + i * 8);
            if (other == reinterpret_cast<int64_t>(this)) continue;
            if (*reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(other + 0x288) + 0x44)
                    <= *reinterpret_cast<float*>(other + 0x3c)
                && *reinterpret_cast<int64_t*>(other + 0x208) == 0) {
                if (*reinterpret_cast<int64_t*>(other + 0x238) == 0) {
                    return nullptr;   // LAB_140111497
                }
                int64_t otherWp = *reinterpret_cast<int64_t*>(other + 0x238);
                int64_t otherStoreIBP = 0;
                CitizenComp_Entity_FindComponent_Storage(otherWp, &otherStoreIBP);
                if (ownStore->residentSlotCount == 1) {
                    if (1 < *reinterpret_cast<int*>(otherStoreIBP + 0x58)) {
                        return nullptr;   // LAB_140111497
                    }
                }
            }
        }
    }

    int count = *reinterpret_cast<int*>(townMgr + 0x28);
    int* mapInfo = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);

    // The original computes the home cell then loops the registry; when the registry
    // is empty the home block is dead (its result is only read inside the loop), so we
    // gate it behind count>0 — a byte-faithful no-op when townMgr+0x20 is empty.
    if (count > 0) {
        CharacterComponent* homeEntity = character;
        if (homeEntity == nullptr) return nullptr;
        // ModelComponent's transform is raw storage (its owner's H5 member): read raw.
        int64_t homeComp = reinterpret_cast<int64_t>(homeEntity->modelComponent);
        if (homeComp == 0) return nullptr;
        int mapW = *mapInfo;
        int homeX = static_cast<int>(*reinterpret_cast<float*>(homeComp + 0x158) + static_cast<float>(mapW >> 1));
        int homeY = static_cast<int>(*reinterpret_cast<float*>(homeComp + 0x15c) + static_cast<float>(mapW >> 1));
        if (homeX < 0) homeX = 0;
        if (mapW <= homeX) homeX = mapW - 1;
        if (homeY < 0) homeY = 0;
        if (mapW <= homeY) homeY = mapW - 1;
        int64_t homeCellIdx = static_cast<int64_t>(homeY * mapW + homeX);

        for (int64_t entryIdx = 0; entryIdx < count; entryIdx++) {
            int64_t entry = *reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(townMgr + 0x20) + entryIdx * 8);  // local_98

            void* resComp = FindEntityComp(entry, ResidenceComponent::TypeId());   // 0x1401c7cf0
            if (!resComp) continue;   // registered entries always carry one; guard.
            int64_t resIBP = 0;
            new (&resIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(resComp));   // B.3.3 bridge

            bool consider;
            if (param_4 != '\0') {
                consider = true;
            } else {
                consider = (*reinterpret_cast<int*>(resIBP + 0x58) < 2);
            }
            if (!consider) continue;

            void* interactComp = FindEntityComp(entry, InteractComponent::TypeId());  // 0x1401c7900
            int64_t interactIBP = 0;
            new (&interactIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(interactComp));   // B.3.3 bridge
            if (!interactComp) continue;   // no work-positions -> can't seat a worker.

            int rosterCount = *reinterpret_cast<int*>(resIBP + 0x58);
            int64_t rosterOff = 0;
            for (int slot = 0; slot < rosterCount; slot++) {
                int64_t rosterBase = *reinterpret_cast<int64_t*>(resIBP + 0x50);
                if (*reinterpret_cast<int*>(rosterBase + 8 + rosterOff) < 1) {  // free slot
                    float pos[3] = {0.0f, 0.0f, 0.0f};
                    Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(interactIBP), pos, 0, 0, nullptr);
                    int mw = *mapInfo;
                    int wpX = static_cast<int>(pos[0] + static_cast<float>(mw >> 1));
                    int wpY = static_cast<int>(pos[1] + static_cast<float>(mw >> 1));
                    if (wpX < 0) wpX = 0;
                    if (mw <= wpX) wpX = mw - 1;
                    if (wpY < 0) wpY = 0;
                    if (mw <= wpY) wpY = mw - 1;
                    Pf_ZoneRefresh_9c480(mapInfo);   // 0x14009c480 (RCX only)
                    int64_t cellBase = *reinterpret_cast<int64_t*>(
                        reinterpret_cast<uint8_t*>(mapInfo) + 0x80);
                    int64_t homeCellPtr = homeCellIdx * 0x30 + cellBase;
                    int64_t wpCellPtr = static_cast<int64_t>(wpY * mw + wpX) * 0x30 + cellBase;
                    uint32_t wpBit   = 1u << (*reinterpret_cast<uint8_t*>(wpCellPtr + 0x29) & 0x1f);
                    uint32_t homeBit = 1u << (*reinterpret_cast<uint8_t*>(homeCellPtr + 0x29) & 0x1f);
                    uint32_t both = homeBit | wpBit;
                    uint32_t mask = both | 0xe1f;
                    short hr = *reinterpret_cast<short*>(homeCellPtr + 0x26);
                    short wr = *reinterpret_cast<short*>(wpCellPtr + 0x26);
                    if ((homeBit & mask) != 0 && (wpBit & mask) != 0 &&
                        ((both & 0x1e0) != 0 || (hr != 0 && wr != 0 && hr == wr))) {
                        if (*reinterpret_cast<int*>(resIBP + 0x58) == 1) {
                            *param_3 = slot;
                            return reinterpret_cast<ComponentEntity*>(entry);
                        }
                        if (bestPartial == 0 && 1 < *reinterpret_cast<int*>(resIBP + 0x58)) {
                            bestPartial = entry;
                            bestSlot = slot;
                        }
                    }
                }
                rosterOff += 0x10;
            }
        }
        if (bestPartial != 0) {
            *param_3 = bestSlot;
            return reinterpret_cast<ComponentEntity*>(bestPartial);
        }
    }

    // LAB_140111497 — no home found.
    return nullptr;
}

// CitizenComponent::CanReach (0x1401158d0; tree: "CheckWorkplaceValid"):
// is `target` reachable from the citizen's position? Reads the map (GameUpdateLoop
// m_game, a void* member: its pointee is read raw), clamps the tile under the
// citizen's model (CharacterComponent +0x68 -> ModelComponent +0x158/+0x15c, raw
// transform storage) against the map width, then defers to the world reachability
// test FUN_1400a00f0(map, 0xE1F, cellIndex, anchorCell(target)).
char CitizenComponent::CanReach(ComponentEntity* target) {
    int* mapInfo = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);

    // CheckPtr on the CharacterComponent IBP (+0x278), read its ModelComponent (+0x68).
    CharacterComponent* homeEntity = character;
    int64_t homeComp = reinterpret_cast<int64_t>(homeEntity->modelComponent);

    int mapW = *mapInfo;
    int halfW = mapW >> 1;
    int tileY = static_cast<int>(*reinterpret_cast<float*>(homeComp + 0x15c) + static_cast<float>(halfW));
    int tileX = static_cast<int>(static_cast<float>(halfW) + *reinterpret_cast<float*>(homeComp + 0x158));
    if (tileX < 0) tileX = 0;
    if (mapW <= tileX) tileX = mapW - 1;
    if (tileY < 0) tileY = 0;
    if (mapW <= tileY) tileY = mapW - 1;

    ComponentEntity* ibpVal = target;
    int regionId = Pf_EntityAnchorCell_7fb0(&ibpVal, nullptr);
    uint32_t cellIdx = static_cast<uint32_t>(mapW * tileY + tileX);
    char res = static_cast<char>(static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(mapInfo, 0xe1f, cellIdx, regionId)));
    return res;
}

// Reimpl of Entity_impl_ef40 (0x14008ef40):
// pick a free production "slot" of category param_4 (0..4) from the storage component's
// task list. Two-pass: pass 1 counts eligible slots (those whose RawMaterialDescription
// capability bit for the category is set AND that have remaining demand); if any, roll a
// random index over them; pass 2 walks the list again returning the chosen slot's IBP.
// The category selects which bit of the material's +0x5c flags word gates a slot:
//   0->bit5, 1->bit6, 2->bit9, 3->bit13, 4->bit14.
int64_t* CitizenComp_Entity_impl_ef40(void* param_1, int64_t* param_2, void* param_3, int param_4) {
    (void)param_1;
    uint32_t rawMatTid = RawMaterialDescription::TypeId();

    // Category -> capability bit-shift of material flags (+0x5c).
    auto catShift = [&](int cat) -> int {
        switch (cat) {
            case 0: return 5; case 1: return 6; case 2: return 9;
            case 3: return 13; case 4: return 14; default: return -1;
        }
    };
    int shift = catShift(param_4);

    ComponentEntity* storeEntity = *reinterpret_cast<ComponentEntity**>(param_3);
    StorageComponent* store = storeEntity->FindComponent<StorageComponent>();
    // Scoped standalone fallback (registered 6270 port's typeid is dead .rdata) +
    // null bail: retail asserts the storage exists; in standalone an entity in a
    // town search list may still lack a live StorageComponent — treat as no slot.
    if (store == nullptr)
        store = StandaloneFindStorageComp(storeEntity);
    if (store == nullptr) { *param_2 = 0; return param_2; }

    uint32_t chosen = 0xffffffff;
    int64_t taskCount = store->slotCount;
    if (taskCount <= 0) { *param_2 = 0; return param_2; }

    // Find the RawMaterialDescription on a task entity, returning its derived ptr (or null).
    auto findMat = [&](int64_t taskEntity) -> void* {
        int rows = *reinterpret_cast<int*>(taskEntity + 0x30);
        int64_t arr = *reinterpret_cast<int64_t*>(taskEntity + 0x28);
        for (int i = 0; i < rows; i++) {
            int64_t comp = *reinterpret_cast<int64_t*>(arr + (int64_t)i * 8);
            uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
            if (tid == rawMatTid) {
                void* obj = reinterpret_cast<void*>(comp);
                return Object::IsDerivedFromNullSafe(obj, rawMatTid) ? obj : nullptr;
            }
        }
        return nullptr;
    };

    // Pass 1: count eligible slots; remember the last eligible index in `chosen`.
    uint32_t eligible = 0;
    for (int64_t row = 0; row < taskCount; row++) {
        int64_t listBase = reinterpret_cast<int64_t>(store->storageSlots);   // void* array data: raw
        int64_t entry = *reinterpret_cast<int64_t*>(listBase + row * 0x10);
        void* mat = findMat(entry);
        bool bitSet = mat && shift >= 0 &&
            ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(mat) + 0x5c) >> shift) & 1);
        int demand = *reinterpret_cast<int*>(listBase + 8 + row * 0x10) -
                     *reinterpret_cast<int*>(listBase + 0xc + row * 0x10);
        if (bitSet && demand > 0) {
            if (*reinterpret_cast<char*>(reinterpret_cast<uint8_t*>(mat) + 0x58) != '\0')
                chosen = eligible;
            eligible++;
        }
    }
    if (eligible == 0) { *param_2 = 0; return param_2; }

    if (static_cast<int>(chosen) < 0)
        chosen = GameUpdateLoop::_singleton->m_random->Rand(static_cast<uint32_t>(eligible));

    // Pass 2: walk again, returning the chosen eligible slot's IBP.
    uint32_t cur = 0;
    for (int64_t row = 0; row < taskCount; row++) {
        int64_t listBase = reinterpret_cast<int64_t>(store->storageSlots);   // void* array data: raw
        int64_t* entryP = reinterpret_cast<int64_t*>(listBase + row * 0x10);
        void* mat = findMat(*entryP);
        bool bitSet = mat && shift >= 0 &&
            ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(mat) + 0x5c) >> shift) & 1);
        int demand = static_cast<int>(entryP[1]) - *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(entryP) + 0xc);
        if (bitSet && demand > 0) {
            if (chosen == cur) { *param_2 = *entryP; return param_2; }
            cur++;
        }
    }
    *param_2 = 0;
    return param_2;
}

// CitizenComponent::UpdateHealth (0x140112990; tree and Ghidra: "ConsumeFood"):
// recompute the citizen's health (+0x50) from this tick's state flags (+0x54). True
// leaf: only reads globals + own fields. The +0x54 flag bits gate per-source health
// deltas (scaled by g_fUVScale), then the result is clamped to [0, description+0x70]
// and the flag word is reset. Pinned to the maximum when the world's no-hunger debug
// bit (gameFlags>>3) is set.
void CitizenComponent::UpdateHealth() {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    CitizenDescription* desc = description;
    float uv   = kRdata_14017214c;
    float c114 = kRdata_140172114;
    float c100 = kRdata_140172100;
    float c180 = kRdata_140172180;

    if ((gw->m_gameFlags >> 3 & 1) != 0) {
        health = desc->maxHealth;
        return;
    }
    uint32_t f = statusFlags;
    uint32_t cnt = (f >> 8 & 1) + (f >> 6 & 1) + (f >> 5 & 1) + (f >> 4 & 1);
    if ((f & 0x200) != 0) cnt++;
    float* acc = &health;
    if ((f & 8) != 0 && (f & 4) != 0 && cnt > 3) *acc += c114;
    if ((f & 8) != 0 && (f & 0x200) == 0 && cnt < 3) *acc -= (uv - (float)cnt / c180);
    if ((f & 1) != 0) *acc -= uv;
    if ((f & 2) != 0) *acc -= uv;
    if ((f & 4) == 0 &&
        desc->adultAge <= age &&
        school == nullptr) {
        *acc -= c100;
    }
    if (*acc <= 0.0f && *acc != 0.0f) health = 0.0f;
    float cap = desc->maxHealth;
    if (cap < *acc) *acc = cap;
    statusFlags = 0;
}

// CitizenComponent::UpdateHappiness (0x140112ae0; tree and Ghidra: "ConsumeFuel"):
// recompute the citizen's happiness (+0x80) from this tick's +0x88 flag bits, then
// drive it toward a target derived from the home tile (the map, GameUpdateLoop m_game:
// a void* member whose tile array is read raw, via Entity_impl_7fb0's anchor cell) and
// the home's household count. Closure-safe: callees 0x140087fb0/0x140008090 registered.
void CitizenComponent::UpdateHappiness() {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    float uv   = kRdata_14017214c;
    float c114 = kRdata_140172114;
    float c1a8 = kRdata_1401721a8;
    float c174 = kRdata_140172174;
    float c194 = kRdata_140172194;
    float c0ec = kRdata_1401720ec;
    double c168 = kRdata_140172168;

    uint32_t f = happinessFlags;
    float* acc = &happiness;
    if ((f & 2) != 0)    *acc -= c1a8;
    if ((f & 0x40) != 0) *acc -= c174;
    if ((char)f < 0)     *acc -= c194;   // bit 7 of low byte
    if ((f & 8) != 0)    *acc -= uv;
    if ((f & 0x10) != 0) *acc -= uv;
    if ((f >> 8 & 1) != 0)  *acc += uv;
    if ((f >> 9 & 1) != 0)  *acc += uv;
    if ((f >> 10 & 1) != 0) *acc += uv;
    if ((f >> 11 & 1) != 0) *acc += uv;
    if ((f >> 12 & 1) != 0) *acc += uv;
    if ((f >> 13 & 1) != 0) *acc += uv;
    if ((f >> 0x12 & 1) != 0) *acc += c114;
    if (*acc <= 0.0f && *acc != 0.0f) happiness = 0.0f;
    CitizenDescription* desc = description;
    if (desc->maxHappiness < *acc) *acc = desc->maxHappiness;

    float fVar5 = 0.0f;
    if ((f & 4) == 0)    fVar5 = uv;
    if ((f & 0x20) == 0) fVar5 += uv;
    if ((f & 1) == 0)    fVar5 += uv;
    if ((f >> 0x11 & 1) != 0) {
        if (field_0x84 < desc->_ib0) fVar5 += c114;
        else fVar5 += uv;
    }
    if ((f >> 0x10 & 1) != 0) {
        if (desc->_ia8 < consumedTypeCount) {
            if (desc->_iac <= consumedTypeCount) fVar5 += uv;
        } else {
            fVar5 -= uv;
        }
    }

    ComponentEntity* wpIBP = home;
    if (wpIBP != nullptr) {
        int64_t mapInfo = reinterpret_cast<int64_t>(gw->m_game);
        int region = Pf_EntityAnchorCell_7fb0(&wpIBP, nullptr);
        int64_t tile = (int64_t)region * 0x30 + *reinterpret_cast<int64_t*>(mapInfo + 0x80);
        int climate = ((*reinterpret_cast<char*>(tile + 0x20) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1f) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1e) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1d) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1c) != 0)) -
                      (int)*reinterpret_cast<uint8_t*>(tile + 0x21);
        if (climate < 0) climate = 0;
        ResidenceComponent* st = home->FindComponent<ResidenceComponent>();
        if (st->residentSlotCount > 1) climate >>= 2;
        fVar5 += (float)climate;
    }
    if (field_0x230 != nullptr) fVar5 += uv;

    float cur = *acc;
    fVar5 = fVar5 * c0ec * desc->maxHappiness;
    if (fVar5 <= cur) {
        if (!(cur <= fVar5)) {
            *acc = (float)((double)cur - c168);
        }
    } else {
        *acc = cur + c114;
    }
    if (*acc <= 0.0f && *acc != 0.0f) happiness = 0.0f;
    float capA4 = desc->maxHappiness;
    if (capA4 < *acc) *acc = capA4;

}

// ============================================================
// Citizen-lifecycle cascade (cascade round, snapshot3)
// ============================================================

// CitizenComponent::SetWorkplace (0x140111ee0; tree and Ghidra: "AssignHousing"):
// detach the citizen from its current workplace (+0x248) then attach it to `workplace`.
// On detach: find the workplace's WorkPlaceComponent (002e10) and call its Detach
// (12f7e0) with a back-IBP to this citizen. Symmetric on attach via Attach (12fa20).
void CitizenComponent::SetWorkplace(ComponentEntity* newWorkplace) {
    if (workplace != nullptr) {
        WorkPlaceComponent* building = workplace->FindComponent<WorkPlaceComponent>();
        CitizenComponent* selfIBP = this;
        WorkplaceComponent_Detach(reinterpret_cast<int64_t>(building), &selfIBP);
    }
    workplace = newWorkplace;
    if (newWorkplace != nullptr) {
        WorkPlaceComponent* building = workplace->FindComponent<WorkPlaceComponent>();
        CitizenComponent* selfIBP = this;
        WorkplaceComponent_Attach(reinterpret_cast<int64_t>(building), &selfIBP);
    }
}

// Reimpl of CitizenComponent_sub_40f0 (0x1400440f0):
// test whether a candidate building (param_1) has a free worker slot AND is reachable
// from the citizen's (param_2) home tile. Returns true iff reachable. Closure-safe:
// callees 002ed0/122e20/002e10/0a00f0 all registered.
// [Plague] sub_40f0 exit reporter. The finder's counters proved the DoctorHouse matches
// the healer key and is then rejected HERE, but this function has three independent
// rejections -- a full attendee roster, an unstaffed building, and a failed reachability
// test -- and they need different fixes. g_sub40f0_exit records which one fired:
//   1 = roster full / AttendanceDescription cast failed (cap 0)
//   2 = building has no worker (+0x60 == 0)
//   3 = the 0x140a00f0 path test said unreachable
//   0 = accepted
int g_sub40f0_exit = 0, g_sub40f0_count = 0, g_sub40f0_cap = 0, g_sub40f0_attTid = 0;

bool CitizenComp_sub_40f0(AttendanceComponent* att, CitizenComponent* citizen) {
    // AttendanceComponent's body (LEFT free: its header is COMP-a's). field30 (the
    // queue count) is declared void* in AttendanceComponent.h: read raw.
    g_sub40f0_exit = 0;
    uint8_t* cp = reinterpret_cast<uint8_t*>(att);
    void* obj = att->_description;
    uint32_t attTid = AttendanceDescription::TypeId();   // cell 0x1401c3f78 (NOT Residence)
    void* res = (obj && attTid && Object::IsDerivedFromNullSafe(obj, attTid)) ? obj : nullptr;
    int cap = res ? *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(res) + 0x20) : 0;
    g_sub40f0_attTid = (int)attTid;
    g_sub40f0_cap    = cap;
    g_sub40f0_count  = *reinterpret_cast<int*>(cp + 0x30);
    if (*reinterpret_cast<int*>(cp + 0x30) >= cap) { g_sub40f0_exit = 1; return false; }

    WorkPlaceComponent* bldg = att->_entity->FindComponent<WorkPlaceComponent>();
    if (bldg->workerCount == 0) { g_sub40f0_exit = 2; return false; }

    int* mapInfo = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);
    InteractComponent* posComp = att->_entity->FindComponent<InteractComponent>();
    float pos[3] = {0,0,0};
    Pf_EntityResolvePos_2e20(posComp, pos, 0, 0, nullptr);

    // ModelComponent's transform is raw storage (its owner's H5 member): read raw.
    int64_t homeComp = reinterpret_cast<int64_t>(citizen->character->modelComponent);

    int mapW = *mapInfo;
    float half = (float)(mapW >> 1);
    auto clampTile = [&](float v) -> int {
        int t = (int)(v + half);
        if (t < 0) t = 0;
        if (mapW <= t) t = mapW - 1;
        return t;
    };
    int homeX = clampTile(*reinterpret_cast<float*>(homeComp + 0x158));
    int homeY = clampTile(*reinterpret_cast<float*>(homeComp + 0x15c));
    int posX  = clampTile(pos[0]);
    int posY  = clampTile(pos[1]);
    uint32_t fromCell = (uint32_t)(mapW * posY + posX);
    uint32_t toCell   = (uint32_t)(mapW * homeY + homeX);
    char ok = WorkBase_ValidateCell_a00f0(mapInfo, 0xe1f, fromCell, toCell);
    if (!ok) g_sub40f0_exit = 3;
    return ok != 0;
}

// Reimpl of CitizenComponent_sub_1930 (0x140111930):
// scan the world building list (gameWorld+0xf0 -> +0x60/+0x68) for the closest building
// whose WorkplaceComponent's job key (+0x48) matches param_4 and that is reachable
// (sub_40f0). Returns the best building's IBP via param_2. Closure-safe: sub_40f0 +
// 0x140087fb0 + 0x140008830 all registered.
// [Plague] sub_1930 outcome counters. FindCareBuilding's "noMatchingBuilding" message
// covers TWO different causes -- no candidate whose WorkPlaceComponent+0x48 equals the
// key, AND a candidate that matched the key but was rejected by sub_40f0 -- because both
// leave *param_2 == 0. The boot census independently reported matchingHealerProfession=1
// for the same registry, so the two arms had to be split before the cause could be named.
int g_sub1930_walked = 0, g_sub1930_haveWpc = 0, g_sub1930_profMatch = 0,
    g_sub1930_reject40f0 = 0, g_sub1930_accepted = 0;
int g_sub1930_lastExit = 0, g_sub1930_lastCount = 0, g_sub1930_lastCap = 0;
extern int g_sub40f0_exit, g_sub40f0_count, g_sub40f0_cap;

// The town attendance registry hangs off GameUpdateLoop::m_citizenList (a void*
// member): its entries are read raw (LEFT); each is an AttendanceComponent.
AttendanceComponent* CitizenComponent::FindAttendance(AttendanceComponent* exclude, Profession* key) {
    g_sub1930_walked = g_sub1930_haveWpc = g_sub1930_profMatch =
        g_sub1930_reject40f0 = g_sub1930_accepted = 0;
    int64_t list = reinterpret_cast<int64_t>(GameUpdateLoop::_singleton->m_citizenList);
    ComponentEntity* srcWp = home;
    int bestCost = -1;
    int count = *reinterpret_cast<int*>(list + 0x68);
    AttendanceComponent* best = nullptr;
    uint32_t srcRegion = 0xffffffff;
    if (srcWp != nullptr) {
        ComponentEntity* srcIBP = srcWp;
        srcRegion = (uint32_t)(Pf_EntityAnchorCell_7fb0(&srcIBP, nullptr));
    }

    for (int i = 0; i < count; i++) {
        AttendanceComponent* cand =
            *reinterpret_cast<AttendanceComponent**>(*reinterpret_cast<int64_t*>(list + 0x60) + (int64_t)i * 8);
        if (cand == exclude) continue;

        // the WorkplaceComponent (DAT_1401c80e0) on the candidate's entity
        WorkPlaceComponent* wp = cand->_entity->FindComponent<WorkPlaceComponent>();
        ++g_sub1930_walked;
        if (wp) ++g_sub1930_haveWpc;

        if (wp->profession != key) continue;
        ++g_sub1930_profMatch;

        if (!CitizenComp_sub_40f0(cand, this)) {
            ++g_sub1930_reject40f0;
            g_sub1930_lastExit  = g_sub40f0_exit;
            g_sub1930_lastCount = g_sub40f0_count;
            g_sub1930_lastCap   = g_sub40f0_cap;
            continue;
        }
        ++g_sub1930_accepted;

        ComponentEntity* candSubIBP = cand->_entity;
        uint32_t dstRegion = (uint32_t)(Pf_EntityAnchorCell_7fb0(&candSubIBP, nullptr));
        int cost = 1;
        if ((int)srcRegion >= 0) {
            int* mapInfo = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);
            cost = WorkBase_CalcDistance_8830(mapInfo, dstRegion, srcRegion);
        }
        if (bestCost < 0 || cost < bestCost) {
            best = cand;
            bestCost = cost;
        }
    }
    return best;
}

// CitizenComponent::FindCareBuilding (0x140111590):
// pick the best reachable building (FindAttendance keyed by description+0x30), validate
// it (sub_40f0), register the citizen into that building's attendee array (3110), and
// store the assignment at +0x228. Returns true on success.
//
// ⚠ THE OLD NAME WAS WRONG AND THE Ghidra LABEL IS WRONG. This is NOT a workplace finder
// and NOT the student finder (an earlier note claimed the latter). It is the SICK
// CITIZEN's CARE-BUILDING finder — the hospital link. Proof:
//   * Its only per-tick caller (CitizenComponent::Update, retail 0x1401131fe) gates it on
//     `citizen+0x68 != 0`, and +0x68 is the DISEASE pointer: citizen_updatehealth.cpp
//     writes it at catch time and zeroes it on recovery.
//   * That same catch path calls this function IMMEDIATELY after setting +0x68, and the
//     recovery path releases the +0x228 link.
//   * On death the caller reads +0x68's string table for the cause-of-death text.
// The sibling FindStorage (0x1401116f0, keyed by description+0x28 -> slot +0x230) is the
// other attendance link. Both resolve through FindAttendance, which matches a candidate
// by `WorkPlaceComponent+0x48 == key`. The SCHOOL key is the THIRD ref, +0x20, consumed
// by TryEnrolInSchool (0x140111840). See [[project_wpc_first_seat_and_attendance_keys]].
// [Plague] soak trace — same kill-switch as citizen_updatehealth.cpp's tag.
// Default ON; BANISHED_PLAGUE_TRACE=0 disables. Diagnostic only.
// AttendanceComponent::field30 (the queue count, int32) is declared void* in its
// header (COMP-a's): read at its retail width through the field's address (LEFT).
static bool CareTraceOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = Opt().plagueTrace;
        en = (e && (e[0] == '0' || std::strcmp(e, "false") == 0)) ? 0 : 1;
    }
    return en == 1;
}

char CitizenComponent::FindCareBuilding() {
    CitizenDescription* desc = description;
    Profession* healerProf = desc->_professions[2];
    AttendanceComponent* found = FindAttendance(nullptr, healerProf);
    if (found != nullptr) {
        CitizenComponent* keyIBP = this;
        if (CitizenComp_sub_40f0(found, this)) {
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(&found->queue)
                .SetSizeFill(found->queueCount + 1, &keyIBP);
            careBuilding = found;
            // [Plague] outcome: linked. Only reached for a sick citizen (the two call
            // sites both gate on +0x68 != 0), so this is inherently low volume.
            if (CareTraceOn()) {
                fprintf(stderr, "[Plague] CARE citizen=%p LINKED building=%p "
                        "healerProf(desc+0x30)=%p\n",
                        (void*)this, (void*)found, (void*)healerProf);
                fflush(stderr);
            }
            return 1;
        }
        // Other arm: a profession-matching building exists but sub_40f0 rejected it.
        if (CareTraceOn()) {
            fprintf(stderr, "[Plague] CARE citizen=%p NOT-LINKED reason=candidateRejected "
                    "(sub_40f0) building=%p healerProf(desc+0x30)=%p\n",
                    (void*)this, (void*)found, (void*)healerProf);
            fflush(stderr);
        }
        return 0;
    }
    // Other arm: FindAttendance returned nothing. Which of its two exits that was is
    // reported from the counters, not guessed from the null result.
    if (CareTraceOn()) {
        const char* reason =
            (healerProf == nullptr)    ? "healerProfessionRefIsNull" :
            (g_sub1930_walked == 0)    ? "attendanceRegistryEmpty" :
            (g_sub1930_profMatch == 0) ? "noBuildingWithMatchingWPC_0x48" :
                                         "profMatchedButRejectedBySub40f0";
        fprintf(stderr, "[Plague] CARE citizen=%p NOT-LINKED reason=%s "
                "healerProf(desc+0x30)=%p walked=%d haveWPC=%d profMatch=%d "
                "reject40f0=%d accepted=%d sub40f0Exit=%d(1=rosterFull/capCastFail,2=noWorker,3=unreachable) attendeeCount=%d cap=%d\n", (void*)this, reason,
                (void*)healerProf, g_sub1930_walked, g_sub1930_haveWpc,
                g_sub1930_profMatch, g_sub1930_reject40f0, g_sub1930_accepted,
                g_sub1930_lastExit, g_sub1930_lastCount, g_sub1930_lastCap);
        fflush(stderr);
    }
    return 0;
}

// CitizenComponent::FindStorage (0x1401116f0):
// same as FindCareBuilding but keyed by description+0x28 and stored at +0x230.
void CitizenComponent::FindStorage() {
    CitizenDescription* desc = description;
    AttendanceComponent* found = FindAttendance(nullptr, desc->_professions[1]);
    if (found != nullptr) {
        CitizenComponent* keyIBP = this;
        if (CitizenComp_sub_40f0(found, this)) {
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(&found->queue)
                .SetSizeFill(found->queueCount + 1, &keyIBP);
            field_0x230 = found;
        }
    }
}

// CitizenComponent::TryEnrolInSchool (0x140111840):
// same shape as the care-building finder but keyed by description+0x20; on success stores
// the AttendanceComponent at +0x208 and sets the been-here-already flag (+0x220).
// `exclude` seeds the exclusion IBP.
//
// Renamed here and in Ghidra 2026-08-15; it shipped as "TryFindPartner" long enough for
// the misnomer to reach four files. It is SCHOOL ENROLLMENT, not partner-finding: the third
// and last caller of FindAttendance, and it consumes the first of CitizenDescription's three
// ExternalRef<Profession> slots. Proof:
//   * On success it appends the citizen into `found+0x28` with count `found+0x30` — that
//     is AttendanceComponent's own waiting queue, not anything to do with a spouse.
//   * The graduation path FUN_140115a50 does the exact inverse: it erases the citizen
//     from `(+0x208)+0x28` and clears +0x208. So +0x208 holds an ATTENDANCE COMPONENT.
//   * Measured 2026-08-15: `CitizenDescription+0x20` is byte-identical to the demo
//     schoolhouse's own WorkPlaceComponent+0x48, while +0x28 (FindStorage) and +0x30
//     (the disease-gated hospital finder) match no building in town.
//
// Its ONLY caller is the child->adult transition in Update (`!isAdult && adultAge <=
// newAge && +0x208 == 0`), which is Banished's "at coming of age, become a student if a
// school will take you, else a laborer" fork — hence the 10-vs-9 life event there.
// CONSEQUENCE for testing: a founder spawned already at adult age has isAdult == true on
// its first tick and NEVER crosses that edge, so it can never enrol. An empty school in a
// short soak is therefore CORRECT, not a defect — you need a citizen that actually grows
// up while a staffed school exists. See [[project_wpc_first_seat_and_attendance_keys]].
char CitizenComponent::TryEnrolInSchool(AttendanceComponent* exclude) {
    CitizenDescription* desc = description;
    AttendanceComponent* found = FindAttendance(exclude, desc->_professions[0]);
    if (found != nullptr) {
        CitizenComponent* keyIBP = this;
        if (CitizenComp_sub_40f0(found, this)) {
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(&found->queue)
                .SetSizeFill(found->queueCount + 1, &keyIBP);
            school = found;
            field_0x220 = true;
            return 1;
        }
    }
    return 0;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include <cstdlib>
#include <cstring>

// ------------------------------------------------------------------
// Phase 3 / Keystone D — CitizenComponent Startup-only live-wiring (Brick 2).
//
// CitizenComponent's per-tick Update is the 711-instruction lifecycle monster
// (hunger/thirst/aging/workplace/housing/partner) with ~25 sub-calls — NOT yet
// safe to run live in standalone. But the sibling CitizenAIComponent (Brick 1)
// bails until this component's BODY is populated (its CitizenDescription at +0x288
// + the Family/Health component at +0x278). That population happens in
// CitizenComponent::Startup (slot +0x20, 0x140114050), NOT its Update.
//
// So this brick brings ONLY the Startup slot live (COMPONENT_LIVE_FACTORY_STARTUP)
// to populate the body at spawn, leaving the heavy Update an inert no-op. That
// lets the CitizenAIComponent decision loop actually RUN (read citizen needs)
// instead of guard-bailing — without the risk of the full citizen lifecycle tick.
// Default (env off) => inert => byte-unchanged.
// ------------------------------------------------------------------

// Self-contained sibling-component find by registered type name (the real-CRC
// source the inert/live factories use for vtable+0x10). Avoids the exe finders
// (Family 0x1400012d0 / MeshInstance 0x140002f90), which key off the .rdata
// type-id globals (DAT_1401c7078 CharacterComponent / DWORD_1401c7e08
// StatusIconComponent) — those are 0xFFFFFFFF in standalone and filling them
// would un-dormant ~30 unrelated readers (same reasoning as CitizenAI's find).
static Component* CitizenComp_FindSiblingByTypeId(ComponentEntity* ownerEntity, uint32_t want) {
    if (!ownerEntity) return nullptr;
    if (want == 0 || want == 0xFFFFFFFF) return nullptr;
    int count = ownerEntity->_componentCount;
    Object::InternalPtr<Component>* arr = ownerEntity->_components;
    for (int i = 0; i < count; i++) {
        Component* comp = arr[i].Get();
        if (!comp) continue;
        uint32_t tid = comp->Type();
        if (tid == want)
            return Object::IsDerivedFromNullSafe(comp, want) ? comp : nullptr;
    }
    return nullptr;
}

// ------------------------------------------------------------------
// ENDGAME B.3.8 CIT — CitizenComponent as a REAL class (CitizenComponent.h).
//
// Before: a manual COMPONENT_LIVE_FACTORY_S expansion — a 48-slot
// CitizenComponent_LiveVtable filled at every create (slots 3 Serialize,
// 4 Startup, 6 OnAdd, 7 OnRemove, 9 Update, 11 post-load re-apply, 15 message
// handler), Inert_ScalarDtor in slot 0, and CitizenComponent_CtorScalars patched
// in after the zero-fill. After: the compiler's 16-slot vtable, a real ctor
// (the scalars are the ctor's own) and a real dtor chain (B38_RULES D1, D2).
// ------------------------------------------------------------------
#include "Component/CitizenComponent.h"


// 0x140114770. Component() is the CALL 0x1401188b0; the 0x140162580 stamp is
// the compiler's; flags |= 0xcb. The scalars are retail's and MUST be set at
// CONSTRUCTION, not at Startup: retail's load path (ComponentEntity::Serialize
// 0x1400c3140) rebuilds a stream-restored component through
// Object::Factory::CreateObject -> ctor and then calls ONLY RefreshEntityFlags —
// Startup is never dispatched for it. Retail's serializer (0x140114160)
// re-supplies +0x30/+0xa8/+0x1e0 from the stream but deliberately SKIPS +0x1dc,
// the live Graphics::Scene object handle, because the ctor already put -1 there.
// With +0x1dc = 0 on a load, HideTool (0x1401176e0) passed its `>= 0` guard and
// called Scene::RemoveObject(scene, 0) on a handle the scene never issued, whose
// parent walk never terminates (the post-load freeze).
//   +0x30 = -1, +0xa8 = -1, +0x1d8 = -1 (qword: toolId AND sceneObjectId),
//   +0x1e0 = 4, everything else 0.
// PORT GAP (retail 0x140114808): retail runs Graphics::MeshInstance::MeshInstance()
// on +0xb8; the tree keeps those 0x120 bytes zero-filled (CitizenComponent.h).
CitizenComponent::CitizenComponent()
    : name(0), field_0x30(-1), field_0x34(0), gender(0), age(0), maxAge(0), hunger(0),
      coldness(0), pregnancyTimer(0), health(0), statusFlags(0), foodTimer(0),
      field_0x60(0), disease(nullptr), field_0x70(0), field_0x78(0), happiness(0),
      field_0x84(0), happinessFlags(0), _pad8C{}, happinessTimer(0),
      consumedTypes(nullptr), consumedTypeCount(0), consumedTypeCapacity(0),
      field_0xA8(-1), field_0xAC(0), field_0xB0(0), _padB4{},
      meshInstance{}, toolId(-1), toolSceneId(-1), field_0x1E0(4), clothingLevel(0),
      clothingTimer(0), profession(nullptr), pendingProfession(nullptr),
      field_0x200(0), school(nullptr), education(0), _pad214{}, schoolTime(0),
      field_0x220(false), _pad221{}, careBuilding(nullptr), field_0x230(nullptr),
      home(nullptr), homeSlot(0), _pad244{}, workplace(nullptr), spouse(nullptr),
      _parent(nullptr), _firstChild(nullptr), _nextSibling(nullptr),
      _prevSibling(nullptr), character(nullptr), statusIcon(nullptr),
      description(nullptr) {
    OrFlagsInCtor(0xcb);
}


// [4] 0x140114050.
// Faithful port of CitizenComponent::Startup (0x140114050) + the ctor scalar init
// (0x140114770) the inert factory zeroed. The two sibling finds are done
// self-contained (see above) instead of via the global-keyed exe finders.
void CitizenComponent::Startup() {
    // ctor scalars moved to CitizenComponent_CtorScalars — see the note there.


    ComponentEntity* owner = _entity;
    if (!owner) return;   // not yet wired into an entity

    // +0x278 = CharacterComponent IBP ("Family" finder; CitizenAI reads its +0x60
    // char-index as the alive flag: index>=0 alive, -1 dead).
    character = static_cast<CharacterComponent*>(
        CitizenComp_FindSiblingByTypeId(owner, CharacterComponent::TypeId()));
    // +0x280 = StatusIconComponent IBP ("MeshInstance" finder; used by the heavy
    // Update only — not by CitizenAI — so a null here is harmless for Brick 2).
    statusIcon = static_cast<StatusIconComponent*>(
        CitizenComp_FindSiblingByTypeId(owner, StatusIconComponent::TypeId()));

    // +0x288 = CitizenDescription: trust comp+0x18 (the copy-ctor stored it).
    // The original validates IsDerivedFrom(obj, CitizenDescription 0x1401c7190),
    // but that is unreliable in standalone's partial factory tree (GrowthComponent
    // precedent), so trust the +0x18 pointer directly. Component::_description is a
    // void* member (Component.h): the pointer is taken as the description it holds.
    description = reinterpret_cast<CitizenDescription*>(_description);
    if (description) {
        health = description->_f6c;
        happiness = description->maxHappiness;

        // --- ADULT-AGE SPAWN FIX (env BANISHED_SPAWN_ADULTS) -------------------
        // Our simplified start-town spawn (StartTown_SpawnCitizens) only CopyCtors
        // + registers colonist entities; it skips the real per-citizen init
        // (FUN_140117080) that assigns age/maxAge. So every citizen keeps age=0
        // (< adultAge desc+0x44 = 2.0) -> perpetual child -> the CitizenComponent
        // lifecycle never runs the adult housing/workplace assignment section
        // (citizencomponent.cpp:633) -> never housed/employed -> the Work state is
        // never a CitizenAI candidate (project_economy_loop). Mirror the founder
        // adult age the real spawn pool (0x1400a0dd0) gives:
        //     age = RandFloat*(reproMax*0.5 - adultMin) + adultMin
        // (desc+0x50 adultMin=2.0, desc+0x54 reproMax=8.0  -> age in [2.0, 4.0]),
        // and the randomized lifespan (FUN_140117080): maxAge = RandFloatTol*desc+0x3c
        // + desc+0x38 (base 15.0, tol +/-2.0). maxAge MUST be set: the inert factory
        // zeroes it, and the Update old-age death check (maxAge <= age) would fire
        // immediately once a non-zero adult age is present. Default-off so a plain
        // BANISHED_LIVE_CITIZEN run stays byte-identical.
    }
}



// create 0x140030110: MemAlloc(0x290, 8), then the ctor.
void* CitizenComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CitizenComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CitizenComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* CitizenComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CitizenComponent());
}

DEFINE_EXE_TYPE(CitizenComponent, "Component")

// ============================================================
// B4 FORWARD: the C spellings of the members above that callers outside this block
// still use (B4_RULES W1 "Callers outside your files"); each takes the old signature.
// ============================================================

// B4 FORWARD: 1 outside caller(s) (standalone/death_harness.cpp)
void CitizenComp_sub_1fa0(int64_t selfV, void* param2) {
    reinterpret_cast<CitizenComponent*>(selfV)->SetParent(*reinterpret_cast<CitizenComponent**>(param2));
}

// B4 FORWARD: 2 outside caller(s) (Component/workplacecomponent.cpp,
// Component/destroycomponent_workerrelease.cpp)
int64_t* CitizenComp_sub_ffc0(int64_t param_1, int64_t* param_2) {
    *reinterpret_cast<ComponentEntity**>(param_2) =
        reinterpret_cast<CitizenComponent*>(param_1)->FindWorkplace();
    return param_2;
}

// B4 FORWARD: 2 outside caller(s) (Component/workplacecomponent.cpp,
// Component/destroycomponent_workerrelease.cpp)
int64_t* CitizenComp_sub_0470(int64_t param_1, int64_t* param_2) {
    *reinterpret_cast<ComponentEntity**>(param_2) =
        reinterpret_cast<CitizenComponent*>(param_1)->FindRankedWorkplace();
    return param_2;
}

// B4 FORWARD: 1 outside caller(s) (standalone/death_harness.cpp)
void CitizenComp_helper_1b90(int64_t selfV) {
    reinterpret_cast<CitizenComponent*>(selfV)->GiveBirth();
}

// B4 FORWARD: 6 outside caller(s) (Component/destroycomponent_workerrelease.cpp,
// Component/workplacecomponent.cpp, Dialog/townhall_helpers.cpp, Entity/citizenbehavior.cpp,
// Game/gameupdateloop.cpp, Game/starttown.cpp) and the town-manager bodies of this block
void CitizenComp_AssignHousing(CitizenComponent* param_1, int64_t* param_2) {
    param_1->SetWorkplace(*reinterpret_cast<ComponentEntity**>(param_2));
}

// B4 FORWARD: 2 outside caller(s) (Component/attendancecomponent.cpp,
// standalone/food_panel_harness.cpp)
char CitizenComp_TryEnrolInSchool(CitizenComponent* param_1, int64_t* param_2) {
    return param_1->TryEnrolInSchool(*reinterpret_cast<AttendanceComponent**>(param_2));
}
