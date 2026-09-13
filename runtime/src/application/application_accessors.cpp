// ============================================================================
// runtime/src/application/application_accessors.cpp — Wave 12 lane A
//   Application::Application::Get / GetInstance / GetWindow / IsCursorEnabled /
//   PlatformIsPaused / PlatformDestruct
// ============================================================================
// Six trivial, non-Win32 bodies: five one-instruction field reads and one
// empty function. None of the five touches the Win32 API directly (they
// return opaque HWND/HINSTANCE-shaped `void*`, never call into it), so per
// PLAN.md §4.1 decision 1 they belong here rather than in application_win.cpp
// — <windows.h> stays out of this file.
//
//   ?Get@Application@1@SAPEAV11@XZ                    retail 0x18000a2f0  (static)
//   ?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ retail 0x18009fb90
//   ?GetWindow@Application@1@QEBAPEAUHWND__@@XZ        retail 0x18009fba0
//   ?IsCursorEnabled@Application@1@QEAA_NXZ            retail 0x18000a0c0
//   ?PlatformIsPaused@Application@1@AEAA_NXZ           retail 0x18009fe80  (private)
//   ?PlatformDestruct@Application@1@AEAAXXZ            retail 0x18009fc00  (private)
//
// Every disassembly quoted below is transcribed from
// `<SCRATCH>/app_disasm_main.txt` / `app_disasm_inline.txt` (objdump -d -M
// intel of the shipped Runtime-x64.dll), re-confirmed at this address by this
// lane rather than trusted from the wave brief alone.
// ============================================================================

#include <cstdint>
#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"

using RtApplication::Application;

// ============================================================================
// Application::Application::Get()                       retail 0x18000a2f0
// ?Get@Application@1@SAPEAV11@XZ
// ----------------------------------------------------------------------------
//   18000a2f0: MOV RAX,[RIP+0x1f2381]   ; # 0x1801fc678
//   18000a2f7: RET
//
// `SA` in the mangling: STATIC, no `this`. It returns the same
// `?_singleton@Application@1@0PEAV11@EA` slot at 0x1801fc678 the ctor
// publishes into (Application.h's `_windowHandle` comment; also
// `runtime/src/engine/singleton_ctors.cpp`'s `SlotApplication()`, which is
// file-local there and not reused from here — this accessor reads the same
// exported slot through its own RT_DATA_SLOT call instead of depending on
// that file's internal linkage).
// ============================================================================
Application* RtApp_Get() {
    Application** slot =
        RT_DATA_SLOT(Application**, "?_singleton@Application@1@0PEAV11@EA");
    Application* ours = slot ? *slot : nullptr;
    return ours;
}

// ============================================================================
// Application::Application::GetInstance() const          retail 0x18009fb90
// ?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ
// ----------------------------------------------------------------------------
//   18009fb90: MOV RAX,[RCX+0x8]
//   18009fb94: RET
// `return this->_platformHandle;` (Application.h +0x08).
// ============================================================================
void* RtApp_GetInstance(const Application* self) {
    void* ours = self->_platformHandle;
    return ours;
}

// ============================================================================
// Application::Application::GetWindow() const             retail 0x18009fba0
// ?GetWindow@Application@1@QEBAPEAUHWND__@@XZ
// ----------------------------------------------------------------------------
//   18009fba0: MOV RAX,[RCX]
//   18009fba3: RET
// `return this->_windowHandle;` (Application.h +0x00).
// ============================================================================
void* RtApp_GetWindow(const Application* self) {
    void* ours = self->_windowHandle;
    return ours;
}

// ============================================================================
// Application::Application::IsCursorEnabled()             retail 0x18000a0c0
// ?IsCursorEnabled@Application@1@QEAA_NXZ
// ----------------------------------------------------------------------------
//   18000a0c0: MOVZX EAX,byte [RCX+0x10]
//   18000a0c4: RET
// `return this->_cursorEnabled != 0;` (Application.h +0x10 — this call is
// what proves that byte is read at all; see the header comment this lane
// corrected).
// ============================================================================
// RETURNS THE RAW BYTE, NOT A NORMALISED BOOL. Retail is `movzx eax,byte
// [rcx+0x10]; ret` -- whatever byte is stored reaches the caller in AL
// unchanged. This body first returned `(_cursorEnabled != 0)`, which C++
// normalises to 1, so a stored 2 became 1. Wave 12 lane VERIFY's orphan probe
// measured that as 144 raw-AL divergences over a 216-state sweep.
//
// The reason it survived lane A's own review is worth keeping: the oracle
// below USED TO DECLARE `Fn` as returning `bool` and to hold `theirs` in a
// `bool`, so the C++ conversion normalised BOTH sides before the comparison.
// The instrument was structurally incapable of expressing the defect it was
// pointed at -- it could not have failed. `uint8_t` on both sides is what
// makes this a real comparison.
//
// No live state can hold a value above 1 today (the ctor's word store writes
// 0x00 and `EnableCursor` stores a C++ bool), so this was not a live bug. It
// is fixed anyway: faithful-first is the project rule, and "unreachable
// today" is a property of our own current callers, not of the retail body.
uint8_t RtApp_IsCursorEnabled(Application* self) {
    const uint8_t ours = self->_cursorEnabled;
    return ours;
}

// ============================================================================
// Application::Application::PlatformIsPaused() const      retail 0x18009fe80  (private)
// ?PlatformIsPaused@Application@1@AEAA_NXZ
// ----------------------------------------------------------------------------
//   18009fe80: XOR EAX,EAX
//   18009fe82: CMP byte [RCX+0x11],AL
//   18009fe85: SETE AL
//   18009fe88: RET
//
// Byte-identical in SHAPE to the already-ported `IsPaused`
// (`?IsPaused@Application@1@QEAA_NXZ`, 0x18009f4e0, application.cpp) — both
// read `this[0x11] == 0`. They are two DISTINCT exports (`Is` vs `PlatformIs`,
// different ordinals) and both are registered here; this is not an alias of
// the other, it is its own port reading the same byte.
// ============================================================================
bool RtApp_PlatformIsPaused(const Application* self) {
    const bool ours = (self->_running == 0);
    return ours;
}

// ============================================================================
// Application::Application::PlatformDestruct()            retail 0x18009fc00  (private)
// ?PlatformDestruct@Application@1@AEAAXXZ
// ----------------------------------------------------------------------------
//   18009fc00: RET 0x0
//
// An empty function — this IS the whole body, not a truncated read. It is
// the Win32 platform seam's no-op counterpart to `PlatformConstruct` (lane
// B), which is why it lives here and not in application_win.cpp: it never
// touches <windows.h> because retail's own body never does either. A real
// port of a real (if trivial) function; it belongs on the wave's checklist
// and is registered exactly like the rest.
// ============================================================================
void RtApp_PlatformDestruct(Application* /*self*/) {
}
