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
// `the old DLL-era switch` this file runs the SHIPPED body (`the raw DLL door`, which
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
// `the diff oracle(count != 0)` is the effectiveness control: a run whose
// `worked=` column is 0 read an empty or missing registry key and the oracle
// did no work at all, whatever `divergences=` says.
// ============================================================================

#include <windows.h>

#include <new>        // placement new -- the ctor door's delegation shape
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include <utility>    // std::move -- the pointer steal out of a POD out-param

#include "../../types/io/Persistence.h"
#include "../../types/io/Registry.h"
#include "../../types/system/String.h"
#include "../collection/avl_trees.h"          // rtavl::StringMapInsert
#include "../../types/collection/Collection.h"   // rtsetclear::Clear
#include "../../types/system/Configuration.h"    // RtSystem_ConfigMap_Find
#include "../../types/system/MemoryAllocator.h"
#include "../../types/system/WideString.h"

using RtIo::SettingsMapNode;
using RtSystem::StringRefCount;

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

// `IO::Registry::_key` is spelled `HKEY__*` against a forward declaration
// (Registry.h); this is the check that it is the same type <windows.h> calls
// HKEY, so every Reg* call below takes it with no conversion.
static_assert(std::is_same<HKEY, HKEY__*>::value, "Registry.h's HKEY__* is <windows.h>'s HKEY");

// ---------------------------------------------------------------------------
// Dependencies
// ---------------------------------------------------------------------------
// B3R RT-IO: every dependency below is called BY NAME (they were RT_EXPORT_ONCE
// / ResolveAddr / RtResolveCached doors):
//   0x18000f7d0  Map_Clear   -> rtsetclear::Clear (collection/set_clear.cpp)
//   0x18002a090  Map_Insert  -> rtavl::StringMapInsert (collection/avl_trees.cpp)
//   0x18002ace0  Map_Find    -> RtSystem_ConfigMap_Find (system/configuration.cpp)
//   System::WideString ctor / Set / AsUTF8, System::String::Set / Format,
//   IO::Registry's ctor, System::MemoryAllocator::MemAlloc / MemFree.

// ENDGAME B.3.2 step 2 (2026-09-13) — NOT CONVERTED to `Collection::Set<T>`
// members: `Persistence` (+0x00/+0x08/+0x0C) IS a `Set<Pair<String,String>>`
// header and 0x18000f7d0 IS `Set<T>::Clear()` (set_clear.cpp's own banner
// names it), but `IO::Persistence` holds that header as three loose fields
// (it has no constructor -- engine/singleton_ctors.cpp builds it), so the
// collection bodies are handed `this` as the untyped Set they are.
void MapClear(void* self) {
    rtsetclear::Clear(static_cast<rtsetavl::Set*>(self));            // 0x18000f7d0  LEFT: see above
}

// Map_Insert hands back the collection's opaque-element node; the settings
// map's node IS `RtIo::SettingsMapNode` (Persistence.h).
SettingsMapNode* MapInsert(void* map, const System::String& key, const System::String& value) {
    rtsetavl::Node* node = nullptr;
    rtavl::StringMapInsert(static_cast<rtsetavl::Set*>(map),         // 0x18002a090  LEFT: see above
                           &node, &key._data, &value._data);
    return reinterpret_cast<SettingsMapNode*>(node);   // LEFT: set_avl.h's node keeps its element opaque
}

// Convert one NUL-terminated UTF-16 buffer to a System::String, exactly the
// six-call sequence retail inlines twice per loop iteration (0x1800b85b2 ..
// 0x1800b8649 for the name, 0x1800b86a5 .. 0x1800b879b for the value).
void AssignFromWide(System::String& dst, const wchar_t* src) {
    System::WideString w(src);                          // zero, then Set(src)

    // AsUTF8 returns System::String BY VALUE into the hidden return slot
    // `utf8`, CONSTRUCTED, not assigned.
    System::String utf8 = w.AsUTF8();

    // MOV RDX,[RAX] — retail reads the char* back out of the returned String
    // and hands THAT to String::Set, which copies the bytes. It does not adopt
    // the temporary's buffer.
    dst.Set(utf8._data);

    // ~utf8 then ~w: retail's two releases, in its order.
}

}  // namespace

namespace IO {

// ---------------------------------------------------------------------------
// IO::Registry::Registry(const char* subKey)              retail 0x1800b9500
// ---------------------------------------------------------------------------
// Member bodies of `IO::X` cannot be written inside `namespace RtIo` — `IO` and
// `RtIo` are siblings, so the definition has to appear in a namespace enclosing
// the class (B34_BRIEFS §A.2). This block sits AFTER the file's anonymous
// namespace, which is what lets the body still reach `AssignFromWide` and
// friends as ordinary file-scope names.
//
// Inside this block a bare `Registry` binds to the REAL CLASS, never to the POD
// (B34_RULES §7.2, THE GREP HAZARD). The POD is spelled `RtIo::Registry` here.
Registry::Registry(const char* subKey) : _path(), _key(nullptr), _enumIndex(0), _enumGroupIndex(0) {

    // 0x1800b9520-0x1800b952a: retail's four stores, the member initialisers
    // above (the qword store at +0x10 clears BOTH dword cursors; see
    // runtime/types/io/Registry.h).

    // 0x1800b954b: String::Format(&tmp, "Software\\%s\\%s", company, product),
    // then String::operator= inlined at 0x1800b9553. B3R RT-IO: Format by name
    // (System::String::Format, retail 0x1800c9730). The leading equality test
    // of `String::operator=` (retail 0x1800c9810) is retail's self-assignment
    // guard (see persistence_startup.cpp's banner for why it must not be
    // tidied away).
    System::String tmp = System::String::Format(RtIo::kRegistryPathFormat,
           RtIo::kRegistryCompanyName, RtIo::kRegistryProductName);
    _path = tmp;
    // ~tmp is retail's release.

    // 0x1800b95c0: `subKey != nullptr && subKey[0] != '\0'`. Retail then walks
    // subKey to its NUL and tests `length <= 0` (0x1800b95d3-0x1800b95ed) —
    // provably redundant, since the byte test above already established
    // length >= 1. The walk is not reproduced; it computes a value retail then
    // uses for nothing but that dead comparison.
    if (subKey != nullptr && subKey[0] != '\0') {
        System::String withSep = _path + RtIo::kRegistryPathSeparator;         // 0x1800b9605
        System::String full    = withSep + subKey;                              // 0x1800b9619
        _path = full;                                                           // 0x1800b9625
        // ~full, ~withSep are retail's two releases at
        // 0x1800b962b/0x1800b9664.
    }

    // 0x1800b969d onward.
    DWORD disposition = 0;

    System::WideString wpath(_path);                    // ??0WideString(const String&)
    const wchar_t* w = wpath._data;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, w, 0, kSam, &_key) != ERROR_SUCCESS) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, w, 0, nullptr, 0, kSam,
                            nullptr, &_key, &disposition) != ERROR_SUCCESS) {
            _key = nullptr;    // 0x1800b971c
        }
    }

    // ~wpath is retail's release.
}

// Retail's inlined `~Registry` (0x1800b887d / 0x1800b8ae5 / 0x1800b8fa8):
// close the key if open, null it, then release `_path` -- the member
// destructor, which runs after this body.
Registry::~Registry() {
    if (_key != nullptr) RegCloseKey(_key);
    _key = nullptr;
}

}  // namespace IO

// ============================================================================
// ?ReadSettings@Persistence@IO@@QEAAXXZ                    retail 0x1800b84b0
// `IO::Persistence::ReadSettings` -- the member body
// ============================================================================
// Defined in `namespace IO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2); the file-local
// helpers live in this translation unit's GLOBAL anonymous namespace and are
// reached as `::MapClear` / `::MapInsert` / `::AssignFromWide` / `::kNameChars`
// / `::kValueChars` / `::kValueBytes`. Every POD is spelled `RtIo::` or
// `RtSystem::` and every real class `System::` (B34_RULES section 7.2); note
// this file's POD namespace is `RtIo`, LOWERCASE o.
//
// NOTHING ELSE CHANGED: same callees in the same order, the same dead
// `if (reg._key)` retained, the same ignored RegQueryValueExW return, the same
// self-assignment guard on the BUFFER. B.4 R-IO: the teardown of `value`,
// `name` and `reg` is their destructors, at the end of the function -- reverse
// declaration order, which is retail's order.
namespace IO {

void Persistence::ReadSettings() {

    ::MapClear(this);                                          // 0x1800b84db

    // 0x1800b84e7 `Registry reg(nullptr)`. Its destructor is retail's inlined
    // ~Registry at 0x1800b887d (Registry.h's banner), the last of the three
    // at the close of this function.
    IO::Registry reg(nullptr);

    System::String name;                                       // 0x1800b84f4
    System::String value;                                      // 0x1800b84fc

    // Deliberately uninitialised — see banner point 4.
    wchar_t nameBuf[::kNameChars];
    wchar_t valueBuf[::kValueChars];

    for (;;) {
        DWORD cchName = ::kNameChars;                          // 0x1800b8510
        const DWORD index = reg._enumIndex++;                   // 0x1800b851a

        if (RegEnumValueW(reg._key, index, nameBuf, &cchName,
                          nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;                                             // 0x1800b8552
        if (cchName > ::kNameChars)
            break;                                             // 0x1800b8562

        if (reg._key != nullptr) {                              // 0x1800b8570, dead
            DWORD type    = REG_SZ;                            // 0x1800b8572, = 1
            DWORD cbData  = ::kValueBytes;                     // 0x1800b857c
            // Return value IGNORED, exactly as at 0x1800b85b2.
            RegQueryValueExW(reg._key, nameBuf, nullptr, &type,
                             reinterpret_cast<BYTE*>(valueBuf), &cbData);  // CAST-OK: win32
        }

        ::AssignFromWide(name, nameBuf);                       // 0x1800b85b2..
        ::AssignFromWide(value, valueBuf);                     // 0x1800b86a5..

        SettingsMapNode* const n = ::MapInsert(this, name, value);   // 0x1800b87ad

        // 0x1800b87b2-0x1800b882d. Map_Insert leaves an ALREADY-PRESENT key's
        // value untouched, so retail re-seats it by hand. The `!=` is the
        // self-assignment guard; see banner points 1 and 2.
        if (n != nullptr) {
            // `String::operator=` (retail 0x1800c9810).
            if (n->value._data != value._data) {
                n->value = value;
            }
        }
    }

    reg._enumIndex = 0;                                         // 0x1800b8832
    // ~value (0x1800b8837), ~name (0x1800b8859), ~reg (0x1800b887d..0x1800b8892)
    // run here, in that order.
}

}  // namespace IO

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
// 0x18002ace0 -- `RtSystem_ConfigMap_Find` (runtime/src/system/configuration.cpp),
// by name (B3R RT-IO). There is no export name at that RVA at all. Its
// signature (system/Configuration.h, R-CORE's) takes a non-const map and the
// RtSystem::String POD, and returns the node untyped.
const SettingsMapNode* MapFind(const IO::Persistence* map, const System::String& key) {
    return static_cast<const SettingsMapNode*>(                  // LEFT: R-CORE's void* result
        RtSystem_ConfigMap_Find(const_cast<IO::Persistence*>(map),   // LEFT: R-CORE's non-const map
                                reinterpret_cast<const RtSystem::String*>(&key)));   // LEFT: R-CORE's POD key
}

// The allocator boundary for phase 2's array: retail open-codes
// `Lock(alloc+0x640); Allocate/Free(_singleton, ...); Unlock()` at 0x1800b8d69
// and 0x1800b8e29 -- which is exactly what `System::MemoryAllocator::MemAlloc`
// / `MemFree` ARE (0x1800c7a50 / 0x1800c7ad0), called by name below.

// ---- the two mutating calls -----------------------------------------------
//
// Effectiveness counters, unconditional.
long g_wsSetCalls    = 0;
long g_wsStaleFound  = 0;
long g_wsDeleteCalls = 0;

void EmitSet(HKEY key, const wchar_t* name, DWORD type,
             const wchar_t* data, DWORD cb) {
    ++g_wsSetCalls;
    RegSetValueExW(key, name, 0, type,
                   reinterpret_cast<const BYTE*>(data), cb);   // CAST-OK: win32
}

void EmitDelete(HKEY key, const wchar_t* name) {
    ++g_wsDeleteCalls;
    // RegDeleteKeyW, not RegDeleteValueW — see the banner. It is expected to
    // fail with ERROR_FILE_NOT_FOUND every time, and retail ignores the result.
    RegDeleteKeyW(key, name);
}

// ---- the tree walk phase 1 uses -------------------------------------------
// The links are SettingsMapNode's own (Persistence.h): parent +0x10 (pinned by
// this very function, 0x1800b8a9c / 0x1800b8aa5), left +0x18, right +0x20.
const SettingsMapNode* Leftmost(const SettingsMapNode* node) {
    if (!node) return nullptr;
    for (;;) {
        const SettingsMapNode* l = node->left;
        if (!l) return node;
        node = l;
    }
}

// 0x1800b8a80..0x1800b8acb, transcribed branch for branch.
const SettingsMapNode* InorderSuccessor(const SettingsMapNode* node) {
    if (!node) return nullptr;
    const SettingsMapNode* r = node->right;
    if (r) {                                    // 0x1800b8a89
        for (;;) {                              // 0x1800b8a8e
            node = r;
            r = node->left;
            if (!r) return node;                // 0x1800b8a98
        }
    }
    const SettingsMapNode* p = node->parent;    // 0x1800b8a9c
    if (!p) return node->parent;
    if (p->left == node) return p;              // 0x1800b8aa5
    while (p && p->right == node) {             // 0x1800b8ab5
        node = p;
        p = node->parent;
    }
    return node->parent;                        // 0x1800b8ac7
}

// ---- phase 2's inlined Array<WideString> -----------------------------------
// The elements are `System::WideString`s in a raw MemAlloc buffer: each slot
// is written by hand (the refcount copy below) and released by its
// destructor, exactly the two halves retail inlines.
struct WideArray {
    System::WideString* data = nullptr; // R12  / [RSP+0x60]
    int32_t             size = 0;       // R13D / [RSP+0x68]
    int32_t             capacity = 0;   //        [RSP+0x6c]
};

// 0x1800b8cfb-0x1800b8dd4 plus the two teardown loops that follow it.
// The `fill` is COPIED BY REFCOUNT (`INC dword [RCX-4]`), not by value.
void ArrayAppend(WideArray* a, const System::WideString& fill) {
    const int32_t oldSize = a->size;
    const int32_t newSize = oldSize + 1;
    System::WideString* oldData = a->data;

    a->size = newSize;
    if (newSize > a->capacity) {                       // 0x1800b8d18
        int32_t cap = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)    // 0x1800b8d24
            cap = newSize * 3;                         // LEA EDX,[RSI+RSI*2]
        a->capacity = cap;
        a->data = static_cast<System::WideString*>(
            ::System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(cap) * 8, 8));
    }

    // 0x1800b8d81: start at oldSize when nothing was reallocated, else at 0.
    int32_t i = (oldData == a->data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        System::WideString* dst = &a->data[i];
        if (!dst) break;                                // retail's TEST RDX,RDX
        const System::WideString* src = (i < oldSize) ? &oldData[i] : &fill;
        dst->_data = nullptr;                           // MOV [RDX],RBX
        if (src->_data) {
            dst->_data = src->_data;
            // The WideString block carries String's {length, refCount} header.
            ++StringRefCount(reinterpret_cast<char*>(src->_data));   // CAST-OK: string_header
        }
    }

    if (oldData != a->data) {                           // 0x1800b8dd4
        for (int32_t j = 0; j < oldSize; ++j)
            oldData[j].~WideString();
        if (oldData) ::System::MemoryAllocator::MemFree(oldData);
    }
    // The `else` arm at 0x1800b8e88 releases elements [newSize, oldSize) — the
    // SHRINK case. Unreachable from an append (newSize > oldSize always), and
    // written out here as a comment rather than as dead code.
}

void ArrayDestroy(WideArray* a) {                       // 0x1800b9027-0x1800b90dc
    for (int32_t i = 0; i < a->size; ++i)
        a->data[i].~WideString();
    if (a->data) ::System::MemoryAllocator::MemFree(a->data);
    a->data = nullptr;
    a->size = 0;
}

}  // namespace

namespace IO {

// ---- the transcription itself ----------------------------------------------
//
// B.4 R-IO: the member body (it was a C body, `WriteSettingsBody`, taking the
// RtIo POD view). Every String / WideString / Registry local is the real class
// and each release is its destructor, placed by scope where retail's inlined
// release sits: the two WideStrings at the end of each phase-1 iteration
// (wName, then wValue -- LIFO), `key` before the found-test in phase 2, `w`
// after the append, and each `reg` at the close of its phase.
void Persistence::WriteSettings() const {

    // ---- phase 1 -----------------------------------------------------------
    {
        IO::Registry reg(nullptr);                              // 0x1800b8912

        const SettingsMapNode* node = Leftmost(_settingsMapRoot);   // 0x1800b8918
        while (node != nullptr) {
            {
                System::WideString wValue(node->value);         // 0x1800b894b
                System::WideString wName(node->key);            // 0x1800b895d

                if (reg._key != nullptr) {                      // 0x1800b896e
                    const wchar_t* data = wValue._data;
                    DWORD len = 0;
                    if (data) { while (data[len]) ++len; }      // 0x1800b8976 loop
                    EmitSet(reg._key, wName._data, REG_SZ, data,
                            static_cast<DWORD>(2 + 2 * len));   // LEA EAX,[2+R8*2]
                }

                // ~wName (0x1800b89ae), ~wValue (0x1800b8a18).
            }

            node = InorderSuccessor(node);
        }

        // ~reg: 0x1800b8ae5 / 0x1800b8af2.
    }

    // ---- phase 2 -----------------------------------------------------------
    // Retail closes the key and releases its path (~reg, 0x1800b8fa8 /
    // 0x1800b8fc3) BEFORE it tears the array down (0x1800b9027), so `reg`
    // lives in an inner scope that closes first.
    {
        WideArray stale;
        {
            IO::Registry reg(nullptr);                              // 0x1800b8b70

            // Deliberately uninitialised, exactly as retail leaves it: the only
            // path that reads it is one RegEnumValueW has just filled.
            wchar_t nameBuf[kNameChars];

            for (;;) {
                DWORD cch = kNameChars;                             // 0x1800b8b7e
                const DWORD index = reg._enumIndex++;               // 0x1800b8b88

                if (RegEnumValueW(reg._key, index, nameBuf, &cch,
                                  nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                    break;                                          // 0x1800b8bc0
                if (cch > kNameChars) break;                        // 0x1800b8bd0

                const SettingsMapNode* found;
                {
                    System::String key;
                    AssignFromWide(key, nameBuf);                   // 0x1800b8be8..
                    found = MapFind(this, key);                     // 0x18002ace0
                    // ~key
                }
                if (found != nullptr) continue;                     // 0x1800b8cd3

                ++g_wsStaleFound;
                {
                    System::WideString w(nameBuf);                  // 0x1800b8ce3 / 0x1800b8cf5
                    ArrayAppend(&stale, w);
                    // ~w: 0x1800b8efe
                }
            }

            reg._enumIndex = 0;                                     // 0x1800b8f7a
            for (int32_t i = 0; i < stale.size; ++i) {              // 0x1800b8f90
                if (reg._key == nullptr) break;
                EmitDelete(reg._key, stale.data[i]._data);
            }

            // ~reg: 0x1800b8fa8 / 0x1800b8fc3.
        }
        ArrayDestroy(&stale);                                   // 0x1800b9027
    }
}

}  // namespace IO

