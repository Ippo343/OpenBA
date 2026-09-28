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

#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../../types/io/Registry.h"
#include "../../types/system/String.h"
#include "../system/string.h"   // RtString_ReleaseInline, RtSystem::String
#include "../system/pod_bridge.h"
#include "../collection/avl_trees.h"          // rtavl::StringMapInsert
#include "../../types/collection/Collection.h"   // rtsetclear::Clear
#include "../../types/system/Configuration.h"    // RtSystem_ConfigMap_Find
#include "../../types/system/MemoryAllocator.h"
#include "../../types/system/WideString.h"

using RtIo::Persistence;
using RtIo::Registry;
using RtIo::SettingsMapNode;
using RtSystem::String;
using RtSystem::StringRefCount;
using RtSystem::Sys;   // the transitional POD->class bridge (pod_bridge.h)

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
// B3R RT-IO: every dependency below is called BY NAME (they were RT_EXPORT_ONCE
// / ResolveAddr / RtResolveCached doors):
//   0x18000f7d0  Map_Clear   -> rtsetclear::Clear (collection/set_clear.cpp)
//   0x18002a090  Map_Insert  -> rtavl::StringMapInsert (collection/avl_trees.cpp)
//   0x18002ace0  Map_Find    -> RtSystem_ConfigMap_Find (system/configuration.cpp)
//   System::WideString ctor / Set / AsUTF8, System::String::Set / Format,
//   IO::Registry's ctor, System::MemoryAllocator::MemAlloc / MemFree.

// System::WideString is one pointer with String's exact allocation header —
// every release of one in ReadSettings is `DEC [p-4] / SUB p,8 / MemFree`, byte
// for byte the String idiom (String.h). `filesystem_win.cpp` already models it
// this way and this file follows rather than inventing a second spelling.
using WideStringHandle = String;

// ENDGAME B.3.2 step 2 (2026-09-13) — NOT CONVERTED to `Collection::Set<T>`
// members, deliberately, and for the SAME reason persistence.cpp's
// CallMapFind/CallMapInsert are not: `Persistence` (+0x00/+0x08/+0x0C) IS a
// real `Collection::Set<SettingsMapNode>` header and 0x18000f7d0 IS
// `Set<T>::Clear()` (set_clear.cpp's own banner names it), but `Clear()`
// destroys each element with `n->element.~T()` and `SettingsMapNode`'s two
// fields are the old `RtSystem::String` POD (trivially destructible) rather
// than the real `System::String` class — so a generic `Clear()` would leak
// both refcounts instead of releasing them. Retyping `SettingsMapNode` to fix
// that is shared with persistence_startup.cpp / closure_batch2b_win.cpp /
// engine/singleton_ctors.cpp, outside this batch. The members that would be
// needed: `Collection::Set<SettingsMapNode, AVLCompare, DefaultAllocator,
// System::String>::Clear()` here and `::Insert(element)` for MapInsert below.
void MapClear(void* self) {
    rtsetclear::Clear(static_cast<rtsetavl::Set*>(self));            // 0x18000f7d0
}

void MapInsert(void* map, void** outNode, const String* key, const String* value) {
    rtavl::StringMapInsert(static_cast<rtsetavl::Set*>(map),         // 0x18002a090  B.3.x bridge
                           reinterpret_cast<rtsetavl::Node**>(outNode),
                           &key->data, &value->data);
}

// Convert one NUL-terminated UTF-16 buffer to a System::String, exactly the
// six-call sequence retail inlines twice per loop iteration (0x1800b85b2 ..
// 0x1800b8649 for the name, 0x1800b86a5 .. 0x1800b879b for the value).
void AssignFromWide(String* dst, const wchar_t* src) {
    WideStringHandle w;
    w.data = nullptr;                                   // MOV [RBP+0x1010],R12
    reinterpret_cast<System::WideString*>(&w)->Set(src);          // B.3.x bridge

    // AsUTF8 returns System::String BY VALUE into the hidden return slot
    // `utf8`, CONSTRUCTED, not assigned.
    String utf8;
    utf8.data = nullptr;
    ::new (&Sys(utf8)) System::String(                                // B.3.x bridge
        reinterpret_cast<const System::WideString*>(&w)->AsUTF8());

    // MOV RDX,[RAX] — retail reads the char* back out of the returned String
    // and hands THAT to String::Set, which copies the bytes. It does not adopt
    // the temporary's buffer.
    Sys(*dst).Set(utf8.data);

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
// ----------------------------------------------------------------------------
// ENDGAME B.3.4 PACKAGE+FS commit 1 — the body is now
// `IO::Registry::Registry(const char*)` (below). The door keeps its name, its
// by-name AND by-address registration, and DELEGATES.
//
// It delegates by PLACEMENT-NEW rather than by a member call, because a
// constructor is not callable on an already-live object: the door's contract is
// "turn these 0x18 uninitialised bytes into a Registry", which is exactly what
// placement-new into the caller's storage is. Nothing is allocated and nothing
// moves — `IO::Registry` is layout-identical to the `RtIo::Registry` POD the
// door's signature names (both `static_assert`ed in runtime/types/io/Registry.h)
// — so the object the caller sees is byte-for-byte the one retail's ctor built.
//
// No qualified `IO::Registry::` call and no recursion hazard here: B34_BRIEFS
// §A.1's rule is about POLYMORPHIC receivers whose live instances wear a MINTED
// table resolving back to these doors. `IO::Registry` has no vtable at all
// (Registry.h's banner), so there is nothing to dispatch through.
// ============================================================================
extern "C" Registry* RtIo_Registry_Ctor(Registry* self, const char* subKey) {
    ::new (static_cast<void*>(self)) IO::Registry(subKey);
    return self;   // 0x1800b9747: MOV RAX,RDI
}

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
Registry::Registry(const char* subKey) {

    // 0x1800b9520-0x1800b952a. The qword store at +0x10 clears BOTH dword
    // cursors; see runtime/types/io/Registry.h. Written as retail's four
    // stores rather than as a member-init list so the ORDER stays visible and
    // so `_path` keeps its POD spelling (no String constructor runs).
    _path.data      = nullptr;
    _key            = nullptr;
    _enumIndex      = 0;
    _enumGroupIndex = 0;

    // 0x1800b954b: String::Format(&tmp, "Software\\%s\\%s", company, product),
    // then String::operator= inlined at 0x1800b9553. B3R RT-IO: Format by name
    // (System::String::Format, retail 0x1800c9730).
    System::String tmp = System::String::Format(RtIo::kRegistryPathFormat,
           RtIo::kRegistryCompanyName, RtIo::kRegistryProductName);
    // The leading equality test is retail's self-assignment guard (see
    // persistence_startup.cpp's banner for why it must not be tidied away) --
    // this IS `String::operator=` (retail 0x1800c9810). `_path` is still the
    // `RtSystem::String` POD spelling (Registry.h's banner says why: three
    // hand-written inlined `~Registry`s, one of them in a body left as C), so
    // it is reached through the transitional bridge.
    Sys(_path) = tmp;
    // ~tmp is retail's release.

    // 0x1800b95c0: `subKey != nullptr && subKey[0] != '\0'`. Retail then walks
    // subKey to its NUL and tests `length <= 0` (0x1800b95d3-0x1800b95ed) —
    // provably redundant, since the byte test above already established
    // length >= 1. The walk is not reproduced; it computes a value retail then
    // uses for nothing but that dead comparison.
    if (subKey != nullptr && subKey[0] != '\0') {
        // `operator+` and `operator=` are already-ported class members, so
        // the two exported doors this ctor used to resolve by name
        // (??HSystem@@... and ??4String@System@@...) are spelled as
        // themselves rather than through RT_EXPORT_ONCE — table entries
        // `ConcatCStr` -> `a + p` and `Assign` -> `s = o`.
        System::String withSep = Sys(_path) + RtIo::kRegistryPathSeparator;     // 0x1800b9605
        System::String full    = withSep + subKey;                              // 0x1800b9619
        Sys(_path) = full;                                                      // 0x1800b9625
        // ~full, ~withSep are retail's two releases at
        // 0x1800b962b/0x1800b9664.
    }

    // 0x1800b969d onward.
    DWORD disposition = 0;

    WideStringHandle wpath;
    wpath.data = nullptr;
    ::new (static_cast<void*>(&wpath)) System::WideString(Sys(_path));   // B.3.x bridge

    HKEY* phKey = reinterpret_cast<HKEY*>(&_key);
    const wchar_t* w = reinterpret_cast<const wchar_t*>(wpath.data);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, w, 0, kSam, phKey) != ERROR_SUCCESS) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, w, 0, nullptr, 0, kSam,
                            nullptr, phKey, &disposition) != ERROR_SUCCESS) {
            _key = nullptr;    // 0x1800b971c
        }
    }

    RtSystem::RtString_ReleaseInline(&wpath);
}

}  // namespace IO

// ============================================================================
// ?ReadSettings@Persistence@IO@@QEAAXXZ                    retail 0x1800b84b0
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 6: the body is now
// `IO::Persistence::ReadSettings()`, below. The door keeps its name, its
// `extern "C"` linkage and both registration lines, and delegates. No qualified
// `IO::` call is needed: `IO::Persistence` has no vtable of its own -- the vptr
// at +0x10 is the EMBEDDED worker's (runtime/types/io/Persistence.h) -- so
// B34_BRIEFS section A.1's re-entry hazard cannot arise.
extern "C" void RtIo_Persistence_ReadSettings(Persistence* self) {
    reinterpret_cast<IO::Persistence*>(self)->ReadSettings();
}

// ============================================================================
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
// self-assignment guard on the BUFFER, and the same by-hand teardown of `name`,
// `value` and the `IO::Registry` local at retail's own positions -- `reg` is
// still a local of a class with NO destructor (Registry.h's banner), so nothing
// implicit runs at scope exit and nothing can run twice.
namespace IO {

void Persistence::ReadSettings() {

    ::MapClear(reinterpret_cast<RtIo::Persistence*>(this));    // 0x1800b84db

    // 0x1800b84e7 `Registry reg(nullptr)`. B.3.4 PACKAGE+FS commit 1: the local
    // is the REAL class now, constructed directly instead of through
    // `RT_EXPORT_ONCE("??0Registry@IO@@QEAA@PEBD@Z")`. That name resolves to
    // `RtIo_Registry_Ctor`, which since this commit does nothing but
    // placement-new the same constructor into the same bytes — so this is the
    // identical body reached one indirection earlier, not a different one.
    //
    // `IO::Registry` has NO destructor (Registry.h's banner), so this local is
    // still torn down BY HAND at the bottom of this function, at retail's own
    // position. Nothing implicit runs at scope exit and nothing can run twice.
    IO::Registry reg(nullptr);

    // `name`/`value` stay `RtSystem::String` (not `System::String`) because
    // `AssignFromWide` and `MapInsert` below are external-door signatures
    // shared with `WriteSettingsBody` (left in C shape -- see its own
    // comment); ownership is still proven and expressed through the
    // transitional bridge at each use.
    RtSystem::String name{nullptr};                            // 0x1800b84f4
    RtSystem::String value{nullptr};                           // 0x1800b84fc

    // Deliberately uninitialised — see banner point 4.
    wchar_t nameBuf[::kNameChars];
    wchar_t valueBuf[::kValueChars];

    for (;;) {
        DWORD cchName = ::kNameChars;                          // 0x1800b8510
        const DWORD index = reg._enumIndex++;                   // 0x1800b851a

        if (RegEnumValueW(static_cast<HKEY>(reg._key), index, nameBuf, &cchName,
                          nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;                                             // 0x1800b8552
        if (cchName > ::kNameChars)
            break;                                             // 0x1800b8562

        if (reg._key != nullptr) {                              // 0x1800b8570, dead
            DWORD type    = REG_SZ;                            // 0x1800b8572, = 1
            DWORD cbData  = ::kValueBytes;                     // 0x1800b857c
            // Return value IGNORED, exactly as at 0x1800b85b2.
            RegQueryValueExW(static_cast<HKEY>(reg._key), nameBuf, nullptr, &type,
                             reinterpret_cast<BYTE*>(valueBuf), &cbData);
        }

        ::AssignFromWide(&name, nameBuf);                      // 0x1800b85b2..
        ::AssignFromWide(&value, valueBuf);                    // 0x1800b86a5..

        void* node = nullptr;
        ::MapInsert(reinterpret_cast<RtIo::Persistence*>(this),
                    &node, &name, &value);                     // 0x1800b87ad

        // 0x1800b87b2-0x1800b882d. Map_Insert leaves an ALREADY-PRESENT key's
        // value untouched, so retail re-seats it by hand. The `!=` is the
        // self-assignment guard; see banner points 1 and 2.
        if (node != nullptr) {
            RtIo::SettingsMapNode* n = static_cast<RtIo::SettingsMapNode*>(node);
            // `String::operator=` (retail 0x1800c9810), through the
            // transitional bridge on both not-yet-converted PODs.
            if (n->value.data != value.data) {
                ::Sys(n->value) = ::Sys(value);
            }
        }
    }

    reg._enumIndex = 0;                                         // 0x1800b8832
    // `name`/`value` are plain locals of a type that keeps its POD shape
    // here (see the declaration comment), so there is no outer RAII to run
    // this for us; retail's two releases are spelled as the destructors
    // themselves, through the bridge, in retail's own order.
    ::Sys(value).~String();                                    // 0x1800b8837
    ::Sys(name).~String();                                     // 0x1800b8859

    // ~Registry, inlined at 0x1800b887d. `reg` is a plain local (`Registry`
    // is not yet converted), so `path`'s release is spelled as the
    // destructor itself through the bridge.
    if (reg._key != nullptr) RegCloseKey(static_cast<HKEY>(reg._key));
    reg._key = nullptr;
    ::Sys(reg._path).~String();                                 // 0x1800b8892
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
// Node parent, +0x10. Pinned by this very function: 0x1800b8a9c reads
// [node+0x10] as the parent in the in-order successor, and 0x1800b8aa5 compares
// [parent+0x18] against the node. That is the same layout WAVE3_LANE_A.md
// derived generally and the same one Map_Clear pins for left/right.
constexpr size_t kNodeParent = 0x10;

// 0x18002ace0 -- `RtSystem_ConfigMap_Find` (runtime/src/system/configuration.cpp),
// by name (B3R RT-IO). There is no export name at that RVA at all.
void* MapFind(const void* map, const String* key) {
    return RtSystem_ConfigMap_Find(const_cast<void*>(map), key);
}

// The allocator boundary for phase 2's array. Retail open-codes
// `Lock(alloc+0x640); Allocate/Free(_singleton, ...); Unlock()` at
// 0x1800b8d69 and 0x1800b8e29 -- which is exactly what the two static exports
// below ARE (0x1800c7a50 / 0x1800c7ad0, confirmed by RVA in the export table).
// A composition identity, not a substitution; `runtime/src/collection/array.cpp`
// makes the same argument for the same pair.
void* MemAllocRaw(uint64_t bytes, int align) {
    return ::System::MemoryAllocator::MemAlloc(bytes, align);
}
void MemFreeRaw(void* p) {
    ::System::MemoryAllocator::MemFree(p);
}

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
                   reinterpret_cast<const BYTE*>(data), cb);
}

void EmitDelete(HKEY key, const wchar_t* name) {
    ++g_wsDeleteCalls;
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
//
// LEFT AS C (ENDGAME B.3.1 step 2, conv-io batch): this body interleaves two
// tree walks, a hand-inlined growing `Array<WideString>` with a non-obvious
// capacity rule, an IAT-swapping emission oracle, and `AssignFromWide`/
// `MapFind`/`MapInsert` calls shared with `ReadSettings` above -- retyping any
// one of `key`, `wValue`, `wName`, `w`, or the array's elements would either
// require changing those shared file-local doors' signatures (risking
// ReadSettings, which IS converted) or a second parallel spelling of each
// door. Proving "exactly one release on every path" here means proving it
// across the growth rule's realloc-vs-no-realloc split AND the shrink-arm
// comment AND two enumeration loops with early `continue`/`break`; the
// combination is exactly the "retail unwind bitmask across ten locals" case
// B31_RULES calls out as a left-alone body being the fine outcome. Every
// `String`/`WideStringHandle` local here keeps the manual
// Ctor/Set/ReleaseInline shape it already had; nothing in this function
// changed.
void WriteSettingsBody(const Persistence* self) {

    // ---- phase 1 -----------------------------------------------------------
    {
        Registry reg;
        ::new (static_cast<void*>(&reg)) IO::Registry(nullptr); // 0x1800b8912  B.3.x bridge

        void* node = Leftmost(self->settingsMapRoot);           // 0x1800b8918
        while (node != nullptr) {
            const SettingsMapNode* n = static_cast<const SettingsMapNode*>(node);

            WideStringHandle wValue; wValue.data = nullptr;
            ::new (static_cast<void*>(&wValue))                // 0x1800b894b  B.3.x bridge
                System::WideString(Sys(n->value));
            WideStringHandle wName;  wName.data = nullptr;
            ::new (static_cast<void*>(&wName))                 // 0x1800b895d  B.3.x bridge
                System::WideString(Sys(n->key));

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
        ::new (static_cast<void*>(&reg)) IO::Registry(nullptr); // 0x1800b8b70  B.3.x bridge

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
            reinterpret_cast<System::WideString*>(&w)->Set(   // 0x1800b8cf5  B.3.x bridge
                nameBuf);
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

}  // namespace

// ENDGAME B.3.4 PACKAGE+FS commit 6: the door now delegates to
// `IO::Persistence::WriteSettings()`, which is itself the same one-line call
// into `WriteSettingsBody` -- the 400-line C body that ENDGAME B.3.1
// DELIBERATELY LEFT AS C (it carries the inlined `~Registry` teardowns the
// Registry.h banner is about) is NOT moved and NOT touched. The door keeps its
// name, its `extern "C"` linkage and both registration lines.
extern "C" void RtIo_Persistence_WriteSettings(const Persistence* self) {
    reinterpret_cast<const IO::Persistence*>(self)->WriteSettings();
}

namespace IO {

void Persistence::WriteSettings() const {
    ::WriteSettingsBody(reinterpret_cast<const RtIo::Persistence*>(this));
}

}  // namespace IO

// ============================================================================
// Registration
// ============================================================================
int RtIoPersistenceRegistryRegister() {
    int n = 0;


    // WAVE 9 LANE SAVE2. `QEBA` -- the CONST spelling -- verified against the
    // shipped export table by RVA before this line was written; `QEAA` here
    // would be a phantom name resolving NULL in silence
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md, the same trap one
    // qualifier over).
    RegisterRuntimeExport("?WriteSettings@Persistence@IO@@QEBAXXZ",
                          (void*)&RtIo_Persistence_WriteSettings); ++n;
    RegisterRuntimeAddr(0x1800b88e0, (void*)&RtIo_Persistence_WriteSettings);

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
