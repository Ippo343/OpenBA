// code/Game/Component/uicomponent.cpp
// UIComponent (0x38 bytes), vtable at 0x1401649f0. Slot [3] (Persist) lives in
// uicomponent_serialize.cpp; slots [5]-[15] are Component's shared no-op bodies
// (inherited, B.3.8 CB1b deleted the unused UIComponent_SlotN copies).

// ============================================================================
// ENDGAME B.3.8 VIS — UIComponent as a REAL class (UIComponent.h).
//
// Before: COMPONENT_INERT_FACTORY_SER — a 48-slot table filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/UIComponent.h"

// 0x14012de00: Component() is the CALL 0x1401188b0; the 0x1401649f0 stamp is
// the compiler's; +0x28 = +0x30 = 0; flags |= 0x80.
UIComponent::UIComponent() : uiData0(nullptr), uiData1(nullptr) {
    OrFlagsInCtor(0x80);
}

// [3] 0x14012dd00 — UIComponent::Persist lives in uicomponent_serialize.cpp.

// [4] 0x14012dca0. PORT GAP (retail 0x14012dca0) — B38 D9: retail has its own
// Startup body; the tree's table never installed one (inert stub). The inert
// behaviour is kept here; porting it is its own titled commit.
void UIComponent::Startup() {}

// [0] 0x140039760: ~UIComponent (0x14012dda0; its array free is a PORT GAP, see the header), MemFree on &1.
Object::Base* UIComponent::DeletingDtor(unsigned flags) {
    this->~UIComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140039550: MemAlloc(0x38, 8), then the ctor.
void* UIComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(UIComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) UIComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* UIComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) UIComponent());
}

DEFINE_EXE_VTABLE(UIComponent, "_ZTV11UIComponent")
DEFINE_EXE_TYPE(UIComponent, "Component")
