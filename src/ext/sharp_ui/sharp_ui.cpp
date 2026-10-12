// ============================================================================
// src/ext/sharp_ui/sharp_ui.cpp -- ext.sharp_ui's engine glue
// ============================================================================
// NOT RETAIL. sharp_ui.h has the contract and the seams; sharp_math.h the rules;
// docs/extensions/handins/SHARPUI.md the why.
// ============================================================================
#include "ext/sharp_ui/sharp_ui.h"
#include "ext/sharp_ui/sharp_math.h"
#include "ext/registry.h"

#include <cstdint>
#include <cstdio>

#include "Game/GameSettings.h"
#include "Game/exe_bodies.h"      // g_gameSettings
#include "collection/Collection.h"
#include "object/External.h"
#include "system/Filename.h"
#include "ui/Dialog.h"
#include "ui/DialogInstance.h"
#include "ui/Element.h"
#include "ui/SharpDraw.h"
#include "ui/SpriteContext.h"
#include "ui/UserInterface.h"
#include "video/CompanionData.h"
#include "video/Context.h"
#include "video/Device.h"

namespace SharpUI {

namespace {

// The UI whose _uiScale this extension last set (compared, never dereferenced
// once it may be stale: SyncScale is handed the live UI by its loop).
const UI::UserInterface* g_ownedUi = nullptr;

// The last scale decision (for the trace) and the last traced frame key.
ScaleChoice g_lastChoice = { 0, 1, 1, 1, false, 1.0f };
bool        g_haveChoice = false;
bool        g_traced = false;
float       g_tracedScale = 0.0f;
int32_t     g_tracedW = 0, g_tracedH = 0;

// Overflowed dialogs already reported at the current traced scale.
constexpr int kMaxReported = 64;
const void* g_reported[kMaxReported];
int         g_reportedCount = 0;

void BackBuffer(int32_t& w, int32_t& h) {
    const Video::Device* const dev = Video::Device::_singleton;
    w = dev != nullptr ? dev->backBufferWidth : 0;
    h = dev != nullptr ? dev->backBufferHeight : 0;
}

int SliderPercent() {
    return g_gameSettings != nullptr ? g_gameSettings->uiScale : 100;
}

ScaleChoice Choose(int percent) {
    int32_t w, h;
    BackBuffer(w, h);
    g_lastChoice = ChooseScale(percent, w, h);
    g_haveChoice = true;
    return g_lastChoice;
}

bool AlreadyReported(const void* key) {
    for (int i = 0; i < g_reportedCount; ++i)
        if (g_reported[i] == key) return true;
    if (g_reportedCount < kMaxReported) g_reported[g_reportedCount++] = key;
    return false;
}

// The evidence for the fit floor: an open dialog whose laid-out root does not
// fit the layout space. Reported once per dialog resource per traced scale.
void ReportOverflow(const UI::UserInterface& ui, int32_t layoutW, int32_t layoutH) {
    using Node = Collection::LinkedList<UI::DialogInstancePtr>::Node;
    for (const Node* n = ui._dialogs._head; n != nullptr; n = n->next) {
        UI::DialogInstance* const d = n->element;   // GetRoot() const has no body (retail 0x180045590 not ported)
        if (d == nullptr) continue;
        const UI::Element* const root = d->GetRoot().Get();
        if (root == nullptr) continue;
        if (root->_layoutLeft >= 0 && root->_layoutTop >= 0 &&
            root->_layoutRight <= layoutW && root->_layoutBottom <= layoutH)
            continue;
        const void* key = d->_dialog != nullptr ? static_cast<const void*>(d->_dialog)
                                                : static_cast<const void*>(d);
        if (AlreadyReported(key)) continue;
        const char* name = "(no resource)";
        System::Filename path;
        if (d->_dialog != nullptr) {
            path = d->_dialog->GetPath();
            if (path._data != nullptr) name = path._data;
        }
        std::fprintf(stderr, "[SharpUI] overflow dialog=%s rect=%d,%d,%d,%d layout=%dx%d\n",
                     name, root->_layoutLeft, root->_layoutTop, root->_layoutRight,
                     root->_layoutBottom, layoutW, layoutH);
    }
}

}  // namespace

float EffectiveUIScale(int percent) {
    return Choose(percent).uiScale;
}

bool OwnsScale() { return g_ownedUi != nullptr; }

void SyncScale(UI::UserInterface& ui) {
    const int percent = SliderPercent();
    if (Ext::Ext().sharp_ui) {
        const float scale = Choose(percent).uiScale;
        if (ui._uiScale != scale) ui._uiScale = scale;   // relayout: retail's next UI Update
        g_ownedUi = &ui;
        return;
    }
    // Off: hand retail's derivation back to the UI this extension last set
    // (ApplyKeyMapToUI / ApplyGameSettings' line, settings[0] / 100.0f).
    if (g_ownedUi == &ui) {
        ui._uiScale = static_cast<float>(percent) / 100.0f;
        std::fprintf(stderr, "[SharpUI] off scale=%.4f slider=%d\n",
                     static_cast<double>(ui._uiScale), percent);
    }
    g_ownedUi = nullptr;
    g_traced = false;
}

void Draw(UI::UserInterface& ui, Video::Context& context) {
    int32_t bbW, bbH;
    BackBuffer(bbW, bbH);
    const DrawChoice c = ChooseDraw(ui._uiScale, bbW, bbH);

    // TRACE FIRST (EYECAM §11): one line when the extension engages or the
    // scale / back buffer changes, so the user's run says which path ran.
    if (!g_traced || ui._uiScale != g_tracedScale || bbW != g_tracedW || bbH != g_tracedH) {
        if (!g_haveChoice) Choose(SliderPercent());
        const ScaleChoice& s = g_lastChoice;
        std::fprintf(stderr,
                     "[SharpUI] on scale=%.4f ratio=%.4f integer=%d snap=%d point=%d "
                     "slider=%d wanted=%dx fit=%dx effective=%dx clamped=%d bb=%dx%d layout=%dx%d\n",
                     static_cast<double>(ui._uiScale), static_cast<double>(c.ratio),
                     c.integer ? 1 : 0, c.snap ? 1 : 0, c.point ? 1 : 0,
                     s.percent, s.wanted, s.fit, s.ratio, s.clamped ? 1 : 0,
                     bbW, bbH, c.layoutWidth, c.layoutHeight);
        g_traced = true;
        g_tracedScale = ui._uiScale;
        g_tracedW = bbW;
        g_tracedH = bbH;
        g_reportedCount = 0;
    }
    ReportOverflow(ui, c.layoutWidth, c.layoutHeight);

    UI::SharpDraw sharp;
    sharp.pxPerUnitX  = c.pxPerUnitX;
    sharp.pxPerUnitY  = c.pxPerUnitY;
    sharp.unitsPerPxX = c.pxPerUnitX > 0.0f ? 1.0f / c.pxPerUnitX : 0.0f;
    sharp.unitsPerPxY = c.pxPerUnitY > 0.0f ? 1.0f / c.pxPerUnitY : 0.0f;
    sharp.point       = c.point;

    // Each Flush picks its batch's sampler (SpriteContext::Flush, ui/SharpDraw.h)
    // and forces the texture slots to rebind when it changes the flag. The flag
    // is false on entry; on exit it is put back to false and the slots are
    // marked for rebind, because Context::BindTextures binds only dirty slots
    // and the next pass would otherwise keep a point sampler.
    if (c.snap) UI::SpriteContext::_extSharp = &sharp;

    ui.Draw(context);   // retail's UserInterface::Draw (0x180121890), unchanged

    UI::SpriteContext::_extSharp = nullptr;
    if (Video::ImageBufferData::_extPointSampling) {
        constexpr uint32_t kAllImageSlots = (1u << RtVideo::kImageBufferSlots) - 1u;
        Video::ImageBufferData::_extPointSampling = false;
        context._imageBufferDirty |= kAllImageSlots;
        context._dirty |= RtVideo::kDirtyImageBuffers;
    }
}

}  // namespace SharpUI
