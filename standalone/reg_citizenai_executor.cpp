// standalone/reg_citizenai_executor.cpp
// Brick 4b — register the action-stack executor + flush + CharacterComponent move-tick.
//
// Three exe code functions (lead-verified addresses this session):
//   0x1400c4c50  CitizenAI_ActionExecutor_c4c50    — action-stack executor
//   0x1400c4bb0  CitizenAI_ActionFlush_c4bb0       — executor flush/cleanup tail
//   0x1401094c0  CharacterComponent_MoveTick_1094c0 — move-tick (runs executor + transform tail)
//
// No new .rdata const storage is added here: every float const / vtable slot the
// move-tick TAIL reads (0x140172xxx floats, 0x1401bc2e8 _rootOnlyUpdates, the
// 0x14015xxxx dispatch slots) is read live via ResolveAddr in citizenai_executor.cpp
// and most are already registered elsewhere (sqrtf 0x140132010 reg, GridToWorld
// 0x14009f3e0 reg, Vec3::Vector3 slot 0x140152a30 reg, Geometry::SetTransform slot
// 0x1401512e0 reg). The tail is DORMANT in standalone (char-index < 0 early-return),
// so any still-unregistered tail sub-callee resolves to 0 and is null-guarded — it
// never executes until ModelComponent is brought live.
//
// TaskListCleanup 0x1400c5400 (called by the executor when a main-slot returns 2) is
// already registered (reg_work_base.cpp). The executor's InternalBasePtr CheckPtr /
// Ctor / DeletePtr go through engine_funcs.h (Runtime exports), not exe addrs.
//
// NOTE: the CharacterComponent Update-slot vtable promotion + address_registry wiring
// + A/B is done by the lead, not here.
#include "resolve.h"
#include <cstdint>

extern "C" void CitizenAI_ActionExecutor_c4c50(int64_t* stackAt48, void* time);
extern "C" void CitizenAI_ActionFlush_c4bb0(int64_t stack);
extern "C" void CharacterComponent_MoveTick_1094c0(uint8_t* self, void* time);

void RegisterCitizenAIExecutor() {
    RegisterExeAddr(0x1400c4c50, reinterpret_cast<uintptr_t>(&CitizenAI_ActionExecutor_c4c50));
    RegisterExeAddr(0x1400c4bb0, reinterpret_cast<uintptr_t>(&CitizenAI_ActionFlush_c4bb0));
    RegisterExeAddr(0x1401094c0, reinterpret_cast<uintptr_t>(&CharacterComponent_MoveTick_1094c0));
}
