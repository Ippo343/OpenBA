// Component registration wave — batch visuals
//
// Registers the exe addresses whose clean reimpl bodies live in the corresponding
// code/Game/Component/*.cpp files. Only the FEASIBLE helpers (no un-batched exe
// sub-helpers, no unavailable engine internals) are wired here; the deep functional
// reimpls in this batch are reported as SKIPPED to the integrator.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// Reimpl bodies (non-static, defined in their component .cpp files).
extern "C" void AmbientArrayResize_Reimpl(void* arrayBase, int newCount);            // 0x140002540
extern "C" void AmbientEmitterResize_Reimpl(void* arrayBase, int newCount);          // 0x140001770
extern "C" void MapTerrainClear_Reimpl(void* gridPtr, int value, int x, int y,
                                        int w, int h);                               // 0x1400a4d30
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue);            // 0x140002cc0
extern "C" void ModelComponentNotify_Reimpl(void* entity, int param);                // 0x1400c2b70

void RegisterComponentBatch_visuals() {
    RegisterExeAddr(0x140002540, reinterpret_cast<uintptr_t>(&AmbientArrayResize_Reimpl));
    RegisterExeAddr(0x140001770, reinterpret_cast<uintptr_t>(&AmbientEmitterResize_Reimpl));
    RegisterExeAddr(0x1400a4d30, reinterpret_cast<uintptr_t>(&MapTerrainClear_Reimpl));
    RegisterExeAddr(0x140002cc0, reinterpret_cast<uintptr_t>(&CemeteryListErase_Reimpl));
    RegisterExeAddr(0x1400c2b70, reinterpret_cast<uintptr_t>(&ModelComponentNotify_Reimpl));
}
#endif
