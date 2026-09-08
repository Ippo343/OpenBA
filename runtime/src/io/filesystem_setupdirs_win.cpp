// ============================================================================
// runtime/src/io/filesystem_setupdirs_win.cpp — Wave 9 lane FS
// IO::FileSystem::SetupDirectories — the cutover wall lane ENG named
// ============================================================================
//
//   | Export                                                    | Retail      | Ord  | Instrs |
//   |-----------------------------------------------------------|-------------|------|--------|
//   | ?SetupDirectories@FileSystem@IO@@QEAAXXZ                  | 0x1800abd00 | 4029 |  722   |
//   | ?GetRootDirectory@FileSystem@IO@@QEAA?AVFilename@System@@XZ| 0x1800ad650 | 2878 |   62   |
//
// Both names checked against Runtime-x64.dll's export directory with pefile
// before a body was written.
//
// This file is `_win` (PLAN §4.1 decision 1) because both bodies call Win32
// DIRECTLY: GetRootDirectory calls GetModuleFileNameW, and SetupDirectories
// calls GetFileAttributesW inside the FileSystem::Exists that MSVC inlined
// into it. Both live in KERNEL32, which every link of this project already
// pulls in — **this lane adds NO new import library**; SA_LIBS is untouched.
//
// ---------------------------------------------------------------------------
// WHAT SetupDirectories ACTUALLY IS
// ---------------------------------------------------------------------------
// It computes the three path fields of IO::FileSystem and nothing else:
//
//     +0x00 sourceDirectory  = "<root><sub>..\resource\"
//     +0x08 dataDirectory    = "<root><sub><platform><build>\"
//     +0x10 packDirectory    = dataDirectory
//
// where `<root>` is the directory the running module sits in (with its
// trailing '\'), `<sub>` is "" unless the command line carries `-bin`, and
// `<platform>`/`<build>` are "Win"/"Data" for the shipped enums. Each of the
// three can then be overridden wholesale by a `-pathres` / `-pathdat` /
// `-pathpkg` switch. Before all that it reads `<root>cmd.txt`, if it exists,
// and feeds the file's contents to Configuration::SetCommandLine — a response
// file.
//
// The literals were read with `read_memory`, not guessed:
//   0x180155698 "cmd.txt"   0x1801551f0 ""          0x1801556a0 "bin"
//   0x1801556a8 "..\resource"            0x1801556b8 "%s%s%s\"
//   0x1801556c0 "%s%s%s%s\"  0x1801556d0 "pathres"  0x1801556d8 "pathdat"
//   0x1801556e0 "pathpkg"
// and the reconstruction is confirmed on disk: "%s%s%s%s\" with
// ("<install>\", "", "Win", "Data") is "<install>\WinData\", which is exactly
// the directory the shipped game has.
//
// ---------------------------------------------------------------------------
// THE THREE ARMS THAT ARE WRITTEN OUT BUT ARE **NOT REACHED** ON THIS BOOT
// ---------------------------------------------------------------------------
// The triage rule says a port only CLOSES a row if every callee it needs is
// satisfied. Nine of SetupDirectories' thirteen callees were unported when
// this lane started. Five are ported by this lane; the other four are NOT, and
// each is behind a branch that is provably not taken here. Stating that
// precisely is the whole reason this row can close at all:
//
//  1. `?ExistsInPackfile@FileSystem@IO@@QEBA_N...` (0x1800ab180, ord 2322).
//     Guarded by `this->existsInPackfileGate` (+0x18). The FileSystem
//     constructor (??0FileSystem@IO@@QEAA@XZ, 0x1800ac7c0) writes
//     `MOV word ptr [RCX+0x18],0x100` — i.e. **+0x18 = 0** and +0x19 = 1 —
//     and NOTHING sets +0x18 before SetupDirectories runs: its only writer on
//     this path is FileSystem::Startup, which sets it AFTER the call
//     (runtime/src/io/filesystem_startup.cpp). So the packfile arm is dead
//     here and the platform arm (GetFileAttributesW) is the live one.
//
//  2/3/4. `?OpenFile@...` (0x1800ac880), `?ReadFile@...` (0x1800ace00),
//     `?CloseFile@...` (0x1800accc0), and the unexported memset at
//     0x1800881b0. All four are inside `if (Exists("<root>cmd.txt"))`. There
//     is no cmd.txt in the install; if one is ever dropped there, this port
//     reaches four RT_EXPORT_ONCE doors with nothing behind them at cutover
//     and dies with their names on the [RtUnresolved] list rather than
//     silently. That is the honest failure and it is preferred to a guard.
//
//  5. `?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z`
//     (0x1800c5300, ord 2495) — four call sites, every one behind a
//     `Map_Find(Configuration::_map, <key>)`. Lane ENG established that
//     `SetCommandLine` is handed "" by standalone/main.cpp and returns before
//     its parse loop, so the map is EMPTY and all four finds miss. **Pass a
//     command line and this becomes the next unresolved symbol** — the same
//     condition lane ENG attached to the SetCommandLine row, inherited here.
//
// ---------------------------------------------------------------------------
// THE ONE PLACE THIS DIFFERS FROM RETAIL, AND WHY
// ---------------------------------------------------------------------------
// Retail loads `Configuration::_singleton` with an unguarded
// `MOV R13,[0x1801fc6f8]` and later does `LEA RCX,[R13+0x10]`. With the DLL
// absent that variable does not exist unless the mint provides it, and
// `?_singleton@Configuration@System@@0PEAV12@EA` (ord 4803) was NOT in
// standalone/rt_data_mint_tables.inc when this lane started — lane ENG's
// attempt-08 prediction Q5 named it. This lane regenerates the mint (the
// generator picks the symbol up from the source scan now that two files spell
// it as a literal), which is the real fix; the `if (cfg)` guards below are the
// belt to that braces, and they exist ONLY because our standalone can be
// missing a variable retail always has. Every DEREFERENCE retail makes of a
// pointer it believes non-null stays unguarded.
//
// It costs nothing on the boot path even if the mint row were still missing:
// `GetPlatformName` and `GetBuildName` never read `this` at all (checked in
// the disassembly — RCX is not dereferenced in either body), and the four
// Map_Find calls would simply miss, which is the answer they give anyway on an
// empty command line.
//
// ---------------------------------------------------------------------------
// KILL SWITCH
// ---------------------------------------------------------------------------
//   BANISHED_RT_FSDIRS=0   removes THIS ENTIRE LANE — SetupDirectories,
//                          GetRootDirectory, Filename::GetPathDirectory,
//                          Configuration::GetPlatformName/GetBuildName and all
//                          four System::WideString rows — in one variable, so
//                          the integrator can A/B the wall in one run instead
//                          of bisecting five files.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../src/system/string.h"
#include "../../src/system/widestring.h"
#include "../../src/system/setupdir_helpers.h"

// ResolveAddr, forward-declared at FILE scope rather than pulled in from
// standalone/resolve.h — that header drags in its own <windows.h> assumptions
// and PLAN §4.1 wants runtime/src/ to name it, not include it. Must stay
// byte-identical to resolve.h's declaration.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtSystem {
// Wave 2 lane A entry points, declared locally rather than by editing that
// lane's headers (the argument filesystem_win.cpp made in Wave 4).
String* RtSystem_String_Format(String* ret, const char* fmt, ...);   // 0x1800c9730
void    RtSystem_Filename_AdjustDelimiters(Filename* self);          // 0x1800c6970
}  // namespace RtSystem

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::WideString;
using RtSystem::StringRefCount;
using RtSystem::RtMemAlloc;
using RtSystem::RtMemFree;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtString_AdoptInline;
using RtSystem::RtSystem_String_Set;
using RtSystem::RtSystem_String_ConcatCStr;
using RtSystem::RtSystem_String_Format;
using RtSystem::RtSystem_Filename_AdjustDelimiters;
using RtSystem::RtSystem_Filename_GetPathDirectory;
using RtSystem::RtSystem_Configuration_GetPlatformName;
using RtSystem::RtSystem_Configuration_GetBuildName;
using RtSystem::RtSystem_WideString_CtorFromString;
using RtSystem::RtSystem_WideString_Set;
using RtSystem::RtSystem_WideString_AsUTF8;

// ---------------------------------------------------------------------------
// IO::FileSystem::FileInfo — only what these two bodies touch
// ---------------------------------------------------------------------------
// Sized and pinned from ?OpenFile@FileSystem@IO@@'s own stack initialisation
// (0x1800ac8b3-0x1800ac8da), which writes, relative to the struct base:
//   +0x00 dword -1   +0x08 qword 0   +0x10 dword 0
//   +0x18 qword -1   +0x20 String 0  +0x28 String 0
// SetupDirectories reads +0x00 as the "did it open" handle
// (`CMP dword [fi],-1`) and the DWORD at +0x08 as the byte size
// (`MOV EDI,dword [fi+8]`), and destroys the two Strings in REVERSE order
// (+0x28 then +0x20) at 0x1800abef2/0x1800abf23.
struct FileInfo {
    int32_t  handle;      // +0x00 — -1 means "not open"
    int32_t  _pad04;
    uint64_t size;        // +0x08 — read as a DWORD by SetupDirectories
    int32_t  _unk10;      // +0x10
    int32_t  _pad14;
    int64_t  _unk18;      // +0x18 — initialised to -1
    String   _name0;      // +0x20
    String   _name1;      // +0x28
};
static_assert(sizeof(FileInfo) == 0x30, "FileInfo is 0x30 bytes (OpenFile's frame)");
static_assert(offsetof(FileInfo, size)   == 0x08, "SetupDirectories: MOV EDI,[fi+8]");
static_assert(offsetof(FileInfo, _name0) == 0x20, "OpenFile: MOV [RBP-0x10],0");
static_assert(offsetof(FileInfo, _name1) == 0x28, "OpenFile: MOV [RBP-0x8],0");

namespace {

typedef bool  (*ExistsInPackfileFn)(const FileSystem*, const Filename*);
typedef void* (*OpenFileFn)(FileSystem*, FileInfo*, const Filename*);
typedef bool  (*ReadFileFn)(FileSystem*, FileInfo*, int, void*);
typedef void  (*CloseFileFn)(FileSystem*, FileInfo*);
typedef void  (*SetCommandLineFn)(void*, const String*);
typedef String* (*GetCommandLineStringFn)(const void*, String*, const String*);
typedef void* (*MapFindFn)(void* map, const String* wanted);
typedef void  (*SetupDirectoriesFn)(FileSystem*);

// `?_singleton@Configuration@System@@0PEAV12@EA`, ord 4803, retail 0x1801fc6f8.
// Read through the PORT-AWARE door so it follows the mint at cutover, exactly
// as runtime/src/io/filesystem_startup.cpp reads it.
void** ConfigurationSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Configuration@System@@0PEAV12@EA");
}

void* ConfigurationSingleton() {
    void** slot = ConfigurationSingletonSlot();
    return slot ? *slot : nullptr;
}

// Retail's INLINED String::operator=, which appears six times in
// SetupDirectories (0x1800abfe2, 0x1800ac0b2, 0x1800ac149, 0x1800ac1b4,
// 0x1800ac294, 0x1800ac3f3, 0x1800ac53f). Written out rather than delegated to
// the ported ??4String@System@@ export for two reasons: it is what the
// instructions do, and that export carries a replaying diff oracle which has
// no business firing seven times inside a boot-once function.
//
// The self-assignment test is retail's and is on the BUFFER, not the object:
// `CMP RCX,[RAX]` compares the two data pointers before doing anything.
void AssignInline(String* dst, const String* src) {
    if (dst->data == src->data) return;
    RtString_ReleaseInline(dst);
    dst->data = nullptr;
    RtString_AdoptInline(dst, src->data);
}

bool ConfigHasKey(void* cfg, const char* key) {
    if (cfg == nullptr) return false;      // see the header note on the mint
    String k{nullptr};
    RtSystem_String_Set(&k, key);
    void* map = static_cast<uint8_t*>(cfg) + 0x10;
    // Unexported (no name in either build), so by address. ResolveAddr consults
    // the PORT REGISTRY FIRST and runtime/src/system/configuration.cpp
    // registers 0x18002ace0, so this reaches our port today and at cutover
    // (reference_resolveaddr_consults_port_registry_first.md).
    const bool found = reinterpret_cast<MapFindFn>(
        ResolveAddr(Module::Runtime, 0x18002ace0))(map, &k) != nullptr;
    RtString_ReleaseInline(&k);
    return found;
}

// The `-pathres` / `-pathdat` / `-pathpkg` override, which retail emits three
// times verbatim: look the key up, and if it is there rebuild the target as
// `Format("%s%s%s\\", root, sub, <the switch's value>)`.
void ApplyPathOverride(void* cfg, const char* key, String* target,
                       const char* rootData, const char* subData) {
    if (!ConfigHasKey(cfg, key)) return;

    String k{nullptr};
    RtSystem_String_Set(&k, key);
    String value{nullptr};
    RT_EXPORT_ONCE(GetCommandLineStringFn,
        "?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z")(
            cfg, &value, &k);

    String formatted{nullptr};
    RtSystem_String_Format(&formatted, "%s%s%s\\", rootData, subData, value.data);
    AssignInline(target, &formatted);
    RtSystem_Filename_AdjustDelimiters(reinterpret_cast<Filename*>(target));

    RtString_ReleaseInline(&formatted);
    RtString_ReleaseInline(&value);
    RtString_ReleaseInline(&k);
}

bool SameText(const char* a, const char* b) {
    if ((a == nullptr) != (b == nullptr)) return false;
    if (a == nullptr) return true;
    return std::strcmp(a, b) == 0;
}

}  // namespace

// ============================================================================
// ?GetRootDirectory@FileSystem@IO@@QEAA?AVFilename@System@@XZ  retail 0x1800ad650
// ============================================================================
// "The directory the running module lives in, with its trailing backslash."
//
// Retail's `this` (RCX) is never dereferenced — the body only ever touches
// RDX, the hidden return slot. The parameter is kept because that is what the
// signature is, the same argument Wave 4 made for GetSaveDirectory.
//
// The 0x1000-wchar_t buffer is retail's (`MOV EAX,0x2040 ; CALL __chkstk`,
// then GetModuleFileNameW with a count of 0x1000). It is NOT pre-zeroed by
// retail and is not pre-zeroed here: GetModuleFileNameW NUL-terminates on
// success, and on failure retail reads whatever is there — faithfully
// reproduced rather than papered over, because a failure of
// GetModuleFileNameW on our own process is not a condition this port should
// be inventing behaviour for.
Filename* RtIo_FileSystem_GetRootDirectory(FileSystem* self, Filename* ret) {
    RT_DIFF_BEGIN("IO::FileSystem::GetRootDirectory");
    (void)self;   // retail loads RCX and never reads it

    wchar_t buf[0x1000];
    ::GetModuleFileNameW(nullptr, buf, 0x1000);

    WideString ws{nullptr};
    RtSystem_WideString_Set(&ws, buf);

    String utf8{nullptr};
    RtSystem_WideString_AsUTF8(&ws, &utf8);

    // `Filename f = utf8;` — retail's inlined Filename(const String&): adopt
    // the buffer (AddRef, no copy) and then normalise the delimiters.
    Filename f{nullptr};
    RtString_AdoptInline(reinterpret_cast<String*>(&f), utf8.data);
    RtSystem_Filename_AdjustDelimiters(&f);

    RtSystem_Filename_GetPathDirectory(&f, ret);

    RtString_ReleaseInline(reinterpret_cast<String*>(&f));
    RtString_ReleaseInline(&utf8);
    RtString_ReleaseInline(reinterpret_cast<String*>(&ws));

    // Safe to replay: reads no FileSystem state and allocates a fresh buffer
    // every call, so ours and theirs never share a pointer and only the TEXT
    // can be compared — the same shape as GetSaveDirectory's oracle.
    if (rt_diff_on__) {
        Filename theirs{nullptr};
        RtOriginal<Filename* (*)(FileSystem*, Filename*)>(
            "?GetRootDirectory@FileSystem@IO@@QEAA?AVFilename@System@@XZ")(self, &theirs);
        RT_DIFF_SHADOW(SameText(ret->data, theirs.data),
                       ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RT_DIFF_WORK(true);
        RtString_ReleaseInline(reinterpret_cast<String*>(&theirs));
    }
    return ret;
}

// ============================================================================
// ?SetupDirectories@FileSystem@IO@@QEAAXXZ                 retail 0x1800abd00
// ============================================================================
void RtIo_FileSystem_SetupDirectories(FileSystem* self) {
    RT_DIFF_BEGIN("IO::FileSystem::SetupDirectories");

    // --- 0x1800abd28 -------------------------------------------------------
    Filename root{nullptr};
    RtIo_FileSystem_GetRootDirectory(self, &root);

    // --- 0x1800abd2e: R13 = Configuration::_singleton ----------------------
    void* const cfg = ConfigurationSingleton();

    // --- 0x1800abd44: String cmdPath = root + "cmd.txt" --------------------
    String cmdPath{nullptr};
    RtSystem_String_ConcatCStr(&cmdPath, reinterpret_cast<const String*>(&root),
                               "cmd.txt");

    // --- 0x1800abd4a-0x1800abe02: `if (Exists(Filename(cmdPath)))`, with
    //     FileSystem::Exists inlined by MSVC. See the header note: the +0x18
    //     arm is dead because the constructor leaves that byte at 0.
    bool exists;
    {
        Filename f{nullptr};
        RtString_AdoptInline(reinterpret_cast<String*>(&f), cmdPath.data);
        RtSystem_Filename_AdjustDelimiters(&f);

        if (self->existsInPackfileGate != 0 &&
            RT_EXPORT_ONCE(ExistsInPackfileFn,
                "?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z")(self, &f)) {
            exists = true;
        } else if (self->platformExistsGate == 0) {
            exists = false;
        } else {
            WideString ws{nullptr};
            RtSystem_WideString_CtorFromString(&ws,
                reinterpret_cast<const String*>(&f));
            // `CMP EAX,-1 ; SETNZ` — INVALID_FILE_ATTRIBUTES, not a HRESULT.
            exists = (::GetFileAttributesW(ws.data) != INVALID_FILE_ATTRIBUTES);
            RtString_ReleaseInline(reinterpret_cast<String*>(&ws));
        }
        RtString_ReleaseInline(reinterpret_cast<String*>(&f));
    }

    // --- 0x1800abe08-0x1800abf50: the cmd.txt response file ----------------
    // NOT REACHED on any boot of this project (there is no cmd.txt); the four
    // doors inside are unported and will name themselves if it ever is.
    if (exists) {
        Filename f{nullptr};
        RtString_AdoptInline(reinterpret_cast<String*>(&f), cmdPath.data);
        RtSystem_Filename_AdjustDelimiters(&f);

        FileInfo fi;
        RT_EXPORT_ONCE(OpenFileFn,
            "?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z")(
                self, &fi, &f);
        RtString_ReleaseInline(reinterpret_cast<String*>(&f));

        if (fi.handle != -1) {
            const int size = static_cast<int>(static_cast<uint32_t>(fi.size));
            const int n    = size + 1;
            char* buf = static_cast<char*>(
                RtMemAlloc(static_cast<uint64_t>(static_cast<int64_t>(n)), 4));
            std::memset(buf, 0, static_cast<size_t>(n));   // 0x1800881b0
            RT_EXPORT_ONCE(ReadFileFn,
                "?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z")(
                    self, &fi, size, buf);
            RT_EXPORT_ONCE(CloseFileFn,
                "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z")(self, &fi);

            String text{nullptr};
            RtSystem_String_Set(&text, buf);
            RT_EXPORT_ONCE(SetCommandLineFn,
                "?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z")(cfg, &text);
            RtMemFree(buf);
            RtString_ReleaseInline(&text);
        }
        // ~FileInfo, reverse order — 0x1800abef2 then 0x1800abf23.
        RtString_ReleaseInline(&fi._name1);
        RtString_ReleaseInline(&fi._name0);
    }

    // --- 0x1800abf54-0x1800ac06a: `sub`, the "-bin" sub-directory ----------
    // Retail seeds it from the EMPTY literal at 0x1801551f0, not from the null
    // String: `String::Set(&sub, "")` allocates a zero-length buffer, so
    // `sub.data` is non-null and the three Format calls below can pass it raw.
    String sub{nullptr};
    RtSystem_String_Set(&sub, "");
    if (ConfigHasKey(cfg, "bin")) {
        String k{nullptr};
        RtSystem_String_Set(&k, "bin");
        String value{nullptr};
        RT_EXPORT_ONCE(GetCommandLineStringFn,
            "?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z")(
                cfg, &value, &k);
        AssignInline(&sub, &value);
        RtString_ReleaseInline(&value);
        RtString_ReleaseInline(&k);
    }

    // --- 0x1800ac06a-0x1800ac08b: platform and build names -----------------
    // Retail reads both enums as DWORDs out of the Configuration object
    // (`MOV R8D,dword [R13+0x20]` / `[R13+0x28]`). Both are zero on every boot
    // of this project — the constructor zeroes them and nothing writes them
    // before Engine::Startup step 3 — giving "Win" and "Data".
    const int platformId = cfg ? *reinterpret_cast<const int32_t*>(
                                      static_cast<const uint8_t*>(cfg) + 0x20) : 0;
    const int buildId    = cfg ? *reinterpret_cast<const int32_t*>(
                                      static_cast<const uint8_t*>(cfg) + 0x28) : 0;
    String platform{nullptr};
    RtSystem_Configuration_GetPlatformName(cfg, &platform, platformId);
    String build{nullptr};
    RtSystem_Configuration_GetBuildName(cfg, &build, buildId);

    // --- 0x1800ac08c-0x1800ac0ef: sourceDirectory --------------------------
    // The four `%s` arguments are passed RAW, exactly as retail passes the
    // qwords. A null here would mean GetRootDirectory failed, which is an
    // upstream fault to find and not a thing to guard
    // (CLAUDE.md, "Don't 'fix' a faithful crash with a null check").
    {
        String t{nullptr};
        RtSystem_String_Format(&t, "%s%s%s\\", root.data, sub.data, "..\\resource");
        AssignInline(&self->sourceDirectory, &t);
        RtSystem_Filename_AdjustDelimiters(
            reinterpret_cast<Filename*>(&self->sourceDirectory));
        RtString_ReleaseInline(&t);
    }

    // --- 0x1800ac119-0x1800ac186: dataDirectory ----------------------------
    {
        String t{nullptr};
        RtSystem_String_Format(&t, "%s%s%s%s\\", root.data, sub.data,
                               platform.data, build.data);
        AssignInline(&self->dataDirectory, &t);
        RtSystem_Filename_AdjustDelimiters(
            reinterpret_cast<Filename*>(&self->dataDirectory));
        RtString_ReleaseInline(&t);
    }

    // --- 0x1800ac1b0-0x1800ac1f1: packDirectory = dataDirectory ------------
    AssignInline(&self->packDirectory, &self->dataDirectory);
    RtSystem_Filename_AdjustDelimiters(
        reinterpret_cast<Filename*>(&self->packDirectory));

    // --- 0x1800ac1f1-0x1800ac5f0: the three command-line overrides ---------
    // `sub.data` is captured once by retail (RBX) and reused by all three.
    ApplyPathOverride(cfg, "pathres", &self->sourceDirectory, root.data, sub.data);
    ApplyPathOverride(cfg, "pathdat", &self->dataDirectory,   root.data, sub.data);
    ApplyPathOverride(cfg, "pathpkg", &self->packDirectory,   root.data, sub.data);

    // --- 0x1800ac5f0-0x1800ac6b4: the epilogue, in retail's order ----------
    RtString_ReleaseInline(&build);
    RtString_ReleaseInline(&platform);
    RtString_ReleaseInline(&sub);
    RtString_ReleaseInline(&cmdPath);
    RtString_ReleaseInline(reinterpret_cast<String*>(&root));

    // ---------------------------------------------------------------------
    // ORACLE
    // ---------------------------------------------------------------------
    // The shipped body is replayed onto a SCRATCH FileSystem carrying the same
    // two gate bytes, never onto `self`: it would otherwise overwrite the three
    // fields we just built and, on the cmd.txt arm, call SetCommandLine a
    // second time. Retail reads only +0x18/+0x19 out of `this` and writes only
    // +0x00/+0x08/+0x10, so a scratch object is a complete stand-in.
    //
    // THE PART THAT IS EASY TO GET WRONG, AND WAS. Retail's body dereferences
    // `[0x1801fc6f8]` — the DLL's OWN copy of Configuration::_singleton —
    // directly. That symbol IS minted (rt_data_mint_tables.inc:1561), so our
    // ported constructor writes OUR storage and the DLL's copy stays NULL
    // forever. A replay guarded on "is our slot populated" would therefore
    // always say yes and always fault INSIDE THE ORACLE, reading as a port
    // defect; a replay guarded on "is the DLL's copy populated" would be safe
    // but would NEVER FIRE, which is the same as having no oracle while
    // looking like having one (reference_recheck_must_match_its_own_proof.md).
    //
    // So the guard reads the DLL's slot RAW, and then LENDS it our pointer for
    // the duration of the replay and puts it back. That is a write into the
    // DLL's .data and it is deliberate and bounded: retail's SetupDirectories
    // only ever READS that pointer (a map find and two enum dwords), this
    // function runs exactly once per process on the single-threaded boot path
    // at Engine::Startup step 3, and the slot is restored on every exit from
    // the block. It is the difference between an oracle and a decoration.
    if (rt_diff_on__) {
        void** dllSlot = static_cast<void**>(
            ResolveExportRaw("?_singleton@Configuration@System@@0PEAV12@EA"));  // RT_RAW_OK
        if (dllSlot != nullptr && cfg != nullptr) {
            void* const saved = *dllSlot;
            *dllSlot = cfg;

            FileSystem scratch;
            std::memset(&scratch, 0, sizeof(scratch));
            scratch.existsInPackfileGate = self->existsInPackfileGate;
            scratch.platformExistsGate   = self->platformExistsGate;
            RtOriginal<SetupDirectoriesFn>(
                "?SetupDirectories@FileSystem@IO@@QEAAXXZ")(&scratch);

            *dllSlot = saved;

            const bool equal =
                SameText(self->sourceDirectory.data, scratch.sourceDirectory.data) &&
                SameText(self->dataDirectory.data,   scratch.dataDirectory.data) &&
                SameText(self->packDirectory.data,   scratch.packDirectory.data);
            char a[768], b[768];
            std::snprintf(a, sizeof(a), "src=%s | dat=%s | pkg=%s",
                          self->sourceDirectory.data ? self->sourceDirectory.data : "(null)",
                          self->dataDirectory.data   ? self->dataDirectory.data   : "(null)",
                          self->packDirectory.data   ? self->packDirectory.data   : "(null)");
            std::snprintf(b, sizeof(b), "src=%s | dat=%s | pkg=%s",
                          scratch.sourceDirectory.data ? scratch.sourceDirectory.data : "(null)",
                          scratch.dataDirectory.data   ? scratch.dataDirectory.data   : "(null)",
                          scratch.packDirectory.data   ? scratch.packDirectory.data   : "(null)");
            RT_DIFF_SHADOW(equal, a, b);
            RtString_ReleaseInline(&scratch.packDirectory);
            RtString_ReleaseInline(&scratch.dataDirectory);
            RtString_ReleaseInline(&scratch.sourceDirectory);
        } else {
            // No DLL at all (cutover), or no Configuration to lend. Say what we
            // DID compute, and check the one post-condition that holds
            // unconditionally: all three fields are non-null and end in a
            // delimiter.
            const char* s = self->sourceDirectory.data;
            const char* d = self->dataDirectory.data;
            const char* p = self->packDirectory.data;
            auto endsSep = [](const char* x) {
                if (x == nullptr || *x == '\0') return false;
                const char c = x[std::strlen(x) - 1];
                return c == '\\' || c == '/';
            };
            char a[768];
            std::snprintf(a, sizeof(a), "src=%s | dat=%s | pkg=%s",
                          s ? s : "(null)", d ? d : "(null)", p ? p : "(null)");
            RT_DIFF_SHADOW(endsSep(s) && endsSep(d) && endsSep(p), a,
                           "no replay: no DLL slot to lend Configuration to");
        }
        RT_DIFF_WORK(true);
    }
}

}  // namespace RtIO

// ============================================================================
// Registration
// ============================================================================
int RtIoSetupDirectoriesRegister() {
    using namespace RtIO;
    int n = 0;

    RegisterRuntimeExport("?SetupDirectories@FileSystem@IO@@QEAAXXZ",
                          (void*)&RtIo_FileSystem_SetupDirectories); ++n;
    RegisterRuntimeExport("?GetRootDirectory@FileSystem@IO@@QEAA?AVFilename@System@@XZ",
                          (void*)&RtIo_FileSystem_GetRootDirectory); ++n;

    RegisterRuntimeAddr(0x1800abd00, (void*)&RtIo_FileSystem_SetupDirectories);
    RegisterRuntimeAddr(0x1800ad650, (void*)&RtIo_FileSystem_GetRootDirectory);

    // DELIBERATELY ABSENT — the four unreached callees, each named with the
    // branch that keeps it unreached in the file header:
    //   ?ExistsInPackfile@FileSystem@IO@@   0x1800ab180  ord 2322
    //   ?OpenFile@FileSystem@IO@@           0x1800ac880  ord 3309
    //   ?ReadFile@FileSystem@IO@@           0x1800ace00  ord 3460
    //   ?CloseFile@FileSystem@IO@@          0x1800accc0  ord 1726
    //   ?GetCommandLineString@Configuration@System@@  0x1800c5300  ord 2495
    // and the row DIRECTLY BEHIND this one, which this lane does not touch:
    //   ?LoadPackages@FileSystem@IO@@QEAAXXZ  0x1800ab460  ord 3209
    return n;
}
