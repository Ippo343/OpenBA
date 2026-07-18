// code/Game/Dialog/toolbar_vtable_standalone.cpp
// Standalone-only scaffolding for the in-game Toolbar HUD (Keystone F).
//
// In the retail exe the ToolbarDialog vtable lives at .rdata 0x14015fa00 and the
// toolbar type-id globals are filled by the exe's *_RegisterType functions. In the
// standalone replacement exe both are absent (ResolveAddr(Exe, …) → 0/null), so the
// gated HUD path (BANISHED_LIVE_TOOLBAR, in gameupdateloop.cpp) cannot construct a
// ToolbarDialog. This file synthesizes:
//   (1) a 10-slot ToolbarDialog vtable that mirrors exe 0x14015fa00 — overridden
//       slots point at our already-ported ToolbarDialog_* methods; the three
//       non-overridden base slots (+0x18/+0x38/+0x48) plus TypeName/GetTypeId
//       (+0x08/+0x10) forward to the raw Runtime-x64.dll UI::DialogInstance virtuals
//       (exactly the targets the exe thunk-slots JMP to); and
//   (2) ToolbarStandalone_Init(): fills the six toolbar type-id globals the ctor /
//       Update read, then publishes the vtable into g_ToolbarDialog_Vtable.
//
// Vtable slot map (verified via read_memory @ 0x14015fa00 + per-slot decompile):
//   +0x00 0x140021760  ScalarDtor  (own; frees 3 dyn arrays + base dtor + free)
//   +0x08 0x140021700  GetTypeName : returns DAT_1401c5d40 (ToolbarDialog Type*)
//   +0x10 0x1400216f0  GetTypeId   : returns DWORD_1401c5d38 (ToolbarDialog type-id)
//   +0x18 0x14003a948  Serialize1  -> Runtime 0x1801b8444   (base DialogInstance)
//   +0x20 0x1400b97c0  Startup     : RET 0
//   +0x28 0x1400b97b0  Shutdown    : -> CloseSubDialog
//   +0x30 0x1400b8d40  Update      : sync toolbar buttons with game state
//   +0x38 0x14003a990  OnUpdate    -> Runtime 0x1801b943c   (base DialogInstance)
//   +0x40 0x1400b92a0  OnEvent     : handle toolbar button clicks
//   +0x48 0x14003a954  Serialize2  -> Runtime 0x1801b84a2   (base DialogInstance)
//
// Type-id globals filled (engine_funcs labels in parens; Ghidra-verified true names):
//   g_TypeId_ToolbarItem               = "Toolbar"            (0x1401c6128, ctor FindNext)
//   g_TypeId_BuildRequirementComponent = "ToolbarDescription" (0x1401c7f58, ctor + UpdateButtons)
//   g_TypeId_HelpButtonElem            = "ButtonElement"      (exref 0x1401514f8, ctor cast)
//   g_TypeId_ImageElement              = "ImageElement"       (exref 0x140151500, ctor sprite)
//   g_TypeId_ToolbarElement            = "CheckElement"       (exref 0x1401516a8, Update)
//   g_TypeId_TextElement               = "TextElement"        (exref 0x140151508, Update SpeedDisplay)
//
// Provides the standalone exe vtable + type-ids for the toolbar.


#include "engine_funcs.h"
#include "Dialog/ui_dialogs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>

// ---- Mangled names of the base UI::DialogInstance virtuals the exe ToolbarDialog
//      vtable's non-overridden slots JMP to (the exe thunks 0x14003a948 / 0x14003a990 /
//      0x14003a954 and the TypeName/Type thunks 0x14003ab4c / 0x14003ab52 forward to
//      these Runtime exports via the IAT-style dispatch slots). Resolved by export name
//      at init (ResolveExport, ASLR-safe).
//
// NB: these are resolved by NAME, not by a hardcoded RVA. The dispatch-slot value in
// the exe (.rdata) is the exe-offset of the mangled-name STRING, not the Runtime RVA —
// the earlier ToolUI child-dialog vtable mistook that string-offset for a Runtime RVA
// (a latent bug: those slots are never dispatched in the OpenToolDialog path, so it
// never crashed). The toolbar IS updated every frame, so it would call the bogus
// +0x38 target and EXEC-fault. Resolving by name avoids the whole class of error.
static constexpr const char* kSym_TypeName   = "?TypeName@DialogInstance@UI@@UEBAPEBDXZ";              // +0x08
static constexpr const char* kSym_Type       = "?Type@DialogInstance@UI@@UEBAIXZ";                     // +0x10
static constexpr const char* kSym_Persist    = "?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z";   // +0x18
static constexpr const char* kSym_OnUpdate   = "?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z"; // +0x38
static constexpr const char* kSym_OnSerSettings = "?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z"; // +0x48

// ---- Slot 0: scalar-deleting destructor (self, flags) ----
// Faithful port of exe 0x140021760 + its inner ~ToolbarDialog: frees the three
// dynamic arrays (+0xF8 speed values, +0xD0 toolbar items, +0xC0 button elements),
// runs the base DialogInstance destructor, then conditionally frees self. Mirrors
// engine/reimpl/toolbardialog.cpp::Reimpl_ScalarDtor.
static void* ToolbarDialog_ScalarDtor_SA(void* self, uint32_t flags) {
    if (!self) return self;
    uintptr_t s = (uintptr_t)self;
    if (fn_MemFreeStatic) {
        fn_MemFreeStatic(*(void**)(s + 0xF8)); *(uint64_t*)(s + 0xF8) = 0; *(uint64_t*)(s + 0x100) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xD0)); *(uint64_t*)(s + 0xD0) = 0; *(uint64_t*)(s + 0xD8) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xC0)); *(uint64_t*)(s + 0xC0) = 0; *(uint64_t*)(s + 0xC8) = 0;
    }
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// The 10-slot standalone ToolbarDialog vtable (mirror of exe 0x14015fa00). The
// base-forward slots (+0x08/+0x10/+0x18/+0x38/+0x48) are filled at init with the raw
// Runtime virtuals so they are dispatched with their exact native arg lists; kept
// null here so a never-initialized table is inert.
void* g_ToolbarDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&ToolbarDialog_ScalarDtor_SA), // +0x00 ScalarDtor
    nullptr,                                                // +0x08 TypeName   (Runtime base)
    nullptr,                                                // +0x10 GetTypeId  (Runtime base)
    nullptr,                                                // +0x18 Serialize1 (Runtime base)
    reinterpret_cast<void*>(&ToolbarDialog_Startup),        // +0x20 Startup (RET 0)
    reinterpret_cast<void*>(&ToolbarDialog_Shutdown),       // +0x28 Shutdown
    reinterpret_cast<void*>(&ToolbarDialog_Update),         // +0x30 Update
    nullptr,                                                // +0x38 OnUpdate   (Runtime base)
    reinterpret_cast<void*>(&ToolbarDialog_OnEvent),        // +0x40 OnEvent
    nullptr,                                                // +0x48 Serialize2 (Runtime base)
};

// Idempotent bring-up: fill the base-forward vtable slots + the toolbar type-id
// globals, then publish the vtable into the engine_funcs global the ctor reads.
// Called from GameUpdateLoop::Startup ONLY when BANISHED_LIVE_TOOLBAR is set (so the
// default build never touches any of these — it stays byte-identical). Must run AFTER
// Engine::Startup (GetTypeIdByName yields the real CRC ids only then) and after
// EngineFuncsInit (which leaves the toolbar globals null in standalone) — both are
// satisfied at GameUpdateLoop::Startup time (the world is already built).
extern "C" void ToolbarStandalone_Init() {
    static bool s_done = false;
    if (s_done) return;

    auto* vt = g_ToolbarDialog_Vtable_Standalone;
    if (!vt[1]) vt[1] = reinterpret_cast<void*>(ResolveExport(kSym_TypeName));      // +0x08
    if (!vt[2]) vt[2] = reinterpret_cast<void*>(ResolveExport(kSym_Type));          // +0x10
    if (!vt[3]) vt[3] = reinterpret_cast<void*>(ResolveExport(kSym_Persist));       // +0x18
    if (!vt[7]) vt[7] = reinterpret_cast<void*>(ResolveExport(kSym_OnUpdate));      // +0x38
    if (!vt[9]) vt[9] = reinterpret_cast<void*>(ResolveExport(kSym_OnSerSettings)); // +0x48
    // Retry later if any base virtual failed to resolve (a partially-null vtable must
    // never go live — the dialog gets per-frame OnUpdate via slot +0x38).
    if (!vt[1] || !vt[2] || !vt[3] || !vt[7] || !vt[9]) {
        fprintf(stderr, "[HUD] ToolbarStandalone_Init: base-virtual resolve incomplete "
                "(TypeName=%p Type=%p Persist=%p OnUpdate=%p OnSerSettings=%p) — retry\n",
                vt[1], vt[2], vt[3], vt[7], vt[9]);
        fflush(stderr);
        return;
    }

    // Publish the vtable for ToolbarDialog_Ctor_Impl (toolbardialog.cpp:878).
    g_ToolbarDialog_Vtable = reinterpret_cast<uintptr_t>(g_ToolbarDialog_Vtable_Standalone);

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
    s_tid_ToolbarDesc = ::Object::ResolveTypeIdByNameLazy("ToolbarDescription");
    s_tid_Button      = ::Object::ResolveTypeIdByNameLazy("ButtonElement");
    s_tid_Image       = ::Object::ResolveTypeIdByNameLazy("ImageElement");
    s_tid_Check       = ::Object::ResolveTypeIdByNameLazy("CheckElement");
    s_tid_Text        = ::Object::ResolveTypeIdByNameLazy("TextElement");
    if (!g_TypeId_ToolbarItem)               g_TypeId_ToolbarItem = &s_tid_Toolbar;
    if (!g_TypeId_BuildRequirementComponent) g_TypeId_BuildRequirementComponent = &s_tid_ToolbarDesc;
    if (!g_TypeId_HelpButtonElem)            g_TypeId_HelpButtonElem = &s_tid_Button;
    if (!g_TypeId_ImageElement)              g_TypeId_ImageElement = &s_tid_Image;
    if (!g_TypeId_ToolbarElement)            g_TypeId_ToolbarElement = &s_tid_Check;
    if (!g_TypeId_TextElement)               g_TypeId_TextElement = &s_tid_Text;

    fprintf(stderr, "[HUD] ToolbarStandalone_Init: vtable=%p [+18 Persist=%p +38 OnUpdate=%p "
            "+48 OnSerSet=%p] Toolbar=%u ToolbarDesc=%u Button=%u Image=%u Check=%u Text=%u\n",
            reinterpret_cast<void*>(g_ToolbarDialog_Vtable_Standalone), vt[3], vt[7], vt[9],
            s_tid_Toolbar, s_tid_ToolbarDesc, s_tid_Button, s_tid_Image, s_tid_Check, s_tid_Text);
    fflush(stderr);
    s_done = true;
}

