// code/Game/Component/captureboxelement.cpp
// Standalone factory for CaptureBoxElement (factory cascade).
//
// CaptureBoxElement (size 0xe0) extends UI::Element. The exe factory
// (0x140013eb0): MemAlloc(0xe0,8) -> UI::Element::Element -> install
// vtable_CaptureBoxElement (0x14015d440) -> UI::Sprite::Sprite(this+0xc0)
// (default ctor) -> *(this+0xd0) = 0, *(this+0xd8) = 0.
//
// DORMANT-SAFE FACTORY: runs the real UI::Element + UI::Sprite base ctors and
// zeroes the type fields, but does NOT install the exe subclass vtable (large
// Element vtable of exe->Runtime thunks that don't resolve standalone). The
// object keeps the working UI::Element base vtable from fn_Element_Ctor.
// Divergence: GetTypeId reports Element; subclass overrides not applied.
// Acceptable for landmine removal — no current path instantiates this; it is
// registered for completeness. To fully activate, rebuild the 0x14015d440 vtable.

#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <new>
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K4)

static void* CaptureBoxElement_Build(void* p) {
    if (!p) return nullptr;
    if (fn_Element_Ctor) fn_Element_Ctor(p);
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    ::new (s + 0xc0) UI::Sprite();   // B.3.6: value-type ctor in place (B6-10)
    *(int64_t*)(s + 0xd0) = 0;
    *(int64_t*)(s + 0xd8) = 0;
    return p;
}
extern "C" void* CaptureBoxElement_CreateInPlace_SA(void* mem) {
    return CaptureBoxElement_Build(mem);
}
extern "C" void* CaptureBoxElement_Factory_SA(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0xe0, 8);
    return CaptureBoxElement_Build(p);
}
REGISTER_TYPE(CaptureBoxElement, "Element",
              &CaptureBoxElement_Factory_SA, &CaptureBoxElement_CreateInPlace_SA);
