// src/ext/enhanced_tab/shadow_res_entry.cpp -- ext.max_shadow_res's row of the Enhanced tab (block FAA-MENU)
//
// NOT RETAIL. The combo "Max shadow resolution" Off / On = the bool row `ext.max_shadow_res`
// (the sun map is 4096 when the retail Shadow Resolution combo is at High). The row has no code
// directory: its seam is Graphics::SunLight::_extMaxShadowRes, which reads the row's bool every
// frame (docs/extensions/handins/FAA-SHADOW.md), so recording the row is the whole live change.

#include "ext/enhanced_tab/enhanced_tab.h"
#include "ext/registry.h"

#include "ui/ComboElement.h"

#include <cstdio>

namespace Ext {
namespace EnhancedTab {

namespace {

UI::ComboElement* g_combo = nullptr;

const Entry& Row() { return *FindEntry("max_shadow_res"); }

void Open(UI::Element* page) {
    g_combo = FindIn<UI::ComboElement>(page, "comboMaxShadowRes");
    const int on = Requested(Row()) ? 1 : 0;
    if (g_combo != nullptr) g_combo->SetCurrentItem(on);
    std::fprintf(stderr, "[ShadowResMenu] combo=%p on=%d\n", static_cast<void*>(g_combo), on);
}

void Apply() {
    if (g_combo == nullptr) return;
    const bool on = g_combo->_currentItem == 1;
    if (Requested(Row()) != on) SetAndRecord(Row(), on);
    std::fprintf(stderr, "[ShadowResMenu] Apply: on=%d\n", Requested(Row()) ? 1 : 0);
}

void Close() { g_combo = nullptr; }

}  // namespace

const Contribution& MaxShadowResEntry() {
    static const Contribution c = { "max_shadow_res", Open, Apply, Close };
    return c;
}

}  // namespace EnhancedTab
}  // namespace Ext
