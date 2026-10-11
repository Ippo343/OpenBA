// ============================================================================
// src/game/Dialog/extensionsdialog.cpp -- the Extensions dialog (NOT RETAIL)
// ============================================================================
// ExtensionsDialog.h has the design. Everything here is standalone code, not a
// retail body: guards are ours to place (CLAUDE.md's no-null-guard rule is
// about faithful bodies), and each failed step is logged as `[ExtDialog]` /
// `[ExtMenu]` to standalone_debug.log and leaves the menu as retail's.
// ============================================================================

// The runtime type headers first (moddialog.cpp's note: Volume.h's Rectangle).
#include "ui/TextElement.h"
#include "ui/LabelElement.h"
#include "ui/ImageElement.h"
#include "ui/ButtonElement.h"
#include "ui/CheckElement.h"
#include "ui/ElementDescription.h"
#include "ui/Element.h"
#include "ui/DialogInstance.h"
#include "ui/UserInterface.h"
#include "ui/Sprite.h"
#include "ui/SpriteSheet.h"
#include "ui/StringTable.h"
#include "ui/String.h"
#include "ui/Dialog.h"
#include "engine/UpdateLoop.h"
#include "engine/MainLoop.h"
#include "video/ImageBuffer.h"
#include "video/Device.h"
#include "graphics/Material.h"
#include "object/External.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "save/SaveLoad.h"
#include "system/String.h"
#include "system/Filename.h"
#include "system/MemoryAllocator.h"
#include "Game/GameUpdateLoop.h"
#include "Game/exe_constants.h"        // kRdata_14017214c (the UV scale ModDialog uses)
#include "Dialog/ExtensionsDialog.h"
#include "Dialog/MainMenuDialog.h"
#include "Dialog/MessageDialog.h"
#include "ext/registry.h"
#include "options.h"

#include <cstdint>
#include <cstdio>
#include <new>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Resource helpers
// ---------------------------------------------------------------------------
const UI::Dialog* LoadDialog(const char* path) {
    if (!::Object::ExternalFactory::_singleton) return nullptr;
    const ::Object::External* ext =
        ::Object::ExternalFactory::_singleton->FindOrLoad(System::Filename(path));
    if (!::Object::IsDerivedFromNullSafe(ext, UI::Dialog::TypeId())) {
        printf("[ExtDialog] %s did not load as a UI::Dialog (%p)\n", path, (const void*)ext);
        return nullptr;
    }
    return static_cast<const UI::Dialog*>(ext);
}

// The general table's "Parameter" string: "@0", the text the Mods rows use.
UI::String ParameterString() {
    UI::String s;
    if (!::Object::ExternalFactory::_singleton) return s;
    UI::StringTable* table = ::Object::ExternalFactory::_singleton->FindOrLoadAs<UI::StringTable>(
        System::Filename("Dialog/StringTable.rsc:general"));
    if (!::Object::IsDerivedFromNullSafe(table, UI::StringTable::TypeId())) {
        printf("[ExtDialog] Dialog/StringTable.rsc:general did not load (%p)\n", (const void*)table);
        return s;
    }
    return UI::String(Object::ExternalPtr<UI::StringTable>(table), System::String("Parameter"));
}

UI::TextElement* AsText(UI::Element* e) {
    if (!::Object::IsDerivedFromNullSafe(e, UI::TextElement::TypeId())) return nullptr;
    return static_cast<UI::TextElement*>(e);
}

// Show `text` in a text element: retarget it to "@0" when `retarget` (its
// own string has no parameter), then fill parameter 0.
void SetElementText(UI::Element* e, const UI::String& param, const char* text, bool retarget) {
    UI::TextElement* t = AsText(e);
    if (t == nullptr) return;
    if (retarget) {
        if (!param.IsValid()) return;
        t->SetText(&param);
    }
    t->SetParameter(0, System::String(text));
}

UI::Element* Find(UI::DialogInstance* d, const char* name) {
    return d->FindElement(System::String(name)).Get();
}

const char* KindText(const Ext::Entry& e) {
    if (e.tier == Ext::Tier::Fix) return "Fix: a retail bug corrected";
    switch (e.cls) {
    case Ext::Class::A: return "Extension, class A: toggles live";
    case Ext::Class::B: return "Extension, class B: changes play, locked per game";
    case Ext::Class::C: return "Extension, class C: extends the save, locked per game";
    case Ext::Class::None: break;
    }
    return "Extension";
}

}  // namespace

// ===========================================================================
// ExtensionsDialog
// ===========================================================================
ExtensionsDialog::ExtensionsDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                                   Object::InternalPtr<MainMenuDialog> parentMenu)
    : UI::DialogInstance(ui, dialog, Object::InternalPtr<UI::DialogInstance>(), nullptr, nullptr, 0),
      _parentMenu(parentMenu) {
    for (Row& r : _rows) { r.entry = -1; r.pending = false; }
}

ExtensionsDialog::~ExtensionsDialog() {}

void ExtensionsDialog::OnOpened() {
    const UI::String param = ParameterString();

    if (UI::Element* cancel = Find(this, "buttonCancel")) {
        cancel->SetElementId(100);
        static_cast<UI::ButtonElement*>(cancel)->_keyIndex = 8;   // Esc, as ModDialog
    }
    if (UI::Element* ok = Find(this, "buttonOk")) ok->SetElementId(0x65);

    // The Mods dialog's chrome, re-texted. A profile from settings.cfg
    // overrides the toggles at the next boot: say so in the title.
    std::string title = "Fixes and Extensions";
    if (Ext::CurrentProfile() != Ext::Profile::PerEntry) {
        title += "  (profile: ";
        title += Ext::ProfileName(Ext::CurrentProfile());
        title += ")";
    }
    SetElementText(Find(this, "labelTitle"), param, title.c_str(), true);
    SetElementText(Find(this, "labelMod"), param, "Fixes and extensions", true);
    // Columns with no meaning here, and the Workshop half of the dialog.
    for (const char* name : {"textOrder", "textTop", "textDetails", "buttonBrowse", "groupProgress"})
        if (UI::Element* e = Find(this, name)) e->Hide(true);

    _table = FindElement(System::String("tableMods"));
    const UI::Dialog* dialog = _dialog;
    if (_table.Get() == nullptr || dialog == nullptr) {
        printf("[ExtDialog] no tableMods / dialog resource: empty dialog\n");
        return;
    }

    _rowCount = 0;
    for (int i = 0; i < Ext::EntryCount() && _rowCount < kMaxRows; ++i) {
        const Ext::Entry& entry = Ext::EntryAt(i);

        Object::ExternalPtr<UI::ElementDescription> rowTemplate = dialog->GetTemplate(0);   // modLine
        if (rowTemplate.Get() == nullptr) break;
        Object::InternalPtr<UI::Element> row;
        rowTemplate->CreateInstance(&row, dialog);
        if (row.Get() == nullptr) break;
        _table->AddChild(&row);

        auto child = [&row](const char* name) {
            return row->FindElement(System::String(name)).Get();
        };
        SetElementText(child("textModName"), param, entry.displayName, false);
        SetElementText(child("textModAuthor"), param, KindText(entry), false);
        SetElementText(child("textModDescription"), param, entry.description, false);
        for (const char* name : {"imageConflict", "imageIcon", "buttonTop", "buttonUp",
                                 "buttonDown", "buttonDetails"})
            if (UI::Element* e = child(name)) e->Hide(true);

        Row& r = _rows[_rowCount];
        r.entry = i;
        r.pending = Ext::Requested(entry);
        r.check = Object::InternalPtr<UI::Element>(child("checkLoaded"));
        r.state = Object::InternalPtr<UI::Element>(child("textModVersion"));
        // ModDialog's row border: slot 11, GetBorder.
        Object::InternalPtr<UI::Element> border;
        r.border = *row->GetBorder(&border);
        if (r.check.Get() != nullptr) r.check->SetElementId(1000 + _rowCount);
        // The version column shows the lock state: retarget it to "@0" once.
        if (UI::TextElement* t = AsText(r.state.Get()))
            if (param.IsValid()) t->SetText(&param);

        RefreshRow(_rowCount);
        ++_rowCount;
    }
    printf("[ExtDialog] opened: %d row(s), game=%p locked=%d\n", _rowCount,
           (void*)GameUpdateLoop::_singleton, Ext::Locked() ? 1 : 0);
}

void ExtensionsDialog::RefreshRow(int index) {
    Row& r = _rows[index];
    const Ext::Entry& e = Ext::EntryAt(r.entry);

    if (::Object::IsDerivedFromNullSafe(r.check.Get(), UI::CheckElement::TypeId()))
        static_cast<UI::CheckElement*>(r.check.Get())->_checked = r.pending ? 1 : 0;

    std::string state;
    if (Ext::EntryLocked(e)) {
        state = "locked for this game";
        if (r.pending != Ext::Active(e))
            state += r.pending ? " (on from the next game)" : " (off from the next game)";
    } else {
        state = "live";
    }
    if (UI::TextElement* t = AsText(r.state.Get()))
        t->SetParameter(0, System::String(state.c_str()));

    // ModDialog's colours: green enabled, white disabled.
    if (r.border.Get() != nullptr) r.border->SetColor(r.pending ? 0x20c0ffc0u : 0x10ffffffu);
}

void ExtensionsDialog::Close() {
    Object::InternalPtr<UI::DialogInstance> self(this);
    _ui->CloseDialog(&self);
}

// The game reload ModDialog::CloseAndReload (0x14006e9b0) runs for a mods
// change: save-restore the running game and load it again through the menu.
// The new GameUpdateLoop takes the class lock with the new request
// (Ext::LockForGame, the EXT SEAM in GameUpdateLoop::Load).
void ExtensionsDialog::ReloadGame() {
    if (_parentMenu.Get() == nullptr) return;
    Save::SavedState* state = nullptr;
    if (void* mem = System::MemoryAllocator::MemAlloc(0x10, 4))
        state = ::new (mem) Save::SavedState();
    if (::Engine::MainLoop* loop = ::Engine::MainLoop::_singleton)
        if (::Engine::UpdateLoop* inner = loop->_currentLoop)
            inner->SaveRestore(true, state);
    Object::InternalPtr<UI::DialogInstance> none;
    _parentMenu.Get()->LoadFromSave(state, -1, &none);
}

// OK. Fixes and class A apply at once. A B / C change while a game holds the
// lock shows Dialog/Reload.rsc:game (the Mods dialog's notice): confirmed, the
// request is recorded and the game reloads under it; cancelled, the B / C
// changes are dropped (the live ones are still applied).
void ExtensionsDialog::Apply() {
    bool lockedChange = false;
    for (int i = 0; i < _rowCount; ++i) {
        const Ext::Entry& e = Ext::EntryAt(_rows[i].entry);
        if (_rows[i].pending != Ext::Requested(e) && Ext::EntryLocked(e)) lockedChange = true;
    }

    bool reload = false;
    if (lockedChange && GameUpdateLoop::_singleton != nullptr) {
        int result = 0;
        if (const UI::Dialog* rsc = LoadDialog("Dialog/Reload.rsc:game")) {
            MessageDialog* notice = nullptr;
            if (void* mem = System::MemoryAllocator::MemAlloc(sizeof(MessageDialog), 8))
                notice = ::new (mem) MessageDialog(_ui, rsc, Object::InternalPtr<UI::DialogInstance>());
            Object::InternalPtr<UI::DialogInstance> ref(notice);
            // BANISHED_MODAL_ANSWER: the harness answer, as ModDialog::Apply.
            if (Opt().modalAnswer) {
                result = Opt().modalAnswerValue;
                ref.DeletePtr();
            } else {
                result = _ui->ShowDialog(&ref);
                ref.DeletePtr();
            }
        }
        reload = static_cast<unsigned>(result - 1) < 2u;
        printf("[ExtDialog] B/C change in game: reload answer=%d -> %s\n", result,
               reload ? "reload" : "dropped");
    }

    bool any = false;
    for (int i = 0; i < _rowCount; ++i) {
        const Ext::Entry& e = Ext::EntryAt(_rows[i].entry);
        if (_rows[i].pending == Ext::Requested(e)) continue;
        if (Ext::EntryLocked(e) && !reload && GameUpdateLoop::_singleton != nullptr) continue;
        Ext::SetAndRecord(e, _rows[i].pending);
        any = true;
    }
    if (any) Ext::PersistSettings();

    if (reload) ReloadGame();
    Close();
}

void ExtensionsDialog::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t msg,
                                      int32_t id, int32_t c, int32_t d, int32_t e, void* userData) {
    if (msg == 0x12) {
        printf("[ExtDialog] click id=%d\n", id);
        if (id == 100) {
            Close();                                  // nothing applied
        } else if (id == 0x65) {
            Apply();
        } else if (id >= 1000 && id < 1000 + _rowCount) {
            Row& r = _rows[id - 1000];
            if (::Object::IsDerivedFromNullSafe(r.check.Get(), UI::CheckElement::TypeId()))
                r.pending = static_cast<UI::CheckElement*>(r.check.Get())->_checked != 0;
            else
                r.pending = !r.pending;
            RefreshRow(id - 1000);
        }
    }
    Object::InternalPtr<UI::Element> copy = *element;
    UI::DialogInstance::OnElementEvent(&copy, msg, id, c, d, e, userData);
}

// ===========================================================================
// Opener (MainMenuDialog::OnElementEvent, id 13)
// ===========================================================================
void ExtensionsDialog_OpenFromMainMenu(MainMenuDialog* menu) {
    if (menu == nullptr || menu->_ui == nullptr) return;
    const UI::Dialog* rsc = LoadDialog("Dialog/Mod.rsc");
    if (rsc == nullptr) return;
    void* mem = System::MemoryAllocator::MemAlloc(sizeof(ExtensionsDialog), 8);
    if (mem == nullptr) return;
    ExtensionsDialog* dlg = ::new (mem) ExtensionsDialog(menu->_ui, rsc,
                                                         Object::InternalPtr<MainMenuDialog>(menu));
    Object::InternalPtr<UI::DialogInstance> ref(dlg);
    menu->_ui->AddDialog(&ref);   // dispatches OnOpened
}

// ===========================================================================
// The menu button (EXT SEAM in MainMenuDialog::OnOpened)
// ===========================================================================
namespace {

// The icon: one process-lifetime sprite sheet over a procedurally drawn
// texture, built the way ModDialog::OnOpened builds a mod's thumbnail
// (ImageBuffer + MaterialInstance over Material/UI/UIMaterial.rsc +
// SpriteSheet). A cog with a plus in its hub, light on a dark rim, so it reads
// whichever byte order the format-5 texture takes (the greys are symmetric).
struct MenuIcon {
    Video::ImageBuffer*         image = nullptr;
    Graphics::MaterialInstance* material = nullptr;
    UI::SpriteSheet*            sheet = nullptr;
    int32_t                     w = 0, h = 0;
};
MenuIcon g_icon;

void DrawIcon(uint32_t* px, int w, int h) {
    const uint32_t kClear = 0x00000000u, kRim = 0xff2a2a2au, kFill = 0xffe8e8e8u;
    const float cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f, s = (w < h ? w : h) * 0.5f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float dx = (x - cx) / s, dy = (y - cy) / s;
            const float r2 = dx * dx + dy * dy;
            // Eight teeth: the outer radius steps out where |cos 4θ| is large.
            const float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
            const bool diagonal = (ax > 0.38f * (ax + ay) && ay > 0.38f * (ax + ay));
            const bool axial = (ax < 0.18f || ay < 0.18f);
            const float outer = (axial || diagonal) ? 0.96f : 0.78f;
            uint32_t c = kClear;
            if (r2 <= outer * outer) c = kRim;
            if (r2 <= (outer - 0.12f) * (outer - 0.12f)) c = kFill;
            if (r2 <= 0.40f * 0.40f) c = kRim;                       // the hub
            if (r2 <= 0.30f * 0.30f) {                               // a plus in the hub
                c = ((ax < 0.07f && ay < 0.22f) || (ay < 0.07f && ax < 0.22f)) ? kFill : kRim;
            }
            px[y * w + x] = c;
        }
    }
}

bool BuildIcon(int32_t w, int32_t h) {
    if (g_icon.sheet != nullptr) return g_icon.w == w && g_icon.h == h;
    if (!::Object::ExternalFactory::_singleton || !::Video::Device::_singleton) return false;

    void* imgMem = System::MemoryAllocator::MemAlloc(0x68, 8);
    void* matMem = System::MemoryAllocator::MemAlloc(0x60, 8);
    void* shtMem = System::MemoryAllocator::MemAlloc(0x58, 8);
    if (!imgMem || !matMem || !shtMem) return false;

    Video::ImageBuffer* image = ::new (imgMem) Video::ImageBuffer();
    image->Create(0, 2, 5, 0u, w, h, 1, 1, 1, 1, nullptr);   // ModDialog's thumbnail arguments
    {
        std::vector<uint32_t> pixels(static_cast<size_t>(w) * static_cast<size_t>(h), 0u);
        DrawIcon(pixels.data(), w, h);
        ::Video::Device::_singleton->Lock();
        image->CopyToTexture(pixels.data(), w, h);
        ::Video::Device::_singleton->Unlock();
    }

    Graphics::MaterialInstance* material = ::new (matMem) Graphics::MaterialInstance();
    {
        Graphics::Material* m = ::Object::ExternalFactory::_singleton->FindOrLoadAs<Graphics::Material>(
            System::Filename("Material/UI/UIMaterial.rsc"));
        if (!::Object::IsDerivedFromNullSafe(m, Graphics::Material::TypeId())) m = nullptr;
        material->SetMaterial(Object::ExternalPtr<Graphics::Material>(m));
        material->AddTexture(System::String("diffuse"), Object::ExternalPtr<Video::ImageBuffer>(image));
        material->BuildParameters();
    }

    UI::SpriteSheet* sheet = ::new (shtMem) UI::SpriteSheet();
    sheet->m_field30 = material;
    sheet->m_field48 = w;
    sheet->m_field4C = h;
    sheet->m_field50 = false;
    const float uv = kRdata_14017214c;
    sheet->AddSpriteInfo(System::String(), System::String(), 0, 0, w, h);
    sheet->SetSpriteInfo(0, w, h, 0.0f, 0.0f, uv, uv);

    g_icon.image = image;
    g_icon.material = material;
    g_icon.sheet = sheet;
    g_icon.w = w;
    g_icon.h = h;
    return true;
}

}  // namespace

void ExtensionsDialog_AttachMenuButton(MainMenuDialog* menu) {
    if (menu == nullptr || menu->_ui == nullptr || menu->_dialog == nullptr) return;
    if (Ext::GateProfileForced()) {
        printf("[ExtMenu] faithful profile forced on the command line: no Extensions button\n");
        return;
    }
    UI::Element* mod = Find(menu, "buttonMod");
    if (mod == nullptr || mod->_parent == nullptr || mod->_description == nullptr) {
        printf("[ExtMenu] no buttonMod / parent / description: no Extensions button\n");
        return;
    }

    // A second instance of the Mods button's own description, in its list.
    Object::InternalPtr<UI::Element> button;
    mod->_description->CreateInstance(&button, menu->_dialog);
    if (button.Get() == nullptr) {
        printf("[ExtMenu] CreateInstance failed\n");
        return;
    }
    UI::Element* list = mod->_parent;
    button->SetName(System::String("buttonExtensions"));
    button->SetElementId(kExtensionsMenuButtonId);
    list->AddChild(&button);

    // Beside Mods: move the appended child from the end to just after buttonMod.
    UI::Element** kids = list->_children.GetData();
    const int n = list->_children.GetSize();
    int modIndex = -1;
    for (int i = 0; i < n; ++i) if (kids[i] == mod) { modIndex = i; break; }
    if (modIndex >= 0 && n >= 2 && kids[n - 1] == button.Get()) {
        UI::Element* added = kids[n - 1];
        for (int i = n - 1; i > modIndex + 1; --i) kids[i] = kids[i - 1];
        kids[modIndex + 1] = added;
    }
    // In-game the Mods button is disabled for some games (game flag bit 10);
    // follow it.
    if (!mod->IsEnabled()) button->Enable(false);

    // The label: "Extensions" and the icon.
    UI::Element* labelElem = button->FindElement(System::String("labelMod")).Get();
    if (::Object::IsDerivedFromNullSafe(labelElem, UI::LabelElement::TypeId())) {
        UI::LabelElement* label = static_cast<UI::LabelElement*>(labelElem);
        SetElementText(label, ParameterString(), "Extensions", true);

        int32_t w = 32, h = 32;
        if (label->_sprite.spriteSheet != nullptr && label->_sprite.spriteIndex >= 0) {
            w = label->_sprite.Width();
            h = label->_sprite.Height();
        }
        if (w < 8 || w > 128) w = 32;
        if (h < 8 || h > 128) h = 32;
        if (BuildIcon(w, h)) {
            UI::Sprite sprite(Object::ExternalPtr<UI::SpriteSheet>(g_icon.sheet), 0);
            label->SetSprite(&sprite);
        }
    } else {
        printf("[ExtMenu] no labelMod in the clone: the button keeps the Mods label\n");
    }
    printf("[ExtMenu] Extensions button added (id %d, list %p, after index %d of %d)\n",
           kExtensionsMenuButtonId, (void*)list, modIndex, n);
}
