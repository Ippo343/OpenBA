// code/Game/Game/inputbindings.cpp
// Clean reimpl of FUN_1400992f0 (Application-x64.exe).
//
// Original sequence:
//   KeyMap* km = ***(KeyMap****)g_pKeyMapObjectExref;
//   Input::KeyMap::ClearAll(km);
//   Input::KeyMap::RegisterInput(km, 0,  2, 0,    "MouseX",       false);
//   ... (50 RegisterInput calls covering mouse axes, hotkeys, camera, toolbar)
//   Input::KeyMap::Load(km);
//
// All 50 bindings are reproduced verbatim from the Ghidra decompile.
//
// In the DLL build the original exe owns this function and our wrapper
// (engine/reimpl/) trampolines to it. In the standalone build there is no
// original; this is the implementation. RunGameMain calls us right after
// Engine::Startup so OptionsDialog can poll hotkey bindings without crashing.

#include "inputbindings.h"
#include "engine_funcs.h"
#include <cstdio>

namespace Game {

namespace {

// One row in the binding table. Mirrors the original RegisterInput call args.
struct Binding {
    int         id;
    int         deviceType;   // 0=gamepad, 1=keyboard, 2=mouse
    int         keyCode;
    const char* name;
    bool        flag;          // true=button-style, false=axis (mouse axes)
};

// 50 default bindings, exact match to FUN_1400992f0.
static const Binding kDefaultBindings[] = {
    // Mouse axes (deviceType=2)
    {0,    2, 0x00, "MouseX",         false},
    {1,    2, 0x01, "MouseY",         false},
    {2,    2, 0x02, "MouseDeltaX",    false},
    {3,    2, 0x03, "MouseDeltaY",    false},
    {4,    2, 0x0C, "MouseWheel",     false},
    {5,    2, 0x04, "MouseLeft",      true },
    {6,    2, 0x05, "MouseRight",     true },
    {7,    2, 0x06, "MouseMiddle",    true },

    // Keyboard hotkeys (deviceType=1)
    {0x08, 1, 0x00, "Cancel",         true},
    {0x09, 1, 0x27, "RotateLeft",     true},
    {0x0A, 1, 0x1B, "RotateRight",    true},
    {0x0B, 1, 0x2D, "Forward",        true},
    {0x0C, 1, 0x29, "Back",           true},
    {0x0D, 1, 0x17, "Left",           true},
    {0x0E, 1, 0x1A, "Right",          true},
    {0x0F, 1, 0x4D, "ZoomIn",         true},
    {0x10, 1, 0x50, "ZoomOut",        true},
    {0x11, 1, 0x4F, "PitchUp",        true},
    {0x12, 1, 0x52, "PitchDown",      true},
    {0x13, 1, 0x28, "StructureLeft",  true},
    {0x14, 1, 0x2A, "StructureRight", true},
    {0x15, 1, 0x1C, "StructureType",  true},
    {0x16, 1, 0x3E, "PathMethod",     true},
    {0x17, 1, 0x47, "TogglePause",    true},

    // Toolbar slots
    {0x18, 1, 0x01, "Toolbar0", true},
    {0x19, 1, 0x02, "Toolbar1", true},
    {0x1A, 1, 0x03, "Toolbar2", true},
    {0x1B, 1, 0x04, "Toolbar3", true},
    {0x1C, 1, 0x05, "Toolbar4", true},
    {0x1D, 1, 0x06, "Toolbar5", true},
    {0x1E, 1, 0x07, "Toolbar6", true},
    {0x1F, 1, 0x08, "Toolbar7", true},
    {0x20, 1, 0x09, "Toolbar8", true},

    // Screenshot
    {0x21, 1, 0x4A, "ScreenshotNoUI", true},
    {0x22, 1, 0x4B, "Screenshot",     true},

    // Tool slots
    {0x23, 1, 0x0E, "Tool0", true},
    {0x24, 1, 0x0F, "Tool1", true},
    {0x25, 1, 0x10, "Tool2", true},
    {0x26, 1, 0x11, "Tool3", true},
    {0x27, 1, 0x12, "Tool4", true},
    {0x28, 1, 0x13, "Tool5", true},
    {0x29, 1, 0x14, "Tool6", true},
    {0x2A, 1, 0x15, "Tool7", true},
    {0x2B, 1, 0x16, "Tool8", true},
    {0x2C, 1, 0x0D, "Tool9", true},

    // Camera presets
    {0x2D, 1, 0x30, "Camera0", true},
    {0x2E, 1, 0x2E, "Camera1", true},
    {0x2F, 1, 0x19, "Camera2", true},
    {0x30, 1, 0x2C, "Camera3", true},
    {0x31, 1, 0x18, "Camera4", true},
};

constexpr size_t kBindingCount = sizeof(kDefaultBindings) / sizeof(kDefaultBindings[0]);
static_assert(kBindingCount == 50, "Banished has exactly 50 default bindings");

} // namespace

void RegisterDefaultInputBindings() {
    // Resolve the actual KeyMap via the same triple-deref pattern the
    // original exe uses. See CLAUDE.md note about External reference
    // indirection levels.
    if (!g_pKeyMapObjectExref || !*g_pKeyMapObjectExref) {
        printf("[InputBindings] FATAL: g_pKeyMapObjectExref not initialized "
               "(slot=%p, *slot=%p)\n",
               (void*)g_pKeyMapObjectExref,
               g_pKeyMapObjectExref ? *g_pKeyMapObjectExref : nullptr);
        return;
    }

    void* level2 = *(void**)*g_pKeyMapObjectExref;
    if (!level2) {
        printf("[InputBindings] FATAL: KeyMap exref level-2 ptr is NULL\n");
        return;
    }

    void* keyMap = *(void**)level2;
    if (!keyMap) {
        printf("[InputBindings] FATAL: KeyMap exref level-3 (object) ptr is NULL\n");
        return;
    }

    if (!fn_KeyMap_ClearAll || !fn_KeyMap_RegisterInput || !fn_KeyMap_Load) {
        printf("[InputBindings] FATAL: missing fn pointers (ClearAll=%p, "
               "RegisterInput=%p, Load=%p)\n",
               (void*)fn_KeyMap_ClearAll, (void*)fn_KeyMap_RegisterInput,
               (void*)fn_KeyMap_Load);
        return;
    }

    printf("[InputBindings] Populating KeyMap %p with %zu default bindings...\n",
           keyMap, kBindingCount);

    fn_KeyMap_ClearAll(keyMap);

    for (const Binding& b : kDefaultBindings) {
        fn_KeyMap_RegisterInput(keyMap, b.id, b.deviceType, b.keyCode,
                                 b.name, b.flag);
    }

    // Load saved overrides from Persistence (HKCU\...\Banished\Input...).
    // No-op on a fresh install.
    fn_KeyMap_Load(keyMap);

    printf("[InputBindings] Done — %zu bindings + Load applied.\n",
           kBindingCount);
}

} // namespace Game
