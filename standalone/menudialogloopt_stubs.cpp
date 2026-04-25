// standalone/menudialogloopt_stubs.cpp
// Standalone-only stub vtable for MenuDialogLoopT (exe vtable @ 0x14015ed00).
//
// MenuDialogLoopT is the exe-internal subclass of Engine::DialogLoopT that drives
// the main menu (Dialog\MainMenu.rsc). Its 13-slot vtable lives at 0x14015ed00 in
// the original exe; in standalone mode we provide our own vtable here.
//
// Slot layout (13 entries — UpdateLoop's 12 + DialogLoopT's CreateDialog at +0x60):
//   +0x00  Destroy(self, flags)           — exe 0x14001A2E0 (scalar deleting dtor)
//   +0x08  Startup(self, remainder)       — exe 0x14009B930
//   +0x10  Shutdown(self)                 — exe 0x14009B280
//   +0x18  Update(self, time)             — exe 0x14009B6E0
//   +0x20  Draw(self, alpha)              — exe 0x14009B440
//   +0x28  DrawFailed(self)               — exe 0x14003A90C (thunk -> Runtime DialogLoopT::DrawFailed)
//   +0x30  Load(self)                     — exe 0x14009A930
//   +0x38  SaveRestore(self, save, state) — exe 0x14003A906 (thunk)
//   +0x40  OnVideoError(self)             — exe 0x14009A8D0
//   +0x48  OnVideoRestart(self)           — exe 0x14009A8B0
//   +0x50  GetResourceFilename(self)      — exe 0x14001A2D0  -> "Game/Menu"
//   +0x58  AllowsModChanges(self)         — exe 0x14001A2C0  -> true
//   +0x60  CreateDialog(self, retptr)     — exe 0x14009BA30
//
// First-pass implementation: everything except GetResourceFilename, AllowsModChanges,
// and Destroy is a stub that prints and returns a safe default. Update returns false
// so MainLoop::Run exits immediately after the first tick. This lets standalone drive
// MenuDialogLoopT_Ctor end-to-end without yet implementing all 11 game-specific slots.
//
// As real reimplementations land in code/Game/Game/menudialogloopt.cpp, replace the
// matching stub entry with the real function and remove the stub here.

#ifdef STANDALONE_EXE

#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "UpdateLoop.h"
#include <cstdio>
#include <cstdint>

namespace {

void Stub_Destroy(UpdateLoop* self, int flags) {
    printf("[MenuDLT::Destroy] self=%p flags=%d\n", (void*)self, flags);
    // Skip the real cleanup chain for now (Audio::Scene dtor, Engine::DialogLoopT dtor,
    // UI::UserInterface dtor, String dtor). Those will leak on this run but are safe to
    // skip because the process exits immediately after MainLoop::Run returns.
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

void Stub_Startup(UpdateLoop* self, int64_t remainder) {
    printf("[MenuDLT::Startup] self=%p remainder=%lld\n", (void*)self, (long long)remainder);
}

void Stub_Shutdown(UpdateLoop* self) {
    printf("[MenuDLT::Shutdown] self=%p\n", (void*)self);
}

bool Stub_Update(UpdateLoop* self, void* time) {
    static int tick = 0;
    ++tick;
    printf("[MenuDLT::Update] self=%p time=%p tick=%d -> false\n", (void*)self, time, tick);
    return false;  // tell MainLoop::Run to bail out
}

void Stub_Draw(UpdateLoop* self, float alpha) {
    static int draws = 0;
    if (draws++ < 3) {
        printf("[MenuDLT::Draw] self=%p alpha=%f\n", (void*)self, alpha);
    }
}

void Stub_DrawFailed(UpdateLoop* self) {
    printf("[MenuDLT::DrawFailed] self=%p\n", (void*)self);
}

void Stub_Load(UpdateLoop* self) {
    printf("[MenuDLT::Load] self=%p\n", (void*)self);
}

void Stub_SaveRestore(UpdateLoop* self, bool save, void* state) {
    printf("[MenuDLT::SaveRestore] self=%p save=%d state=%p\n", (void*)self, save, state);
}

void Stub_OnVideoError(UpdateLoop* self) {
    printf("[MenuDLT::OnVideoError] self=%p\n", (void*)self);
}

void Stub_OnVideoRestart(UpdateLoop* self) {
    printf("[MenuDLT::OnVideoRestart] self=%p\n", (void*)self);
}

const char* Stub_GetResourceFilename(UpdateLoop* self) {
    (void)self;
    // Original exe slot 10 returns the literal "Game/Menu" via LEA RAX,[rip+...].
    return "Game/Menu";
}

bool Stub_AllowsModChanges(UpdateLoop* self) {
    (void)self;
    // Original exe slot 11 = MOV AL,1; RET — i.e. always true.
    return true;
}

void* Stub_CreateDialog(UpdateLoop* self, void* retPtr) {
    printf("[MenuDLT::CreateDialog] self=%p retPtr=%p (returning null IBP)\n",
           (void*)self, retPtr);
    // Pure-virtual in DialogLoopT. Real impl loads & instantiates Dialog\MainMenu.rsc.
    // Return an empty 16-byte InternalPtr (zeroed) for now.
    if (retPtr) {
        uint64_t* p = reinterpret_cast<uint64_t*>(retPtr);
        p[0] = 0;
        p[1] = 0;
    }
    return retPtr;
}

// 13-slot vtable — exposed via RegisterMenuDialogLoopVtable() to the address registry.
void* g_menuDialogLoopVtable[13] = {
    reinterpret_cast<void*>(&Stub_Destroy),               // +0x00
    reinterpret_cast<void*>(&Stub_Startup),               // +0x08
    reinterpret_cast<void*>(&Stub_Shutdown),              // +0x10
    reinterpret_cast<void*>(&Stub_Update),                // +0x18
    reinterpret_cast<void*>(&Stub_Draw),                  // +0x20
    reinterpret_cast<void*>(&Stub_DrawFailed),            // +0x28
    reinterpret_cast<void*>(&Stub_Load),                  // +0x30
    reinterpret_cast<void*>(&Stub_SaveRestore),           // +0x38
    reinterpret_cast<void*>(&Stub_OnVideoError),          // +0x40
    reinterpret_cast<void*>(&Stub_OnVideoRestart),        // +0x48
    reinterpret_cast<void*>(&Stub_GetResourceFilename),   // +0x50
    reinterpret_cast<void*>(&Stub_AllowsModChanges),      // +0x58
    reinterpret_cast<void*>(&Stub_CreateDialog),          // +0x60
};

} // namespace

// Called from address_registry.cpp's RegisterVtablesAndTypeIds().
void RegisterMenuDialogLoopVtable() {
    RegisterExeAddr(0x14015ed00, reinterpret_cast<uintptr_t>(g_menuDialogLoopVtable));
}

#endif // STANDALONE_EXE
