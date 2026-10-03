// code/Game/Action/action.cpp — ENDGAME B.3.8 block ACT
// The members of `Action` (Action.h). Retail addresses on every body.

#include "Action/Action.h"
#include "Object/exe_type.h"
#include "Object/factory_registration.h"
#include "io/Serializer.h"

// [1] TypeName / [2] Type, the record and retail's abstract factory (create =
// the logging stub, in-place = null): ENDGAME B.3.9 RF.
DEFINE_EXE_ABSTRACT_TYPE(Action, "Object::Internal")

// [3] 0x1400c4b40: one Serializer(stream, 0) scope holding the status bool.
// Retail does NOT call Internal::Persist here.
void Action::Persist(IO::Stream& stream) {
    IO::Serializer scope(stream, 0);
    scope.Serialize(_status, nullptr);
}

// [5]-[9]: retail's shared trivial bodies.
int  Action::Execute(void*, void*) { return 0; }   // 0x1400c4a90 XOR EAX,EAX
void Action::Enter(void*, void*) {}                 // 0x1400c4b90 RET 0
void Action::Exit(void*) {}                         // 0x1400c4ba0 RET 0
void Action::CheckEntity() {}                       // 0x1400c4a30 RET 0
bool Action::GetTarget(ComponentEntity**) { return false; }     // 0x1400c4a40 XOR AL,AL

