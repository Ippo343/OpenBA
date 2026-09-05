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
#include "Object/factory_registration.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

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
namespace {

// Member-ABI FindComponent helpers the screen-rect path uses:
//   0x140003f60 -> ModelComponent, 0x140004680 -> MapComponent. Both are
//   (entity, &outIBP) -> &outIBP.
typedef void* (*fn_FindComp_t)(uintptr_t entity, void* outIBP);
// 0x14009f3e0 — grid cell -> world position (fills a real terrain z).
typedef float* (*fn_GridToWorld_t)(int* grid, float* out, float* in, char flag, void* extra);

void TutHLDiag(uintptr_t self, uint64_t entity, bool ok, const float* rect) {
    static int enabled = -1;
    if (enabled < 0) {
        const char* e = std::getenv("BANISHED_TUTHL_DIAG");
        enabled = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (!enabled || !entity) return;

    static fn_FindComp_t    findModel = nullptr;
    static fn_FindComp_t    findMap   = nullptr;
    static fn_GridToWorld_t gridToWorld = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        findModel   = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140003f60));
        findMap     = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140004680));
        gridToWorld = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    }
    if (!findModel || !findMap) return;

    uint64_t modelIBP = 0, mapIBP = 0;
    findModel(entity, &modelIBP);
    findMap(entity, &mapIBP);

    int16_t placed = -1;
    float xf[7] = {0,0,0,0,0,0,0};
    float box[6] = {0,0,0,0,0,0};
    if (modelIBP) {
        placed = *reinterpret_cast<int16_t*>(modelIBP + 0x166);
        for (int i = 0; i < 7; ++i) xf[i]  = *reinterpret_cast<float*>(modelIBP + 0x148 + i * 4);
        for (int i = 0; i < 6; ++i) box[i] = *reinterpret_cast<float*>(modelIBP + 0x5c + i * 4);
    }

    int rx = 0, ry = 0, rw = 0, rh = 0;
    if (mapIBP) {
        rx = *reinterpret_cast<int16_t*>(mapIBP + 0x28);
        ry = *reinterpret_cast<int16_t*>(mapIBP + 0x2a);
        rw = *reinterpret_cast<int16_t*>(mapIBP + 0x2c);
        rh = *reinterpret_cast<int16_t*>(mapIBP + 0x2e);
    }

    // Footprint centre through the same grid->world conversion the outline uses.
    int   g = 0;
    float centreWorld[3] = {0,0,0};
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton && gridToWorld) {
        int* grid = *reinterpret_cast<int**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
        if (grid) {
            g = grid[0];
            float bias = static_cast<float>(g >> 1) - 0.5f;
            float in[3] = { static_cast<float>(rx + (rw >> 1)) - bias,
                            static_cast<float>(ry + (rh >> 1)) - bias, 0.0f };
            gridToWorld(grid, centreWorld, in, '\0', nullptr);
        }
    }

    // Only reprint when something actually changes.
    static uint64_t lastEntity = 0;
    static int lastRect[4] = { -12345, 0, 0, 0 };
    if (entity == lastEntity && rx == lastRect[0] && ry == lastRect[1] &&
        rw == lastRect[2] && rh == lastRect[3]) return;
    lastEntity = entity;
    lastRect[0] = rx; lastRect[1] = ry; lastRect[2] = rw; lastRect[3] = rh;

    // Type names (vtable+0x08) so we know we are looking at the components we
    // think we are, and the MeshInstance's OWN transform (model+0x28, +0x84) —
    // that is what the renderer actually draws from, so it is the oracle for
    // what +0x148 should have held.
    // Type identity via the registry (vtable slot 1 returns null on our inert
    // component vtables, so the NAME slot is not a usable oracle here).
    auto typeName = [](uint64_t obj) -> const char* {
        if (!obj) return "(null)";
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(obj);
        if (!vt) return "(no vtable)";
        typedef uint32_t (*fn_t)(void*);
        fn_t f = *reinterpret_cast<fn_t*>(vt + 0x10);
        if (!f) return "(no GetTypeId)";
        const char* n = ::Object::GetTypeNameById(f(reinterpret_cast<void*>(obj)));
        return n ? n : "(unknown id)";
    };
    // The entity's whole component roster (+0x30 array / +0x38 count). Print
    // EVERY entry with its identifying payload: FindComponent-by-type returns
    // the FIRST match, so if a type appears twice the lookups and the renderer
    // can be looking at different objects.
    {
        int n = *reinterpret_cast<int32_t*>(entity + 0x38);
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        std::fprintf(stderr, "[TutHLDiag] entity=%016llX roster count=%d\n",
                     (unsigned long long)entity, n);
        for (int i = 0; i < n && arr; ++i) {
            uint64_t c = *reinterpret_cast<uint64_t*>(arr + (uintptr_t)i * 8);
            const char* nm = typeName(c);
            char extra[128]; extra[0] = 0;
            if (c && std::strcmp(nm, "MapComponent") == 0)
                std::snprintf(extra, sizeof(extra), " cell=(%u,%u)..(%u,%u)",
                    *reinterpret_cast<uint16_t*>(c + 0x28), *reinterpret_cast<uint16_t*>(c + 0x2a),
                    *reinterpret_cast<uint16_t*>(c + 0x2c), *reinterpret_cast<uint16_t*>(c + 0x2e));
            else if (c && std::strcmp(nm, "ModelComponent") == 0)
                std::snprintf(extra, sizeof(extra), " pos=(%.2f,%.2f,%.2f) variant=%d modelId=%d",
                    *reinterpret_cast<float*>(c + 0x158), *reinterpret_cast<float*>(c + 0x15c),
                    *reinterpret_cast<float*>(c + 0x160),
                    (int)*reinterpret_cast<int16_t*>(c + 0x166),
                    *reinterpret_cast<int32_t*>(c + 0x16C));
            std::fprintf(stderr, "[TutHLDiag]   [%2d] %016llX %s%s\n",
                         i, (unsigned long long)c, nm, extra);
        }
        std::fflush(stderr);
    }
    float mi[7] = {0,0,0,0,0,0,0};
    if (modelIBP)
        for (int i = 0; i < 7; ++i)
            mi[i] = *reinterpret_cast<float*>(modelIBP + 0x28 + 0x84 + i * 4);
    std::fprintf(stderr,
        "[TutHLDiag] names: entity=%s model=%s map=%s  meshInstXform pos=(%.2f,%.2f,%.2f)"
        " quat=(%.3f,%.3f,%.3f,%.3f)\n",
        typeName(entity), typeName(modelIBP), typeName(mapIBP),
        mi[4], mi[5], mi[6], mi[0], mi[1], mi[2], mi[3]);


    std::fprintf(stderr,
        "[TutHLDiag] entity=%016llX model=%016llX placed(+0x166)=%d map=%016llX\n"
        "            modelXform pos=(%.2f,%.2f,%.2f) quat=(%.3f,%.3f,%.3f,%.3f)\n"
        "            modelBox min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n"
        "            footprint cell=(%d,%d) %dx%d  grid=%d  centreWorld=(%.2f,%.2f,%.2f)\n"
        "            branch=%s screenRect ok=%d (%.1f,%.1f)-(%.1f,%.1f)\n",
        (unsigned long long)entity, (unsigned long long)modelIBP, (int)placed,
        (unsigned long long)mapIBP,
        xf[4], xf[5], xf[6], xf[0], xf[1], xf[2], xf[3],
        box[0], box[1], box[2], box[3], box[4], box[5],
        rx, ry, rw, rh, g, centreWorld[0], centreWorld[1], centreWorld[2],
        (placed >= 0) ? "MODEL-BOX" : "FOOTPRINT-FALLBACK",
        (int)ok, rect[0], rect[1], rect[2], rect[3]);
    std::fflush(stderr);
}

}  // namespace

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

    // [diag] BANISHED_TUTHL_DIAG — where the arrow thinks the target is.
    //
    // EntityScreenRect (0x140100380) has TWO sources: the ModelComponent's own
    // world Transform + Box (taken when modelComp+0x166 >= 0), else a FALLBACK
    // that rebuilds four corners from the MapComponent footprint rect. The
    // ground outline (Impl_b930) is built from that same footprint rect, so a
    // wrong rect misplaces BOTH markers identically while the model itself
    // still draws from its Transform — exactly the symptom. Print both sources
    // side by side; whichever disagrees with the drawn building is the culprit.
    // Rate-limited by CHANGE (the entity + rect tuple), never by a fixed budget.
    TutHLDiag(s, entityIBP, ok, reinterpret_cast<const float*>(rect));

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
