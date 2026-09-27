// code/Game/Dialog/followdialog.cpp
// Clean reimplementation of FollowDialog game logic.
//
// FollowDialog (0xD8 bytes) — camera follow dialog.
// Extends DialogInstance (0xB8 base).
// +0xB8 = entity InternalBasePtr, +0xC0 = (unused here),
// +0xC8 = textFollow element, +0xD0 = textAction element.
//
// Decompiled from Application-x64.exe.

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "input/Input.h"   // ENDGAME B.3.7 callers (K3)
#include "Dialog/FollowDialog.h"
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

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
            new (retval) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
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
                new (retval) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
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

void FollowDialog::OnOpened() {
    uintptr_t s = (uintptr_t)this;

    // Find "buttonClose" and set element ID 100
    {
        System::String nameStr("buttonClose");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;

        if (elem) {
            reinterpret_cast<UI::Element*>(elem)->SetElementId(100);  // B.3.6 bridge
        }
    }

    // Find "textFollow" → store at +0xC8, set parameter 0 to entity name
    {
        System::String nameStr("textFollow");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;

        *(void**)(s + 0xC8) = elem;

        // Get entity at +0xB8, read its name at +0x28
        void* entity = *(void**)(s + 0xB8);
        if (entity && elem) {
            reinterpret_cast<UI::TextElement*>(elem)->SetParameter(0, *reinterpret_cast<const System::String*>((uint8_t*)((uintptr_t)entity + 0x28)));  // B.3.6 bridge
        }
    }

    // Find "textAction" → store at +0xD0
    {
        System::String nameStr("textAction");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;

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

void FollowDialog::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

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
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(profEntry + 0x20))), *reinterpret_cast<const System::String*>((void*)(profEntry + 0x30)));  // B.3.6 bridge
            System::String sysStr;
            ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge

            reinterpret_cast<UI::TextElement*>(textFollow)->SetParameter(1, sysStr);  // B.3.6 bridge
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
        ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), actionIndex);  // B.3.6 bridge
        uint8_t textVal[16];
        *(uint64_t*)textVal = *(uint64_t*)uiStr;
        *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        reinterpret_cast<UI::TextElement*>(textAction)->SetText(reinterpret_cast<const UI::String*>(textVal));  // B.3.6 bridge
    }
}

// ============================================================
// FollowDialog::Slot5 (vtable +0x28) — Shutdown: clear follow-active flag
// ============================================================
// Clears the follow-active byte at gameWorld->+0x120->+0x38.
// Mirror of Slot4 which sets it to 1; Slot5 clears it to 0 on shutdown.
//
// Original: 0x140069320 (19 bytes).

void FollowDialog::OnClosed() {
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

void FollowDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t param3, int32_t param4, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (param3 != 0x12 || param4 != 100)
        return;

    uintptr_t s = (uintptr_t)this;

    // Validate and read the followed entity InternalBasePtr at +0xC0
    uintptr_t inner = *(uintptr_t*)(s + 0xC0);

    // Null out the back-reference: *(inner+0xE0) = null InternalBasePtr
    {
        uint8_t nullPtr[8] = {};
        new (nullPtr) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *(uint64_t*)(inner + 0xE0) = *(uint64_t*)nullPtr;
    }

    // Close dialog: wrap this in InternalBasePtr, call CloseDialog
    {
        uint8_t selfIBP[8];
        new (selfIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)this));   // B.3.3 bridge
        void* ui = *(void**)(s + 0x10);
        reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfIBP));  // B.3.6 bridge
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

namespace { void EnsureFollowGlobals(); }
void FollowDialog::OnUpdate(const System::Time& /*time*/) {
    EnsureFollowGlobals();
    uintptr_t s = (uintptr_t)this;

    // Get the followed entity from InternalBasePtr at +0xB8
    // CheckPtr validates the reference, then read entity+0x10 to get inner ref
    uintptr_t entityRef = *(uintptr_t*)(s + 0xB8);
    if (!entityRef) return;  // guard: followed-entity ref may be uninit/cleared
    uintptr_t entityInner = *(uintptr_t*)(entityRef + 0x10);

    // Find ModelComponent on the entity (via fn_FindComponent_Character)
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
    // FUN_140068f00: null back-ref on followed entity at +0xC0, then CloseDialog on this.
    void* keyMap = g_pInputConfigSingletonExref ? ***(void****)g_pInputConfigSingletonExref : nullptr;
    if (keyMap && reinterpret_cast<const Input::KeyMap*>(keyMap)->Pressed(0, 8)) {  // B.3.7 bridge
        uintptr_t followedInner = *(uintptr_t*)(s + 0xC0);

        // Null out back-reference: *(followedInner+0xE0) = null InternalBasePtr
        {
            uint8_t nullPtr[8] = {};
            new (nullPtr) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *(uint64_t*)(followedInner + 0xE0) = *(uint64_t*)nullPtr;
        }

        // Close dialog: wrap this, call CloseDialog
        {
            uint8_t selfIBP[8];
            new (selfIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>((void*)this));   // B.3.3 bridge
            void* ui = *(void**)(s + 0x10);
            reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfIBP));  // B.3.6 bridge
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
#include "Component/ModelComponent.h"   // X::TypeId() (B.3.8 callers)

// ENDGAME B.3.8 block DLG: FollowDialog is a real class (Dialog/FollowDialog.h);
// the hand-built table above is gone. Since the CB2 caller batch CitizenUI's
// Follow button (citizenui.cpp) builds the dialog with the real ctor below and
// fills +0xB8/+0xC0 itself; the door + stamp (and its FollowDialog_GetStandalone-
// Vtable getter) are gone. 0x14015be00 stays unregistered; engine_funcs.cpp's
// g_FollowDialog_Vtable has no reader left.

// The ModelComponent type id the follow camera's fn_FindComponent_Character needs.
// g_TypeId_ModelComponent_FC is DAT_1401c7a50 (documented misnomer,
// engine_funcs.h); its raw exe .rdata slot is never registered in the standalone
// build, so it is resolved by name from the live factory tree into namespace-scope
// storage (no function-local static, B6-5) on each tick until it holds a real id.
namespace {
uint32_t s_followModelTypeId = 0;
void EnsureFollowGlobals() {
    uint32_t* g = g_TypeId_ModelComponent_FC;
    if (!g || *g == 0 || *g == 0xFFFFFFFFu) {
        if (!s_followModelTypeId) s_followModelTypeId = ModelComponent::TypeId();
        if (s_followModelTypeId) g_TypeId_ModelComponent_FC = &s_followModelTypeId;
    }
}
}  // namespace

FollowDialog::FollowDialog() : dataB8(0), dataC0(0), dataC8(0), dataD0(0) {}

FollowDialog::FollowDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                           Object::InternalPtr<UI::DialogInstance> parent)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0),
      dataB8(0), dataC0(0), dataC8(0), dataD0(0) {}

// [0] 0x140011af0: ~FollowDialog -> ~DialogInstance -> ~Internal, MemFree on &1.
Object::Base* FollowDialog::DeletingDtor(unsigned flags) {
    this->~FollowDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140011a20: MemAlloc(0xd8, 8), then the ctor.
void* FollowDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FollowDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) FollowDialog());
}
// createInPlace 0x140011a90 (the tree's in-place build is kept, B8-13).
void* FollowDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) FollowDialog());
}

// The reflection record stays at the TU position of the UI_DIALOG_STUB row it
// replaces (ui_dialog_stub_factories.cpp, B38 D4).
DEFINE_EXE_VTABLE(FollowDialog, "_ZTV12FollowDialog")

