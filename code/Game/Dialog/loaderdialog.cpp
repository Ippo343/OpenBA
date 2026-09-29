#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/LoaderDialog.h"
#include "ui/UserInterface.h"
#include "runtime/types/ui/DialogInstance.h"
#include "object/TypeInfo.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Time
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// ENDGAME B.3.8 block DLG: LoaderDialog is a real class (Dialog/LoaderDialog.h)
// with a compiler-generated vtable; retail evidence per slot is in the header.

// ============================================================
// [4] OnOpened (0x1400B2D90) — Setup
// ============================================================
// Finds "loadingImage" element and stores pointer at +0xB8.
void LoaderDialog::OnOpened() {
    System::String nameStr("loadingImage");
    loadingImage = FindElement(nameStr).Get();
}

// [6] OnUpdateLayout (0x1400B2D00) — Draw
// Applies the current rotation angle to the loading spinner image element.
void LoaderDialog::OnUpdateLayout() {
    if (loadingImage) {
        static_cast<UI::ImageElement*>(loadingImage)->SetRotation(rotation);
    }
}

// [7] OnUpdate (0x1400B2D50) — Update
// Accumulates rotation angle from elapsed time at 180 degrees/sec.
void LoaderDialog::OnUpdate(const System::Time& time) {
    rotation += time.AsSeconds() * 180.0f;
}

// ============================================================
// Construction, destruction, reflection
// ============================================================
// SplashDLT::CreateDialog builds a real LoaderDialog for both
// Dialog/ShiningRock.rsc and Dialog/Loading.rsc. Retail LoaderDialog_Ctor3
// @ 0x140099070:
//   1. (caller allocs 200 bytes already)
//   2. Build a null parent IBP.
//   3. DialogInstance::DialogInstance(this, ui_at_loop+0x60,
//      rsc_at_loop+0x50, parentIBP, 0, 0, 0).
//   4. The vtable store.
//   5. Zero +0xB8 (loadingImage element ptr) and +0xC0 (rotation float).
// Loop layout is DialogLoaderLoopT: m_dialogExternal at +0x50, embedded UI
// at +0x60. (Different from DialogLoopT's +0x30/+0x40.)

#include "Object/factory_registration.h"

LoaderDialog::LoaderDialog() : loadingImage(nullptr), rotation(0.0f) {}

LoaderDialog::LoaderDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                           Object::InternalPtr<UI::DialogInstance> parent)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0),
      loadingImage(nullptr), rotation(0.0f) {}

// create 0x140020bb0: MemAlloc(0xc8, 8), then the ctor.
void* LoaderDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(LoaderDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) LoaderDialog());
}
// createInPlace 0x140020c20 (the tree's in-place build is kept, B8-13).
void* LoaderDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) LoaderDialog());
}


// Public entry called from SplashDLT::CreateDialog (standalone/splash_loop_stubs.cpp):
// alloc, ctor, wrap the result in the caller's IBP.
extern "C" void* LoaderDialog_FromSplashDLT(void* loop, void* retInternalBasePtr) {
    if (!System::MemoryAllocator::_singleton) {   // B3R: the g_pMemAllocSingleton snapshot, read live
        if (retInternalBasePtr) {
            *reinterpret_cast<uint64_t*>(retInternalBasePtr) = 0;
            *(reinterpret_cast<uint64_t*>(retInternalBasePtr) + 1) = 0;
        }
        return retInternalBasePtr;
    }
    void* mem = System::MemoryAllocator::MemAlloc(sizeof(LoaderDialog), 8);
    uintptr_t lp = reinterpret_cast<uintptr_t>(loop);
    void* ui  = reinterpret_cast<void*>(lp + 0x60);
    void* rsc = *reinterpret_cast<void**>(lp + 0x50);
    LoaderDialog* dlg = ::new (mem) LoaderDialog(static_cast<UI::UserInterface*>(ui),
                                                 static_cast<const UI::Dialog*>(rsc),
                                                 Object::InternalPtr<UI::DialogInstance>());
    new (retInternalBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(dlg));   // B.3.3 bridge
    return retInternalBasePtr;
}

DEFINE_EXE_TYPE(LoaderDialog, "UI::DialogInstance")
