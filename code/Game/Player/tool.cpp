// code/Game/Player/tool.cpp — ENDGAME B.3.8 TOOL
// The members of `Tool` (Tool.h). Retail addresses on every body.

#include "Player/Tool.h"
#include "Object/factory_registration.h"

// [0] 0x14002e9b0: stamp 0x140161f10, `~Internal`, `if (flags & 1) MemFree`.
Object::Base* Tool::DeletingDtor(unsigned flags) {
    this->~Tool();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] 0x14002e950 / 0x14002e940 over Tool's BaseTypeInfo. The abstract
// registration stays in factory_registrations_bulk.cpp (B38 D4), so the base
// reads its id by name. Only a half-built or half-destroyed object dispatches
// here; every leaf overrides both.
const char* Tool::TypeName() const { return "Tool"; }
uint32_t    Tool::Type() const { return ::Object::GetTypeIdByName("Tool"); }

// [10] 0x1400268a0 `XOR AL,AL; RET`.
bool Tool::IsSelected(uint64_t*) { return false; }

// [11] 0x140004950 `MOV dword [RDX],0; MOV RAX,RDX; RET` — no ToolSetting.
Object::Type* Tool::GetSettingType(Object::Type* ret) const {
    *ret = Object::Type(0);
    return ret;
}

// [12] 0x140105860 `MOV EAX,1; RET`.
int Tool::GetCursor() const { return 1; }
