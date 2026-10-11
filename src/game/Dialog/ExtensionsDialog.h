#pragma once
// ============================================================================
// src/game/Dialog/ExtensionsDialog.h -- the Extensions dialog (NOT RETAIL)
// ============================================================================
// docs/extensions/DESIGN.md §3 "Menu". Lists every fix and extension of the
// registry (src/ext/registry.h) with its kind and class letter, its lock
// state ("live" / "locked for this game"), a toggle and its description.
//
// THE RESOURCES ARE RETAIL'S. The shipped loader reads COMPILED resources
// only (Object::External::Load, external_load.cpp: a CompressedFileStream,
// magic 0x5412a91f, the Binary encoding; the Text encoding's reader/writer
// were stripped from the DLL and return null, encoding_visitor_text.cpp), and
// Tools-x64.exe, the one compiler, is not on this machine. So the dialog is
// built on Dialog/Mod.rsc (the Mods dialog's own template and row template
// `modLine`), with its texts switched to the general table's "Parameter"
// string (@0) and filled from the registry; the front-end button is a runtime
// clone of the menu's buttonMod with its label re-texted and a procedurally
// drawn icon of its own (ExtensionsDialog_AttachMenuButton).
//
// Shape: ModDialog's (ModDialog.h) -- OnOpened builds a row per entry,
// OnElementEvent dispatches Cancel 100 / OK 0x65 / row toggles 1000+i, and
// OK in a running game with a B / C change shows Dialog/Reload.rsc:game, the
// notice the Mods dialog shows, and reloads the game the way ModDialog's
// CloseAndReload does. Not a reflected type (no factory registration): it is
// only ever built by ExtensionsDialog_OpenFromMainMenu.
// ============================================================================

#include <cstdint>
#include "ui/DialogInstance.h"
#include "object/Internal.h"

class MainMenuDialog;                                   // Dialog/MainMenuDialog.h

// The menu button's element id (MainMenuDialog's ids stop at 12, Tutorial).
constexpr int32_t kExtensionsMenuButtonId = 13;

class ExtensionsDialog final : public UI::DialogInstance {
public:
    ExtensionsDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                     Object::InternalPtr<MainMenuDialog> parentMenu);
    ~ExtensionsDialog() override;

    void OnOpened() override;
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;

private:
    struct Row {
        int32_t entry;                               // registry index
        bool    pending;                             // the checkbox, not yet applied
        Object::InternalPtr<UI::Element> check;      // CheckElement
        Object::InternalPtr<UI::Element> state;      // textModVersion: the lock state
        Object::InternalPtr<UI::Element> border;     // the row's border (colour)
    };
    static constexpr int kMaxRows = 64;

    void RefreshRow(int index);
    void Apply();
    void ReloadGame();
    void Close();

    Row     _rows[kMaxRows];
    int32_t _rowCount = 0;
    Object::InternalPtr<UI::Element>  _table;        // "tableMods"
    Object::InternalPtr<MainMenuDialog> _parentMenu; // the menu that opened it (the reload goes through it)
};

// The "Extensions" button's click (MainMenuDialog::OnElementEvent, id 13).
void ExtensionsDialog_OpenFromMainMenu(MainMenuDialog* menu);

// EXT SEAM in MainMenuDialog::OnOpened: clone buttonMod beside it as the
// Extensions button (id 13). Does nothing when the gate forced the faithful
// profile on the command line (Ext::GateProfileForced), when the menu has no
// buttonMod, or when any step fails (each logged as `[ExtMenu]`).
void ExtensionsDialog_AttachMenuButton(MainMenuDialog* menu);
