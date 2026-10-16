// src/ext/billboard_shadow/enhanced_entry.cpp -- ext.billboard_shadow's row of the Enhanced tab (block TS-CORE)
//
// NOT RETAIL. The combo "Stable tree shading" Off / On = the bool row `ext.billboard_shadow`.
// SetAndRecord records the key and runs BillboardShadow::Sync, so the next frame follows Apply / OK
// (docs/extensions/TREE_SHADOW.md, handins/TS-CORE.md).

#include "ext/enhanced_tab/enhanced_tab.h"
#include "ext/registry.h"

#include "ui/ComboElement.h"

#include <cstdio>

namespace Ext {
namespace EnhancedTab {

namespace {

UI::ComboElement* g_combo = nullptr;

const Entry& Row() { return *FindEntry("billboard_shadow"); }

void Open(UI::Element* page) {
    g_combo = FindIn<UI::ComboElement>(page, "comboBillboardShadow");
    const int item = Ext::Ext().billboard_shadow ? 1 : 0;
    if (g_combo != nullptr) g_combo->SetCurrentItem(item);
    std::fprintf(stderr, "[BillboardShadowMenu] combo=%p item=%d\n", static_cast<void*>(g_combo), item);
}

void Apply() {
    if (g_combo == nullptr) return;
    const bool on = g_combo->_currentItem == 1;
    const Entry& row = Row();
    if (Requested(row) != on) SetAndRecord(row, on);
    std::fprintf(stderr, "[BillboardShadowMenu] Apply: item=%d active=%d\n", g_combo->_currentItem,
                 Ext::Ext().billboard_shadow ? 1 : 0);
}

void Close() { g_combo = nullptr; }

}  // namespace

const Contribution& BillboardShadowEntry() {
    static const Contribution c = { "billboard_shadow", Open, Apply, Close };
    return c;
}

}  // namespace EnhancedTab
}  // namespace Ext
