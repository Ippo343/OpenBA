// engine/reimpl/settextparams.cpp
// Null-guard fix for FUN_14005e700 — "SetTextElementParams"
//
// This shared utility function loops over a dialog controller's TextElement
// array and calls TextElement::SetParameter on each one. The original code
// does NOT null-check after CheckPtr, so if any InternalBasePtr resolves to
// NULL, it crashes in TextElement::SetParameter reading [NULL + 0xd0].
//
// This is a pre-existing game bug that affects multiple dialog types
// (BridgeTool cost display, AutoSaveDialog, etc.) through the shared
// operator_impl_fc60 → FUN_14005e700 path.
//
// Original: 0x14005e700 (47 instructions)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// Dispatch table function pointers (resolved once)
// String::Format at [0x140151560]
typedef void* (*fn_StringFormat_t)(void* retbuf, const char* fmt, ...);
static fn_StringFormat_t s_fn_StringFormat = nullptr;

// TextElement::SetParameter at [0x140151558]
typedef void (*fn_SetParameter_t)(void* textElement, int paramIndex, void* str);
static fn_SetParameter_t s_fn_SetParameter = nullptr;

// String::~String at [0x140152af0] — already available as fn_StringDtor but
// we resolve from dispatch table to match original code path
typedef void (*fn_StringDtor_dispatch_t)(void* self);
static fn_StringDtor_dispatch_t s_fn_StringDtor = nullptr;

static bool s_resolved = false;

static void ResolveSetTextParamsFuncs() {
    if (s_resolved) return;
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151560));
        if (d) s_fn_StringFormat = reinterpret_cast<fn_StringFormat_t>(*d);
    }
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151558));
        if (d) s_fn_SetParameter = reinterpret_cast<fn_SetParameter_t>(*d);
    }
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152af0));
        if (d) s_fn_StringDtor = reinterpret_cast<fn_StringDtor_dispatch_t>(*d);
    }
    s_resolved = true;
}

// Format string constant: "%i" at 0x14015b018 in the exe
static const char* s_fmtString = nullptr;

// ============================================================
// Reimplementation of FUN_14005e700 with null-guard
// ============================================================
// Original loops over controller's TextElement IBPs and calls SetParameter
// with a formatted integer string. We add a null check after CheckPtr.
//
// Controller layout:
//   +0x38: int multiplier (written by this function from param_2)
//   +0x40: void** array of InternalBasePtr<TextElement> (8 bytes each)
//   +0x50: int*  array of base values (4 bytes each)
//   +0x58: int   count of text elements
//
static HookInfo g_hookSetTextParams;

static void Reimpl_SetTextParams(void* controller, int multiplier,
                                  uint64_t /*unused1*/, uint64_t /*unused2*/) {
    g_lastHook = "SetTextElementParams";
    ResolveSetTextParamsFuncs();

    if (!s_fmtString) {
        s_fmtString = reinterpret_cast<const char*>(ResolveAddr(Module::Exe, 0x14015b018));
    }

    uintptr_t ctrl = (uintptr_t)controller;

    // Store multiplier
    *(int*)(ctrl + 0x38) = multiplier;

    int count = *(int*)(ctrl + 0x58);
    if (count <= 0) return;

    uintptr_t ibpArray = *(uintptr_t*)(ctrl + 0x40);  // array of IBPs (8 bytes each)
    uintptr_t valArray = *(uintptr_t*)(ctrl + 0x50);   // array of int values (4 bytes each)

    for (int i = 0; i < count; i++) {
        // Compute display value: baseValue * multiplier
        int baseVal = *(int*)(valArray + (uintptr_t)i * 4);
        int displayVal = baseVal * multiplier;

        // Format string: String::Format(retbuf, "%i", displayVal)
        uint64_t tempStr[2] = {0};  // System::String is 8 bytes
        void* formatted = s_fn_StringFormat(tempStr, s_fmtString, displayVal);

        // CheckPtr on the IBP
        void* ibp = (void*)(ibpArray + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(ibp);
        }

        // *** NULL GUARD — the original game bug ***
        void* textElement = *(void**)ibp;
        if (textElement) {
            s_fn_SetParameter(textElement, 0, formatted);
        }
#ifdef _DEBUG
        else {
            printf("[SetTextParams] WARN: NULL TextElement at index %d/%d (controller=%p)\n",
                   i, count, controller);
        }
#endif

        // Destroy temp string
        if (s_fn_StringDtor) {
            s_fn_StringDtor(tempStr);
        }
    }
}

void InstallSetTextParams() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005e700);
    // Prologue: PUSH RDI(1) + PUSH R14(2) + PUSH R15(2) + SUB RSP,0x30(4) = 9 bytes
    // Need 12 for JMP overwrite; 12 bytes splits into MOV [RSP+0x20],-2 but
    // we never call the trampoline so that's fine.
    InstallHook(target, (void*)Reimpl_SetTextParams, &g_hookSetTextParams, 12);
#ifdef _DEBUG
    printf("[SetTextParams] Null-guard hook at %p\n", (void*)target);
#endif
}
void RemoveSetTextParams() { RemoveHook(&g_hookSetTextParams); }
