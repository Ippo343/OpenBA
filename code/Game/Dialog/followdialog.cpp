// code/Game/Dialog/followdialog.cpp
// Clean reimplementation of FollowDialog game logic.
//
// FollowDialog (0xD8 bytes) — camera follow dialog.
// Extends DialogInstance (0xB8 base).
// +0xB8 = entity InternalBasePtr, +0xC0 = (unused here),
// +0xC8 = textFollow element, +0xD0 = textAction element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>

// Standalone profession-list data is not fully live (the CitizenAI +0x288 desc
// backref and the ProfessionList entries can be garbage/-1). Mirror the
// citizenui.cpp CitUI_OkPtr guard so Slot6's profession-text build can't deref a
// bogus pointer and hardfault (the Follow crash). Byte-faithful when these
// pointers are valid (as they are in the live game).
static inline bool FD_OkPtr(uintptr_t p) { return p >= 0x10000 && p < 0x7FFFFFFFFFF0ULL; }

// ============================================================
// WorldGrid_FindEntity (0x1400f9a60)
// ============================================================
// BST search by tile index. worldGrid+0x30 = root node ptr.
// Node: *node = key (int), *(node+6) = left ptr, *(node+8) = right ptr
// On match: copies *(node+2) into retval (InternalBasePtr).
static void* WorldGrid_FindEntity_Impl(void* worldGrid, void* retval, int tileIndex)
{
    int* node = *(int**)((uintptr_t)worldGrid + 0x30);
    while (true) {
        if (node == nullptr) {
            fn_InternalBasePtr_Ctor(retval, nullptr);
            return retval;
        }
        if (*node <= tileIndex) {
            if (tileIndex <= *node) {
                // Exact match
                if (node != nullptr) {
                    *(uint64_t*)retval = 0;
                    *(uint64_t*)retval = *(uint64_t*)(node + 2);
                    return retval;
                }
                fn_InternalBasePtr_Ctor(retval, nullptr);
                return retval;
            }
            node = *(int**)(node + 8);  // right child
        } else {
            node = *(int**)(node + 6);  // left child
        }
    }
}

// ============================================================
// FollowDialog::Slot4 (vtable +0x20) — Setup follow camera UI
// ============================================================
// Finds buttonClose (ID 100), textFollow (+0xC8), textAction (+0xD0).
// Sets textFollow parameter 0 to the followed entity's name.
// Sets follow-active flag on the game world subsystem at +0x120 → +0x38.
//
// Original: 0x140069340.

void FollowDialog_Slot4(FollowDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "buttonClose" and set element ID 100
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonClose");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        if (elem) {
            fn_Element_SetElementId(elem, 100);
        }
    }

    // Find "textFollow" → store at +0xC8, set parameter 0 to entity name
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textFollow");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        *(void**)(s + 0xC8) = elem;

        // Get entity at +0xB8, read its name at +0x28
        void* entity = *(void**)(s + 0xB8);
        if (entity && elem) {
            fn_TextElement_SetParameter(elem, 0, (uint8_t*)((uintptr_t)entity + 0x28));
        }
    }

    // Find "textAction" → store at +0xD0
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textAction");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        *(void**)(s + 0xD0) = elem;
    }

    // Set follow-active flag: *(gameWorld+0x120)+0x38 = 1
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    void* subsys = gameWorld ? *(void**)((uintptr_t)gameWorld + 0x120) : nullptr;
    if (subsys) *(uint8_t*)((uintptr_t)subsys + 0x38) = 1;
}

// ============================================================
// FollowDialog::Slot6 (vtable +0x30) — Draw: update profession and action text
// ============================================================
// Called every frame to refresh the follow dialog's text elements:
//   - textFollow (+0xC8): parameter 1 set to current profession description string
//   - textAction (+0xD0): text set to current action string by index
//
// Logic mirrors CitizenUI Slot7's profession/action text update block.
// The citizen entity's CitizenAI component is stored at +0xB8 (InternalBasePtr).
//
// Original: 0x140068F70.

void FollowDialog_Slot6(FollowDialog* self, void* /*p2*/, void* /*p3*/, void* /*p4*/) {
    uintptr_t s = (uintptr_t)self;

    // Get CitizenAI inner ptr from InternalBasePtr at +0xB8
    uintptr_t ai = *(uintptr_t*)(s + 0xB8);
    if (!ai) return;  // guard: followed-entity ref may be uninit/cleared

    // --- Profession text (parameter 1 on textFollow at +0xC8) ---
    // If isChild (age < childThreshold) OR hasWorkplace: show child/worker profession
    // Otherwise: show current profession description
    // isChild: guard the CitizenAI +0x288 desc backref (garbage/-1 in standalone).
    uintptr_t descBack = *(uintptr_t*)(ai + 0x288);
    bool isChild = FD_OkPtr(descBack) &&
                   (*(float*)(ai + 0x3C) < *(float*)(descBack + 0x44));
    bool hasWorkplace = (*(uintptr_t*)(ai + 0x208) != 0);

    {
        uintptr_t profEntry = 0;
        if (isChild || hasWorkplace) {
            // ProfessionList at gameWorld+0xB0 -> +0x40 (child) or +0x48 (worker)
            uintptr_t profListBase = *(uintptr_t*)(*(uintptr_t*)*g_pGameWorldSingleton + 0xB0);
            if (FD_OkPtr(profListBase)) {
                if (*(uintptr_t*)(ai + 0x208) == 0) {
                    profEntry = *(uintptr_t*)(profListBase + 0x40);  // child profession
                } else {
                    profEntry = *(uintptr_t*)(profListBase + 0x48);  // worker profession
                }
            }
        } else {
            profEntry = *(uintptr_t*)(ai + 0x1F0);  // current profession description
        }

        // Build UI::String from profession entry's string table (+0x20) and name key (+0x30)
        // Convert to System::String and set as parameter 1 on textFollow.
        // Guard: profEntry (and its +0x20 string-table ptr) can be garbage in standalone
        // where the profession-description data is not live — deref would hardfault.
        void* textFollow = *(void**)(s + 0xC8);
        if (FD_OkPtr(profEntry) && FD_OkPtr(*(uint64_t*)(profEntry + 0x20)) && textFollow) {
            uint8_t uiStr[16];
            fn_UIString_Ctor(uiStr, *(uint64_t*)(profEntry + 0x20), (void*)(profEntry + 0x30));
            uint8_t sysStr[8];
            fn_UIString_ToSystemString(uiStr, sysStr);

            fn_TextElement_SetParameter(textFollow, 1, sysStr);

            // Cleanup System::String
            fn_String_Dtor(sysStr);
        }
    }

    // --- Action text (textAction at +0xD0, set by index from element's string table) ---
    // Find ProfessionComponent on the entity (entity ref at ai+0x10)
    uintptr_t entityRef = *(uintptr_t*)(ai + 0x10);
    uint64_t profCompRef = 0;
    fn_FindComponent_CitizenAI((void*)entityRef, &profCompRef);

    // Read action index from CitizenAIComponent+0x28.
    // Guard: FindProfessionComponent yields null if the type-id is unresolved
    // (standalone) — deref of null+0x28 would fault.
    int actionIndex = profCompRef ? *(int*)((uintptr_t)profCompRef + 0x28) : 0;

    // Special case: action 9 (idle/searching for home)
    // Check if citizen's home tile entity has a different profession — if so, show index 0x10
    if (actionIndex == 9) {
        uintptr_t homeRef = 0;
        int homeTile = *(int*)(ai + 0x30);
        if (homeTile >= 0) {
            // Look up entity at home tile in world grid at gameWorld+0x118
            void* worldGrid = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x118);
            uint64_t foundRef = 0;
            WorldGrid_FindEntity_Impl(worldGrid, &foundRef, homeTile);
            homeRef = foundRef;
        }
        if (homeRef != 0) {
            // Get profession description from home entity via vtable[10] (offset +0x50)
            uintptr_t homeEntity = homeRef;
            typedef void* (*fn_VGetDesc_t)(void* self, void* retval);
            void** homeVtable = *(void***)(void*)homeEntity;
            fn_VGetDesc_t vGetDesc = (fn_VGetDesc_t)homeVtable[10];
            uint64_t descRef = 0;
            vGetDesc((void*)homeEntity, &descRef);
            uintptr_t profDesc = *(uintptr_t*)(ai + 0x1F0);
            if (*(uintptr_t*)&descRef != profDesc) {
                actionIndex = 0x10;  // profession mismatch — show "looking for work" string
            }
        }
    }

    // Build UI::String from textAction element's own string table at +0xD8, by index
    {
        void* textAction = *(void**)(s + 0xD0);
        if (!textAction) return;  // guard: element may be unresolved
        uintptr_t elemPtr = (uintptr_t)textAction;
        uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
        uint8_t uiStr[16];
        fn_UIString_Ctor_Index(uiStr, stringTable, actionIndex);
        uint8_t textVal[16];
        *(uint64_t*)textVal = *(uint64_t*)uiStr;
        *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        fn_TextElement_SetText(textAction, textVal);
    }
}

// ============================================================
// FollowDialog::Slot5 (vtable +0x28) — Shutdown: clear follow-active flag
// ============================================================
// Clears the follow-active byte at gameWorld->+0x120->+0x38.
// Mirror of Slot4 which sets it to 1; Slot5 clears it to 0 on shutdown.
//
// Original: 0x140069320 (19 bytes).

void FollowDialog_Slot5(FollowDialog* /*self*/) {
    void* gameWorld = *g_pGameWorldSingleton;
    void* subsys = *(void**)((uintptr_t)gameWorld + 0x120);
    *(uint8_t*)((uintptr_t)subsys + 0x38) = 0;
}

// ============================================================
// FollowDialog::Slot8 (vtable +0x40) — OnCommand: handle buttonClose click
// ============================================================
// When msg==0x12 (button click) and id==100 (buttonClose):
//   1. Validates InternalBasePtr at +0xC0 (the followed entity)
//   2. Gets inner ptr from +0xC0, nulls out inner+0xE0 (back-reference to dialog)
//   3. Wraps self in InternalBasePtr, calls UserInterface::CloseDialog.
//
// Original: 0x140069230 (18 bytes).

void FollowDialog_Slot8(FollowDialog* self, void* /*param2*/, int param3, int param4) {
    if (param3 != 0x12 || param4 != 100)
        return;

    uintptr_t s = (uintptr_t)self;

    // Validate and read the followed entity InternalBasePtr at +0xC0
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
    uintptr_t inner = *(uintptr_t*)(s + 0xC0);

    // Null out the back-reference: *(inner+0xE0) = null InternalBasePtr
    {
        uint8_t nullPtr[8] = {};
        fn_InternalBasePtr_Ctor(nullPtr, nullptr);
        *(uint64_t*)(inner + 0xE0) = *(uint64_t*)nullPtr;
    }

    // Close dialog: wrap self in InternalBasePtr, call CloseDialog
    {
        uint8_t selfIBP[8];
        fn_InternalBasePtr_Ctor(selfIBP, (void*)self);
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, selfIBP);
    }
}

// ============================================================
// FollowDialog::Slot7 (vtable +0x38) — Update: follow camera position
// ============================================================
// This is the DialogInstance per-frame self-tick. The Runtime framework calls
// it every frame from UI::DialogInstance::Update (0x180113980):
//     (**(code **)(*(longlong *)this + 0x38))(this);   // idx7 (+0x38) on the dialog
// which is driven by UI::UserInterface::Update (0x1801213C0) iterating each open
// dialog. So idx7 is the correct slot for the camera-follow tick (idx8/+0x40 is
// OnCommand, only invoked on element events with msg==0x12).
//
// Reads position from the followed entity's ModelComponent (found by exe type-id
// 0x1401c7a50 via fn_FindComponent_Character / FUN_140003f60) and writes it to the
// camera target. Checks for Escape key to close.
//
// Original: 0x140069250.

void FollowDialog_Slot7(FollowDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Get the followed entity from InternalBasePtr at +0xB8
    // CheckPtr validates the reference, then read entity+0x10 to get inner ref
    uintptr_t entityRef = *(uintptr_t*)(s + 0xB8);
    if (!entityRef) return;  // guard: followed-entity ref may be uninit/cleared
    uintptr_t entityInner = *(uintptr_t*)(entityRef + 0x10);

    // Find CharacterComponent on the entity
    uint8_t compBuf[16];
    fn_FindComponent_Character((void*)entityInner, compBuf);
    void* charComp = *(void**)compBuf;

    // Read model position: x at +0x158, z at +0x15C on the ModelComponent itself.
    // fn_FindComponent_Character writes the component pointer directly into the retbuf,
    // so charComp IS the ModelComponent — do NOT deref again (that would read the
    // component's vtable+0x158, garbage). Matches the exe (0x140069250: MOV RAX,[retbuf]
    // then MOVSS [RAX+0x158]) and Toolbar_CenterOnEntity_Impl (model = retbuf value,
    // read model+0x158). The previous *(uintptr_t*)charComp deref was the follow bug.
    // Only update the camera when the ModelComponent resolved (guard the null).
    if (charComp) {
        uintptr_t compData = (uintptr_t)charComp;
        uint32_t posX = *(uint32_t*)(compData + 0x158);
        uint32_t posZ = *(uint32_t*)(compData + 0x15C);

        // Write position to camera subsystem: gameWorld->+0xE0 -> {+0x1C, +0x20, +0x24}
        void* world = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
        uintptr_t cameraSys = world ? *(uintptr_t*)((uintptr_t)world + 0xE0) : 0;
        if (cameraSys) {
            *(uint32_t*)(cameraSys + 0x1C) = posX;
            *(uint32_t*)(cameraSys + 0x20) = posZ;
            *(uint32_t*)(cameraSys + 0x24) = 0;
        }
    }

    // Check Escape key (key ID 8) — if pressed, close the follow dialog.
    // FUN_140068f00: null back-ref on followed entity at +0xC0, then CloseDialog on self.
    void* keyMap = g_pKeyMapObjectExref ? ***(void****)g_pKeyMapObjectExref : nullptr;
    if (keyMap && fn_KeyMap_Pressed(keyMap, 0, 8)) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
        uintptr_t followedInner = *(uintptr_t*)(s + 0xC0);

        // Null out back-reference: *(followedInner+0xE0) = null InternalBasePtr
        {
            uint8_t nullPtr[8] = {};
            fn_InternalBasePtr_Ctor(nullPtr, nullptr);
            *(uint64_t*)(followedInner + 0xE0) = *(uint64_t*)nullPtr;
        }

        // Close dialog: wrap self, call CloseDialog
        {
            uint8_t selfIBP[8];
            fn_InternalBasePtr_Ctor(selfIBP, (void*)self);
            void* ui = *(void**)(s + 0x10);
            fn_UI_CloseDialog(ui, selfIBP);
        }
    }
}

// ============================================================
// STANDALONE EXE: real FollowDialog vtable
// ============================================================
// In standalone mode the exe .rdata vtable at 0x14015be00 is never populated, so
// g_FollowDialog_Vtable (engine_funcs) resolves to NULL. CitizenUI_Slot9 patches
// a freshly-constructed FollowDialog's vtable pointer with g_FollowDialog_Vtable
// → a NULL vtable → hard-fault the instant the UI dispatches the dialog. That is
// the Follow-button crash.
//
// This block builds a real 10-slot FollowDialog vtable mirroring the exe layout
// exactly (verified against 0x14015be00 in Ghidra):
//   +0x00 idx0  ScalarDtor          -> FD_SA_Dtor         (was 0x140011AF0)
//   +0x08 idx1  static getter        -> returns 0          (was 0x140011A10)
//   +0x10 idx2  GetTypeId            -> FD_SA_TypeId       (was 0x140011A00)
//   +0x18 idx3  Persist              -> no-op (serialize)  (was 0x14003A948)
//   +0x20 idx4  Setup                -> FollowDialog_Slot4 (was 0x140069340)
//   +0x28 idx5  Shutdown             -> FollowDialog_Slot5 (was 0x140069320)
//   +0x30 idx6  OnUpdateLayout       -> FollowDialog_Slot6 (was 0x140068F70)
//   +0x38 idx7  OnUpdate (per-frame) -> FollowDialog_Slot7 (was 0x140069250)
//   +0x40 idx8  OnCommand            -> FollowDialog_Slot8 (was 0x140069230)
//   +0x48 idx9  OnSerializeSettings  -> no-op (serialize)  (was 0x14003A954)
//
// The per-frame follow-camera tick is idx7 (+0x38) — the DialogInstance OnUpdate
// hook the UserInterface calls on each open dialog — NOT idx8. (idx8 is only
// invoked on element/button events with msg==0x12.)
//
// CitizenUI_Slot9 (standalone) falls back to FollowDialog_GetStandaloneVtable()
// when g_FollowDialog_Vtable is null. The FollowDialog REGISTER_TYPE entry (with
// its type id and dormant-safe factory) still lives in ui_dialog_stub_factories.cpp;
// this vtable is only bound to the manually-constructed instance.

#include "Object/factory_registration.h"

namespace {

void* GetFollowDialogVtable();  // fwd

// idx0 — scalar deleting destructor. Mirrors FollowDialog_ScalarDtor (0x140011AF0):
// reset vtable, run the DialogInstance base dtor, free the object if flag bit 0.
void* FD_SA_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = GetFollowDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// idx1 — small static getter in the exe (returns DAT_1401c4fe8). Not consulted on
// the follow path; return 0.
void* FD_SA_Slot1(void*) { return nullptr; }

// idx2 — GetTypeId.
uint32_t FD_SA_TypeId(void*) {
    static uint32_t cached = 0;
    if (!cached) cached = ::Object::GetTypeIdByName("FollowDialog");
    return cached;
}

// idx3 / idx9 — base DialogInstance serialization thunks. Never invoked for a live
// follow dialog; harmless no-op.
void FD_SA_SerializeStub(void*, ...) {}

// Typed reimpl trampolines wrapping the void*-based vtable ABI.
void FD_SA_Slot4(void* self) { FollowDialog_Slot4(reinterpret_cast<FollowDialog*>(self)); }
void FD_SA_Slot5(void* self) { FollowDialog_Slot5(reinterpret_cast<FollowDialog*>(self)); }
void FD_SA_Slot6(void* self, void* p2, void* p3, void* p4) {
    FollowDialog_Slot6(reinterpret_cast<FollowDialog*>(self), p2, p3, p4);
}
// Lazily bind the ModelComponent type-id fn_FindComponent_Character needs. The global
// g_TypeId_CharacterComponent is DAT_1401c7a50 = ModelComponent's TypeInfo (documented
// misnomer, engine_funcs.h:4476). Its raw exe .rdata slot is never registered in the
// standalone build (ResolveAddr -> 0), so the finder early-returns null and the follow
// camera never receives the citizen's ModelComponent+0x158 position -> the dialog ticks
// but the camera never tracks. Resolve it by name from the live factory tree (same fix
// pattern as statusiconcomponent.cpp). The value written by name equals what the
// exe .rdata holds.
void EnsureFollowGlobals() {
    static uint32_t s_tidModel = 0;
    uint32_t* g = g_TypeId_CharacterComponent;
    if (!g || *g == 0 || *g == 0xFFFFFFFFu) {
        if (!s_tidModel) s_tidModel = ::Object::GetTypeIdByName("ModelComponent");
        if (s_tidModel) g_TypeId_CharacterComponent = &s_tidModel;
    }
}

void FD_SA_Slot7(void* self) { EnsureFollowGlobals(); FollowDialog_Slot7(reinterpret_cast<FollowDialog*>(self)); }
void FD_SA_Slot8(void* self, void* e, int msg, int id) {
    FollowDialog_Slot8(reinterpret_cast<FollowDialog*>(self), e, msg, id);
}

void* g_FollowDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&FD_SA_Dtor),           // +0x00 idx0 ScalarDtor
    reinterpret_cast<void*>(&FD_SA_Slot1),          // +0x08 idx1 getter
    reinterpret_cast<void*>(&FD_SA_TypeId),         // +0x10 idx2 GetTypeId
    reinterpret_cast<void*>(&FD_SA_SerializeStub),  // +0x18 idx3 Persist
    reinterpret_cast<void*>(&FD_SA_Slot4),          // +0x20 idx4 Setup
    reinterpret_cast<void*>(&FD_SA_Slot5),          // +0x28 idx5 Shutdown
    reinterpret_cast<void*>(&FD_SA_Slot6),          // +0x30 idx6 OnUpdateLayout
    reinterpret_cast<void*>(&FD_SA_Slot7),          // +0x38 idx7 OnUpdate (per-frame)
    reinterpret_cast<void*>(&FD_SA_Slot8),          // +0x40 idx8 OnCommand
    reinterpret_cast<void*>(&FD_SA_SerializeStub),  // +0x48 idx9 OnSerializeSettings
};

void* GetFollowDialogVtable() {
    return reinterpret_cast<void*>(g_FollowDialog_Vtable_Standalone);
}

}  // namespace

extern "C" void* FollowDialog_GetStandaloneVtable() {
    return GetFollowDialogVtable();
}

