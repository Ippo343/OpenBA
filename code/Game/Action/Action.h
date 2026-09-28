#pragma once
// ============================================================================
// code/Game/Action/Action.h — ENDGAME B.3.8 block ACT (B38_RULES.md §2.1, D1-D5)
//
// `Action`, the exe's citizen action-stack node base, as a REAL C++ class whose
// vtable the compiler generates. Global namespace, as in retail (reflected name
// "Action", parent literal "Object::Internal"; the record is DEFINE_EXE_TYPE in
// action.cpp with retail's abstract factory, ENDGAME B.3.9 RF).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1/§6, bytes read 2026-09-26):
//   table 0x14015faa0, 10 slots, pure slot 4, create = 0x140003730 (abstract)
//   ctor (inlined into every leaf create/ctor, e.g. ShowTool create 0x1400013b0):
//        `??0Internal@Object@@` (IAT 0x140152768); stamp 0x14015faa0;
//        +0x10 = 0 (qword); +0x18 = 0 (byte)
//   [0] 0x140021c90 scalar deleting dtor: stamp 0x14015faa0;
//        `if ([this+0x10]) InternalBasePtr::DeletePtr(this+0x10)` (IAT
//        0x140151ab8); `~Internal` (IAT 0x140152770); `if (flags & 1) MemFree`
//   [1] 0x140021c80 TypeName   [2] 0x140021c70 Type
//   [3] 0x1400c4b40 Persist: Serializer(stream, 0); Serialize(bool +0x18);
//        ~Serializer. NO Internal::Persist call.
//   [4] 0x14003addc _purecall — the action's start/precheck (the executor
//        0x1400c4c50 calls it `[vt+0x20](node, stack)` and reads EAX)
//   [5] 0x1400c4a90 `XOR EAX,EAX; RET` — the per-tick main slot
//        (`[vt+0x28](node, stack, time)`, EAX read as int)
//   [6] 0x1400c4b90 `RET 0`   [7] 0x1400c4ba0 `RET 0` (the executor's exit,
//        `[vt+0x38](node, stack)`)   [8] 0x1400c4a30 `RET 0`
//   [9] 0x1400c4a40 `XOR AL,AL; RET`
//
// SLOT NAMES 4-9 are the tree's (the exe has no symbols; B38 B8-6). The argument
// list of 8 is the tree's C body's (B38_RULES §6 NOT VERIFIED); 4, 5 and 7 are
// the executor's (citizenai_executor.cpp:82-100, asm 0x1400c4ced / 0x1400c4d57 /
// 0x1400c4ddc); 6 is 0x1400c4f80's (B3X W0, below).
//
// LAYOUT: Object::Internal (0x10) + InternalBasePtr +0x10 + bool +0x18 = 0x20.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/Internal.h"
#include "Object/exe_type.h"   // DECLARE_EXE_TYPE (ENDGAME B.3.9 RF)

class Action : public Object::Internal {
    DECLARE_EXE_TYPE(Action)

public:
    // ---- the 10 slots, in retail order -------------------------------------
    // The retail dtor's non-vptr work: release +0x10. `~Internal` is chained by
    // the compiler.
    ~Action() override { if (_entity) _entity.DeletePtr(); }         // [0] 0x140021c90
    // [1]/[2]: the abstract base answers its own registration by name (B38 D4,
    // as the pilot's Component); every leaf overrides both.
    // [1] TypeName 0x140021c80 and [2] Type 0x140021c70: DECLARE_EXE_TYPE (retail `MOV RAX,[ti+8]` / `MOV EAX,[ti]`).
    void        Persist(void* stream) override;                   // [3] 0x1400c4b40
    virtual int  AbstractImpl(void* actionStack) = 0;             // [4] _purecall
    virtual int  Execute(void* actionStack, void* time);          // [5] 0x1400c4a90 ret 0
    // [6] args from retail's only caller, Character_UpdateAnimation 0x1400c4f80:
    // RDX = the action stack (CharacterComponent+0x48, `MOV RDX,RSI` 0x1400c4fc4),
    // R8 = the Time* CharacterComponent::Update got in RDX (`MOV R8,RDI` 0x1400c4fbe)
    // before `CALL [RAX+0x30]` 0x1400c4fc7. The bodies read neither.
    virtual void Enter(void* actionStack, void* time);            // [6] 0x1400c4b90 RET
    virtual void Exit(void* actionStack);                         // [7] 0x1400c4ba0 RET
    virtual void CheckEntity();                                   // [8] 0x1400c4a30 RET
    virtual bool GetTarget(void* param);                          // [9] 0x1400c4a40 ret false


    // ---- the 0x10 bytes Action adds, retail order ---------------------------
    // PUBLIC while unconverted bodies (the executor, Pf_ActStackWalkClear_4f10)
    // read them at raw offsets.
    ::Object::InternalBasePtr _entity;   // +0x10 released by the dtor (DeletePtr)
    bool     _status;                     // +0x18 serialized by slot 3; the
                                          //       stack walk 0x1400c4f10 clears it
    uint8_t  _pad19[7];                   // +0x19 never written by the ctor

protected:
    // The inlined retail base ctor: `Internal()`, then +0x10 = 0, +0x18 = 0.
    // The vptr store is the compiler's.
    Action() : Object::Internal(), _entity(), _status(false) {}
};

static_assert(sizeof(Action) == 0x20, "Action is 0x20 bytes (0x10 Internal + 0x10)");
static_assert(alignof(Action) == 8, "Action is 8-aligned");
static_assert(offsetof(Action, _entity) == 0x10, "Action +0x10 entity IBP (0x1400013ee)");
static_assert(offsetof(Action, _status) == 0x18, "Action +0x18 status byte (0x1400013f6)");
