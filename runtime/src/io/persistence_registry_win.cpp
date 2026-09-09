// ============================================================================
// runtime/src/io/persistence_registry_win.cpp — Wave 9 lane BOOT2
// The registry READ half of Engine::Startup step 4
// ============================================================================
//
//   | Export                                   | Retail      | Ord  | Instrs |
//   |------------------------------------------|-------------|------|--------|
//   | ?ReadSettings@Persistence@IO@@QEAAXXZ    | 0x1800b84b0 | 3462 |  256   |
//   | ??0Registry@IO@@QEAA@PEBD@Z              | 0x1800b9500 |  345 |  102   |
//
// Both names were checked against the shipped Runtime-x64.dll's export
// directory with `pefile` BEFORE a line was written
// (reference_resolveexport_phantom_names.md); neither is phantom, and both
// retail addresses agree with the export table.
//
// `_win` because the whole of both bodies is Win32: RegEnumValueW,
// RegQueryValueExW, RegOpenKeyExW, RegCreateKeyExW, RegCloseKey. There is no
// portable half to split off, so this file has no non-`_win` twin — the same
// shape `filesystem_win.cpp` already has (PLAN.md §4.1 decision 1).
//
// NO NEW IMPORT LIBRARY IS NEEDED, and that was checked rather than assumed:
// `standalone/main.cpp:144` already calls `RegOpenKeyExW`, `:251`
// `RegSetValueExW` and `:166` `RegCloseKey`, and the exe links today with
// `SA_LIBS = -luser32 -ldbghelp -lshell32 -lole32` — no `-ladvapi32`. MinGW's
// default spec supplies advapi32, and the five Reg* entry points this file
// adds come from that same library. (Lane ENG's `-lole32` is the counter-
// example that makes this worth stating: `-fsyntax-only` cannot see a missing
// import library at all.)
//
// This file touches nothing under `runtime/src/io/filesystem*` (lane FS owns
// those) and nothing under `standalone/rt_data_mint_tables.inc` or
// `tools/rt_mint_gen.py` (the integrator owns the mint).
//
// ---------------------------------------------------------------------------
// WHY THIS IS NOT THE HOLDBACK WAVE 4 LANE C WROTE
// ---------------------------------------------------------------------------
// `docs/runtime_port/WAVE4_LANE_C.md` §5 held BOTH `ReadSettings` and
// `WriteSettings` back, and its first and load-bearing reason was:
//
//     "No safe oracle exists. Both mutate the real HKCU\...\Banished registry
//      key."
//
// **That sentence is true of `WriteSettings` and false of `ReadSettings`,** and
// the wave doc reached its verdict on the pair rather than on each function.
// `ReadSettings`'s only ADVAPI32 calls are `RegEnumValueW` (0x18014b000, via
// the IAT slot at 0x1800b854a) and `RegQueryValueExW` (0x18014b010, at
// 0x1800b85ac) — both pure reads. The only mutation it performs anywhere is to
// the caller's in-memory `Set<Pair<String,String>>`. `WriteSettings` is the one
// that calls `RegSetValueExW` (0x18014b018) and `RegDeleteKeyW` (0x18014b008);
// it is NOT ported here and the reasons are recorded in the lane report.
//
// So `ReadSettings` gets a real differential oracle and it cannot touch the
// player's registry: see ORACLE below.
//
// ---------------------------------------------------------------------------
// ?ReadSettings@Persistence@IO@@QEAAXXZ — the body
// ---------------------------------------------------------------------------
//   0x1800b84db  Map_Clear(this)                  ; 0x18000f7d0, unnamed DLL
//                                                   internal: recursively frees
//                                                   the tree, then writes
//                                                   this->root = 0 and the
//                                                   DWORD this->count = 0
//   0x1800b84e7  Registry reg(&reg, nullptr)      ; 0x1800b9500, ord 345
//   loop @ 0x1800b8510
//     0x1800b8510  cchName = 0x400                ; in WCHARs
//     0x1800b851a  index = reg._enumIndex++
//     0x1800b854a  if (RegEnumValueW(reg._key, index, nameBuf, &cchName,
//                                    0,0,0,0) != 0) break
//     0x1800b8558  if (cchName > 0x400) break
//     0x1800b856d  if (reg._key) {                ; see DEAD GUARD below
//     0x1800b85ac    RegQueryValueExW(reg._key, nameBuf, 0, &type,
//                                     valueBuf, &cbData)   ; result IGNORED
//                  }
//     0x1800b85c4  name  = utf8(nameBuf)          ; WideString::Set + AsUTF8
//     0x1800b86ba  value = utf8(valueBuf)         ; + String::Set
//     0x1800b87ad  Map_Insert(this, &node, &name, &value)   ; 0x18002a090
//     0x1800b87c0  if (node->value.data != value.data) {    ; self-assign guard
//                      release node->value; node->value = value; AddRef
//                  }
//     goto loop
//   done @ 0x1800b8832
//     reg._enumIndex = 0
//     release value; release name
//     if (reg._key) RegCloseKey(reg._key); reg._key = 0
//     release reg._path                          ; ~Registry, INLINED
//
// FOUR THINGS A TIDY REWRITE WOULD GET WRONG
//
//  1. **`Map_Insert` does not overwrite an existing entry's value.** That is
//     the only reason the four instructions at 0x1800b87b2-0x1800b87c3 exist:
//     retail reads the value back out of the node it was just handed and, if it
//     is not already the String it passed in, releases the old one and adopts
//     the new one by hand. Drop that and a settings key that already existed
//     keeps its stale value forever. It is written out below rather than
//     delegated.
//
//  2. **The `!=` in that same test is retail's self-assignment guard**, not an
//     optimisation. On the fresh-insert path `Map_Insert` has already stored
//     the same `char*`, so taking the release-then-adopt path would drop the
//     refcount to zero, free the buffer, and then AddRef freed memory. Same
//     trap `persistence_startup.cpp` documents for `String::operator=`.
//
//  3. **The two buffers are adjacent and their units differ.** `nameBuf` is at
//     RBP-0x40 and `cchName` is 0x400 **WCHARs** (0x800 bytes); `valueBuf`
//     starts at RBP+0x7c0 — exactly RBP-0x40+0x800, i.e. immediately after it —
//     and `cbData` is 0x400 **BYTES** (0x200 WCHARs). Sizing `valueBuf` as
//     `wchar_t[0x400]` would be twice retail's capacity and would let a long
//     registry value through that retail truncates.
//
//  4. **The `if (reg._key)` at 0x1800b856d is dead code, and knowing that is
//     what makes the uninitialised `valueBuf` defensible.** `RegQueryValueExW`'s
//     return value is ignored (0x1800b85b2 is the next instruction), so on a
//     failed query `valueBuf` is converted while still holding whatever was on
//     the stack. But the guard's false arm is unreachable: if `reg._key` were
//     null, `RegEnumValueW(NULL, …)` at the top of the same iteration would
//     have returned ERROR_INVALID_HANDLE and the loop would already have
//     broken. So the only path to an unwritten `valueBuf` is a
//     `RegQueryValueExW` that fails on a handle a `RegEnumValueW` just
//     succeeded on — the value being deleted by another process between the two
//     calls. The buffers are therefore left uninitialised, exactly as retail
//     leaves them, rather than zeroed: zeroing would be a silent behaviour
//     change on the one path where the two builds could ever differ.
//
// ---------------------------------------------------------------------------
// ??0Registry@IO@@QEAA@PEBD@Z — the body
// ---------------------------------------------------------------------------
//   _path = String::Format("Software\\%s\\%s", "Shining Rock Software LLC",
//                          "Banished")                       ; 0x1800b954b
//   if (subKey && *subKey) _path = (_path + "\\") + subKey    ; 0x1800b95f3..
//   if (RegOpenKeyExW(HKCU, WideString(_path), 0, KEY_ALL_ACCESS, &_key) != 0)
//       if (RegCreateKeyExW(HKCU, …, &_key, &disposition) != 0)
//           _key = 0                                          ; 0x1800b971c
//
// The ctor CREATES the key when it is absent. That is retail's behaviour and it
// is the one registry mutation on this whole boot path — it is also what the
// harness in `standalone/main.cpp` already relies on existing. It is idempotent,
// which is what lets the oracle below run the shipped body a second time.
//
// ---------------------------------------------------------------------------
// ORACLE — a real differential one, and it cannot write to HKCU
// ---------------------------------------------------------------------------
// `ReadSettings` writes ONLY into the object handed to it. So under
// `BANISHED_RT_DIFF` this file runs the SHIPPED body (`RT_ORIGINAL_ONCE`, which
// deliberately skips the port registry) against a zeroed SCRATCH
// `{ root, count }` pair on our own stack, digests the resulting tree, and
// compares that digest with the digest of the tree our body just built in the
// real object. Neither run writes a registry value; both only enumerate and
// query. The scratch tree is freed with the same `Map_Clear` retail uses.
//
// The digest is FNV-1a over the ENTIRE in-order walk — the count, then every
// node's key bytes and value bytes with their separators — so it folds tree
// SHAPE and CONTENT together. Both trees are built by the same `Map_Insert`
// from the same enumeration order, so a shape difference is a real divergence,
// not a tolerated one.
//
// POPULATION, STATED HONESTLY. A digest over a map with two entries in it
// proves almost nothing, and this lane cannot tell how many entries the
// integrator's `HKCU\Software\Shining Rock Software LLC\Banished` holds because
// **this lane ran nothing at all**. So the site also prints one
// `[BOOT2 ReadSettings]` line naming the entry count and the first key, under
// the diff switch only. Read that line before quoting `divergences=0`: if the
// count is small, the number is weak evidence and should be reported as such
// (reference_probe_needs_an_effectiveness_control.md).
//
// `RT_DIFF_WORK(count != 0)` is the effectiveness control: a run whose
// `worked=` column is 0 read an empty or missing registry key and the oracle
// did no work at all, whatever `divergences=` says.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../../types/io/Registry.h"
#include "../../types/system/String.h"
#include "../system/string.h"   // RtString_ReleaseInline, RtSystem::String

using RtIo::Persistence;
using RtIo::Registry;
using RtIo::SettingsMapNode;
using RtSystem::String;
using RtSystem::StringRefCount;

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — same reasoning persistence.cpp gives (that header drags in <windows.h> and
// the runtime tree must stay platform-neutral outside a `_win` file). This one
// IS a `_win` file, but the declaration must stay byte-identical to resolve.h's
// so the linker keeps checking the two against each other.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// The RAW address door -- deliberately skipping the port registry, so the
// WriteSettings oracle below reaches the SHIPPED body and not our own.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// Retail's stack layout, as constants rather than as magic numbers
// ---------------------------------------------------------------------------
// See banner point 3. `kNameChars` is the WCHAR capacity retail passes to
// RegEnumValueW as `lpcchValueName`; `kValueBytes` is the BYTE capacity it
// passes to RegQueryValueExW as `lpcbData`. They are not the same unit and the
// buffers are not the same size.
constexpr DWORD kNameChars  = 0x400;   // 0x1800b8510 / 0x1800b857c
constexpr DWORD kValueBytes = 0x400;   // 0x1800b857c (the SECOND write)
constexpr size_t kValueChars = kValueBytes / sizeof(wchar_t);

// KEY_ALL_ACCESS. Retail spells it 0xF003F literally at 0x1800b96ba and again
// at 0x1800b96f6; the named constant expands to the same value.
constexpr REGSAM kSam = KEY_ALL_ACCESS;
static_assert(kSam == 0xF003F, "retail passes 0xF003F as samDesired");

// ---------------------------------------------------------------------------
// Dependencies
// ---------------------------------------------------------------------------
// Every one of these goes through the PORT-AWARE door (RT_EXPORT_ONCE by name,
// or ResolveAddr for the two unnamed internals), NOT through RT_ORIGINAL_ONCE.
// This wave's whole point is that the DLL will not be there, so a dependency
// that insists on the shipped body is a null call at cutover
// (reference_converting_a_door_is_not_satisfying_it.md).
//
// ON reference_resolveaddr_consults_port_registry_first.md: `ResolveAddr(
// Module::Runtime, …)` checks OUR registry before the DLL, so an address
// another lane has registered silently becomes ours. Both addresses below were
// grepped across the tree before use:
//   0x18002a090  Map_Insert — NOT registered by any lane. Reaches the DLL.
//   0x18000f7d0  Map_Clear  — PORTED 2026-09-08 by Wave 9 lane SAVE2
//                (runtime/src/collection/set_clear.cpp), registered BY ADDRESS,
//                so ResolveAddr now hands back OUR body. This line used to read
//                "NOT registered by any lane. Reaches the DLL." Nothing about
//                this call site changes; what changes is which body answers.
// The near miss worth naming: 0x18002ace0 (Map_Find, Map_Insert's sibling) IS
// registered, by `runtime/src/system/configuration.cpp:310`. This file does not
// use it. A future lane that adds a Map_Find call here must know that.

typedef void  (*MapClearFn)(void* self);
typedef void* (*MapInsertFn)(void* map, void** outNode, const String* key, const String* value);
typedef Registry* (*RegistryCtorFn)(Registry* self, const char* subKey);
typedef void  (*StringSetFn)(String* self, const char* src);
typedef void  (*WideSetFn)(String* self, const wchar_t* src);
typedef void* (*WideAsUtf8Fn)(const String* self, String* ret);
typedef void  (*ReadSettingsFn)(Persistence* self);

// System::WideString is one pointer with String's exact allocation header —
// every release of one in ReadSettings is `DEC [p-4] / SUB p,8 / MemFree`, byte
// for byte the String idiom (String.h). `filesystem_win.cpp` already models it
// this way and this file follows rather than inventing a second spelling.
using WideStringHandle = String;

// NEITHER of these is null-checked, deliberately. With the DLL present
// `ResolveAddr` hands back a real address and the check would be dead; with the
// DLL absent it hands back 0 and the check would turn "no port for Map_Clear /
// Map_Insert yet" into SILENCE — ReadSettings would return having loaded no
// settings at all, and the first symptom would be a wrong default somewhere
// else entirely. Left unguarded so it dies as EXEC-of-0 with the VEH handler
// naming this frame, which is the loud failure WAVE9.md asks for and the same
// reasoning rt_port.h gives for RT_DATA_SLOT. (Contrast the guard in
// persistence_startup.cpp: that one is over a DATA SLOT whose absence is a real
// standalone-vs-retail difference, not over a call whose absence is a defect.)
void MapClear(void* self) {
    static MapClearFn fn = reinterpret_cast<MapClearFn>(
        ResolveAddr(Module::Runtime, 0x18000f7d0));
    fn(self);
}

void MapInsert(void* map, void** outNode, const String* key, const String* value) {
    static MapInsertFn fn = reinterpret_cast<MapInsertFn>(
        ResolveAddr(Module::Runtime, 0x18002a090));
    fn(map, outNode, key, value);
}

// Convert one NUL-terminated UTF-16 buffer to a System::String, exactly the
// six-call sequence retail inlines twice per loop iteration (0x1800b85b2 ..
// 0x1800b8649 for the name, 0x1800b86a5 .. 0x1800b879b for the value).
void AssignFromWide(String* dst, const wchar_t* src) {
    WideStringHandle w;
    w.data = nullptr;                                   // MOV [RBP+0x1010],R12
    RT_EXPORT_ONCE(WideSetFn, "?Set@WideString@System@@QEAAXPEB_W@Z")(&w, src);

    // AsUTF8 returns System::String BY VALUE, so the hidden return slot is the
    // SECOND argument and it is CONSTRUCTED, not assigned — retail does not
    // pre-zero the temporary. It is zeroed here anyway so that a null return
    // from the resolver cannot leave an indeterminate pointer to release; that
    // is a difference from retail's stack, not from retail's behaviour.
    String utf8;
    utf8.data = nullptr;
    RT_EXPORT_ONCE(WideAsUtf8Fn,
                   "?AsUTF8@WideString@System@@QEBA?AVString@2@XZ")(&w, &utf8);

    // MOV RDX,[RAX] — retail reads the char* back out of the returned String
    // and hands THAT to String::Set, which copies the bytes. It does not adopt
    // the temporary's buffer.
    RT_EXPORT_ONCE(StringSetFn, "?Set@String@System@@QEAAXPEBD@Z")(dst, utf8.data);

    RtSystem::RtString_ReleaseInline(&utf8);
    RtSystem::RtString_ReleaseInline(&w);
}

// ---------------------------------------------------------------------------
// The oracle's tree walk
// ---------------------------------------------------------------------------
// Node layout: { String key @+0x00; String value @+0x08; parent @+0x10;
// left @+0x18; right @+0x20 }. The two this walk uses — left and right — are
// pinned by `Map_Clear` itself (0x18000f7f4 recurses on [node+0x18], 0x18000f7fd
// on [node+0x20]); `parent` is WAVE3_LANE_A.md's general formula and is NOT
// read here. `runtime/types/io/Persistence.h`'s `SettingsMapNode` deliberately
// models only the first 0x10 bytes, so the two child pointers are reached by
// byte offset rather than by extending that shared type from a decompile.
constexpr size_t kNodeLeft  = 0x18;
constexpr size_t kNodeRight = 0x20;

void* NodeChild(const void* node, size_t off) {
    return *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(node) + off);
}

void FnvBytes(uint64_t& h, const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
}

void FnvCStr(uint64_t& h, const char* s) {
    if (s == nullptr) { FnvBytes(h, "\x01<null>", 7); return; }
    size_t n = 0;
    while (s[n] != '\0') ++n;
    FnvBytes(h, s, n);
    FnvBytes(h, "\x00", 1);
}

// In-order walk. `count` counts nodes so the caller can print an effectiveness
// figure; `depthLeft` is a hard recursion bound so a corrupted or cyclic tree
// makes the oracle report a difference instead of blowing the stack.
void DigestTree(const void* node, uint64_t& h, uint32_t& count,
                const char*& firstKey, int depthLeft) {
    if (node == nullptr || depthLeft <= 0) return;
    DigestTree(NodeChild(node, kNodeLeft), h, count, firstKey, depthLeft - 1);

    const SettingsMapNode* n = static_cast<const SettingsMapNode*>(node);
    if (firstKey == nullptr) firstKey = n->key.data;
    FnvCStr(h, n->key.data);
    FnvCStr(h, n->value.data);
    ++count;

    DigestTree(NodeChild(node, kNodeRight), h, count, firstKey, depthLeft - 1);
}

uint64_t DigestMap(const void* root, uint32_t* countOut, const char** firstKeyOut) {
    uint64_t h = 14695981039346656037ull;
    uint32_t count = 0;
    const char* firstKey = nullptr;
    DigestTree(root, h, count, firstKey, 128);
    FnvBytes(h, &count, sizeof(count));
    if (countOut)    *countOut = count;
    if (firstKeyOut) *firstKeyOut = firstKey;
    return h;
}

}  // namespace

// ============================================================================
// ??0Registry@IO@@QEAA@PEBD@Z                              retail 0x1800b9500
// ============================================================================
extern "C" Registry* RtIo_Registry_Ctor(Registry* self, const char* subKey) {
    RT_DIFF_BEGIN("IO::Registry::Registry(const char*)");

    // 0x1800b9520-0x1800b952a. The qword store at +0x10 clears BOTH dword
    // cursors; see runtime/types/io/Registry.h.
    self->path.data      = nullptr;
    self->key            = nullptr;
    self->enumIndex      = 0;
    self->enumGroupIndex = 0;

    // 0x1800b954b: String::Format(&tmp, "Software\\%s\\%s", company, product),
    // then String::operator= inlined at 0x1800b9553. Format is real varargs, so
    // RT_EXPORT_ONCE's non-variadic `Fn` will not hold it — the same exception
    // persistence.cpp makes. It still goes through RtResolveCached rather than a
    // bare `static FormatFn = ResolveExport(...)`, so it keeps that macro's
    // never-cache-a-null contract: a resolve that misses is named and retried
    // instead of being frozen by whichever caller happened to run first
    // (rt_port.h, "Resolve-and-cache that NEVER caches a null"). Unguarded, for
    // the reason given above MapClear.
    using FormatFn = String* (*)(String* ret, const char* fmt, ...);
    static void* formatSlot = nullptr;
    FormatFn format = reinterpret_cast<FormatFn>(
        RtResolveCached("?Format@String@System@@SA?AV12@PEBDZZ", &formatSlot));
    String tmp;
    tmp.data = nullptr;
    format(&tmp, RtIo::kRegistryPathFormat,
           RtIo::kRegistryCompanyName, RtIo::kRegistryProductName);
    // The leading equality test is retail's self-assignment guard (see
    // persistence_startup.cpp's banner for why it must not be tidied away).
    if (self->path.data != tmp.data) {
        RtSystem::RtString_ReleaseInline(&self->path);
        self->path.data = tmp.data;
        if (tmp.data) ++StringRefCount(tmp.data);
    }
    RtSystem::RtString_ReleaseInline(&tmp);

    // 0x1800b95c0: `subKey != nullptr && subKey[0] != '\0'`. Retail then walks
    // subKey to its NUL and tests `length <= 0` (0x1800b95d3-0x1800b95ed) —
    // provably redundant, since the byte test above already established
    // length >= 1. The walk is not reproduced; it computes a value retail then
    // uses for nothing but that dead comparison.
    if (subKey != nullptr && subKey[0] != '\0') {
        using ConcatCStrFn = String* (*)(String* ret, const String* lhs, const char* rhs);
        using AssignFn     = String* (*)(String* self, const String* rhs);
        ConcatCStrFn cat = RT_EXPORT_ONCE(ConcatCStrFn,
                                          "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z");
        String withSep;  withSep.data = nullptr;
        String full;     full.data    = nullptr;
        cat(&withSep, &self->path, RtIo::kRegistryPathSeparator);   // 0x1800b9605
        cat(&full, &withSep, subKey);                               // 0x1800b9619
        RT_EXPORT_ONCE(AssignFn, "??4String@System@@QEAAAEAV01@AEBV01@@Z")
            (&self->path, &full);                                   // 0x1800b9625
        RtSystem::RtString_ReleaseInline(&full);                    // 0x1800b962b
        RtSystem::RtString_ReleaseInline(&withSep);                 // 0x1800b9664
    }

    // 0x1800b969d onward.
    DWORD disposition = 0;

    using WideCtorFromStringFn = WideStringHandle* (*)(WideStringHandle* self, const String* src);
    WideStringHandle wpath;
    wpath.data = nullptr;
    RT_EXPORT_ONCE(WideCtorFromStringFn, "??0WideString@System@@QEAA@AEBVString@1@@Z")
        (&wpath, &self->path);

    HKEY* phKey = reinterpret_cast<HKEY*>(&self->key);
    const wchar_t* w = reinterpret_cast<const wchar_t*>(wpath.data);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, w, 0, kSam, phKey) != ERROR_SUCCESS) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, w, 0, nullptr, 0, kSam,
                            nullptr, phKey, &disposition) != ERROR_SUCCESS) {
            self->key = nullptr;    // 0x1800b971c
        }
    }

    RtSystem::RtString_ReleaseInline(&wpath);

    if (rt_diff_on__) {
        // No second run: constructing a Registry twice would call
        // RegCreateKeyExW twice, and while that is idempotent it is still a
        // registry mutation this lane will not perform for measurement's sake.
        // The shadow is the post-condition the body is FOR: a path was built
        // and a handle was obtained under it.
        const bool ok = self->path.data != nullptr && self->key != nullptr;
        char a[192], b[96];
        std::snprintf(a, sizeof(a), "path=\"%s\" key=%p disp=%lu",
                      self->path.data ? self->path.data : "(null)",
                      self->key, static_cast<unsigned long>(disposition));
        std::snprintf(b, sizeof(b), "path built AND key open [ok=%d]", ok ? 1 : 0);
        RT_DIFF_SHADOW(ok, a, b);
        RT_DIFF_WORK(true);
    }

    return self;   // 0x1800b9747: MOV RAX,RDI
}

// ============================================================================
// ?ReadSettings@Persistence@IO@@QEAAXXZ                    retail 0x1800b84b0
// ============================================================================
extern "C" void RtIo_Persistence_ReadSettings(Persistence* self) {
    RT_DIFF_BEGIN("IO::Persistence::ReadSettings");

    MapClear(self);                                            // 0x1800b84db

    Registry reg;
    RT_EXPORT_ONCE(RegistryCtorFn, "??0Registry@IO@@QEAA@PEBD@Z")(&reg, nullptr);

    String name;   name.data  = nullptr;                       // 0x1800b84f4
    String value;  value.data = nullptr;                       // 0x1800b84fc

    // Deliberately uninitialised — see banner point 4.
    wchar_t nameBuf[kNameChars];
    wchar_t valueBuf[kValueChars];

    for (;;) {
        DWORD cchName = kNameChars;                            // 0x1800b8510
        const DWORD index = reg.enumIndex++;                   // 0x1800b851a

        if (RegEnumValueW(static_cast<HKEY>(reg.key), index, nameBuf, &cchName,
                          nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;                                             // 0x1800b8552
        if (cchName > kNameChars)
            break;                                             // 0x1800b8562

        if (reg.key != nullptr) {                              // 0x1800b8570, dead
            DWORD type    = REG_SZ;                            // 0x1800b8572, = 1
            DWORD cbData  = kValueBytes;                       // 0x1800b857c
            // Return value IGNORED, exactly as at 0x1800b85b2.
            RegQueryValueExW(static_cast<HKEY>(reg.key), nameBuf, nullptr, &type,
                             reinterpret_cast<BYTE*>(valueBuf), &cbData);
        }

        AssignFromWide(&name, nameBuf);                        // 0x1800b85b2..
        AssignFromWide(&value, valueBuf);                      // 0x1800b86a5..

        void* node = nullptr;
        MapInsert(self, &node, &name, &value);                 // 0x1800b87ad

        // 0x1800b87b2-0x1800b882d. Map_Insert leaves an ALREADY-PRESENT key's
        // value untouched, so retail re-seats it by hand. The `!=` is the
        // self-assignment guard; see banner points 1 and 2.
        if (node != nullptr) {
            SettingsMapNode* n = static_cast<SettingsMapNode*>(node);
            if (n->value.data != value.data) {
                RtSystem::RtString_ReleaseInline(&n->value);
                n->value.data = value.data;
                if (value.data) ++StringRefCount(value.data);
            }
        }
    }

    reg.enumIndex = 0;                                         // 0x1800b8832
    RtSystem::RtString_ReleaseInline(&value);                  // 0x1800b8837
    RtSystem::RtString_ReleaseInline(&name);                   // 0x1800b8859

    // ~Registry, inlined at 0x1800b887d.
    if (reg.key != nullptr) RegCloseKey(static_cast<HKEY>(reg.key));
    reg.key = nullptr;
    RtSystem::RtString_ReleaseInline(&reg.path);               // 0x1800b8892

    if (rt_diff_on__) {
        uint32_t    ours     = 0;
        const char* firstKey = nullptr;
        const uint64_t oursDigest = DigestMap(self->settingsMapRoot, &ours, &firstKey);

        // The effectiveness control, printed BEFORE the comparison so it is
        // legible even when the comparison agrees. See the ORACLE banner: a
        // digest over a two-entry map is not a verification.
        printf("[BOOT2 ReadSettings] entries=%u count_field=%d first=\"%s\"\n",
                    ours, self->settingsMapCount,
                    firstKey ? firstKey : "(none)");

        // Run the SHIPPED body into a zeroed SCRATCH container and digest that.
        // Written out HERE, inside the guard, rather than in a helper: with the
        // RT_ORIGINAL_ONCE in a helper, `tools/port_bypass_audit.py` reported it
        // as BYPASS=1 and it was RIGHT to — the raw door sat in a function whose
        // own body carried no guard, so nothing but a human reading two call
        // sites could tell that it never runs on a live path. Keeping the oracle
        // where its guard is visible is what makes that audit's denominator mean
        // something (reference_documenting_a_check_can_delete_it.md).
        //
        // The scratch is a whole zeroed Persistence, not the two fields the
        // shipped body is known to touch (root at +0x00, count at +0x08), so
        // that any field this lane failed to notice lands in our own storage
        // rather than past the end of a two-field struct.
        uint64_t theirsDigest = 0;
        ReadSettingsFn theirs =
            RT_ORIGINAL_ONCE(ReadSettingsFn, "?ReadSettings@Persistence@IO@@QEAAXXZ");
        if (theirs != nullptr) {
            Persistence scratch;
            std::memset(&scratch, 0, sizeof(scratch));
            theirs(&scratch);
            theirsDigest = DigestMap(scratch.settingsMapRoot, nullptr, nullptr);
            // Retail's own destructor, so the strings the shipped body just
            // allocated do not leak once per diffed call.
            MapClear(&scratch);
        }

        RT_DIFF_SCALAR(oursDigest, theirsDigest);
        RT_DIFF_WORK(ours != 0);
    }
}

// ============================================================================
// ?WriteSettings@Persistence@IO@@QEBAXXZ                   retail 0x1800b88e0
//                                                          ordinal 4561
// ----------------------------------------------------------------------------
// WAVE 9 LANE SAVE2. Wave 4 lane C held this back with `ReadSettings` on one
// sentence — "No safe oracle exists. Both mutate the real HKCU\...\Banished
// registry key." Lane BOOT2 showed that sentence was true of THIS function and
// false of `ReadSettings`, and ported the latter (see the banner above). Lane
// SYS2 then decoded this body, designed a scratch-key oracle for it, and ran
// out of budget. This is the execution of that hand-off, with one change to the
// design that makes it strictly better; see ORACLE below.
//
// IT IS A LIVE CUTOVER DOOR: `runtime/src/engine/engine_lifecycle.cpp:922`
// (our ported `Engine::Shutdown`, phase 1 step "save settings") calls it by
// name through `RT_EXPORT_ONCE`, so with no DLL it is a call through NULL on
// every clean exit.
//
// ----------------------------------------------------------------------------
// A FINDING IN RETAIL, and it changes what the second half of this function IS
// ----------------------------------------------------------------------------
// The IAT slot the delete pass calls, 0x18014b008, is **`RegDeleteKeyW`, not
// `RegDeleteValueW`** — read out of Runtime-x64.dll's import directory with
// pefile, not inferred from the name of the loop. `ADVAPI32.dll RegDeleteKeyW`.
//
// So phase 2 collects the names of registry VALUES that are present under the
// key but absent from the settings map, and then asks Windows to delete a
// SUBKEY of each of those names. There is no such subkey, so every one of those
// calls returns ERROR_FILE_NOT_FOUND and **nothing is ever deleted**. Retail's
// "prune stale settings" pass has never pruned anything.
//
// That is transcribed exactly as it is. It is not a bug we may fix: the whole
// of phase 2 is observable only through those calls, and "fixing" it to
// `RegDeleteValueW` would start deleting real registry values that the shipped
// game leaves alone — a behaviour change dressed as a correction, on the
// player's own registry. Named here so the next reader does not silently
// "correct" it either.
//
// ----------------------------------------------------------------------------
// THE BODY
// ----------------------------------------------------------------------------
// PHASE 1 — write every entry of the settings map, 0x1800b8908..0x1800b8b55
//
//   Registry reg(nullptr);                                   ; 0x1800b8912
//   node = leftmost(this->_settings.root)                    ; 0x1800b8918-0x1800b892c
//   while (node) {
//       WideString wValue(node->value);                      ; 0x1800b894b, node+0x08
//       WideString wName (node->key);                        ; 0x1800b895d, node+0x00
//       if (reg._key)
//           RegSetValueExW(reg._key, wName, 0, REG_SZ,
//                          wValue, 2 + 2*wcslen(wValue));    ; 0x1800b89a7
//       release wName;  release wValue;                      ; LIFO, 818 then 820
//       node = inorder_successor(node);                      ; 0x1800b8a80..0x1800b8acb
//   }
//   if (reg._key) RegCloseKey(reg._key);  reg._key = 0;      ; 0x1800b8ae5
//   release reg._path;                                       ; ~Registry, inlined
//
//   *** THE NAME IS THE KEY AND THE DATA IS THE VALUE, and the argument order
//   is the one place a transcription of this can go wrong invisibly.*** Retail
//   loads `RDX = wide(node+0x00)` as `lpValueName` and passes
//   `[RSP+0x20] = wide(node+0x08)` as `lpData`, and it computes `cbData` by
//   walking the DATA buffer (`2 + 2*len`, the NUL included). Swap them and the
//   settings still round-trip through `ReadSettings` — mirrored — which is
//   exactly the kind of defect that survives an end-to-end test.
//
//   The two conversions happen whether or not `reg._key` is open; only the
//   RegSetValueExW is guarded. Reproduced, because the conversions allocate and
//   a "tidier" early-continue would change the allocation trace.
//
// PHASE 2 — collect the value names NOT in the map, then RegDeleteKeyW each,
//           0x1800b8b58..0x1800b90d4
//
//   Registry reg(nullptr);                                   ; second one, same
//                                                            ; stack slot
//   Array<WideString> stale;                                 ; R12/[RSP+0x60]
//   for (;;) {
//       cch = 0x400; index = reg._enumIndex++;
//       if (RegEnumValueW(reg._key, index, nameBuf, &cch,
//                         0,0,0,0) != 0) break;              ; 0x1800b8bb8
//       if (cch > 0x400) break;
//       String key = utf8(nameBuf);                          ; Set + AsUTF8
//       found = Map_Find(this, &key);                        ; 0x18002ace0
//       release key
//       if (found) continue;
//       stale.SetSize(stale.size + 1, WideString(nameBuf));  ; INLINED, below
//   }
//   reg._enumIndex = 0;
//   for (i = 0; i < stale.size; ++i)
//       if (reg._key) RegDeleteKeyW(reg._key, stale[i]);     ; 0x1800b8f98
//   if (reg._key) RegCloseKey(reg._key);  reg._key = 0;
//   release reg._path;
//   for (i = 0; i < stale.size; ++i) release stale[i];
//   MemFree(stale.data);  stale = {}                         ; 0x1800b90d4
//
//   THE GROWTH RULE IS NOT `capacity * 2`, and it is written out rather than
//   delegated because no shared `SetSizeFill` in this tree has this exact
//   shape inlined at this address (0x1800b8d02-0x1800b8dd4):
//
//       newSize = size + 1
//       if (newSize > capacity) {
//           capacity = newSize;
//           if (newSize == oldSize + 1 && oldSize != 0) capacity = newSize * 3;
//           data = Allocate(capacity * 8, 8);
//       }
//
//   i.e. the FIRST append allocates exactly one slot (oldSize == 0 fails the
//   second test), and every later append allocates 3*newSize. `LEA EDX,[RSI +
//   RSI*2]` at 0x1800b8d2f is the ×3, not a ×2. Retail reallocates on EVERY
//   append after the first, because `capacity` is then always exactly
//   `3*newSize` from the previous round... no: 3*newSize > newSize+1 for
//   newSize >= 1, so after the second append the capacity does hold for a
//   while. Either way the rule is transcribed, not reasoned about.
//
// ----------------------------------------------------------------------------
// ORACLE — an EMISSION-SEQUENCE comparison that writes NOTHING, anywhere
// ----------------------------------------------------------------------------
// Lane SYS2's design was two scratch registry keys, one per side. This does
// better and is simpler: **intercept the two mutating ADVAPI32 calls and
// compare what each side WOULD have written.**
//
//   * ours   — the body below routes `RegSetValueExW` and `RegDeleteKeyW`
//              through `EmitSet`/`EmitDelete`, which record into the active
//              recorder when one is armed and call the real API otherwise.
//   * theirs — Runtime-x64.dll reaches both through its own IAT, at
//              0x18014b018 and 0x18014b008 (both confirmed with pefile). The
//              probe swaps those two slots for recording thunks for the
//              duration of the shipped call and puts them back afterwards.
//              That is the `standalone/main.cpp:706-760` MessageBoxW-in-the-IAT
//              pattern, applied to two slots and a few microseconds.
//
// The two mutations are therefore the only calls that do not happen. Everything
// else runs for real on both sides — the same key is opened, the same values
// are enumerated, the same map is searched — so the comparison is over the real
// population rather than a synthetic one, and the player's registry is not
// touched by either side.
//
// WHY REPLAY IS SOUND HERE. `WriteSettings` is `QEBA` — const — and it is one
// of the pure readers the lane-S finding is about: it walks the settings map
// and emits, and mutates nothing it reads. The only state it changes lives in
// the registry, and that is the state the recorder removes. Running it twice
// back to back is therefore a legitimate replay, which is NOT true of most of
// this file's neighbours.
//
// WHAT IT CANNOT SEE. The allocation trace. Both sides allocate WideString
// temporaries and (in phase 2) a growing array; the recorder observes neither,
// so a port that leaked every temporary would compare clean here. That is not a
// gap this oracle can close and it is stated rather than left implied.
//
// EFFECTIVENESS, counted inside the body: `g_wsSetCalls` (entries emitted),
// `g_wsStaleFound` (phase-2 names NOT in the map, i.e. how many times the array
// append ran at all) and `g_wsDeleteCalls`. A run with `stale=0` never entered
// phase 2's interesting half whatever `mismatches=0` says, and the probe says
// so on its own line.
// ============================================================================

namespace {

// ---- dependencies this half adds -------------------------------------------
// Node parent, +0x10. Pinned by this very function: 0x1800b8a9c reads
// [node+0x10] as the parent in the in-order successor, and 0x1800b8aa5 compares
// [parent+0x18] against the node. That is the same layout WAVE3_LANE_A.md
// derived generally and the same one Map_Clear pins for left/right.
constexpr size_t kNodeParent = 0x10;

// 0x18002ace0. PORTED, by runtime/src/system/configuration.cpp:310
// (`RtSystem_ConfigMap_Find`, registered BY ADDRESS), so this must go through
// `ResolveAddr` and not by name -- there is no export name at that RVA at all
// (checked with pefile). Reaching it by any other door would walk past our own
// port into the DLL, which is what tools/port_bypass_audit.py exists to catch.
void* MapFind(const void* map, const String* key) {
    using Fn = void* (*)(const void*, const String*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18002ace0));
    return fn(map, key);
}

// The allocator boundary for phase 2's array. Retail open-codes
// `Lock(alloc+0x640); Allocate/Free(_singleton, ...); Unlock()` at
// 0x1800b8d69 and 0x1800b8e29 -- which is exactly what the two static exports
// below ARE (0x1800c7a50 / 0x1800c7ad0, confirmed by RVA in the export table).
// A composition identity, not a substitution; `runtime/src/collection/array.cpp`
// makes the same argument for the same pair.
void* MemAllocRaw(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    return RT_EXPORT_ONCE(Fn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(bytes, align);
}
void MemFreeRaw(void* p) {
    using Fn = void (*)(void*);
    RT_EXPORT_ONCE(Fn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// ---- the two mutating calls, behind a recorder seam ------------------------
//
// A recorded emission. `name` and `data` are copied out of the wide buffers
// rather than aliased, because the caller frees them immediately after the
// call and a pointer comparison would be reading freed memory on both sides.
struct RegEmission {
    int      kind;              // 0 = RegSetValueExW, 1 = RegDeleteKeyW
    unsigned type;              // dwType, RegSetValueExW only
    unsigned cb;                // cbData, RegSetValueExW only
    wchar_t  name[256];
    wchar_t  data[512];
};

constexpr int kMaxEmissions = 512;

struct RegRecorder {
    int         n = 0;
    int         dropped = 0;    // emissions past the cap; reported, not hidden
    RegEmission e[kMaxEmissions];
};

RegRecorder* g_recorder = nullptr;   // armed only inside the probe

void CopyWide(wchar_t* dst, size_t cap, const wchar_t* src) {
    size_t i = 0;
    if (src) { for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i]; }
    dst[i] = 0;
}

void RecordEmission(int kind, const wchar_t* name, unsigned type,
                    const wchar_t* data, unsigned cb) {
    RegRecorder* r = g_recorder;
    if (!r) return;
    if (r->n >= kMaxEmissions) { ++r->dropped; return; }
    RegEmission& e = r->e[r->n++];
    e.kind = kind; e.type = type; e.cb = cb;
    CopyWide(e.name, 256, name);
    CopyWide(e.data, 512, data);
}

// Effectiveness counters. Unconditional, so they describe the REAL calls too
// and not only the probed one.
long g_wsSetCalls    = 0;
long g_wsStaleFound  = 0;
long g_wsDeleteCalls = 0;

// The negative control's one defect: emit the value's NAME and DATA the wrong
// way round. That is the exact mistake the banner warns is invisible end to end
// (a mirrored settings map still round-trips through ReadSettings), so it is
// the right thing to prove the comparison can see.
bool g_negativeSwap = false;

void EmitSet(HKEY key, const wchar_t* name, DWORD type,
             const wchar_t* data, DWORD cb) {
    ++g_wsSetCalls;
    if (g_recorder) {
        if (g_negativeSwap) RecordEmission(0, data, type, name, cb);
        else                RecordEmission(0, name, type, data, cb);
        return;
    }
    RegSetValueExW(key, name, 0, type,
                   reinterpret_cast<const BYTE*>(data), cb);
}

void EmitDelete(HKEY key, const wchar_t* name) {
    ++g_wsDeleteCalls;
    if (g_recorder) { RecordEmission(1, name, 0, nullptr, 0); return; }
    // RegDeleteKeyW, not RegDeleteValueW — see the banner. It is expected to
    // fail with ERROR_FILE_NOT_FOUND every time, and retail ignores the result.
    RegDeleteKeyW(key, name);
}

// ---- the tree walk phase 1 uses -------------------------------------------
void* Leftmost(void* node) {
    if (!node) return nullptr;
    for (;;) {
        void* l = NodeChild(node, kNodeLeft);
        if (!l) return node;
        node = l;
    }
}

// 0x1800b8a80..0x1800b8acb, transcribed branch for branch.
void* InorderSuccessor(void* node) {
    if (!node) return nullptr;
    void* r = NodeChild(node, kNodeRight);
    if (r) {                                    // 0x1800b8a89
        for (;;) {                              // 0x1800b8a8e
            node = r;
            r = NodeChild(node, kNodeLeft);
            if (!r) return node;                // 0x1800b8a98
        }
    }
    void* p = NodeChild(node, kNodeParent);     // 0x1800b8a9c
    if (!p) return NodeChild(node, kNodeParent);
    if (NodeChild(p, kNodeLeft) == node) return p;          // 0x1800b8aa5
    while (p && NodeChild(p, kNodeRight) == node) {         // 0x1800b8ab5
        node = p;
        p = NodeChild(node, kNodeParent);
    }
    return NodeChild(node, kNodeParent);        // 0x1800b8ac7
}

// ---- phase 2's inlined Array<WideString> -----------------------------------
struct WideArray {
    WideStringHandle* data = nullptr;   // R12  / [RSP+0x60]
    int32_t           size = 0;         // R13D / [RSP+0x68]
    int32_t           capacity = 0;     //        [RSP+0x6c]
};

// 0x1800b8cfb-0x1800b8dd4 plus the two teardown loops that follow it.
// The `fill` is COPIED BY REFCOUNT (`INC dword [RCX-4]`), not by value.
void ArrayAppend(WideArray* a, const WideStringHandle* fill) {
    const int32_t oldSize = a->size;
    const int32_t newSize = oldSize + 1;
    WideStringHandle* oldData = a->data;

    a->size = newSize;
    if (newSize > a->capacity) {                       // 0x1800b8d18
        int32_t cap = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)    // 0x1800b8d24
            cap = newSize * 3;                         // LEA EDX,[RSI+RSI*2]
        a->capacity = cap;
        a->data = static_cast<WideStringHandle*>(
            MemAllocRaw(static_cast<uint64_t>(cap) * 8, 8));
    }

    // 0x1800b8d81: start at oldSize when nothing was reallocated, else at 0.
    int32_t i = (oldData == a->data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        WideStringHandle* dst = &a->data[i];
        if (!dst) break;                                // retail's TEST RDX,RDX
        const WideStringHandle* src = (i < oldSize) ? &oldData[i] : fill;
        dst->data = nullptr;                            // MOV [RDX],RBX
        if (src->data) {
            dst->data = src->data;
            ++StringRefCount(src->data);                // INC dword [RCX-4]
        }
    }

    if (oldData != a->data) {                           // 0x1800b8dd4
        for (int32_t j = 0; j < oldSize; ++j)
            RtSystem::RtString_ReleaseInline(&oldData[j]);
        if (oldData) MemFreeRaw(oldData);
    }
    // The `else` arm at 0x1800b8e88 releases elements [newSize, oldSize) — the
    // SHRINK case. Unreachable from an append (newSize > oldSize always), and
    // written out here as a comment rather than as dead code.
}

void ArrayDestroy(WideArray* a) {                       // 0x1800b9027-0x1800b90dc
    for (int32_t i = 0; i < a->size; ++i)
        RtSystem::RtString_ReleaseInline(&a->data[i]);
    if (a->data) MemFreeRaw(a->data);
    a->data = nullptr;
    a->size = 0;
}

// ---- the transcription itself, separated from the probe --------------------
void WriteSettingsBody(const Persistence* self) {
    RegistryCtorFn regCtor = RT_EXPORT_ONCE(RegistryCtorFn,
                                            "??0Registry@IO@@QEAA@PEBD@Z");
    using WideCtorFromStringFn =
        WideStringHandle* (*)(WideStringHandle* self, const String* src);
    using WideSetFn2 = void (*)(WideStringHandle* self, const wchar_t* src);

    // ---- phase 1 -----------------------------------------------------------
    {
        Registry reg;
        regCtor(&reg, nullptr);                                 // 0x1800b8912

        void* node = Leftmost(self->settingsMapRoot);           // 0x1800b8918
        while (node != nullptr) {
            const SettingsMapNode* n = static_cast<const SettingsMapNode*>(node);

            WideStringHandle wValue; wValue.data = nullptr;
            RT_EXPORT_ONCE(WideCtorFromStringFn,
                           "??0WideString@System@@QEAA@AEBVString@1@@Z")
                (&wValue, &n->value);                           // 0x1800b894b
            WideStringHandle wName;  wName.data = nullptr;
            RT_EXPORT_ONCE(WideCtorFromStringFn,
                           "??0WideString@System@@QEAA@AEBVString@1@@Z")
                (&wName, &n->key);                              // 0x1800b895d

            if (reg.key != nullptr) {                           // 0x1800b896e
                const wchar_t* data =
                    reinterpret_cast<const wchar_t*>(wValue.data);
                DWORD len = 0;
                if (data) { while (data[len]) ++len; }          // 0x1800b8976 loop
                EmitSet(static_cast<HKEY>(reg.key),
                        reinterpret_cast<const wchar_t*>(wName.data),
                        REG_SZ, data,
                        static_cast<DWORD>(2 + 2 * len));       // LEA EAX,[2+R8*2]
            }

            RtSystem::RtString_ReleaseInline(&wName);           // 0x1800b89ae
            RtSystem::RtString_ReleaseInline(&wValue);          // 0x1800b8a18

            node = InorderSuccessor(node);
        }

        if (reg.key != nullptr) RegCloseKey(static_cast<HKEY>(reg.key));
        reg.key = nullptr;
        RtSystem::RtString_ReleaseInline(&reg.path);            // 0x1800b8af2
    }

    // ---- phase 2 -----------------------------------------------------------
    {
        WideArray stale;
        Registry reg;
        regCtor(&reg, nullptr);                                 // 0x1800b8b70

        // Deliberately uninitialised, exactly as retail leaves it: the only
        // path that reads it is one RegEnumValueW has just filled.
        wchar_t nameBuf[kNameChars];

        for (;;) {
            DWORD cch = kNameChars;                             // 0x1800b8b7e
            const DWORD index = reg.enumIndex++;                // 0x1800b8b88

            if (RegEnumValueW(static_cast<HKEY>(reg.key), index, nameBuf, &cch,
                              nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;                                          // 0x1800b8bc0
            if (cch > kNameChars) break;                        // 0x1800b8bd0

            String key; key.data = nullptr;
            AssignFromWide(&key, nameBuf);                      // 0x1800b8be8..

            void* found = MapFind(self, &key);                  // 0x18002ace0
            RtSystem::RtString_ReleaseInline(&key);
            if (found != nullptr) continue;                     // 0x1800b8cd3

            ++g_wsStaleFound;
            WideStringHandle w; w.data = nullptr;               // 0x1800b8ce3
            RT_EXPORT_ONCE(WideSetFn2, "?Set@WideString@System@@QEAAXPEB_W@Z")
                (&w, nameBuf);                                  // 0x1800b8cf5
            ArrayAppend(&stale, &w);
            RtSystem::RtString_ReleaseInline(&w);               // 0x1800b8efe
        }

        reg.enumIndex = 0;                                      // 0x1800b8f7a
        for (int32_t i = 0; i < stale.size; ++i) {              // 0x1800b8f90
            if (reg.key == nullptr) break;
            EmitDelete(static_cast<HKEY>(reg.key),
                       reinterpret_cast<const wchar_t*>(stale.data[i].data));
        }

        if (reg.key != nullptr) RegCloseKey(static_cast<HKEY>(reg.key));
        reg.key = nullptr;
        RtSystem::RtString_ReleaseInline(&reg.path);            // 0x1800b8fc3
        ArrayDestroy(&stale);                                   // 0x1800b9027
    }
}

// ---- the probe -------------------------------------------------------------
//
// IAT interception. The two slots are DATA in the DLL's .rdata-adjacent import
// table; `ResolveAddr(Module::Runtime, …)` supplies the ASLR-corrected address
// (nothing is registered at either, so no port-registry interception applies —
// reference_resolveaddr_consults_port_registry_first.md, checked by grep).
LONG WINAPI ThunkRegSetValueExW(HKEY, LPCWSTR name, DWORD, DWORD type,
                                const BYTE* data, DWORD cb) {
    RecordEmission(0, name, type, reinterpret_cast<const wchar_t*>(data), cb);
    return ERROR_SUCCESS;
}
LONG WINAPI ThunkRegDeleteKeyW(HKEY, LPCWSTR name) {
    RecordEmission(1, name, 0, nullptr, 0);
    return ERROR_FILE_NOT_FOUND;   // what the real call returns here; see banner
}

bool PatchSlot(uintptr_t ghidraVa, void* fn, void** saved) {
    uintptr_t va = ResolveAddr(Module::Runtime, ghidraVa);
    if (!va) return false;
    void** slot = reinterpret_cast<void**>(va);
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *saved = *slot;
    *slot = fn;
    VirtualProtect(slot, sizeof(void*), old, &old);
    return true;
}

void RestoreSlot(uintptr_t ghidraVa, void* saved) {
    uintptr_t va = ResolveAddr(Module::Runtime, ghidraVa);
    if (!va || !saved) return;
    void** slot = reinterpret_cast<void**>(va);
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return;
    *slot = saved;
    VirtualProtect(slot, sizeof(void*), old, &old);
}

bool g_probeArmed = false;
bool g_probeRan   = false;

// The same predicate as CompareRecorders but SILENT — used by the negative
// control, which EXPECTS a difference and must not print three of them as if
// they were defects. Split rather than parameterised so the two cannot drift:
// the verdict and the message are computed from one expression each
// (reference_shadow_predicate_must_name_its_failing_term.md).
int CompareRecordersQuiet(const RegRecorder& o, const RegRecorder& t) {
    int bad = 0;
    const int n = (o.n < t.n) ? o.n : t.n;
    for (int i = 0; i < n; ++i) {
        const RegEmission& a = o.e[i];
        const RegEmission& b = t.e[i];
        if (a.kind == b.kind && a.type == b.type && a.cb == b.cb &&
            std::wcscmp(a.name, b.name) == 0 &&
            (a.kind != 0 || std::wcscmp(a.data, b.data) == 0))
            continue;
        ++bad;
    }
    if (o.n != t.n) ++bad;
    return bad;
}

int CompareRecorders(const RegRecorder& o, const RegRecorder& t) {
    int bad = 0;
    const int n = (o.n < t.n) ? o.n : t.n;
    for (int i = 0; i < n; ++i) {
        const RegEmission& a = o.e[i];
        const RegEmission& b = t.e[i];
        if (a.kind == b.kind && a.type == b.type && a.cb == b.cb &&
            std::wcscmp(a.name, b.name) == 0 &&
            (a.kind != 0 || std::wcscmp(a.data, b.data) == 0))
            continue;
        if (bad < 3)
            printf("[WriteSettings] EMISSION %d DIFFERS: ours[kind=%d "
                        "type=%u cb=%u name=\"%ls\" data=\"%ls\"] "
                        "theirs[kind=%d type=%u cb=%u name=\"%ls\" data=\"%ls\"]\n",
                        i, a.kind, a.type, a.cb, a.name, a.data,
                        b.kind, b.type, b.cb, b.name, b.data);
        ++bad;
    }
    if (o.n != t.n) {
        printf("[WriteSettings] EMISSION COUNT DIFFERS: ours=%d theirs=%d\n",
                    o.n, t.n);
        ++bad;
    }
    return bad;
}

void MaybeRunProbe(const Persistence* self) {
    if (!g_probeArmed || g_probeRan) return;
    g_probeRan = true;

    using WriteFn = void (*)(const Persistence*);
    WriteFn theirs = reinterpret_cast<WriteFn>(ResolveRuntimeRaw(0x1800b88e0));
    if (!theirs) {
        printf("[WriteSettings] SKIP -- the shipped 0x1800b88e0 is not "
                    "reachable (cutover, or the DLL is absent). "
                    "NO COMPARISON WAS MADE.\n");
        return;
    }

    // ---- THE CORPUS CONTROL, and why it writes one value ------------------
    // The first run of this probe reported `staleNamesFound=0`: every value
    // under the key was in the settings map, so phase 2's array append and its
    // delete pass NEVER RAN and the comparison covered phase 1 only. A green
    // line over an unexercised half is the failure this project keeps paying
    // for, so the probe seeds the branch it needs rather than reporting that it
    // could not reach it.
    //
    // One value, under a name no setting will ever collide with, written before
    // the comparison and removed with `RegDeleteValueW` after it. It is the one
    // registry mutation anywhere in this file's oracle, it is confined to the
    // probe's own opt-in env var, and it is undone unconditionally below.
    //
    // NOTE it must be a VALUE and not a subkey: what phase 2 enumerates is
    // values (RegEnumValueW), and what it then calls is RegDeleteKeyW — see
    // the banner's finding. Seeding a value is what puts a name in the stale
    // array; retail's delete then correctly fails to remove it, which is the
    // behaviour under test.
    static const wchar_t kSeedName[] = L"__WriteSettingsOracle_StaleName__";
    HKEY seedKey = nullptr;
    bool seeded = false;
    {
        Registry probeReg;
        RT_EXPORT_ONCE(RegistryCtorFn, "??0Registry@IO@@QEAA@PEBD@Z")(&probeReg, nullptr);
        seedKey = static_cast<HKEY>(probeReg.key);
        if (seedKey) {
            static const wchar_t kSeedData[] = L"stale";
            seeded = RegSetValueExW(seedKey, kSeedName, 0, REG_SZ,
                                    reinterpret_cast<const BYTE*>(kSeedData),
                                    sizeof(kSeedData)) == ERROR_SUCCESS;
        }
        // The handle is kept open across the comparison so the seed can be
        // removed afterwards; only the path String is released here.
        RtSystem::RtString_ReleaseInline(&probeReg.path);
    }

    static RegRecorder ours, theirsRec;
    ours.n = ours.dropped = 0;
    theirsRec.n = theirsRec.dropped = 0;

    const long set0 = g_wsSetCalls, stale0 = g_wsStaleFound, del0 = g_wsDeleteCalls;

    // OURS: the body's own seam, no patching needed.
    g_recorder = &ours;
    WriteSettingsBody(self);
    g_recorder = nullptr;

    const long dSet = g_wsSetCalls - set0;
    const long dStale = g_wsStaleFound - stale0;
    const long dDel = g_wsDeleteCalls - del0;

    // THEIRS: two IAT slots, swapped for the duration and put back.
    void* savedSet = nullptr;
    void* savedDel = nullptr;
    const bool okS = PatchSlot(0x18014b018, (void*)&ThunkRegSetValueExW, &savedSet);
    const bool okD = PatchSlot(0x18014b008, (void*)&ThunkRegDeleteKeyW, &savedDel);
    if (!okS || !okD) {
        RestoreSlot(0x18014b018, savedSet);
        RestoreSlot(0x18014b008, savedDel);
        printf("[WriteSettings] SKIP -- could not swap the ADVAPI32 IAT "
                    "slots (set=%d del=%d). Running the shipped body without "
                    "them would WRITE TO THE PLAYER'S REGISTRY, so no "
                    "comparison was made.\n", okS ? 1 : 0, okD ? 1 : 0);
        return;
    }
    g_recorder = &theirsRec;
    theirs(self);
    g_recorder = nullptr;
    RestoreSlot(0x18014b018, savedSet);
    RestoreSlot(0x18014b008, savedDel);

    const int bad = CompareRecorders(ours, theirsRec);

    // ---- NEGATIVE CONTROL --------------------------------------------------
    // Re-run OUR body with the name/data swap, into a third recorder, and check
    // that the comparison rejects it. If it does not, every "mismatches=0"
    // above is void (reference_probe_needs_an_effectiveness_control.md). The
    // swapped run records and writes nothing, exactly like the other two.
    static RegRecorder negRec;
    negRec.n = negRec.dropped = 0;
    g_recorder = &negRec;
    g_negativeSwap = true;
    WriteSettingsBody(self);
    g_negativeSwap = false;
    g_recorder = nullptr;
    const bool negCaught = CompareRecordersQuiet(negRec, theirsRec) != 0;

    // Remove the seed unconditionally, whichever way the comparisons went, and
    // ONLY NOW — the negative control must see the same corpus as the other two
    // runs, or its verdict comes from an emission-COUNT difference the swap did
    // not cause and it would report CAUGHT for the wrong reason.
    // RegDeleteValueW, not RegDeleteKeyW: this is the harness cleaning up after
    // itself and it must actually work, which is precisely what retail's own
    // delete pass does not do.
    if (seeded) RegDeleteValueW(seedKey, kSeedName);
    if (seedKey) RegCloseKey(seedKey);

    printf("[WriteSettings] negative-control name/data swap: %s\n",
                negCaught ? "CAUGHT" : "*** MISSED ***");
    if (!negCaught)
        printf("[WriteSettings] *** the negative control was MISSED: the "
                    "emission comparison cannot see a swapped name and value, "
                    "so the green line above is void ***\n");

    printf("[WriteSettings] corpus: stale-name seed %s\n",
                seeded ? "PLANTED and removed" : "*** NOT PLANTED ***");
    printf("[WriteSettings] emissions ours=%d theirs=%d mismatches=%d "
                "dropped(ours=%d theirs=%d)\n"
                "[WriteSettings] effectiveness (inside our body): "
                "RegSetValueExW=%ld staleNamesFound=%ld RegDeleteKeyW=%ld\n",
                ours.n, theirsRec.n, bad, ours.dropped, theirsRec.dropped,
                dSet, dStale, dDel);
    if (dSet == 0)
        printf("[WriteSettings] *** RegSetValueExW=0: the settings map was "
                    "EMPTY, so phase 1 emitted nothing and this run verified "
                    "only that both sides emit nothing ***\n");
    if (dStale == 0)
        printf("[WriteSettings] *** staleNamesFound=0: every enumerated "
                    "value was in the map, so phase 2's array append and its "
                    "delete pass never ran. The corpus, not the port, is what "
                    "is untested here ***\n");
}

}  // namespace

void RtIoWriteSettingsProbeArm(bool on) { g_probeArmed = on; }

extern "C" void RtIo_Persistence_WriteSettings(const Persistence* self) {
    RT_DIFF_BEGIN("IO::Persistence::WriteSettings");
    MaybeRunProbe(self);
    WriteSettingsBody(self);
    // No RT_DIFF_* comparison here: the comparison IS the probe above, and it
    // cannot run on every call (it replays the whole body). Reported through
    // its own [WriteSettings] channel so a reader is never shown a green
    // `divergences=0` line that measured nothing.
    RT_DIFF_SHADOW(true, "emitted (see [WriteSettings] for the comparison)",
                   "not compared here");
}

// ============================================================================
// Registration
// ============================================================================
int RtIoPersistenceRegistryRegister() {
    int n = 0;
    RegisterRuntimeExport("?ReadSettings@Persistence@IO@@QEAAXXZ",
                          (void*)&RtIo_Persistence_ReadSettings); ++n;
    RegisterRuntimeAddr(0x1800b84b0, (void*)&RtIo_Persistence_ReadSettings);

    RegisterRuntimeExport("??0Registry@IO@@QEAA@PEBD@Z",
                          (void*)&RtIo_Registry_Ctor); ++n;
    RegisterRuntimeAddr(0x1800b9500, (void*)&RtIo_Registry_Ctor);

    // WAVE 9 LANE SAVE2. `QEBA` -- the CONST spelling -- verified against the
    // shipped export table by RVA before this line was written; `QEAA` here
    // would be a phantom name resolving NULL in silence
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md, the same trap one
    // qualifier over).
    RegisterRuntimeExport("?WriteSettings@Persistence@IO@@QEBAXXZ",
                          (void*)&RtIo_Persistence_WriteSettings); ++n;
    RegisterRuntimeAddr(0x1800b88e0, (void*)&RtIo_Persistence_WriteSettings);
    {
        const char* p = std::getenv("BANISHED_RT_WRITESETTINGS_PROBE");
        RtIoWriteSettingsProbeArm(p && *p && std::strcmp(p, "0") != 0);
    }

    // DELIBERATELY ABSENT — every dependency this file reaches that is still on
    // the DLL, so the next lane does not have to re-derive the list:
    //   ?Set@WideString@System@@QEAAXPEB_W@Z            0x1800caf40  ord 3823
    //   ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ   0x1800cacd0  ord 1625
    //   ??0WideString@System@@QEAA@AEBVString@1@@Z      0x1800cab50  ord  513
    //   Map_Insert (no export name)                     0x18002a090
    //   Map_Clear  (no export name)                     0x18000f7d0
    // Ported already and reached through the port-aware door:
    //   ?Set@String@System@@QEAAXPEBD@Z, ?Format@String@System@@SA?AV12@PEBDZZ,
    //   ??HSystem@@YA?AVString@0@AEBV10@PEBD@Z, ??4String@System@@QEAAAEAV01@AEBV01@@Z
    //
    // CORRECTED 2026-09-08 by WAVE 9 LANE SAVE2. This block used to end "NOT
    // registered, and not ported: ?WriteSettings@Persistence@IO@@QEBAXXZ
    // (0x1800b88e0, ord 4561) ... it is the one function in this pair that
    // calls RegSetValueExW and RegDeleteKeyW". The second half of that sentence
    // is still true and is exactly WHY it was portable: those two calls are the
    // whole of its observable effect, so intercepting them turns the "no safe
    // oracle" into an emission-sequence comparison that writes nothing. It is
    // ported and registered above.
    //
    // ALSO CORRECTED: `Map_Clear (no export name) 0x18000f7d0` is listed above
    // as a dependency still on the DLL. It is PORTED as of this wave
    // (runtime/src/collection/set_clear.cpp, registered by address), so the
    // MapClear() call at the top of ReadSettings now reaches our body. The
    // "NOT registered by any lane. Reaches the DLL." note further up this file
    // is corrected in place for the same reason.
    return n;
}
