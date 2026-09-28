// code/Game/Goal/tutorialgoalcount.cpp
// Clean reimplementation of TutorialGoalCount vtable methods.
//
// TutorialGoalCount (0x50 bytes) — tutorial goal counting items/buildings/citizens.
// Vtable: 0x140160950
// Extends TutorialGoal -> Object::External
//
// The Check function (Slot8) is a large switch on countMode (+0x20) with 11 cases.
// Each case counts a different game metric and compares against targetCount (+0x24).
// If greaterOrEqual (+0x28), checks count >= target; else checks count == target.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialGoalCount.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "Tutorial/exe_bodies.h"
#include "Goal/goal_exe_helpers.h"
#include "object/TypeInfo.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "Component/ComponentDescription.h"
#include "Game/Profession.h"
#include <new>
#include "ui/SpinnerElement.h"   // ENDGAME B.3.6 UI callers (K4)

// ============================================================
// Exe-internal helpers, called by name:
//   0x1400265b0 StatGoal_ResolveExternalPtr (Goal/goal_exe_helpers.h) -- resolve the
//               External reference named by a String (lazy load)
//   0x140092010 TutBase_CountBuildingsByType   -- world BST at +0x160, returns node[2]
//   0x1400910a0 TutBase_CountProfessionsByType -- world BST at +0x100
//   0x14012fbb0 WorkPlaceComponent_GetMaxWorkers -- available job slots of a
//               WorkPlaceComponent (WorkComponent+0x30, lazily via 0x14012e5c0)
//   0x140013920 TutBase_CastToSliderElement -- IsDerivedFrom SpinnerElement, else null
//   0x1400e1870 FindUIElementByPath_Impl (find_ui_element_by_path.cpp)
//   (Tutorial/exe_bodies.h)
// ============================================================

// 0x1401c7318 is ComponentDescription's cell, 0x1401c5a98 is Profession's (Table 1).
// Neither result is cached (matching the file's existing no-cache pattern for
// every other lazily-resolved pointer here); a plain read is cheap and this
// runs only on the single game-update thread.
static uint32_t s_tidBuildingDesc;
static uint32_t* TypeIdBuildingDesc() {       // cases 0, 7, 10
    s_tidBuildingDesc = ComponentDescription::TypeId();
    return &s_tidBuildingDesc;
}
static uint32_t s_tidProfessionDesc;
static uint32_t* TypeIdProfessionDesc() {     // cases 1, 3, 6
    s_tidProfessionDesc = Profession::TypeId();
    return &s_tidProfessionDesc;
}

// ============================================================
// BST lookup helper — searches a binary search tree keyed by pointer
// ============================================================
// BST node layout (8 bytes each slot):
//   [0] = key (pointer value used as comparison key)
//   [1] = value (context-dependent: array ptr, count, etc.)
//   [2] = count (for some BSTs)
//   [3] = (unused/parent?)
//   [4] = left child
//   [5] = right child
static uintptr_t* BSTFind(uintptr_t root, uintptr_t key) {
    uintptr_t* node = (uintptr_t*)root;
    while (node != nullptr) {
        uintptr_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[4];  // left
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[5];  // right
        } else {
            return node;  // found
        }
    }
    return nullptr;
}

// The per-building trade-order tree (BuildingComponent+0x60) is a DIFFERENT node
// layout from the world-manager trees above. tradeui.cpp:2907 documents it as
//   { key +0x00, value +0x08, colour +0x10, left +0x18, right +0x20 }
// and retail's own case-7 descent (0x1400df290) confirms it:
//   if (key < *node) node = node[3];        // left  = +0x18
//   else { if (key <= *node) { v = node[1]; break; } node = node[4]; }   // right = +0x20
//
// Walking it with the +0x20/+0x28 offsets above reads the RIGHT child as LEFT and
// runs off the end of the node as RIGHT. That still "works" for a key sitting at
// the ROOT, which is why the tutorial's leather-coat goal passed while the iron-tool
// goal — a non-root node — read 0 forever and stalled the trade tutorial.
static uintptr_t* TradeOrderBSTFind(uintptr_t root, uintptr_t key) {
    uintptr_t* node = (uintptr_t*)root;
    while (node != nullptr) {
        uintptr_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[3];  // left  (+0x18)
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[4];  // right (+0x20)
        } else {
            return node;  // found
        }
    }
    return nullptr;
}

// ============================================================
// Resolve external type pointer from self->typeName
// ============================================================
// Calls FUN_1400265b0 to ensure external ref is loaded, then reads
// self->_field40 and checks IsDerivedFrom against the given type ID.
// Returns the type pointer if it matches, or 0 if not.
static uintptr_t ResolveTypePtr(uintptr_t self, uint32_t* typeId) {
    StatGoal_ResolveExternalPtr((void*)(self + 0x30));
    uintptr_t typePtr = *(uintptr_t*)(self + 0x40);
    if (typePtr && typeId) {
        if (Object::IsDerivedFromNullSafe((void*)typePtr, *typeId)) {
            return typePtr;
        }
    }
    return 0;
}

// ============================================================
// TutorialGoalCount construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400dfa60 (inlined in create 0x1400277b0; createInPlace 0x1400277f0
// null-checks then runs it): External(), the TutorialGoal and
// TutorialGoalCount stamps (the compiler's), then the fields.
TutorialGoalCount::TutorialGoalCount()
    : countMode(0), targetCount(0),          // one qword store at +0x20
      greaterOrEqual(0), _pad29{},
      typeName(0),                           // null string
      _field38(-1),                          // generation sentinel
      _pad3C{}, _field40(nullptr),
      elementName(0) {}                      // null string

// The dtor inside [0] 0x140027820: ~String +0x48, ~String +0x30; the
// compiler re-stamps TutorialGoal and runs ~External.
TutorialGoalCount::~TutorialGoalCount() {
    reinterpret_cast<System::String*>(&elementName)->~String();   // B.3.1 bridge
    reinterpret_cast<System::String*>(&typeName)->~String();      // B.3.1 bridge
}

// ============================================================
// TutorialGoalCount::Serialize (vtable +0x28, 0x1400df900)
// ============================================================
// Serializes all fields. After loading (version == 0) resolves the external
// ref at +0x30.
bool TutorialGoalCount::Serialize(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    TutorialGoal::Serialize(stream);   // QUALIFIED: the base body (an empty Serializer scope)

    // Create serializer for our fields
    alignas(8) uint8_t ser[32];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // +0x20 through the UINT door (rw_vtbl::kScalarUInt, readerWriter +0x98).
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x20), nullptr);   // B.3.4 bridge  (0x98 = uint door)

    // Serialize targetCount (+0x24) as int
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x24), nullptr);   // B.3.4 bridge

    // Serialize typeName (+0x30) as String
    reinterpret_cast<System::String*>(s + 0x30)->Serialize(ser);   // B.3.1 bridge

    // If loading (serializer version == 0), resolve the external ref
    int version = reinterpret_cast<IO::Serializer*>(ser)->GetVersion();   // B.3.4 bridge
    if (version == 0) {
        StatGoal_ResolveExternalPtr((void*)(s + 0x30));
    }

    // Serialize greaterOrEqual (+0x28) as bool
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(s + 0x28), nullptr);   // B.3.4 bridge

    // Serialize elementName (+0x48) as String
    reinterpret_cast<System::String*>(s + 0x48)->Serialize(ser);   // B.3.1 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// TutorialGoalCount::Reconstruct [6] 0x140027800 / Destruct [7] 0x140001640
// ============================================================
// `~T()` (slot 0, flags 0), then the ctor (0x1400dfa60) on `this`.
Object::External* TutorialGoalCount::Reconstruct() {
    this->~TutorialGoalCount();
    return ::new (static_cast<void*>(this)) TutorialGoalCount();
}
void TutorialGoalCount::Destruct() { this->~TutorialGoalCount(); }

// ============================================================
// TutorialGoalCount::Check (vtable +0x40, 0x1400df290)
// ============================================================
// 11-case switch on countMode (+0x20). Counts a game metric and
// compares against targetCount (+0x24). If greaterOrEqual (+0x28),
// returns count >= target; otherwise returns count == target.
//
// Count modes:
//   0: Building count by type (BST at worldData+0x160)
//   1: Citizens assigned to a profession type
//   2: Adult population (worldData+0x28)
//   3: Profession slot count by type (BST at worldData+0x100)
//   4: Total population including children (worldData+0x08)
//   5: Has trade posts (merchantList at worldData2+0xF0 > 0)
//   6: Stored resource count across buildings of a profession type
//   7: Building storage total by building type (BST at worldData+0x160, iterate)
//   8: Total stored items across all buildings
//   9: UI spinner/slider element value (by elementName path)
//  10: Occupied building count by type
//
// param2: unused (game world passed implicitly via g_pGameWorldSingleton)
// param3: InternalBasePtr* — retail's case 9 CheckPtr'd it and read nothing else
// from it, so with CheckPtr deleted (B.3.3 step 6D) nothing here reads it. The
// PARAMETER stays: this is retail's signature and the caller still passes it.
//
// Each int result is answered as `!= 0` (the bool return), exactly as the
// member delegate did before this fold.
// [8] 0x1400df290 — the 11-mode count switch; StepExecute reads the result.
bool TutorialGoalCount::IsComplete(void* param2, void* param3) {
    (void)param3;

    uintptr_t s = (uintptr_t)this;
    uint32_t mode = *(uint32_t*)(s + 0x20);
    int count = 0;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) {
        goto compare;
    }

    switch (mode) {
    // --------------------------------------------------------
    // Case 0: Count buildings of a type
    // --------------------------------------------------------
    case 0: {
        uintptr_t typePtr = ResolveTypePtr(s, TypeIdBuildingDesc());
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (worldData) {
            count = (int)TutBase_CountBuildingsByType(worldData, typePtr);
        }
        break;
    }

    // --------------------------------------------------------
    // Case 1: Count citizens assigned to a specific profession
    // --------------------------------------------------------
    // Iterates citizen list (worldData+0xA0, count at +0xA8).
    // For each alive citizen (health >= deathThreshold && no death flag),
    // checks if their assigned profession matches the target type.
    case 1: {
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (!worldData) break;
        int citizenCount = *(int*)(worldData + 0xa8);
        uintptr_t citizenArray = *(uintptr_t*)(worldData + 0xa0);
        int matched = 0;
        for (int i = 0; i < citizenCount; i++) {
            // Each citizen is an IBP at citizenArray + i*8
            uintptr_t citizenIBP = *(uintptr_t*)(citizenArray + (uintptr_t)i * 8);
            // Store in local for CheckPtr
            uintptr_t localCitizen = citizenIBP;
            uintptr_t citizen = localCitizen;
            if (!citizen) continue;

            // Check alive: health (float at +0x3c) >= deathThreshold (float at citizen->desc(+0x288)+0x44)
            float health = *(float*)(citizen + 0x3c);
            uintptr_t desc = *(uintptr_t*)(citizen + 0x288);
            if (!desc) continue;
            float deathThreshold = *(float*)(desc + 0x44);
            if (health < deathThreshold) continue;

            // Check no death flag: citizen+0x208 == 0
            if (*(uintptr_t*)(citizen + 0x208) != 0) continue;

            // Get assigned profession type: citizen+0x1F0
            uintptr_t profType = *(uintptr_t*)(localCitizen + 0x1f0);

            // Resolve target profession type and compare
            uintptr_t targetProf = ResolveTypePtr(s, TypeIdProfessionDesc());
            if (profType == targetProf) {
                matched++;
            }
        }
        count = matched;
        break;
    }

    // --------------------------------------------------------
    // Case 2: Adult population count (worldData+0x28)
    // --------------------------------------------------------
    case 2: {
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (worldData) {
            count = *(int*)(worldData + 0x28);
        }
        break;
    }

    // --------------------------------------------------------
    // Case 3: Profession slot count by type
    // --------------------------------------------------------
    case 3: {
        uintptr_t typePtr = ResolveTypePtr(s, TypeIdProfessionDesc());
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (worldData) {
            count = (int)TutBase_CountProfessionsByType(worldData, typePtr);
        }
        break;
    }

    // --------------------------------------------------------
    // Case 4: StorageLocation count — the DynArray at townMgr+0x00/+0x08
    // --------------------------------------------------------
    // Retail is just `*(uint*)(townMgr + 8)`. That is NOT "total population" as this
    // case was previously labelled: townMgr+0x00 is the DATA pointer and +0x08 the
    // COUNT of a DynArray of storage-location entity IBPs — the array
    // StorageLocationComponent's remove path (0x14012d270) erases from with
    // DynArray8_EraseByValue(townMgr, entityIBP). The .rsc name for this CountType is
    // `StorageLocation` (goalstoragebarnfinished).
    //
    // This matters because TutorialGoalCount compares with EQUALITY unless the goal
    // sets `bool _greaterEqual = true` — only two goals in the whole tutorial do. So
    // `goalstoragebarnfinished { _count = 3 }` passes ONLY at exactly 3, and an
    // off-by-one here is a permanent stall, not a delay. Observed 2026-09-04 at
    // stepCutter7 (step 60): count=4 target=3 ge=0, forever.
    //
    // BANISHED_TUTORIAL_TRACE also dumps the array so the extra entry can be
    // identified by its description pointer (correlate with the [TutRestrict]/[GoalRef]
    // lines, which print the resolved description for each Template/*.rsc).
    case 4: {
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (worldData) {
            count = *(int*)(worldData + 0x08);

        }
        break;
    }

    // --------------------------------------------------------
    // Case 5: Has trade posts (merchant list count > 0)
    // --------------------------------------------------------
    case 5: {
        uintptr_t worldData2 = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
        if (worldData2) {
            count = (*(int*)(worldData2 + 0xf0) > 0) ? 1 : 0;
        }
        break;
    }

    // --------------------------------------------------------
    // Case 6: Available job slots (work capacity) across buildings of a profession type
    // --------------------------------------------------------
    // Gets profession count from BST at worldData+0x100, then for each entity
    // of that profession type, finds their WorkPlaceComponent (type 0x1401c80e0),
    // checks if active (+0x50 != 0), and sums work capacity via helper_fbb0.
    // helper_fbb0 internally finds WorkComponent on the parent entity and returns
    // its cached capacity field (+0x30), lazily computing it if needed.
    case 6: {
        // Resolve profession type (re-resolved inside loop to match original)
        uintptr_t profType = ResolveTypePtr(s, TypeIdProfessionDesc());
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (!worldData) break;

        int profCount = (int)TutBase_CountProfessionsByType(worldData, profType);
        int total = 0;

        for (int i = 0; i < profCount; i++) {
            // Re-resolve profession type each iteration (matches original)
            uintptr_t curProfType = ResolveTypePtr(s, TypeIdProfessionDesc());

            // BST lookup at worldData+0x100 for this profession type
            // Re-read worldData each iteration (matches original)
            worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
            uintptr_t bstRoot = *(uintptr_t*)(worldData + 0x100);
            uintptr_t* node = BSTFind(bstRoot, curProfType);
            if (!node) continue;

            // node[1] = array of entity IBPs, indexed by i
            uintptr_t entityIBP = *(uintptr_t*)(node[1] + (uintptr_t)i * 8);
            uintptr_t localEntity = entityIBP;

            // Find WorkPlaceComponent (FUN_140002e10, type 0x1401c80e0)
            uintptr_t workplaceIBP = 0;
            fn_FindComponent_WorkPlace((void*)localEntity, &workplaceIBP);

            // Check if workplace is active (byte at +0x50 != 0)
            uintptr_t workplace = workplaceIBP;
            if (workplace && *(uint8_t*)(workplace + 0x50) != 0) {
                // Get work capacity (available job slots)
                total += WorkPlaceComponent_GetMaxWorkers(workplaceIBP);
            }
        }
        count = total;
        break;
    }

    // --------------------------------------------------------
    // Case 7: Building storage total by building type
    // --------------------------------------------------------
    // Iterates all entities (worldData+0x30 array, count at +0x38),
    // finds their BuildingComponent, checks if type matches,
    // then sums from the BuildingComponent's storage map at +0x60.
    case 7: {
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (!worldData) break;
        int entityCount = *(int*)(worldData + 0x38);
        uintptr_t entityArray = *(uintptr_t*)(worldData + 0x30);
        int total = 0;

        for (int i = 0; i < entityCount; i++) {
            // Get entity IBP
            uintptr_t entityIBP = *(uintptr_t*)(entityArray + (uintptr_t)i * 8);
            uintptr_t localEntity = entityIBP;

            // Find BuildingComponent
            uintptr_t buildingIBP = 0;
            fn_FindComponent_Building((void*)localEntity, &buildingIBP);

            // Resolve building description type
            uintptr_t typePtr = ResolveTypePtr(s, TypeIdBuildingDesc());

            // CheckPtr and get building component
            uintptr_t building = buildingIBP;
            if (!building) continue;

            // Search the building's storage BST at +0x60 for matching type
            uintptr_t storageBST = *(uintptr_t*)(building + 0x60);
            uintptr_t* storageNode = TradeOrderBSTFind(storageBST, typePtr);

            int storageCount = 0;
            if (storageNode != nullptr) {
                storageCount = (int)storageNode[1];
            }
            total += storageCount;

            // Re-read worldData for next iteration (may change)
            worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        }
        count = total;
        break;
    }

    // --------------------------------------------------------
    // Case 8: Total stored items across all buildings
    // --------------------------------------------------------
    // Iterates all entities, finds StorageComponent, sums all
    // item counts from the storage's item array.
    case 8: {
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (!worldData) break;
        int entityCount = *(int*)(worldData + 0x38);
        uintptr_t entityArray = *(uintptr_t*)(worldData + 0x30);
        int total = 0;

        for (int i = 0; i < entityCount; i++) {
            // Get entity IBP
            uintptr_t entityIBP = *(uintptr_t*)(entityArray + (uintptr_t)i * 8);
            uintptr_t localEntity = entityIBP;

            // Find StorageComponent
            uintptr_t storageIBP = 0;
            fn_FindComponent_Storage((void*)localEntity, &storageIBP);
            uintptr_t storage = storageIBP;
            if (!storage) continue;

            // Iterate storage items: count at +0x30, array ptr at +0x28
            // Each item is 16 bytes (stride 2 qwords), count value at offset -8 from item+16
            int itemCount = *(int*)(storage + 0x30);
            for (int j = 0; j < itemCount; j++) {
                storage = storageIBP;
                uintptr_t itemArray = *(uintptr_t*)(storage + 0x28);
                // Items at stride 16 bytes (2 qwords): count at itemArray + j*16 + 8
                int itemStored = *(int*)(itemArray + (uintptr_t)j * 16 + 8);
                total += itemStored;
            }

            // Re-read worldData for next iteration
            worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        }
        count = total;
        break;
    }

    // --------------------------------------------------------
    // Case 9: UI spinner/slider element value
    // --------------------------------------------------------
    // Finds a UI element by the elementName path (+0x48),
    // casts it to SpinnerElement, reads its value if not active.
    case 9: {
        // Find UI element by path
        uintptr_t foundIBP[2] = {0, 0};  // InternalBasePtr buffer
        FindUIElementByPath_Impl(foundIBP, (void*)(s + 0x48), nullptr);   // 0x1400e1870

        uintptr_t element = foundIBP[0];
        if (!element) { break; }

        // Cast to SliderElement (checks IsDerivedFrom SliderElement type)
        void* slider = TutBase_CastToSliderElement((void*)element);
        uintptr_t sliderIBP = (uintptr_t)slider;
        if (!sliderIBP) { break; }

        // Create IBP and check if active
        if (reinterpret_cast<const UI::SpinnerElement*>(sliderIBP)->IsActive()) {  // B.3.6 bridge
            // Retail refuses to read a spinner mid-edit. If this never clears, the
            // goal stalls forever with the right number visible on screen.
            break;
        }

        // Read current value
        count = reinterpret_cast<const UI::SpinnerElement*>(sliderIBP)->GetValue();  // B.3.6 bridge
        break;
    }

    // --------------------------------------------------------
    // Case 10: Occupied building count by type
    // --------------------------------------------------------
    // Gets building count by type, then for each building in the BST,
    // finds the BuildStateComponent and checks occupancy flags.
    case 10: {
        // Get building description type
        uintptr_t typePtr = ResolveTypePtr(s, TypeIdBuildingDesc());
        uintptr_t worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        if (!worldData) break;

        int buildingCount = (int)TutBase_CountBuildingsByType(worldData, typePtr);
        int occupied = 0;

        for (int i = 0; i < buildingCount; i++) {
            // Look up building type in BST at worldData+0x160
            // Need to re-resolve type each iteration (original code does this)
            uintptr_t curTypePtr = ResolveTypePtr(s, TypeIdBuildingDesc());
            worldData = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
            uintptr_t bstRoot = *(uintptr_t*)(worldData + 0x160);
            uintptr_t* node = BSTFind(bstRoot, curTypePtr);

            uintptr_t entityIBP = 0;
            if (node) {
                // node[1] = array of entity IBPs
                entityIBP = *(uintptr_t*)(node[1] + (uintptr_t)i * 8);
            } else {
                // Not found — initialize empty IBP
                new (&entityIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            }

            // CheckPtr and find BuildStateComponent
            uintptr_t buildStateIBP = 0;
            fn_FindComponent_BuildState((void*)entityIBP, &buildStateIBP);
            uintptr_t buildState = buildStateIBP;
            if (!buildState) continue;

            // Check occupancy: byte at +0x52 (if negative/high bit set, use +0x50 instead)
            uint8_t flag52 = *(uint8_t*)(buildState + 0x52);
            uint8_t checkFlag = flag52;
            if ((int8_t)flag52 < 0) {
                checkFlag = *(uint8_t*)(buildState + 0x50);
            }
            if (checkFlag != 0) {
                occupied++;
            }
        }
        count = occupied;
        break;
    }

    default:
        // Unknown mode — count stays 0, skip comparison
        goto compare;
    }

compare:

    // Final comparison: greaterOrEqual ? (count >= target) : (count == target)
    if (*(uint8_t*)(s + 0x28) != 0) {
        return (count >= *(int32_t*)(s + 0x24)) ? 1 : 0;
    }
    return (count == *(int32_t*)(s + 0x24)) ? 1 : 0;
}


// [9] 0x1400277f0: null-check, then the ctor (0x1400dfa60) on `this`.
Object::External* TutorialGoalCount::Construct() {
    return ::new (static_cast<void*>(this)) TutorialGoalCount();
}

// create 0x1400277b0: MemAlloc(0x50, 8), then the ctor.
void* TutorialGoalCount::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialGoalCount), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialGoalCount());
}
// createInPlace 0x1400277f0.
void* TutorialGoalCount::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialGoalCount());
}

DEFINE_EXE_TYPE(TutorialGoalCount, "TutorialGoal")
