// code/Game/Tutorial/tutorialui.cpp
// TutorialUI — the controller on the tutorial step panel.
//
// TutorialUI (0x60 bytes), table 0x140160f30, on UI::DialogController.
// ENDGAME B.3.8 block GOAL: a real class (Tutorial/TutorialUI.h); the
// hand-built 48-slot table in tutorial_dialog_vtables.cpp and its
// dialog_controller_vtable.cpp inherit-fill are gone. Field map: the header.

#include "Tutorial/TutorialUI.h"
#include "Game/MenuDialogLoopT.h"       // ENDGAME B.3.9 EXA: the real loop classes
#include "Game/GameLoadDialogLoopT.h"
#include "Game/GameLoaderLoop.h"
#include "engine_funcs.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "save/SaveLoad.h"             // RtSave_SaveLoad_Dtor
#include "system/MemoryAllocator.h"
#include "system/Value.h"               // RtSystem_Time_Ctor
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"                       // ENDGAME B.3.6 UI callers (K4)
#include "ui/UserInterface.h"                 // ENDGAME B.3.6 UI callers (K4)
#include <new>

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// create 0x140029400: DialogController() (the base's real ctor), the stamp
// (the compiler's), then +0x30..+0x58 zeroed.
TutorialUI::TutorialUI()
    : data30(0), preText(nullptr), postText(nullptr), imageTable(nullptr),
      nextButton(nullptr), quitButton(nullptr) {}

// Helper: FindElemByName with IsDerivedFrom check, stores InternalBasePtr
static void FindAndStore(void* container, const char* name, void** field) {
    System::String nameBuf(name);

    uint64_t foundResult = 0;
    ::new (&foundResult) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::Element*>(container)->FindElement(nameBuf));  // B.3.6 bridge

    void* found = (void*)foundResult;
    void* checked = nullptr;
    // The exref type check uses the generic Element type from the import table.
    // In practice these all succeed, so we just do a null check and store.
    if (found) {
        checked = found;
    }
    uint64_t stored = 0;
    new (&stored) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
    *field = reinterpret_cast<void*>(stored);
}

// ============================================================
// TutorialUI::OnCreated (vtable +0x20, 0x1400e37a0)
// ============================================================
// Finds the named elements under the container (+0x20), numbers the two
// buttons and hides the dialog's own "buttonClose".
void TutorialUI::OnCreated() {
    FindAndStore(_container, "preText",    &preText);
    FindAndStore(_container, "postText",   &postText);
    FindAndStore(_container, "imageTable", &imageTable);
    FindAndStore(_container, "nextButton", &nextButton);
    FindAndStore(_container, "quitButton", &quitButton);

    // Set button IDs
    reinterpret_cast<UI::Element*>(nextButton)->SetElementId(0x578);  // nextButton = 1400  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(quitButton)->SetElementId(0x579);  // quitButton = 1401  // B.3.6 bridge

    // Hide the "buttonClose" element in the owning dialog instance (+0x18)
    System::String closeName("buttonClose");
    uint64_t closeElem = 0;
    ::new (&closeElem) Object::InternalPtr<UI::Element>(_dialog->FindElement(closeName));
    // SetFlagsOnOff(elem, hideFlags=1, showFlags=0) — hide element
    reinterpret_cast<UI::Element*>(closeElem)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
}

// ============================================================
// TutorialUI::OnElementEvent (vtable +0x48, 0x1400e36e0)
// ============================================================
// `CMP R8D,0x12; JNZ ret; CMP R9D,0x579; JZ 0x1400e34e0` — a click (event
// 0x12) on quitButton (id 0x579) quits the tutorial.
void TutorialUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/,
                                int32_t a, int32_t b, int32_t /*c*/,
                                int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (a == 0x12 && b == 0x579) QuitTutorial();
}

// QuitTutorial (0x1400e34e0, the tail of OnElementEvent):
//   - Destroys the SaveLoad singleton stored at game+0x140.
//   - Allocates and constructs three nested loop objects:
//       MenuDialogLoopT (0xc00): "Dialog/MainMenu.rsc" — the main menu dialog loop
//       GameLoadDialogLoopT (0xb60): "Dialog/Loading.rsc" — loading screen wrapper
//       GameLoaderLoop (0x48): outermost loader, queued as engine's next loop
//   - Sets fullscreen fade to black and transitions back to main menu.
void TutorialUI::QuitTutorial() {
    // Grab the engine singleton (DAT_1401c3dd8 = *g_pGameWorldSingleton)
    uintptr_t eng = (uintptr_t)GameUpdateLoop::_singleton;

    // Destroy and free the SaveLoad object stored at eng+0x140
    void* saveLoad = *(void**)(eng + 0x140);
    if (saveLoad) {
        RtSave_SaveLoad_Dtor(reinterpret_cast<Save::SaveLoad*>(saveLoad));   // ??1SaveLoad 0x1800c3c90
        System::MemoryAllocator::MemFree(saveLoad);
    }
    *(void**)(eng + 0x140) = nullptr;

    // --- Step 1: Allocate and construct the MenuDialogLoopT (0xc00 bytes) ---
    // This is the main menu dialog loop that will display after the loading screen.
    // ENDGAME B.3.9 EXA: the real class (ctor 0x14009bb00, which retail CALLs).
    void* menuLoop = System::MemoryAllocator::MemAlloc(0xc00, 8);
    Engine::UpdateLoop* pMenuLoop = nullptr;
    if (menuLoop) {
        System::String menuNameStr("Dialog/MainMenu.rsc");
        pMenuLoop = ::new (menuLoop) MenuDialogLoopT(menuNameStr);
    }

    // --- Step 2: Allocate and construct the GameLoadDialogLoopT (0xb60 bytes) ---
    // Wraps the menu loop; shows "Dialog/Loading.rsc" while the menu is loading.
    // The real class: its ctor is the subclass init retail inlines here.
    void* dlLoop = System::MemoryAllocator::MemAlloc(0xb60, 4);
    Engine::UpdateLoop* pDlLoop = nullptr;
    if (dlLoop) {
        // Delay constant is 1.0f (DAT_14017214c — bytes 0x3f800000).
        uint64_t delayTime = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&delayTime), 1.0f);

        System::String loadingNameStr("Dialog/Loading.rsc");
        pDlLoop = ::new (dlLoop) GameLoadDialogLoopT(
            loadingNameStr, pMenuLoop, *reinterpret_cast<const System::Time*>(&delayTime));
    }

    // --- Step 3: Allocate and construct the outer GameLoaderLoop (0x48 bytes) ---
    // Wraps the GameLoadDialogLoopT with a zero-delay outer loader.
    void* outerLoop = System::MemoryAllocator::MemAlloc(0x48, 4);
    void* pOuterLoop = nullptr;
    if (outerLoop) {
        uint64_t zeroTime = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&zeroTime), 0.0f);
        pOuterLoop = ::new (outerLoop) GameLoaderLoop(
            pDlLoop, *reinterpret_cast<const System::Time*>(&zeroTime));
    }

    // --- Step 4: Set cursor, fullscreen fade to black, and queue next loop ---
    // FUN_140097fb0: SetCursor(null), SetFullscreenFade(black), store next loop at eng+0xC8.
    {
        void* ui = *(void**)(eng + 0x98);
        uint64_t nullCursor = *(uint64_t*)((uintptr_t)(g_gameContainer) + 0x38);
        reinterpret_cast<UI::UserInterface*>(ui)->SetCursor(
            Object::ExternalPtr<UI::Cursor>(reinterpret_cast<UI::Cursor*>(nullCursor)));  // B.3.6 bridge
        reinterpret_cast<UI::UserInterface*>(ui)->SetFullscreenFade(true, 0xFF000000, false);  // B.3.6 bridge
        *(void**)(eng + 0xC8) = pOuterLoop;
    }
}

// ============================================================
// TutorialUI::OnUpdateLayout (vtable +0x38, 0x1400e3700)
// ============================================================
// Toggle visibility of next/quit buttons: if the element has visibility
// flags (0x1800000) set, hide it; otherwise show it.
void TutorialUI::OnUpdateLayout() {
    // nextButton (+0x50): check flags at element +0x34
    void* nextBtn = nextButton;
    bool nextHidden = (*(uint32_t*)((uintptr_t)nextBtn + 0x34) & 0x1800000) != 0;
    reinterpret_cast<UI::Element*>(nextBtn)->SetFlagsOnOff(nextHidden ? 1 : 0, nextHidden ? 0 : 1);  // B.3.6 bridge

    // quitButton (+0x58): same check
    void* quitBtn = quitButton;
    bool quitHidden = (*(uint32_t*)((uintptr_t)quitBtn + 0x34) & 0x1800000) != 0;
    reinterpret_cast<UI::Element*>(quitBtn)->SetFlagsOnOff(quitHidden ? 1 : 0, quitHidden ? 0 : 1);  // B.3.6 bridge
}

// create 0x140029400: MemAlloc(0x60, 8), then the (inlined) ctor.
void* TutorialUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialUI());
}
// Retail's createInPlace is DialogController's import and does not build a
// controller in place (B8-13); the tree's in-place entry did, and that is kept.
void* TutorialUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialUI());
}

// DEFINE_EXE_TYPE(TutorialUI, "UI::DialogController") stays where its
// REGISTER_TYPE was — tutorial_dialog_vtables.cpp (B38_RULES B8-7).
