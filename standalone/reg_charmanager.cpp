// standalone/reg_charmanager.cpp
// Phase 3 Keystone D / move-tick tail — register the character spatial-hash manager.
//
// Maps each manager exe addr to its reimpl in code/Game/Component/charmanager.cpp so
// external callers route here via ResolveAddr(Module::Exe, ...):
//   * OnAdd (0x1401099c0) -> sub_83f0 (assigns CharacterComponent+0x40 char-index)
//   * move-tick TAIL (0x1401094c0) -> sub_8660 (re-place char each tick; resolved via s_fn_18660)
//   * OnRemove -> impl_8560 (release char-index)
//   * the radius queries (FUN_140092260/990) -> helper_28c0 (dormant; routed for correctness)
//
// NOT registered: FUN_1400021d0 (0x1400021d0) — a GENERIC int-array resize. sub_83f0 calls
// our port (CharMgr_IntArrayResize_21d0) DIRECTLY by name, so the exe addr need not be
// registered, and we avoid clobbering any other caller's expectations for that shared leaf.
//
// The lead calls RegisterCharManager() from address_registry.cpp and CharManager_Seed()
// from gameupdateloop_load.cpp; this file only provides the registration.
#include "resolve.h"
#include <cstdint>

extern "C" int64_t CharMgr_sub_83f0(float* mgrF, int64_t* ibp);
extern "C" void    CharMgr_sub_8660(float* mgrF, int idx);
extern "C" void    CharMgr_helper_8c40(int64_t mgr, int idx);
extern "C" void    CharMgr_helper_8d00(int64_t mgr, int idx);
extern "C" float*  CharMgr_helper_28c0(void* slotPtr, float* outRect);
extern "C" void    CharMgr_impl_8560(int64_t mgr, int idx);

void RegisterCharManager() {
    RegisterExeAddr(0x1400183f0, reinterpret_cast<uintptr_t>(&CharMgr_sub_83f0));
    RegisterExeAddr(0x140018660, reinterpret_cast<uintptr_t>(&CharMgr_sub_8660));
    RegisterExeAddr(0x140018c40, reinterpret_cast<uintptr_t>(&CharMgr_helper_8c40));
    RegisterExeAddr(0x140018d00, reinterpret_cast<uintptr_t>(&CharMgr_helper_8d00));
    RegisterExeAddr(0x1400928c0, reinterpret_cast<uintptr_t>(&CharMgr_helper_28c0));
    RegisterExeAddr(0x140018560, reinterpret_cast<uintptr_t>(&CharMgr_impl_8560));
}
