// code/Game/Action/action.cpp — ENDGAME B.3.8 block ACT
// The members of `Action` (Action.h). Retail addresses on every body.

#include "Action/Action.h"
#include "Object/exe_type.h"
#include "Object/factory_registration.h"
#include "io/Serializer.h"
#include "resolve.h"

// [0] 0x140021c90: stamp 0x14015faa0, DeletePtr(+0x10) if set, `~Internal`,
// MemFree on &1.
Object::Base* Action::DeletingDtor(unsigned flags) {
    this->~Action();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] 0x140021c80 / 0x140021c70 over Action's BaseTypeInfo. The abstract
// registration stays in factory_registrations_bulk.cpp (B38 D4), so the base
// reads its id by name; only a half-built or half-destroyed action dispatches
// here — every leaf overrides both.
const char* Action::TypeName() const { return "Action"; }
uint32_t    Action::Type() const { return ::Object::GetTypeIdByName("Action"); }

// [3] 0x1400c4b40: one Serializer(stream, 0) scope holding the status bool.
// Retail does NOT call Internal::Persist here.
void Action::Persist(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)
    scope.Serialize(_status, nullptr);
}

// [5]-[9]: retail's shared trivial bodies.
int  Action::Execute(void*, void*) { return 0; }   // 0x1400c4a90 XOR EAX,EAX
void Action::Enter() {}                             // 0x1400c4b90 RET 0
void Action::Exit(void*) {}                         // 0x1400c4ba0 RET 0
void Action::CheckEntity() {}                       // 0x1400c4a30 RET 0
bool Action::GetTarget(void*) { return false; }     // 0x1400c4a40 XOR AL,AL

DEFINE_EXE_VTABLE(Action, "_ZTV6Action")

// B38 D5 / D9 #2. 0x14015faa0 is read back BY ADDRESS by unconverted builders
// outside this block (work_act_{clear,hunt,path,perform}.cpp,
// work_base_actionctor.cpp, citizenai_{actionctors,handlers}.cpp, K1's
// componententity.cpp:46), each of which stamps it into a fresh action before
// the leaf's own table. It used to be served by `&g_ComponentEntity_Standalone
// Vtable[5]` (standalone/address_registry.cpp), a 7-slot run whose slot 2
// answered ComponentEntity's type id; it now serves the compiler's Action
// table. Called from RegisterPlayAnimationVtable() (the first Action-family
// row of RegisterVtablesAndTypeIds, which runs after RegisterCodeFunctions'
// old row and so overrides it) until the coordinator's address_registry hunk
// calls it directly.
extern "C" void RegisterActionVtable() {
    RegisterExeAddr(0x14015faa0, reinterpret_cast<uintptr_t>(Action::Vtable()));
}
