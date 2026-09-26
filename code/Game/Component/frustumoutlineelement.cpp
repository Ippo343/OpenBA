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

// graphics/Camera.h FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once
// <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Camera.h"
#include <new>
#include "video/Context.h"        // ENDGAME B.3.6 UI callers (K4): Video::Viewport by value
#include "ui/Element.h"          // ENDGAME B.3.6 UI callers (K4)
#include "ui/Sprite.h"           // ENDGAME B.3.6 UI callers (K4)
#include "ui/SpriteContext.h"    // ENDGAME B.3.6 UI callers (K4)
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "math/Math.h"
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

// UI::Element::GetAlignedPosition (member overload, Runtime 0x180115ff0). Computes the
// element's aligned screen rect from its size (+0x60/+0x64), margins (+0x18..+0x24) and
// alignment (+0x30) within the parent-allocated (x,y,w,h).

// Slot 5 (+0x28) — ComputeLayout (exe 0x14006c1c0). Faithful port: run the base
// UI::Element::ComputeLayout, then compute the aligned screen rect via GetAlignedPosition
// and store it at +0x68 (left) / +0x6c (top) / +0x70 (right) / +0x74 (bottom). The outline
// Draw reads +0x68/+0x6c as the absolute pixel offset for its lines and scissor — without
// this write they stay 0 and the frustum draws relative to screen origin instead of the
// inset minimap rect (renders too high/left).
static void Frustum_ComputeLayout(void* self, int x, int y, int w, int h) {
    // base — QUALIFIED: this body IS slot 5 of the hand-built FrustumOutlineElement table.
    reinterpret_cast<UI::Element*>(self)->UI::Element::ComputeLayout(x, y, w, h);  // B.3.6 bridge
    int outX = 0, outY = 0, outW = 0, outH = 0;
    reinterpret_cast<UI::Element*>(self)->GetAlignedPosition(  // B.3.6 bridge
        x, y, w, h, 0, 0, outX, outY, outW, outH, false, false);
    uint8_t* e = reinterpret_cast<uint8_t*>(self);
    *reinterpret_cast<int*>(e + 0x68) = outX;
    *reinterpret_cast<int*>(e + 0x70) = outX + outW;
    *reinterpret_cast<int*>(e + 0x6c) = outY;
    *reinterpret_cast<int*>(e + 0x74) = outY + outH;
}

// ===========================================================================
// Camera-frustum ground-projection — port of FrustumOutlineElement_helper_c5b0
// (exe 0x14006c5b0). Projects the camera view-frustum's 4 FAR corners onto the
// z=0 ground plane, writing 4 world-space Vector3 points to out[0..3] (12 bytes
// each). Registered at 0x14006c5b0 (see frustumoutlinedescription.cpp) so
// fn_MapUI_ProjectCornersToGround (minimap click-to-move-camera) resolves too.
// ===========================================================================
typedef void  (*Frustum_fnGetCorners_t)(void* frustum, void* out8Vec3);

static Frustum_fnGetCorners_t s_getCorners = nullptr;
static bool s_frustumMathReady = false;

// These are plain Runtime engine exports; resolve them by name directly. (The
// exe reaches them through .rdata dispatch slots 0x1401518a8/a0, 0x140151860.
// Vector3::Normalize / Transform3::Rotate / Vector3::Dot — slots 0x1401512c0,
// 0x140151298 — are now direct Math:: class calls.)
static void EnsureFrustumMathFns() {
    if (s_frustumMathReady) return;
    s_frustumMathReady = true;
    s_getCorners = reinterpret_cast<Frustum_fnGetCorners_t>(
        ResolveExport("?GetCorners@Frustum@Volume@@QEBAXQEAVVector3@Math@@@Z"));
}

extern "C" void FrustumOutline_ProjectCorners(float* out) {
    if (!out) return;
    EnsureFrustumMathFns();
    if (!s_getCorners) return;
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    uint8_t* cam = *reinterpret_cast<uint8_t**>(reinterpret_cast<uintptr_t>(gw) + 0xe0);
    if (!cam) return;

    // Camera Transform3 (rotation + translation, 0x30 bytes) at cam+0x98;
    // translation Vector3 at cam+0xbc. Copy the transform so Rotate reads a
    // stable object.
    const Math::Transform3 xform = *reinterpret_cast<const Math::Transform3*>(cam + 0x98);   // B.3.1 bridge
    const Math::Vector3 trans(*reinterpret_cast<float*>(cam + 0xbc),
                              *reinterpret_cast<float*>(cam + 0xc0),
                              *reinterpret_cast<float*>(cam + 0xc4));

    // 8 view-frustum corners (camera object at cam+0x34), in camera space.
    // Camera::GetFrustum is a hidden-retslot door in retail; the door body moves
    // to the call site as exactly ONE construction into the same caller storage.
    alignas(::Frustum) uint8_t frustum[0x80];
    void* fr = ::new (static_cast<void*>(frustum)) ::Frustum(
        reinterpret_cast<const Graphics::Camera*>(cam + 0x34)->GetFrustum());   // B.3.5 bridge
    Math::Vector3 corners[8];
    s_getCorners(fr, corners);

    // Far corners 4..7 -> normalized -> rotated into world-space ray directions.
    Math::Vector3 dir[4];
    for (int i = 4; i < 8; ++i) {
        Math::Vector3 nrm = corners[i].Normalize();
        Math::Vector3 rot = xform.Rotate(nrm);
        dir[i - 4] = rot;
    }

    // Ground plane z=0: t = -trans.z / dir.z ; point = trans + t*dir.
    // (The exe scales t by DAT_14017214c == 1.0f.)
    const Math::Vector3 up(0.0f, 0.0f, 1.0f);
    const float negTransZ = -up.Dot(trans).v;
    for (int i = 0; i < 4; ++i) {
        const float dz = up.Dot(dir[i]).v;
        if (dz == 0.0f) continue;
        float t = (1.0f / dz) * negTransZ;
        out[i * 3 + 0] = trans.x + t * dir[i].x;
        out[i * 3 + 1] = trans.y + t * dir[i].y;
        out[i * 3 + 2] = trans.z + t * dir[i].z;
    }
}

// Slot 6 (+0x30) — Draw. Port of FrustumOutlineElement_Draw (exe 0x14006be90):
// draws the camera view-frustum footprint as a 4-line wire quad clipped to the
// minimap rect. (AddLine no-ops on a spriteless sprite — CreateElement wires the
// element's line sprite from the description; see frustumoutlinedescription.cpp.)
static void Frustum_Draw(void* self, void* ctx, uint8_t alpha) {
    uint8_t* e = reinterpret_cast<uint8_t*>(self);
    // base element draw — QUALIFIED: this body IS slot 6 of the hand-built table.
    reinterpret_cast<const UI::Element*>(self)->UI::Element::Draw(  // B.3.6 bridge
        *reinterpret_cast<UI::SpriteContext*>(ctx), alpha);
    if (!self || !ctx) return;

    // Line color = element+0x38 modulated by alpha (halved if flag bit 18 set).
    uint32_t a32 = alpha;
    if (*reinterpret_cast<uint32_t*>(e + 0x34) & 0x40000) a32 = (uint32_t)(alpha >> 1);
    uint32_t v = *reinterpret_cast<uint32_t*>(e + 0x38);
    uint32_t color = ((((v >> 8) & 0xffff0000u) * a32 ^ v) & 0xffffffu)
                     ^ (((v >> 8) & 0xff0000u) * a32);

    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;

    float pts[4 * 3] = {0};
    FrustumOutline_ProjectCorners(pts);

    // Map the world-space ground corners to minimap pixel coordinates.
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    int   width   = *reinterpret_cast<int*>(e + 0xd0);
    int   height  = *reinterpret_cast<int*>(e + 0xd4);
    int*  gridPtr = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gw) + 0xf8);
    if (!gridPtr || *gridPtr == 0) return;
    float scale = (float)width / (float)(*gridPtr);
    float halfW = (float)width * 0.5f;
    float halfH = (float)height * 0.5f;
    for (int i = 0; i < 4; ++i) {
        float sx = scale * pts[i * 3 + 0] + halfW;
        float sy = scale * pts[i * 3 + 1] + halfH;
        pts[i * 3 + 0] = sx;
        pts[i * 3 + 1] = ((float)width - uvScale) - sy;   // flip Y into image space
        pts[i * 3 + 2] = 0.0f;
    }

    int ex = *reinterpret_cast<int*>(e + 0x68);   // element origin X
    int ey = *reinterpret_cast<int*>(e + 0x6c);   // element origin Y
    void* sprite = e + 0xc0;
    auto ix = [&](int i){ return (int)pts[i * 3 + 0] + ex; };
    auto iy = [&](int i){ return (int)pts[i * 3 + 1] + ey; };

    UI::SpriteContext* sc = reinterpret_cast<UI::SpriteContext*>(ctx);  // B.3.6 bridge
    const UI::Sprite& lineSprite = *reinterpret_cast<const UI::Sprite*>(sprite);  // B.3.6 bridge
    uint8_t savedVP[24] = {0}, scratchVP[24] = {0};
    ::new (savedVP) Video::Viewport(sc->SetScissor(ex, ey, width, height));

    // Wire quad, corner order pt0-pt1-pt3-pt2-pt0 (matches the exe's 4 AddLines).
    sc->AddLine(lineSprite, ix(0), iy(0), ix(1), iy(1), color);
    sc->AddLine(lineSprite, ix(1), iy(1), ix(3), iy(3), color);
    sc->AddLine(lineSprite, ix(2), iy(2), ix(3), iy(3), color);
    sc->AddLine(lineSprite, ix(2), iy(2), ix(0), iy(0), color);

    ::new (scratchVP) Video::Viewport(
        sc->SetScissor(*reinterpret_cast<const Video::Viewport*>(savedVP)));
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
    ::new (s + 0xc0) UI::Sprite();   // B.3.6: value-type ctor in place (B6-10)
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
