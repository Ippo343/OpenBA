// ============================================================================
// src/platform/win/engine_platform_win.cpp — Wave 23 lane ENGINE
// The Win32 half of ?PlatformConfigure@Engine@1@ (retail 0x1800a1670).
// ============================================================================
//
// PLAN.md §4.1 decision 1 keeps <windows.h> out of every non-`_win` file under
// src/engine/. PlatformConfigure's OS calls, and what is left of them here:
//
//   0x1800a16f7  CALL qword ptr [0x18014b548]   ; user32!GetAsyncKeyState(VK_LSHIFT)
//   0x1800a170e  CALL qword ptr [0x18014b548]   ; user32!GetAsyncKeyState(VK_RSHIFT)
//   0x1800a17d4  CALL qword ptr [0x18014b4d0]   ; user32!DialogBoxParamW -- REPLACED
//   (+ the two launcher check-box bytes the dialog proc writes -- REPLACED)
//
// The IAT slots were read out of Runtime-x64.dll's import directory, not
// guessed from a Ghidra label. 0xA2 / 0xA3 are VK_LSHIFT / VK_RSHIFT and the
// mask is 0x8000, the "down right now" bit — NOT 0x0001, the "pressed since
// last call" bit. That distinction decides whether holding shift at launch
// takes Half B, which is the documented retail behaviour.
//
// ---------------------------------------------------------------------------
// THE LAUNCHER DIALOG IS REPLACED, NOT PORTED (Phase D.4 SETTINGS E.5)
// ---------------------------------------------------------------------------
// `DialogBoxParamW(DAT_1801fc680, MAKEINTRESOURCE(0x66), NULL, FUN_1800a1ae0, 0)`
// needed the DLL's template 0x66 (with the options sub-dialog 0x68 and the
// bitmap 0x2711), its unexported procs 0x1800a1ae0 / 0x1800a1dc0 / 0x1800a2020
// and its HINSTANCE 0x1801fc680. The user decided (ENDGAME §E item 2) that it
// is not ported as a Win32 dialog: its two check boxes are the settings.cfg
// entries `LauncherDisableMods` / `LauncherResetSettings`, read in
// src/engine/engine/engine_platform.cpp; the settings its video page edited
// are the `Video*` entries the file already carries. The refusal function that stood
// here (`RtEnginePlatformShowLauncherDialog`) is deleted; known_port_issues
// row 34 is FIXED by it.
// ============================================================================

#include <windows.h>
#include <cstdint>

bool RtEnginePlatformShiftHeld() {
    return (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0
        || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
}

// ---------------------------------------------------------------------------
// The one OS call in ?LoadResources@UpdateLoop@Engine@@ (0x1800dd9c4):
//     MOV RCX,[RAX] ; CALL qword ptr [0x18014b0d0]   ; kernel32!GetFileAttributesW
//     CMP EAX,-1    ; SETNZ
// The IAT slot was read out of Runtime-x64.dll's import directory. The test is
// against INVALID_FILE_ATTRIBUTES (0xFFFFFFFF) and NOTHING else — a directory
// and a read-only file both count as "exists", which is retail's behaviour.
// ---------------------------------------------------------------------------
bool RtEnginePlatformWidePathExists(const char16_t* path) {
    if (path == nullptr) return false;
    return GetFileAttributesW(reinterpret_cast<const wchar_t*>(path)) != INVALID_FILE_ATTRIBUTES;  // CAST-OK: win32
}
