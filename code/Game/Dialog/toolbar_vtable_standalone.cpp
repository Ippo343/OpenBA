// code/Game/Dialog/toolbar_vtable_standalone.cpp
// Standalone-only scaffolding for the in-game Toolbar HUD (Keystone F).
//
// ENDGAME B.3.8 block DLG: ToolbarDialog is a real class (Dialog/ToolbarDialog.h)
// with a compiler vtable. The 10-slot hand-built mirror of exe 0x14015fa00 that
// lived here (base slots resolved by export name, published into
// g_ToolbarDialog_Vtable for the ctor to stamp) is gone: those slots are inherited
// from UI::DialogInstance, the same Runtime bodies.
//
// What remains: ToolbarStandalone_Init() fills the toolbar type-id globals the
// ctor / Update read, which the exe's *_RegisterType functions fill in retail:
//   g_TypeId_ToolbarItem               = "Toolbar"            (0x1401c6128, ctor FindNext)
//   g_TypeId_ToolbarDescription_FC = "ToolbarDescription" (0x1401c7f58, ctor + UpdateButtons)
//   g_TypeId_HelpButtonElem            = "ButtonElement"      (exref 0x1401514f8, ctor cast)
//   g_TypeId_ImageElement              = "ImageElement"       (exref 0x140151500, ctor sprite)
//   g_TypeId_CheckElement            = "CheckElement"       (exref 0x1401516a8, Update)
//   g_TypeId_TextElement               = "TextElement"        (exref 0x140151508, Update SpeedDisplay)


#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include "Component/ToolbarDescription.h"   // X::TypeId() (B.3.8 callers)

// Idempotent: fill the toolbar type-id globals. Called from GameUpdateLoop::Startup
// before the toolbar is built. Must run AFTER Engine::Startup (the type ids are the
// real CRC ids only then) and after EngineFuncsInit (which leaves the toolbar
// globals null in standalone).
extern "C" void ToolbarStandalone_Init() {
    static bool s_done = false;
    if (s_done) return;

    // Fill the toolbar type-id globals (null in standalone). Backing storage lives
    // here; the engine_funcs globals point at it. Only override if still null so we
    // never clobber an id another standalone path already filled.
    //
    // Use ResolveTypeIdByNameLazy (not GetTypeIdByName): it queries the LIVE Runtime
    // Factory tree, so it resolves BOTH the exe-registered types (Toolbar /
    // ToolbarDescription) AND the Runtime-self-registered UI element types
    // (ButtonElement / ImageElement / CheckElement / TextElement). GetTypeIdByName
    // only knows our exe-side REGISTER_TYPE list and would return 0 for the UI types.
    static uint32_t s_tid_Toolbar = 0, s_tid_ToolbarDesc = 0, s_tid_Button = 0,
                    s_tid_Image = 0, s_tid_Check = 0, s_tid_Text = 0;
    s_tid_Toolbar     = ::Object::ResolveTypeIdByNameLazy("Toolbar");
    s_tid_ToolbarDesc = ToolbarDescription::TypeId();
    s_tid_Button      = ::Object::ResolveTypeIdByNameLazy("ButtonElement");
    s_tid_Image       = ::Object::ResolveTypeIdByNameLazy("ImageElement");
    s_tid_Check       = ::Object::ResolveTypeIdByNameLazy("CheckElement");
    s_tid_Text        = ::Object::ResolveTypeIdByNameLazy("TextElement");
    if (!g_TypeId_ToolbarItem)               g_TypeId_ToolbarItem = &s_tid_Toolbar;
    if (!g_TypeId_ToolbarDescription_FC) g_TypeId_ToolbarDescription_FC = &s_tid_ToolbarDesc;
    if (!g_TypeId_HelpButtonElem)            g_TypeId_HelpButtonElem = &s_tid_Button;
    if (!g_TypeId_ImageElement)              g_TypeId_ImageElement = &s_tid_Image;
    if (!g_TypeId_CheckElement)            g_TypeId_CheckElement = &s_tid_Check;
    if (!g_TypeId_TextElement)               g_TypeId_TextElement = &s_tid_Text;

    fprintf(stderr, "[HUD] ToolbarStandalone_Init: "
            "Toolbar=%u ToolbarDesc=%u Button=%u Image=%u Check=%u Text=%u\n",
            s_tid_Toolbar, s_tid_ToolbarDesc, s_tid_Button, s_tid_Image, s_tid_Check, s_tid_Text);
    fflush(stderr);
    s_done = true;
}

