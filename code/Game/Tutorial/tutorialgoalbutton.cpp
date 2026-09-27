// code/Game/Goal/tutorialgoalbutton.cpp
// Clean reimplementation of TutorialGoalButton vtable methods.
//
// TutorialGoalButton (0x30 bytes) — tutorial goal that waits for a UI element state.
// Vtable: 0x140160800
// Extends TutorialGoal -> Object::External
//
// Fields (from Ghidra decompilation):
//   +0x20: System::String elementPath — dotted path to UI element (e.g. "DialogType.ElementName")
//   +0x28: bool invertCheck — selects which bit of element flags to check
//
// Check logic (Slot8):
//   Finds UI element via FUN_1400e1870 path lookup.
//   If found, wraps in InternalBasePtr, checks IsDerivedFrom(Object::Internal).
//   Reads flags at element+0x34:
//     invertCheck=true:  returns ~(flags >> 23) & 1  (bit 23 NOT set = success)
//     invertCheck=false: returns (flags >> 22) & 1   (bit 22 set = success)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialGoalButton.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B; InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "ui/Element.h"
#include <cstdint>
#include <new>

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400dee90 (inlined in create 0x140027290 / createInPlace 0x140027300):
// External(), the TutorialGoal and TutorialGoalButton stamps (the compiler's),
// then the two fields zeroed.
TutorialGoalButton::TutorialGoalButton() : elementPath(0), invertCheck(0), _pad29{} {}

// The dtor inside [0] 0x1400273b0: ~String on +0x20; the compiler re-stamps
// TutorialGoal and runs ~External.
TutorialGoalButton::~TutorialGoalButton() {
    reinterpret_cast<System::String*>(&elementPath)->~String();   // B.3.1 bridge
}

// [0] 0x1400273b0: ~TutorialGoalButton, MemFree on &1.
Object::Base* TutorialGoalButton::DeletingDtor(unsigned flags) {
    this->~TutorialGoalButton();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// TutorialGoalButton::Serialize (vtable +0x28, 0x1400deda0)
// ============================================================
// 1. TutorialGoal::Serialize (its empty Serializer scope)
// 2. a Serializer over the stream: String +0x20, bool +0x28
bool TutorialGoalButton::Serialize(void* stream) {
    TutorialGoal::Serialize(stream);   // QUALIFIED: the base body

    // Create serializer for our fields
    alignas(8) uint8_t ser[32];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize elementPath (+0x20)
    reinterpret_cast<System::String*>(&elementPath)->Serialize(ser);   // B.3.1 bridge

    // Serialize invertCheck (+0x28) — bool
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(&invertCheck), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// TutorialGoalButton::Reconstruct [6] 0x140027350 / Destruct [7] 0x140001640
// ============================================================
// DeletingDtor(0) (destroy in place), then the ctor on `this`.
Object::External* TutorialGoalButton::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) TutorialGoalButton();
}
void TutorialGoalButton::Destruct() { DeletingDtor(0); }

// ============================================================
// TutorialGoalButton::IsComplete (vtable +0x40, 0x1400dece0)
// ============================================================
// Finds the UI element by path, checks its flag bits.
bool TutorialGoalButton::IsComplete(void* /*time*/, void* /*instance*/) {
    // Find UI element by elementPath (+0x20)
    uintptr_t elementIBP = 0;
    if (fn_FindUIElementByPath) {
        fn_FindUIElementByPath(&elementIBP, static_cast<void*>(&elementPath), nullptr);
    }

    if (elementIBP == 0) {
        return false;
    }

    // Check if element IsDerivedFrom UI::Element
    uintptr_t element = elementIBP;
    if (!fn_IsDerivedFrom) {
        return false;
    }

    bool isDerived = fn_IsDerivedFrom((void*)element, UI::Element::TypeId());
    void* validElement = isDerived ? (void*)element : nullptr;

    // Wrap in another InternalBasePtr
    uintptr_t wrappedIBP = 0;
    if (fn_InternalBasePtr_Ctor) {
        new (&wrappedIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(validElement));   // B.3.3 bridge
    }

    if (wrappedIBP == 0) {
        return false;
    }

    // Read flags from element+0x34
    uintptr_t elem = wrappedIBP;
    uint32_t flags = *(uint32_t*)(elem + 0x34);

    if (invertCheck != 0) {
        // Bit 23 NOT set = success
        return ((~(flags >> 23)) & 1) != 0;
    }
    // Bit 22 set = success
    return ((flags >> 22) & 1) != 0;
}

// ============================================================
// TutorialGoalButton::Construct (vtable +0x48, 0x140027300)
// ============================================================
Object::External* TutorialGoalButton::Construct() {
    return ::new (static_cast<void*>(this)) TutorialGoalButton();
}

// create 0x140027290: MemAlloc(0x30, 8), then the (inlined) ctor.
void* TutorialGoalButton::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialGoalButton), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialGoalButton());
}
// createInPlace 0x140027300.
void* TutorialGoalButton::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialGoalButton());
}

DEFINE_EXE_VTABLE(TutorialGoalButton, "_ZTV18TutorialGoalButton")
DEFINE_EXE_TYPE(TutorialGoalButton, "TutorialGoal")
