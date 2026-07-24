// Out-of-line half of dx11_backend_access.h — see that header for the design.

#include "dx11_backend_access.h"

#include <cstdio>

namespace VideoDX11 {

void* Dx11ResolveRuntimeImport(const char* mangled, uintptr_t iatGhidra) {
    // Preferred: Runtime-x64.dll export table, by MSVC mangled name.
    void* fn = ResolveExport(mangled);
    if (fn) return fn;

    // Fallback: read VideoDX11's own IAT slot. This is the pointer the shipped
    // code CALLs through, so it is by construction the right target — it just
    // requires the loader to have bound the DLL.
    //
    // Only available in AB mode. In Standalone mode the DLL is never mapped,
    // ResolveAddr returns 0, and this degrades to the FAILED path below, which
    // is correct: a Runtime export we cannot resolve by name is a real problem
    // and must not be papered over.
    uintptr_t slot = ResolveAddr(Module::VideoDX11, iatGhidra);
    if (slot) {
        fn = *reinterpret_cast<void**>(slot);
        if (fn) {
            fprintf(stderr,
                    "[Dx11] ResolveExport(\"%s\") missed; using VideoDX11 IAT "
                    "slot 0x%llx -> %p\n",
                    mangled, static_cast<unsigned long long>(iatGhidra), fn);
            return fn;
        }
    }

    fprintf(stderr, "[Dx11] FAILED to resolve Runtime import \"%s\" "
                    "(IAT slot 0x%llx)\n",
            mangled, static_cast<unsigned long long>(iatGhidra));
    return nullptr;
}

bool Dx11VerifyDeviceSingleton() {
    void** viaRuntime = Dx11VideoDeviceSingletonSlot();

    uintptr_t iat = ResolveAddr(Module::VideoDX11, kDx11Iat_VideoDeviceSingleton);
    if (!iat) return true;   // VideoDX11 not loaded: nothing to contradict us
    void** viaIat = *reinterpret_cast<void***>(iat);

    if (viaRuntime == viaIat) return true;

    fprintf(stderr,
            "[Dx11] Video::Device::_singleton MISMATCH: Runtime 0x%llx -> %p, "
            "VideoDX11 IAT 0x%llx -> %p. Ported and un-ported slots would read "
            "different Devices.\n",
            static_cast<unsigned long long>(kRt_VideoDeviceSingleton),
            static_cast<void*>(viaRuntime),
            static_cast<unsigned long long>(kDx11Iat_VideoDeviceSingleton),
            static_cast<void*>(viaIat));
    return false;
}

}  // namespace VideoDX11
