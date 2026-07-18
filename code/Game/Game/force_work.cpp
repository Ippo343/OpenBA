// code/Game/Game/force_work.cpp
// Generic force-work testing framework core — see force_work.h.
// Extracted from forester_force_harness.cpp (unchanged mechanics) so multiple module
// tests (forester, construction, ...) share the crash-safe probes + drive step.


#include "force_work.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <windows.h>   // VirtualQuery

namespace ForceWork {

bool Readable(const void* p, size_t n) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect;
    if (prot & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD rd = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                     PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if (!(prot & rd)) return false;
    uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return (reinterpret_cast<uintptr_t>(p) + n) <= end;
}

bool Executable(const void* p) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect;
    if (prot & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD ex = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                     PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (prot & ex) != 0;
}

uint32_t SafeGetTypeId(void* obj) {
    if (!Readable(obj, 8)) return 0;
    uintptr_t vt = *reinterpret_cast<uintptr_t*>(obj);
    if (!Readable(reinterpret_cast<void*>(vt + 0x10), 8)) return 0;
    void* slot = *reinterpret_cast<void**>(vt + 0x10);
    if (!Executable(slot)) return 0;
    typedef uint32_t (*gt_t)(void*);
    return reinterpret_cast<gt_t>(slot)(obj);
}

void* FindComp(void* entity, uint32_t tid) {
    if (!entity || !tid || !Readable(entity, 0x40)) return nullptr;
    int count = *reinterpret_cast<int*>((uintptr_t)entity + kEntCompCnt);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>((uintptr_t)entity + kEntCompArr);
    if (count <= 0 || count > 512 || !Readable(reinterpret_cast<void*>(arr), (size_t)count * 8))
        return nullptr;
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        if (SafeGetTypeId(comp) == tid) return comp;
    }
    return nullptr;
}

int WorkAreaCenterCell(void* area) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return -1;
    static uint32_t tidMap = 0;
    if (!tidMap) tidMap = ::Object::GetTypeIdByName("MapComponent");
    void* mc = FindComp(area, tidMap);
    if (!mc || !Readable(mc, 0x30)) return -1;
    uintptr_t game = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!game) return -1;
    int dim = *reinterpret_cast<int*>(game);
    if (dim <= 0) return -1;
    int16_t tileX = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x28);
    int16_t tileY = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x2a);
    int16_t fpW   = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x2c);
    int16_t fpH   = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x2e);
    int cell = (((int)fpH >> 1) + (int)tileY) * dim + (((int)fpW >> 1) + (int)tileX);
    if (cell < 0 || cell >= dim * dim) return -1;
    return cell;
}

bool EnvOn(const char* name) {
    const char* e = std::getenv(name);
    return e && e[0] && e[0] != '0';
}

bool CitizenStackIdle(void* citizen) {
    if (!citizen || !Readable(citizen, kCharCompOff + 8)) return false;
    void* charComp = *reinterpret_cast<void**>((uintptr_t)citizen + kCharCompOff);
    if (!charComp || !Readable(charComp, kActionTopOff + 8)) return false;
    return *reinterpret_cast<int*>((uintptr_t)charComp + kActionTopOff) < 0;
}

int DriveWhenIdle(void* citizen, void* work, int (*driveFn)(void*, void*),
                  const char* tag, int* driveCount, int traceMax) {
    if (!work || !driveFn || !fn_InternalBasePtr_Ctor) return INT_MIN;
    if (!CitizenStackIdle(citizen)) return INT_MIN;

    int64_t citizenIBP = 0;
    fn_InternalBasePtr_Ctor(&citizenIBP, citizen);
    int rc = driveFn(work, &citizenIBP);

    if (driveCount && *driveCount < traceMax) {
        ++*driveCount;
        void* charComp = *reinterpret_cast<void**>((uintptr_t)citizen + kCharCompOff);
        int top = charComp ? *reinterpret_cast<int*>((uintptr_t)charComp + kActionTopOff) : -999;
        std::fprintf(stderr, "[%s] DRIVE#%d citizen=%p ret=%d newTop+0x60=%d\n",
                     tag ? tag : "ForceWork", *driveCount, citizen, rc, top);
        std::fflush(stderr);
    }
    return rc;
}

}  // namespace ForceWork

