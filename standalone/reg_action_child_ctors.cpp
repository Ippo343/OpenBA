// standalone/reg_action_child_ctors.cpp
// Registration for the 5 missing Action child-ctors (Keystone B, Phase 2):
// code/Game/Action/action_child_ctors.cpp. Wired into RegisterCodeFunctions() in
// standalone/address_registry.cpp via RegisterActionChildCtorsBatch().
//
// Every address is a real function ENTRY (Ghidra Entry == address) and a faithful 1:1
// port. Closure verified: all exe sub-callees are already registered —
//   Entity_ClearPathQueue 0x140003f60 (reg_action_cnr), FUN_140004680 0x140004680
//   (reg_component_nature), CharacterComponent_impl_f3e0 0x14009f3e0 (mapquery),
//   the action vtables 0x1401589b0/0x140160050/0x14015fb10/0x14015fca0 (action reimpls),
//   plus Object::Internal::Internal / CheckPtr engine helpers and the Vector3
//   Normalize/Heading Runtime exports.
#include "resolve.h"
#include <cstdint>

extern void* ActChild_ShowToolCtor_42890(void*, void*, int);                       // 0x140042890
extern void* ActChild_RemoveNRCopyCtor_cadf0(void*, uint64_t*, uint64_t*, uint8_t); // 0x1400cadf0
extern void* ActChild_AnimateNRCopyCtor_c57d0(void*, uint64_t*, uint32_t, uint8_t); // 0x1400c57d0
extern void* ActChild_CreateNRCopyCtor_c6a20(void*, uint64_t, uint64_t*, uint32_t); // 0x1400c6a20
extern float ActChild_Entity29a0_c29a0(float*, void*);                             // 0x1400c29a0

void RegisterActionChildCtorsBatch() {
    RegisterExeAddr(0x140042890, reinterpret_cast<uintptr_t>(&ActChild_ShowToolCtor_42890));
    RegisterExeAddr(0x1400cadf0, reinterpret_cast<uintptr_t>(&ActChild_RemoveNRCopyCtor_cadf0));
    RegisterExeAddr(0x1400c57d0, reinterpret_cast<uintptr_t>(&ActChild_AnimateNRCopyCtor_c57d0));
    RegisterExeAddr(0x1400c6a20, reinterpret_cast<uintptr_t>(&ActChild_CreateNRCopyCtor_c6a20));
    RegisterExeAddr(0x1400c29a0, reinterpret_cast<uintptr_t>(&ActChild_Entity29a0_c29a0));
}
