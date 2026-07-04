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

// ---------------------------------------------------------------------------
// Standalone FrustumOutlineElement vtable.
//
// The old factory kept the base UI::Element vtable that fn_Element_Ctor installs.
// UI::Element is ABSTRACT: its vtable slots 4/5/6 (+0x20 ComputeSize, +0x28
// ComputeLayout, +0x30 Draw) are pure-virtual (_purecall). Every concrete element
// (ImageElement, TextElement, and the exe's FrustumOutlineElement) overrides them.
// When the minimap's Ribbon/GroupElement lays out its children it calls each
// child's `vtable[+0x20]` (ComputeSize) — for a frustum on the base vtable that hit
// _purecall → the R6025 "Runtime Error!" MessageBox (verified under GDB:
// GroupElement::ComputeSize → child vtable+0x20 → _purecall → MessageBoxW).
//
// So we build a per-type standalone vtable = a copy of the base Element vtable with
// slots 4/5/6 pointed at real implementations (ports of exe 0x14006c290 /
// 0x14006c1c0 / 0x14006be90). Slots 0/1/2/3 and 7.. keep the base (concrete)
// entries. Draw is a safe no-op for now — the map (imageMap/imageOverlay) renders;
// the camera-frustum outline is a cosmetic follow-up (needs the frustum-projection
// deps wired).
static uintptr_t g_FrustumOutlineElement_Vtable[24];
static bool      g_FrustumVtableReady = false;

// Slot 4 (+0x20) — ComputeSize (exe 0x14006c290). If the "size-pending" flag
// (bit 27 of +0x34) is set, copy the element's intrinsic width/height (+0xd0/+0xd4)
// into its size fields (+0x60/+0x64) and clear the flag. Pure field arithmetic —
// no engine deps. (RibbonElement::ComputeSize clears the same bit 27 after it.)
static void Frustum_ComputeSize(void* self) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint32_t flags = *reinterpret_cast<uint32_t*>(s + 0x34);
    if ((flags >> 27) & 1u) {
        *reinterpret_cast<uint32_t*>(s + 0x60) = *reinterpret_cast<uint32_t*>(s + 0xd0);
        *reinterpret_cast<uint32_t*>(s + 0x34) = flags & ~(1u << 27);
        *reinterpret_cast<uint32_t*>(s + 0x64) = *reinterpret_cast<uint32_t*>(s + 0xd4);
    }
}

// Slot 5 (+0x28) — ComputeLayout (exe 0x14006c1c0). Delegate to the concrete base
// UI::Element::ComputeLayout (a real Runtime export even though the vtable slot is
// pure). The frustum-specific screen-rect write (+0x68..+0x74) only feeds the
// outline Draw, which is a no-op here, so it is omitted.
static void Frustum_ComputeLayout(void* self, int x, int y, int w, int h) {
    if (fn_Element_ComputeLayout) fn_Element_ComputeLayout(self, x, y, w, h);
}

// Slot 6 (+0x30) — Draw. No-op (see header comment).
static void Frustum_Draw(void* self, void* ctx, uint8_t alpha) {
    (void)self; (void)ctx; (void)alpha;
}

static void* FrustumOutlineElement_Build(void* p) {
    if (!p) return nullptr;
    if (fn_Element_Ctor) fn_Element_Ctor(p);
    // Build the standalone vtable once from the base Element vtable the ctor just
    // installed, then override slots 4/5/6.
    if (!g_FrustumVtableReady) {
        uintptr_t base = *reinterpret_cast<uintptr_t*>(p);
        if (base) {
            const uintptr_t* bv = reinterpret_cast<const uintptr_t*>(base);
            for (int i = 0; i < 24; ++i) g_FrustumOutlineElement_Vtable[i] = bv[i];
            g_FrustumOutlineElement_Vtable[4] = reinterpret_cast<uintptr_t>(&Frustum_ComputeSize);
            g_FrustumOutlineElement_Vtable[5] = reinterpret_cast<uintptr_t>(&Frustum_ComputeLayout);
            g_FrustumOutlineElement_Vtable[6] = reinterpret_cast<uintptr_t>(&Frustum_Draw);
            g_FrustumVtableReady = true;
        }
    }
    if (g_FrustumVtableReady)
        *reinterpret_cast<uintptr_t*>(p) = reinterpret_cast<uintptr_t>(g_FrustumOutlineElement_Vtable);
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
