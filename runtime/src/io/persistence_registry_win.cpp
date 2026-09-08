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
//   0x18000f7d0  Map_Clear  — NOT registered by any lane. Reaches the DLL.
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

void MapClear(void* self) {
    static MapClearFn fn = reinterpret_cast<MapClearFn>(
        ResolveAddr(Module::Runtime, 0x18000f7d0));
    if (fn) fn(self);
}

void MapInsert(void* map, void** outNode, const String* key, const String* value) {
    static MapInsertFn fn = reinterpret_cast<MapInsertFn>(
        ResolveAddr(Module::Runtime, 0x18002a090));
    if (fn) fn(map, outNode, key, value);
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
    // it is a plain typedef rather than an RT_EXPORT_ONCE constrained Fn — the
    // same exception persistence.cpp makes, but through the PORT-AWARE door,
    // because at cutover the shipped Format is not there to call.
    using FormatFn = String* (*)(String* ret, const char* fmt, ...);
    static FormatFn format = reinterpret_cast<FormatFn>(
        ResolveExport("?Format@String@System@@SA?AV12@PEBDZZ"));
    String tmp;
    tmp.data = nullptr;
    if (format) {
        format(&tmp, RtIo::kRegistryPathFormat,
               RtIo::kRegistryCompanyName, RtIo::kRegistryProductName);
    }
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
        std::printf("[BOOT2 ReadSettings] entries=%u count_field=%d first=\"%s\"\n",
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
    // NOT registered, and not ported: ?WriteSettings@Persistence@IO@@QEBAXXZ
    // (0x1800b88e0, ord 4561). It is the shutdown half — Engine::Shutdown phase
    // 1, runtime/src/engine/engine_lifecycle.cpp:922 — not the boot path, and it
    // is the one function in this pair that calls RegSetValueExW and
    // RegDeleteKeyW. See the lane report.
    return n;
}
