// standalone/loader_loop_stubs.cpp
// Standalone-only stub vtables for the exe's two LoaderLoop subclasses
// (vtables @ 0x140158870 inner and @ 0x1401588d0 outer, used in this nesting
// in the original GameMain).
//
// LoaderLoop has 12 virtuals (UpdateLoop's full set, no CreateDialog).
//
// Inner vtable @ 0x140158870 (PTR_FUN_140158870) — wraps the splash chain:
//   +0x00  Destroy(self, flags)            — exe 0x140001150 (LoaderLoop::~ + free)
//   +0x08  Startup                         — thunk 0x14003A8E2 -> Runtime LoaderLoop::Startup
//   +0x10  Shutdown                        — thunk 0x14003A8DC
//   +0x18  Update(self, time)              — thunk 0x14003A8D6 -> Runtime LoaderLoop::Update
//   +0x20  Draw(self, alpha)               — thunk 0x14003A8D0 -> Runtime LoaderLoop::Draw
//   +0x28  DrawFailed                      — thunk 0x14003A90C
//   +0x30  Load(self)                      — exe 0x140001120 (PlacementTool ctor + reset loadBit)
//   +0x38  SaveRestore                     — thunk 0x14003A906
//   +0x40  OnVideoError                    — thunk 0x14003A8C4
//   +0x48  OnVideoRestart                  — thunk 0x14003A900
//   +0x50  GetResourceFilename             — exe 0x140001110 (returns "Game/Global")
//   +0x58  AllowsModChanges                — thunk 0x14003A8F4
//
// Outer vtable @ 0x1401588d0 (g_GameLoaderLoop_Vtable) — wraps the inner LoaderLoop:
//   +0x00  Destroy(self, flags)            — exe 0x140001190 (identical to inner Destroy)
//   +0x08-0x58                             — all thunks to Runtime LoaderLoop methods
//
// First-pass: every slot is a print-only stub. Update returns false on the first
// tick so MainLoop::Run exits cleanly. Real reimpls will land slot-by-slot.

#ifdef STANDALONE_EXE

#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "UpdateLoop.h"
#include <cstdio>
#include <cstdint>

namespace {

// ============================================================
// Inner LoaderLoop stubs (vtable @ 0x140158870)
// ============================================================

void Inner_Destroy(UpdateLoop* self, unsigned flags) {
    printf("[InnerLL::Destroy] self=%p flags=%u\n", (void*)self, flags);
    // Skip the LoaderLoop::~LoaderLoop chain (UpdateLoop dtor + ExternalList free).
    // Acceptable leak — process exits immediately after Run returns.
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

void Inner_Startup(UpdateLoop* self) {
    printf("[InnerLL::Startup] self=%p\n", (void*)self);
}

void Inner_Shutdown(UpdateLoop* self) {
    printf("[InnerLL::Shutdown] self=%p\n", (void*)self);
}

bool Inner_Update(UpdateLoop* self, void* time) {
    static int tick = 0;
    ++tick;
    printf("[InnerLL::Update] self=%p time=%p tick=%d -> false\n",
           (void*)self, time, tick);
    return false;  // exit MainLoop::Run on first tick
}

void Inner_Draw(UpdateLoop* self, float alpha) {
    static int draws = 0;
    if (draws++ < 3) {
        printf("[InnerLL::Draw] self=%p alpha=%f\n", (void*)self, alpha);
    }
}

void Inner_DrawFailed(UpdateLoop* self) {
    printf("[InnerLL::DrawFailed] self=%p\n", (void*)self);
}

void Inner_Load(UpdateLoop* self) {
    printf("[InnerLL::Load] self=%p (skipping PlacementTool init)\n", (void*)self);
    // Real impl @ 0x140001120: PlacementTool_Ctor(KeyMap, self->m_resourceList);
    // self->m_loadBit = -1;
    // We skip both — PlacementTool isn't reimplemented standalone yet.
    self->m_loadBit = -1;
}

void Inner_SaveRestore(UpdateLoop* self, bool save, void* state) {
    printf("[InnerLL::SaveRestore] self=%p save=%d state=%p\n",
           (void*)self, save, state);
}

void Inner_OnVideoError(UpdateLoop* self) {
    printf("[InnerLL::OnVideoError] self=%p\n", (void*)self);
}

void Inner_OnVideoRestart(UpdateLoop* self) {
    printf("[InnerLL::OnVideoRestart] self=%p\n", (void*)self);
}

const char* Inner_GetResourceFilename(UpdateLoop* self) {
    (void)self;
    // Original (FUN_140001110) returns LEA-loaded "Game/Global".
    return "Game/Global";
}

bool Inner_AllowsModChanges(UpdateLoop* self) {
    (void)self;
    // Original thunks to Runtime LoaderLoop::AllowsModChanges → true.
    return true;
}

void* g_innerLoaderLoopVtable[12] = {
    reinterpret_cast<void*>(&Inner_Destroy),               // +0x00
    reinterpret_cast<void*>(&Inner_Startup),               // +0x08
    reinterpret_cast<void*>(&Inner_Shutdown),              // +0x10
    reinterpret_cast<void*>(&Inner_Update),                // +0x18
    reinterpret_cast<void*>(&Inner_Draw),                  // +0x20
    reinterpret_cast<void*>(&Inner_DrawFailed),            // +0x28
    reinterpret_cast<void*>(&Inner_Load),                  // +0x30
    reinterpret_cast<void*>(&Inner_SaveRestore),           // +0x38
    reinterpret_cast<void*>(&Inner_OnVideoError),          // +0x40
    reinterpret_cast<void*>(&Inner_OnVideoRestart),        // +0x48
    reinterpret_cast<void*>(&Inner_GetResourceFilename),   // +0x50
    reinterpret_cast<void*>(&Inner_AllowsModChanges),      // +0x58
};

// ============================================================
// Outer LoaderLoop stubs (vtable @ 0x1401588d0)
// ============================================================
// All slots are pure thunks in the original — same shape as inner but with
// no exe-internal Load/GetResourceFilename overrides.

void Outer_Destroy(UpdateLoop* self, unsigned flags) {
    printf("[OuterLL::Destroy] self=%p flags=%u\n", (void*)self, flags);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

void Outer_Startup(UpdateLoop* self) {
    printf("[OuterLL::Startup] self=%p\n", (void*)self);
}

void Outer_Shutdown(UpdateLoop* self) {
    printf("[OuterLL::Shutdown] self=%p\n", (void*)self);
}

bool Outer_Update(UpdateLoop* self, void* time) {
    static int tick = 0;
    ++tick;
    printf("[OuterLL::Update] self=%p time=%p tick=%d -> false\n",
           (void*)self, time, tick);
    return false;
}

void Outer_Draw(UpdateLoop* self, float alpha) {
    static int draws = 0;
    if (draws++ < 3) {
        printf("[OuterLL::Draw] self=%p alpha=%f\n", (void*)self, alpha);
    }
}

void Outer_DrawFailed(UpdateLoop* self) {
    printf("[OuterLL::DrawFailed] self=%p\n", (void*)self);
}

void Outer_Load(UpdateLoop* self) {
    printf("[OuterLL::Load] self=%p\n", (void*)self);
    self->m_loadBit = -1;
}

void Outer_SaveRestore(UpdateLoop* self, bool save, void* state) {
    printf("[OuterLL::SaveRestore] self=%p save=%d state=%p\n",
           (void*)self, save, state);
}

void Outer_OnVideoError(UpdateLoop* self) {
    printf("[OuterLL::OnVideoError] self=%p\n", (void*)self);
}

void Outer_OnVideoRestart(UpdateLoop* self) {
    printf("[OuterLL::OnVideoRestart] self=%p\n", (void*)self);
}

const char* Outer_GetResourceFilename(UpdateLoop* self) {
    (void)self;
    // Original thunks straight to Runtime LoaderLoop::GetResourceFilename
    // (returns nullptr in the base — stub matches "no resource group").
    return nullptr;
}

bool Outer_AllowsModChanges(UpdateLoop* self) {
    (void)self;
    return true;
}

void* g_outerLoaderLoopVtable[12] = {
    reinterpret_cast<void*>(&Outer_Destroy),               // +0x00
    reinterpret_cast<void*>(&Outer_Startup),               // +0x08
    reinterpret_cast<void*>(&Outer_Shutdown),              // +0x10
    reinterpret_cast<void*>(&Outer_Update),                // +0x18
    reinterpret_cast<void*>(&Outer_Draw),                  // +0x20
    reinterpret_cast<void*>(&Outer_DrawFailed),            // +0x28
    reinterpret_cast<void*>(&Outer_Load),                  // +0x30
    reinterpret_cast<void*>(&Outer_SaveRestore),           // +0x38
    reinterpret_cast<void*>(&Outer_OnVideoError),          // +0x40
    reinterpret_cast<void*>(&Outer_OnVideoRestart),        // +0x48
    reinterpret_cast<void*>(&Outer_GetResourceFilename),   // +0x50
    reinterpret_cast<void*>(&Outer_AllowsModChanges),      // +0x58
};

} // namespace

// Called from address_registry.cpp's RegisterVtablesAndTypeIds().
void RegisterLoaderLoopVtables() {
    RegisterExeAddr(0x140158870, reinterpret_cast<uintptr_t>(g_innerLoaderLoopVtable));
    RegisterExeAddr(0x1401588d0, reinterpret_cast<uintptr_t>(g_outerLoaderLoopVtable));
}

#endif // STANDALONE_EXE
