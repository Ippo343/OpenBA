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
#include "Dialog/citizen_icon_helpers.h"
#include "engine_funcs.h"
#include <cstdint>
#if defined(_DEBUG)
#include <cstdio>
#include <cstdlib>
#endif

#include "Object/factory_registration.h"
// FollowDialog's real standalone vtable (built in followdialog.cpp). In standalone
// mode the exe .rdata FollowDialog vtable (0x14015be00) is never populated, so
// g_FollowDialog_Vtable stays null; Slot9 uses this as the fallback.
extern "C" void* FollowDialog_GetStandaloneVtable();

// #6a: g_TypeId_CitizenAIComponent (exe .rdata DAT_1401c70e8) is the CitizenAIComponent
// type id — the component whose +0x28 holds the citizen's current action-state index
// (drives the Action text on the panel and the Follow dialog). It is NULL in standalone
// (raw .rdata constant → ResolveAddr returns 0), so fn_FindComponent_CitizenAI yields
// null and the Action line can't resolve. Resolve it lazily from the live Runtime
// factory tree on first panel update.
static void CitizenUI_EnsureProfessionTypeId() {
    if (g_TypeId_CitizenAIComponent) return;  // already wired
    static uint32_t s_profTid = 0;
    if (s_profTid == 0)
        s_profTid = ::Object::ResolveTypeIdByNameLazy("CitizenAIComponent");
    if (s_profTid) g_TypeId_CitizenAIComponent = &s_profTid;
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
            fn_InternalBasePtr_Ctor(retval, nullptr);
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
                fn_InternalBasePtr_Ctor(retval, nullptr);
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
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint64_t result = 0;
    fn_DialogInstance_FindElement(dialogInst, &result, nameStr);
    fn_String_Dtor(nameStr);
    // A missing element yields a non-null sentinel / unresolved value here rather than
    // 0 (seen as 0xffffffff00000000), which defeats the `if (elem)` guards downstream
    // and faults inside the engine. Normalise anything that isn't a real pointer to 0.
    return CitUI_OkPtr(result) ? result : 0;
}

// Helper: Find element by name in element tree, return InternalPtr value
static uint64_t FindElemInTree(void* elem, const char* name) {
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint64_t result = 0;
    fn_FindElement(elem, &result, nameStr);
    fn_String_Dtor(nameStr);
    // Normalise a missing/unresolved element to 0 (see FindDialogElem).
    return CitUI_OkPtr(result) ? result : 0;
}

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

void CitizenUI_Slot4(CitizenUI* self) {
    uintptr_t s = (uintptr_t)self;
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

    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x78), 0x227);
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x80), 0x226);
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x88), 0x228);
}

// ============================================================
// Slot6: Close sub-dialog if open, then clear the reference.
// ============================================================
// Original: 0x140065130

void CitizenUI_Slot6(CitizenUI* self) {
    uintptr_t s = (uintptr_t)self;

    uint64_t subDialog = *(uint64_t*)(s + 0xE0);
    if (subDialog != 0) {
        // Get the UserInterface from DialogInstance (+0x18) -> +0x10
        void* dialogInst = (void*)*(uint64_t*)(s + 0x18);
        void* userInterface = *(void**)((uintptr_t)dialogInst + 0x10);
        fn_UI_CloseDialog(userInterface, (void*)(s + 0xE0));
    }

    // Reset +0xE0 to null InternalBasePtr
    uint64_t nullPtr = 0;
    fn_InternalBasePtr_Ctor(&nullPtr, nullptr);
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

void CitizenUI_Slot7(CitizenUI* self) {
    uintptr_t s = (uintptr_t)self;

    CitizenUI_EnsureProfessionTypeId();

    // Get entity from data source (+0x28), cast to ComponentEntity
    void* entityBase = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entityBase, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? entityBase : nullptr;
    uint64_t entityRef = 0;
    fn_InternalBasePtr_Ctor(&entityRef, entity);

    // Find CitizenAIComponent
    uint64_t citizenAIRef = 0;
    fn_FindComponent_Citizen((void*)entityRef, &citizenAIRef);
    uintptr_t ai = (uintptr_t)citizenAIRef;
    if (!ai) return;

    // Get CitizenDescription from CitizenAI (+0x18), check IsDerivedFrom
    void* descBase = *(void**)(ai + 0x18);
    bool isDesc = fn_IsDerivedFrom(descBase, *g_TypeId_CitizenDescription);
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
        if (imageFemale) fn_Element_SetFlagsOnOff(imageFemale, 1, 0);  // hide imageFemale
    } else {
        if (imageFemale) fn_Element_SetFlagsOnOff(imageFemale, 0, 1);  // show imageFemale
    }
    void* imageMale = (void*)*(uint64_t*)(s + 0x30);
    if (imageMale) fn_Element_SetFlagsOnOff(imageMale, isMale ? 0 : 1, isMale ? 1 : 0);

    // --- Education visibility ---
    // age at +0x210 (int), education age threshold at description+0x118 (aiDesc = the
    // CitizenDescription; see aiDesc note above). Skip if no description resolved.
    bool isEducated = aiDesc && (*(int*)(ai + 0x210) < *(int*)(aiDesc + 0x118));
    void* textEducatedNo = (void*)*(uint64_t*)(s + 0x60);
    if (isEducated) {
        if (textEducatedNo) fn_Element_SetFlagsOnOff(textEducatedNo, 0, 1);  // show textEducatedNo
    } else {
        if (textEducatedNo) fn_Element_SetFlagsOnOff(textEducatedNo, 1, 0);  // hide textEducatedNo
    }
    void* textEducatedYes = (void*)*(uint64_t*)(s + 0x58);
    if (textEducatedYes) fn_Element_SetFlagsOnOff(textEducatedYes,
                             isEducated ? 1 : 0, isEducated ? 0 : 1);

    // --- Name text ---
    // CitizenAI+0x28 is the citizen's name (System::String)
    void* textName = (void*)*(uint64_t*)(s + 0x40);
    // Guard both the destination element AND the source name String (ai+0x28), which is
    // uninitialized on a standalone citizen and would fault when SetParameter bumps its
    // ref-count.
    bool okName = CitUI_OkString((void*)(ai + 0x28));
    bool okParam = CitUI_OkTextParam(textName);
    if (okName && okParam)
        fn_TextElement_SetParameter(textName, 0, (void*)(ai + 0x28));
#if defined(_DEBUG)
    {
        static int s_nameTrace = 0;
        if (s_nameTrace < 4 && std::getenv("BANISHED_DIALOG_TRACE")) {
            ++s_nameTrace;
            const char* nm = okName ? (const char*)*(char**)(ai + 0x28) : "<invalid>";
            fprintf(stderr, "[CitizenUI::Slot7] textName=%p okString=%d okParam=%d name='%s'"
                    " -> SetParameter %s\n", textName, (int)okName, (int)okParam,
                    nm ? nm : "(null)", (okName && okParam) ? "FIRED" : "skipped");
            // Diagnose empty-rows: are the content text elements' string tables (+0xD8)
            // and format/param arrays bound? All rows (incl. static labels) render empty
            // if the content sub-panel's elements never got their strings.
            void* clo = (void*)*(uint64_t*)(s + 0x68);   // textClothing
            auto D8 = [](void* e)->void*{ return e ? *(void**)((uintptr_t)e + 0xD8) : nullptr; };
            auto vis = [](void* e)->uint32_t{ return e ? *(uint32_t*)((uintptr_t)e + 0x34) : 0; };
            fprintf(stderr, "[CitizenUI::Slot7]   textName: strTbl(+0xD8)=%p flags(+0x34)=0x%x "
                    "paramCnt(+0xD0)=%d paramArr(+0xC8)=%p | clothing=%p strTbl=%p flags=0x%x\n",
                    D8(textName), vis(textName),
                    textName ? *(int*)((uintptr_t)textName + 0xD0) : -1,
                    textName ? *(void**)((uintptr_t)textName + 0xC8) : nullptr,
                    clo, D8(clo), vis(clo));
            fflush(stderr);
        }
    }
#endif

    // --- Age number ---
    float ageFloat = *(float*)(ai + 0x3c);
    int ageYears = (int)(ageFloat * *g_pAgeMultiplier);
    void* numberAge = (void*)*(uint64_t*)(s + 0x48);
    if (numberAge) fn_NumberElement_SetValueInt(numberAge, ageYears);

    // --- Clothing text (string table lookup) ---
    {
        void* clothingElem = (void*)*(uint64_t*)(s + 0x68);
        if (clothingElem) {
            uint32_t clothingIdx = *(uint32_t*)(ai + 0x1e4);
            uintptr_t elemPtr = (uintptr_t)clothingElem;
            // Build UI::String from element's string table (+0xD8) and string entry (+0xE0)
            uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
            uint8_t uiStr[16];
            fn_UIString_Ctor_Index(uiStr, stringTable, clothingIdx);
            uint8_t textVal[16];
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
            fn_TextElement_SetText(clothingElem, textVal);
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
        bool isDesc2 = fn_IsDerivedFrom(descBase2, *g_TypeId_CitizenDescription);
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
            fn_UIString_Ctor_Index(uiStr, stringTable, toolValue);
            uint8_t textVal[16];
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
            fn_TextElement_SetText(toolElem, textVal);
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
                    fn_UIString_Ctor(uiStr, strTable, (void*)(profEntry + 0x30));
                    *(uint64_t*)textVal = *(uint64_t*)uiStr;
                    *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
                    haveText = true;
                }
            } else {
                uintptr_t profDesc = *(uintptr_t*)(ai + 0x1F0);
                if (okPtr(profDesc)) {
                    uint64_t strTable = *(uint64_t*)(profDesc + 0x20);
                    uint8_t uiStr[16];
                    fn_UIString_Ctor(uiStr, strTable, (void*)(profDesc + 0x30));
                    *(uint64_t*)textVal = *(uint64_t*)uiStr;
                    *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
                    haveText = true;
                }
            }
            if (haveText) fn_TextElement_SetText(profElem, textVal);
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
            typedef void (*fn_VSetEnabled_t)(void* self, bool enabled);
            void** vtable = *(void***)profBtn;
            fn_VSetEnabled_t vfunc = (fn_VSetEnabled_t)vtable[8]; // vtable+0x40
            vfunc(profBtn, profEnabled);
        }
    }

    // --- Home button virtual call ---
    {
        void* homeBtn = (void*)*(uint64_t*)(s + 0x80);
        if (homeBtn) {
            typedef void (*fn_VSetEnabled_t)(void* self, bool enabled);
            void** vtable = *(void***)homeBtn;
            fn_VSetEnabled_t vfunc = (fn_VSetEnabled_t)vtable[8]; // vtable+0x40
            // Original passes result of CheckPtr on CitizenAI (always true if we got here)
            vfunc(homeBtn, true);
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
    // Get entity reference from CitizenAI+0x10, find profession component
    uintptr_t entityFromAI = *(uintptr_t*)(ai + 0x10);
    uint64_t profCompRef = 0;
    // Guard the entity backref (component-list walk dereferences entity+0x30/+0x38);
    // a partially-initialized standalone citizen can leave it as garbage.
    if (CitUI_OkPtr(entityFromAI))
        fn_FindComponent_CitizenAI((void*)entityFromAI, &profCompRef);

    // The profession component may be absent — profCompRef null — when the citizen
    // has no profession (child) or, in standalone, when the ProfessionComponent
    // type-id could not be resolved from the factory tree (fn_FindComponent_CitizenAI
    // then finds nothing). Skip the action text rather than dereferencing null+0x28.
    if (profCompRef != 0) {
    // Read action index from profession component (+0x28)
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
            fn_UIString_Ctor_Index(uiStr, stringTable, actionIndex);
            uint8_t textVal[16];
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
            fn_TextElement_SetText(actionElem, textVal);
        }
    }
    }  // if (profCompRef != 0)
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

void CitizenUI_Slot9(CitizenUI* self, uint64_t /*param2*/, int param3, int param4) {
    // Only handle event type 0x12 (button click)
    if (param3 != 0x12) return;

    uintptr_t s = (uintptr_t)self;

    CitizenUI_EnsureProfessionTypeId();

    if (param4 == 0x227) {
        // --- buttonProfession: go to workplace/job ---
        void* entityBase = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(entityBase, *g_TypeId_ComponentEntity);
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        fn_InternalBasePtr_Ctor(&entityRef, entity);

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
        bool isEntity = fn_IsDerivedFrom(entityBase, *g_TypeId_ComponentEntity);
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        fn_InternalBasePtr_Ctor(&entityRef, entity);

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
        bool isEntity = fn_IsDerivedFrom(entityBase, *g_TypeId_ComponentEntity);
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        fn_InternalBasePtr_Ctor(&entityRef, entity);
        fn_InternalBasePtr_CheckPtr(&entityRef);

        // Find CitizenAI component on entity
        uint64_t citizenAIRef = 0;
        fn_FindComponent_Citizen((void*)entityRef, &citizenAIRef);

        // Load Dialog/Follow.rsc resource
        uint8_t filename[8];
        fn_Filename_CtorCStr(filename, "Dialog/Follow.rsc");
        void* extResource = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filename);

        // Type-check the resource
        void* checkedRes = nullptr;
        if (extResource) {
            bool isExternal = fn_IsDerivedFrom(extResource, *g_pExternalTypeId);
            if (isExternal) checkedRes = extResource;
        }
        fn_Filename_Dtor(filename);

        // Allocate and construct FollowDialog (0xD8 bytes)
        void* followDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xD8, 8);
        void* resultPtr = nullptr;
        if (followDialog) {
            // Construct base DialogInstance(self, userInterface, resource, nullParent, 0, 0, 0)
            uint64_t nullParent = 0;
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
            void* userInterface = *(void**)((uintptr_t)*(void**)(s + 0x18) + 0x10);
            fn_DialogInstance_Ctor(followDialog, userInterface, checkedRes, &nullParent, 0, 0, 0);

            // Patch vtable to FollowDialog.
            void* fdVtable = (void*)g_FollowDialog_Vtable;
            // Standalone: exe .rdata vtable (0x14015be00) is null → g_FollowDialog_Vtable
            // unresolved. Use the real reimplemented vtable from followdialog.cpp,
            // otherwise the dialog gets a NULL vtable and hard-faults on dispatch.
            if (!fdVtable) fdVtable = FollowDialog_GetStandaloneVtable();
            *(void**)followDialog = fdVtable;

            // Set FollowDialog-specific fields
            uintptr_t fd = (uintptr_t)followDialog;
            *(uint64_t*)(fd + 0xB8) = citizenAIRef;   // entity to follow (CitizenAI IBP)

            uint64_t selfRef = 0;
            fn_InternalBasePtr_Ctor(&selfRef, (void*)self);
            *(uint64_t*)(fd + 0xC0) = selfRef;         // controller back-reference

            *(uint64_t*)(fd + 0xC8) = 0;               // textFollow (set by Slot4)
            *(uint64_t*)(fd + 0xD0) = 0;               // textAction (set by Slot4)

            resultPtr = followDialog;
        }

        // Store dialog ref at CitizenUI+0xE0
        uint64_t dialogIBP = 0;
        fn_InternalBasePtr_Ctor(&dialogIBP, resultPtr);
        *(uint64_t*)(s + 0xE0) = dialogIBP;

        // Add dialog to UI
        uint64_t dialogRef = dialogIBP;
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* ui = *(void**)((uintptr_t)*(void**)(s + 0x18) + 0x10);
        fn_UI_AddDialog(ui, &dialogRef);
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
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include "dialog_controller_vtable.h"

namespace citizenui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("CitizenUI");
    return c;
}
void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// g_pAgeMultiplier's exe .rdata (0x140172198) is uninitialized in the standalone
// image (unreadable / zero-fill), so fall back to 1.0 — citizen age at CitizenAI+0x3c
// is already in display years for the panel. Not a crash risk either way; the fallback
// just avoids a null-deref in the age line.
static float s_ageMul = 1.0f;

void EnsureGlobals() {
    static uint32_t s_ce = 0, s_cd = 0, s_cai = 0, s_prof = 0, s_ext = 0;
    static uint32_t s_profZero = 0;  // safe no-op type-id for fn_FindComponent_CitizenAI
    if (!g_TypeId_ComponentEntity)        { s_ce  = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");    if (s_ce)   g_TypeId_ComponentEntity = &s_ce; }
    if (!g_TypeId_CitizenDescription)     { s_cd  = ::Object::ResolveTypeIdByNameLazy("CitizenDescription"); if (s_cd)   g_TypeId_CitizenDescription = &s_cd; }
    // g_TypeId_CitizenComponent_FC is DAT_1401c7158 — its name says "CitizenComponent"
    // now, but the finder that reads it (fn_FindComponent_Citizen) is what the AI panel
    // actually needs: the name at +0x28, age +0x3c, profession +0x1f0, housing +0x248,
    // desc +0x288 all live on the CitizenComponent it resolves (the same component the
    // founding path writes the generated name to). Resolving the literal
    // "CitizenAIComponent" here finds a DIFFERENT sibling component whose +0x28 is
    // unset, so the dialog read the wrong object. Resolve "CitizenComponent" (matching
    // the founding writer's TypeId lookup) so the dialog reads the citizen data that
    // was actually filled in.
    if (!g_TypeId_CitizenComponent_FC)    { s_cai = ::Object::GetTypeIdByName("CitizenComponent"); if (s_cai)  g_TypeId_CitizenComponent_FC = &s_cai; }
    if (!g_TypeId_CitizenAIComponent)     { s_prof= ::Object::ResolveTypeIdByNameLazy("ProfessionComponent");if (s_prof) g_TypeId_CitizenAIComponent = &s_prof; }
    // ProfessionComponent's registered name doesn't resolve in the standalone factory
    // tree (0x1401c70e8 is settled as CitizenAIComponent's BaseTypeInfo — writer
    // CitizenAIComponent_RegisterType). It's used ONLY by the Slot7 action-text lookup, which
    // is now null-guarded (skips if the component isn't found). Point the global at a
    // static 0 so fn_FindComponent_CitizenAI is a safe no-op (finds nothing) instead of
    // dereferencing a null type-id pointer.
    if (!g_TypeId_CitizenAIComponent)     { g_TypeId_CitizenAIComponent = &s_profZero; }
    if (!g_pExternalTypeId)               { s_ext = ::Object::ResolveTypeIdByNameLazy("External");           if (s_ext)  g_pExternalTypeId = &s_ext; }
    // The Home/Profession(workplace) buttons call Toolbar_CenterOnEntity_Impl, whose
    // EditTool re-select derefs *g_TypeId_EditTool — NULL in standalone (raw exe .rdata).
    // Resolve it here so the camera snap AND the EditTool re-highlight both work; the
    // inline also guards the deref, so an unresolved id degrades to camera-move only
    // instead of faulting.
    static uint32_t s_edit = 0;
    if (!g_TypeId_EditTool)               { s_edit = ::Object::ResolveTypeIdByNameLazy("EditTool");          if (s_edit) g_TypeId_EditTool = &s_edit; }
    if (!g_pAgeMultiplier) g_pAgeMultiplier = &s_ageMul;

    static bool s_logged = false;
    if (!s_logged && std::getenv("BANISHED_DIALOG_TRACE")) {
        s_logged = true;
        fprintf(stderr, "[CitizenUI] globals: CE=%p CD=%p CAI=%p Prof=%p Ext=%p\n",
                (void*)g_TypeId_ComponentEntity, (void*)g_TypeId_CitizenDescription,
                (void*)g_TypeId_CitizenComponent_FC, (void*)g_TypeId_CitizenAIComponent,
                (void*)g_pExternalTypeId);
        fflush(stderr);
    }
}

void W_Slot4(void* self) {
    EnsureGlobals();
    if (!self || !SA_DialogEntityAlive(self)) return;
    CitizenUI_Slot4(reinterpret_cast<CitizenUI*>(self));
}
void W_Slot6(void* self) {
    if (!self || !SA_DialogEntityAlive(self)) return;
    CitizenUI_Slot6(reinterpret_cast<CitizenUI*>(self));
}
void W_Slot7(void* self) {
    EnsureGlobals();
    // ProfessionComponent is not required — its only use (action text) is null-guarded
    // and its global has a safe static-0 fallback (see EnsureGlobals).
    if (!self || !g_TypeId_ComponentEntity || !g_TypeId_CitizenDescription
        || !g_TypeId_CitizenComponent_FC) return;
    if (!SA_DialogEntityAlive(self)) return;
    CitizenUI_Slot7(reinterpret_cast<CitizenUI*>(self));
}
void W_OnUpdate(void* self, void* /*time*/) { W_Slot7(self); }   // +0x40 per-frame
void W_Slot9(void* self, void* p2, int msg, int id) {
    EnsureGlobals();
    // Follow button (0x228) builds a FollowDialog whose vtable is g_FollowDialog_Vtable
    // — null in standalone. CitizenUI_Slot9 handles profession/home unconditionally;
    // guard the follow path by skipping if the vtable is unresolved (leave a no-op).
    // Also gate on datasource-alive like the sibling slots (W_Slot4/6/7): a click on a
    // dead citizen's still-open panel must not drive CitizenUI_Slot9 into a freed entity.
    if (self && SA_DialogEntityAlive(self))
        CitizenUI_Slot9(reinterpret_cast<CitizenUI*>(self),
                        reinterpret_cast<uint64_t>(p2), msg, id);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Slot4);
        g_vt[6] = reinterpret_cast<void*>(&W_Slot6);
        g_vt[7] = reinterpret_cast<void*>(&W_Slot7);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_Slot9);
    }
} g_init;

// Wave 10D lane VTFID: retail's CitizenUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "CitizenUI", &StubNop);

void Zero(void* p) { for (size_t o = 0; o < (size_t)0xE8; o += 8) *reinterpret_cast<uint64_t*>((char*)p + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)0xE8, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt; return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; return m; }

}  // namespace citizenui_sa

REGISTER_TYPE(CitizenUI, "UI::DialogController",
              &citizenui_sa::Factory, &citizenui_sa::InPlace);

