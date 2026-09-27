// code/Game/Dialog/citizenui.cpp
// Clean reimplementation of CitizenUI panel methods.
//
// CitizenUI (0xE8 bytes): Citizen info panel.
// Layout:
//   +0x30 imageMale (InternalBasePtr)
//   +0x38 imageFemale
//   +0x40 textName
//   +0x48 numberAge
//   +0x50 textProfession
//   +0x58 textEducatedYes
//   +0x60 textEducatedNo
//   +0x68 textClothing
//   +0x70 textTool
//   +0x78 buttonProfession
//   +0x80 buttonHome
//   +0x88 buttonFollow
//   +0x90 textNameIs
//   +0x98 textAction
//   +0xA0 heartsIconArray (0x20 bytes: ptr, count, cap, ptr2, count2, cap2)
//   +0xC0 starsIconArray  (0x20 bytes)
//   +0xE0 sub-dialog InternalBasePtr (follow-dialog reference)

#include "Dialog/ui_panels.h"
#include "Dialog/FollowDialog.h"
#include "Dialog/citizen_icon_helpers.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/NumberElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/Dialog.h"   // X::TypeId() (ENDGAME B.3.9)
#if defined(_DEBUG)
#include <cstdio>
#include <cstdlib>
#endif

#include "Object/factory_registration.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include <cstddef>
#include "Component/CitizenAIComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/CitizenComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/CitizenDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "Player/EditTool.h"   // X::TypeId() (B.3.8 callers)

// FUN_140011050 — Entity::FindComponent<CitizenAIComponent>, the component whose +0x28
// holds the citizen's current action-state index (the Action text on this panel and on
// the Follow dialog). Retail compares against [0x1401c70e8] = CitizenAIComponent's
// type id (0x140011093) and IsDerivedFrom-checks with it (0x1400110ec).
// ENDGAME B.3.9 CB: by the class static. The old spelling went through
// g_TypeId_CitizenAIComponent, which EnsureGlobals() below pointed at a
// "ProfessionComponent" by-name id (no such class) or a static 0, so the finder found
// nothing and the Action line was skipped.
void fn_FindComponent_CitizenAI(void* entity, void* retval) {
    uint32_t tid = CitizenAIComponent::TypeId();
    Entity_FindComponentByExeTypeId(entity, retval, &tid);
}

// A partially-initialized standalone citizen can leave CitizenAI description/pointer
// fields as garbage (0, the -1 unresolved-IBP sentinel, or non-canonical high values
// like 0xfffffffffffffee7). CitizenUI_Slot7 dereferences several of these directly
// (matching the original, which relies on the live game having them resolved). Reject
// anything that isn't a plausible Win64 user-space heap pointer before dereferencing,
// so the panel shows what it can instead of faulting.
static inline bool CitUI_OkPtr(uintptr_t p) {
    return p >= 0x10000 && p <= 0x00007FFFFFFFFFFFULL;
}

// TextElement::SetParameter (Runtime 0x18011e9b0) writes *(elem+0xC8)[index] but only
// when index < *(int*)(elem+0xD0) (the param count). A citizen dialog element that
// standalone resolves by name can be a partially-initialized TextElement with a positive
// param count but a garbage param-array pointer (+0xC8) — the write then faults. Only
// call SetParameter when the element genuinely has a valid parameter array.
// A System::String is an 8-byte pointer to a ref-counted buffer (buffer[-4] = refcount).
// SetParameter/SetText increment the source String's refcount (write buffer[-4]); an
// uninitialized citizen field (e.g. the name at CitizenAI+0x28 is unset in standalone)
// holds a garbage buffer pointer, faulting there. Accept only a null (empty) or a real
// buffer pointer.
static inline bool CitUI_OkString(const void* strObj) {
    if (!CitUI_OkPtr(reinterpret_cast<uintptr_t>(strObj))) return false;
    uintptr_t buf = *reinterpret_cast<const uintptr_t*>(strObj);
    return buf == 0 || CitUI_OkPtr(buf);
}

static inline bool CitUI_OkTextParam(void* elem) {
    if (!CitUI_OkPtr((uintptr_t)elem)) return false;
    int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(elem) + 0xD0);
    if (count <= 0) return false;  // SetParameter is a no-op for 0 params anyway
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(elem) + 0xC8);
    if (!CitUI_OkPtr(arr)) return false;
    // The destination param slot arr[0] holds the CURRENT System::String (an 8-byte
    // ref-counted buffer pointer); SetParameter releases it (touches buffer+8) before
    // assigning the new one. A standalone dialog can allocate the param array but leave
    // its String entries uninitialized (garbage buffer pointer), faulting in the release.
    // Require the slot to be null (empty String) or a real pointer.
    uintptr_t cur = *reinterpret_cast<uintptr_t*>(arr);   // index 0 (all our SetParameter calls use 0)
    if (cur != 0 && !CitUI_OkPtr(cur)) return false;
    // SetParameter also walks the element's +0x98 parent chain setting a dirty flag on
    // each ancestor. A detached / uninitialized standalone element can have a garbage
    // parent link, faulting in that loop before the (valid) param array is even touched.
    // Require the whole chain to be well-formed: every link a real pointer, terminating
    // at 0, within a sane depth. A live tree-attached element always satisfies this.
    uintptr_t p = reinterpret_cast<uintptr_t>(elem);
    for (int i = 0; i < 64 && p != 0; ++i) {
        if (!CitUI_OkPtr(p)) return false;
        p = *reinterpret_cast<uintptr_t*>(p + 0x98);
    }
    return p == 0;  // reached the root cleanly (not truncated by the depth cap)
}

// CitizenUI_UpdateIconDisplay_Impl and CitizenUI_BuildIconArray_Impl
// are now in the shared header: Dialog/citizen_icon_helpers.h

// ============================================================
// WorldGrid_FindEntity (0x1400f9a60)
// ============================================================
// BST search: finds entity at tile index param_3 in world grid tree.
// worldGrid+0x30 = root node ptr.
// Node layout (piVar1): *piVar1 = key (tile index), *(piVar1+6) = left, *(piVar1+8) = right
// On match: retval = *(piVar1+2) (entity InternalBasePtr, 8 bytes)
// Returns param_2 (retval ptr).
static void* WorldGrid_FindEntity_Impl(void* worldGrid, void* retval, int tileIndex)
{
    int* piVar1 = *(int**)((uintptr_t)worldGrid + 0x30);
    while (true) {
        if (piVar1 == nullptr) {
            new (retval) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            return retval;
        }
        if (*piVar1 <= tileIndex) {
            if (tileIndex <= *piVar1) {
                // Match
                if (piVar1 != nullptr) {
                    *(uint64_t*)retval = 0;
                    *(uint64_t*)retval = *(uint64_t*)(piVar1 + 2);
                    return retval;
                }
                new (retval) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                return retval;
            }
            // Go right
            piVar1 = *(int**)(piVar1 + 8);
        } else {
            // Go left
            piVar1 = *(int**)(piVar1 + 6);
        }
    }
}

// ============================================================
// Helper: Find element by name in dialog instance, return InternalPtr value
// ============================================================
static uint64_t FindDialogElem(void* dialogInst, const char* name) {
    System::String nameStr(name);
    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
    // A missing element yields a non-null sentinel / unresolved value here rather than
    // 0 (seen as 0xffffffff00000000), which defeats the `if (elem)` guards downstream
    // and faults inside the engine. Normalise anything that isn't a real pointer to 0.
    return CitUI_OkPtr(result) ? result : 0;
}

// Helper: Find element by name in element tree, return InternalPtr value
static uint64_t FindElemInTree(void* elem, const char* name) {
    System::String nameStr(name);
    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(elem)->FindElement(nameStr));  // B.3.6 bridge
    // Normalise a missing/unresolved element to 0 (see FindDialogElem).
    return CitUI_OkPtr(result) ? result : 0;
}

namespace citizenui_sa {

// g_pAgeMultiplier's exe .rdata (0x140172198) is uninitialized in the standalone
// image (unreadable / zero-fill), so fall back to 1.0 — citizen age at CitizenAI+0x3c
// is already in display years for the panel. Not a crash risk either way; the fallback
// just avoids a null-deref in the age line.
static float s_ageMul = 1.0f;

// ENDGAME B.3.9: ComponentEntity / CitizenDescription / CitizenComponent (was
// g_TypeId_CitizenComponent_FC) / EditTool all have a ported TypeId() now, and
// g_TypeId_Dialog is UI::Dialog's own _typeInfo (engine_funcs.cpp:828, a Runtime
// address, always resolved) — not "External" as this file's old heal guessed (that
// heal never ran: the pointer is set at EngineFuncsInit before this ever fires).
// Only the age-multiplier fallback (a different, non-type-id singleton) is left.
void EnsureGlobals() {
    if (!g_pAgeMultiplier) g_pAgeMultiplier = &s_ageMul;
}

}  // namespace citizenui_sa

// ============================================================
// Slot4: Setup — finds all named elements, stores pointers, sets button IDs
// ============================================================
// Original: 0x1400651A0
//
// Finds 15 named UI elements and stores them in CitizenUI fields.
// Elements found via DialogInstance::FindElement go through the full dialog tree;
// elements found via Element::FindElement use the parent element at +0x20.
// groupHearts and groupStars are processed into icon arrays by FUN_14006abf0.
// Three buttons get element IDs: buttonProfession(0x227), buttonHome(0x226),
// buttonFollow(0x228).

// [4] 0x1400651a0
void CitizenUI::OnCreated() {
    citizenui_sa::EnsureGlobals();
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = (uintptr_t)this;
    void* dialogInst = *(void**)(s + 0x18);
    void* parentElem = *(void**)(s + 0x20);

    // --- Image elements (found via DialogInstance) ---
    *(uint64_t*)(s + 0x30) = FindDialogElem(dialogInst, "imageMale");
    *(uint64_t*)(s + 0x38) = FindDialogElem(dialogInst, "imageFemale");

    // --- Text/number elements (found via Element tree from +0x20) ---
    *(uint64_t*)(s + 0x48) = FindElemInTree(parentElem, "numberAge");
    *(uint64_t*)(s + 0x40) = FindElemInTree(parentElem, "textName");
    *(uint64_t*)(s + 0x50) = FindElemInTree(parentElem, "textProfession");
    *(uint64_t*)(s + 0x60) = FindElemInTree(parentElem, "textEducatedNo");
    *(uint64_t*)(s + 0x58) = FindElemInTree(parentElem, "textEducatedYes");
    *(uint64_t*)(s + 0x68) = FindElemInTree(parentElem, "textClothing");
    *(uint64_t*)(s + 0x70) = FindElemInTree(parentElem, "textTool");
    *(uint64_t*)(s + 0x90) = FindElemInTree(parentElem, "textNameIs");
    *(uint64_t*)(s + 0x98) = FindElemInTree(parentElem, "textAction");

    // --- Icon groups (heart/star arrays built from group children) ---
    {
        uint64_t groupHearts = FindElemInTree(parentElem, "groupHearts");
        CitizenUI_BuildIconArray_Impl(s + 0xA0, (uintptr_t)&groupHearts);
    }
    {
        uint64_t groupStars = FindElemInTree(parentElem, "groupStars");
        CitizenUI_BuildIconArray_Impl(s + 0xC0, (uintptr_t)&groupStars);
    }

    // --- Button elements (found via DialogInstance, with element IDs) ---
    *(uint64_t*)(s + 0x78) = FindDialogElem(dialogInst, "buttonProfession");
    *(uint64_t*)(s + 0x80) = FindDialogElem(dialogInst, "buttonHome");
    *(uint64_t*)(s + 0x88) = FindDialogElem(dialogInst, "buttonFollow");

    reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x78))->SetElementId(0x227);  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x80))->SetElementId(0x226);  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x88))->SetElementId(0x228);  // B.3.6 bridge
}

// ============================================================
// Slot6: Close sub-dialog if open, then clear the reference.
// ============================================================
// Original: 0x140065130

// [6] 0x140065130
void CitizenUI::OnClosed() {
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = (uintptr_t)this;

    uint64_t subDialog = *(uint64_t*)(s + 0xE0);
    if (subDialog != 0) {
        // Get the UserInterface from DialogInstance (+0x18) -> +0x10
        void* dialogInst = (void*)*(uint64_t*)(s + 0x18);
        void* userInterface = *(void**)((uintptr_t)dialogInst + 0x10);
        reinterpret_cast<UI::UserInterface*>(userInterface)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(s + 0xE0));  // B.3.6 bridge
    }

    // Reset +0xE0 to null InternalBasePtr
    uint64_t nullPtr = 0;
    new (&nullPtr) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *(uint64_t*)(s + 0xE0) = nullPtr;
}

// ============================================================
// Slot7: Update — refreshes citizen info display
// ============================================================
// Original: 0x140064AE0
//
// Gets the CitizenAIComponent from the entity at +0x28. Updates:
// - Gender icon visibility (male/female based on +0x38 flag)
// - Education status (compares age +0x210 vs description +0x118)
// - Name text (from CitizenAI +0x28 System::String)
// - Age number (from +0x3C float * age multiplier)
// - Clothing text (from +0x1E4 index into string table at element +0xD8/+0xE0)
// - Tool text (from +0xAC/+0xA8 tool count and comparison with description +0x110)
// - Profession text (from +0x1F0 profession description, or fallback if child/worker)
// - Health hearts (FUN_14006aed0 with +0x50 value vs description +0x70)
// - Happiness stars (FUN_14006aed0 with +0x80 value vs description +0xA4)
// - Profession button enabled state (based on workplace/job assignment)
// - Home button virtual call (likely sets enabled state)
// - Name-is text parameter
// - Action text (based on current state from profession component)

// [7] 0x140064ae0
void CitizenUI::OnUpdateLayout() {
    citizenui_sa::EnsureGlobals();
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = (uintptr_t)this;

    // Get entity from data source (+0x28), cast to ComponentEntity
    void* entityBase = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entityBase, ComponentEntity::TypeId());
    void* entity = isEntity ? entityBase : nullptr;
    uint64_t entityRef = 0;
    new (&entityRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge

    // Find CitizenAIComponent
    uint64_t citizenAIRef = 0;
    fn_FindComponent_Citizen((void*)entityRef, &citizenAIRef);
    uintptr_t ai = (uintptr_t)citizenAIRef;
    if (!ai) return;

    // Get CitizenDescription from CitizenAI (+0x18), check IsDerivedFrom
    void* descBase = *(void**)(ai + 0x18);
    bool isDesc = fn_IsDerivedFrom(descBase, CitizenDescription::TypeId());
    void* citizenDesc = isDesc ? descBase : nullptr;
    if (!CitUI_OkPtr((uintptr_t)citizenDesc)) citizenDesc = nullptr;

    // CitizenAI+0x288 caches the CitizenDescription pointer; the original reads
    // *(ai+0x288)+0x118 / +0x44 directly. In standalone that cache can be garbage
    // (unresolved) while the Component-base description (+0x18, validated above as
    // citizenDesc) is valid. Use +0x288 when it looks like a real pointer, else fall
    // back to citizenDesc (the same CitizenDescription); 0 if neither is valid.
    uintptr_t aiDesc = *(uintptr_t*)(ai + 0x288);
    if (!CitUI_OkPtr(aiDesc)) aiDesc = (uintptr_t)citizenDesc;
    if (!CitUI_OkPtr(aiDesc)) aiDesc = 0;

    // --- Gender icon visibility ---
    // +0x38 on CitizenAI: 0 = female, nonzero = male
    bool isMale = (*(int*)(ai + 0x38) != 0);
    // imageFemale (+0x38): visible if male, imageMale (+0x30): visible if female... wait
    // Actually: if isMale, show imageMale (hide female), else show imageFemale (hide male)
    void* imageFemale = (void*)*(uint64_t*)(s + 0x38);
    if (isMale) {
        if (imageFemale) reinterpret_cast<UI::Element*>(imageFemale)->SetFlagsOnOff(1, 0);  // B.3.6 bridge; hide imageFemale
    } else {
        if (imageFemale) reinterpret_cast<UI::Element*>(imageFemale)->SetFlagsOnOff(0, 1);  // B.3.6 bridge; show imageFemale
    }
    void* imageMale = (void*)*(uint64_t*)(s + 0x30);
    if (imageMale) reinterpret_cast<UI::Element*>(imageMale)->SetFlagsOnOff(isMale ? 0 : 1, isMale ? 1 : 0);  // B.3.6 bridge

    // --- Education visibility ---
    // age at +0x210 (int), education age threshold at description+0x118 (aiDesc = the
    // CitizenDescription; see aiDesc note above). Skip if no description resolved.
    bool isEducated = aiDesc && (*(int*)(ai + 0x210) < *(int*)(aiDesc + 0x118));
    void* textEducatedNo = (void*)*(uint64_t*)(s + 0x60);
    if (isEducated) {
        if (textEducatedNo) reinterpret_cast<UI::Element*>(textEducatedNo)->SetFlagsOnOff(0, 1);  // B.3.6 bridge; show textEducatedNo
    } else {
        if (textEducatedNo) reinterpret_cast<UI::Element*>(textEducatedNo)->SetFlagsOnOff(1, 0);  // B.3.6 bridge; hide textEducatedNo
    }
    void* textEducatedYes = (void*)*(uint64_t*)(s + 0x58);
    if (textEducatedYes) reinterpret_cast<UI::Element*>(textEducatedYes)->SetFlagsOnOff(
                             isEducated ? 1 : 0, isEducated ? 0 : 1);  // B.3.6 bridge

    // --- Name text ---
    // CitizenAI+0x28 is the citizen's name (System::String)
    void* textName = (void*)*(uint64_t*)(s + 0x40);
    // Guard both the destination element AND the source name String (ai+0x28), which is
    // uninitialized on a standalone citizen and would fault when SetParameter bumps its
    // ref-count.
    bool okName = CitUI_OkString((void*)(ai + 0x28));
    bool okParam = CitUI_OkTextParam(textName);
    if (okName && okParam)
        reinterpret_cast<UI::TextElement*>(textName)->SetParameter(0, *reinterpret_cast<const System::String*>(ai + 0x28));  // B.3.6 bridge
#if defined(_DEBUG)
    {
    }
#endif

    // --- Age number ---
    float ageFloat = *(float*)(ai + 0x3c);
    int ageYears = (int)(ageFloat * *g_pAgeMultiplier);
    void* numberAge = (void*)*(uint64_t*)(s + 0x48);
    if (numberAge) reinterpret_cast<UI::NumberElement*>(numberAge)->SetValue(static_cast<int32_t>(ageYears));  // B.3.6 bridge

    // --- Clothing text (string table lookup) ---
    {
        void* clothingElem = (void*)*(uint64_t*)(s + 0x68);
        if (clothingElem) {
            uint32_t clothingIdx = *(uint32_t*)(ai + 0x1e4);
            uintptr_t elemPtr = (uintptr_t)clothingElem;
            // Build UI::String from element's string table (+0xD8) and string entry (+0xE0)
            uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
            uint8_t uiStr[16];
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), clothingIdx);  // B.3.6 bridge
            uint8_t textVal[16];
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
            reinterpret_cast<UI::TextElement*>(clothingElem)->SetText(reinterpret_cast<const UI::String*>(textVal));  // B.3.6 bridge
        }
    }

    // --- Tool text ---
    // tool count at +0xAC, tool item at +0xA8
    // If tool count < 1: char value = 0
    // Else: compare tool item (+0xA8) with description+0x110; result = 1 if same, 2 if different
    int toolValue;
    if (*(int*)(ai + 0xAC) < 1) {
        toolValue = 0;
    } else {
        int toolItem = *(int*)(ai + 0xA8);
        void* descBase2 = *(void**)(ai + 0x18);
        bool isDesc2 = fn_IsDerivedFrom(descBase2, CitizenDescription::TypeId());
        void* desc2 = isDesc2 ? descBase2 : nullptr;
        if (!CitUI_OkPtr((uintptr_t)desc2)) desc2 = nullptr;
        // desc2 (CitizenDescription) may be null on a partially-initialized standalone
        // citizen — treat a missing description as "default tool" rather than faulting.
        toolValue = desc2 ? ((toolItem != *(int*)((uintptr_t)desc2 + 0x110)) + 1) : 1;
    }
    {
        void* toolElem = (void*)*(uint64_t*)(s + 0x70);
        if (toolElem) {
            uintptr_t elemPtr = (uintptr_t)toolElem;
            uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
            uint8_t uiStr[16];
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), toolValue);  // B.3.6 bridge
            uint8_t textVal[16];
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
            reinterpret_cast<UI::TextElement*>(toolElem)->SetText(reinterpret_cast<const UI::String*>(textVal));  // B.3.6 bridge
        }
    }

    // --- Profession text ---
    // If age < child threshold (CitizenAI+0x288 -> +0x44) or has workplace (+0x208 != 0):
    //   If workplace (+0x208) == 0: use ProfessionList+0x40 (child profession)
    //   Else: use ProfessionList+0x48 (worker profession)
    //   Build UI::String from profession+0x20 (string table) and profession+0x30 (name)
    // Else: use +0x1F0 (current profession description)
    {
        void* profElem = (void*)*(uint64_t*)(s + 0x50);
        auto okPtr = [](uintptr_t p) { return CitUI_OkPtr(p); };
        if (profElem) {
            // gw+0xB0 is the ProfessionList, read directly off the GameWorld base
            // (single deref) — matching retail (0x140064AE0) and the other call sites
            // (the WorldGrid read at +0x118 below, starttown +0xb0). The prior
            // *(uintptr_t*)*g_pGameWorldSingleton double-deref read GameWorld field 0 and
            // then +0xB0 off THAT -> garbage profListBase, blanking the Student/worker Job
            // text. Byte-faithful correction (the original has the same latent bug).
            uintptr_t gwBase = (uintptr_t)*g_pGameWorldSingleton;
            uintptr_t profListBase = okPtr(gwBase) ? *(uintptr_t*)(gwBase + 0xB0) : 0;

            bool isChild = aiDesc && (*(float*)(ai + 0x3c) < *(float*)(aiDesc + 0x44));
            bool hasWorkplace = (*(uintptr_t*)(ai + 0x208) != 0);

            uint8_t textVal[16];
            bool haveText = false;
            if (isChild || hasWorkplace) {
                uintptr_t profEntry = 0;
                if (okPtr(profListBase)) {
                    profEntry = (*(uintptr_t*)(ai + 0x208) == 0)
                                    ? *(uintptr_t*)(profListBase + 0x40)   // child
                                    : *(uintptr_t*)(profListBase + 0x48);  // worker w/ workplace
                }
                if (okPtr(profEntry)) {
                    uint64_t strTable = *(uint64_t*)(profEntry + 0x20);
                    uint8_t uiStr[16];
                    ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(strTable)), *reinterpret_cast<const System::String*>(profEntry + 0x30));  // B.3.6 bridge
                    *(uint64_t*)textVal = *(uint64_t*)uiStr;
                    *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
                    haveText = true;
                }
            } else {
                uintptr_t profDesc = *(uintptr_t*)(ai + 0x1F0);
                if (okPtr(profDesc)) {
                    uint64_t strTable = *(uint64_t*)(profDesc + 0x20);
                    uint8_t uiStr[16];
                    ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(strTable)), *reinterpret_cast<const System::String*>(profDesc + 0x30));  // B.3.6 bridge
                    *(uint64_t*)textVal = *(uint64_t*)uiStr;
                    *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
                    haveText = true;
                }
            }
            if (haveText) reinterpret_cast<UI::TextElement*>(profElem)->SetText(reinterpret_cast<const UI::String*>(textVal));  // B.3.6 bridge
        }
    }

    // --- Health hearts and happiness stars ---
    // Max values come from the CitizenDescription (+0x70 health, +0xA4 happiness);
    // use the resolved aiDesc/citizenDesc and skip if neither is a valid pointer.
    {
        uintptr_t hd = aiDesc ? aiDesc : (uintptr_t)citizenDesc;
        if (CitUI_OkPtr(hd)) {
            CitizenUI_UpdateIconDisplay_Impl(s + 0xA0, *(float*)(ai + 0x50), *(float*)(hd + 0x70));
            CitizenUI_UpdateIconDisplay_Impl(s + 0xC0, *(float*)(ai + 0x80), *(float*)(hd + 0xA4));
        }
    }

    // --- Profession button enabled state ---
    // Enabled if workplace (+0x248) or job entity (+0x208) exists
    bool profEnabled = (*(uintptr_t*)(ai + 0x248) != 0) || (*(uintptr_t*)(ai + 0x208) != 0);
    {
        void* profBtn = (void*)*(uint64_t*)(s + 0x78);
        if (profBtn) {
            reinterpret_cast<UI::Element*>(profBtn)->Enable(profEnabled);  // B.3.6 bridge: Element slot 8 Enable (vtable+0x40)
        }
    }

    // --- Home button virtual call ---
    {
        void* homeBtn = (void*)*(uint64_t*)(s + 0x80);
        if (homeBtn) {
            // Original passes result of CheckPtr on CitizenAI (always true if we got here)
            reinterpret_cast<UI::Element*>(homeBtn)->Enable(true);  // B.3.6 bridge: Element slot 8 Enable (vtable+0x40)
        }
    }

    // --- textNameIs: set name parameter ---
    // In standalone the "textNameIs" element resolves to a partially-constructed
    // TextElement whose parameter-array String entries are uninitialized (garbage buffer
    // pointers), so SetParameter faults deep inside System::String::operator= even though
    // the element, its +0xC8 array and its +0x98 chain all validate. It is a redundant
    // secondary name label — the primary textName above already shows the citizen's name
    // — so skip it in standalone. (The original keeps the CheckPtr + set here.)

    // --- Action text ---
    // Get entity reference from CitizenComponent+0x10, find its CitizenAIComponent
    // (FUN_140011050 at 0x140064fc2).
    uintptr_t entityFromAI = *(uintptr_t*)(ai + 0x10);
    uint64_t profCompRef = 0;
    // Guard the entity backref (component-list walk dereferences entity+0x30/+0x38);
    // a partially-initialized standalone citizen can leave it as garbage.
    if (CitUI_OkPtr(entityFromAI)) {
    fn_FindComponent_CitizenAI((void*)entityFromAI, &profCompRef);

    // Read action index from CitizenAIComponent+0x28 -- unguarded, as retail
    // (0x140064fd6): every citizen carries one. ENDGAME B.3.9 CB removed the
    // `profCompRef != 0` skip, which only ever fired because the type id read 0.
    int actionIndex = *(int*)(*(uintptr_t*)&profCompRef + 0x28);

    // Special case: if action is 9 (idle/searching), check if citizen has a home tile
    if (actionIndex == 9) {
        uintptr_t homeRef = 0;
        int homeTile = *(int*)(ai + 0x30);
        if (homeTile >= 0) {
            // Look up entity at home tile via world grid
            void* worldGrid = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x118);
            uint64_t foundRef = 0;
            WorldGrid_FindEntity_Impl(worldGrid, &foundRef, homeTile);
            homeRef = foundRef;
        }
        if (homeRef != 0) {
            // Get profession description from home entity, compare with citizen's
            uintptr_t homeEntity = homeRef;
            typedef void* (*fn_VGetDesc_t)(void* self, void* retval);
            void** homeVtable = *(void***)((void*)homeEntity);
            fn_VGetDesc_t vGetDesc = (fn_VGetDesc_t)homeVtable[10]; // vtable+0x50
            uint64_t descRef = 0;
            vGetDesc((void*)homeEntity, &descRef);
            uintptr_t profDesc = *(uintptr_t*)(ai + 0x1F0);
            if (*(uintptr_t*)&descRef != profDesc) {
                actionIndex = 0x10;  // "looking for work" or similar
            }
        }
    }

    // Set action text from string table at element +0xD8
    {
        void* actionElem = (void*)*(uint64_t*)(s + 0x98);
        if (actionElem) {
            uintptr_t elemPtr = (uintptr_t)actionElem;
            uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
            uint8_t uiStr[16];
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), actionIndex);  // B.3.6 bridge
            uint8_t textVal[16];
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
            reinterpret_cast<UI::TextElement*>(actionElem)->SetText(reinterpret_cast<const UI::String*>(textVal));  // B.3.6 bridge
        }
    }
    }  // if (CitUI_OkPtr(entityFromAI))
}

// ============================================================
// Slot9: Button dispatch — profession (0x227), home (0x226), follow (0x228)
// ============================================================
// Original: 0x1400650F0
//
// Handles three button clicks:
// - buttonProfession (0x227): Navigate camera to citizen's workplace or job location
// - buttonHome (0x226): Navigate camera to citizen's home building
// - buttonFollow (0x228): Open follow dialog to track this citizen
//
// Navigation uses Toolbar_CenterOnEntity_Impl (FUN_140103040) which moves
// the camera to the entity's world position or build footprint center.

// [9] 0x1400650f0
void CitizenUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t param3, int32_t param4,
                               int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    citizenui_sa::EnsureGlobals();
    // Gate on datasource-alive like the sibling slots (4/6/7): a click on a dead
    // citizen's still-open panel must not drive this body into a freed entity.
    if (!SA_DialogEntityAlive(this)) return;
    // Only handle event type 0x12 (button click)
    if (param3 != 0x12) return;

    uintptr_t s = (uintptr_t)this;

    if (param4 == 0x227) {
        // --- buttonProfession: go to workplace/job ---
        void* entityBase = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(entityBase, ComponentEntity::TypeId());
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        new (&entityRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge

        uint64_t aiRef = 0;
        fn_FindComponent_Citizen((void*)entityRef, &aiRef);
        uintptr_t ai = (uintptr_t)aiRef;

        uint64_t targetRef;
        uint64_t dialogRef;
        if (*(uintptr_t*)(ai + 0x208) != 0) {
            // Has workplace: navigate to workplace entity
            uintptr_t workplace = *(uintptr_t*)(ai + 0x208);
            targetRef = workplace;
            // Get dialog from workplace entity +0x10
            dialogRef = *(uint64_t*)(workplace + 0x10);
        } else {
            if (*(uintptr_t*)(ai + 0x248) == 0) return;
            // Has job entity (+0x248): navigate there
            targetRef = *(uintptr_t*)(ai + 0x248);
            dialogRef = *(uint64_t*)(s + 0x18);
        }

        void* gameSubsys = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x120);
        Toolbar_CenterOnEntity_Impl(gameSubsys, &dialogRef, &targetRef);
    }
    else if (param4 == 0x226) {
        // --- buttonHome: go to home building ---
        void* entityBase = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(entityBase, ComponentEntity::TypeId());
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        new (&entityRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge

        uint64_t aiRef = 0;
        fn_FindComponent_Citizen((void*)entityRef, &aiRef);
        uintptr_t ai = (uintptr_t)aiRef;

        uintptr_t homeEntity = *(uintptr_t*)(ai + 0x238);
        if (homeEntity == 0) return;

        uint64_t targetRef = homeEntity;
        uint64_t dialogRef = *(uint64_t*)(s + 0x18);
        void* gameSubsys = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x120);
        Toolbar_CenterOnEntity_Impl(gameSubsys, &dialogRef, &targetRef);
    }
    else if (param4 == 0x228) {
        // --- buttonFollow: open follow dialog ---
        // Creates a FollowDialog (0xD8 bytes) from Dialog/Follow.rsc and adds it to the UI.
        // Manually constructs the object: DialogInstance base ctor + FollowDialog vtable + fields.
        void* entityBase = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(entityBase, ComponentEntity::TypeId());
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        new (&entityRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge

        // Find CitizenAI component on entity
        uint64_t citizenAIRef = 0;
        fn_FindComponent_Citizen((void*)entityRef, &citizenAIRef);

        // Load Dialog/Follow.rsc resource
        System::Filename filename("Dialog/Follow.rsc");
        void* extResource = const_cast<::Object::External*>(
            reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
                ->FindOrLoad(filename));

        // Type-check the resource
        void* checkedRes = nullptr;
        if (extResource) {
            bool isExternal = fn_IsDerivedFrom(extResource, UI::Dialog::TypeId());
            if (isExternal) checkedRes = extResource;
        }

        // Allocate and construct FollowDialog (0xD8 bytes)
        void* followDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xD8, 8);
        void* resultPtr = nullptr;
        if (followDialog) {
            // The real ctor (B38 B8-5): DialogInstance(ui, resource, null parent, 0, 0, 0),
            // then FollowDialog's own table and zeroed +0xB8..+0xD0 - the bytes the tree's
            // DialogInstance ctor door + stamp of FollowDialog_GetStandaloneVtable() built.
            // (It used to read g_FollowDialog_Vtable first: 0x14015be00 is unregistered, so
            // that read was always 0 and the stamp always took the getter.)
            void* userInterface = *(void**)((uintptr_t)*(void**)(s + 0x18) + 0x10);
            FollowDialog* fd = ::new (followDialog) FollowDialog(
                reinterpret_cast<UI::UserInterface*>(userInterface),
                reinterpret_cast<const UI::Dialog*>(checkedRes),   // B.3.8 bridge
                Object::InternalPtr<UI::DialogInstance>());

            fd->dataB8 = citizenAIRef;                 // entity to follow (CitizenAI IBP)

            uint64_t selfRef = 0;
            new (&selfRef) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>((void*)this));   // B.3.3 bridge
            fd->dataC0 = selfRef;                      // controller back-reference
            // +0xC8 textFollow / +0xD0 textAction: zeroed by the ctor, set by slot 4.

            resultPtr = followDialog;
        }

        // Store dialog ref at CitizenUI+0xE0
        uint64_t dialogIBP = 0;
        new (&dialogIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(resultPtr));   // B.3.3 bridge
        *(uint64_t*)(s + 0xE0) = dialogIBP;

        // Add dialog to UI
        uint64_t dialogRef = dialogIBP;
        void* ui = *(void**)((uintptr_t)*(void**)(s + 0x18) + 0x10);
        reinterpret_cast<UI::UserInterface*>(ui)->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogRef));  // B.3.6 bridge
    }
}

// ============================================================
// STANDALONE promotion — real CitizenUI controller vtable
// ============================================================
// Clicking a citizen (now pickable via the Picking::Scene) opens a dialog whose
// controller is CitizenUI. In standalone this type was a StubNop (all slots no-op)
// so the panel opened blank. This block installs a real per-class vtable wiring the
// reimplemented slots above, following the ToolUI/MapUI promotion pattern in
// ui_dialog_stub_factories.cpp (which previously registered CitizenUI as a stub —
// that registration is REMOVED there to avoid a duplicate REGISTER_TYPE).
//
// Vtable offsets (verified from the exe CitizenUI vtable @ 0x14015ba00):
//   +0x20 (idx4) Slot4  = Setup (find elements + button ids)
//   +0x30 (idx6) Slot6  = close sub-dialog
//   +0x38 (idx7) Slot7  = content update (name/age/profession/hearts/stars)
//   +0x40 (idx8)        = base DialogController::OnUpdate (per-frame; the framework
//                         DialogInstance::OnUpdate dispatches ONLY +0x40 each frame,
//                         Runtime 0x180115400) — base is a content no-op, so we drive
//                         Slot7 here to refresh the panel every frame.
//   +0x48 (idx9) Slot9  = button dispatch (profession/home/follow)
//
// Every exe-.rdata type-id / multiplier CitizenUI_Slot7 dereferences is NULL in
// standalone (raw .rdata addr → ResolveAddr returns null). EnsureGlobals lazy-fills
// them from the live factory tree (post-Engine::Startup); Slot7 is guarded to skip
// (not fault) if any critical id failed to resolve.





// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void CitizenUI::OnUpdate(const System::Time& /*time*/) {
    CitizenUI::OnUpdateLayout();
}


// [0] 0x140010fd0: ~CitizenUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* CitizenUI::DeletingDtor(unsigned flags) {
    this->~CitizenUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140010f80: MemAlloc(0xe8, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* CitizenUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CitizenUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CitizenUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* CitizenUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CitizenUI());
}

DEFINE_EXE_VTABLE(CitizenUI, "_ZTV9CitizenUI")
DEFINE_EXE_TYPE(CitizenUI, "UI::DialogController")

