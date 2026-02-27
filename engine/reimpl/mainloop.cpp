// engine/reimpl/mainloop.cpp
// Reimplementation of Engine::MainLoop core methods from Runtime-x64.dll.
//
// Decompiled from Ghidra, verified against binary layout in engine/types/MainLoop.h.
// Each function can be hooked independently via the registry in hooks.cpp.
//
// Methods reimplemented:
//   - MainLoop::Frame      (0x1800dd100) — fixed-timestep frame (update + draw)
//   - MainLoop::Update     (0x1800dd070) — loop transition handler
//   - MainLoop::Draw       (0x1800dcd40) — render one frame
//   - MainLoop::SetNextLoop(0x1800dd7a0) — queue loop transition
//   - MainLoop::Run        (0x1800dd3c0) — outer main loop

#include "MainLoop.h"
#include "engine_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include <windows.h>
#include <cstdio>

// ============================================================
// QPC / timing helpers
// ============================================================

// Convert raw QPC value to engine ticks.
static inline int64_t QPCToTicks(int64_t qpc) {
    return ((qpc - *g_Timer_systemInitialTick) * *g_Timer_ticksPerSecond)
           / *g_Timer_systemTicksPerSecond;
}

// Get sub-tick remainder of QPC conversion (for Startup/Present calls).
static inline int64_t QPCRemainder(int64_t qpc) {
    return ((qpc - *g_Timer_systemInitialTick) * *g_Timer_ticksPerSecond)
           % *g_Timer_systemTicksPerSecond;
}

static inline int64_t GetQPC() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

// ============================================================
// Engine sub-object accessors (via EngineInstance at m_engine)
// ============================================================

// EngineInstance field offsets (deduced from MainLoop::Frame / MainLoop::Run):
//   +0x690 = Application::Application*
//   +0x6d0 = Audio::Device*
//   +0x6e0 = Input::Device*

static inline void* GetInputDevice(MainLoop* self) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self->m_engine) + 0x6e0);
}

static inline void* GetAudioDevice(MainLoop* self) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self->m_engine) + 0x6d0);
}

static inline void* GetApplication(MainLoop* self) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self->m_engine) + 0x690);
}

// ============================================================
// UpdateLoop virtual call helpers
// ============================================================
// Vtable layout from engine/types/UpdateLoop.h.
// All calls use Win64 ABI: this in RCX, args in RDX/XMM1/R8/XMM2.

static inline void VCall_Destroy(UpdateLoop* loop, int flags) {
    typedef void (*fn_t)(UpdateLoop*, int);
    ((fn_t)loop->vtable->Destroy)(loop, flags);
}

static inline void VCall_Startup(UpdateLoop* loop, int64_t remainderTicks) {
    typedef void (*fn_t)(UpdateLoop*, int64_t);
    ((fn_t)loop->vtable->Startup)(loop, remainderTicks);
}

static inline void VCall_Shutdown(UpdateLoop* loop) {
    typedef void (*fn_t)(UpdateLoop*);
    ((fn_t)loop->vtable->Shutdown)(loop);
}

static inline bool VCall_Update(UpdateLoop* loop, Time* dt) {
    typedef bool (*fn_t)(UpdateLoop*, Time*);
    return ((fn_t)loop->vtable->Update)(loop, dt);
}

static inline void VCall_Draw(UpdateLoop* loop, float alpha) {
    typedef void (*fn_t)(UpdateLoop*, float);
    ((fn_t)loop->vtable->Draw)(loop, alpha);
}

static inline void VCall_DrawFailed(UpdateLoop* loop) {
    typedef void (*fn_t)(UpdateLoop*);
    ((fn_t)loop->vtable->DrawFailed)(loop);
}

// ============================================================
// Function signatures (Win64 ABI with explicit this)
// ============================================================

typedef void (*MainLoop_Run_t)(MainLoop* self, UpdateLoop* loop);
typedef bool (*MainLoop_Frame_t)(MainLoop* self, bool drawOnly);
typedef bool (*MainLoop_Update_t)(MainLoop* self, Time* dt);
typedef void (*MainLoop_Draw_t)(MainLoop* self, float alpha);
typedef void (*MainLoop_SetNextLoop_t)(MainLoop* self, UpdateLoop* loop);

// ============================================================
// Resolved runtime addresses (for chaining calls between methods)
// ============================================================
// These point to the start of the original functions in Runtime-x64.dll.
// If a function is hooked, calling through this address routes to the reimpl.
// If not hooked, it runs the original. This lets us enable/disable independently.

static MainLoop_Update_t g_addr_Update = nullptr;
static MainLoop_Draw_t   g_addr_Draw   = nullptr;

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookFrame;
static HookInfo g_hookUpdate;
static HookInfo g_hookDraw;
static HookInfo g_hookSetNextLoop;
static HookInfo g_hookRun;


// ============================================================
// MainLoop::SetNextLoop (0x1800dd7a0)
// Trivial setter — stores the next loop pointer for transition.
// ============================================================

static void Reimpl_SetNextLoop(MainLoop* self, UpdateLoop* loop) {
    self->m_nextLoop = loop;
}


// ============================================================
// MainLoop::Update (0x1800dd070)
// Ticks the current loop. If a nextLoop is pending, performs the
// transition: shutdown old → destroy old → startup new → tick new.
// Returns false to signal quit (loop ended with no successor).
// ============================================================

static bool Reimpl_Update(MainLoop* self, Time* dt) {
    UpdateLoop* current = self->m_currentLoop;

    // Tick the current loop's Update(dt) — returns true if it wants to keep running
    bool keepRunning = VCall_Update(current, dt);

    if (!keepRunning && !self->m_nextLoop) {
        // Loop is done and no successor queued → quit
        return false;
    }

    if (keepRunning && !self->m_nextLoop) {
        // Loop still running, no transition pending → continue
        return true;
    }

    // --- Loop transition ---

    // 1. Shutdown the current loop
    VCall_Shutdown(current);

    // 2. Destroy the current loop (virtual destructor, flag=1 means free memory)
    if (current) {
        VCall_Destroy(current, 1);
    }

    // 3. Switch: move nextLoop → currentLoop, clear nextLoop
    UpdateLoop* next = self->m_nextLoop;
    self->m_nextLoop = nullptr;
    self->m_currentLoop = next;

    // 4. Initialize the new loop (remainder = 0, mid-frame transition)
    VCall_Startup(next, 0);

    // 5. Run the new loop's first tick with the current dt
    VCall_Update(next, dt);

    // 6. Mark video resources dirty — forces ClearResources on next draw
    self->m_videoContextDirty = 1;

    return true;
}


// ============================================================
// MainLoop::Draw (0x1800dcd40)
// Renders one frame with the given interpolation alpha.
// Handles device-lost recovery and viewport updates.
// ============================================================

static void Reimpl_Draw(MainLoop* self, float alpha) {
    uint8_t* ctx = reinterpret_cast<uint8_t*>(self->m_context);

    // Get the video device object: context+0x2160 → device ptr → vtable
    void* device = *reinterpret_cast<void**>(ctx + 0x2160);
    void** deviceVtable = *reinterpret_cast<void***>(device);

    // BeginFrame check: device->vtable[1]() → returns true if device is ready
    typedef bool (*DeviceBeginFrame_t)(void*);
    bool deviceReady = ((DeviceBeginFrame_t)deviceVtable[1])(device);

    if (!deviceReady) {
        // Device lost (minimized, display mode change, etc.)
        VCall_DrawFailed(self->m_currentLoop);
        self->m_videoContextDirty = 0;
        fn_VideoCtx_Reset(self->m_context);
        return;
    }

    // If resources were invalidated (loop transition), clear and reload
    if (self->m_videoContextDirty) {
        self->m_videoContextDirty = 0;
        fn_VideoCtx_ClearResources(self->m_context);
    }

    // Check if viewport needs updating to match swap chain dimensions
    // Swap chain info: *(context+0x2138) → width at +0x10, height at +0x14
    uint8_t* swapChainInfo = *reinterpret_cast<uint8_t**>(ctx + 0x2138);
    int32_t scWidth  = *reinterpret_cast<int32_t*>(swapChainInfo + 0x10);
    int32_t scHeight = *reinterpret_cast<int32_t*>(swapChainInfo + 0x14);

    // Current viewport: context+0x1d0 (x,y,w,h as int32)
    int32_t vpX = *reinterpret_cast<int32_t*>(ctx + 0x1d0);
    int32_t vpY = *reinterpret_cast<int32_t*>(ctx + 0x1d4);
    int32_t vpW = *reinterpret_cast<int32_t*>(ctx + 0x1d8);
    int32_t vpH = *reinterpret_cast<int32_t*>(ctx + 0x1dc);

    if (vpX != 0 || vpY != 0 || vpW != scWidth || vpH != scHeight) {
        *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x200;  // dirty: viewport changed
        *reinterpret_cast<int64_t*>(ctx + 0x1d0) = 0;        // x=0, y=0 (combined write)
        *reinterpret_cast<int32_t*>(ctx + 0x1d8) = scWidth;
        *reinterpret_cast<int32_t*>(ctx + 0x1dc) = scHeight;
    }

    // Let the current loop draw its content
    VCall_Draw(self->m_currentLoop, alpha);

    // Invoke post-draw callbacks: callback(videoContext, userData)
    for (int32_t i = 0; i < self->m_postDrawCount; i++) {
        typedef void (*PostDrawFn_t)(void*, void*);
        ((PostDrawFn_t)self->m_postDrawCallbacks[i].funcPtr)(
            self->m_context,
            self->m_postDrawCallbacks[i].userData
        );
    }

    // EndFrame: device->vtable[2]()
    typedef void (*DeviceEndFrame_t)(void*);
    ((DeviceEndFrame_t)deviceVtable[2])(device);

    // Capture render phase duration (from frame start to now)
    int64_t qpc = GetQPC();
    int64_t now = QPCToTicks(qpc);
    int64_t remainder = QPCRemainder(qpc);
    self->m_frameUpdateTime = now - self->m_frameStartTime;

    // Increment frame counter at context+0x8
    (*reinterpret_cast<int32_t*>(ctx + 0x8))++;

    // Present: device->vtable[3](device, remainderTicks)
    typedef void (*DevicePresent_t)(void*, int64_t);
    ((DevicePresent_t)deviceVtable[3])(device, remainder);
}


// ============================================================
// Audio channel cleanup helper (called from Frame)
// ============================================================
// Audio::Device has an inline array of 64 channels starting at offset 0.
// Each channel is 0x28 bytes:
//   +0x00: uint32_t flags (bit 1 = pending free)
//   +0x08: uint64_t (cleared on free)
//   +0x10: uint64_t (callback arg 2)
//   +0x18: uint32_t (callback arg 3)
//   +0x20: void*    completion callback (or null)

static void CleanupAudioChannels(MainLoop* self) {
    void* audioDevice = GetAudioDevice(self);
    uint8_t* base = reinterpret_cast<uint8_t*>(audioDevice);

    for (int ch = 0; ch < 0x40; ch++) {
        uint8_t* channelBase = base + ch * 0x28;
        uint32_t* flags = reinterpret_cast<uint32_t*>(channelBase);

        if (*flags & 2) {
            // Channel flagged for release
            fn_AudioDevice_FreeChannel(audioDevice, ch);
            *flags &= ~3u;  // clear bits 0-1 (active + pending-free)
            *reinterpret_cast<uint64_t*>(channelBase + 0x08) = 0;

            // Fire completion callback if registered
            typedef void (*AudioCB_t)(void*, uint64_t, uint32_t);
            auto callback = *reinterpret_cast<AudioCB_t*>(channelBase + 0x20);
            if (callback) {
                uint64_t cbArg1 = *reinterpret_cast<uint64_t*>(channelBase + 0x10);
                uint32_t cbArg2 = *reinterpret_cast<uint32_t*>(channelBase + 0x18);
                callback(channelBase, cbArg1, cbArg2);
            }
        }
    }
}


// ============================================================
// MainLoop::Frame (0x1800dd100)
// The heart of the game loop. Implements fixed-timestep with
// interpolated rendering:
//   1. Accumulate elapsed time
//   2. Run N fixed-step update ticks (input, audio, game logic)
//   3. Compute interpolation fraction
//   4. Draw one interpolated render frame
//   5. Cooperative yield
// ============================================================

static bool Reimpl_Frame(MainLoop* self, bool drawOnly) {
    // --- Capture current time and update accumulator ---
    int64_t now = QPCToTicks(GetQPC());
    int64_t elapsed = now - self->m_lastTimestamp;
    self->m_lastTimestamp = now;
    self->m_accumulator += elapsed;

    // --- Spiral-of-death guard ---
    // If accumulator exceeds max (e.g., after breakpoint, alt-tab, long stall),
    // clamp to one tick to prevent the update loop from running hundreds of iterations.
    if (self->m_accumulator > self->m_maxAccumulator) {
        self->m_accumulator = self->m_deltaTime;
    }

    // --- Clear quit flag for this frame ---
    self->m_quit = 0;

    // --- Fixed-timestep update loop ---
    // Run as many fixed-step ticks as the accumulated time allows.
    while (self->m_accumulator >= self->m_deltaTime) {

        // Update input devices (one tick's worth)
        fn_InputDevice_Update(GetInputDevice(self), reinterpret_cast<void*>(&self->m_deltaTime));

        // Clean up finished audio channels
        CleanupAudioChannels(self);

        // Check if the application has requested quit (e.g., via scripting/UI)
        uint8_t* app = reinterpret_cast<uint8_t*>(GetApplication(self));
        if (*(app + 0x28) != 0) {
            PostQuitMessage(0);
        }

        // Pump the Windows message queue
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                self->m_quit = 1;
                break;
            }
        }

        // Tick game logic (unless in draw-only mode)
        if (!drawOnly) {
            bool ok = g_addr_Update(self, reinterpret_cast<Time*>(&self->m_deltaTime));
            if (!ok) {
                self->m_quit = 1;
            }
        }

        // Invoke post-update callbacks: callback(&deltaTime, userData)
        for (int32_t i = 0; i < self->m_postUpdateCount; i++) {
            typedef void (*PostUpdateFn_t)(void*, void*);
            ((PostUpdateFn_t)self->m_postUpdateCallbacks[i].funcPtr)(
                &self->m_deltaTime,
                self->m_postUpdateCallbacks[i].userData
            );
        }

        // Drain one tick from the accumulator, advance total elapsed time
        self->m_accumulator -= self->m_deltaTime;
        self->m_totalTime += self->m_deltaTime;
    }

    // --- Compute interpolation alpha ---
    // Alpha is the fraction of a tick remaining in the accumulator (0..1).
    // Used to interpolate between the last two simulation states for smooth rendering.
    double accSec = (double)self->m_accumulator * (*g_Timer_secondsPerTick);
    double dtSec  = (double)self->m_deltaTime * (*g_Timer_secondsPerTick);
    float alpha = (float)(accSec / dtSec);

    // --- Write interpolated time to Video::Context ---
    uint8_t* ctx = reinterpret_cast<uint8_t*>(self->m_context);
    float totalSec = (float)((double)self->m_totalTime * (*g_Timer_secondsPerTick));
    float dtSecF   = (float)((double)self->m_deltaTime * (*g_Timer_secondsPerTick));
    *reinterpret_cast<float*>(ctx + 0x3c8) = totalSec + dtSecF * alpha;
    *reinterpret_cast<uint32_t*>(ctx + 0x3cc) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3d0) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3d4) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x800;  // dirty: time updated

    // --- Draw the interpolated frame ---
    g_addr_Draw(self, alpha);

    // --- Cooperative yield ---
    // Let other threads (game workers, OS) run before we start the next frame.
    SwitchToThread();

    return (bool)self->m_quit;
}


// ============================================================
// MainLoop::Run (0x1800dd3c0)
// Outer main loop. Sets up the first UpdateLoop and calls Frame
// in a while loop until quit.
// ============================================================

static void Reimpl_Run(MainLoop* self, UpdateLoop* loop) {
#ifdef _DEBUG
    printf("[MainLoop::Run] ENTER self=%p loop=%p\n", (void*)self, (void*)loop);
    printf("[MainLoop::Run]   fn_InputDevice_Update  = %p\n", (void*)fn_InputDevice_Update);
    printf("[MainLoop::Run]   fn_AudioDevice_Update  = %p\n", (void*)fn_AudioDevice_Update);
    printf("[MainLoop::Run]   fn_App_PlatformUpdate  = %p\n", (void*)fn_App_PlatformUpdate);
    printf("[MainLoop::Run]   g_addr_Update          = %p\n", (void*)g_addr_Update);
    printf("[MainLoop::Run]   m_engine               = %p\n", (void*)self->m_engine);
#endif

    // Capture initial timestamp
    int64_t qpc = GetQPC();
    int64_t initialTicks = QPCToTicks(qpc);
    int64_t remainder = QPCRemainder(qpc);

    // Set up the first loop
    self->m_currentLoop = loop;
    self->m_lastTimestamp = initialTicks;

    // Initialize the loop with the sub-tick remainder
    VCall_Startup(loop, remainder);

#ifdef _DEBUG
    printf("[MainLoop::Run] Startup done, running initial system updates...\n");
#endif

    // Initial system updates before the first frame
    fn_InputDevice_Update(GetInputDevice(self), reinterpret_cast<void*>(&self->m_deltaTime));

    if (fn_AudioDevice_Update) {
        fn_AudioDevice_Update(GetAudioDevice(self), reinterpret_cast<void*>(&self->m_deltaTime));
    } else {
#ifdef _DEBUG
        printf("[MainLoop::Run] WARNING: fn_AudioDevice_Update is NULL, skipping\n");
#endif
    }

    if (fn_App_PlatformUpdate) {
        fn_App_PlatformUpdate(GetApplication(self));
    } else {
#ifdef _DEBUG
        printf("[MainLoop::Run] WARNING: fn_App_PlatformUpdate is NULL, skipping\n");
#endif
    }

#ifdef _DEBUG
    printf("[MainLoop::Run] Initial Update tick...\n");
#endif

    // Run one initial update tick
    g_addr_Update(self, reinterpret_cast<Time*>(&self->m_deltaTime));

#ifdef _DEBUG
    printf("[MainLoop::Run] Entering main loop, quit=%d\n", (int)self->m_quit);
#endif

    // --- Main loop ---
    // Resolve Frame address once (stable across the session)
    MainLoop_Frame_t frameAddr = reinterpret_cast<MainLoop_Frame_t>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Frame));

    while (!self->m_quit) {
        // Capture frame start time
        self->m_frameStartTime = QPCToTicks(GetQPC());

        // Run one frame (fixed-step updates + interpolated draw)
        frameAddr(self, false);

        // Capture frame end time and compute frame duration
        int64_t endTicks = QPCToTicks(GetQPC());
        self->m_frameEndTime = endTicks;
        self->m_frameTime = endTicks - self->m_frameStartTime;
    }

#ifdef _DEBUG
    printf("[MainLoop::Run] Main loop exited, cleaning up...\n");
#endif

    // --- Cleanup ---
    // Shutdown the current loop
    VCall_Shutdown(self->m_currentLoop);

    // Destroy the loop (virtual destructor, flag=1 = free)
    UpdateLoop* finalLoop = self->m_currentLoop;
    if (finalLoop) {
        VCall_Destroy(finalLoop, 1);
    }

#ifdef _DEBUG
    printf("[MainLoop::Run] EXIT\n");
#endif
}


// ============================================================
// Install / Remove functions (called from hooks.cpp)
// ============================================================

void InstallMainLoopFrame() {
    // Resolve addresses needed by Frame
    g_addr_Update = reinterpret_cast<MainLoop_Update_t>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Update));
    g_addr_Draw = reinterpret_cast<MainLoop_Draw_t>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Draw));

    uintptr_t target = ResolveAddr(Module::Runtime, MainLoopAddr::Frame);
    // Prologue: MOV(5)+PUSH(1)+SUB(4)+XORPS(3)=13 bytes
    InstallHook(target, (void*)Reimpl_Frame, &g_hookFrame, 13);

#ifdef _DEBUG
    printf("[MainLoop] Frame hooked at %p -> Reimpl_Frame\n", (void*)target);
    printf("[MainLoop]   Update @ %p, Draw @ %p\n", (void*)g_addr_Update, (void*)g_addr_Draw);
#endif
}

void RemoveMainLoopFrame() {
    RemoveHook(&g_hookFrame);
#ifdef _DEBUG
    printf("[MainLoop] Frame hook removed\n");
#endif
}

void InstallMainLoopUpdate() {
    uintptr_t target = ResolveAddr(Module::Runtime, MainLoopAddr::Update);
    // Prologue: MOV(5)+PUSH(1)+SUB(4)+MOV(3)=13 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[MainLoop] Update hooked at %p\n", (void*)target);
#endif
}

void RemoveMainLoopUpdate() {
    RemoveHook(&g_hookUpdate);
#ifdef _DEBUG
    printf("[MainLoop] Update hook removed\n");
#endif
}

void InstallMainLoopDraw() {
    uintptr_t target = ResolveAddr(Module::Runtime, MainLoopAddr::Draw);
    // Prologue: 13 bytes (verified against binary)
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[MainLoop] Draw hooked at %p\n", (void*)target);
#endif
}

void RemoveMainLoopDraw() {
    RemoveHook(&g_hookDraw);
#ifdef _DEBUG
    printf("[MainLoop] Draw hook removed\n");
#endif
}

void InstallMainLoopSetNextLoop() {
    uintptr_t target = ResolveAddr(Module::Runtime, MainLoopAddr::SetNextLoop);
    InstallHook(target, (void*)Reimpl_SetNextLoop, &g_hookSetNextLoop);
#ifdef _DEBUG
    printf("[MainLoop] SetNextLoop hooked at %p\n", (void*)target);
#endif
}

void RemoveMainLoopSetNextLoop() {
    RemoveHook(&g_hookSetNextLoop);
#ifdef _DEBUG
    printf("[MainLoop] SetNextLoop hook removed\n");
#endif
}

void InstallMainLoopRun() {
    // Also resolve Frame address for Run to call
    g_addr_Update = reinterpret_cast<MainLoop_Update_t>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Update));
    g_addr_Draw = reinterpret_cast<MainLoop_Draw_t>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Draw));

    uintptr_t target = ResolveAddr(Module::Runtime, MainLoopAddr::Run);
    // Prologue: MOV(5)+PUSH(2)+SUB(4)+MOV(4)=15 bytes
    InstallHook(target, (void*)Reimpl_Run, &g_hookRun, 15);
#ifdef _DEBUG
    printf("[MainLoop] Run hooked at %p\n", (void*)target);
#endif
}

void RemoveMainLoopRun() {
    RemoveHook(&g_hookRun);
#ifdef _DEBUG
    printf("[MainLoop] Run hook removed\n");
#endif
}
