#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// TutorialHighlightDialog::Startup (0x1400E0020)
// Finds "glowGroup", "glow", "arrow" elements by name and stores them
// at +0xC0, +0xC8, +0xD0 respectively.
void TutorialHighlightDialog_Startup(TutorialHighlightDialog* self) {
    uintptr_t s = (uintptr_t)self;

    struct { const char* name; uintptr_t offset; } elements[] = {
        { "glowGroup", 0xC0 },
        { "glow",      0xC8 },
        { "arrow",     0xD0 },
    };

    for (auto& e : elements) {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, e.name);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        fn_String_Dtor(nameStr);

        // Store the found element pointer at the target offset
        void* elem = *(void**)retBuf;
        *(void**)(s + e.offset) = elem;
    }
}

// TutorialHighlightDialog::Load (0x1400e0000) — vtable slot +0x38
// Original is 11 bytes: MOV RAX,[RDX]; ADD [RCX+0xB8],RAX; RET
// Accumulates a delta-time tick count (pointed to by param2) into dataB8,
// which is a System::Time accumulator used by Update for animation timing.
void TutorialHighlightDialog_Load(TutorialHighlightDialog* self, void* param2) {
    self->dataB8 += *(uint64_t*)param2;
}

// ===========================================================================
// The two positioning helpers behind slot 6, and slot 6 itself.
//
// Field map (TutorialHighlightDialog, 0xF0, base UI::DialogInstance):
//   +0x10  UI::UserInterface*        (base) — +0xDC on it is the UI scale
//   +0xB8  System::Time              accumulated dt (slot 7 adds to it)
//   +0xC0  IBP UI::Element*          "glowGroup"
//   +0xC8  IBP UI::ImageElement*     "glow"
//   +0xD0  IBP UI::ImageElement*     "arrow"
//   +0xD8  int32                     arrow rotation, degrees (0/90/180/270)
//   +0xE0  IBP UI::Element*          UI-element target   (element-path branch)
//   +0xE8  IBP entity                world-entity target (toolbar-cell branch)
//
// Constants read out of .rdata: 0x140172114 = 0.5f, 0x140172184 = pi,
// 0x1401721a8 = 8.0f. FUN_140131c90 is sinf (an inlined CRT sin), so the
// arrow's bob is  sinf(elapsedSeconds * pi) * 8  pixels.
// ===========================================================================

#include "resolve.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace {

// Volume::Rectangle::Rectangle() — the same export tool_base_screenrect.cpp uses.
typedef void* (*fn_RectCtor_t)(void* self);
// 0x140100380 — entity screen bounding rectangle (ported in
// code/Game/Player/tool_base_screenrect.cpp, registered by reg_player_screenrect.cpp).
// param_1 is unused by the body; retail passes the rectangle itself here.
typedef bool (*fn_EntityScreenRect_t)(uintptr_t unused, void* entityIBP, void* rectOut);

fn_RectCtor_t         s_RectCtor         = nullptr;
fn_EntityScreenRect_t s_EntityScreenRect = nullptr;
bool                  s_resolvedHL       = false;

void EnsureHighlightFuncs() {
    if (s_resolvedHL) return;
    s_resolvedHL = true;
    s_RectCtor = reinterpret_cast<fn_RectCtor_t>(
        ResolveExport("??0Rectangle@Volume@@QEAA@XZ"));
    s_EntityScreenRect = reinterpret_cast<fn_EntityScreenRect_t>(
        ResolveAddr(Module::Exe, 0x140100380));
}

// sinf(elapsed * pi) * 8 — the shared bob offset both helpers apply.
float ArrowBob(uintptr_t self) {
    if (!fn_Time_AsSeconds) return 0.0f;
    float t = fn_Time_AsSeconds(reinterpret_cast<void*>(self + 0xB8));
    return std::sin(t * 3.14159274f) * 8.0f;
}

}  // namespace

// ---------------------------------------------------------------------------
// 0x1400DFAC0 — the UI-ELEMENT branch (+0xE8 == 0, target element at +0xE0).
// Grows the glow ring to the element's rect + 13px on every side, parks the
// glowGroup with a four-sided padding measured against the scaled screen, then
// puts the arrow on the corner selected by the rotation field.
// ---------------------------------------------------------------------------
void TutorialHighlightDialog_UpdateElement(TutorialHighlightDialog* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (!fn_InternalBasePtr_CheckPtr || !fn_Element_SetPadding) return;

    void* targetIBP = reinterpret_cast<void*>(s + 0xE0);
    fn_InternalBasePtr_CheckPtr(targetIBP);
    uintptr_t elem = *reinterpret_cast<uintptr_t*>(targetIBP);
    if (!elem) return;

    // Element rect: +0x68 left, +0x6C top, +0x70 right, +0x74 bottom.
    int left   = *reinterpret_cast<int32_t*>(elem + 0x68);
    int top    = *reinterpret_cast<int32_t*>(elem + 0x6C);
    int width  = *reinterpret_cast<int32_t*>(elem + 0x70) - left;
    int height = *reinterpret_cast<int32_t*>(elem + 0x74) - top;

    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0xC8));
    void* glow = *reinterpret_cast<void**>(s + 0xC8);
    if (glow && fn_ImageElement_SetWidthHeight)
        fn_ImageElement_SetWidthHeight(glow, width + 0x1A, height + 0x1A);

    int padLeft = left - 13;
    int padTop  = top  - 13;

    // Screen size from the display singleton (+4 width, +8 height), scaled by
    // the UserInterface's +0xDC factor.
    // Video::Device is behind a DOUBLE deref of the 0x140151a80 exref
    //   MOV RAX,[0x140151a80] ; MOV RCX,[RAX] ; MOVD XMM0,[RCX+4]
    // — one deref short lands on the holder and reads garbage dimensions, which
    // is exactly what mis-sizes and mis-places the glow ring.
    int screenW = 0, screenH = 0;
    if (const char* dbg = std::getenv("BANISHED_TUTORIAL_TRACE")) {
        if (dbg[0] && dbg[0] != '0') {
            static int once = 0;
            if (!once) { once = 1;
                void* holder = g_pSingletonExref_151a80 ? *g_pSingletonExref_151a80 : nullptr;
                void* dev = holder ? *reinterpret_cast<void**>(holder) : nullptr;
                fprintf(stderr, "[Tutorial] display exref=%p holder=%p dev=%p uiHolder=%p\n",
                        (void*)(void*)g_pSingletonExref_151a80, holder, dev,
                        *reinterpret_cast<void**>(s + 0x10));
                fflush(stderr); }
        }
    }
    if (g_pSingletonExref_151a80) {
        if (void* holder = *g_pSingletonExref_151a80) {
            if (void* device = *reinterpret_cast<void**>(holder)) {
                screenW = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(device) + 4);
                screenH = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(device) + 8);
            }
        }
    }
    float scale = 1.0f;
    uintptr_t ui = *reinterpret_cast<uintptr_t*>(s + 0x10);
    if (ui) scale = *reinterpret_cast<float*>(ui + 0xDC);

    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0xC0));
    void* glowGroup = *reinterpret_cast<void**>(s + 0xC0);
    if (glowGroup) {
        fn_Element_SetPadding(glowGroup,
                              padLeft,
                              (static_cast<int>(static_cast<float>(screenW) * scale) - padLeft) - width - 0x1A,
                              padTop,
                              (static_cast<int>(static_cast<float>(screenH) * scale) - padTop) - height - 0x1A);
        if (fn_Element_SetNeedsSizeCalc) fn_Element_SetNeedsSizeCalc(glowGroup);
        if (const char* e = std::getenv("BANISHED_TUTORIAL_TRACE")) {
            if (e[0] && e[0] != '0') {
                static int s_n = 0;
                if (s_n < 3) {
                    ++s_n;
                    fprintf(stderr, "[Tutorial] glow rect=(%d,%d) %dx%d screen=%dx%d scale=%f "
                                    "pad=(%d,%d,%d,%d)\n",
                            left, top, width, height, screenW, screenH, (double)scale,
                            padLeft,
                            (int)((int)((float)screenW * scale) - padLeft) - width - 0x1A,
                            padTop,
                            (int)((int)((float)screenH * scale) - padTop) - height - 0x1A);
                    fflush(stderr);
                }
            }
        }
    }

    // Arrow: corner + sign pair chosen by the rotation.
    void* arrowIBP = reinterpret_cast<void*>(s + 0xD0);
    fn_InternalBasePtr_CheckPtr(arrowIBP);
    void* arrow = *reinterpret_cast<void**>(arrowIBP);
    if (!arrow || !fn_ImageElement_GetSprite || !fn_Sprite_Width) return;

    uint8_t sprite[16] = {};
    fn_ImageElement_GetSprite(arrow, sprite);
    int arrowW = fn_Sprite_Width(sprite);

    int rot = *reinterpret_cast<int32_t*>(s + 0xD8);
    int x, y, signX, signY;
    if (rot == 90) {
        x = left - arrowW;                                    signX = 1;
        y = *reinterpret_cast<int32_t*>(elem + 0x74);         signY = -1;
    } else if (rot == 180) {
        x = *reinterpret_cast<int32_t*>(elem + 0x70);         signX = -1;
        y = *reinterpret_cast<int32_t*>(elem + 0x74);         signY = -1;
    } else if (rot == 270) {
        x = *reinterpret_cast<int32_t*>(elem + 0x70);         signX = -1;
        y = top - arrowW;                                     signY = 1;
    } else {
        x = left - arrowW;                                    signX = 1;
        y = top - arrowW;                                     signY = 1;
    }

    float bob = ArrowBob(s);
    fn_Element_SetPadding(arrow,
                          static_cast<int>(static_cast<float>(signX) * bob + static_cast<float>(x)), 0,
                          static_cast<int>(static_cast<float>(signY) * bob + static_cast<float>(y)), 0);
    if (fn_ImageElement_SetRotation)
        fn_ImageElement_SetRotation(arrow, static_cast<float>(-rot));
}

// ---------------------------------------------------------------------------
// 0x1400DFDA0 — the WORLD-ENTITY branch (+0xE8 holds an entity IBP).
// The glow ring is meaningless on a world object, so the group is hidden and
// only the arrow is placed, over the centre of the entity's screen rect. When
// the entity has no projectable rect (off screen / no model) the arrow hides
// too — that is the tutorial's "the thing I am pointing at is not visible".
// ---------------------------------------------------------------------------
void TutorialHighlightDialog_UpdateWorld(TutorialHighlightDialog* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    EnsureHighlightFuncs();
    if (!fn_InternalBasePtr_CheckPtr || !fn_Element_SetFlagsOnOff) return;

    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0xC0));
    if (void* glowGroup = *reinterpret_cast<void**>(s + 0xC0))
        fn_Element_SetFlagsOnOff(glowGroup, 1, 0);   // hide

    // Volume::Rectangle rect; ok = EntityScreenRect(rect, &entityIBP, &rect)
    alignas(16) uint8_t rect[16] = {};
    if (s_RectCtor) s_RectCtor(rect);
    uint64_t entityIBP = *reinterpret_cast<uint64_t*>(s + 0xE8);
    bool ok = s_EntityScreenRect &&
              s_EntityScreenRect(reinterpret_cast<uintptr_t>(rect), &entityIBP, rect);

    void* arrowIBP = reinterpret_cast<void*>(s + 0xD0);
    fn_InternalBasePtr_CheckPtr(arrowIBP);
    void* arrow = *reinterpret_cast<void**>(arrowIBP);
    if (!ok) {
        if (arrow) fn_Element_SetFlagsOnOff(arrow, 1, 0);   // hide
        return;
    }
    if (!arrow) return;
    fn_Element_SetFlagsOnOff(arrow, 0, 1);                  // show

    if (!fn_ImageElement_GetSprite || !fn_Sprite_Width || !fn_Sprite_Height ||
        !fn_Element_SetPadding) return;

    uint8_t sprite[16] = {};
    fn_ImageElement_GetSprite(arrow, sprite);
    int arrowW = fn_Sprite_Width(sprite);
    fn_ImageElement_GetSprite(arrow, sprite);
    int arrowH = fn_Sprite_Height(sprite);

    // Rectangle is four floats: minX, minY, maxX, maxY.
    const float* r = reinterpret_cast<const float*>(rect);
    float centreX = (r[0] + r[2]) * 0.5f;
    float centreY = (r[1] + r[3]) * 0.5f;

    float bob = ArrowBob(s);
    // Retail multiplies the horizontal bob by 0.0 — the arrow bobs vertically
    // only. Kept literal rather than "simplified" away: the term is in the
    // shipped code and dropping it would hide a future edit to that constant.
    fn_Element_SetPadding(arrow,
                          static_cast<int>(static_cast<float>(static_cast<int>(centreX) - (arrowW >> 1)) + bob * 0.0f), 0,
                          static_cast<int>(static_cast<float>(static_cast<int>(centreY) - arrowH) + bob), 0);
    if (fn_ImageElement_SetRotation)
        fn_ImageElement_SetRotation(arrow, static_cast<float>(-*reinterpret_cast<int32_t*>(s + 0xD8)));
}

// TutorialHighlightDialog::Draw (0x1400DFFE0) — vtable slot 6.
// Retail is the dispatch plus an inlined copy of the 0x1400DFDA0 body.
void TutorialHighlightDialog_Draw(TutorialHighlightDialog* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<uint64_t*>(s + 0xE8) == 0)
        TutorialHighlightDialog_UpdateElement(self);
    else
        TutorialHighlightDialog_UpdateWorld(self);
}
