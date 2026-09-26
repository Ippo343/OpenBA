// code/Game/Dialog/tradeui.cpp
// Clean reimplementation of TradeUI methods.
//
// TradeUI (0x148 bytes): Trading post panel — shows incoming merchants,
// trade items, buy/sell interface with order management.
// Extends DialogController (0x30 base).
//
// Layout:
//   +0x30..+0x58: sell order array (ptr, ptr, count, capacity) x3 vectors
//   +0x60..+0x78: sell entries (ptr, count, capacity, stride 0x20)
//   +0x70..+0x88: buy entries (ptr, count, capacity, stride 0x20)
//   +0x80..+0x98: storage entries (ptr, count, capacity, stride 0x18)
//   +0x90..+0xA8: order entries (ptr, count, capacity, stride 0x20)
//   +0xA0..+0xB8: purchase entries (ptr, count, capacity, stride 0x08)
//   +0xB0..+0xC8: reserved
//   +0xC0: comboOrder element
//   +0xC8: tableSell element
//   +0xD0: tableBuy element
//   +0xD8: tableStorage element
//   +0xE0: tablePurchase element
//   +0xE8: tableOrder element
//   +0xF0: buttonTrade element
//   +0xF8: buttonDismiss element
//   +0x100: numberSellTotal element
//   +0x108: numberBuyTotal element
//   +0x110: textNoStorage element
//   +0x118: textTooMuch element
//   +0x120: textNotEnough element
//   +0x128: reserved
//   +0x130: reserved
//   +0x138: comboPurchase element
//   +0x140: sortCombo element

#include "Dialog/ui_panels.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>
#include "runtime/types/ui/Element.h"      // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/ElementDescription.h"  // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/String.h"       // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/Sprite.h"       // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K1)


// Dispatch slot addresses for element type IDs used in Serialize inner loops
#define SLOT_BUTTON_ELEM   0x1401514f8  // ButtonElement / HelpButtonElem
#define SLOT_LABEL_ELEM    0x140151668  // LabelElement
#define SLOT_SLIDER_ELEM   0x140151cf0  // SliderElement
#define SLOT_SPINNER_ELEM  0x1401516c8  // SpinnerElement
#define SLOT_TOOLBAR_ELEM  0x1401516a8  // ToolbarElement

// Retail's sell/buy tables call the min/max setter through exe .rdata dispatch slot
// 0x140151cf8 (RefreshSellTable 0x1400841df, RefreshBuyTable 0x140083629,
// RefreshDisplay 0x140084be8 — one shared slot for all three tables).
//
// That slot's name string (slot -> RVA 0x1bb5d0 -> C string) is literally
//   "?SetMinMax@SpinnerElement@UI@@QEAAXHH@Z"
// and Runtime-x64.dll exports it as ordinal 3918. So the slot IS
// fn_SpinnerElement_SetMinMax; the "SliderElement" naming here is the same mislabel
// reg_element_typeids.cpp:36 documents (0x140151cf0 is UI::SpinnerElement, and the
// real SliderElement slot is 0x1401516d0, which nothing in the trade panel touches).
//
// The slot is NOT in the address registry — nothing anywhere registers 0x140151cf8 —
// so ResolveAddr returned 0 and this helper silently did nothing on every call. That
// left every sell/buy spinner at its .rsc min/max, so a typed quantity was clamped
// straight back to 0. Fall back to the export the slot names.
#define SLOT_SLIDER_SETMINMAX 0x140151cf8
static fn_SpinnerElement_SetMinMax_t fn_SliderElement_SetMinMax = nullptr;

static void SliderElement_SetMinMax(void* self, int minVal, int maxVal) {
    static bool s_resolved = false;
    if (!s_resolved) {
        s_resolved = true;
        uintptr_t slot = ResolveAddr(Module::Exe, SLOT_SLIDER_SETMINMAX);
        if (slot) {
            fn_SliderElement_SetMinMax = *(fn_SpinnerElement_SetMinMax_t*)slot;
        }
        if (!fn_SliderElement_SetMinMax)
            fn_SliderElement_SetMinMax = fn_SpinnerElement_SetMinMax;
        // Report the OUTCOME: which pointer we ended up with, or that we have none.
        fprintf(stderr, "[TradeUI] spinner SetMinMax = %p (slot 0x%llx -> %p, export %p)\n",
                (void*)fn_SliderElement_SetMinMax,
                (unsigned long long)SLOT_SLIDER_SETMINMAX, (void*)slot,
                (void*)fn_SpinnerElement_SetMinMax);
        fflush(stderr);
    }
    if (fn_SliderElement_SetMinMax)
        fn_SliderElement_SetMinMax(self, minVal, maxVal);
}

// ============================================================
// Draw (vtable +0x28): Mark tables for layout recalc, set building dirty flag
// ============================================================
// Original: 0x140082a70
//
// Marks tableSell (+0xC8) and tableBuy (+0xD0) as needing size recalculation.
// Then finds the BuildingComponent on the owning entity and sets its
// dirty flag (byte at +0x71) to 1, triggering a visual refresh.

void TradeUI_Draw(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Mark sell and buy tables for layout recalculation
    reinterpret_cast<UI::Element*>(*(void**)(s + 0xC8))->SetNeedsSizeCalc();  // B.3.6 bridge

    reinterpret_cast<UI::Element*>(*(void**)(s + 0xD0))->SetNeedsSizeCalc();  // B.3.6 bridge

    // Get owning entity and find BuildingComponent
    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Set building dirty flag
    *(uint8_t*)(buildingPtr[0] + 0x71) = 1;
}

// ============================================================
// Slot6 (vtable +0x30): Clear building dirty flag
// ============================================================
// Original: 0x140082a00
//
// Finds BuildingComponent on owning entity, clears its dirty flag
// (byte at +0x71 = 0). Called when panel is hidden/deactivated.

void TradeUI_Slot6(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    *(uint8_t*)(buildingPtr[0] + 0x71) = 0;
}

// ============================================================
// Serialize (vtable +0x20): Full UI setup — element lookup, table building
// ============================================================
// Original: 0x140084dd0
//
// Phase 1: Finds 15 named elements and stores as InternalBasePtr in self.
// Phase 2: Sets element IDs on buttons, combos, and sort combo.
// Phase 3: Builds tablePurchase rows with child spinner elements (stride 0x20).
// Phase 4: Counts resource types via ExternalFactory, counts selectable
//          build types, sizes tableStorage/tableSell/tableBuy/tableOrder,
//          clones template elements, finds row sub-elements (spinners etc).
// Phase 5: Calls RefreshDisplay, RefreshSellTable, RefreshBuyTable.

// Helper: find named element in dialog, cast via IsDerivedFrom, return raw ptr
static void* TradeUI_FindTypedElement(void* dialogElem, const char* name, uint32_t* typeId) {
    System::String strBuf(name);
    uint64_t foundBuf[2] = {0, 0};
    ::new (static_cast<void*>(foundBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)dialogElem)->FindElement(strBuf));  // B.3.6 bridge
    void* elem = *(void**)foundBuf;
    void* result = nullptr;
    if (elem && typeId) {
        bool derived = fn_IsDerivedFrom(elem, *typeId);
        if (derived) result = elem;
    }
    uint64_t ibpBuf[1] = {0};

    return result;
}

// B.3.2 bridge: every TradeUI_DynArrayPush(s, ptrOff, countOff, capOff, entry, stride)
// call site below was a hand-transcribed copy of retail's inlined
// Collection::Array<T>::SetSize(count+1, fill) (see Component/coll_bridge.h). Converted
// in place to GameColl::ArrayAt<GameColl::Bytes<stride, 8>>(s, ptrOff).SetSize(...) — the runtime stride parameter
// this helper took becomes the compile-time N of the blob element, so the helper is
// deleted (zero remaining callers, confirmed by grep across code/, standalone/,
// engine/, runtime/).

// Helper: clone template into all table cells
// templateObj must be read from first child's +0xB8 BEFORE calling SetSize,
// because SetSize destroys/replaces children.
static void TradeUI_CloneTableCells(uintptr_t s, void* tableElem, void* templateObj) {
    int totalCells = reinterpret_cast<const UI::Element*>(tableElem)->ChildCount();  // B.3.6 bridge
    for (int i = 0; i < totalCells; i++) {
        // Get dialog resource reference
        void* dialogInst = *(void**)(s + 0x18);
        void* dialogResource = *(void**)((uintptr_t)dialogInst + 0x18);

        // Clone via the template's slot 8, UI::ElementDescription::CreateInstance
        // (retslot, B6-2). templateObj is Element::_description (+0xB8) of the
        // table's Child(0) — every caller passes TradeUI_GetTableTemplate's result.
        uint64_t cloneBuf[2] = {0, 0};
        reinterpret_cast<const UI::ElementDescription*>(templateObj)->CreateInstance(
            reinterpret_cast<::Object::InternalPtr<UI::Element>*>(cloneBuf),
            reinterpret_cast<const UI::Dialog*>(dialogResource));  // B.3.6 bridge

        fn_TableElement_SetChild2(tableElem, cloneBuf, i);
    }
}

// Helper: read template from table's first child +0xB8 (must be called BEFORE SetSize)
static void* TradeUI_GetTableTemplate(void* ibpPtr) {
    uint64_t childBuf[2] = {0, 0};
    ::new (static_cast<void*>(childBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)ibpPtr)->Child(0));  // B.3.6 bridge
    void* firstChild = *(void**)childBuf;
    if (!firstChild) return nullptr;
    return *(void**)((uintptr_t)firstChild + 0xB8);
}

// ============================================================
// Helper: find a description sub-component by typeId
// ============================================================
static void* FindDescComponent(void* descEntity, uint32_t typeId)
{
    if (!descEntity) return nullptr;
    int count = *(int*)((uintptr_t)descEntity + 0x30);
    void** arr = *(void***)((uintptr_t)descEntity + 0x28);
    for (int i = 0; i < count; i++) {
        void* comp = arr[i];
        void** vtbl = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int tid = ((fn_GetTypeId_t)vtbl[2])(comp);
        if ((uint32_t)tid == typeId) {
            if (fn_IsDerivedFrom(comp, typeId))
                return comp;
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Helper: set spinner increment based on max value
// ============================================================
static void SetSpinnerIncrement(void* spinner)
{
    int maxVal = fn_SpinnerElement_GetMaxValue(spinner);
    if (maxVal < 0x15) {
        fn_SpinnerElement_SetIncrement(spinner, 1);
    } else if (maxVal < 0x33) {
        fn_SpinnerElement_SetIncrement(spinner, 5);
    } else if (maxVal < 0x65) {
        fn_SpinnerElement_SetIncrement(spinner, 0x14);
    }
    // >= 0x65: no increment set (uses default)
}

// ============================================================
// Helper: set element name from External resource path
// ============================================================
static void SetElementNameFromExternal(void* element, void* externalPtr)
{
    // `GetPath` returns by value through a hidden return slot and the callee
    // CONSTRUCTS into it; copy-initialising the local is the same store.
    System::Filename pathBuf =
        reinterpret_cast<const ::Object::External*>(externalPtr)->GetPath();  // B.3.3 bridge
    System::Filename nameStr = pathBuf.GetPathFilename(false);
    reinterpret_cast<UI::Element*>(element)->SetName(*reinterpret_cast<const System::String*>(&nameStr));  // B.3.6 bridge
}

// ============================================================
// Helper: virtual call for Element::SetEnabled(this, bool)
// vtable[8] (offset 0x40)
// ============================================================
// UI::Element slot 8 is Enable(bool). Every caller passes an element this file
// stored from a Child()/FindElement() result (the +0x60/+0x70/+0x80/+0x90/+0xA0
// row arrays and the +0xC0 combo, all filled in TradeUI_Serialize).
static inline void VCall_Element_SetEnabled(void* elem, bool enabled)
{
    reinterpret_cast<UI::Element*>(elem)->Enable(enabled);  // B.3.6 bridge
}

// ============================================================
// Helper: virtual call for Element vtable[10] (offset 0x50)
// Used as a visibility/show function in some contexts:
//   arg=0 hides, arg=-1 (0xFFFFFFFF) shows.
// ============================================================
// UI::Element slot 10 is SetColor(uint32); callers pass the +0x60 sell rows.
static inline void VCall_Element_Slot10(void* elem, int arg)
{
    reinterpret_cast<UI::Element*>(elem)->SetColor(static_cast<uint32_t>(arg));  // B.3.6 bridge
}

// ============================================================
// RefreshSellTable (0x140083c70)
// ============================================================
// Rebuilds the sell table data and UI.
//
// Phase 1: Get entity components (Building, StateMachine).
// Phase 2: Iterate resource entries from the game world's resource table.
//          For each resource type, check building's storage BST for availability.
//          If available (count > 0), find ValueDescription + ResourceType sub-components
//          and push a 0x20-byte sell data entry.
// Phase 3: Build state check, then populate sell table UI cells.
//          For each sell entry: set sprite, text, counts, spinner limits, color.
// Phase 4: Clear unused sell table cells.

void TradeUI_RefreshSellTable(TradeUI* self)
{
    uintptr_t s = (uintptr_t)self;

    // Get entity, BuildingComponent, StateMachineComponent
    void* entityRaw = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entityRaw : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Check TradeDescription on building
    fn_IsDerivedFrom(*(void**)(buildingPtr[0] + 0x18), *g_TypeInfo_TradeDescription);

    uint64_t smPtr[2] = {0, 0};
    fn_FindComponent_Storage((void*)entityPtr[0], &smPtr);

    // Get resource table from game world singleton (+0xf0 = resource manager)
    void* resourceMgr = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf0);
    int resourceCount = *(int*)((uintptr_t)resourceMgr + 0x118);

    // Clear sell data array at +0x30 (stride 0x20)
    fn_DynArray_Clear_0x20((void*)(s + 0x30));

    // Phase 2: Iterate resources and build sell entries
    if (resourceCount > 0) {
        for (int ri = 0; ri < resourceCount; ri++) {
            void* valueDesc = nullptr;
            void* resTypeComp = nullptr;

            // Get resource entity from resource table
            uint64_t resEntity = *(uint64_t*)(*(uintptr_t*)((uintptr_t)resourceMgr + 0x110)
                                  + (uintptr_t)ri * 8);

            // Search building's storage BST for this resource
            uintptr_t* bstNode = *(uintptr_t**)(buildingPtr[0] + 0x60);
            bool found = false;
            while (bstNode != nullptr) {
                if (resEntity < *bstNode) {
                    // left at +0x18. Confirmed byte-exact: the node is
                    // { key +0x00, value +0x08, parent +0x10, left +0x18,
                    //   right +0x20, balance +0x28 } -- see the layout block in
                    // tradecomponent_serialize.cpp (disassembled from this field's
                    // own insert/rotate bodies) and retail's descent in
                    // TutorialGoalCount::Check case 7 (0x1400df290).
                    bstNode = (uintptr_t*)bstNode[3];  // left
                } else if (resEntity <= *bstNode) {
                    // Found — check if count (at node+0x08) > 0
                    if (bstNode != nullptr && (int)bstNode[1] != 0) {
                        // Find ValueDescription and ResourceType in resource entity
                        int descCount = *(int*)(resEntity + 0x30);
                        void** descArray = *(void***)(resEntity + 0x28);
                        for (int d = 0; d < descCount; d++) {
                            void* comp = descArray[d];
                            void** cvtbl = *(void***)comp;
                            typedef int (*fn_GetTypeId_t)(void*);
                            int tid = ((fn_GetTypeId_t)cvtbl[2])(comp);
                            if ((uint32_t)tid == *g_TypeId_RawMaterialDescription) {
                                if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription))
                                    resTypeComp = comp;
                                break;
                            }
                        }
                        // Find ValueDescription
                        for (int d = 0; d < descCount; d++) {
                            void* comp = descArray[d];
                            void** cvtbl = *(void***)comp;
                            typedef int (*fn_GetTypeId_t)(void*);
                            int tid = ((fn_GetTypeId_t)cvtbl[2])(comp);
                            if ((uint32_t)tid == *g_TypeInfo_ValueDescription) {
                                if (fn_IsDerivedFrom(comp, *g_TypeInfo_ValueDescription))
                                    valueDesc = comp;
                                break;
                            }
                        }
                        found = true;
                    }
                    break;
                } else {
                    bstNode = (uintptr_t*)bstNode[4];  // right
                }
            }

            if (!found) continue;

            // Build 0x20-byte sell data entry and push to array at +0x30.
            // B.3.2 bridge: TradeUI+0x30 is Collection::Array<{resEntity, valueDesc,
            // resTypeComp, pad}> (stride 0x20, count +0x38, capacity +0x3C).
            auto& sellRows = GameColl::ArrayAt<GameColl::Bytes<0x20, 8>>(s, 0x30);
            sellRows.SetSize(sellRows.GetSize() + 1,
                              GameColl::Bytes<0x20, 8>(resEntity, (uint64_t)valueDesc,
                                             (uint64_t)resTypeComp, 0));
        }
    }

    // Phase 3: Get build state, populate sell table UI
    int sellCount = *(int*)(s + 0x38);
    uint64_t sellCountU = (uint64_t)sellCount;

    // Find BuildState component on entity
    void* buildStateComp = nullptr;
    {
        int compCount = *(int*)(entityPtr[0] + 0x38);
        void** compArr = *(void***)(entityPtr[0] + 0x30);
        for (int i = 0; i < compCount; i++) {
            void* comp = compArr[i];
            void** cvtbl = *(void***)comp;
            typedef int (*fn_GetTypeId_t)(void*);
            int tid = ((fn_GetTypeId_t)cvtbl[2])(comp);
            if ((uint32_t)tid == *g_TypeId_StateMachineComponent_FC) {
                if (fn_IsDerivedFrom(comp, *g_TypeId_StateMachineComponent_FC))
                    buildStateComp = comp;
                break;
            }
        }
    }

    // Check trade state: buildState byte +0x52 (or +0x50 if negative)
    bool tradeActive = false;
    if (buildStateComp) {
        new (&buildStateComp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(buildStateComp));   // B.3.3 bridge
        int8_t state52 = *(int8_t*)((uintptr_t)buildStateComp + 0x52);
        int8_t stateVal = state52;
        if (state52 < 0) stateVal = *(int8_t*)((uintptr_t)buildStateComp + 0x50);
        tradeActive = (stateVal == 0x13);
    }

    // Populate sell table cells
    if (sellCount > 0) {
        uintptr_t dataOffset = 0;
        for (int i = 0; i < sellCount; i++) {
            uintptr_t* sellData = (uintptr_t*)(*(uintptr_t*)(s + 0x30) + dataOffset);
            uintptr_t* sellUI = (uintptr_t*)(*(uintptr_t*)(s + 0x60) + dataOffset);

            // Get sellable count from building
            int sellableCount = fn_Building_GetSellableCount(
                (void*)buildingPtr[0], (void*)sellData[0]);

            bool hasStock = (sellableCount > 0);
            uint32_t color = hasStock ? 0xFFFFFFFF : 0xFF808080;

            // Set sprite from ValueDescription (+0x20=spriteSheet, +0x28=spriteName)
            void* valueDesc = (void*)sellData[1];
            uint8_t spriteBuf[16] = {0};
            ::new (static_cast<void*>(spriteBuf)) UI::Sprite(::Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)valueDesc + 0x20))), *reinterpret_cast<const System::String*>((uintptr_t)valueDesc + 0x28));  // B.3.6 bridge
            fn_LabelElement_SetSprite(*(void**)sellUI, spriteBuf);

            // Set text from ValueDescription (+0x30=stringTable, +0x38=stringName)
            uint8_t uiStrBuf[16] = {0};
            ::new (static_cast<void*>(uiStrBuf)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)valueDesc + 0x30))), *reinterpret_cast<const System::String*>((uintptr_t)valueDesc + 0x38));  // B.3.6 bridge
            fn_TextElement_SetText(*(void**)sellUI, uiStrBuf);

            // Set sellable count on number element (sellUI+0x10)
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)sellUI + 0x10), sellableCount);

            // Search StateMachine's order list for matching resource to get order qty
            int orderQty = 0;
            int smOrderCount = *(int*)(smPtr[0] + 0x30);
            if (smOrderCount > 0) {
                uintptr_t* orderArr = *(uintptr_t**)(smPtr[0] + 0x28);
                for (int j = 0; j < smOrderCount; j++) {
                    if (orderArr[j * 2] == sellData[0]) {
                        if (j >= 0) {
                            orderQty = (int)orderArr[j * 2 + 1]
                                       - *(int*)((uintptr_t)&orderArr[j * 2 + 1] + 4);
                        }
                        break;
                    }
                }
            }

            // Set order qty on number element (sellUI+0x08)
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)sellUI + 0x08), orderQty);

            // Set spinner enabled state
            bool spinnerEnabled = tradeActive && hasStock;
            void* spinner = *(void**)((uintptr_t)sellUI + 0x18);
            VCall_Element_SetEnabled(spinner, spinnerEnabled);

            // Set number and label elements enabled
            VCall_Element_SetEnabled(*(void**)((uintptr_t)sellUI + 0x08), true);
            VCall_Element_SetEnabled(*(void**)((uintptr_t)sellUI + 0x10), true);

            // Set spinner min/max
            int spinnerMax = 0;
            if (tradeActive && hasStock) {
                spinnerMax = sellableCount;
            }
            // sellUI+0x18 is a SliderElement (NOT SpinnerElement) — use NumberEditElement::SetMinMax
            SliderElement_SetMinMax(*(void**)((uintptr_t)sellUI + 0x18), 0, spinnerMax);

            // Set spinner increment based on max value
            void* spinnerElem = *(void**)((uintptr_t)sellUI + 0x18);
            SetSpinnerIncrement(spinnerElem);

            // Reset spinner value to 0
            fn_SpinnerElement_SetValue(*(void**)((uintptr_t)sellUI + 0x18), 0);

            // Set element name from resource path
            SetElementNameFromExternal(*(void**)((uintptr_t)sellUI + 0x18),
                                       (void*)sellData[0]);

            // Set color on label, number, and count elements
            VCall_Element_Slot10(*(void**)sellUI, color);
            VCall_Element_Slot10(*(void**)((uintptr_t)sellUI + 0x08), color);
            VCall_Element_Slot10(*(void**)((uintptr_t)sellUI + 0x10), color);

            dataOffset += 0x20;
        }
    }

    // Phase 4: Clear unused sell table cells
    int totalSellCells = 0;
    totalSellCells = *(int*)(*(uintptr_t*)(s + 0xC8) + 0xC0);
    if ((int)sellCountU < totalSellCells) {
        uintptr_t clearOffset = sellCountU << 5;
        for (int i = (int)sellCountU; i < totalSellCells; i++) {
            uintptr_t* cellUI = (uintptr_t*)(*(uintptr_t*)(s + 0x60) + clearOffset);

            // Set empty sprite
            uint8_t emptySprite[16] = {0};
            ::new (static_cast<void*>(emptySprite)) UI::Sprite();  // B.3.6 bridge
            fn_LabelElement_SetSprite(*(void**)cellUI, emptySprite);

            // Set empty text
            uint8_t emptyStr[16] = {0};
            ::new (static_cast<void*>(emptyStr)) UI::String();  // B.3.6 bridge
            fn_TextElement_SetText(*(void**)cellUI, emptyStr);

            // Disable spinner, number, and label elements (null-guard each)
            if (*(void**)((uintptr_t)cellUI + 0x18))
                VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x18), false);
            if (*(void**)((uintptr_t)cellUI + 0x08))
                VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x08), false);
            if (*(void**)((uintptr_t)cellUI + 0x10))
                VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x10), false);

            // cellUI+0x18 is a SliderElement — use NumberEditElement::SetMinMax
            if (*(void**)((uintptr_t)cellUI + 0x18))
                SliderElement_SetMinMax(*(void**)((uintptr_t)cellUI + 0x18), 0, 0);

            clearOffset += 0x20;
        }
    }
}

// ============================================================
// RefreshBuyTable (0x140083100)
// ============================================================
// Rebuilds the buy table data and UI.
//
// Phase 1: Get entity components (Building + TradeDescription).
// Phase 2: Set up order header text ("textName"/"textOrder") with
//          trade order name and current merchant info.
// Phase 3: Build buy entries array from trade orders (building+0x50/+0x58).
// Phase 4: Populate buy table UI cells with sprite/text/numbers.
// Phase 5: Clear unused buy table cells.
// Phase 6: Build purchase reserved entries (building+0xb0/+0xb8).
// Phase 7: Set combo and enable state for order combo.
// Phase 8: Populate purchase button array with checked states.

void TradeUI_RefreshBuyTable(TradeUI* self)
{
    uintptr_t s = (uintptr_t)self;

    // Get entity, BuildingComponent, TradeDescription
    void* entityRaw = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entityRaw : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    void* tradeDescRaw = *(void**)(buildingPtr[0] + 0x18);
    bool isTrade = fn_IsDerivedFrom(tradeDescRaw, *g_TypeInfo_TradeDescription);
    void* tradeDesc = isTrade ? tradeDescRaw : nullptr;

    // Clear buy data array at +0x40 (stride 0x18)
    fn_DynArray_Clear_0x18_Buy((void*)(s + 0x40));

    // Phase 2: Set order header text elements
    for (int ti = 0; ti < 2; ti++) {
        const char* elemName = (ti == 0) ? "textName" : "textOrder";
        System::String nameStr(elemName);
        uint8_t foundBuf[16] = {0};
        ::new (static_cast<void*>(foundBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0x20))->FindElement(nameStr));  // B.3.6 bridge

        // IsDerivedFrom TextElement
        void* textElem = nullptr;
        if (*(void**)foundBuf) {
            bool isText = fn_IsDerivedFrom(*(void**)foundBuf, *g_TypeId_TextElement);
            if (isText) textElem = *(void**)foundBuf;
        }
        uint64_t textIbp[1] = {0};
        new (textIbp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(textElem));   // B.3.3 bridge

        // Set text parameters based on current order
        int orderIdx = *(int*)(buildingPtr[0] + 0x2c);
        if (orderIdx < 0) {
            // No active order — text stays default
        } else {
            // Get order entry from trade description
            uintptr_t orderEntry = *(uintptr_t*)((uintptr_t)tradeDesc + 0x48)
                                   + (uintptr_t)orderIdx * 0x48;

            // Set parameter 0: order name string
            System::String orderNameBuf;
            fn_Building_GetOrderName((void*)buildingPtr[0], &orderNameBuf);
            fn_TextElement_SetParameter((void*)*textIbp, 0, &orderNameBuf);

            // Set parameter 1: UI string from order entry
            uint8_t uiStrBuf[16] = {0};
            ::new (static_cast<void*>(uiStrBuf)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)orderEntry)), *reinterpret_cast<const System::String*>(orderEntry + 8));  // B.3.6 bridge
            // Convert to System::String
            System::String sysStrBuf;
            ::new (static_cast<void*>(&sysStrBuf)) System::String(reinterpret_cast<const UI::String*>(uiStrBuf)->operator System::String());  // B.3.6 bridge
            fn_TextElement_SetParameter((void*)*textIbp, 1, &sysStrBuf);
        }

        // Show/hide text element based on order validity
        // vtable+0x50 is a visibility function: EDX=0 hides, EDX=-1 shows
        // Original asm: orderIdx<0 → XOR EDX,EDX; orderIdx>=0 → OR EDX,0xffffffff
        // Slot 10 of UI::Element is SetColor(uint32); the receiver is the
        // FindElement("textName"/"textOrder") result above.
        {
            void* elem = (void*)*textIbp;
            reinterpret_cast<UI::Element*>(elem)->SetColor(
                static_cast<uint32_t>((orderIdx < 0) ? 0 : -1));  // B.3.6 bridge
        }
    }

    // Phase 3: Build buy entries from trade orders
    int tradeOrderCount = *(int*)(buildingPtr[0] + 0x58);
    if (tradeOrderCount > 0) {
        for (int oi = 0; oi < tradeOrderCount; oi++) {
            // Get trade order entry
            uintptr_t orderList = *(uintptr_t*)(buildingPtr[0] + 0x50);
            uintptr_t orderEntry = *(uintptr_t*)(orderList + (uintptr_t)oi * 0x10);

            // Find ValueDescription in order entry's description
            void* valDesc = FindDescComponent((void*)orderEntry, *g_TypeInfo_ValueDescription);

            // Build 0x18-byte buy data entry and push to array at +0x40.
            // B.3.2 bridge: TradeUI+0x40 is Collection::Array<{orderEntry, valDesc,
            // pad}> (stride 0x18, count +0x48, capacity +0x4C).
            auto& buyRows = GameColl::ArrayAt<GameColl::Bytes<0x18, 8>>(s, 0x40);
            buyRows.SetSize(buyRows.GetSize() + 1,
                             GameColl::Bytes<0x18, 8>(orderEntry, (uint64_t)valDesc, 0));
        }
    }

    // Phase 4: Populate buy table UI cells
    int buyCount = *(int*)(buildingPtr[0] + 0x58);
    uint64_t buyCountU = (uint64_t)buyCount;

    if (buyCount > 0) {
        uintptr_t dataOffset = 0;
        uintptr_t buyDataOffset = 0;
        for (int i = 0; i < buyCount; i++) {
            uintptr_t* buyUI = (uintptr_t*)(*(uintptr_t*)(s + 0x70) + dataOffset);

            uintptr_t* buyOrder = (uintptr_t*)(*(uintptr_t*)(buildingPtr[0] + 0x50) + buyDataOffset);
            uintptr_t orderDesc = *buyOrder;

            // Find ValueDescription
            void* valDesc = FindDescComponent((void*)orderDesc, *g_TypeInfo_ValueDescription);

            // Set sprite from ValueDescription
            uint8_t spriteBuf[16] = {0};
            ::new (static_cast<void*>(spriteBuf)) UI::Sprite(::Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)valDesc + 0x20))), *reinterpret_cast<const System::String*>((uintptr_t)valDesc + 0x28));  // B.3.6 bridge
            fn_LabelElement_SetSprite(*(void**)buyUI, spriteBuf);

            // Set text from ValueDescription
            uint8_t uiStrBuf[16] = {0};
            ::new (static_cast<void*>(uiStrBuf)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)valDesc + 0x30))), *reinterpret_cast<const System::String*>((uintptr_t)valDesc + 0x38));  // B.3.6 bridge
            fn_TextElement_SetText(*(void**)buyUI, uiStrBuf);

            // Set price number (buyUI+0x18 = price element)
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)buyUI + 0x18),
                                        *(int*)((uintptr_t)buyOrder + 0x0C));

            // Set quantity number (buyUI+0x08)
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)buyUI + 0x08),
                                        (int)buyOrder[1]);

            // Set slider min/max (buyUI+0x10 is a SliderElement)
            SliderElement_SetMinMax(*(void**)((uintptr_t)buyUI + 0x10),
                                        0, *(int*)((uintptr_t)buyOrder + 0x0C));

            // Set spinner increment
            void* spinnerElem = *(void**)((uintptr_t)buyUI + 0x10);
            SetSpinnerIncrement(spinnerElem);

            // Set element name from resource path
            SetElementNameFromExternal(*(void**)((uintptr_t)buyUI + 0x10),
                                       (void*)*buyOrder);

            // Enable all sub-elements
            VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0x10), true);
            VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0x18), true);
            VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0x08), true);

            dataOffset += 0x20;
            buyDataOffset += 0x10;
        }
    }

    // Phase 5: Clear unused buy table cells
    int totalBuyCells = *(int*)(*(uintptr_t*)(s + 0xD0) + 0xC0);
    if ((int)buyCountU < totalBuyCells) {
        uintptr_t clearOffset = buyCountU << 5;
        for (int i = (int)buyCountU; i < totalBuyCells; i++) {
            uintptr_t* cellUI = (uintptr_t*)(*(uintptr_t*)(s + 0x70) + clearOffset);

            // Set empty sprite
            uint8_t emptySprite[16] = {0};
            ::new (static_cast<void*>(emptySprite)) UI::Sprite();  // B.3.6 bridge
            fn_LabelElement_SetSprite(*(void**)cellUI, emptySprite);

            // Set empty text
            uint8_t emptyStr[16] = {0};
            ::new (static_cast<void*>(emptyStr)) UI::String();  // B.3.6 bridge
            fn_TextElement_SetText(*(void**)cellUI, emptyStr);

            // cellUI+0x10 is a SliderElement — use NumberEditElement::SetMinMax
            if (*(void**)((uintptr_t)cellUI + 0x10))
                SliderElement_SetMinMax(*(void**)((uintptr_t)cellUI + 0x10), 0, 0);

            if (*(void**)((uintptr_t)cellUI + 0x10))
                VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x10), false);
            if (*(void**)((uintptr_t)cellUI + 0x18))
                VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x18), false);
            if (*(void**)((uintptr_t)cellUI + 0x08))
                VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x08), false);

            clearOffset += 0x20;
        }
    }

    // Phase 6: Build purchase reserved entries (building+0xb0/+0xb8)
    int orderIdx = *(int*)(buildingPtr[0] + 0x2c);
    if (orderIdx < 0) {
        // No active order — clear purchase list
        fn_MemFreeStatic(*(void**)(s + 0xb0));
        *(uint64_t*)(s + 0xb0) = 0;
        *(uint64_t*)(s + 0xb8) = 0;
    } else {
        // Populate purchase reserved entries via helper
        fn_Building_GetBuyEntries((void*)buildingPtr[0], (void*)(s + 0xb0));
    }

    // Phase 7: Set order combo current item and enabled state
    int comboIdx = 0;
    if (*(int*)(buildingPtr[0] + 0x2c) >= 0) {
        comboIdx = *(int*)(*(uintptr_t*)(buildingPtr[0] + 0x30)
                    + (uintptr_t)*(int*)(buildingPtr[0] + 0x2c) * 0x20 + 8);
    }
    fn_ComboElement_SetCurrentItem(*(void**)(s + 0xC0), comboIdx);

    void* comboElem = *(void**)(s + 0xC0);
    bool comboEnabled = (*(int*)(buildingPtr[0] + 0x2c) >= 0);
    VCall_Element_SetEnabled(comboElem, comboEnabled);

    // Phase 8: Populate purchase button array with checked states
    int purchaseCount = *(int*)(s + 0xb8);
    if (purchaseCount > 0) {
        uintptr_t offset = 0;
        for (int pi = 0; pi < purchaseCount; pi++) {
            uintptr_t purchaseEntity = *(uintptr_t*)(offset + *(uintptr_t*)(s + 0xb0));

            // Find ValueDescription for this purchase entity
            void* valDesc = FindDescComponent((void*)purchaseEntity, *g_TypeInfo_ValueDescription);

            // Set sprite and text on purchase button element
            void* purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            int childCount = reinterpret_cast<const UI::Element*>(*(void**)purchaseUI)->ChildCount();  // B.3.6 bridge

            for (int ci = 0; ci < childCount; ci++) {
                void* purchaseUIPtr = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                uint64_t childBuf[2] = {0, 0};
                ::new (static_cast<void*>(childBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)purchaseUIPtr)->Child(ci));  // B.3.6 bridge

                // IsDerivedFrom TextElement check
                void* childElem = nullptr;
                if (*(void**)childBuf) {
                    bool isText = fn_IsDerivedFrom(*(void**)childBuf, *g_TypeId_TextElement);
                    if (isText) childElem = *(void**)childBuf;
                }
                uint64_t childIbp[2] = {0, 0};
                new (childIbp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(childElem));   // B.3.3 bridge

                if ((void*)*childIbp != nullptr) {
                    // Set sprite
                    uint8_t spriteBuf[16] = {0};
                    ::new (static_cast<void*>(spriteBuf)) UI::Sprite(::Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)valDesc + 0x20))), *reinterpret_cast<const System::String*>((uintptr_t)valDesc + 0x28));  // B.3.6 bridge
                    fn_LabelElement_SetSprite((void*)*childIbp, spriteBuf);

                    // Set text
                    uint8_t uiStrBuf[16] = {0};
                    ::new (static_cast<void*>(uiStrBuf)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)valDesc + 0x30))), *reinterpret_cast<const System::String*>((uintptr_t)valDesc + 0x38));  // B.3.6 bridge
                    fn_TextElement_SetText((void*)*childIbp, uiStrBuf);
                }

                purchaseUIPtr = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                childCount = reinterpret_cast<const UI::Element*>(*(void**)purchaseUIPtr)->ChildCount();  // B.3.6 bridge
            }

            // Enable purchase button
            void* purchaseBtn = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            VCall_Element_SetEnabled(*(void**)purchaseBtn, true);

            // Check if this purchase entity is in the current order's selection list
            uintptr_t orderData = (uintptr_t)*(int*)(buildingPtr[0] + 0x2c) * 0x20
                                  + *(uintptr_t*)(buildingPtr[0] + 0x30);
            int selCount = *(int*)(orderData + 0x18);
            if (selCount > 0) {
                uintptr_t* selList = *(uintptr_t**)(orderData + 0x10);
                for (int si = 0; si < selCount; si++) {
                    if (selList[si] == *(uintptr_t*)(*(uintptr_t*)(s + 0xb0) + offset)) {
                        void* purchaseBtnPtr = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                        *(uint8_t*)(*(uintptr_t*)purchaseBtnPtr + 0xd8) = 1;
                        break;
                    }
                }
            }

            offset += 8;
        }
    }

    // Clear unused purchase buttons
    int totalPurchaseSlots = *(int*)(s + 0xa8);
    if (purchaseCount < totalPurchaseSlots) {
        uintptr_t offset = (uintptr_t)purchaseCount * 8;
        for (int i = purchaseCount; i < totalPurchaseSlots; i++) {
            // Clear children
            int ci = 0;
            void* purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            int childCount = reinterpret_cast<const UI::Element*>(*(void**)purchaseUI)->ChildCount();  // B.3.6 bridge
            while (ci < childCount) {
                purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                uint64_t childBuf[2] = {0, 0};
                ::new (static_cast<void*>(childBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)purchaseUI)->Child(ci));  // B.3.6 bridge

                void* childElem = nullptr;
                if (*(void**)childBuf) {
                    bool isText = fn_IsDerivedFrom(*(void**)childBuf, *g_TypeId_TextElement);
                    if (isText) childElem = *(void**)childBuf;
                }
                uint64_t childIbp[2] = {0, 0};
                new (childIbp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(childElem));   // B.3.3 bridge

                if ((void*)*childIbp != nullptr) {
                    uint8_t emptySprite[16] = {0};
                    ::new (static_cast<void*>(emptySprite)) UI::Sprite();  // B.3.6 bridge
                    fn_LabelElement_SetSprite((void*)*childIbp, emptySprite);

                    uint8_t emptyStr[16] = {0};
                    ::new (static_cast<void*>(emptyStr)) UI::String();  // B.3.6 bridge
                    fn_TextElement_SetText((void*)*childIbp, emptyStr);
                }

                ci++;
                purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                childCount = reinterpret_cast<const UI::Element*>(*(void**)purchaseUI)->ChildCount();  // B.3.6 bridge
            }

            // Disable button and clear checked state
            purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            void* btn = *(void**)purchaseUI;
            VCall_Element_SetEnabled(btn, false);

            purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            *(uint8_t*)(*(uintptr_t*)purchaseUI + 0xd8) = 0;

            offset += 8;
        }
    }
}

// Positive readback for the SpinnerElement::SetElementId tagging below. The override
// (Runtime 0x1800d7e10) writes the id into TWO places: the spinner's own Element+0x88
// and the inner edit element reached through **(spinner+0xa0). That inner write is the
// half the Element base setter never performs, so a matching inner id is a value only
// the faithful call can produce -- the difference between "we tagged something" and
// "we tagged what retail tags". One line per family (row 0 only) under
// BANISHED_TRADEUI_EVENTDIAG; every branch prints, including a MISMATCH.
static void TradeUI_ReportSpinnerTag(const char* family, int idx, void* spinner, int id) {
    if (!spinner) {
        fprintf(stderr, "[TradeSpinnerTag] %s row0 id=%d spinner=NULL -- nothing tagged\n",
                family, id);
        fflush(stderr);
        return;
    }
    uintptr_t sp = (uintptr_t)spinner;
    int own = *(int*)(sp + 0x88);
    void** innerPtr = *(void***)(sp + 0xa0);
    void* inner = innerPtr ? *innerPtr : nullptr;
    int innerId = inner ? *(int*)((uintptr_t)inner + 0x88) : -1;
    fprintf(stderr,
            "[TradeSpinnerTag] %s row0 id=%d spinner=%p own(+0x88)=%d inner=%p "
            "inner(+0x88)=%d %s\n",
            family, id, spinner, own, inner, innerId,
            (own == id && inner && innerId == id) ? "OK" : "MISMATCH");
    fflush(stderr);
}

void TradeUI_Serialize(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Get entity and find BuildingComponent ---
    void* entityRaw = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entityRaw : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // --- Phase 1: Find all 15 named elements ---
    void* dialogElemPtr = (void*)(s + 0x20);

    // Tables (IsDerivedFrom TableElement)
    *(uintptr_t*)(s + 0xC8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableSell", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xD0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableBuy", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xD8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableStorage", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xE0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tablePurchase", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xE8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableOrder", g_TypeId_TableElement);

    // Number elements (IsDerivedFrom SpinnerElement/NumberElement)
    *(uintptr_t*)(s + 0x100) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "numberSellTotal", g_TypeId_SpinnerElement);
    *(uintptr_t*)(s + 0x108) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "numberBuyTotal", g_TypeId_SpinnerElement);

    // Buttons (IsDerivedFrom ButtonElement)
    *(uintptr_t*)(s + 0xF0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "buttonTrade", g_TypeId_HelpButtonElem);
    reinterpret_cast<UI::Element*>(*(void**)(s + 0xF0))->SetElementId(0x4e20);  // B.3.6 bridge

    *(uintptr_t*)(s + 0xF8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "buttonDismiss", g_TypeId_HelpButtonElem);
    reinterpret_cast<UI::Element*>(*(void**)(s + 0xF8))->SetElementId(0x4e21);  // B.3.6 bridge

    // Text elements (IsDerivedFrom TextElement)
    *(uintptr_t*)(s + 0x110) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "textNoStorage", g_TypeId_TextElement);
    *(uintptr_t*)(s + 0x118) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "textTooMuch", g_TypeId_TextElement);
    *(uintptr_t*)(s + 0x120) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "textNotEnough", g_TypeId_TextElement);

    // Combo elements (IsDerivedFrom ComboElement)
    *(uintptr_t*)(s + 0x138) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "comboPurchase", g_TypeId_HelpComboElem);
    // Set comboPurchase current item from building component field
    int purchaseIdx = *(int*)(buildingPtr[0] + 0x28);
    fn_ComboElement_SetCurrentItem(*(void**)(s + 0x138), purchaseIdx);
    reinterpret_cast<UI::Element*>(*(void**)(s + 0x138))->SetElementId(0x4e22);  // B.3.6 bridge

    *(uintptr_t*)(s + 0xC0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "comboOrder", g_TypeId_HelpComboElem);
    reinterpret_cast<UI::Element*>(*(void**)(s + 0xC0))->SetElementId(0x4e23);  // B.3.6 bridge

    *(uintptr_t*)(s + 0x140) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "sortCombo", g_TypeId_HelpComboElem);
    reinterpret_cast<UI::Element*>(*(void**)(s + 0x140))->SetElementId(199);  // B.3.6 bridge — 0xC7
    fn_ComboElement_SetCurrentItem(*(void**)(s + 0x140), 0);

    // --- Phase 2: Build tablePurchase row array (stride 0x20, 4 elements per row) ---
    // tablePurchase uses FLAT indexing with stride 3 per logical row:
    //   child[k]   = compound cell with sub-child(0)=label, sub-child(1)=number
    //   child[k+1] = button element (standalone)
    //   child[k+2] = spinner element (standalone)
    // +0xC0 = row count. Element IDs: label=baseId-1000, number=baseId, spinner=baseId+1000
    void* tablePurchase = *(void**)(s + 0xE0);
    int purchaseRowCount = *(int*)((uintptr_t)tablePurchase + 0xC0);

    int baseId = 6000;
    int flatIdx = 0;
    for (int row = 0; row < purchaseRowCount; row++) {
        uint64_t buf[2] = {0, 0};
        uint64_t sub[2] = {0, 0};

        // child[flatIdx] = compound cell -> sub-child(0) = label
        ::new (static_cast<void*>(buf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xE0))->Child(flatIdx));  // B.3.6 bridge
        ::new (static_cast<void*>(sub)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)buf)->Child(0));  // B.3.6 bridge
        void* label = *(void**)sub;

        // same compound cell -> sub-child(1) = number
        ::new (static_cast<void*>(buf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xE0))->Child(flatIdx));  // B.3.6 bridge
        flatIdx++;
        ::new (static_cast<void*>(sub)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)buf)->Child(1));  // B.3.6 bridge
        void* number = *(void**)sub;

        // child[flatIdx] = button (standalone)
        ::new (static_cast<void*>(buf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xE0))->Child(flatIdx));  // B.3.6 bridge
        void* button = *(void**)buf;

        // child[flatIdx+1] = spinner (standalone)
        ::new (static_cast<void*>(buf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xE0))->Child(flatIdx + 1));  // B.3.6 bridge
        void* spinner = *(void**)buf;

        flatIdx += 2;  // advance past button + spinner (total +3 per row)

        // Set element IDs
        uint64_t r0[2] = {0, 0};
        new (r0) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(label));   // B.3.3 bridge
        if (*(void**)r0) reinterpret_cast<UI::Element*>(*(void**)r0)->SetElementId(baseId - 1000);  // B.3.6 bridge

        new (r0) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(number));   // B.3.3 bridge
        if (*(void**)r0) reinterpret_cast<UI::Element*>(*(void**)r0)->SetElementId(baseId);  // B.3.6 bridge

        // The spinner carries the id the msg-0x15 (spinner changed) notification sends
        // as param4. Retail tags it through SpinnerElement::SetElementId (exe dispatch slot
        // 0x140151cd8 -> "?SetElementId@SpinnerElement@UI@@QEAAXH@Z", Runtime 0x1800d7e10),
        // NOT Element::SetUserData and NOT the Element base SetElementId. The override
        // writes +0x88 on the spinner AND on the inner edit element at *(spinner+0xa0), so
        // both halves of the widget answer to the same id. Faithful, verified at the four
        // retail call sites (EDX = base+0x3e8 / +0x7d0 / +0xbb8 / +0xfa0).
        new (r0) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(spinner));   // B.3.3 bridge
        if (*(void**)r0) fn_SpinnerElement_SetElementId(*(void**)r0, baseId + 1000);
        TradeUI_ReportSpinnerTag("purchase", row, *(void**)r0, baseId + 1000);

        // Build 0x20-byte entry {label, number, button, spinner} and push to the
        // order-entries array at +0x90.
        // B.3.2 bridge: TradeUI+0x90 is Collection::Array<{label, number, button,
        // spinner}> (stride 0x20, count +0x98, capacity +0x9C).
        auto& orderRows = GameColl::ArrayAt<GameColl::Bytes<0x20, 8>>(s, 0x90);
        orderRows.SetSize(orderRows.GetSize() + 1,
                           GameColl::Bytes<0x20, 8>((uint64_t)label, (uint64_t)number,
                                          (uint64_t)button, (uint64_t)spinner));

        baseId++;
    }

    // --- Phase 3: Count resources and selectable builds via ExternalFactory ---
    int resourceTypeCount = 0;
    void* factory = *g_pExternalFactorySingleton;
    const ::Object::ExternalFactory* etuFactory =
        reinterpret_cast<const ::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    void* ext = const_cast<::Object::External*>(
        etuFactory->FindNext(*g_TypeId_RawMaterialDescription, nullptr));  // B.3.3 bridge
    while (ext != nullptr) {
        resourceTypeCount++;
        ext = const_cast<::Object::External*>(
            etuFactory->FindNext(*g_TypeId_RawMaterialDescription,
                                 reinterpret_cast<const ::Object::External*>(ext)));  // B.3.3 bridge
    }

    int selectableCount = 0;
    void* buildExt = const_cast<::Object::External*>(
        etuFactory->FindNext(*g_TypeId_ComponentDescription_FC, nullptr)); // B.3.3 bridge
    while (buildExt != nullptr) {
        // Check if this ComponentDescription external has a SelectDescription sub-component
        bool isDerivedBuild = fn_IsDerivedFrom(buildExt, *g_TypeId_ComponentDescription_FC);
        void* buildComp = isDerivedBuild ? buildExt : nullptr;
        if (buildComp) {
            int compCount = *(int*)((uintptr_t)buildComp + 0x30);
            void** compArray = *(void***)((uintptr_t)buildComp + 0x28);
            for (int c = 0; c < compCount; c++) {
                void* comp = compArray[c];
                typedef int (*GetTypeId_t)(void*);
                uintptr_t vtbl = *(uintptr_t*)comp;
                int typeId = ((GetTypeId_t)(*(void**)(vtbl + 0x10)))(comp);
                if (typeId == (int)*g_TypeId_SelectDescription) {
                    void* selectComp = nullptr;
                    if (fn_IsDerivedFrom(comp, *g_TypeId_SelectDescription))
                        selectComp = comp;
                    if (selectComp) selectableCount++;
                    break;
                }
            }
        }
        buildExt = const_cast<::Object::External*>(
            etuFactory->FindNext(*g_TypeId_ComponentDescription_FC,
                                 reinterpret_cast<const ::Object::External*>(buildExt))); // B.3.3 bridge
    }

    // --- Phase 4a: Setup tableStorage ---
    void* tableStorage = *(void**)(s + 0xD8);
    int storageColCount = *(int*)((uintptr_t)tableStorage + 0xC4);

    // Get template BEFORE SetSize (SetSize destroys children)
    {
        int rowCount = ((resourceTypeCount - 1 + storageColCount) / storageColCount * storageColCount)
                       / storageColCount;
        void* storageTemplate = TradeUI_GetTableTemplate((void*)(s + 0xD8));
        fn_TableElement_SetSize(*(void**)(s + 0xD8), rowCount, storageColCount);

        TradeUI_CloneTableCells(s, *(void**)(s + 0xD8), storageTemplate);
    }

    // Iterate tableStorage cells, find "ribbonStorageRow" sub-elements
    tableStorage = *(void**)(s + 0xD8);
    int storageCellCount = reinterpret_cast<const UI::Element*>(tableStorage)->ChildCount();  // B.3.6 bridge
    for (int i = 0; i < storageCellCount; i++) {
        // Find "ribbonStorageRow" in cell
        System::String strBuf("ribbonStorageRow");
        uint64_t cellBuf[2] = {0, 0};
        ::new (static_cast<void*>(cellBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xD8))->Child(i));  // B.3.6 bridge
        uint64_t rowBuf[2] = {0, 0};
        ::new (static_cast<void*>(rowBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)cellBuf)->FindElement(strBuf));  // B.3.6 bridge

        // Get 3 child elements from row: child(0), child(1), child(2)
        void* rowElem = *(void**)rowBuf;

        uint64_t c0[2] = {0, 0}, c1[2] = {0, 0}, c2[2] = {0, 0};
        // child(0) = label, child(1) = slider, child(2) = spinner
        ::new (static_cast<void*>(c0)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(0));  // B.3.6 bridge
        void* e0 = *(void**)c0;

        ::new (static_cast<void*>(c1)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(1));  // B.3.6 bridge
        void* e1 = *(void**)c1;

        ::new (static_cast<void*>(c2)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(2));  // B.3.6 bridge
        void* e2 = *(void**)c2;

        // child(1) is a SpinnerElement, not a slider: retail tags it through
        // SpinnerElement::SetElementId (slot 0x140151cd8), which sets +0x88 on the spinner
        // and on its inner edit element at *(spinner+0xa0). Retail call site 0x140085d40,
        // EDX = i + 0x7d0. See the tablePurchase loop above.
        new (c0) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(e1));   // B.3.3 bridge
        fn_SpinnerElement_SetElementId(*(void**)c0, i + 2000);
        TradeUI_ReportSpinnerTag("storage", i, *(void**)c0, i + 2000);


        // Build 0x18-byte entry {e0, e1, e2} and push to the storage-entries array
        // at +0x80.
        // B.3.2 bridge: TradeUI+0x80 is Collection::Array<{e0, e1, e2}> (stride
        // 0x18, count +0x88, capacity +0x8C).
        auto& storageRows = GameColl::ArrayAt<GameColl::Bytes<0x18, 8>>(s, 0x80);
        storageRows.SetSize(storageRows.GetSize() + 1,
                             GameColl::Bytes<0x18, 8>((uint64_t)e0, (uint64_t)e1, (uint64_t)e2));
    }

    // --- Phase 4b: Setup tableSell ---
    void* tableSell = *(void**)(s + 0xC8);
    int sellColCount = *(int*)((uintptr_t)tableSell + 0xC4);

    {
        int rowCount = (((resourceTypeCount - 1 + sellColCount) / sellColCount) * sellColCount)
                       / sellColCount;
        void* sellTemplate = TradeUI_GetTableTemplate((void*)(s + 0xC8));
        fn_TableElement_SetSize(*(void**)(s + 0xC8), rowCount, sellColCount);

        TradeUI_CloneTableCells(s, *(void**)(s + 0xC8), sellTemplate);
    }

    // Iterate tableSell cells, find "ribbonSellRow" sub-elements (4 children per row)
    tableSell = *(void**)(s + 0xC8);
    int sellCellCount = *(int*)((uintptr_t)tableSell + 0xC0);
    for (int i = 0; i < sellCellCount; i++) {
        System::String strBuf("ribbonSellRow");
        uint64_t cellBuf[2] = {0, 0};
        ::new (static_cast<void*>(cellBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xC8))->Child(i));  // B.3.6 bridge
        uint64_t rowBuf[2] = {0, 0};
        ::new (static_cast<void*>(rowBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)cellBuf)->FindElement(strBuf));  // B.3.6 bridge

        void* rowElem = *(void**)rowBuf;

        // Get 4 child elements from ribbonSellRow:
        // child(0)=SpinnerElement, child(1)=LabelElement, child(2)=SpinnerElement, child(3)=SliderElement
        uint64_t c[2];
        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(0));  // B.3.6 bridge
        void* spinElem = *(void**)c;      // child(0) = SpinnerElement

        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(1));  // B.3.6 bridge
        void* labelElem = *(void**)c;     // child(1) = LabelElement

        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(2));  // B.3.6 bridge
        void* numElem = *(void**)c;       // child(2) = SpinnerElement (amount)

        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(3));  // B.3.6 bridge
        void* sliderElem = *(void**)c;    // child(3) = SliderElement

        // child(3) is a SpinnerElement (the "SliderElement" naming is the same mislabel
        // reg_element_typeids.cpp:36 documents). Retail call site 0x14008614e, EDX = i+0xbb8.
        new (c) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(sliderElem));   // B.3.3 bridge
        fn_SpinnerElement_SetElementId(*(void**)c, i + 3000);
        TradeUI_ReportSpinnerTag("sell", i, *(void**)c, i + 3000);

        // Build 0x20-byte entry {child1, child0, child2, child3} and push to the
        // sell-entries array at +0x60.
        // +0=LabelElement, +8=SpinnerElement, +10=SpinnerElement, +18=SliderElement
        // B.3.2 bridge: TradeUI+0x60 is Collection::Array<{labelElem, spinElem,
        // numElem, sliderElem}> (stride 0x20, count +0x68, capacity +0x6C).
        auto& sellEntryRows = GameColl::ArrayAt<GameColl::Bytes<0x20, 8>>(s, 0x60);
        sellEntryRows.SetSize(sellEntryRows.GetSize() + 1,
                               GameColl::Bytes<0x20, 8>((uint64_t)labelElem, (uint64_t)spinElem,
                                              (uint64_t)numElem, (uint64_t)sliderElem));
    }

    // --- Phase 4c: Setup tableBuy ---
    void* tableBuy = *(void**)(s + 0xD0);
    int buyColCount = *(int*)((uintptr_t)tableBuy + 0xC4);

    {
        // tableBuy size: rows for (selectableCount + resourceTypeCount - 1) items
        int totalItems = selectableCount + resourceTypeCount - 1;

        int rowCount = (((totalItems + buyColCount) / buyColCount) * buyColCount) / buyColCount;
        void* buyTemplate = TradeUI_GetTableTemplate((void*)(s + 0xD0));
        fn_TableElement_SetSize(*(void**)(s + 0xD0), rowCount, buyColCount);

        TradeUI_CloneTableCells(s, *(void**)(s + 0xD0), buyTemplate);
    }

    // Iterate tableBuy cells, find "ribbonBuyRow" sub-elements (4 children per row)
    tableBuy = *(void**)(s + 0xD0);
    int buyCellCount = *(int*)((uintptr_t)tableBuy + 0xC0);
    for (int i = 0; i < buyCellCount; i++) {
        System::String strBuf("ribbonBuyRow");
        uint64_t cellBuf[2] = {0, 0};
        ::new (static_cast<void*>(cellBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xD0))->Child(i));  // B.3.6 bridge
        uint64_t rowBuf[2] = {0, 0};
        ::new (static_cast<void*>(rowBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)cellBuf)->FindElement(strBuf));  // B.3.6 bridge

        void* rowElem = *(void**)rowBuf;

        // child(0)=SpinnerElement, child(1)=LabelElement, child(2)=SpinnerElement, child(3)=SliderElement
        uint64_t c[2];
        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(0));  // B.3.6 bridge
        void* spinElem = *(void**)c;      // child(0) = SpinnerElement

        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(1));  // B.3.6 bridge
        void* labelElem = *(void**)c;     // child(1) = LabelElement

        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(2));  // B.3.6 bridge
        void* numElem = *(void**)c;       // child(2) = SpinnerElement

        ::new (static_cast<void*>(c)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->Child(3));  // B.3.6 bridge
        void* sliderElem = *(void**)c;    // child(3) = SliderElement

        // child(3) is a SpinnerElement (same mislabel as the sell loop).
        // Retail call site 0x14008653e, EDX = i + 0xfa0.
        new (c) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(sliderElem));   // B.3.3 bridge
        fn_SpinnerElement_SetElementId(*(void**)c, i + 4000);
        TradeUI_ReportSpinnerTag("buy", i, *(void**)c, i + 4000);

        // Build 0x20-byte entry {child1, child2, child3, child0} and push to the
        // buy-entries array at +0x70.
        // +0=LabelElement, +8=SpinnerElement, +10=SliderElement, +18=SpinnerElement
        // B.3.2 bridge: TradeUI+0x70 is Collection::Array<{labelElem, numElem,
        // sliderElem, spinElem}> (stride 0x20, count +0x78, capacity +0x7C).
        auto& buyEntryRows = GameColl::ArrayAt<GameColl::Bytes<0x20, 8>>(s, 0x70);
        buyEntryRows.SetSize(buyEntryRows.GetSize() + 1,
                              GameColl::Bytes<0x20, 8>((uint64_t)labelElem, (uint64_t)numElem,
                                             (uint64_t)sliderElem, (uint64_t)spinElem));
    }

    // --- Phase 4d: Setup tableOrder ---
    void* tableOrder = *(void**)(s + 0xE8);
    uint32_t orderColCount = *(uint32_t*)((uintptr_t)tableOrder + 0xC4);

    {
        int totalOrderItems = selectableCount + orderColCount + resourceTypeCount - 1;

        int rowCount = ((int)(totalOrderItems / (int)orderColCount * (int)orderColCount))
                       / (int)orderColCount;
        void* orderTemplate = TradeUI_GetTableTemplate((void*)(s + 0xE8));
        fn_TableElement_SetSize(*(void**)(s + 0xE8), rowCount, (int)orderColCount);

        TradeUI_CloneTableCells(s, *(void**)(s + 0xE8), orderTemplate);
    }

    // Iterate tableOrder cells, set element IDs (0x1F40+i) on purchase button children
    tableOrder = *(void**)(s + 0xE8);
    int orderCellCount = reinterpret_cast<const UI::Element*>(tableOrder)->ChildCount();  // B.3.6 bridge
    for (int i = 0; i < orderCellCount; i++) {
        uint64_t cellBuf[2] = {0, 0};
        ::new (static_cast<void*>(cellBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0xE8))->Child(i));  // B.3.6 bridge

        void* typed = *(void**)cellBuf;
        uint64_t ibp[2] = {0, 0};
        new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(typed));   // B.3.3 bridge
        reinterpret_cast<UI::Element*>(*(void**)ibp)->SetElementId(i + 0x1F40);  // B.3.6 bridge

        // Push 8-byte entry (just the element pointer) into the purchase array
        // at +0xA0.
        // B.3.2 bridge: TradeUI+0xA0 is Collection::Array<{elem}> (stride 0x08,
        // count +0xA8, capacity +0xAC).
        auto& purchaseRows = GameColl::ArrayAt<GameColl::Bytes<0x08, 8>>(s, 0xA0);
        purchaseRows.SetSize(purchaseRows.GetSize() + 1,
                              GameColl::Bytes<0x08, 8>(*(uint64_t*)ibp));
    }

    // --- Phase 5: Call refresh functions ---
    TradeUI_RefreshDisplay(self, 0, 0, 0);
    TradeUI_RefreshSellTable(self);
    TradeUI_RefreshBuyTable(self);
}

// ============================================================
// Startup (vtable +0x38): Trade value calculations
// ============================================================
// Original: 0x140082150
//
// Calculates total sell/buy values from order arrays and resource values.
// Checks storage capacity, shows/hides warning text (textNoStorage,
// textTooMuch, textNotEnough). Enables/disables trade and dismiss buttons
// based on trade state and merchant presence.
//
// sell orders: self+0x30 = ptr, self+0x38 = count, stride 0x20
//   entry[i]: +0x00 = resource entity ptr, +0x18 = quantity int
// buy orders: self+0x40 = ptr, self+0x48 = count, stride 0x18
//   entry[j]: +0x00 = resource entity ptr, +0x10 = quantity int
// resource entity: +0x28 = component array ptr, +0x30 = component count
//   component value at +0x48 (identified by vtable+0x10 returning g_TypeId_RawMaterialDescription)
// merchant state check: BuildState component, byte at +0x52 (or +0x50 if negative)
//   0x13 = merchant arrived (enables dismiss button)
// storage capacity: StateMachineComp+0x38 vs StorageComponent_QueryCount
//   bStorageFull = capacity < storageCount + (buyTotal - sellTotal)

// Inline helper to call UI::Element::SetEnabled via vtable slot 8 (+0x40)
// Callers pass the +0xF0/+0xF8 buttons (FindElement results) and the +0x80
// storage rows (Child results) — all UI::Element. Slot 8 is Enable(bool).
static inline void Element_SetEnabled(void* element, bool enabled) {
    reinterpret_cast<UI::Element*>(element)->Enable(enabled);  // B.3.6 bridge
}

// Find resource value for a given resource entity ptr by searching its component array
// Returns 0 if not found. Resource type identified via vtable+0x10 == g_TypeId_RawMaterialDescription.
static int FindResourceValue(void* resourceEntity) {
    if (!resourceEntity || !g_TypeId_RawMaterialDescription) return 0;
    uintptr_t entity = (uintptr_t)resourceEntity;
    int compCount = *(int*)(entity + 0x30);
    void** compArray = *(void***)(entity + 0x28);
    for (int k = 0; k < compCount; k++) {
        void* comp = compArray[k];
        if (!comp) continue;
        // Call vtable+0x10 to get type ID
        typedef int (*GetTypeId_t)(void* self);
        uintptr_t vtbl = *(uintptr_t*)comp;
        GetTypeId_t getType = *(GetTypeId_t*)(vtbl + 0x10);
        int typeId = getType(comp);
        if (typeId == (int)*g_TypeId_RawMaterialDescription) {
            // Verify with IsDerivedFrom, then return value at +0x48
            bool isRes = fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription);
            if (isRes) return *(int*)((uintptr_t)comp + 0x48);
            break;
        }
    }
    return 0;
}

void TradeUI_Startup(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find BuildingComponent and StateMachineComponent
    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    uint64_t storageCompPtr[2] = {0, 0};
    fn_FindComponent_Storage((void*)entityPtr[0], &storageCompPtr);

    // --- Compute sell total ---
    // sell orders: ptr at self+0x30, count at self+0x38, stride 0x20
    // each entry: +0x00=resource entity ptr, +0x18=quantity
    int sellTotal = 0;
    int sellCount = *(int*)(s + 0x38);
    uintptr_t sellBase = *(uintptr_t*)(s + 0x30);
    for (int i = 0; i < sellCount; i++) {
        uintptr_t entry = sellBase + (uintptr_t)i * 0x20;
        int qty = *(int*)(entry + 0x18);
        if (qty != 0) {
            void* resEntity = *(void**)(entry);
            int resVal = FindResourceValue(resEntity);
            sellTotal += resVal * qty;
        }
    }

    // --- Compute buy total ---
    // buy orders: ptr at self+0x40, count at self+0x48, stride 0x18
    // each entry: +0x00=resource entity ptr, +0x10=quantity
    int buyTotal = 0;
    int buyCount = *(int*)(s + 0x48);
    uintptr_t buyBase = *(uintptr_t*)(s + 0x40);
    for (int j = 0; j < buyCount; j++) {
        uintptr_t entry = buyBase + (uintptr_t)j * 0x18;
        int qty = *(int*)(entry + 0x10);
        if (qty != 0) {
            void* resEntity = *(void**)(entry);
            int resVal = FindResourceValue(resEntity);
            buyTotal += resVal * qty;
        }
    }

    // --- Read displayed totals from NumberElements ---
    int displaySellTotal = *(int*)(*(uintptr_t*)(s + 0x100) + 0xd8);
    int displayBuyTotal  = *(int*)(*(uintptr_t*)(s + 0x108) + 0xd8);

    bool bOverBought  = displaySellTotal < displayBuyTotal;  // buy value > sell value
    bool bBuySell     = displayBuyTotal  < displaySellTotal; // buy < sell (textTooMuch case)

    // --- Storage capacity check ---
    uintptr_t storageComp = storageCompPtr[0];
    int storageCapacity = *(int*)(storageComp + 0x38);
    int storageCount = fn_StorageComponent_QueryCount((void*)storageComp);
    bool bStorageFull = storageCapacity < storageCount + (buyTotal - sellTotal);

    // --- Show/hide warning texts ---
    if (bOverBought) {
        // Buy value exceeds sell value — show "not enough" text with difference
        System::String strBuf = System::String::Format("%d", (unsigned)(displayBuyTotal - displaySellTotal));
        fn_TextElement_SetParameter(*(void**)(s + 0x120), 0, &strBuf);
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x110))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textNoStorage
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x118))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textTooMuch
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x120))->SetFlagsOnOff(0, 1);  // B.3.6 bridge — show textNotEnough
    } else if (bStorageFull) {
        // Storage overflow — show "no storage" text
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x110))->SetFlagsOnOff(0, 1);  // B.3.6 bridge — show textNoStorage
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x118))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textTooMuch
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x120))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textNotEnough
    } else if (!bBuySell) {
        // Totals are equal (no net trade) — hide all warning texts
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x110))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textNoStorage
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x118))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textTooMuch
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x120))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textNotEnough
    } else {
        // Sell value exceeds buy value — show "too much" text with difference
        System::String strBuf = System::String::Format("%d", (unsigned)(displaySellTotal - displayBuyTotal));
        fn_TextElement_SetParameter(*(void**)(s + 0x118), 0, &strBuf);
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x110))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textNoStorage
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x118))->SetFlagsOnOff(0, 1);  // B.3.6 bridge — show textTooMuch
        reinterpret_cast<UI::Element*>(*(void**)(s + 0x120))->SetFlagsOnOff(1, 0);  // B.3.6 bridge — hide textNotEnough
    }

    // --- Find BuildState (merchant) component and check merchant arrival state ---
    // FindComponent_BuildState walks building's component array for the BuildState type
    uint64_t buildStatePtr[2] = {0, 0};
    fn_FindComponent_BuildState((void*)entityPtr[0], &buildStatePtr);

    uintptr_t buildState = buildStatePtr[0];
    // State byte at +0x52 (primary), +0x50 (fallback if negative)
    int8_t stateByte = *(int8_t*)(buildState + 0x52);
    if (stateByte < 0) stateByte = *(int8_t*)(buildState + 0x50);
    bool merchantArrived = (stateByte == 0x13);

    // --- Enable/disable buttonDismiss (enabled when merchant is present) ---
    Element_SetEnabled(*(void**)(s + 0xf8), merchantArrived);

    // --- Enable/disable buttonTrade ---
    // Enabled only if: buyTotal.value > 0, no storage overflow, no over-bought
    bool tradeEnabled = (displayBuyTotal > 0) && !bStorageFull && !bOverBought;
    Element_SetEnabled(*(void**)(s + 0xf0), tradeEnabled);

    // --- Layout recalc for sell and buy tables ---
    reinterpret_cast<UI::Element*>(*(void**)(s + 0xC8))->SetNeedsSizeCalc();  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(*(void**)(s + 0xD0))->SetNeedsSizeCalc();  // B.3.6 bridge

    // --- Refresh table displays ---
    TradeUI_RefreshOrderQuantityDisplay(self);
    TradeUI_RefreshSellSpinnerValues(self);
    TradeUI_RefreshOrderDisplay(self);
}

// ============================================================
// OnCreate (vtable +0x40): Refresh on order changes
// ============================================================
// Original: 0x1400827b0
//
// Finds BuildingComponent, checks if the buy order count (+0x48 on self)
// differs from building's order count (+0x58 on building). If changed,
// refreshes sell, buy, and order tables.

void TradeUI_OnCreate(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get owning entity and find BuildingComponent
    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Check if order count changed
    int selfOrderCount = *(int*)(s + 0x48);
    int buildingOrderCount = *(int*)(buildingPtr[0] + 0x58);

    if (selfOrderCount != buildingOrderCount) {
        // Order count changed — refresh sell, buy, and order tables
        TradeUI_RefreshSellTable(self);
        TradeUI_RefreshBuyTable(self);
        TradeUI_RefreshDisplay(self, 0, 0, 0);
    }

    // Always update totals
    TradeUI_UpdateSellTotals(self);
    TradeUI_UpdateBuyTotals(self);
}

// ============================================================
// OnRemove (vtable +0x48): Event dispatcher
// ============================================================
// Original: 0x140081f20
//
// Dispatches UI events based on type and element ID:
//   eventType 0x15: storage spinners (0x7d0+), sell spinners (0xbb8+),
//     buy spinners (0xfa0+), order amount spinners (0x1b58+)
//   eventType 0x12: buttonTrade (0x4e20), buttonDismiss (0x4e21),
//     orderUp spinners (0x1388+), orderDown spinners (0x1770+),
//     purchase buttons (0x1f40+)
//   eventType 0x14: comboPurchase (0x4e22), comboOrder (0x4e23), sortCombo (0xc7)
//
// Note: The three event type sections fall through to a shared check for
// eventType 0x14 IDs, so eventType 0x12 events can also trigger 0x14 handlers
// if no 0x12 match was found.


void TradeUI_OnRemove(TradeUI* self, void* param2, int eventType, int eventId) {
    uintptr_t s = (uintptr_t)self;


    int switchVal = eventType - 0x12;
    if (switchVal == 0) {
        // --- eventType 0x12: click events ---
        if (eventId == 0x4e20) {
            // buttonTrade — the CHECKOUT. Ported in code/Game/Dialog/tradeui_checkout.cpp
            // and registered at 0x140081420 (reg_tradeui_checkout.cpp), so
            // fn_TradeUI_OnTradeButton is non-null. It is nonetheless still gated:
            // the handler itself is opt-in behind BANISHED_LIVE_TRADE_CHECKOUT and
            // returns without committing anything while that is unset. The null
            // check below survives only as EXEC-of-0 insurance for a build where the
            // registration wave is not linked in.
            if (!fn_TradeUI_OnTradeButton) {
                static bool s_warned = false;
                if (!s_warned) {
                    s_warned = true;
                    fprintf(stderr, "[TradeUI] buttonTrade clicked but the checkout "
                            "(0x140081420) did not resolve — is reg_tradeui_checkout.cpp "
                            "linked? Ignoring the click.\n");
                    fflush(stderr);
                }
                return;
            }
            fn_TradeUI_OnTradeButton(self, param2, nullptr, 0);
            return;
        }
        if (eventId == 0x4e21) {
            TradeUI_OnDismissButton(self);
            return;
        }
        // orderUp spinners: id range [0x1388, 0x1388 + orderCount]
        {
            int orderCount = *(int*)(s + 0x98);
            if (eventId >= 0x1388 && eventId <= orderCount + 0x1388) {
                TradeUI_OnOrderSpinner(self, eventId - 0x1388, eventId - 0x1389);
                goto check_type14;
            }
        }
        // orderDown spinners: id range [0x1770, 0x1770 + orderCount]
        {
            int orderCount = *(int*)(s + 0x98);
            if (eventId >= 0x1770 && eventId <= orderCount + 0x1770) {
                // Note: second arg is idx+1 relative to base (idx-0x176f = idx-0x1770+1)
                TradeUI_OnOrderSpinner(self, eventId - 0x1770, eventId - 0x176f);
                goto check_type14;
            }
        }
        // purchase buttons: id range [0x1f40, 0x1f40 + purchaseCount]
        {
            int purchaseCount = *(int*)(s + 0xa8);
            if (eventId >= 0x1f40 && eventId <= purchaseCount + 0x1f40) {
                TradeUI_OnPurchaseButton(self, eventId - 0x1f40);
                goto check_type14;
            }
        }
        goto check_type14;
    }

    switchVal -= 2;
    if (switchVal == 0) {
        // --- eventType 0x14: combo/sort selection events ---
        // (handled below in check_type14)
        goto check_type14;
    }

    switchVal -= 1;
    if (switchVal != 0) {
        return;
    }

    // --- eventType 0x15: spinner change events ---
    // storage spinners: id range [0x7d0, 0x7d0 + storageCount)
    {
        int storageCount = *(int*)(s + 0x58);
        if (eventId >= 0x7d0 && eventId < storageCount + 0x7d0) {
            TradeUI_OnStorageSpinner(self, eventId - 0x7d0, param2);
            return;
        }
    }
    // sell spinners: id range [0xbb8, 0xbb8 + tableSell.childCount)
    if (eventId >= 0xbb8) {
        void* tableSell = *(void**)(s + 0xC8);
        int sellChildCount = *(int*)((uintptr_t)tableSell + 0xC0);
        if (eventId < sellChildCount + 0xbb8) {
            TradeUI_OnSellSpinner(self, eventId - 0xbb8);
            return;
        }
    }
    // buy spinners: id range [0xfa0, 0xfa0 + tableBuy.childCount)
    if (eventId >= 0xfa0) {
        void* tableBuy = *(void**)(s + 0xD0);
        int buyChildCount = *(int*)((uintptr_t)tableBuy + 0xC0);
        if (eventId < buyChildCount + 0xfa0) {
            TradeUI_OnBuySpinner(self, eventId - 0xfa0);
            return;
        }
    }
    // order amount spinners: id range [0x1b58, 0x1b58 + orderCount)
    {
        int orderCount = *(int*)(s + 0x98);
        if (eventId >= 0x1b58 && eventId < orderCount + 0x1b58) {
            TradeUI_OnOrderAmountSpinner(self, eventId - 0x1b58);
            return;
        }
    }
    return;

check_type14:
    // Shared check for combo/sort events (reached from type 0x12 and 0x14)
    if (eventId == 0x4e22) {
        TradeUI_OnComboPurchase(self);
        return;
    }
    if (eventId == 0x4e23) {
        TradeUI_OnComboOrder(self);
        return;
    }
    if (eventId == 0xc7) {
        TradeUI_RefreshDisplay(self, 0, 0, 0);
    }
}

// ============================================================
// Update (vtable +0x50): Serialize tab/sort state
// ============================================================
// Original: 0x1400868c0
//
// Finds "tradeTabs" TabElement, serializes active tab index and
// sortCombo selection via IO::Serializer. On load, restores tab
// and combo state, then refreshes the sorted display.

void TradeUI_Update(TradeUI* self, void* stream, void* p3, int64_t p4) {
    uintptr_t s = (uintptr_t)self;

    // Find "tradeTabs" TabElement
    System::String strBuf("tradeTabs");

    uint64_t foundBuf[2] = {0, 0};
    ::new (static_cast<void*>(foundBuf)) ::Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0x20))->FindElement(strBuf));  // B.3.6 bridge

    void* element = *(void**)foundBuf;

    // Cast to TabElement via IsDerivedFrom
    void* tabElement = nullptr;
    if (element && g_TypeId_TabElement) {
        bool isDerived = fn_IsDerivedFrom(element, *g_TypeId_TabElement);
        if (isDerived) tabElement = element;
    }

    uint64_t tabIBP = 0;
    new (&tabIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(tabElement));   // B.3.3 bridge

    // Read current tab index from TabElement (+0xf8)
    int tabIndex = *(int*)(tabIBP + 0xf8);

    // Read current sortCombo selection (+0x140 element, +0x118 = combo index)
    uint32_t sortIndex = *(uint32_t*)(*(uintptr_t*)(s + 0x140) + 0x118);

    // Create serializer
    alignas(8) uint64_t serializer[3] = {0, 0, 0};
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Only serialize if version > 0 (newer save format)
    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version > 0) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>((int*)&sortIndex), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&tabIndex), nullptr);   // B.3.4 bridge
    }

    // On load (mode == 0), restore tab and combo state
    if (*(int*)(serializer[0] + 8) == 0) {
        fn_TabElement_SetActiveTab((void*)tabIBP, tabIndex);

        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x140), sortIndex);

        // Refresh sorted display
        TradeUI_RefreshDisplay(self, (uint64_t)sortIndex, 0, p4);
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// UpdateBuyTotals (FUN_140082850): Sum buy order values into numberBuyTotal
// ============================================================
// Iterates all buy orders (ptr at self+0x40, count at self+0x48, stride 0x18).
// Each entry: +0x10 = quantity. Resource value = building's resource value table:
//   building+0x50 = resource type ptr array (sorted), stride 0x10
//   For entry i: value at *(building+0x50)[stride*i - 8]
// Then sets numberBuyTotal (+0x108) to the accumulated total.

void TradeUI_UpdateBuyTotals(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Iterate buy orders: ptr at +0x40, count at +0x48, stride 0x18
    // Each entry: [0]=resource entity ptr, [+0x10]=quantity int
    // Resource value = building's type-indexed price table:
    //   building+0x50 = ptr to struct of resource types (sorted set/tree?)
    //   Ghidra: *(int *)(pIVar3 + *(longlong *)(local_res10 + 0x50) + -8)
    //   where pIVar3 increments by 0x10 per iteration starting at 0
    int total = 0;
    int buyCount = *(int*)(s + 0x48);
    uintptr_t building = buildingPtr[0];

    uintptr_t resourceTypeBase = *(uintptr_t*)(building + 0x50);
    uintptr_t buyBase = *(uintptr_t*)(s + 0x40);
    for (int i = 0; i < buyCount; i++) {
        uintptr_t entry = buyBase + (uintptr_t)i * 0x18;
        int qty = *(int*)(entry + 0x10);
        if (qty != 0) {
            // Price lives at entry+8 of the TradeComponent's buy-order array
            // (building+0x50, stride 0x10: +0x00 entity, +0x08 unit price,
            //  +0x0c quantity — the same layout TradeComponent_RemoveBuyOrder
            //  edits in tradeui_checkout.cpp).
            //
            // The index register is PRE-incremented before the load, which the
            // decompiler renders as a bare "-8": at 0x1400828cd the induction
            // variable becomes (i+1)*0x10 and the load at 0x1400828d1 reads
            // [-8] off it, i.e. i*0x10 + 8 — NOT i*0x10 - 8. Transcribing the
            // "-8" literally read entry i-1's price and, at i == 0, read eight
            // bytes off the front of the array.
            int price = *(int*)(resourceTypeBase + (uintptr_t)i * 0x10 + 8);
            total += price * qty;
        }
    }

    fn_NumberElement_SetValueInt(*(void**)(s + 0x108), total);
}

// ============================================================
// UpdateSellTotals (FUN_140082920): Sum sell order values into numberSellTotal
// ============================================================
// Iterates all sell orders (ptr at self+0x30, count at self+0x38, stride 0x20).
// Each entry: +0x18 = quantity. Uses Building_GetSellableCount to get unit value.
// Sets numberSellTotal (+0x100) to accumulated total.

void TradeUI_UpdateSellTotals(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    int total = 0;
    int sellCount = *(int*)(s + 0x38);
    uintptr_t building = buildingPtr[0];

    uintptr_t sellBase = *(uintptr_t*)(s + 0x30);
    for (int i = 0; i < sellCount; i++) {
        uintptr_t entry = sellBase + (uintptr_t)i * 0x20;
        int qty = *(int*)(entry + 0x18);
        if (qty != 0) {
            void* resourceEntity = *(void**)(entry);
            // FUN_14005a520: gets available sell value for this resource at the building
            int unitVal = fn_Building_GetSellableCount((void*)building, resourceEntity);
            total += qty * unitVal;
        }
    }

    fn_NumberElement_SetValueInt(*(void**)(s + 0x100), total);
}

// ============================================================
// OnBuySpinner (FUN_1400819f0): Store buy spinner value into buy order entry
// ============================================================
// Reads spinner value from buy entry [idx].spinner (+0x10 in 0x20-stride buyEntries array).
// Stores to buy orders (+0x40 base, stride 0x18, offset +0x10 for qty).
// Then calls UpdateBuyTotals.

void TradeUI_OnBuySpinner(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    // Buy entries at self+0x70, stride 0x20, spinner at +0x10
    uintptr_t entryBase = *(uintptr_t*)(s + 0x70);
    uintptr_t entry = entryBase + (uintptr_t)idx * 0x20;

    int val = fn_SpinnerElement_GetValue(*(void**)(entry + 0x10));

    // Store to buy order qty at (buyOrdersBase + idx*0x18 + 0x10)
    uintptr_t buyOrders = *(uintptr_t*)(s + 0x40);
    *(int*)(buyOrders + (uintptr_t)idx * 0x18 + 0x10) = val;

    TradeUI_UpdateBuyTotals(self);
}

// ============================================================
// OnSellSpinner (FUN_140081a50): Store sell spinner value into sell order entry
// ============================================================
// Reads spinner value from sell entry [idx].spinner (+0x18 in 0x20-stride sellEntries).
// Stores to sell orders (+0x30 base, stride 0x20, offset +0x18 for qty).
// Then calls UpdateSellTotals.

void TradeUI_OnSellSpinner(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    // Sell entries at self+0x60, stride 0x20, spinner at +0x18
    uintptr_t entryBase = *(uintptr_t*)(s + 0x60);
    uintptr_t entry = entryBase + (uintptr_t)idx * 0x20;

    int val = fn_SpinnerElement_GetValue(*(void**)(entry + 0x18));

    // Store to sell order qty at (sellOrdersBase + idx*0x20 + 0x18)
    uintptr_t sellOrders = *(uintptr_t*)(s + 0x30);
    *(int*)(sellOrders + (uintptr_t)idx * 0x20 + 0x18) = val;

    TradeUI_UpdateSellTotals(self);
}

// ============================================================
// OnComboPurchase (FUN_140081bc0): Save comboPurchase selection to building
// ============================================================
// Reads comboPurchase (+0x138) current item index (+0x118).
// Stores it to building component's field at +0x28.

void TradeUI_OnComboPurchase(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Read combo selection
    uint32_t comboIdx = *(uint32_t*)(*(uintptr_t*)(s + 0x138) + 0x118);

    // Store to building +0x28
    *(uint32_t*)(buildingPtr[0] + 0x28) = comboIdx;
}

// ============================================================
// OnComboOrder (FUN_140081e80): Save comboOrder selection to building's order record
// ============================================================
// Reads comboOrder (+0xC0) current item index (+0x118).
// Stores to building's order slot at: orders_ptr + (curOrderSlot * 0x20 + 8)
// where curOrderSlot = building +0x2c.

void TradeUI_OnComboOrder(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Read comboOrder selection
    uint32_t comboIdx = *(uint32_t*)(*(uintptr_t*)(s + 0xC0) + 0x118);

    // Store to building's current order slot +8
    uintptr_t building = buildingPtr[0];
    int curSlot = *(int*)(building + 0x2c);
    uintptr_t ordersBase = *(uintptr_t*)(building + 0x30);
    *(uint32_t*)(ordersBase + (uintptr_t)(uint32_t)curSlot * 0x20 + 8) = comboIdx;
}

// ============================================================
// OnOrderAmountSpinner (FUN_140081c50): Save order amount to building order table
// ============================================================
// Reads order amount spinner from orderEntries (+0x90, stride 0x20, spinner at +0x18).
// Stores to building's order amount at: building.orders (+0x40) + idx*0xc + 8.

void TradeUI_OnOrderAmountSpinner(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Read spinner value from orderEntries[idx].spinner (at +0x18 in 0x20-stride array)
    uintptr_t orderEntryBase = *(uintptr_t*)(s + 0x90);
    uintptr_t entry = orderEntryBase + (uintptr_t)idx * 0x20;

    int val = fn_SpinnerElement_GetValue(*(void**)(entry + 0x18));

    // Store to building's order amount table: building.ordersAmt (+0x40) + idx*0xc + 8
    uintptr_t ordersAmt = *(uintptr_t*)(buildingPtr[0] + 0x40);
    *(int*)(ordersAmt + (uintptr_t)idx * 0xc + 8) = val;
}

// ============================================================
// OnOrderSpinner (FUN_140081d00): Swap two order entries in building's order table
// ============================================================
// Swaps entries at indices param_2 and param_3 in building's 0xc-stride order array (+0x40).
// Used when up/down order spinner buttons are clicked.

void TradeUI_OnOrderSpinner(TradeUI* self, int idx, int idx2) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    uintptr_t ordersBase = *(uintptr_t*)(buildingPtr[0] + 0x40);

    // Swap 12-byte entries at idx and idx2 (stride 0xc)
    uintptr_t a = ordersBase + (uintptr_t)idx  * 0xc;
    uintptr_t b = ordersBase + (uintptr_t)idx2 * 0xc;

    uint32_t a0 = *(uint32_t*)(a + 0);
    uint32_t a1 = *(uint32_t*)(a + 4);
    uint32_t a2 = *(uint32_t*)(a + 8);

    *(uint32_t*)(a + 0) = *(uint32_t*)(b + 0);
    *(uint32_t*)(a + 4) = *(uint32_t*)(b + 4);
    *(uint32_t*)(a + 8) = *(uint32_t*)(b + 8);

    *(uint32_t*)(b + 0) = a0;
    *(uint32_t*)(b + 4) = a1;
    *(uint32_t*)(b + 8) = a2;
}

// ============================================================
// OnDismissButton (FUN_140081950): Dismiss the merchant
// ============================================================
// Finds BuildState component on the owning entity. Reads merchant
// state byte from +0x51 (sign-extended). Calls vtable[2] on the
// object at buildState+0x48 to trigger dismissal, then refreshes
// display and both sell/buy tables.
//
// Original asm register setup (interleaved for RefreshDisplay):
//   RCX = *(buildState+0x48)        — object ptr (this for vtable call)
//   R8D = MOVSX byte [buildState+0x51] — state byte (3rd arg to RefreshDisplay)
//   RDX = buildState + 0x48         — address (2nd arg to RefreshDisplay)
//   R9D = 10                        — constant (4th arg to RefreshDisplay)

void TradeUI_OnDismissButton(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    // Find BuildState component (FUN_140003ea0)
    uint64_t buildStatePtr[2] = {0, 0};
    fn_FindComponent_BuildState((void*)entityPtr[0], buildStatePtr);

    uintptr_t buildState = buildStatePtr[0];

    // Read merchant state byte from +0x51 (sign-extended, no fallback)
    int8_t stateByte = *(int8_t*)(buildState + 0x51);
    uintptr_t stateAddr = buildState + 0x48;
    uint64_t stateVal = (uint64_t)(uint32_t)(int32_t)stateByte;

    // Call vtable[2] on object at buildState+0x48 (dismiss notification)
    // Original: MOV RCX,[RDX+0x48]; MOV RAX,[RCX]; CALL [RAX+0x10]
    void* obj = *(void**)(buildState + 0x48);
    void** vtbl = *(void***)obj;
    typedef void (*StateCall_t)(void* self);
    ((StateCall_t)(vtbl[2]))(obj);

    // Refresh display and tables
    TradeUI_RefreshDisplay(self, stateAddr, stateVal, 10);
    TradeUI_RefreshSellTable(self);
    TradeUI_RefreshBuyTable(self);
}

// ============================================================
// OnPurchaseButton (FUN_140081dd0): Toggle a purchase order entry
// ============================================================
// Reads the checked state of the purchase button at index param_2 (+0xd8 = checked byte).
// Gets the corresponding resource entity from self's purchaseReserved (+0xb0, stride 8).
// Calls Building_AddPurchaseOrder(building, resource, checkedState).

void TradeUI_OnPurchaseButton(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Get purchase button element from purchaseEntries (+0xA0, stride 8)
    uintptr_t purchaseBase = *(uintptr_t*)(s + 0xA0);
    uintptr_t buttonPtr = purchaseBase + (uintptr_t)idx * 8;
    void* button = *(void**)buttonPtr;

    // Read checked state byte at +0xd8
    int8_t checked = *(int8_t*)((uintptr_t)button + 0xd8);

    // Get corresponding resource entity from purchaseReserved (+0xb0, stride 8)
    uintptr_t reservedBase = *(uintptr_t*)(s + 0xB0);
    void* resourceEntity = *(void**)(reservedBase + (uintptr_t)idx * 8);

    // Call Building_AddPurchaseOrder
    fn_Building_AddPurchaseOrder((void*)buildingPtr[0], resourceEntity, checked);
}

// ============================================================
// OnStorageSpinner (FUN_140081ab0): Update storage resource assignment
// ============================================================
// Reads the new storage spinner value for resource at idx.
// Looks up resource entity from self's storageOrdersArray (+0x50, stride 0x18).
// Searches building's storage BST (+0x60) for the resource.
// If found: update quantity. If not found: insert new entry.
// Then refreshes sell table and display.

void TradeUI_OnStorageSpinner(TradeUI* self, int idx, void* /*reserved*/) {
    uintptr_t s = (uintptr_t)self;

    // storageEntries at self+0x80, stride 0x18, spinner at +0x08
    uintptr_t storageEntries = *(uintptr_t*)(s + 0x80);
    uintptr_t entryOffset = (uintptr_t)idx * 0x18;
    int newVal = fn_SpinnerElement_GetValue(*(void**)(storageEntries + 0x08 + entryOffset));

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Get resource entity ptr from storageOrdersArray (+0x50, stride 0x18, first field)
    uintptr_t storageOrders = *(uintptr_t*)(s + 0x50);
    void* resourceEntity = *(void**)(storageOrders + entryOffset);

    uintptr_t building = buildingPtr[0];
    uintptr_t storageBST = building + 0x60;  // BST root pointer at building+0x60

    // Search BST for existing entry and update or insert
    // The insert/update logic (FUN_14000e1c0) handles both cases:
    // If resource already in BST: update quantity
    // If not found: insert new leaf
    uint64_t newEntry[2] = { (uint64_t)resourceEntity, (uint64_t)(uint32_t)newVal };
    void* retval = nullptr;
    fn_StorageTree_Insert((void*)storageBST, &retval, newEntry);

    // If we got a result back (found existing), update its qty
    if (retval) {
        *(int*)((uintptr_t)retval + 8) = newVal;
    }


    TradeUI_RefreshSellTable(self);
    TradeUI_RefreshDisplay(self, (uint64_t)retval, (uint64_t)newEntry, (int64_t)building);
}

// ============================================================
// RefreshOrderQuantityDisplay (FUN_140082fb0): Update order quantity spinners
// ============================================================
// For each storage entry (count at self+0x58, storageArray at self+0x50, stride 0x18):
//   - Gets resource entity from storageArray[i] (offset 0)
//   - Searches StorageComponent's list (at +0x28/+0x30) for matching entity
//   - Sets NumberElement value (storageEntries[i].numberElement at +0x10) to found qty
// Note: StorageComponent is found via entity+FindComponent_Storage (0x140006270).

void TradeUI_RefreshOrderQuantityDisplay(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Check building's base type — used later for sprite/text lookups
    // IsDerivedFrom call on building's type desc at +0x18 — just a type check, result unused here
    // fn_IsDerivedFrom(*(void**)(buildingPtr[0] + 0x18), DAT_1401c4bb8);

    uint64_t storageCompPtr[2] = {0, 0};
    fn_FindComponent_Storage((void*)entityPtr[0], &storageCompPtr);

    int storageCount = *(int*)(s + 0x58);

    if (storageCount > 0) {
        for (int i = 0; i < storageCount; i++) {
            uintptr_t orderOffset = (uintptr_t)i * 0x18;
            uintptr_t storageOrders = *(uintptr_t*)(s + 0x50);   // re-read each iteration like original
            uintptr_t storageEntries = *(uintptr_t*)(s + 0x80);  // re-read each iteration like original
            void* resourceEntity = *(void**)(storageOrders + orderOffset);

            // Search StorageComponent's item list for this resource entity
            // Storage list: ptr at comp+0x28, count at comp+0x30
            // List entries: pairs [resource_entity_ptr, qty] each 0x10 bytes (2 x 8-byte)
            uintptr_t storageComp = storageCompPtr[0];
            int listCount = *(int*)(storageComp + 0x30);
            void** itemList = *(void***)(storageComp + 0x28);

            int foundQty = 0;
            for (int64_t j = 0; j < listCount; j++) {
                if (itemList[j * 2] == resourceEntity) {
                    if (j >= 0) {
                        foundQty = (int)(uintptr_t)itemList[j * 2 + 1];
                    }
                    break;
                }
            }

            // Set NumberElement value
            fn_NumberElement_SetValueInt(*(void**)(storageEntries + orderOffset + 0x10), foundQty);
        }
    }
}

// ============================================================
// RefreshSellSpinnerValues (FUN_140082d80): Update sell spinners with available amounts
// ============================================================
// For each sell order entry (count at self+0x38, sellOrders at self+0x30, stride 0x20):
//   - Gets available resource count via Building_GetSellableCount
//   - Searches StorageComponent's list for current stored qty
//   - Sets NumberElement (+0x08) to storedQty
//   - Sets SpinnerElement (+0x18) min=0, max=storedQty (or 0 if no merchant / available<=0)
//   - Sets spinner increment (1 if max<=0x14, 5 if max<=0x32, 20 if max<=0x64, skip if >0x64)
// Also checks merchant state: if not arrived, force max=0 (spinners disabled).

void TradeUI_RefreshSellSpinnerValues(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Type check on building's class info at +0x18 (unused result, but done in original)
    // fn_IsDerivedFrom(*(void**)(buildingPtr[0] + 0x18), DAT_1401c4bb8);

    uint64_t storageCompPtr[2] = {0, 0};
    fn_FindComponent_Storage((void*)entityPtr[0], &storageCompPtr);

    int sellCount = *(int*)(s + 0x38);

    // Check merchant state: Find BuildState component
    uint64_t buildStatePtr[2] = {0, 0};
    fn_FindComponent_BuildState((void*)entityPtr[0], buildStatePtr);

    // BuildState component: buildStatePtr[0] is the component pointer after CheckPtr.
    // The decompiler shows *(longlong*)self which is the IBP dereference = buildStatePtr[0].
    // Then byte at +0x52 (fallback +0x50 if negative) compared to 0x13 (merchant arrived).
    uintptr_t bsComp = buildStatePtr[0];
    int8_t stateByte = *(int8_t*)(bsComp + 0x52);
    if (stateByte < 0) stateByte = *(int8_t*)(bsComp + 0x50);
    bool merchantArrived = (stateByte == 0x13);

    if (sellCount > 0) {
        for (int i = 0; i < sellCount; i++) {
            uintptr_t sellOffset = (uintptr_t)i * 0x20;
            uintptr_t sellEntries = *(uintptr_t*)(s + 0x60);  // re-read each iteration
            uintptr_t sellOrders  = *(uintptr_t*)(s + 0x30);  // re-read each iteration
            uintptr_t entryBase   = sellEntries + sellOffset;
            void* resourceEntity  = *(void**)(sellOrders + sellOffset);

            // Get available sell count from building component
            int available = fn_Building_GetSellableCount((void*)buildingPtr[0], resourceEntity);

            // Search StorageComponent's item list for current stored qty
            uintptr_t storageComp = storageCompPtr[0];
            int listCount = *(int*)(storageComp + 0x30);
            void** itemList = *(void***)(storageComp + 0x28);
            int storedQty = 0;
            for (int64_t j = 0; j < listCount; j++) {
                if (itemList[j * 2] == resourceEntity) {
                    if (j >= 0) {
                        storedQty = (int)(uintptr_t)itemList[j * 2 + 1];
                    }
                    break;
                }
            }

            // Set NumberElement (+0x08) to storedQty
            fn_NumberElement_SetValueInt(*(void**)(entryBase + 0x08), storedQty);

            // Determine spinner max: storedQty if merchant arrived AND available > 0, else 0
            int spinnerMax = storedQty;
            if (!merchantArrived || available < 1) {
                spinnerMax = 0;
            }

            // sellUI+0x18 is a SliderElement — use NumberEditElement::SetMinMax
            SliderElement_SetMinMax(*(void**)(entryBase + 0x18), 0, spinnerMax);

            // Re-read spinner ptr and CheckPtr for GetMaxValue (matches original pattern)
            int actualMax = fn_SpinnerElement_GetMaxValue(*(void**)(entryBase + 0x18));

            // Set increment based on max value; skip SetIncrement entirely if max > 0x64
            if (actualMax <= 0x14) {
                fn_SpinnerElement_SetIncrement(*(void**)(entryBase + 0x18), 1);
            } else if (actualMax <= 0x32) {
                fn_SpinnerElement_SetIncrement(*(void**)(entryBase + 0x18), 5);
            } else if (actualMax <= 0x64) {
                fn_SpinnerElement_SetIncrement(*(void**)(entryBase + 0x18), 20);
            }
            // else: max > 0x64 (100), do NOT call SetIncrement (original skips it)
        }
    }
}

// ============================================================
// RefreshOrderDisplay (FUN_140082b20): Update order table UI rows
// ============================================================
// For each active order in building.orders (+0x40, count at building+0x48, stride 0xc):
//   entry +0 = type index, entry +4 = resource type index, entry +8 = amount
// Gets sprite from BuildType description sprite sheet and sets it on LabelElement.
// Gets name text from BuildType string table and sets it on TextElement/LabelElement.
// Sets SpinnerElement value (order amount).
// Also handles enable/disable on the last element differently.
// NOTE: This function requires UI::Sprite and UI::String constructors — calls via
//       existing fn_LabelElement_SetSprite and fn_TextElement_SetText as raw vtable calls.

void TradeUI_RefreshOrderDisplay(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    int iVar7 = 0;
    uintptr_t building = buildingPtr[0];

    int orderCount = *(int*)(building + 0x48);
    if (orderCount <= 0) return;

    for (int64_t idx = 0; idx < orderCount; idx++) {
        // UI entry array at self+0x90, stride 0x20 (4 InternalBasePtrs per entry)
        uintptr_t uiEntry = *(uintptr_t*)(s + 0x90) + (uintptr_t)idx * 0x20;

        // Read order amount from building orders array (stride 0xc)
        int64_t orderOff = idx * 0xc;
        uintptr_t ordersBase = *(uintptr_t*)(buildingPtr[0] + 0x40);
        int amount = *(int*)(ordersBase + orderOff + 8);

        // Set spinner value (uiEntry[3] = offset +0x18)
        fn_SpinnerElement_SetValue(*(void**)(uiEntry + 0x18), amount);

        // Get buildTypeDesc and typeIdx for sprite/string lookups
        void* rawDesc = *(void**)(buildingPtr[0] + 0x18);
        uintptr_t ordersBase2 = *(uintptr_t*)(buildingPtr[0] + 0x40);
        int typeIdx = *(int*)(ordersBase2 + orderOff + 4);

        // IsDerivedFrom check for TradeDescription — original does this each iteration
        bool isTradeDesc = fn_IsDerivedFrom(rawDesc, *g_TypeInfo_TradeDescription);
        void* tradeDesc = isTradeDesc ? rawDesc : nullptr;

        // Construct UI::Sprite via fn_Sprite_Ctor (NOT raw struct init)
        // fn_Sprite_Ctor(retbuf, spriteSheet_ExternalPtr, &spriteName_String)
        // Sprite name is at entriesBase + (typeIdx*3 + 1)*8
        uint64_t spriteTemp[2] = {0, 0};  // 16-byte retbuf for Sprite constructor
        uint64_t spriteSheet = *(uint64_t*)((uintptr_t)tradeDesc + 0x20);
        uintptr_t entriesBase = *(uintptr_t*)((uintptr_t)tradeDesc + 0x38);
        void* spriteName = (void*)(entriesBase + ((int64_t)typeIdx * 3 + 1) * 8);
        ::new (static_cast<void*>(spriteTemp)) UI::Sprite(::Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(spriteSheet)), *reinterpret_cast<const System::String*>(spriteName));  // B.3.6 bridge

        // Copy constructed sprite to local for SetSprite (matches MOVAPS/MOVDQA in original)
        uint64_t spriteLocal[2];
        spriteLocal[0] = spriteTemp[0];
        spriteLocal[1] = spriteTemp[1];

        fn_LabelElement_SetSprite(*(void**)(uiEntry + 0x10), spriteLocal);

        // Get buildTypeDesc and typeIdx AGAIN for string lookup (original re-reads)
        rawDesc = *(void**)(buildingPtr[0] + 0x18);
        uintptr_t ordersBase3 = *(uintptr_t*)(buildingPtr[0] + 0x40);
        typeIdx = *(int*)(ordersBase3 + orderOff + 4);

        isTradeDesc = fn_IsDerivedFrom(rawDesc, *g_TypeInfo_TradeDescription);
        tradeDesc = isTradeDesc ? rawDesc : nullptr;

        // Construct UI::String via fn_UIString_Ctor (NOT raw struct init)
        // fn_UIString_Ctor(retbuf, stringTable_ExternalPtr, &stringName_String)
        // String name is at entriesBase + (typeIdx*3 + 2)*8 = entriesBase + typeIdx*0x18 + 0x10
        uint64_t stringTemp[2] = {0, 0};  // 16-byte retbuf for UIString constructor
        uint64_t stringTable = *(uint64_t*)((uintptr_t)tradeDesc + 0x28);
        entriesBase = *(uintptr_t*)((uintptr_t)tradeDesc + 0x38);
        void* stringName = (void*)(entriesBase + (int64_t)typeIdx * 0x18 + 0x10);
        ::new (static_cast<void*>(stringTemp)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), *reinterpret_cast<const System::String*>(stringName));  // B.3.6 bridge

        // Copy constructed string to local for SetText (matches MOVAPS/MOVDQA in original)
        uint64_t stringLocal[2];
        stringLocal[0] = stringTemp[0];
        stringLocal[1] = stringTemp[1];

        fn_TextElement_SetText(*(void**)(uiEntry + 0x10), stringLocal);

        // Enable/disable logic:
        // i==0: disable uiEntry[0] (first up/down button)
        // i==orderCount-1: disable uiEntry[1] (second up/down button)
        if (idx == 0) {
            VCall_Element_SetEnabled(*(void**)(uiEntry), false);
        } else {
            if (iVar7 == *(int*)(buildingPtr[0] + 0x48) - 1) {
                VCall_Element_SetEnabled(*(void**)(uiEntry + 0x08), false);
            }
        }

        iVar7++;
    }
}

// ============================================================
// RefreshDisplay (FUN_1400846b0): Rebuild sorted storage table display
// ============================================================
// Original: 0x1400846b0 (336 lines)
//
// Iterates the global trade entity list (game world +0xF0), finds
// ResourceType and ValueDescription sub-objects on each entity,
// builds a display array (stride 0x18: {entity, resourceComp, valueDescComp}).
//
// Finds TradeComponent and StorageComponent on the owning building entity.
// Sorts entries by name (sort combo == 0) or by quantity (sort combo != 0).
//
// For each entry: sets sprite icon + name text on label element, looks up
// inventory count from StorageComponent's item array, looks up order amount
// from TradeComponent's BST, sets NumberElement and SpinnerElement values.
// Clears remaining UI rows beyond the data.

// Helper: find a sub-object on a description/external entity (array at +0x28, count at +0x30)
static void* FindDescSubObject(void* descEntity, uint32_t* typeIdPtr) {
    if (!descEntity || !typeIdPtr) return nullptr;
    uint32_t target = *typeIdPtr;
    void** compArray = *(void***)((uintptr_t)descEntity + 0x28);
    int compCount = *(int*)((uintptr_t)descEntity + 0x30);
    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if (!comp) continue;
        void** vtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int typeId = ((fn_GetTypeId_t)vtable[2])(comp);
        if ((uint32_t)typeId == target) {
            if (fn_IsDerivedFrom(comp, target))
                return comp;
            break;
        }
    }
    return nullptr;
}

void TradeUI_RefreshDisplay(TradeUI* self, uint64_t p2, uint64_t p3, int64_t p4) {
    uintptr_t s = (uintptr_t)self;

    // Get trade entity list from game world singleton (+0xF0)
    uintptr_t tradeList = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xF0);
    int entityCount = *(int*)(tradeList + 0x118);

    // Clear display entries array at self+0x50
    fn_DynArray_Clear_0x18((void*)(s + 0x50));

    // Build display entries: for each trade entity, find ResourceType + ValueDescription
    for (int i = 0; i < entityCount; i++) {
        uintptr_t entity = *(uintptr_t*)(*(uintptr_t*)(tradeList + 0x110) + (uintptr_t)i * 8);

        void* resourceComp = FindDescSubObject((void*)entity, g_TypeId_RawMaterialDescription);
        void* valueComp = FindDescSubObject((void*)entity, g_TypeInfo_ValueDescription);

        // Push 0x18-byte entry {entity, resourceComp, valueComp} into the display
        // array at +0x50.
        // B.3.2 bridge: TradeUI+0x50 is Collection::Array<{entity, resourceComp,
        // valueComp}> (stride 0x18, count +0x58, capacity +0x5C).
        auto& displayRows = GameColl::ArrayAt<GameColl::Bytes<0x18, 8>>(s, 0x50);
        displayRows.SetSize(displayRows.GetSize() + 1,
                             GameColl::Bytes<0x18, 8>(entity, (uint64_t)resourceComp,
                                            (uint64_t)valueComp));
    }

    // Get owning entity -> TradeComponent + StorageComponent
    void* ownerEntity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(ownerEntity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? ownerEntity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    new (&entityPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(compEntity));   // B.3.3 bridge

    // Find TradeComponent (uses g_TypeId_TradeComponent_FC = DAT_1401c4b80)
    uint64_t tradePtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &tradePtr);

    // Validate TradeDescription derivation (result unused — validation only)
    fn_IsDerivedFrom(*(void**)(tradePtr[0] + 0x18), *g_TypeInfo_TradeDescription);

    // Find StorageComponent
    uint64_t storagePtr[2] = {0, 0};
    fn_FindComponent_Storage((void*)entityPtr[0], &storagePtr);

    // Sort entries based on sort combo selection (+0x140 = sortCombo element)
    int entryCount = *(int*)(s + 0x58);
    int sortMode = *(int*)(*(uintptr_t*)(s + 0x140) + 0x118);

    if (sortMode == 0) {
        // Sort by name
        if (entryCount > 1) {
            uint64_t sortCtx = (uint64_t)tradeList;
            fn_IntroSort_ByName(*(void**)(s + 0x50), entryCount, &sortCtx, p4);
        }
    } else {
        // Sort by quantity (uses TradeComponent as context)
        uint64_t sortCtx = tradePtr[0];
        fn_IntroSort_ByComponent(*(void**)(s + 0x50), entryCount, &sortCtx);
    }

    // Re-read count (may have been modified)
    entryCount = *(int*)(s + 0x58);

    // --- Display loop: populate UI rows with data ---
    for (int i = 0; i < entryCount; i++) {
        int byteOff = i * 0x18;
        uintptr_t* entry = (uintptr_t*)(*(uintptr_t*)(s + 0x50) + byteOff);
        uintptr_t* uiRow = (uintptr_t*)(*(uintptr_t*)(s + 0x80) + byteOff);

        uintptr_t valueDesc = entry[2];  // ValueDescription component

        // Set sprite icon from ValueDescription (+0x20=spriteSheet, +0x28=spriteName)
        uint8_t spriteTmp[32] = {};
        ::new (static_cast<void*>(spriteTmp)) UI::Sprite(::Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(valueDesc + 0x20))), *reinterpret_cast<const System::String*>(valueDesc + 0x28));  // B.3.6 bridge
        uint64_t spriteVal[2] = {((uint64_t*)spriteTmp)[0], ((uint64_t*)spriteTmp)[1]};
        fn_LabelElement_SetSprite(*(void**)uiRow, spriteVal);

        // Set text name from ValueDescription (+0x30=stringTable, +0x38=stringName)
        // Original uses fn_UIString_Ctor — TextElement::SetText expects UI::String, not System::String
        uint64_t strBuf[2] = {0, 0};
        ::new (static_cast<void*>(strBuf)) UI::String(::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(valueDesc + 0x30))), *reinterpret_cast<const System::String*>(valueDesc + 0x38));  // B.3.6 bridge
        uint64_t strVal[2] = {strBuf[0], strBuf[1]};
        fn_TextElement_SetText(*(void**)uiRow, strVal);

        // Lookup inventory count from StorageComponent item array (+0x28/+0x30)
        // Each item is 0x10 bytes: {key(uint64), countA(int32), countB(int32)}
        // Result = countA - countB (available = total - committed)
        int invCount = 0;
        uintptr_t scAddr = storagePtr[0];
        int scItemCount = *(int*)(scAddr + 0x30);
        uintptr_t scItems = *(uintptr_t*)(scAddr + 0x28);
        for (int j = 0; j < scItemCount; j++) {
            uintptr_t itemAddr = scItems + (uintptr_t)j * 0x10;
            if (*(uintptr_t*)itemAddr == entry[0]) {
                if (j >= 0) {
                    invCount = *(int*)(itemAddr + 8) - *(int*)(itemAddr + 0xC);
                }
                break;
            }
        }

        // Set NumberElement value (at uiRow + 0x10)
        void* numSlot = (void*)((uintptr_t)uiRow + 0x10);
        fn_NumberElement_SetValueInt(*(void**)numSlot, invCount);

        // Lookup trade order amount from TradeComponent BST (red-black tree at +0x60)
        // Node: {key(+0), value(+8), color(+10), left(+18), right(+20)}
        int orderAmt = 0;
        uintptr_t* bstNode = *(uintptr_t**)(tradePtr[0] + 0x60);
        while (bstNode != nullptr) {
            if (entry[0] < *bstNode) {
                bstNode = (uintptr_t*)bstNode[3];  // left child at +0x18
            } else if (entry[0] > *bstNode) {
                bstNode = (uintptr_t*)bstNode[4];  // right child at +0x20
            } else {
                orderAmt = (int)bstNode[1];  // value at +0x08
                break;
            }
        }

        // Set SpinnerElement value (at uiRow + 0x08)
        void* spinSlot = (void*)((uintptr_t)uiRow + 8);
        fn_SpinnerElement_SetValue(*(void**)spinSlot, orderAmt);

        // Enable spinner and number elements via vtable[8] (+0x40) = SetEnabled
        Element_SetEnabled(*(void**)spinSlot, true);
        Element_SetEnabled(*(void**)numSlot, true);

        // spinSlot is storage entry[1] = SpinnerElement (confirmed via vtable at runtime)
        fn_SpinnerElement_SetMinMax(*(void**)spinSlot, 0, 9999);

        // Set spinner element name to resource path filename.
        // `GetPath` returns by value through a hidden return slot and the callee
        // CONSTRUCTS into it; copy-initialising the local is the same store.
        System::Filename pathBuf =
            reinterpret_cast<const ::Object::External*>(entry[0])->GetPath();  // B.3.3 bridge
        System::Filename filenameBuf = pathBuf.GetPathFilename(false);
        reinterpret_cast<UI::Element*>(*(void**)spinSlot)->SetName(*reinterpret_cast<const System::String*>(&filenameBuf));  // B.3.6 bridge
    }

    // --- Clear remaining UI rows beyond data ---
    int uiRowCount = *(int*)(s + 0x88);
    for (int i = entryCount; i < uiRowCount; i++) {
        int byteOff = i * 0x18;
        uintptr_t* uiRow = (uintptr_t*)(*(uintptr_t*)(s + 0x80) + byteOff);

        // Set blank sprite
        uint8_t blankSprite[16] = {};
        ::new (static_cast<void*>(blankSprite)) UI::Sprite();  // B.3.6 bridge
        uint64_t spriteVal[2] = {((uint64_t*)blankSprite)[0], ((uint64_t*)blankSprite)[1]};
        fn_LabelElement_SetSprite(*(void**)uiRow, spriteVal);

        // Set blank text — must use UI::String default ctor, not System::String
        uint64_t blankStr[2] = {0, 0};
        ::new (static_cast<void*>(blankStr)) UI::String();  // B.3.6 bridge
        uint64_t strVal[2] = {blankStr[0], blankStr[1]};
        fn_TextElement_SetText(*(void**)uiRow, strVal);

        // Disable spinner (at +8) via vtable SetEnabled
        void* spinSlot = (void*)((uintptr_t)uiRow + 8);
        Element_SetEnabled(*(void**)spinSlot, false);

        // Disable number (at +0x10) via vtable SetEnabled
        void* numSlot = (void*)((uintptr_t)uiRow + 0x10);
        Element_SetEnabled(*(void**)numSlot, false);

        // spinSlot is storage entry[1] = SpinnerElement (confirmed via vtable at runtime)
        fn_SpinnerElement_SetMinMax(*(void**)spinSlot, 0, 0);
    }
}

// ============================================================
// NOTE: this block used to defer three functions as "needs opus". All three have
// since landed, so the list is kept only to say where they went — a deferral list
// that outlives the deferral is exactly the kind of comment this project keeps
// getting bitten by.
//
//   fn_TradeUI_RefreshSellTable (0x140083c70) -> TradeUI_RefreshSellTable, above
//   fn_TradeUI_RefreshBuyTable  (0x140083100) -> TradeUI_RefreshBuyTable, above
//   fn_TradeUI_OnTradeButton    (0x140081420) -> code/Game/Dialog/tradeui_checkout.cpp
//
// The old description of the checkout said it "destroys/removes/spawns entities".
// That was read off two Ghidra mislabels and is wrong in the way that matters:
// 0x140091e60 and 0x140091a20 are the town AVAILABILITY-POOL registrars, not
// entity teardown. See the header block of tradeui_checkout.cpp.
// ============================================================
