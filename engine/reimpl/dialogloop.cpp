// engine/reimpl/dialogloop.cpp
// Reimplementation of Engine::DialogLoopT methods from Runtime-x64.dll.
//
// DialogLoopT extends UpdateLoop with a UI dialog overlay — no background loading.
// Simpler sibling of DialogLoaderLoopT: shows a dialog, processes UI events,
// draws the UI overlay, and shuts down when done. No Loader, no sleep, no transition.
//
// It's abstract — concrete subclasses in the exe implement CreateDialog() (vtable
// slot 12) to provide the specific dialog widget (main menu, options, etc.).
//
// Methods reimplemented:
//   - Startup   (0x1800DB2F0) — create dialog via pure virtual, show it, add to UI pending
//   - Shutdown  (0x1800DB2C0) — close the dialog
//   - Update    (0x1800DB270) — UI::Update with float-precision time
//   - Draw      (0x1800DB210) — clear screen to gray, UI::Draw
//
// Methods reimplemented (with resolved engine function pointers):
//   - Load      (0x1800DB140) — uses ExternalFactory::FindOrLoad, String lifecycle

#include "DialogLoopT.h"
#include "MainLoop.h"
#include "SystemString.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <windows.h>
#include <cstdio>

// ============================================================
// Address constants
// ============================================================
namespace DialogLoopAddr {
    constexpr uintptr_t Startup   = 0x1800DB2F0;
    constexpr uintptr_t Shutdown  = 0x1800DB2C0;
    constexpr uintptr_t Update    = 0x1800DB270;
    constexpr uintptr_t Draw      = 0x1800DB210;
    constexpr uintptr_t Load      = 0x1800DB140;
}

// ============================================================
// Resolved globals
// ============================================================

static MainLoop** g_pMainLoopSingleton = nullptr;

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookStartup;
static HookInfo g_hookShutdown;
static HookInfo g_hookUpdate;
static HookInfo g_hookDraw;
static HookInfo g_hookLoad;

// ============================================================
// Helper: get UserInterface pointer from DialogLoopT
// ============================================================
static inline void* GetUI(DialogLoopT* self) {
    return reinterpret_cast<void*>(&self->m_userInterface[0]);
}

// ============================================================
// DialogLoopT::Startup (0x1800DB2F0)
// Creates the dialog via pure virtual CreateDialog() (vtable slot 12),
// shows it, and adds it to the UI pending-open list.
// Unlike DialogLoaderLoopT, does NOT start background loading.
// ============================================================

typedef void (*DialogLoopT_Startup_t)(DialogLoopT* self);

static void Reimpl_Startup(DialogLoopT* self) {
#ifdef _DEBUG
    printf("[DialogLoopT::Startup] self=%p\n", (void*)self);
#endif

    // 1. Call pure virtual CreateDialog() — vtable slot 12 (offset +0x60)
    //    Signature: void* CreateDialog(this, &outPtr)
    //    Returns InternalPtr<DialogInstance> by pointer in RDX
    void** vtable = *reinterpret_cast<void***>(self);
    typedef void* (*CreateDialog_t)(DialogLoopT* self, void* outPtr);
    void* dialogResult = nullptr;
    ((CreateDialog_t)vtable[12])(self, &dialogResult);

    // 2. Store the dialog instance
    self->m_dialogInstance = dialogResult;

    // 3. Call dialog->Show() — vtable slot 4 (offset +0x20)
    if (dialogResult) {
        void** dialogVtable = *reinterpret_cast<void***>(dialogResult);
        typedef void (*DialogShow_t)(void* self);
        ((DialogShow_t)dialogVtable[4])(dialogResult);
    }

    // 4. Append dialog to UserInterface's pending-open list
    //    Pending-open array is at UserInterface+0x28 = self+0x68
    //    Array struct: {void* data, int count, int capacity} at +0x68/+0x70/+0x74
    uint8_t* uiBase = &self->m_userInterface[0];
    int32_t* pCount = reinterpret_cast<int32_t*>(uiBase + 0x30); // count at UI+0x30
    fn_DynArrayPush(uiBase + 0x28, *pCount + 1, &dialogResult);

#ifdef _DEBUG
    printf("[DialogLoopT::Startup] dialog=%p\n", dialogResult);
#endif
}

// ============================================================
// DialogLoopT::Shutdown (0x1800DB2C0)
// Closes the dialog from the embedded UserInterface.
// No loader wait — DialogLoopT doesn't do background loading.
// ============================================================

typedef void (*DialogLoopT_Shutdown_t)(DialogLoopT* self);

static void Reimpl_Shutdown(DialogLoopT* self) {
#ifdef _DEBUG
    printf("[DialogLoopT::Shutdown] self=%p dialog=%p\n",
           (void*)self, self->m_dialogInstance);
#endif

    // Close the dialog from the embedded UserInterface
    fn_UI_CloseDialog(GetUI(self), &self->m_dialogInstance);

#ifdef _DEBUG
    printf("[DialogLoopT::Shutdown] dialog closed\n");
#endif
}

// ============================================================
// DialogLoopT::Update (0x1800DB270)
// Updates the UI with float-precision time conversion.
// Much simpler than DialogLoaderLoopT: no sleep, no loader check,
// no transition logic. Just UI::Update and return true.
// ============================================================

typedef bool (*DialogLoopT_Update_t)(DialogLoopT* self, Time* dt);

static bool Reimpl_Update(DialogLoopT* self, Time* dt) {
#ifdef _DEBUG
    static int updateCount = 0;
    if (updateCount < 5) {
        printf("[DialogLoopT::Update] self=%p dt=%lld (call #%d)\n",
               (void*)self, (long long)dt->ticks, updateCount);
        updateCount++;
    }
#endif

    // Convert tick-based time through float precision for UI
    // Original: (int64_t)((float)((double)dt->ticks * secondsPerTick) / secondsPerTick)
    double dtSeconds = (double)dt->ticks * (*g_Timer_secondsPerTick);
    int64_t uiDelta = (int64_t)((float)dtSeconds / (*g_Timer_secondsPerTick));

    // Update the UI (processes events, layouts, animations)
    fn_UI_Update(GetUI(self), &uiDelta);

    return true;
}

// ============================================================
// DialogLoopT::Draw (0x1800DB210)
// Clears the screen to gray (0xFF808080), then draws the UI overlay.
// Unlike DialogLoaderLoopT, does NOT set time uniforms (m_viewTime etc.)
// because there's no elapsed time tracking.
// ============================================================

typedef void (*DialogLoopT_Draw_t)(DialogLoopT* self, float alpha);

static void Reimpl_Draw(DialogLoopT* self, float /*alpha*/) {
#ifdef _DEBUG
    static int drawCount = 0;
    if (drawCount < 3) {
        printf("[DialogLoopT::Draw] self=%p (call #%d)\n", (void*)self, drawCount);
        drawCount++;
    }
#endif

    MainLoop* mainLoop = *g_pMainLoopSingleton;
    uint8_t* ctx = reinterpret_cast<uint8_t*>(mainLoop->m_context);

    // Clear screen: device->Clear(flags=7, color=0xFF808080, depth=1.0f, stencil=0)
    // Note: gray background (0xFF808080), NOT black like DialogLoaderLoopT
    void* device = *reinterpret_cast<void**>(ctx + 0x2160);
    void** deviceVtable = *reinterpret_cast<void***>(device);
    typedef void (*DeviceClear_t)(void*, uint32_t, uint32_t, float, int);
    ((DeviceClear_t)deviceVtable[4])(device, 7, 0xFF808080, 1.0f, 0);

    // Draw the UI overlay
    fn_UI_Draw(GetUI(self), mainLoop->m_context);
}

// ============================================================
// DialogLoopT::Load (0x1800DB140) [hookable, prologue=15]
// Loads the dialog External resource by filename.
// Same logic as DialogLoaderLoopT::Load but offsets differ.
// ============================================================

static void Reimpl_Load(uint8_t* self) {
    // local Filename (same as SystemString — 8-byte COW pointer)
    SystemString tempStr;
    tempStr.m_data = nullptr;

    // Copy m_dialogFilename (COW string at +0x28) via copy ctor
    SystemString* srcStr = reinterpret_cast<SystemString*>(self + 0x28);
    fn_String_Ctor_Copy(&tempStr, srcStr);

    // AdjustDelimiters on the copy
    fn_Filename_AdjustDelimiters(&tempStr);

    // FindOrLoad from ExternalFactory
    void* factory = *g_pExternalFactorySingleton;
    void* external = fn_ExternalFactory_FindOrLoad(factory, &tempStr);

    void* result = nullptr;
    if (external) {
        // Type check: IsDerivedFrom(external, typeId)
        bool isDerived = fn_IsDerivedFrom(external, *g_pExternalTypeId);
        if (isDerived) {
            result = external;
        }
    }

    // Store result in m_dialogExternal (+0x30)
    *reinterpret_cast<void**>(self + 0x30) = result;

    // Release temp string via proper destructor
    fn_String_Dtor(&tempStr);
}

// ============================================================
// Install / Remove
// ============================================================

static void ResolveGlobals() {
    if (!g_pMainLoopSingleton) {
        g_pMainLoopSingleton = reinterpret_cast<MainLoop**>(
            ResolveAddr(Module::Runtime, MainLoopAddr::Singleton));
    }
}

void InstallDialogLoopStartup() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoopAddr::Startup);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RAX,[RCX](3) + LEA RDX,[RSP+0x30](5) = 14 bytes
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 14);
#ifdef _DEBUG
    printf("[DialogLoopT] Startup hooked at %p (prologue=14)\n", (void*)target);
#endif
}
void RemoveDialogLoopStartup() { RemoveHook(&g_hookStartup); }

void InstallDialogLoopShutdown() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoopAddr::Shutdown);
    // Prologue: SUB RSP,0x28(4) + MOV RAX,[RCX+0x38](4) + LEA RDX,[RSP+0x30](5) = 13 bytes
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[DialogLoopT] Shutdown hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveDialogLoopShutdown() { RemoveHook(&g_hookShutdown); }

void InstallDialogLoopUpdate() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoopAddr::Update);
    // Prologue: SUB RSP,0x28(4) + XORPS XMM0(3) + ADD RCX,0x40(4) + CVTSI2SD(5) = 16 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[DialogLoopT] Update hooked at %p (prologue=16)\n", (void*)target);
#endif
}
void RemoveDialogLoopUpdate() { RemoveHook(&g_hookUpdate); }

void InstallDialogLoopDraw() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoopAddr::Draw);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x30(4) + MOV RAX,[RIP+...](7) = 17 bytes
    // NOTE: byte 10 is RIP-relative MOV RAX,[RIP+offset] — trampoline is broken
    // but we never call it (reimpl handles everything).
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 17);
#ifdef _DEBUG
    printf("[DialogLoopT] Draw hooked at %p (prologue=17)\n", (void*)target);
#endif
}
void RemoveDialogLoopDraw() { RemoveHook(&g_hookDraw); }

// --- Load (0x1800DB140, prologue=15) ---

void InstallDialogLoopLoad() {
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoopAddr::Load);
    InstallHook(target, (void*)Reimpl_Load, &g_hookLoad, 15);
#ifdef _DEBUG
    printf("[DialogLoopT] Load hooked at %p (prologue=15)\n", (void*)target);
#endif
}
void RemoveDialogLoopLoad() { RemoveHook(&g_hookLoad); }
