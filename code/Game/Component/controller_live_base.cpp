// code/Game/Component/controller_live_base.cpp
// Shared crash-safe state-node vtable for live ControllerComponents.
// See controller_live_base.h for the full design.

#include "Component/controller_live_base.h"
#include "Component/component_factory_base.h"
#include <cstdint>

#ifdef STANDALONE_EXE

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

#endif  // STANDALONE_EXE
