// src/ext/ambient_occlusion/options_tab.h -- the Options dialog's "Enhanced" tab (block AOMENU)
//
// NOT RETAIL. A fifth tab added at run time to retail's OptionsDialog by UI::TabElement::AddTab
// (docs/extensions/AO.md §6.3, handins/AOSPIKE.md S6 (a)). The tab's button and page are NEW
// objects in Dialog/AoOptions.rsc (loose .crs, tools/ext_resources.sh): a combo "Ambient
// occlusion" (Off / Low / Medium / High = Params::quality 0..3) and a slider "AO strength"
// (0..30 = Params::opacity).
//
// The element pointers live HERE, keyed by the dialog instance, never in OptionsDialog (its
// sizeof 0x1C0 is retail's, asserted). One Options dialog is open at a time (it is modal), so the
// side state is one slot; a call for any other dialog is ignored.
//
// The dialog's arms (src/game/Dialog/optionsdialog.cpp):
//   OnOpened  Open(this, tabOptions)   before the last-tab restore, so tab 4 can be restored
//   OnApply   Apply(this)              Ext::AO::Set from the two elements (retail's
//                                      ApplyVideoSettings does not know the tab)
//   OnClosed  Close(this)              forget the pointers
// Cancel needs no arm: nothing is written before Apply, and the next Open re-reads Current().
#pragma once

namespace UI {
class TabElement;
}

namespace Ext {
namespace AO {
namespace OptionsTab {

// Whether this build offers the tab: always, except under the gate (`/profile:faithful` from the
// command line), whose menus stay exactly retail's (Ext::GateProfileForced). Deliberately NOT the
// row's value: the player turns ambient occlusion ON from this tab.
bool Offered();

// Adds the tab to `tabs`, finds the combo and the slider in the new page, fills them from
// Ext::AO::Current(). Returns false (and adds nothing) when not Offered() or the resources did not
// load (the loose .crs files are not deployed); the dialog then has its four retail tabs.
bool Open(const void* dialog, UI::TabElement* tabs);

// Reads the two elements and writes them through Ext::AO::Set. No-op unless `dialog` is the one
// Open filled. The caller flushes settings.cfg (OnApply's WriteSettings).
void Apply(const void* dialog);

// Forgets the element pointers (the dialog is closing). No-op for any other dialog.
void Close(const void* dialog);

}  // namespace OptionsTab
}  // namespace AO
}  // namespace Ext
