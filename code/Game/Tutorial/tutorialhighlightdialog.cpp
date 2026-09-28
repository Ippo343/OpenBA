// code/Game/Tutorial/tutorialhighlightdialog.cpp
// TutorialHighlightDialog — the glow ring and bobbing arrow over the thing the
// current tutorial step wants clicked.
//
// TutorialHighlightDialog (0xF0 bytes), table 0x1401609e0, on UI::DialogInstance.
// ENDGAME B.3.8 block GOAL: a real class (Tutorial/TutorialHighlightDialog.h);
// the hand-built 48-slot table and the hand-rolled ctor in
// tutorial_dialog_vtables.cpp are gone.

// volume/Volume.h FIRST: its global-scope `struct Rectangle` is hidden by <wingdi.h>'s
// `Rectangle()` once <windows.h> has been seen.
#include "volume/Volume.h"
#include "Tutorial/TutorialHighlightDialog.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include "system/Value.h"   // ENDGAME B.3.4b System callers: System::Time
#include "object/Internal.h"   // Object::detail::InternalMemAlloc / InternalMemFree
#include "object/VSlot.h"      // B.3.x W0: the typeName probe's slot
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"                       // ENDGAME B.3.6 UI callers (K4)
#include "ui/Sprite.h"                        // ENDGAME B.3.6 UI callers (K4)
#include "ui/ImageElement.h"                  // ENDGAME B.3.6 UI callers (K4)
#include <new>
#include <cstdint>
#include "video/Device.h"   // ENDGAME B.3.9 SS2: Video::Device::_singleton

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// 0x1400e0290 (create 0x1400278c0 runs it): DialogInstance() (the base's
// default ctor), the stamp (the compiler's), then +0xb8..+0xe8 zeroed.
TutorialHighlightDialog::TutorialHighlightDialog()
    : elapsedTicks(0), glowGroup(nullptr), glow(nullptr), arrow(nullptr),
      rotation(0), targetElement(nullptr), targetEntity(nullptr) {}

// 0x1400e01d0: DialogInstance(ui, dialog, parent, 0, 0, 0), the stamp, the
// fields zeroed, then +0xd8 = rotation, +0xe0 = *targetElement,
// +0xe8 = *targetEntity (retail passes the two handles by address; the caller
// dereferences them here). The tree's hand-rolled copy skipped the base ctor
// when `dialog` was null; retail never does.
TutorialHighlightDialog::TutorialHighlightDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                                                 Object::InternalPtr<UI::DialogInstance> parent,
                                                 void* targetElementArg, int32_t rotationArg,
                                                 void* targetEntityArg)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0),
      elapsedTicks(0), glowGroup(nullptr), glow(nullptr), arrow(nullptr),
      rotation(rotationArg), targetElement(targetElementArg), targetEntity(targetEntityArg) {}

// ============================================================
// TutorialHighlightDialog::OnOpened (vtable +0x20, 0x1400e0020)
// ============================================================
// Finds "glowGroup", "glow", "arrow" by name and keeps them at +0xC0, +0xC8, +0xD0.
void TutorialHighlightDialog::OnOpened() {
    struct { const char* name; void** slot; } elements[] = {
        { "glowGroup", &glowGroup },
        { "glow",      &glow },
        { "arrow",     &arrow },
    };

    for (auto& e : elements) {
        System::String nameStr(e.name);
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));
        *e.slot = *reinterpret_cast<void**>(retBuf);
    }
}

// ============================================================
// TutorialHighlightDialog::OnUpdate (vtable +0x38, 0x1400e0000)
// ============================================================
// Retail is 11 bytes: MOV RAX,[RDX]; ADD [RCX+0xB8],RAX; RET — the frame's
// tick count added to the elapsed time the arrow's bob reads.
void TutorialHighlightDialog::OnUpdate(const System::Time& time) {
    elapsedTicks += time._ticks;
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

#include "Action/exe_bodies.h"      // 0x140003f60 ActCNR_FindComponent_3f60
#include "Component/exe_bodies.h"   // 0x140004680 FloorComponent_FindResData
#include "Game/exe_bodies.h"        // 0x14009f3e0 Reimpl_MapGetTerrainPos
#include "Player/exe_bodies.h"      // 0x140100380 PlayerScreenRect_Entity_0380
#include "Object/factory_registration.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {

// Volume::Rectangle::Rectangle() is RtVolume_Rectangle_Ctor; 0x140100380 — entity
// screen bounding rectangle — is PlayerScreenRect_Entity_0380 (ported in
// code/Game/Player/tool_base_screenrect.cpp). param_1 is unused by the body; retail
// passes the rectangle itself there.

// sinf(elapsed * pi) * 8 — the shared bob offset both helpers apply.
float ArrowBob(uintptr_t self) {
    float t = reinterpret_cast<const System::Time*>(reinterpret_cast<void*>(self + 0xB8))->AsSeconds();   // B.3.4 bridge
    return std::sin(t * 3.14159274f) * 8.0f;
}

}  // namespace

// ---------------------------------------------------------------------------
// 0x1400DFAC0 — the UI-ELEMENT branch (+0xE8 == 0, target element at +0xE0).
// Grows the glow ring to the element's rect + 13px on every side, parks the
// glowGroup with a four-sided padding measured against the scaled screen, then
// puts the arrow on the corner selected by the rotation field.
// ---------------------------------------------------------------------------
void TutorialHighlightDialog::PlaceOnElement() {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    void* targetIBP = reinterpret_cast<void*>(s + 0xE0);
    uintptr_t elem = *reinterpret_cast<uintptr_t*>(targetIBP);
    if (!elem) return;

    // Element rect: +0x68 left, +0x6C top, +0x70 right, +0x74 bottom.
    int left   = *reinterpret_cast<int32_t*>(elem + 0x68);
    int top    = *reinterpret_cast<int32_t*>(elem + 0x6C);
    int width  = *reinterpret_cast<int32_t*>(elem + 0x70) - left;
    int height = *reinterpret_cast<int32_t*>(elem + 0x74) - top;

    void* glow = *reinterpret_cast<void**>(s + 0xC8);
    if (glow)
        reinterpret_cast<UI::ImageElement*>(glow)->SetWidthHeight(width + 0x1A, height + 0x1A);  // B.3.6 bridge

    int padLeft = left - 13;
    int padTop  = top  - 13;

    // Screen size from the display singleton (+4 width, +8 height), scaled by
    // the UserInterface's +0xDC factor.
    // Video::Device is behind a DOUBLE deref of the 0x140151a80 exref
    //   MOV RAX,[0x140151a80] ; MOV RCX,[RAX] ; MOVD XMM0,[RCX+4]
    // — one deref short lands on the holder and reads garbage dimensions, which
    // is exactly what mis-sizes and mis-places the glow ring.
    int screenW = 0, screenH = 0;
    if (void* device = ::Video::Device::_singleton) {
        screenW = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(device) + 4);
        screenH = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(device) + 8);
    }
    float scale = 1.0f;
    uintptr_t ui = *reinterpret_cast<uintptr_t*>(s + 0x10);
    if (ui) scale = *reinterpret_cast<float*>(ui + 0xDC);

    void* glowGroup = *reinterpret_cast<void**>(s + 0xC0);
    if (glowGroup) {
        reinterpret_cast<UI::Element*>(glowGroup)->SetPadding(  // B.3.6 bridge
                              padLeft,
                              (static_cast<int>(static_cast<float>(screenW) * scale) - padLeft) - width - 0x1A,
                              padTop,
                              (static_cast<int>(static_cast<float>(screenH) * scale) - padTop) - height - 0x1A);
        reinterpret_cast<UI::Element*>(glowGroup)->SetNeedsSizeCalc();  // B.3.6 bridge
    }

    // Arrow: corner + sign pair chosen by the rotation.
    void* arrowIBP = reinterpret_cast<void*>(s + 0xD0);
    void* arrow = *reinterpret_cast<void**>(arrowIBP);
    if (!arrow) return;

    uint8_t sprite[16] = {};
    ::new (sprite) UI::Sprite(reinterpret_cast<const UI::ImageElement*>(arrow)->GetSprite());  // B.3.6 bridge
    int arrowW = reinterpret_cast<const UI::Sprite*>(sprite)->Width();  // B.3.6 bridge

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
    reinterpret_cast<UI::Element*>(arrow)->SetPadding(  // B.3.6 bridge
                          static_cast<int>(static_cast<float>(signX) * bob + static_cast<float>(x)), 0,
                          static_cast<int>(static_cast<float>(signY) * bob + static_cast<float>(y)), 0);
    reinterpret_cast<UI::ImageElement*>(arrow)->SetRotation(static_cast<float>(-rot));  // B.3.6 bridge
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
//   0x140003f60 -> ModelComponent (body ActCNR_FindComponent_3f60), 0x140004680 ->
//   MapComponent (body FloorComponent_FindResData). Both are (entity, &outIBP) -> &outIBP.
// 0x14009f3e0 Reimpl_MapGetTerrainPos — grid cell -> world position (real terrain z).

// The diagnostic's reprint-on-change filter (main thread only: OnUpdateLayout).
uint64_t s_diagLastEntity = 0;
int      s_diagLastRect[4] = { -12345, 0, 0, 0 };

void TutHLDiag(uintptr_t self, uint64_t entity, bool ok, const float* rect) {

    uint64_t modelIBP = 0, mapIBP = 0;
    ActCNR_FindComponent_3f60(static_cast<uintptr_t>(entity), &modelIBP);            // 0x140003f60
    FloorComponent_FindResData(reinterpret_cast<void*>(entity), &mapIBP);            // 0x140004680

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
    if (GameUpdateLoop::_singleton != nullptr) {
        int* grid = *reinterpret_cast<int**>(
            reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8);
        if (grid) {
            g = grid[0];
            float bias = static_cast<float>(g >> 1) - 0.5f;
            float in[3] = { static_cast<float>(rx + (rw >> 1)) - bias,
                            static_cast<float>(ry + (rh >> 1)) - bias, 0.0f };
            Reimpl_MapGetTerrainPos(grid, centreWorld, in, '\0', nullptr);   // 0x14009f3e0
        }
    }

    // Only reprint when something actually changes.
    // The reprint-on-change filter lives at namespace scope (s_diagLast*).
    uint64_t& lastEntity = s_diagLastEntity;
    int* lastRect = s_diagLastRect;
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
        if (!::Object::SlotAddressOf(reinterpret_cast<void*>(obj), &::Object::Base::Type))
            return "(no GetTypeId)";
        const char* n = ::Object::GetTypeNameById(
            reinterpret_cast< ::Object::Base*>(obj)->Type());   // B.3.x bridge (slot 2)
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

void TutorialHighlightDialog::PlaceOnEntity() {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    if (void* glowGroup = *reinterpret_cast<void**>(s + 0xC0))
        reinterpret_cast<UI::Element*>(glowGroup)->SetFlagsOnOff(1, 0);   // hide  // B.3.6 bridge

    // Volume::Rectangle rect; ok = EntityScreenRect(rect, &entityIBP, &rect)
    alignas(16) uint8_t rect[16] = {};
    RtVolume_Rectangle_Ctor(reinterpret_cast<struct Rectangle*>(rect));
    uint64_t entityIBP = *reinterpret_cast<uint64_t*>(s + 0xE8);
    bool ok = PlayerScreenRect_Entity_0380(reinterpret_cast<uintptr_t>(rect), &entityIBP,
                                           reinterpret_cast<uint64_t*>(rect)) != 0;   // 0x140100380

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
    void* arrow = *reinterpret_cast<void**>(arrowIBP);
    if (!ok) {
        if (arrow) reinterpret_cast<UI::Element*>(arrow)->SetFlagsOnOff(1, 0);   // hide  // B.3.6 bridge
        return;
    }
    if (!arrow) return;
    reinterpret_cast<UI::Element*>(arrow)->SetFlagsOnOff(0, 1);                  // show  // B.3.6 bridge

    uint8_t sprite[16] = {};
    ::new (sprite) UI::Sprite(reinterpret_cast<const UI::ImageElement*>(arrow)->GetSprite());  // B.3.6 bridge
    int arrowW = reinterpret_cast<const UI::Sprite*>(sprite)->Width();  // B.3.6 bridge
    ::new (sprite) UI::Sprite(reinterpret_cast<const UI::ImageElement*>(arrow)->GetSprite());  // B.3.6 bridge
    int arrowH = reinterpret_cast<const UI::Sprite*>(sprite)->Height();  // B.3.6 bridge

    // Rectangle is four floats: minX, minY, maxX, maxY.
    const float* r = reinterpret_cast<const float*>(rect);
    float centreX = (r[0] + r[2]) * 0.5f;
    float centreY = (r[1] + r[3]) * 0.5f;

    float bob = ArrowBob(s);
    // Retail multiplies the horizontal bob by 0.0 — the arrow bobs vertically
    // only. Kept literal rather than "simplified" away: the term is in the
    // shipped code and dropping it would hide a future edit to that constant.
    reinterpret_cast<UI::Element*>(arrow)->SetPadding(  // B.3.6 bridge
                          static_cast<int>(static_cast<float>(static_cast<int>(centreX) - (arrowW >> 1)) + bob * 0.0f), 0,
                          static_cast<int>(static_cast<float>(static_cast<int>(centreY) - arrowH) + bob), 0);
    reinterpret_cast<UI::ImageElement*>(arrow)->SetRotation(  // B.3.6 bridge
        static_cast<float>(-*reinterpret_cast<int32_t*>(s + 0xD8)));
}

// TutorialHighlightDialog::OnUpdateLayout (0x1400DFFE0) — vtable slot 6.
// Retail is the dispatch plus an inlined copy of the 0x1400DFDA0 body.
void TutorialHighlightDialog::OnUpdateLayout() {
    if (targetEntity == nullptr)
        PlaceOnElement();
    else
        PlaceOnEntity();
}

// create 0x1400278c0: MemAlloc(0xf0, 8), then the default ctor 0x1400e0290.
void* TutorialHighlightDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialHighlightDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialHighlightDialog());
}
// Retail's createInPlace 0x140027900 does not build a dialog in place (B8-13);
// the tree's in-place entry did, and that is kept.
void* TutorialHighlightDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialHighlightDialog());
}

// DEFINE_EXE_TYPE(TutorialHighlightDialog, "UI::DialogInstance") stays where
// its REGISTER_TYPE was — tutorial_dialog_vtables.cpp (B38_RULES B8-7).
