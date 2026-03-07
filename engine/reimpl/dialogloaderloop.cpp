// engine/reimpl/dialogloaderloop.cpp
// Reimplementation of Engine::DialogLoaderLoopT methods from Runtime-x64.dll.
//
// DialogLoaderLoopT extends LoaderLoop with a UI dialog overlay during loading.
// It's abstract — concrete subclasses in the exe implement CreateDialog() (vtable
// slot 12) to provide the specific dialog widget (splash screen, loading bar, etc.).
//
// Methods reimplemented:
//   - Startup   (0x1800DB000) — create dialog, show it, start background loading
//   - Shutdown  (0x1800DAFC0) — close dialog, wait for loader completion
//   - Update    (0x1800DAF20) — UI::Update, sleep, check completion, accumulate time
//   - Draw      (0x1800DAE20) — clear screen, set time uniforms, UI::Draw
//
// Methods reimplemented (with resolved engine function pointers):
//   - Load      (0x1800DAD50) — uses ExternalFactory::FindOrLoad, String lifecycle

#include "DialogLoaderLoopT.h"
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
namespace DialogLoaderLoopAddr {
    constexpr uintptr_t Startup   = 0x1800DB000;
    constexpr uintptr_t Shutdown  = 0x1800DAFC0;
    constexpr uintptr_t Update    = 0x1800DAF20;
    constexpr uintptr_t Draw      = 0x1800DAE20;
    constexpr uintptr_t Load      = 0x1800DAD50;
}

// ============================================================
// Resolved globals
// ============================================================

// Uses g_pMainLoopSingleton from engine_funcs.h

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookStartup;
static HookInfo g_hookShutdown;
static HookInfo g_hookUpdate;
static HookInfo g_hookDraw;
static HookInfo g_hookLoad;

// ============================================================
// Helper: get UserInterface pointer from DialogLoaderLoopT
// ============================================================
static inline void* GetUI(DialogLoaderLoopT* self) {
    return reinterpret_cast<void*>(&self->m_userInterface[0]);
}

// ============================================================
// DialogLoaderLoopT::Startup (0x1800DB000)
// Creates the loading dialog via the pure virtual CreateDialog(),
// shows it, adds it to the UI pending-open list, then starts
// background resource loading via Loader::Load.
// ============================================================

typedef void (*DialogLoaderLoopT_Startup_t)(DialogLoaderLoopT* self);

static void Reimpl_Startup(DialogLoaderLoopT* self) {
    g_lastHook = "DialogLoaderLoopT::Startup";
#ifdef _DEBUG
    printf("[DialogLoaderLoopT::Startup] self=%p nextLoop=%p\n",
           (void*)self, (void*)self->m_nextLoop);
#endif

    // 1. Call pure virtual CreateDialog() — vtable slot 12 (offset +0x60)
    //    Signature: void* CreateDialog(this, &outPtr)
    //    Returns InternalPtr<DialogInstance> by pointer in RDX
    void** vtable = *reinterpret_cast<void***>(self);
    typedef void* (*CreateDialog_t)(DialogLoaderLoopT* self, void* outPtr);
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
    //    Pending-open array is at UserInterface+0x28 = self+0x88
    //    Array struct: {void* data, int count, int capacity} at +0x88/+0x90/+0x94
    uint8_t* uiBase = &self->m_userInterface[0];
    int32_t* pCount = reinterpret_cast<int32_t*>(uiBase + 0x30); // count at UI+0x30
    fn_DynArrayPush(uiBase + 0x28, *pCount + 1, &dialogResult);

    // 5. Start background loading
    fn_Loader_Load(g_pLoader, self->m_nextLoop);

#ifdef _DEBUG
    printf("[DialogLoaderLoopT::Startup] dialog=%p, loading started\n", dialogResult);
#endif
}

// ============================================================
// DialogLoaderLoopT::Shutdown (0x1800DAFC0)
// Closes the loading dialog, then waits for the background
// loader thread to complete.
// ============================================================

typedef void (*DialogLoaderLoopT_Shutdown_t)(DialogLoaderLoopT* self);

static void Reimpl_Shutdown(DialogLoaderLoopT* self) {
    g_lastHook = "DialogLoaderLoopT::Shutdown";
#ifdef _DEBUG
    printf("[DialogLoaderLoopT::Shutdown] self=%p dialog=%p\n",
           (void*)self, self->m_dialogInstance);
#endif

    // Close the dialog from the embedded UserInterface
    fn_UI_CloseDialog(GetUI(self), &self->m_dialogInstance);

    // Wait for background loader thread
    WaitForSingleObject(*g_pLoaderCompletionEvent, INFINITE);

#ifdef _DEBUG
    printf("[DialogLoaderLoopT::Shutdown] loader completed\n");
#endif
}

// ============================================================
// DialogLoaderLoopT::Update (0x1800DAF20)
// Like LoaderLoop::Update but adds UI::Update before the sleep.
// The deltaTime is converted through float precision (tick->sec->tick)
// before passing to UI::Update, matching the original's behavior.
// ============================================================

typedef bool (*DialogLoaderLoopT_Update_t)(DialogLoaderLoopT* self, Time* dt);

static bool Reimpl_Update(DialogLoaderLoopT* self, Time* dt) {
    g_lastHook = "DialogLoaderLoopT::Update";
#ifdef _DEBUG
    static int updateCount = 0;
    if (updateCount < 5) {
        printf("[DialogLoaderLoopT::Update] self=%p dt=%lld (call #%d) "
               "finished=%d loaderFlags=0x%x\n",
               (void*)self, (long long)dt->ticks, updateCount,
               (int)self->m_finished, *g_pLoaderFlags);
        updateCount++;
    }
#endif
    // Convert tick-based time through float precision for UI
    // Original: (int64_t)((float)((double)dt->ticks * secondsPerTick) / secondsPerTick)
    double dtSeconds = (double)dt->ticks * (*g_Timer_secondsPerTick);
    int64_t uiDelta = (int64_t)((float)dtSeconds / (*g_Timer_secondsPerTick));

    // Update the UI (processes events, layouts, animations)
    fn_UI_Update(GetUI(self), &uiDelta);

    // Sleep ~14ms per frame (loading screen frame rate cap)
    fn_App_SleepThread(*g_pAppSingleton, 14);

    // Check transition conditions (same as LoaderLoop)
    if (self->m_finished != 0 &&
        (*g_pLoaderFlags & 1) == 0 &&
        self->m_delayTicks <= self->m_elapsedTicks)
    {
#ifdef _DEBUG
        printf("[DialogLoaderLoopT::Update] Loading complete, transitioning to nextLoop=%p\n",
               (void*)self->m_nextLoop);
#endif
        MainLoop* mainLoop = reinterpret_cast<MainLoop*>(*g_pMainLoopSingleton);
        mainLoop->m_nextLoop = self->m_nextLoop;
    }

    // Accumulate elapsed time
    self->m_elapsedTicks += dt->ticks;

    return true;
}

// ============================================================
// DialogLoaderLoopT::Draw (0x1800DAE20)
// Clears the screen, sets up time uniforms in the video context,
// then draws the UI overlay.
// ============================================================

typedef void (*DialogLoaderLoopT_Draw_t)(DialogLoaderLoopT* self, float alpha);

static void Reimpl_Draw(DialogLoaderLoopT* self, float /*alpha*/) {
    g_lastHook = "DialogLoaderLoopT::Draw";
#ifdef _DEBUG
    static int drawCount = 0;
    if (drawCount < 3) {
        printf("[DialogLoaderLoopT::Draw] self=%p (call #%d)\n", (void*)self, drawCount);
        drawCount++;
    }
#endif
    MainLoop* mainLoop = reinterpret_cast<MainLoop*>(*g_pMainLoopSingleton);
    uint8_t* ctx = reinterpret_cast<uint8_t*>(mainLoop->m_context);

    // Clear screen: device->Clear(flags=7, color=0, depth=1.0f, stencil=0)
    void* device = *reinterpret_cast<void**>(ctx + 0x2160);
    void** deviceVtable = *reinterpret_cast<void***>(device);
    typedef void (*DeviceClear_t)(void*, uint32_t, uint32_t, float, int);
    ((DeviceClear_t)deviceVtable[4])(device, 7, 0, 1.0f, 0);

    // Compute elapsed time in seconds (matches original: double → float)
    float totalSeconds = (float)((double)self->m_elapsedTicks * (*g_Timer_secondsPerTick));

    // 1. Set m_gameTime FIRST (context+0x3C8, float4) — matches original order
    *reinterpret_cast<float*>(ctx + 0x3C8) = totalSeconds;
    *reinterpret_cast<uint32_t*>(ctx + 0x3CC) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3D0) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3D4) = 0;

    // 2. Intermediate dirty flag: gameTime updated
    *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x800;

    // 3. Recompute time as double for prevTime/viewTime (matches original's dVar2 path)
    double dVar2 = (double)self->m_elapsedTicks * (*g_Timer_secondsPerTick);

    // 4. Set m_prevTime (context+0x3B8, float4)
    *reinterpret_cast<float*>(ctx + 0x3B8) = (float)dVar2;
    *reinterpret_cast<uint32_t*>(ctx + 0x3BC) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3C0) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3C4) = 0;

    // 5. Set m_viewTime (context+0x288, float4)
    *reinterpret_cast<float*>(ctx + 0x288) = (float)dVar2;
    *reinterpret_cast<uint32_t*>(ctx + 0x28C) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x290) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x294) = 0;

    // 6. Final dirty flags: time + view updated
    *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x10800;

    // Draw the UI overlay
    fn_UI_Draw(GetUI(self), mainLoop->m_context);
}

// ============================================================
// DialogLoaderLoopT::Load (0x1800DAD50) [hookable, prologue=15]
// Loads the dialog External resource by filename.
// ============================================================

static void Reimpl_Load(uint8_t* self) {
    g_lastHook = "DialogLoaderLoopT::Load";
    // local Filename (same as SystemString — 8-byte COW pointer)
    SystemString tempStr;
    tempStr.m_data = nullptr;

    // Copy m_dialogFilename (COW string at +0x48) via copy ctor
    SystemString* srcStr = reinterpret_cast<SystemString*>(self + 0x48);
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

    // Store result in m_dialogExternal (+0x50)
    *reinterpret_cast<void**>(self + 0x50) = result;

    // Release temp string via proper destructor
    fn_String_Dtor(&tempStr);
}

// ============================================================
// Install / Remove
// ============================================================

static void ResolveGlobals() {
    if (!g_pMainLoopSingleton) {
        g_pMainLoopSingleton = reinterpret_cast<void**>(
            ResolveAddr(Module::Runtime, MainLoopAddr::Singleton));
    }
}

void InstallDialogLoaderLoopStartup() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoaderLoopAddr::Startup);
    // Prologue: PUSH(2)+SUB(4)+MOV(3)+LEA(5)=14 bytes
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 14);
#ifdef _DEBUG
    printf("[DialogLoaderLoopT] Startup hooked at %p (prologue=14)\n", (void*)target);
#endif
}
void RemoveDialogLoaderLoopStartup() { RemoveHook(&g_hookStartup); }

void InstallDialogLoaderLoopShutdown() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoaderLoopAddr::Shutdown);
    // Prologue: SUB(4)+MOV(4)+LEA(5)=13 bytes
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[DialogLoaderLoopT] Shutdown hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveDialogLoaderLoopShutdown() { RemoveHook(&g_hookShutdown); }

void InstallDialogLoaderLoopUpdate() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoaderLoopAddr::Update);
    // Prologue needs 13 bytes: MOV(5)+PUSH(1)+SUB(4)+XORPS(3) = 13.
    // Default 12 would split the 3-byte XORPS at offset 10.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[DialogLoaderLoopT] Update hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveDialogLoaderLoopUpdate() { RemoveHook(&g_hookUpdate); }

void InstallDialogLoaderLoopDraw() {
    ResolveGlobals();
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoaderLoopAddr::Draw);
    // Prologue: MOV(5)+PUSH(1)+SUB(4)+MOV_RIP(7)=17 bytes
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 17);
#ifdef _DEBUG
    printf("[DialogLoaderLoopT] Draw hooked at %p (prologue=17)\n", (void*)target);
#endif
}
void RemoveDialogLoaderLoopDraw() { RemoveHook(&g_hookDraw); }

// --- Load (0x1800DAD50, prologue=15) ---

void InstallDialogLoaderLoopLoad() {
    uintptr_t target = ResolveAddr(Module::Runtime, DialogLoaderLoopAddr::Load);
    InstallHook(target, (void*)Reimpl_Load, &g_hookLoad, 15);
#ifdef _DEBUG
    printf("[DialogLoaderLoopT] Load hooked at %p (prologue=15)\n", (void*)target);
#endif
}
void RemoveDialogLoaderLoopLoad() { RemoveHook(&g_hookLoad); }
