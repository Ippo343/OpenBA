// Proxy version.dll - loads the real system version.dll and forwards all calls.
// This file is auto-included in the build. Each exported function resolves the
// real function pointer on first call (lazy init) for robustness.

#include "proxy.h"
#include <cstdio>

static HMODULE g_realVersionDll = nullptr;

// Function pointer storage for each forwarded export
#define DECLARE_PROXY(name) static FARPROC fp_##name = nullptr;

DECLARE_PROXY(GetFileVersionInfoA)
DECLARE_PROXY(GetFileVersionInfoW)
DECLARE_PROXY(GetFileVersionInfoByHandle)
DECLARE_PROXY(GetFileVersionInfoExA)
DECLARE_PROXY(GetFileVersionInfoExW)
DECLARE_PROXY(GetFileVersionInfoSizeA)
DECLARE_PROXY(GetFileVersionInfoSizeW)
DECLARE_PROXY(GetFileVersionInfoSizeExA)
DECLARE_PROXY(GetFileVersionInfoSizeExW)
DECLARE_PROXY(VerFindFileA)
DECLARE_PROXY(VerFindFileW)
DECLARE_PROXY(VerInstallFileA)
DECLARE_PROXY(VerInstallFileW)
DECLARE_PROXY(VerLanguageNameA)
DECLARE_PROXY(VerLanguageNameW)
DECLARE_PROXY(VerQueryValueA)
DECLARE_PROXY(VerQueryValueW)

bool ProxyInit() {
    // Load the REAL version.dll from System32
    char sysDir[MAX_PATH];
    GetSystemDirectoryA(sysDir, MAX_PATH);
    char dllPath[MAX_PATH];
    snprintf(dllPath, MAX_PATH, "%s\\version.dll", sysDir);

    g_realVersionDll = LoadLibraryA(dllPath);
    if (!g_realVersionDll) return false;

    // Resolve all function pointers
    #define RESOLVE(name) fp_##name = GetProcAddress(g_realVersionDll, #name);

    RESOLVE(GetFileVersionInfoA)
    RESOLVE(GetFileVersionInfoW)
    RESOLVE(GetFileVersionInfoByHandle)
    RESOLVE(GetFileVersionInfoExA)
    RESOLVE(GetFileVersionInfoExW)
    RESOLVE(GetFileVersionInfoSizeA)
    RESOLVE(GetFileVersionInfoSizeW)
    RESOLVE(GetFileVersionInfoSizeExA)
    RESOLVE(GetFileVersionInfoSizeExW)
    RESOLVE(VerFindFileA)
    RESOLVE(VerFindFileW)
    RESOLVE(VerInstallFileA)
    RESOLVE(VerInstallFileW)
    RESOLVE(VerLanguageNameA)
    RESOLVE(VerLanguageNameW)
    RESOLVE(VerQueryValueA)
    RESOLVE(VerQueryValueW)

    #undef RESOLVE
    return true;
}

void ProxyShutdown() {
    if (g_realVersionDll) {
        FreeLibrary(g_realVersionDll);
        g_realVersionDll = nullptr;
    }
}

// Generate forwarding stubs. Each one just jumps to the real function.
// We use naked-style passthrough via a common pattern: cast to the original
// signature and tail-call. For version.dll this is fine since all functions
// use the stdcall/cdecl convention on x64 (which is just the MS x64 ABI).

#define PROXY_FUNC(name) \
    extern "C" __declspec(dllexport) void* proxy_##name() { \
        if (!fp_##name) ProxyInit(); \
        /* Tail-call trampoline: we forward with the same stack frame. \
           On x64, the first 4 args are in rcx/rdx/r8/r9 and are preserved \
           across our thin wrapper since we don't touch them. */ \
        return ((void*(*)())fp_##name)(); \
    }

// Actually, the above approach is fragile for functions with real parameters.
// Better: use inline assembly or a simpler approach - just define each as
// a direct call-through using the proper signatures.

// Clean approach: each proxy function has the exact signature and forwards.
#undef PROXY_FUNC

extern "C" {

__declspec(dllexport)
BOOL proxy_GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!fp_GetFileVersionInfoA) ProxyInit();
    typedef BOOL(WINAPI* fn_t)(LPCSTR, DWORD, DWORD, LPVOID);
    return ((fn_t)fp_GetFileVersionInfoA)(lptstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport)
BOOL proxy_GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!fp_GetFileVersionInfoW) ProxyInit();
    typedef BOOL(WINAPI* fn_t)(LPCWSTR, DWORD, DWORD, LPVOID);
    return ((fn_t)fp_GetFileVersionInfoW)(lptstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport)
int proxy_GetFileVersionInfoByHandle(int a, HANDLE b, void* c, void* d) {
    if (!fp_GetFileVersionInfoByHandle) ProxyInit();
    typedef int(WINAPI* fn_t)(int, HANDLE, void*, void*);
    return ((fn_t)fp_GetFileVersionInfoByHandle)(a, b, c, d);
}

__declspec(dllexport)
BOOL proxy_GetFileVersionInfoExA(DWORD dwFlags, LPCSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!fp_GetFileVersionInfoExA) ProxyInit();
    typedef BOOL(WINAPI* fn_t)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
    return ((fn_t)fp_GetFileVersionInfoExA)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport)
BOOL proxy_GetFileVersionInfoExW(DWORD dwFlags, LPCWSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (!fp_GetFileVersionInfoExW) ProxyInit();
    typedef BOOL(WINAPI* fn_t)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
    return ((fn_t)fp_GetFileVersionInfoExW)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

__declspec(dllexport)
DWORD proxy_GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
    if (!fp_GetFileVersionInfoSizeA) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(LPCSTR, LPDWORD);
    return ((fn_t)fp_GetFileVersionInfoSizeA)(lptstrFilename, lpdwHandle);
}

__declspec(dllexport)
DWORD proxy_GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    if (!fp_GetFileVersionInfoSizeW) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(LPCWSTR, LPDWORD);
    return ((fn_t)fp_GetFileVersionInfoSizeW)(lptstrFilename, lpdwHandle);
}

__declspec(dllexport)
DWORD proxy_GetFileVersionInfoSizeExA(DWORD dwFlags, LPCSTR lpwstrFilename, LPDWORD lpdwHandle) {
    if (!fp_GetFileVersionInfoSizeExA) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPCSTR, LPDWORD);
    return ((fn_t)fp_GetFileVersionInfoSizeExA)(dwFlags, lpwstrFilename, lpdwHandle);
}

__declspec(dllexport)
DWORD proxy_GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lpwstrFilename, LPDWORD lpdwHandle) {
    if (!fp_GetFileVersionInfoSizeExW) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPCWSTR, LPDWORD);
    return ((fn_t)fp_GetFileVersionInfoSizeExW)(dwFlags, lpwstrFilename, lpdwHandle);
}

__declspec(dllexport)
DWORD proxy_VerFindFileA(DWORD uFlags, LPCSTR szFileName, LPCSTR szWinDir, LPCSTR szAppDir,
                         LPSTR szCurDir, PUINT puCurDirLen, LPSTR szDestDir, PUINT puDestDirLen) {
    if (!fp_VerFindFileA) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT);
    return ((fn_t)fp_VerFindFileA)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, puCurDirLen, szDestDir, puDestDirLen);
}

__declspec(dllexport)
DWORD proxy_VerFindFileW(DWORD uFlags, LPCWSTR szFileName, LPCWSTR szWinDir, LPCWSTR szAppDir,
                         LPWSTR szCurDir, PUINT puCurDirLen, LPWSTR szDestDir, PUINT puDestDirLen) {
    if (!fp_VerFindFileW) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
    return ((fn_t)fp_VerFindFileW)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, puCurDirLen, szDestDir, puDestDirLen);
}

__declspec(dllexport)
DWORD proxy_VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szSrcDir,
                            LPCSTR szDestDir, LPCSTR szCurDir, LPSTR szTmpFile, PUINT puTmpFileLen) {
    if (!fp_VerInstallFileA) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT);
    return ((fn_t)fp_VerInstallFileA)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, puTmpFileLen);
}

__declspec(dllexport)
DWORD proxy_VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szSrcDir,
                            LPCWSTR szDestDir, LPCWSTR szCurDir, LPWSTR szTmpFile, PUINT puTmpFileLen) {
    if (!fp_VerInstallFileW) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT);
    return ((fn_t)fp_VerInstallFileW)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, puTmpFileLen);
}

__declspec(dllexport)
DWORD proxy_VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang) {
    if (!fp_VerLanguageNameA) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPSTR, DWORD);
    return ((fn_t)fp_VerLanguageNameA)(wLang, szLang, cchLang);
}

__declspec(dllexport)
DWORD proxy_VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD cchLang) {
    if (!fp_VerLanguageNameW) ProxyInit();
    typedef DWORD(WINAPI* fn_t)(DWORD, LPWSTR, DWORD);
    return ((fn_t)fp_VerLanguageNameW)(wLang, szLang, cchLang);
}

__declspec(dllexport)
BOOL proxy_VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    if (!fp_VerQueryValueA) ProxyInit();
    typedef BOOL(WINAPI* fn_t)(LPCVOID, LPCSTR, LPVOID*, PUINT);
    return ((fn_t)fp_VerQueryValueA)(pBlock, lpSubBlock, lplpBuffer, puLen);
}

__declspec(dllexport)
BOOL proxy_VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    if (!fp_VerQueryValueW) ProxyInit();
    typedef BOOL(WINAPI* fn_t)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    return ((fn_t)fp_VerQueryValueW)(pBlock, lpSubBlock, lplpBuffer, puLen);
}

} // extern "C"
