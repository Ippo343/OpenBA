// code/Game/Game/filesystem_setup.cpp
//
// Standalone-only: relativize the engine FileSystem's resource directories
// after Engine::Engine::Startup, so all resource paths resolve RELATIVE to the
// game's working directory (the install dir) instead of as long absolute
// "C:\GOG Games\Banished\WinData\..." strings.
//
// --- Why ----------------------------------------------------------------
// IO::FileSystem::SetupDirectories (Runtime 0x1800abd00, called from
// IO::FileSystem::Startup <- Engine::Engine::Startup) builds three directory
// Filenames from GetRootDirectory():
//   fs+0x00 source/resource dir = "<root>..\resource\"
//   fs+0x08 data dir           = "<root><platform><build>\"  (= "<root>WinData\")
//   fs+0x10 pack dir           = same as data dir
// where <root> = GetPathDirectory(GetModuleFileNameW(NULL)) = the absolute
// directory of the running exe = "C:\GOG Games\Banished\".
//
// Every resource lookup (Object::ExternalFactory::FindOrLoad /
// ExpandRelativePath) prepends GetDataDirectory() (fs+0x08) to form the full
// resource path, AND uses that full path as the dedup cache key
// (ExternalFactory::Find / Add key off External+0x8). So every resource String
// carries the long absolute prefix. The intermittent menu heap crash is a
// System::String use-after-free whose freed block held one of these absolute
// resource-path strings; shortening the path prefix changes the String size
// class (allocator bucket) and the per-string churn.
//
// Since both the real game and the standalone run with their CWD = the install
// dir (GOG/Steam launch the exe in place), a relative data dir of "WinData\"
// resolves identically via the loose-file fallback (GetFileAttributesW) and
// the packfile lookup (ExistsInPackfile, which already strips to the relative
// form internally). This mirrors what SetupDirectories would produce if
// GetRootDirectory returned "" instead of the absolute exe path.
//
// --- What ---------------------------------------------------------------
// We rewrite the three directory Filenames in-place using System::String::Set
// (the refcounting setter that releases the old String and installs the new
// one) followed by System::Filename::AdjustDelimiters, exactly as
// SetupDirectories does. The relative forms are derived from the absolute ones
// by stripping the "<root>" prefix (the directory of GetModuleFileNameW).
//
// This is standalone-only scaffolding; the DLL-hook build keeps the stock
// absolute paths (the real game tolerates them — it just spends more on path
// strings).


#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <windows.h>

namespace Game {

// System::Filename / System::String are an 8-byte struct: a single pointer to
// ref-counted char data (refcount int32 at data-4, length int32 at data-8).
// fn_String_Set(thisStr, cstr) releases the old backing String and installs a
// fresh one for the C-string — the same call SetupDirectories uses.

static const char* FilenameCStr(void* fnameSlot) {
    char* data = *reinterpret_cast<char**>(fnameSlot);
    return data ? data : "(null)";
}

// Rewrite a single FileSystem directory Filename (at fs+off) to `relative`.
// Uses the refcounting String::Set + AdjustDelimiters, so the old absolute
// String is properly released.
static void SetDir(uint8_t* fs, uintptr_t off, const char* relative) {
    void* slot = fs + off;
    fn_String_Set(slot, relative);
    if (fn_Filename_AdjustDelimiters) fn_Filename_AdjustDelimiters(slot);
}

// Compare two path characters for prefix matching: case-insensitive AND
// delimiter-insensitive ('/' == '\\'). The engine's AdjustDelimiters and our
// GetModuleFileName-derived root may disagree on slash direction, so we treat
// both as equal.
static bool PathCharEq(char a, char b) {
    if (a == '/' || a == '\\') return (b == '/' || b == '\\');
    return (char)tolower((unsigned char)a) == (char)tolower((unsigned char)b);
}

// Strip the leading <root> directory (the install dir) from an absolute engine
// path, yielding the relative remainder. Delimiter- and case-insensitive prefix
// match; falls back to the original if the prefix doesn't match (defensive — we
// then leave it absolute).
static const char* StripRoot(const char* abs, const char* root) {
    if (!abs || !root || !*root) return abs;
    const char* a = abs;
    const char* r = root;
    while (*r) {
        if (!*a || !PathCharEq(*a, *r)) return abs;  // prefix mismatch
        ++a; ++r;
    }
    while (*a == '/' || *a == '\\') ++a;  // drop leading separators
    return a;
}

// Public entry: called from RunGameMain after a successful Engine::Startup,
// before any resource is loaded. Idempotent.
extern "C" void FileSystem_MakePathsRelative() {
    if (!fn_String_Set) {
        printf("[FSRelative] SKIP: fn_String_Set unresolved\n");
        return;
    }

    // Read the FileSystem singleton fresh from the Runtime .data slot
    // (IO::FileSystem::_singleton @ Runtime 0x1801fc6a8). g_pFileSystemSingleton
    // may have been NULL when EngineFuncsInit cached it (Engine::Startup hadn't
    // allocated the FileSystem yet), so go to the source.
    void** fsSlot = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fc6a8));
    uint8_t* fs = fsSlot ? reinterpret_cast<uint8_t*>(*fsSlot) : nullptr;
    if (!fs) {
        printf("[FSRelative] SKIP: FileSystem::_singleton is NULL\n");
        return;
    }

    // Derive <root> = directory of the running exe, normalized to forward
    // slashes with a trailing slash — matches the engine's GetRootDirectory
    // output prefix.
    char modPath[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, modPath, MAX_PATH);
    char root[MAX_PATH] = {0};
    {
        // GetPathDirectory: keep through the last separator (inclusive).
        std::strncpy(root, modPath, sizeof(root) - 1);
        char* lastSep = nullptr;
        for (char* p = root; *p; ++p) {
            if (*p == '\\' || *p == '/') { *p = '/'; lastSep = p; }
        }
        if (lastSep) lastSep[1] = '\0';  // truncate after the final '/'
        else root[0] = '\0';
    }

    const char* curSource = FilenameCStr(fs + 0x00);
    const char* curData   = FilenameCStr(fs + 0x08);
    const char* curPack   = FilenameCStr(fs + 0x10);
    printf("[FSRelative] BEFORE: root=\"%s\"\n", root);
    printf("[FSRelative]   source(+0x00)=\"%s\"\n", curSource);
    printf("[FSRelative]   data  (+0x08)=\"%s\"\n", curData);
    printf("[FSRelative]   pack  (+0x10)=\"%s\"\n", curPack);

    // Compute relative forms by stripping the install-dir prefix. These are
    // copied to local buffers because SetDir releases the backing String the
    // pointer we read points into.
    char relSource[MAX_PATH], relData[MAX_PATH], relPack[MAX_PATH];
    std::strncpy(relSource, StripRoot(curSource, root), sizeof(relSource) - 1); relSource[sizeof(relSource)-1]=0;
    std::strncpy(relData,   StripRoot(curData,   root), sizeof(relData)   - 1); relData[sizeof(relData)-1]=0;
    std::strncpy(relPack,   StripRoot(curPack,   root), sizeof(relPack)   - 1); relPack[sizeof(relPack)-1]=0;

    // Only rewrite if stripping actually shortened the path (i.e. the prefix
    // matched). If a dir was already relative or the prefix didn't match, leave
    // it untouched.
    if (relData[0] && std::strcmp(relData, curData) != 0)
        SetDir(fs, 0x08, relData);
    if (relPack[0] && std::strcmp(relPack, curPack) != 0)
        SetDir(fs, 0x10, relPack);
    if (relSource[0] && std::strcmp(relSource, curSource) != 0)
        SetDir(fs, 0x00, relSource);

    printf("[FSRelative] AFTER:\n");
    printf("[FSRelative]   source(+0x00)=\"%s\"\n", FilenameCStr(fs + 0x00));
    printf("[FSRelative]   data  (+0x08)=\"%s\"\n", FilenameCStr(fs + 0x08));
    printf("[FSRelative]   pack  (+0x10)=\"%s\"\n", FilenameCStr(fs + 0x10));
    fflush(stdout);
}

// Session 4 fix-test for the intermittent menu heap crash. The crash is a
// use-after-free of the FileSystem **data-directory** String (fs+0x08): its
// refcount baseline is 2, but it erodes 2->1->0 over geometry's ~230
// ExternalFactory::ExpandRelativePath / FindOrLoad calls (a Runtime-internal
// over-release), after which it is freed yet GetDataDirectory keeps AddRef'ing
// the dead String -> the +0x100000000 free-list bit-32 flip -> crash.
// (Pinned via standalone/filename_runtime_hook.cpp: BANISHED_FNHOOK catches the
// AddRef-on-freed at GetDataDirectory 0x180007bd0; BANISHED_FNHOOK_DATADIR
// catches the freeing MemFree inside ExpandRelativePath 0x1800bd360.)
//
// FIX/CONFIRM: bump the three directory Strings' refcounts by a large constant
// so the erosion can never reach 0 (these dir Strings live for the whole
// process anyway, so an intentional permanent reference is harmless). If this
// makes the menu crash vanish, the data-dir-String erosion is confirmed as THE
// root cause. Env-gated BANISHED_PIN_DATADIR (default OFF for clean A/B).
extern "C" void FileSystem_PinDataDir() {
    const char* e = getenv("BANISHED_PIN_DATADIR");
    if (!(e && *e && e[0] != '0')) return;

    void** fsSlot = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fc6a8));
    uint8_t* fs = fsSlot ? reinterpret_cast<uint8_t*>(*fsSlot) : nullptr;
    if (!fs) {
        printf("[PinDataDir] SKIP: FileSystem::_singleton is NULL\n");
        return;
    }
    const uintptr_t kBump = 0x00100000;  // huge, but far from the int32 ceiling
    static const uintptr_t kOffs[] = { 0x00, 0x08, 0x10 };
    for (uintptr_t off : kOffs) {
        void* str = *reinterpret_cast<void**>(fs + off);
        if (!str) continue;
        int* rc = reinterpret_cast<int*>(str) - 1;
        int before = *rc;
        *rc = before + (int)kBump;
        printf("[PinDataDir] fs+0x%02zx String=%p refcount %d -> %d (\"%s\")\n",
               (size_t)off, str, before, *rc, FilenameCStr(fs + off));
    }
    fflush(stdout);
}

} // namespace Game

