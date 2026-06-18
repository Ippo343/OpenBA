// code/Game/Component/frustumoutlineelement.cpp
// Standalone factory for FrustumOutlineElement (factory cascade).
//
// FrustumOutlineElement (size 0xd8) extends UI::Element. The exe factory
// (0x140012400): MemAlloc(0xd8,8) -> UI::Element::Element -> install
// vtable_FrustumOutlineElement (0x14015c2a0) -> UI::Sprite::Sprite(this+0xc0)
// (default ctor) -> *(this+0xd0) = -1.
//
// DORMANT-SAFE FACTORY: we run the real UI::Element + UI::Sprite base ctors and
// set the type field, but DO NOT install the exe subclass vtable (it is a large
// Element vtable of exe->Runtime thunks that don't resolve standalone). The
// object keeps the working UI::Element base vtable that fn_Element_Ctor installs.
// Divergence: GetTypeId reports Element, and the subclass draw/layout overrides
// are not applied. This is acceptable for landmine removal — no current path
// instantiates FrustumOutlineElement; it is registered for completeness, not
// driven. To fully activate, reconstruct the 0x14015c2a0 subclass vtable.

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>

static void* FrustumOutlineElement_Build(void* p) {
    if (!p) return nullptr;
    if (fn_Element_Ctor) fn_Element_Ctor(p);
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    if (fn_Sprite_DefaultCtor2) fn_Sprite_DefaultCtor2(s + 0xc0);
    *(int64_t*)(s + 0xd0) = -1;
    return p;
}
extern "C" void* FrustumOutlineElement_CreateInPlace_SA(void* mem) {
    return FrustumOutlineElement_Build(mem);
}
extern "C" void* FrustumOutlineElement_Factory_SA(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0xd8, 8);
    return FrustumOutlineElement_Build(p);
}
REGISTER_TYPE(FrustumOutlineElement, "Element",
              &FrustumOutlineElement_Factory_SA, &FrustumOutlineElement_CreateInPlace_SA);
#endif // STANDALONE_EXE
