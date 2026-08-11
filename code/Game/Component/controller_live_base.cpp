// code/Game/Component/controller_live_base.cpp
// Shared crash-safe state-node vtable for live ControllerComponents.
// See controller_live_base.h for the full design.

#include "Component/controller_live_base.h"
#include "Component/component_factory_base.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>


namespace {

// State-node virtuals — crash-safe stubs designed against StateMachine::Tick
// (StateMachine_TickReimpl in statemachinecomponent.cpp):
//   * Step returns 0 (>=0): Tick's `while (c < 0)` parent-walk never runs, so it
//     never dereferences a null parent on an empty state-type table.
//   * Enter/Update/Transition return non-zero ('\x01'): Tick's
//     `(... || virt(node,sm)=='\0') && (node=parent)!=0` fall-through loops exit
//     immediately rather than walking the parent chain.
//   * OnState is a void no-op.
// Extra args (sm/time/a/b) are ignored — the Win64 ABI passes them in registers
// the stubs simply don't read.
int  Node_Step(void* /*node*/, int /*input*/)                 { return 0; }
char Node_Enter(void* /*node*/, void* /*sm*/)                 { return 1; }
char Node_Update(void* /*node*/, void* /*sm*/)               { return 1; }
void Node_OnState(void* /*node*/, void* /*sm*/, int, int)     { }
char Node_Transition(void* /*node*/, void* /*sm*/, int64_t)   { return 1; }

// Default no-op for any other slot (dtor / GetTypeId / Serialize / …). Returns 0,
// which reads correctly as void / int / pointer under Win64.
void* Node_Stub(void*, ...) { return nullptr; }

void* g_nodeVtable[ComponentFactory::kInertVtableSlots];
bool  g_nodeVtableInit = false;

}  // namespace

namespace ControllerLive {

void** NodeVtable() {
    if (!g_nodeVtableInit) {
        for (size_t i = 0; i < ComponentFactory::kInertVtableSlots; ++i)
            g_nodeVtable[i] = reinterpret_cast<void*>(&Node_Stub);
        g_nodeVtable[0] = reinterpret_cast<void*>(&Node_Step);        // +0x00 Step
        g_nodeVtable[3] = reinterpret_cast<void*>(&Node_Enter);       // +0x18 Enter
        g_nodeVtable[4] = reinterpret_cast<void*>(&Node_Update);      // +0x20 Update
        g_nodeVtable[5] = reinterpret_cast<void*>(&Node_OnState);     // +0x28 OnState
        g_nodeVtable[6] = reinterpret_cast<void*>(&Node_Transition);  // +0x30 Transition
        g_nodeVtable[7] = reinterpret_cast<void*>(&Node_Transition);  // +0x38 Transition
        g_nodeVtableInit = true;
    }
    return g_nodeVtable;
}

}  // namespace ControllerLive

// ── Family-A component vtable slot 18 — the secondary-base deleting destructor ────
// Identical in retail across CropField (0x140033040), Orchard (0x140009a10) and
// Pasture (0x1400368b0):
//     self->vtable = &<Class>_secondary_vtable;   // unwind to the controller subobject
//     self->vtable = &PTR_FUN_1401629a0;          // then to its base
//     (*dispatch 0x140151ea0)(self);              // External base dtor
//     if (flags & 1) MemFree(self);
//     return self;
//
// Ported here once rather than three times, because the only class-specific part is
// the secondary vtable address — and those two stores are the part we deliberately do
// NOT reproduce: they retarget the PRIMARY vtable pointer at successive base
// subobjects as the destructor unwinds, and we hold no storage for those secondary
// vtables. Writing our live vtable there instead would be actively wrong. When
// flags & 1 is set (the normal case) the object is freed on the next line and the
// stores are unobservable; when it is clear, our object simply keeps its live vtable
// rather than a partially-unwound one, which is benign.
//
// What IS reproduced is the part with observable consequences: the External base dtor
// call (dispatch slot 0x140151ea0, already registered) and the conditional free. The
// inert stub this replaces did neither, so every destroyed crop field, orchard and
// pasture component leaked.
extern "C" void* ControllerLive_ScalarDeletingDtor(void* self, unsigned int flags) {
    if (!self) return self;
    typedef void (*fn_ExternalBaseDtor_t)(void* self);
    static fn_ExternalBaseDtor_t s_baseDtor = nullptr;
    static bool s_res = false;
    if (!s_res) {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140151ea0);
        if (slot) s_baseDtor = *reinterpret_cast<fn_ExternalBaseDtor_t*>(slot);
        s_res = true;
    }
    if (s_baseDtor) s_baseDtor(self);
    if ((flags & 1) != 0 && fn_MemFree && g_pMemAllocSingleton)
        fn_MemFree(g_pMemAllocSingleton, self);
    return self;
}


