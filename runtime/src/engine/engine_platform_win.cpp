// ============================================================================
// runtime/src/engine/engine_platform_win.cpp — Wave 23 lane ENGINE
// The Win32 half of ?PlatformConfigure@Engine@1@ (retail 0x1800a1670).
// ============================================================================
//
// PLAN.md §4.1 decision 1 keeps <windows.h> out of every non-`_win` file under
// runtime/src/. PlatformConfigure makes exactly four kinds of OS call and they
// all live here:
//
//   0x1800a16f7  CALL qword ptr [0x18014b548]   ; user32!GetAsyncKeyState(VK_LSHIFT)
//   0x1800a170e  CALL qword ptr [0x18014b548]   ; user32!GetAsyncKeyState(VK_RSHIFT)
//   0x1800a17d4  CALL qword ptr [0x18014b4d0]   ; user32!DialogBoxParamW
//   (+ the two launcher checkbox bytes the dialog proc writes)
//
// The IAT slots were read out of Runtime-x64.dll's import directory, not
// guessed from a Ghidra label. 0xA2 / 0xA3 are VK_LSHIFT / VK_RSHIFT and the
// mask is 0x8000, the "down right now" bit — NOT 0x0001, the "pressed since
// last call" bit. That distinction decides whether holding shift at launch
// opens the launcher, which is the documented retail behaviour.
//
// ---------------------------------------------------------------------------
// WHY THE DIALOG ITSELF IS NOT PORTED, AND WHAT HAPPENS AT CUTOVER
// ---------------------------------------------------------------------------
// `DialogBoxParamW(DAT_1801fc680, MAKEINTRESOURCE(0x66), NULL, FUN_1800a1ae0, 0)`
// needs three things that exist only inside Runtime-x64.dll:
//
//   * the dialog TEMPLATE, resource id 0x66, in the DLL's .rsrc — with the
//     options sub-dialog 0x68 and the bitmap 0x2711 it draws;
//   * the dialog PROCEDURE 0x1800a1ae0 (and 0x1800a1dc0, 0x1800a2020), none of
//     which is exported — checked against the shipped export directory with
//     pefile, not assumed;
//   * the module's own HINSTANCE, 0x1801fc680, also unexported.
//
// So with the DLL present this is byte-faithful — we call the DLL's own proc
// against the DLL's own template, exactly as retail does. With the DLL ABSENT
// there is no template and no proc, and there is no honest way to synthesise
// one here. This file therefore REFUSES BY NAME rather than jumping to null.
//
// That is a real behavioural divergence at cutover and it is written down
// rather than smoothed over: an ATTENDED cutover run whose arming bit is set
// gets no launcher and proceeds as if the player had pressed Play. An
// UNATTENDED run never reaches this code at all, because
// HarnessGuardUnattendedBoot (standalone/main.cpp:184) clears the arming bit.
//
// ADDRESSEE FOR THE RESIDUAL: this is handed to the integrator as a scoping
// item for a future LAUNCHER lane — porting the three dialog procs AND adding
// the templates to the replacement exe's own resource script. It is not "the
// next lane that touches this file"; a condition with no name never fires.
// ============================================================================

#include <windows.h>
#include <cstdint>
#include <cstdio>

namespace {

// GetModuleHandleW, not a cached ResolveInit handle: this file must be able to
// tell "the DLL is loaded" from "it is not", and that is exactly the question
// GetModuleHandleW answers. A null here IS the cutover condition.
HMODULE RuntimeModule() {
    return GetModuleHandleW(L"Runtime-x64.dll");
}

// The two launcher checkbox bytes the DLL's dialog proc writes. Unexported, so
// they are reached as module-relative offsets off the loaded base — and only
// ever when that base exists. At cutover both read as 0, which is precisely
// what retail sees when neither box was ticked, so the caller's `|| command
// line` fallback behaves correctly rather than accidentally.
const uint8_t* LauncherFlagByte(uintptr_t ghidraAddr) {
    HMODULE h = RuntimeModule();
    if (!h) return nullptr;
    return reinterpret_cast<const uint8_t*>(
        reinterpret_cast<uintptr_t>(h) + (ghidraAddr - 0x180000000ull));
}

bool g_refusalPrinted = false;

}  // namespace

bool RtEnginePlatformShiftHeld() {
    return (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0
        || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
}

// Retail returns the DialogBoxParamW result and the caller compares it to 1.
long long RtEnginePlatformShowLauncherDialog() {
    HMODULE h = RuntimeModule();
    if (h) {
        // 0x1801fc680 is the DLL's HINSTANCE, which for a DLL IS its HMODULE —
        // so `h` is that value, not a substitute for it.
        DLGPROC proc = reinterpret_cast<DLGPROC>(
            reinterpret_cast<uintptr_t>(h) + (0x1800a1ae0ull - 0x180000000ull));
        return static_cast<long long>(
            DialogBoxParamW(h, MAKEINTRESOURCEW(0x66), nullptr, proc, 0));
    }

    if (!g_refusalPrinted) {
        g_refusalPrinted = true;
        fprintf(stderr,
            "[RtPort] Engine::PlatformConfigure: REFUSING to show the launcher "
            "dialog -- Runtime-x64.dll is not loaded, so dialog template 0x66, "
            "bitmap 0x2711 and dialog proc 0x1800a1ae0 do not exist in this "
            "process. Proceeding as if Play had been pressed. This branch is "
            "the Half B residual named in runtime/src/engine/engine_platform.cpp; "
            "an unattended run never reaches it (HarnessGuardUnattendedBoot "
            "clears the arming bit).\n");
    }
    return 1;   // IDOK == Play
}

bool RtEnginePlatformLauncherFlagNoMods() {
    const uint8_t* p = LauncherFlagByte(0x1801fc699);
    return p && *p != 0;
}

bool RtEnginePlatformLauncherFlagReset() {
    const uint8_t* p = LauncherFlagByte(0x1801fc698);
    return p && *p != 0;
}

// ---------------------------------------------------------------------------
// The one OS call in ?LoadResources@UpdateLoop@Engine@@ (0x1800dd9c4):
//     MOV RCX,[RAX] ; CALL qword ptr [0x18014b0d0]   ; kernel32!GetFileAttributesW
//     CMP EAX,-1    ; SETNZ
// The IAT slot was read out of Runtime-x64.dll's import directory. The test is
// against INVALID_FILE_ATTRIBUTES (0xFFFFFFFF) and NOTHING else — a directory
// and a read-only file both count as "exists", which is retail's behaviour.
// ---------------------------------------------------------------------------
bool RtEnginePlatformWidePathExists(const wchar_t* path) {
    if (path == nullptr) return false;
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}
