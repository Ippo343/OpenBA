// Out-of-line half of dx11_backend_access.h — see that header for the design.
//
// ENDGAME B.3R block DX: `Dx11ResolveRuntimeImport` (the by-name port-registry
// lookup dx11's import tables went through) is GONE; every former caller calls
// the Runtime body its mangled name bound, by name.

#include "dx11_backend_access.h"

namespace VideoDX11 {

bool Dx11VerifyDeviceSingleton() {
    // Was a cross-check of Runtime's Video::Device::_singleton against the
    // address VideoDX11-x64.dll's IAT held for it. That DLL is never mapped
    // (ENDGAME.md A.1), so there is no second reader left to disagree with us.
    (void)kDx11Iat_VideoDeviceSingleton;
    return Dx11VideoDeviceSingletonSlot() != nullptr;
}

}  // namespace VideoDX11
