// standalone/reg_action_child_ctors2.cpp
// Registration for the B3 leaf Action copy-ctors + ConsumeRawMaterial list helper
// (code/Game/Action/action_child_ctors2.cpp). Wired via RegisterActionChildCtors2Batch().
// Closure-safe (all sub-callees registered); faithful Ghidra-verified ports.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void* ActChild_CompleteRoadCopyCtor_c59d0(void*, uint32_t, uint32_t, uint8_t); // 0x1400c59d0
extern void* ActChild_HuntAnimalCopyCtor_c8400(void*, uint64_t*, uint64_t*);          // 0x1400c8400
extern void* ActChild_DropOffRawCopyCtor_c7cd0(void*, uint64_t*, uint64_t*, uint64_t, uint32_t, uint8_t); // 0x1400c7cd0
extern void  Entity_ConsumeListAppend_64b0(int64_t, int64_t, int);                    // 0x1400c64b0
extern void* ActChild_ConsumeRawCopyCtor_c63b0(void*, uint64_t*, int64_t, uint64_t, uint8_t); // 0x1400c63b0

void RegisterActionChildCtors2Batch() {
    RegisterExeAddr(0x1400c59d0, reinterpret_cast<uintptr_t>(&ActChild_CompleteRoadCopyCtor_c59d0));
    RegisterExeAddr(0x1400c8400, reinterpret_cast<uintptr_t>(&ActChild_HuntAnimalCopyCtor_c8400));
    RegisterExeAddr(0x1400c7cd0, reinterpret_cast<uintptr_t>(&ActChild_DropOffRawCopyCtor_c7cd0));
    RegisterExeAddr(0x1400c64b0, reinterpret_cast<uintptr_t>(&Entity_ConsumeListAppend_64b0));
    RegisterExeAddr(0x1400c63b0, reinterpret_cast<uintptr_t>(&ActChild_ConsumeRawCopyCtor_c63b0));
}
#endif // STANDALONE_EXE
