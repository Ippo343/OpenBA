// engine/src/ui/buildui.cpp
// Clean reimplementation of BuildUI panel methods.
//
// BuildUI (0x50 bytes): Building info panel showing build progress.
// Layout:
//   +0x20 = dialog element (InternalBasePtr, from DialogController)
//   +0x28 = entity data source (InternalBasePtr, from DialogController)
//   +0x30 = row array ptr (each 0x10: labelStorage ptr, textTotal ptr)
//   +0x38 = row array count (int32)
//   +0x3C = row array capacity (int32)
//   +0x40 = progress element (InternalBasePtr)
//   +0x48 = textTotal element (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)

#ifdef _DEBUG
// Debug sprite logging wrappers (runtime/src/ui/)
extern void* Debug_Sprite_Ctor(void* retval, uint64_t spriteSheet, const void* spriteName);
extern void* Debug_Sprite_DefaultCtor(void* retval);
extern void Debug_ImageElement_SetSprite(void* self, void* spriteData);
extern void Debug_LabelElement_SetSprite(void* self, void* spriteData);
#endif

// ============================================================
// CountOccupiedBuildTiles (0x140118330)
// ============================================================
// Counts tiles in the entity's building footprint that are occupied
// by other entities (not self). Uses the BuildComponent footprint
// to iterate the tile grid and checks each tile's occupant.
//
// param: smComp = StorageComponent ptr (smComp in BuildUI_Slot7)
// Returns: count of occupied tiles not owned by self.
static int CountOccupiedBuildTiles_Impl(void* smComp)
{
    uintptr_t sc = (uintptr_t)smComp;

    // Entity InternalBasePtr at smComp+0x10
    uintptr_t entityRef = *(uintptr_t*)(sc + 0x10);  // inner ptr value = self identity

    // Find BuildComponent on entity
    uint64_t buildComp = 0;
    fn_FindComponent_Build((void*)entityRef, &buildComp);
    uintptr_t bc = buildComp;

    // BuildComponent footprint fields:
    //   +0x28 = originX (short), +0x2a = originY (short)
    //   +0x2c = width (short),   +0x2e = height (short)
    int originX = (int)*(int16_t*)(bc + 0x28);
    int originY = (int)*(int16_t*)(bc + 0x2a);
    int width   = (int)*(int16_t*)(bc + 0x2c);
    int height  = (int)*(int16_t*)(bc + 0x2e);

    // WorldGrid at gameWorld+0xF8
    // piVar3 = *(int**)(gameWorld + 0xF8)
    // *piVar3 = mapWidth (int), *(piVar3+0x20) = tile array base ptr
    void* gameWorld = *g_pGameWorldSingleton;
    int* piVar3 = *(int**)((uintptr_t)gameWorld + 0xF8);
    int mapWidth = *piVar3;
    uintptr_t tileArrayBase = *(uintptr_t*)((uintptr_t)piVar3 + 0x20 * sizeof(int));

    int count = 0;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            // Each tile entry is 0x30 bytes; occupant ptr is at offset +8
            int tileIndex = (originY + row) * mapWidth + originX + col;
            uintptr_t occupant = *(uintptr_t*)(tileArrayBase + 8 + (uintptr_t)tileIndex * 0x30);
            // Count tiles occupied by other entities (not self, not null)
            if (occupant != *(uintptr_t*)(sc + 0x10) && occupant != 0) {
                count++;
            }
        }
    }
    return count;
}

// Helper: find element by name in parent element, returns raw InternalPtr value
static void* BuildUI_FindChildElem(void* parentElem, const char* name) {
    uint8_t retBuf[16];
    System::String nameStr(name);
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameStr));  // B.3.6 bridge
    void* result = *(void**)retBuf;
    return result;
}

// Helper: find element by name, wrap result as InternalBasePtr
static uint64_t BuildUI_FindElemAsBasePtr(void* parentElem, const char* name) {
    void* raw = BuildUI_FindChildElem(parentElem, name);
    uint8_t bpBuf[8];
    new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(raw));   // B.3.3 bridge
    return *(uint64_t*)bpBuf;
}

// ============================================================
// BuildUI::Slot4 (vtable +0x20) — Setup/populate build storage table
// ============================================================
// Finds "storageTable", "progress", "buttonPause" elements.
// Iterates storage table rows, populating each with resource sprite,
// label, and required amount from the ToolbarDescription.
// Sets up the progress bar and pause button state.
//
// Original: 0x1400B1F40.

void BuildUI_Slot4(BuildUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "storageTable" element, wrap as InternalBasePtr
    void* dialogElem = *(void**)(s + 0x20);
    uint64_t tableElem = BuildUI_FindElemAsBasePtr(dialogElem, "storageTable");

    // Find "progress" element, store at +0x40
    void* progressRaw = BuildUI_FindChildElem(dialogElem, "progress");
    *(uint64_t*)(s + 0x40) = (uint64_t)progressRaw;

    // Set progress max to 1.0f: *(float*)(progressElem + 0x124) = 1.0f
    *(float*)(*(uintptr_t*)(s + 0x40) + 0x124) = 1.0f;

    // Get entity from data source +0x28, check ComponentEntity
    void* entityRaw = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? entityRaw : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find PositionComponent (FUN_14000d6c0) → has ToolbarDescription at +0x18
    uint64_t posComp = 0;
    fn_FindComponent_Position((void*)entityPtr, &posComp);

    // Find WorkplaceComponent (FUN_140002e10)
    uint64_t workplaceComp = 0;
    fn_FindComponent_WorkPlace((void*)entityPtr, &workplaceComp);

    // Find "buttonPause" element, set ID 6000
    uint64_t buttonPause = BuildUI_FindElemAsBasePtr(dialogElem, "buttonPause");
    reinterpret_cast<UI::Element*>(buttonPause)->SetElementId(6000);  // B.3.6 bridge

    // Configure pause button based on workplace state
    if (workplaceComp == 0) {
        // No workplace — hide button via vtable[8](button, false)
        reinterpret_cast<UI::Element*>(buttonPause)->Enable(false);  // B.3.6 bridge: Element slot 8 Enable
    } else {
        // Set button checked state: element+0xD8 = (workplace+0x50 == 0)
        char wpEnabled = *(char*)(workplaceComp + 0x50);
        *(uint8_t*)(buttonPause + 0xD8) = (wpEnabled == 0) ? 1 : 0;
    }

    // Get ResourceComponent from PositionComponent+0x18
    void* resourceComp = nullptr;
    int resourceCount = 0;
    if (posComp != 0) {
        void* desc = *(void**)(posComp + 0x18);
        bool isResource = fn_IsDerivedFrom(desc, *g_TypeId_ResourceComponent);
        if (isResource) {
            resourceComp = desc;
        }
    }
    if (resourceComp) {
        resourceCount = *(int*)((uintptr_t)resourceComp + 0x28);
    }

    // Find SM component (FUN_14000a170) → storage info
    uint64_t smComp = 0;
    fn_FindComponent_SM((void*)entityPtr, &smComp);

    // Find first child's "textTotal" element → store at +0x48
    // First child (row 0) is the header/total row
    uint8_t childBuf[16];
    ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(0));  // B.3.6 bridge
    void* firstChild = *(void**)childBuf;
    uint64_t textTotalElem = BuildUI_FindElemAsBasePtr(firstChild, "textTotal");
    *(uint64_t*)(s + 0x48) = textTotalElem;

    // Set header textTotal: param 0 = "0", param 1 = storage capacity
    {
        System::String zeroStr("0");
        fn_TextElement_SetParameter(*(void**)(s + 0x48), 0, &zeroStr);

        // smComp (the site's build-material storage, found via fn_FindComponent_SM =
        // ClearAreaComponent) can be null in standalone if that component isn't present
        // / its type-id didn't resolve. Retail always has it; guard the deref so a
        // partial panel (no capacity number) beats a crash.
        int capacity = smComp ? *(int*)(smComp + 0x48) : 0;
        System::String capStr = System::String::Format("%d", (uint64_t)(uint32_t)capacity);
        fn_TextElement_SetParameter(*(void**)(s + 0x48), 1, &capStr);
    }

    // Iterate storage table rows (skip first child = header)
    int childCount = reinterpret_cast<const UI::Element*>(tableElem)->ChildCount();  // B.3.6 bridge
    int rowIdx = 0;

    for (int i = 1; i < childCount; i++) {
        // Get row child element
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(tableElem)->Child(i));  // B.3.6 bridge
        void* rowElem = *(void**)childBuf;

        // Find "labelStorage" and "textTotal" in this row
        uint64_t labelStorage = BuildUI_FindElemAsBasePtr(rowElem, "labelStorage");
        uint64_t rowTextTotal = BuildUI_FindElemAsBasePtr(rowElem, "textTotal");

        // Push {labelStorage, textTotal} pair into the row array at +0x30.
        // B.3.2 bridge: BuildUI+0x30 is Collection::Array<{InternalBasePtr,
        // InternalBasePtr}> (stride 0x10, count +0x38, capacity +0x3C) inside a
        // struct §B.2 has not converted, so it is reached at its offset.
        auto& rows = GameColl::ArrayAt<GameColl::Bytes<0x10, 8>>(s, 0x30);
        rows.SetSize(rows.GetSize() + 1, GameColl::Bytes<0x10, 8>(labelStorage, rowTextTotal));

        // Populate row content based on resource index
        uintptr_t resBase = resourceComp ? *(uintptr_t*)((uintptr_t)resourceComp + 0x20) : 0;
        if (rowIdx < resourceCount) {
            // Active resource row: set amount, sprite, text, tooltip
            uintptr_t resEntry = *(uintptr_t*)(resBase + rowIdx * 0x10);

            // Find RawMaterialDescription in entry's component list
            int compCount = *(int*)(resEntry + 0x30);
            void* resType = nullptr;
            for (int c = 0; c < compCount; c++) {
                void* comp = *(void**)(*(uintptr_t*)(resEntry + 0x28) + c * 8);
                // Get TypeId via vtable[2] and compare with RawMaterialDescription
                typedef int (*fn_GetTypeId_t)(void*);
                int typeId = ((fn_GetTypeId_t)(*(void***)comp)[2])(comp);
                if (typeId == (int)*g_TypeId_RawMaterialDescription) {
                    if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription)) {
                        resType = comp;
                    }
                    break;
                }
            }

            // Calculate required amount: entry_count * multiplier
            int entryCount = *(int*)(resBase + rowIdx * 0x10 + 8);
            int multiplier = *(int*)(posComp + 0x4C);
            {
                System::String reqStr =
                    System::String::Format("%d", (uint64_t)(uint32_t)(entryCount * multiplier));
                fn_TextElement_SetParameter((void*)rowTextTotal, 1, &reqStr);
            }

            // Set row textTotal param 0 = "0" initially
            {
                System::String zeroStr("0");
                fn_TextElement_SetParameter((void*)rowTextTotal, 0, &zeroStr);
            }

            if (resType) {
                // Set sprite from resource type
                uint8_t spriteBuf[16];
#ifdef _DEBUG
                Debug_Sprite_Ctor(spriteBuf, *(uint64_t*)((uintptr_t)resType + 0x20),
                               (void*)((uintptr_t)resType + 0x30));
#else
                ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)resType + 0x20))),
                               *reinterpret_cast<const System::String*>((uintptr_t)resType + 0x30));  // B.3.6 bridge
#endif
#ifdef _DEBUG
                Debug_LabelElement_SetSprite((void*)labelStorage, spriteBuf);
#else
                fn_LabelElement_SetSprite((void*)labelStorage, spriteBuf);
#endif

                // Set text from resource type
                uint8_t uiStrBuf[16];
                ::new (uiStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)resType + 0x28))),
                                 *reinterpret_cast<const System::String*>((uintptr_t)resType + 0x30));  // B.3.6 bridge
                fn_TextElement_SetText((void*)labelStorage, uiStrBuf);

                // Set tooltip: "ResourceTip" string from label's string table
                System::String tipNameStr("ResourceTip");
                uint64_t stringTable = *(uint64_t*)(labelStorage + 0x48);
                uint8_t tipStrBuf[16];
                ::new (tipStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), tipNameStr);  // B.3.6 bridge
                reinterpret_cast<UI::Element*>(labelStorage)->SetToolTip(*reinterpret_cast<const UI::String*>(tipStrBuf));  // B.3.6 bridge
            }
        } else if (rowIdx == resourceCount && resourceComp != nullptr) {
            // Total row: shows total required materials
            int totalCount = *(int*)((uintptr_t)resourceComp + 0x30);
            int multiplier = *(int*)(posComp + 0x4C);
            System::String totStr =
                System::String::Format("%d", (uint64_t)(uint32_t)(totalCount * multiplier));
            fn_TextElement_SetParameter((void*)rowTextTotal, 1, &totStr);
        } else {
            // Empty row: clear text and sprite (blanks table rows beyond the material
            // count — a WoodHouse uses only 2 of the 7 storageItem cells). This mirrors the
            // retail else-branch of 0x1400b1f40, which builds an EMPTY UI::String via the
            // DEFAULT ctor (UI::String::String()) and SetText's it. The earlier reimpl used
            // fn_UIString_Ctor_Entry — the NAMED single-arg ctor (IAT dispatch slot
            // 0x140151300, unpopulated in standalone -> null -> EXEC of 0), so the clear was
            // guarded off and the surplus rows kept their .rsc placeholder ("CountGathered"
            // /"Labor"). fn_UIString_DefaultCtor is the correct default ctor (dispatch
            // 0x140151ca8 + Runtime-export fallback) and resolves in standalone (same call
            // TradeUI/ProductionUI use to blank their surplus rows).
            if (fn_TextElement_SetText) {
                uint8_t emptyStr[16] = {};
                ::new (emptyStr) UI::String();  // B.3.6 bridge
                fn_TextElement_SetText((void*)rowTextTotal, emptyStr);

                uint8_t emptyStr2[16] = {};
                ::new (emptyStr2) UI::String();  // B.3.6 bridge
                fn_TextElement_SetText((void*)labelStorage, emptyStr2);
            }

            if (fn_LabelElement_SetSprite) {
                uint8_t emptySprite[16];
#ifdef _DEBUG
                Debug_Sprite_DefaultCtor(emptySprite);
                Debug_LabelElement_SetSprite((void*)labelStorage, emptySprite);
#else
                ::new (emptySprite) UI::Sprite();  // B.3.6 bridge
                fn_LabelElement_SetSprite((void*)labelStorage, emptySprite);
#endif
            }
        }

        rowIdx++;
    }
}

// ============================================================
// BuildUI::Slot7 (vtable +0x38) — Update build progress and materials
// ============================================================
// Updates the build progress bar and per-resource delivered amounts.
// Reads StateMachine component to determine how many materials have
// been delivered for each resource type. Calculates total progress
// ratio and displays as percentage.
//
// Original: 0x1400B1A50.

void BuildUI_Slot7(BuildUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity, check ComponentEntity
    void* entityRaw = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? entityRaw : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge
    uintptr_t entityPtr = *(uintptr_t*)entityBuf;

    // Find PositionComponent → ToolbarDescription at +0x18
    uint64_t posComp = 0;
    fn_FindComponent_Position((void*)entityPtr, &posComp);

    // Get ResourceComponent from PositionComponent+0x18
    void* resourceComp = nullptr;
    if (posComp != 0) {
        void* desc = *(void**)(posComp + 0x18);
        bool isResource = fn_IsDerivedFrom(desc, *g_TypeId_ResourceComponent);
        if (isResource) {
            resourceComp = desc;
        }
    }

    // Find SM component (storage info)
    uint64_t smComp = 0;
    fn_FindComponent_SM((void*)entityPtr, &smComp);
    // Progress/percent math below unconditionally derefs smComp (+0x48 capacity, +0x10
    // entity self-ref via CountOccupiedBuildTiles). If the build-material storage
    // component isn't found (standalone: missing component / unresolved type-id), skip
    // this update rather than fault. Slot4 already set an initial "0/total" display.
    if (smComp == 0) return;

    // Check if build is complete: totalRequired * multiplier <= posComp delivered total
    if (posComp != 0 && resourceComp != nullptr) {
        int totalRequired = *(int*)((uintptr_t)resourceComp + 0x30);
        int multiplier = *(int*)(posComp + 0x4C);
        if (totalRequired * multiplier <= *(int*)(posComp + 0x48)) {
            return;  // Build complete, nothing to update
        }
    }

    // Calculate remaining capacity: capacity - occupied tiles
    int capacity = *(int*)(smComp + 0x48);
    int occupied = CountOccupiedBuildTiles_Impl((void*)smComp);
    uint32_t remaining = (uint32_t)(capacity - occupied);

    // Update header textTotal param 0 with remaining count
    {
        System::String remStr = System::String::Format("%d", (uint64_t)remaining);
        fn_TextElement_SetParameter(*(void**)(s + 0x48), 0, &remStr);
    }

    int totalRequired = 0;
    uint32_t totalDelivered = 0;

    if (posComp == 0 || resourceComp == nullptr) {
        // No resource component — just use capacity
        totalRequired = *(int*)(smComp + 0x48);
        totalDelivered = remaining;
    } else {
        // Find StorageComponent for delivered counts (DAT_1401c7e78, FUN_140006270)
        uint64_t stateMachineComp = 0;
        fn_FindComponent_Storage((void*)*(uintptr_t*)entityBuf, &stateMachineComp);

        // Iterate resource rows, query StateMachine for delivered counts
        int resourceCount = *(int*)((uintptr_t)resourceComp + 0x28);
        int rowCount = *(int*)(s + 0x38);
        if (rowCount > resourceCount) rowCount = resourceCount;

        int sumDelivered = 0;
        int sumCapacity = 0;
        uintptr_t rowOffset = 0;

        for (int i = 0; i < rowCount; i++) {
            uintptr_t rowArrayBase = *(uintptr_t*)(s + 0x30);
            uintptr_t resBase = *(uintptr_t*)((uintptr_t)resourceComp + 0x20);
            uintptr_t resEntry = *(uintptr_t*)(resBase + rowOffset);

            // Search StateMachine component for matching resource entry
            int smCount = stateMachineComp ? *(int*)(stateMachineComp + 0x30) : 0;
            uint64_t delivered = 0;
            if (smCount > 0) {
                uintptr_t smArray = *(uintptr_t*)(stateMachineComp + 0x28);
                for (int j = 0; j < smCount; j++) {
                    uintptr_t smEntry = *(uintptr_t*)(smArray + j * 0x10);
                    if (smEntry == resEntry) {
                        if (j >= 0) {
                            // Delivered = entry[j].count - entry[j].reserved
                            int count = *(int*)(smArray + j * 0x10 + 8);
                            int reserved = *(int*)(smArray + j * 0x10 + 0xC);
                            delivered = (uint64_t)(uint32_t)(count - reserved);
                        }
                        break;
                    }
                }
            }

            // Update row textTotal param 0 with delivered count
            {
                System::String delStr = System::String::Format("%d", delivered);
                void* rowTextTotal = *(void**)(rowArrayBase + 8 + rowOffset);
                fn_TextElement_SetParameter(rowTextTotal, 0, &delStr);
            }

            sumDelivered += (int)delivered;
            int entryCount = *(int*)(resBase + i * 0x10 + 8);
            int multiplier = *(int*)(posComp + 0x4C);
            sumCapacity += entryCount * multiplier;
            rowOffset += 0x10;
        }

        // Update the total row (at index = rowCount in array)
        uintptr_t totalRowPtr = rowCount * 0x10 + *(uintptr_t*)(s + 0x30);
        {
            System::String rowStr =
                System::String::Format("%d", (uint64_t)(uint32_t)*(int*)(posComp + 0x48));
            void* totalRowText = *(void**)(totalRowPtr + 8);
            fn_TextElement_SetParameter(totalRowText, 0, &rowStr);
        }

        // Calculate totals for progress
        int multiplier = *(int*)(posComp + 0x4C);
        totalRequired = *(int*)((uintptr_t)resourceComp + 0x30) * multiplier + *(int*)(smComp + 0x48) + sumCapacity;
        totalDelivered = *(int*)(posComp + 0x48) + sumDelivered + remaining;
    }

    // Calculate fill ratio and percentage
    float ratio = 0.0f;
    if ((float)totalRequired != 0.0f) {
        ratio = (float)(int)totalDelivered / (float)totalRequired;
    }
    int percent = (int)(ratio * 100.0f + 0.5f);

    // Update progress bar
    *(float*)(*(uintptr_t*)(s + 0x40) + 0x128) = ratio;
    System::String pctStr = System::String::Format("%d", (uint64_t)(uint32_t)percent);
    fn_ProgressElement_SetProgressTextParameter(*(void**)(s + 0x40), 0, &pctStr);
}

// Slot9: On button click (event 0x12, id 6000), toggle workplace enabled state.
void BuildUI_Slot9(BuildUI* self, uint64_t param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 == 0x12 && param4 == 6000) {
        // Get entity at +0x28, check IsA ComponentEntity
        void* entity = (void*)*(uint64_t*)(s + 0x28);
        uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
        void* checkedEntity = nullptr;
        if (fn_IsDerivedFrom(entity, entityTypeId)) {
            checkedEntity = entity;
        }

        // Wrap in InternalBasePtr for FindComponent call
        uint64_t entityBasePtr = 0;
        new (&entityBasePtr) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

        // Find the WorkPlaceComponent on the entity. Retail (BuildUI_Update 0x1400b1ea0)
        // calls FUN_140002e10 — the DAT_1401c80e0 (WorkPlaceComponent) finder. It must NOT
        // be fn_FindComponent_Work: despite the name that helper is the DAT_1401c8070
        // (StorageLocation/WorkComponent) finder, FUN_140003050. Handing the c8070 component
        // to Entity_impl_fc00 below made it read +0x50/+0x58/+0x60 off the wrong object, so
        // the pause path treated non-roster fields as the worker roster (live 2026-07-25:
        // base(+0x58)==8 with count>0, then a non-citizen fed to CitizenComponent::
        // AssignHousing -> FUN_140002e10 -> READ of 0xffffffffffffffff).
        // fn_FindComponent_WorkPlace is the c80e0 finder (likewise misnamed).
        uint64_t workplaceComp = 0;
        fn_FindComponent_WorkPlace((void*)entityBasePtr, &workplaceComp);

        // Read enabled state byte at comp+0x50, toggle it. Both guards matter: the
        // building need not have a WorkPlaceComponent at all, and fn_WorkplaceComponent_
        // Toggle is an EXE address (0x14012fc00) that resolves NULL whenever it is not in
        // the address registry — calling it blind crashed on EXEC of 0 (live 2026-07-25).
        if (workplaceComp == 0 || !fn_WorkplaceComponent_Toggle) {
            std::fprintf(stderr, "[BuildUI] Slot9 pause click ignored: workplaceComp=%p "
                         "toggleFn=%p\n", (void*)workplaceComp,
                         (void*)fn_WorkplaceComponent_Toggle);
            std::fflush(stderr);
            return;
        }
        char currentState = *(char*)(workplaceComp + 0x50);
        fn_WorkplaceComponent_Toggle((void*)workplaceComp, currentState == 0);
    }
}
