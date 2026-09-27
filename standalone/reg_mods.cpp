// standalone/reg_mods.cpp
// Lane 4 (local mod list) address registrations. Wired into RegisterCodeFunctions()
// in standalone/address_registry.cpp via RegisterModsBatch().
//
// Every address below was confirmed a real function/vtable ENTRY in Ghidra
// (get_function_by_address: Entry == address, or read_memory over the .rdata
// table). No mid-function raw-address resolve
// ([[reference-midfunction-raw-address-resolves]]).
//
//   0x14015c870  vtable_ModDialog          (.rdata, 10 slots, next table @0x14015c8c0)
//   0x14015c670  vtable_ModDetailsDialog   (.rdata, 10 slots)
//   0x140070030  ModDialog::ModDialog      (Entry 140070030, body ..0x1400700b0)
//   0x14006eec0  ModDialog::RefreshDisplay (Entry 14006eec0, body ..0x14006f146)
//   0x1401519e8  .rdata exref -> IO::Modification::_singleton (Runtime 0x1801fc6b0)
//
// The last one is a DISPATCH-SLOT registration, not a code address: engine_funcs.cpp
// reads `exref = ResolveAddr(Exe, 0x1401519e8); g_ppModificationSingleton = *exref;`
// so the slot must hold the ADDRESS of the Runtime's `Modification* _singleton`
// variable. Verified in Runtime-x64.dll: `IO::Modification::Get` (0x18000dab0) is
// literally `MOV RAX, qword ptr [0x1801fc6b0]; RET`. Without this registration
// g_ppModificationSingleton stays null and every ModDialog/ModDetailsDialog body
// null-derefs on its first `*g_ppModificationSingleton`. Only those two files read
// the global (grep: 8 sites, all in code/Game/Dialog/mod*.cpp), so publishing it
// cannot perturb anything else.
//
// NOT registered here, deliberately — the three remaining entries of
// notes/wave_remaining/lane4_mods.txt are Ghidra MIS-LABELS, not ModDialog code:
//   0x1400938d0 "ModDialog_helper_38d0"        = Weather::DrawUpdate
//                                                 (code/Game/Game/weather_sun.cpp)
//   0x140095da0 "ModDialog_impl_6010_sub_5da0" = GameUpdateLoop::TakeScreenshot
//                                                 NOT a "QuickSave handler" -- that name
//                                                 was a misnomer and it hid a live crash:
//                                                 gameupdateloop.cpp calls it UNGUARDED at
//                                                 :2060/:2135 via fn_QuickSave, so with this
//                                                 unregistered, pressing quicksave (key 0x21)
//                                                 or quickload (0x22) was an EXEC-of-0.
//                                                 Registered by reg_w2_screenshot.cpp
//                                                 (NOT reg_code_lane4.cpp -- an earlier
//                                                 revision of this comment said so and was
//                                                 wrong; lane 4 identified it but did not
//                                                 register it).
//   0x1400a5140 "ModDialog_impl_5140"          = ImageCapture::DrawCheck / minimap
//                                                 paint (code/Game/Game/minimap_paint.cpp)
// All three are consumed by code/Game/Game/gameupdateloop.cpp, which already owns
// their null-fallbacks. Registering them from a mod-dialog batch would silently
// change in-game rendering behaviour. Left to the GameUpdateLoop owner.

#include "resolve.h"
#include <cstdint>

// code/Game/Dialog/mod_dialog_vtables.cpp
extern "C" void* ModDetailsDialog_GetVtable();
extern "C" void* ModDialog_Ctor_Standalone(void* self, void* userInterface,
                                           void* externalRes, void* parentIBP,
                                           void* parentMenuIBP);
extern "C" void ModDialog_CloseAndReload(void* self);          // 0x14006e9b0
// code/Game/Dialog/moddialog.cpp
extern "C" void ModDialog_RefreshDisplay(void* self);
// code/Game/Game/worldsetup_reload.cpp
extern "C" void WorldSetup_LoadFromSave(void* worldSetup, void* savedState,
                                        void* scenarioIdx, void* retvalIbpPtr);
// code/Game/Dialog/mainmenudialog.cpp — EXISTING faithful port of exe 0x1400b4790
// ("ReturnToMenu (0x1400B4790)" per its own comment at :189-192). It was never
// registered at that address, so fn_MainMenu_Reload stayed NULL. Declared here by
// hand because mainmenudialog.cpp has no header; the pointer parameter makes the
// C++ mangling independent of MainMenuDialog's definition.
class MainMenuDialog;
void MainMenuDialog_ReturnToMenu(MainMenuDialog* self);

// Backing storage for the IO::Modification singleton exref slot. Must outlive the
// process; a file-scope static is exactly the .rdata cell the exe would have had.
static uintptr_t g_slot_ModificationSingleton = 0;

void RegisterModsBatch() {
    RegisterExeAddr(0x14015c670, reinterpret_cast<uintptr_t>(ModDetailsDialog_GetVtable()));
    RegisterExeAddr(0x140070030, reinterpret_cast<uintptr_t>(&ModDialog_Ctor_Standalone));
    RegisterExeAddr(0x14006eec0, reinterpret_cast<uintptr_t>(&ModDialog_RefreshDisplay));

    // --- Apply / reload path (Lane 1's EXEC-of-0 sweep) ----------------------
    // All three sit on the path a user walks EVERY time they enable a mod:
    // ModDialog_Apply_Impl -> CloseAndReload -> MainMenu_Reload (menu context)
    // or -> Game_ReloadWithSave (in-game context).
    RegisterExeAddr(0x14006e9b0, reinterpret_cast<uintptr_t>(&ModDialog_CloseAndReload));
    RegisterExeAddr(0x1400b4790, reinterpret_cast<uintptr_t>(&MainMenuDialog_ReturnToMenu));
    // ONE registration for BOTH fn_Game_ReloadWithSave (moddialog.cpp) and
    // fn_LoadScenarioFromSave (scenariodialog.cpp) — same retail function, same
    // ABI, verified by disassembly. See code/Game/Game/worldsetup_reload.cpp.
    RegisterExeAddr(0x1400b49f0, reinterpret_cast<uintptr_t>(&WorldSetup_LoadFromSave));

    // Unattended diagnostic: arms a background thread that waits for
    // Engine::Startup to publish IO::Modification::_singleton, then dumps the mod
    // list + the FileSystem package list. No click, no BANISHED_LIVE_MODDIALOG.
    // No-op unless BANISHED_MOD_PROBE is set.

    g_slot_ModificationSingleton = ResolveAddr(Module::Runtime, 0x1801fc6b0);
    RegisterExeAddr(0x1401519e8,
                    reinterpret_cast<uintptr_t>(&g_slot_ModificationSingleton));
}
