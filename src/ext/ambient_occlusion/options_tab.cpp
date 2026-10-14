// src/ext/ambient_occlusion/options_tab.cpp -- the Options dialog's "Enhanced" tab (block AOMENU)
//
// NOT RETAIL. options_tab.h has the contract.

#include "ext/ambient_occlusion/options_tab.h"

#include "ext/ambient_occlusion/params.h"
#include "ext/registry.h"

#include "ui/ComboElement.h"
#include "ui/Element.h"
#include "ui/ElementDescription.h"
#include "ui/SliderElement.h"
#include "ui/TabElement.h"
#include "object/External.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "system/Filename.h"
#include "system/String.h"

#include <cstdint>
#include <cstdio>

namespace Ext {
namespace AO {
namespace OptionsTab {

namespace {

constexpr const char* kButtonRsc = "Dialog/AoOptions.rsc:buttonEnhanced";
constexpr const char* kPageRsc   = "Dialog/AoOptions.rsc:enhancedOptions";
constexpr const char* kComboName  = "comboAoQuality";
constexpr const char* kSliderName = "sliderAoOpacity";

// The one open Options dialog's elements (the dialog is modal).
struct Side {
    const void*        dialog = nullptr;
    UI::ComboElement*  quality = nullptr;
    UI::SliderElement* opacity = nullptr;
};
Side g_side;

UI::ElementDescription* LoadDescription(const char* path) {
    UI::ElementDescription* d =
        ::Object::ExternalFactory::_singleton->FindOrLoadAs<UI::ElementDescription>(
            System::Filename(path));
    if (!::Object::IsDerivedFromNullSafe(d, UI::ElementDescription::TypeId())) {
        std::fprintf(stderr, "[AoMenu] %s did not load (%p): no Enhanced tab\n", path,
                     static_cast<const void*>(d));
        return nullptr;
    }
    return d;
}

template <class T>
T* FindIn(UI::Element* page, const char* name) {
    UI::Element* e = page->FindElement(System::String(name)).Get();
    return ::Object::IsDerivedFromNullSafe(e, T::TypeId()) ? static_cast<T*>(e) : nullptr;
}

}  // namespace

bool Offered() { return !GateProfileForced(); }

bool Open(const void* dialog, UI::TabElement* tabs) {
    g_side = Side();
    if (!Offered()) return false;
    UI::ElementDescription* button = LoadDescription(kButtonRsc);
    UI::ElementDescription* content = LoadDescription(kPageRsc);
    if (button == nullptr || content == nullptr) return false;

    Object::InternalPtr<UI::Element> page =
        tabs->AddTab(Object::ExternalPtr<UI::ElementDescription>(button),
                     Object::ExternalPtr<UI::ElementDescription>(content));
    UI::Element* root = page.Get();
    if (root == nullptr) return false;

    g_side.dialog = dialog;
    g_side.quality = FindIn<UI::ComboElement>(root, kComboName);
    g_side.opacity = FindIn<UI::SliderElement>(root, kSliderName);

    const Params& now = Current();
    if (g_side.quality != nullptr) g_side.quality->SetCurrentItem(now.quality);
    if (g_side.opacity != nullptr) {
        g_side.opacity->_minimum = 0.0f;      // like the UI scale slider (OptionsDialog::OnOpened)
        g_side.opacity->_maximum = 100.0f;
        g_side.opacity->_value = static_cast<float>(now.opacity);
    }
    std::fprintf(stderr, "[AoMenu] Enhanced tab added: tabs=%d combo=%p slider=%p quality=%d opacity=%d\n",
                 tabs->_tabCount, static_cast<void*>(g_side.quality),
                 static_cast<void*>(g_side.opacity), now.quality, now.opacity);
    return true;
}

void Apply(const void* dialog) {
    if (dialog == nullptr || dialog != g_side.dialog) return;
    Params p = Current();
    if (g_side.quality != nullptr) p.quality = g_side.quality->_currentItem;
    if (g_side.opacity != nullptr) p.opacity = static_cast<int>(g_side.opacity->_value + 0.5f);
    Set(p);
    const Params& now = Current();
    std::fprintf(stderr, "[AoMenu] Apply: quality=%d opacity=%d\n", now.quality, now.opacity);
}

void Close(const void* dialog) {
    if (dialog != nullptr && dialog == g_side.dialog) g_side = Side();
}

}  // namespace OptionsTab
}  // namespace AO
}  // namespace Ext
