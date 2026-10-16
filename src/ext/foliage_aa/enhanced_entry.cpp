// src/ext/foliage_aa/enhanced_entry.cpp -- foliage AA's row of the Enhanced tab (block FAA-MENU)
//
// NOT RETAIL. The combo "Foliage edges": Off / Smooth edges / Smooth edges + distance fill =
// FoliageAA::Current().mode 0 / 1 / 2. It is the master of the bool row `ext.foliage_aa`: Off turns
// the row off (the stored `ext.foliage_aa_mode` is kept), the others set it on and the mode key.
// FoliageAA::Set records the keys and Sync()s the render side, so the next frame follows Apply / OK.

#include "ext/enhanced_tab/enhanced_tab.h"
#include "ext/foliage_aa/foliage_aa.h"

#include "ui/ComboElement.h"

#include <cstdio>

namespace Ext {
namespace EnhancedTab {

namespace {

UI::ComboElement* g_combo = nullptr;

void Open(UI::Element* page) {
    g_combo = FindIn<UI::ComboElement>(page, "comboFoliageAA");
    const int mode = FoliageAA::Current().mode;
    if (g_combo != nullptr) g_combo->SetCurrentItem(mode);
    std::fprintf(stderr, "[FoliageAAMenu] combo=%p mode=%d\n", static_cast<void*>(g_combo), mode);
}

void Apply() {
    if (g_combo == nullptr) return;
    FoliageAA::Params p;
    p.mode = g_combo->_currentItem;
    FoliageAA::Set(p);
    std::fprintf(stderr, "[FoliageAAMenu] Apply: mode=%d\n", FoliageAA::Current().mode);
}

void Close() { g_combo = nullptr; }

}  // namespace

const Contribution& FoliageAAEntry() {
    static const Contribution c = { "foliage_aa", Open, Apply, Close };
    return c;
}

}  // namespace EnhancedTab
}  // namespace Ext
