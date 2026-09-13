// Out-of-line half of dx11_backend_access.h — see that header for the design.

#include "dx11_backend_access.h"

#include <cstdio>

namespace VideoDX11 {

void* Dx11ResolveRuntimeImport(const char* mangled, uintptr_t iatGhidra) {
    // The Runtime-x64.dll export table, by MSVC mangled name -- which since
    // ENDGAME.md A.1 means the port registry, because the DLL is never mapped.
    // There is no second source: the old fallback read VideoDX11-x64.dll's own
    // IAT slot, and that DLL is never mapped either. A Runtime export we cannot
    // resolve by name is a real problem and must not be papered over.
    void* fn = ResolveExport(mangled);
    if (fn) return fn;

    fprintf(stderr, "[Dx11] FAILED to resolve Runtime import \"%s\" "
                    "(retail IAT slot 0x%llx)\n",
            mangled, static_cast<unsigned long long>(iatGhidra));
    return nullptr;
}

bool Dx11VerifyDeviceSingleton() {
    // Was a cross-check of Runtime's Video::Device::_singleton against the
    // address VideoDX11-x64.dll's IAT held for it. That DLL is never mapped
    // (ENDGAME.md A.1), so there is no second reader left to disagree with us.
    (void)kDx11Iat_VideoDeviceSingleton;
    return Dx11VideoDeviceSingletonSlot() != nullptr;
}

}  // namespace VideoDX11
