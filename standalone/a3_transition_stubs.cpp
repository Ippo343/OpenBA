// ============================================================================
// standalone/a3_transition_stubs.cpp -- TEMPORARY. DELETE WITH ITS CALLERS.
// ============================================================================
//
// ENDGAME.md A.3 deleted the 19 detour/hook installers in standalone/. They
// patched code bytes inside Runtime-x64.dll, and since A.1 that module is never
// mapped, so every one of them returned false / null on every run.
//
// A handful of bodies under runtime/src/ still CALL them. Those files are owned
// by the other A.3 batches, so this file keeps this branch linking until their
// call sites go. Every stub here answers exactly what the real installer
// answered on a DLL-absent run -- `false` for "no detour installed", `nullptr`
// for "no trampoline holding retail's displaced prologue" -- so the surviving
// behaviour is the behaviour of the run that already ships.
//
// THE FIX IS TO DELETE THE CALLERS, NOT TO KEEP THIS FILE. Each call site is of
// the shape `if (void* tramp = ...Trampoline()) { <retail path> }` or
// `detoured = ...InstallDetour(&ours)`, and the arm that survives is the one
// taken when the answer is null/false. Current callers:
//
//   runtime/src/engine/updateloop_loadresources.cpp:335,336
//   runtime/src/graphics/wave21_lane_gfxbody.cpp:370
//   runtime/src/object/external_find.cpp:240,292
//   runtime/src/object/external_path_win.cpp:630,689,750,981
//   runtime/src/ui/elementdescription_serialize.cpp:240,311
//   runtime/src/ui/userinterface_close.cpp:608,705
//   runtime/src/ui/userinterface_draw.cpp:723,785
//   runtime/src/ui/userinterface_frame.cpp:589,667
//   runtime/src/ui/widgets_layout.cpp:766,1178
//   standalone/resolve_standalone.cpp:1032   (RtTInfoDoorNote; coordinator's file)
// ============================================================================

extern "C" {

bool  RtEngineLoadResourcesDetourOptedIn()          { return false; }
bool  RtEngineLoadResourcesInstallDetour(void*)     { return false; }

bool  RtGraphicsMatSetupInstallDetour(void*)        { return false; }

bool  RtResFindInstallDetour(void*)                 { return false; }
void* RtResFindTrampoline()                         { return nullptr; }

bool  RtResPathInstallDetour(void*)                 { return false; }
void* RtResPathTrampoline()                         { return nullptr; }

bool  RtUiElementDescSerializeInstallDetour(void*)  { return false; }
void* RtUiElementDescSerializeTrampoline()          { return nullptr; }

bool  RtUiCloseDialogInstallDetour(void*)           { return false; }
void* RtUiCloseDialogTrampoline()                   { return nullptr; }

bool  RtUiUserInterfaceDrawInstallDetour(void*)     { return false; }
void* RtUiUserInterfaceDrawTrampoline()             { return nullptr; }

bool  RtUiUserInterfaceUpdateInstallDetour(void*)   { return false; }
void* RtUiUserInterfaceUpdateTrampoline()           { return nullptr; }

bool  RtUiUpdateContainersInstallDetour(void*)      { return false; }
void* RtUiUpdateContainersTrampoline()              { return nullptr; }

}  // extern "C"

// standalone/tinfo_door_screen.cpp's recorder, called from
// standalone/resolve_standalone.cpp (the coordinator's file). The screen it fed
// compared our minted BaseTypeInfo nodes against the loaded DLL's, so there is
// nothing left for it to record.
void RtTInfoDoorNote(const char*) {}
