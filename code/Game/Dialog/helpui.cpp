// engine/src/ui/helpui.cpp
// Clean reimplementation of HelpUI game logic.
//
// HelpUI (0x78 bytes) — help page navigation panel.
// Extends DialogController (0x30 base). Fields:
//   +0x30: first element (InternalBasePtr) — scroll position source at elem+0x160
//   +0x38: DocumentTextElement (InternalBasePtr), ID=0xE75
//   +0x40: ComboElement (InternalBasePtr), ID=0xE74
//   +0x48: buttonBack (InternalBasePtr), ID=0xE76
//   +0x50: buttonForward (InternalBasePtr), ID=0xE77
//   +0x58: container element (InternalBasePtr) — scroll offsets
//   +0x60: current page resource index (int32_t)
//   +0x64: current history index (int32_t)
//   +0x68: history array pointer (8-byte entries: int32_t pageIndex + float scrollY)
//   +0x70: history count (int32_t)
//   +0x74: history capacity (int32_t)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include <cstdio>
#include "system/String.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ContainerElement.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ComboElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/DocumentTextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/ScrollElement.h"   // X::TypeId() (ENDGAME B.3.9)
#include "runtime/types/ui/ButtonElement.h"   // X::TypeId() (ENDGAME B.3.9)
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// ============================================================
// File-local helpers
// ============================================================

// Get the help topics ExternalList from the game world singleton
// (retail: `*(ExternalList**)(g_pGameWorldSingleton + 0xa0)`, verified against the
// disassembly of 0x140069f38 — the singleton IS the GameUpdateLoop at .rdata
// 0x1401c3dd8). Standalone deviation: the singleton is null before the game loop
// exists (and the list itself can be null if the help resources never loaded), so
// return null rather than fault; every caller below treats null as "no topics".
static void* GetHelpExternalList() {
    if (!g_pGameWorldSingleton) return nullptr;
    void* world = *g_pGameWorldSingleton;
    if (!world) return nullptr;
    return *(void**)((uintptr_t)world + 0xa0);
}

// ============================================================
// HelpUI_LoadPage (FUN_140069f20) — Load help page content
// ============================================================
// Gets the currently selected combo item, looks up the corresponding
// StringTable resource, sets its content (text index 2) on the document
// element, resets scroll to top, and pushes a history entry.
//
// Original: 0x140069f20.

static void HelpUI_LoadPage(HelpUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* extList = GetHelpExternalList();

    // Get currently selected combo item
    void* comboElem = *(void**)(s + 0x40);

    // Standalone guard (retail has none — its .rsc always yields both elements and
    // the world always owns the help list). If an element lookup failed (a null
    // element type-id makes FindElementType store null even when the element
    // exists) this would deref null at combo+0x118 / inside SetText.
    if (!extList || !comboElem || !*(void**)(s + 0x38) || !*(void**)(s + 0x58))
        return;
    int currentItem = *(int*)((uintptr_t)comboElem + 0x118);
    void* itemData = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetItemData(currentItem);  // B.3.6 bridge
    int pageResIndex = (int)(intptr_t)itemData;

    // Look up StringTable resource from ExternalList
    void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
    void* stringTable = *(void**)((uintptr_t)resourceArray + (int64_t)pageResIndex * 8);

    // Check IsDerivedFrom (StringTable type)
    bool isStringTable = ::Object::IsDerivedFromNullSafe(stringTable, UI::StringTable::TypeId());
    void* validTable = isStringTable ? stringTable : nullptr;

    // Construct UI::String with text index 2 (help content body)
    uint8_t uiString[16] = {0};
    ::new (uiString) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>((uint64_t)validTable)), 2);  // B.3.6 bridge

    // Set document text
    void* docElem = *(void**)(s + 0x38);
    reinterpret_cast<UI::DocumentTextElement*>(docElem)->SetText(reinterpret_cast<const UI::String*>(uiString));  // B.3.6 bridge

    // Reset scroll offsets to (0, 0)
    void* containerElem = *(void**)(s + 0x58);
    reinterpret_cast<UI::ContainerElement*>(containerElem)->SetOffsets(0, 0);  // B.3.6 bridge

    // Store current page resource index
    *(int*)(s + 0x60) = pageResIndex;

    // History array at +0x68: Collection::Array<{int32 pageIndex, float scrollY}>
    // (stride 8, count +0x70, capacity +0x74) inside a struct §B.2 has not
    // converted, so it is reached at its offset. B.3.2 bridge for both the
    // truncate (was HelpUI_ArrayResize_Impl, FUN_140011d50) and the append below.
    //
    // The element is GameColl::I32Pair and NOT GameColl::Bytes<8, 8>, for the two
    // reasons its declaration in Component/coll_bridge.h spells out: retail's
    // helper passed 4 — not 8 — as MemAlloc's alignment argument for this array,
    // and it left a GROWN tail uninitialised, which only a trivially
    // default-constructible element reproduces.
    auto& hist = GameColl::ArrayAt<GameColl::I32Pair>(s, 0x68);

    // Truncate history to (historyIndex + 1) entries — removes forward history.
    int historyIndex = *(int*)(s + 0x64);
    hist.SetSize(historyIndex + 1);

    // Push new history entry: {pageResIndex, scrollY=0.0f} at the new end.
    hist.SetSize(hist.GetSize() + 1, GameColl::I32Pair{pageResIndex, 0});  // scrollY = 0.0f

    // Set history index to last entry
    *(int*)(s + 0x64) = hist.GetSize() - 1;
}

// ============================================================
// HelpUI_FindLinkTarget (FUN_14006a140) — Find page by link text
// ============================================================
// Iterates the help topics ExternalList, gets text at index 1
// (topic name) from each StringTable, compares with the search
// string. Returns the matching resource index, or -1 if not found.
//
// Note: Destroys the input search string (param_2) on return.
//
// Original: 0x14006a140.

// Takes ownership of searchStr (by value) and destroys it on return — the RAII
// destructor at scope exit reproduces retail's manual release on every path.
static int HelpUI_FindLinkTarget(System::String searchStr) {
    void* extList = GetHelpExternalList();
    if (!extList) { return -1; }   // standalone guard
    int count = reinterpret_cast<const ::Object::ExternalList*>(extList)  // B.3.3 bridge
                        ->GetResourceCount();

    for (int i = 0; i < count; i++) {
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* entry = *(void**)((uintptr_t)resourceArray + (int64_t)i * 8);

        bool isStringTable = ::Object::IsDerivedFromNullSafe(entry, UI::StringTable::TypeId());
        if (isStringTable && entry != nullptr) {
            // GetText at index 0 = topic key/name for matching (non-trivial return)
            System::String nameStr;
            ::new (&nameStr) System::String(reinterpret_cast<const UI::StringTable*>(entry)->GetText(0));  // B.3.6 bridge

            bool match = (searchStr == nameStr);

            if (match) {
                return i;
            }
        }

        // Re-check count each iteration (matches original)
        count = reinterpret_cast<const ::Object::ExternalList*>(extList)  // B.3.3 bridge
                        ->GetResourceCount();
    }

    return -1;
}

// ---- HelpUI: PROMOTED to the real reimpl (help-dialog bring-up 2026-07-31) ----
// The toolbar "help" utility dialog (Template\UtilityHelp.rsc -> StandardDialog.rsc,
// content Dialog/Help.rsc). The whole controller is already reimplemented in
// code/Game/Dialog/helpui.cpp, but the generic stub below left every slot a no-op,
// so the dialog opened with an empty topic combo and a blank document — the
// "controllers render NO content" follow-up recorded in GUI_PLAN.md Phase 4.
//
// Slot map, read out of the retail vtable at exe .rdata 0x14015bf30 (qword N =
// slot N), NOT from our labels:
//   slot 4 (+0x20) 0x14006a500 Slot4  — setup: find the 4 typed elements + the two
//                                       named buttons, populate the topic combo from
//                                       the help ExternalList, tail into LoadPage.
//   slot 5 (+0x28) 0x14006a4f0        — 3-byte RET no-op (left as StubNop).
//   slot 7 (+0x38) 0x14006a230 Slot7  — enable/disable back+forward per history index.
//   slot 9 (+0x48) 0x14006a290 Slot9  — combo change (msg 0x14), document link click
//                                       and back/forward (msg 0x12).
// Slot9's 7th parameter is the clicked link index: retail reads it from
// [RSP+0xb0] at 0x14006a2ed, which is arg 7 in this frame — hence the 7-arg wrapper.
//
// ── Construction deviation ───────────────────────────────────────────────────
// HelpUI_Ctor (0x14006a920) zeroes +0x30..+0x58 and +0x68/+0x70 but writes
// **-1** to the qword at +0x60, i.e. BOTH the current page index (+0x60) and the
// history index (+0x64) start at -1. A plain zero-fill is not equivalent: LoadPage
// truncates the history to (historyIndex + 1) entries, so a 0 index leaves one
// uninitialised history slot in front of the first real page and the back button
// then navigates to a garbage page id. Factory/InPlace stamp the -1 explicitly.
//
// ── Type-id dependency ───────────────────────────────────────────────────────
// Slot4 looks its elements up by type (ScrollElement/ComboElement/
// DocumentTextElement/ContainerElement, plus ButtonElement for the two named
// buttons) and checks the topic resources against StringTable. Those ids come from
// exe .rdata dispatch slots that standalone only fills when
// BANISHED_LIVE_ELEMENT_TIDS is set (standalone/reg_element_typeids.cpp). Rather
// than make the help dialog depend on that gate, EnsureTypeIds() points the globals
// straight at the Runtime DLL's own BaseTypeInfo exports — the same objects retail's
// name resolution would have written into those slots, so this is a no-op when the
// gate is on and a self-heal when it is off. A type-id that fails to resolve leaves
// the element null, and the reimpl's standalone guards then degrade to today's
// empty-panel behaviour instead of faulting.
namespace helpui_sa {
    // ENDGAME B.3.9: UI::ScrollElement / UI::ComboElement / UI::DocumentTextElement /
    // UI::ContainerElement / UI::ButtonElement / UI::StringTable all have a ported
    // TypeId() now (class statics, always populated), so the EnsureTypeIds() fill
    // from Runtime exports and the Ready() pointer checks over them are dead.
    // B3R: the function-pointer readiness over the engine_funcs doors was always true
    // and is gone; the allocator singleton (the old g_pMemAllocSingleton snapshot)
    // is read live.
    bool Ready() {
        return System::MemoryAllocator::_singleton != nullptr;
    }
    // Report the OUTCOME of the readiness test, once, naming what is missing.
    bool CheckOnce() {
        static int s_ok = -1;
        if (s_ok >= 0) return s_ok != 0;
        s_ok = Ready() ? 1 : 0;
        if (!s_ok) {
            fprintf(stderr, "[HelpUI] slots stay no-op — unresolved engine function deps\n");
            fflush(stderr);
        }
        return s_ok != 0;
    }
    void Trace([[maybe_unused]] const char* slot, void* self) {
        static int n=0; if (n>=24) return; ++n;
        [[maybe_unused]] uintptr_t s=reinterpret_cast<uintptr_t>(self);
        void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
        [[maybe_unused]] void* topics = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw)+0xa0) : nullptr;
        printf("[HelpUI] %s self=%p root(+0x20)=%p scroll=%p doc=%p combo=%p back=%p fwd=%p "
               "cont=%p page=%d hist=%d/%d topics=%p n=%d\n", slot, self,
               *reinterpret_cast<void**>(s+0x20), *reinterpret_cast<void**>(s+0x30),
               *reinterpret_cast<void**>(s+0x38), *reinterpret_cast<void**>(s+0x40),
               *reinterpret_cast<void**>(s+0x48), *reinterpret_cast<void**>(s+0x50),
               *reinterpret_cast<void**>(s+0x58), *reinterpret_cast<int*>(s+0x60),
               *reinterpret_cast<int*>(s+0x64), *reinterpret_cast<int*>(s+0x70), topics,
               topics
                   ? reinterpret_cast<const ::Object::ExternalList*>(topics)->GetResourceCount()  // B.3.3 bridge
                   : -1);
        fflush(stdout);
    }
}

// ============================================================
// HelpUI::Slot4 (vtable +0x20) — Setup / initialization
// ============================================================
// Finds typed UI elements from the dialog:
//   - First generic element stored at +0x30
//   - DocumentTextElement at +0x38, ID=0xE75
//   - ComboElement at +0x40, ID=0xE74
//   - ContainerElement at +0x58
// Finds named elements:
//   - "buttonBack" at +0x48, ID=0xE76
//   - "buttonForward" at +0x50, ID=0xE77
// Populates combo with help topic names from ExternalList.
// Calls LoadPage to display the first topic.
//
// Original: 0x14006A500.

// [4] 0x14006a500
void HelpUI::OnCreated() {
    if (!helpui_sa::CheckOnce()) return;
    // Setup is not idempotent: a second run appends every topic to the combo
    // again and pushes a second history entry. The framework dispatches it once
    // per dialog build; skip if the element cache is already populated.
    if (*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(this)+0x40)) {
        helpui_sa::Trace("Slot4-skip(already set up)", this);
        return;
    }
    uintptr_t s = (uintptr_t)this;
    void* dialogElem = *(void**)(s + 0x20);

    // FindElementType calls use different type IDs from _typeInfo_exref.
    // Each finds a different typed element in the dialog tree.
    // Order matches original: +0x30 first, then +0x40, +0x38, +0x58.

    uint8_t retBuf[16] = {0};
    uint8_t bpBuf[8] = {0};
    void* elem;
    bool derived;

    // 1. Find first element (type 0x1401517e8) -> +0x30
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(UI::ScrollElement::TypeId()));  // B.3.6 bridge
    elem = *(void**)retBuf;
    derived = ::Object::IsDerivedFromNullSafe(elem, UI::ScrollElement::TypeId());
    new (bpBuf) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(derived ? elem : nullptr));   // B.3.3 bridge
    *(uint64_t*)(s + 0x30) = *(uint64_t*)bpBuf;

    // 2. Find ComboElement (type 0x140151690) -> +0x40
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(UI::ComboElement::TypeId()));  // B.3.6 bridge
    elem = *(void**)retBuf;
    derived = ::Object::IsDerivedFromNullSafe(elem, UI::ComboElement::TypeId());
    new (bpBuf) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(derived ? elem : nullptr));   // B.3.3 bridge
    *(uint64_t*)(s + 0x40) = *(uint64_t*)bpBuf;

    // 3. Find DocumentTextElement (type 0x1401517f0) -> +0x38
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(UI::DocumentTextElement::TypeId()));  // B.3.6 bridge
    elem = *(void**)retBuf;
    derived = ::Object::IsDerivedFromNullSafe(elem, UI::DocumentTextElement::TypeId());
    new (bpBuf) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(derived ? elem : nullptr));   // B.3.3 bridge
    *(uint64_t*)(s + 0x38) = *(uint64_t*)bpBuf;

    // 4. Find ContainerElement (type 0x1401517f8) -> +0x58
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(UI::ContainerElement::TypeId()));  // B.3.6 bridge
    elem = *(void**)retBuf;
    derived = ::Object::IsDerivedFromNullSafe(elem, UI::ContainerElement::TypeId());
    new (bpBuf) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(derived ? elem : nullptr));   // B.3.3 bridge
    *(uint64_t*)(s + 0x58) = *(uint64_t*)bpBuf;

    // Set element IDs on combo and document elements
    // (standalone guard: SetElementId derefs its `this`, so skip a failed lookup)
    if (*(void**)(s + 0x40)) reinterpret_cast<UI::Element*>(*(void**)(s + 0x40))->SetElementId(0xE74);  // B.3.6 bridge
    if (*(void**)(s + 0x38)) reinterpret_cast<UI::Element*>(*(void**)(s + 0x38))->SetElementId(0xE75);  // B.3.6 bridge

    // Find "buttonBack" by name -> +0x48, set ID 0xE76
    void* backBtn;
    {
        System::String nameStr("buttonBack");
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
        backBtn = *(void**)retBuf;
    }
    derived = ::Object::IsDerivedFromNullSafe(backBtn, UI::ButtonElement::TypeId());
    new (bpBuf) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(derived ? backBtn : nullptr));   // B.3.3 bridge
    *(uint64_t*)(s + 0x48) = *(uint64_t*)bpBuf;

    // Find "buttonForward" by name -> +0x50, set ID 0xE77
    void* fwdBtn;
    {
        System::String nameStr("buttonForward");
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
        fwdBtn = *(void**)retBuf;
    }
    derived = ::Object::IsDerivedFromNullSafe(fwdBtn, UI::ButtonElement::TypeId());
    new (bpBuf) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(derived ? fwdBtn : nullptr));   // B.3.3 bridge
    *(uint64_t*)(s + 0x50) = *(uint64_t*)bpBuf;

    if (*(void**)(s + 0x48)) reinterpret_cast<UI::Element*>(*(void**)(s + 0x48))->SetElementId(0xE76);  // B.3.6 bridge
    if (*(void**)(s + 0x50)) reinterpret_cast<UI::Element*>(*(void**)(s + 0x50))->SetElementId(0xE77);  // B.3.6 bridge

    // Populate combo with help topic names from ExternalList
    void* extList = GetHelpExternalList();
    void* comboElem = *(void**)(s + 0x40);
    if (!extList || !comboElem) return;    // standalone guard
    int topicCount = reinterpret_cast<const ::Object::ExternalList*>(extList)  // B.3.3 bridge
                        ->GetResourceCount();

    for (int i = 0; i < topicCount; i++) {
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* entry = *(void**)((uintptr_t)resourceArray + (int64_t)i * 8);

        bool isStringTable = ::Object::IsDerivedFromNullSafe(entry, UI::StringTable::TypeId());
        if (isStringTable && entry != nullptr) {
            // Get topic name (text index 1) — non-trivial return
            System::String topicName;
            ::new (&topicName) System::String(reinterpret_cast<const UI::StringTable*>(entry)->GetText(1));  // B.3.6 bridge

            // Create default sprite
            uint8_t sprite[32];
            ::new (sprite) UI::Sprite();  // B.3.6 bridge

            // Add item to combo: (sprite, name, userData=index)
            reinterpret_cast<UI::ComboElement*>(comboElem)->AddItem(*reinterpret_cast<const UI::Sprite*>(sprite), topicName, (void*)(intptr_t)i);  // B.3.6 bridge
        }

        // Re-check count each iteration (matches original)
        topicCount = reinterpret_cast<const ::Object::ExternalList*>(extList)  // B.3.3 bridge
                        ->GetResourceCount();
    }

    // Select first item and load its content
    reinterpret_cast<UI::ComboElement*>(comboElem)->SetCurrentItem(0);  // B.3.6 bridge
    HelpUI_LoadPage(this);
    helpui_sa::Trace("Slot4", this);
}

// ============================================================
// HelpUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Enables/disables prev/next buttons based on current page index.
//
// Original: 0x14006A230.

// [7] 0x14006a230
void HelpUI::OnUpdateLayout() {
    if (!helpui_sa::CheckOnce()) return;
    int pageIndex = *(int*)((uintptr_t)this + 0x64);
    int pageCount = *(int*)((uintptr_t)this + 0x70);

    // Enable prev button if not on first page
    // (standalone guard: a failed "buttonBack"/"buttonForward" lookup leaves the
    // field null, and retail dereferences it unconditionally)
    void* prevBtn = (void*)this->data48;
    if (prevBtn) {
        reinterpret_cast<UI::Element*>(prevBtn)->Enable(pageIndex > 0);  // B.3.6 bridge: Element slot 8 Enable
    }

    // Enable next button if not on last page
    void* nextBtn = (void*)this->data50;
    if (nextBtn) {
        reinterpret_cast<UI::Element*>(nextBtn)->Enable(pageIndex < pageCount - 1);  // B.3.6 bridge: Element slot 8 Enable
    }
}

// ============================================================
// HelpUI::Slot9 (vtable +0x48) — Notification handler
// ============================================================
// Handles UI events:
//   Message 0x12 (button/link click):
//     - 0xE75 (document link): get link text, find target page,
//       save scroll position to history, navigate to target
//     - 0xE76 (back): go to previous history entry, restore scroll
//     - 0xE77 (forward): go to next history entry, restore scroll
//   Message 0x14 (combo selection change):
//     - 0xE74: topic changed, reload page content
//
// Original: 0x14006A290.

// Standalone guard helper: every navigation branch below (link / back / forward)
// dereferences the scroll source (+0x30), the document (+0x38), the combo (+0x40),
// the container (+0x58) and the history array (+0x68) without a null check, exactly
// as retail does. If setup could not find an element, treat the event as unhandled.
static bool HelpUI_NavReady(uintptr_t s) {
    return *(void**)(s + 0x30) && *(void**)(s + 0x38) && *(void**)(s + 0x40) &&
           *(void**)(s + 0x58) && *(void**)(s + 0x68);
}

// [9] 0x14006a290
void HelpUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t msg, int32_t id,
                            int32_t param5, int32_t param6, int32_t linkIndex, void* /*userData*/) {
    uint64_t param2 = reinterpret_cast<uint64_t>(element);
    if (!helpui_sa::CheckOnce()) return;
    helpui_sa::Trace("Slot9", this);
    (void)param2; (void)param5; (void)param6;
    uintptr_t s = (uintptr_t)this;

    if (msg == 0x14 && id == 0xE74) {
        // Combo selection changed — reload page
        HelpUI_LoadPage(this);
        return;
    }

    if (msg != 0x12)
        return;

    if (id != 0xE75 && id != 0xE76 && id != 0xE77)
        return;
    if (!HelpUI_NavReady(s))
        return;   // standalone guard, see above

    if (id == 0xE75) {
        // Document link clicked — navigate to linked page
        void* docElem = *(void**)(s + 0x38);

        // Get the clicked link's text (non-trivial return)
        System::String linkStr;
        ::new (&linkStr) System::String(reinterpret_cast<const UI::DocumentTextElement*>(docElem)->GetLink(linkIndex));  // B.3.6 bridge

        // FindLinkTarget takes its own copy (by value) and destroys it; returns
        // the matching index or -1.
        int targetIdx = HelpUI_FindLinkTarget(linkStr);

        if (targetIdx >= 0) {
            // Save current scroll position to history
            void* firstElem = *(void**)(s + 0x30);
            int histIdx = *(int*)(s + 0x64);
            void* histArray = *(void**)(s + 0x68);
            float scrollPos = *(float*)((uintptr_t)firstElem + 0x160);
            *(float*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4) = scrollPos;

            // Navigate to target page
            void* comboElem = *(void**)(s + 0x40);
            reinterpret_cast<UI::ComboElement*>(comboElem)->SetCurrentItem(targetIdx);  // B.3.6 bridge
            HelpUI_LoadPage(this);
        }

        return;
    }

    if (id == 0xE76) {
        // Back button — go to previous history entry
        void* firstElem = *(void**)(s + 0x30);
        int histIdx = *(int*)(s + 0x64);
        void* histArray = *(void**)(s + 0x68);

        // Save current scroll position before navigating
        int scrollVal = *(int*)((uintptr_t)firstElem + 0x160);
        *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4) = scrollVal;

        // Move back one entry
        histIdx--;
        *(int*)(s + 0x64) = histIdx;

        // Get page index from history
        int pageIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);

        // Look up StringTable from ExternalList
        void* extList = GetHelpExternalList();
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* stringTable = *(void**)((uintptr_t)resourceArray + (int64_t)pageIdx * 8);

        bool isStringTable = ::Object::IsDerivedFromNullSafe(stringTable, UI::StringTable::TypeId());
        void* validTable = isStringTable ? stringTable : nullptr;

        // Construct UI::String with text index 2 (content) and set on document
        uint8_t uiString[16] = {0};
        ::new (uiString) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>((uint64_t)validTable)), 2);  // B.3.6 bridge

        void* docElem = *(void**)(s + 0x38);
        reinterpret_cast<UI::DocumentTextElement*>(docElem)->SetText(reinterpret_cast<const UI::String*>(uiString));  // B.3.6 bridge

        // Restore scroll position from history
        histArray = *(void**)(s + 0x68);
        histIdx = *(int*)(s + 0x64);
        float savedScroll = *(float*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4);
        void* containerElem = *(void**)(s + 0x58);
        reinterpret_cast<UI::ContainerElement*>(containerElem)->SetOffsets(0, (int)savedScroll);  // B.3.6 bridge

        // Update combo to show current page
        int comboIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);
        void* comboElem = *(void**)(s + 0x40);
        reinterpret_cast<UI::ComboElement*>(comboElem)->SetCurrentItem(comboIdx);  // B.3.6 bridge
        return;
    }

    if (id == 0xE77) {
        // Forward button — go to next history entry
        void* firstElem = *(void**)(s + 0x30);
        int histIdx = *(int*)(s + 0x64);
        void* histArray = *(void**)(s + 0x68);

        // Save current scroll position before navigating
        int scrollVal = *(int*)((uintptr_t)firstElem + 0x160);
        *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4) = scrollVal;

        // Move forward one entry
        histIdx++;
        *(int*)(s + 0x64) = histIdx;

        // Get page index from history
        int pageIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);

        // Look up StringTable from ExternalList
        void* extList = GetHelpExternalList();
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* stringTable = *(void**)((uintptr_t)resourceArray + (int64_t)pageIdx * 8);

        bool isStringTable = ::Object::IsDerivedFromNullSafe(stringTable, UI::StringTable::TypeId());
        void* validTable = isStringTable ? stringTable : nullptr;

        // Construct UI::String with text index 2 (content) and set on document
        uint8_t uiString[16] = {0};
        ::new (uiString) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>((uint64_t)validTable)), 2);  // B.3.6 bridge

        void* docElem = *(void**)(s + 0x38);
        reinterpret_cast<UI::DocumentTextElement*>(docElem)->SetText(reinterpret_cast<const UI::String*>(uiString));  // B.3.6 bridge

        // Restore scroll position from history
        histArray = *(void**)(s + 0x68);
        histIdx = *(int*)(s + 0x64);
        float savedScroll = *(float*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4);
        void* containerElem = *(void**)(s + 0x58);
        reinterpret_cast<UI::ContainerElement*>(containerElem)->SetOffsets(0, (int)savedScroll);  // B.3.6 bridge

        // Update combo to show current page
        int comboIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);
        void* comboElem = *(void**)(s + 0x40);
        reinterpret_cast<UI::ComboElement*>(comboElem)->SetCurrentItem(comboIdx);  // B.3.6 bridge
        return;
    }
}

// ============================================================
// HelpUI::Slot5 (vtable +0x28) — No-op
// ============================================================
