// ============================================================================
// src/platform/posix/gfx_posix.cpp -- the graphics-library seam over dxvk-native
// ============================================================================
// The POSIX half of src/platform/common/platform_gfx.h (Phase D.4 POSIX; RENDER
// step 4 consumes it). dxvk-native ships D3D11 and DXGI as two shared libraries;
// each entry point is dlopen + dlsym of the library's SONAME, then of the unversioned
// development name. A library already loaded by the process is returned by dlopen
// itself (the GetModuleHandleW-then-LoadLibraryW pair of gfx_win.cpp in one call).
//
// Library names: dxvk-native's meson build installs libdxvk_d3d11.so.0 /
// libdxvk_dxgi.so.0 (SONAME) with the unversioned .so symlinks for linking.
//
// Both return null after the same "[Dx11] FAILED to resolve" stderr line the
// Windows body prints; the renderer then does not start.
// ============================================================================

#include "platform_gfx.h"

#include <dlfcn.h>

#include <cstdio>
#include <cstring>

namespace {

Platform::GfxProc ResolveGfxImport(const char* const* libraries, const char* name) {
    for (const char* const* lib = libraries; *lib != nullptr; ++lib) {
        void* module = dlopen(*lib, RTLD_NOW | RTLD_LOCAL);
        if (module == nullptr) continue;
        void* sym = dlsym(module, name);
        if (sym != nullptr) {
            // POSIX guarantees a data pointer from dlsym converts to a function pointer;
            // copied bytewise because C++ spells no such conversion.
            Platform::GfxProc fn;
            static_assert(sizeof(fn) == sizeof(sym), "dlsym's void* holds a function pointer");
            std::memcpy(&fn, &sym, sizeof(fn));  // CAST-OK: posix
            return fn;
        }
    }
    std::fprintf(stderr, "[Dx11] FAILED to resolve %s!%s — the renderer cannot start\n",
                 libraries[0], name);
    return nullptr;
}

}  // namespace

namespace Platform {

GfxProc ResolveCreateDXGIFactory1() {
    static const char* const kLibraries[] = {"libdxvk_dxgi.so.0", "libdxvk_dxgi.so", nullptr};
    return ResolveGfxImport(kLibraries, "CreateDXGIFactory1");
}

GfxProc ResolveD3D11CreateDevice() {
    static const char* const kLibraries[] = {"libdxvk_d3d11.so.0", "libdxvk_d3d11.so", nullptr};
    return ResolveGfxImport(kLibraries, "D3D11CreateDevice");
}

}  // namespace Platform
