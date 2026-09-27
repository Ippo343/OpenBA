// standalone/reg_keystone_a_gamedtor.cpp
// Keystone A (Game::Dtor + worker-thread teardown cluster) registration. Maps the exe
// addresses to the faithful 1:1 ports in code/Game/Game/keystone_a_gamedtor.cpp. The
// integrator wires RegisterKeystoneAGameDtorBatch() into RegisterCodeFunctions() in
// address_registry.cpp.
//
// Closure — every exe sub-callee is satisfiable:
//   ported here:   0x14009ed60 0x1400a6690 0x1400a6720 0x1400a8570 0x14001a450
//                  0x14001a5f0 0x14001ced0 0x14001dac0 0x14001e3a0
//   reg elsewhere: 0x140014c30 0x14000e320 (reg_work_base),
//                  0x14001d400 0x14001e490 (reg_pf_pools)
//   Runtime exports (resolved via ResolveExport inside the ports):
//     CriticalSection::Lock/Unlock/~CriticalSection, Thread::Start/WaitForFinish/~Thread,
//     Application::YieldToThreads, Picking::Scene::~Scene; static MemFree via
//     fn_MemFreeStatic (engine_funcs); Application singleton via g_pAppSingleton.
//
// Shutdown-only path (not autostart-exercised) — registering is purely additive.
#include "resolve.h"
#include <cstdint>

extern "C" void Game_Dtor_9ed60(int64_t);                  // 0x14009ed60
extern "C" void GameDtor_DrainQueue_a6690(int64_t);        // 0x1400a6690
extern "C" void GameDtor_SubObjDtor_a8570(int64_t);        // 0x1400a8570
extern "C" void GameDtor_TreeArrayDtor_1a450(int64_t);     // 0x14001a450
extern "C" void GameDtor_FreeListDtor_1a5f0(uint64_t*);    // 0x14001a5f0
extern "C" void GameDtor_CellBstClear_1ced0(int64_t*);     // 0x14001ced0
extern "C" void GameDtor_FreeArray18_1dac0(int64_t*);      // 0x14001dac0
extern "C" void GameDtor_FreeArray20_1e3a0(int64_t*);      // 0x14001e3a0

void RegisterKeystoneAGameDtorBatch() {
    RegisterExeAddr(0x14009ed60, reinterpret_cast<uintptr_t>(&Game_Dtor_9ed60));
}
