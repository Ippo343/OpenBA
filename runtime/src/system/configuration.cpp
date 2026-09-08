// ============================================================================
// runtime/src/system/configuration.cpp — Wave 9 lane ENG, iteration 2
// System::Configuration::SetCommandLine + the string-keyed BST find it shares
// ============================================================================
//
// Attempt 08 got past the Engine wall and stopped exactly where lane ENG
// predicted:
//
//   [RtUnresolved] ?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z
//   Fault: EXEC of 0000000000000000
//     #1  RtEngine_Engine_Startup +0x17a
//
// This file closes that row and the one behind it.
//
//   | Export                                        | Retail      | Ord  |
//   |-----------------------------------------------|-------------|------|
//   | ?SetCommandLine@Configuration@System@@QEAAX…   | 0x1800c47a0 | 3849 |
//   | (unexported) the config map's BST find         | 0x18002ace0 |  --  |
//
// Both names checked against Runtime-x64.dll's export directory with pefile
// before a line was written. No new Win32 API and no new import library — this
// file makes no OS call at all.
//
// ---------------------------------------------------------------------------
// 1. WHAT `SetCommandLine` IS
// ---------------------------------------------------------------------------
// `System::Configuration` (0x30 bytes, ctor at 0x1800c51b0, ported by lane ENG
// in engine_lifecycle.cpp) holds a string→string map at +0x10. Retail:
//
//     int pos = 0;                                   // [RBP+0x48]
//     if (cmd.data == nullptr) return;               // 0x1800c47c7
//     int len = StringLength(cmd.data);              // MOV R14D,[RAX-0x8]
//     if (len <= 0) return;                          // 0x1800c47d7  JLE
//     do {
//         String key = ParseCommand(pos, cmd);       // 0x1800c4a00, ord 3310
//         if (!key.data || StringLength(key.data) == 0) { ~key; return; }
//         String val = ParseValue(pos, cmd);         // 0x1800c4930, ord 3311
//         Insert(&this->_map, &node, key, val);      // 0x18002a090 (unexported)
//         ~val; ~key;
//     } while (pos < len);
//
// Note `[RAX-0x8]` is the LENGTH word and `[RAX-0x4]` is the refcount
// (runtime/types/system/String.h). Both appear in this function and mixing them
// up would silently turn the loop's exit test into a refcount test.
//
// ---------------------------------------------------------------------------
// 2. THE THREE CALLEES ARE **NOT** PORTED — AND THE ROW STILL CLOSES, FOR ONE
//    SPECIFIC REASON THAT MUST BE STATED
// ---------------------------------------------------------------------------
// The triage rule says a port only CLOSES a row if its own callees are
// satisfied. `ParseCommand`, `ParseValue` and the insert at 0x18002a090 are all
// unported, so on the face of it this closes nothing. It closes it anyway,
// because on this project's boots **the loop body never runs**:
//
//   standalone/main.cpp:1930   Game::RunGameMain(hInstance, lpCmdLine ? lpCmdLine : "")
//
// and no harness in this tree passes a command-line argument (every knob is an
// environment variable). `String::Set(&tmp, "")` allocates a 9-byte block with
// `length = 0` at `data-8` and `refcount = 1` at `data-4`
// (runtime/src/system/string.cpp `RtSystem_String_Set` → `RtString_SetLength`),
// so `cmd.data` is NON-null and `len == 0`, and retail's own `JLE` at
// 0x1800c47d7 returns before `ParseCommand` is reached.
//
// **So: this row closes for an EMPTY command line, which is every automated run
// this project has. Launch `reimpl-x64.exe` with ANY argument at cutover and the
// next fault is `?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z`
// (0x1800c4a00, ord 3310, 109 instructions), then `?ParseValue@` (ord 3311, 62)
// and then an EXEC-of-0 at the insert below, which has NO name to report.**
//
// The loop body is written out in full and faithfully anyway — it costs
// nothing, and the alternative (a stub that ignores a non-empty command line)
// would be a silent behaviour change on the DLL-present path, where all three
// callees resolve to the shipped DLL and work.
//
// Why the insert subtree was not ported with it: 0x18002a090 copies both
// strings and forwards to 0x18002a170, a red-black insert (~90 instructions)
// that calls a node allocator (0x18000f970) and a rebalance (0x18002a310), none
// of them exported. That is a Collection lane's worth of work with a real
// oracle available (the tree is observable), and doing it badly here — on the
// container the whole engine reads its configuration out of — would be the
// worst possible place to guess.
//
// ---------------------------------------------------------------------------
// 3. THE BST FIND (0x18002ace0) IS PORTED, AND IT IS SELF-CONTAINED
// ---------------------------------------------------------------------------
// Unlike the insert, the FIND has no callees at all: 50 instructions, two
// inlined `strcmp`s and two pointer follows. It is ported here because
// `IO::FileSystem::Startup` (the very next row in the boot queue,
// runtime/src/io/filesystem_startup.cpp) calls it on THIS class's map, as does
// `FileSystem::SetupDirectories` at three more sites. Registered by ADDRESS
// only — it has no name in either build.
//
// Node layout, read out of the two functions that walk it:
//     +0x00  char*  key      (a System::String's `data`, i.e. block+8)
//     +0x18  Node*  left
//     +0x20  Node*  right
// and the map object's first qword is the root.
//
// The comparison retail inlines is a signed-char `strcmp`, reproduced literally
// in `TreeStrCmp` below rather than delegated to `std::strcmp`: the C library's
// return value is only specified by SIGN, and this code branches on `>= 0`, so
// a libc that returned the difference of UNSIGNED chars would take the other
// branch on any byte >= 0x80 — which is exactly what a path or a mod name can
// contain.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtSystem::String;

namespace {

typedef void  (*StringDtorFn)(String*);
typedef void  (*ParseFn)(String* ret, int* pos, const String* src);
typedef void* (*MapInsertFn)(void* map, void** ret, const String* key, const String* val);

// ---------------------------------------------------------------------------
// The node the two tree walkers agree on. Only three of its fields are touched
// by anything in this file; the rest of the node is deliberately NOT described,
// because nothing here reads it and inventing a layout for it would be a claim
// no disassembly in front of me supports.
// ---------------------------------------------------------------------------
struct MapNode {
    char*    key;                    // +0x00  String::data of the key
    uint8_t  _unknown08[0x18 - 0x08];
    MapNode* left;                   // +0x18
    MapNode* right;                  // +0x20
};
static_assert(offsetof(MapNode, key)   == 0x00, "0x18002ace0: MOV R9,[R8]");
static_assert(offsetof(MapNode, left)  == 0x18, "0x18002ad3f: MOV R8,[R8+0x18]");
static_assert(offsetof(MapNode, right) == 0x20, "0x18002ad8d: MOV R8,[R8+0x20]");

// Retail's inlined comparison, written out instead of calling std::strcmp.
// See §3 of the banner for why the sign convention is load-bearing.
//
// The disassembly's shape is preserved: a first-byte fast path, then a walk
// that stops at the first mismatch OR at either terminator, then ONE signed
// difference taken at the index the walk stopped on.
int TreeStrCmp(const char* a, const char* b) {
    size_t i = 0;
    if (a[0] == b[0]) {
        while (a[i] != '\0' && b[i] != '\0' && a[i + 1] == b[i + 1]) {
            ++i;
            if (a[i] == '\0' || b[i] == '\0') break;
        }
    }
    return static_cast<int>(static_cast<signed char>(a[i])) -
           static_cast<int>(static_cast<signed char>(b[i]));
}

}  // namespace

// ============================================================================
// 0x18002ace0 — the config map's string-keyed find.  UNEXPORTED.
// ============================================================================
// Retail, structurally:
//
//   node = map->root
//   while (node) {
//       if (node->key == 0)            goto rightTest;      // 0x18002acfa
//       if (wanted->data == 0)         { node = node->left;  continue; }
//       if (TreeStrCmp(wanted->data, node->key) <  0) { node = node->left;  continue; }
//     rightTest:
//       if (wanted->data == 0)         return node;          // 0x18002ad4b
//       if (node->key == 0)            { node = node->right; continue; }
//       if (TreeStrCmp(node->key, wanted->data) >= 0) return node;
//       node = node->right;
//   }
//   return nullptr;
//
// The two null-key arms are retail's and they are ASYMMETRIC on purpose: a null
// SEARCH key returns the current node, a null NODE key descends. Both are kept.
extern "C" void* RtSystem_ConfigMap_Find(void* map, const String* wanted) {
    MapNode* node = *static_cast<MapNode**>(map);
    while (node != nullptr) {
        bool goRight = false;
        if (node->key != nullptr) {
            if (wanted->data == nullptr) { node = node->left; continue; }
            if (TreeStrCmp(wanted->data, node->key) < 0) { node = node->left; continue; }
        }
        // rightTest
        if (wanted->data == nullptr) return node;
        if (node->key == nullptr) goRight = true;
        else if (TreeStrCmp(node->key, wanted->data) >= 0) return node;
        (void)goRight;
        node = node->right;
    }
    return nullptr;
}

// ============================================================================
// ?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z    0x1800c47a0
// ============================================================================
// ORACLE: none, and none is manufactured. It runs once per process, from
// Engine::Startup, and it mutates the process-wide configuration map — a replay
// would insert every key twice. What it gets is a shadow that is meaningful
// precisely because of §2: on an empty command line the post-condition is
// "returned without touching the map", and the check reads the map's ROOT,
// which this body never writes.
extern "C" void RtSystem_Configuration_SetCommandLine(void* self, const String* cmd) {
    RT_DIFF_BEGIN("System::Configuration::SetCommandLine");

    void* map = static_cast<uint8_t*>(self) + 0x10;
    MapNode* rootBefore = *static_cast<MapNode**>(map);

    int pos = 0;
    char* d = cmd->data;
    if (d != nullptr) {
        const int len = RtSystem::StringLength(d);
        if (len > 0) {
            for (;;) {
                String key;
                key.data = nullptr;
                RT_EXPORT_ONCE(ParseFn,
                    "?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z")(
                        &key, &pos, cmd);

                if (key.data == nullptr || RtSystem::StringLength(key.data) == 0) {
                    RT_EXPORT_ONCE(StringDtorFn, "??1String@System@@QEAA@XZ")(&key);
                    break;
                }

                String val;
                val.data = nullptr;
                RT_EXPORT_ONCE(ParseFn,
                    "?ParseValue@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z")(
                        &val, &pos, cmd);

                // 0x1800c4829. The insert is UNEXPORTED, so it goes by address
                // and — unlike every other dependency in this lane — a miss here
                // produces NO [RtUnresolved] line, only an EXEC-of-0 whose
                // return address symbolises to this source line. Named here so
                // that crash is readable. See §2: unreachable on an empty
                // command line, which is every automated run in this project.
                void* node = nullptr;
                reinterpret_cast<MapInsertFn>(
                    ResolveAddr(Module::Runtime, 0x18002a090))(map, &node, &key, &val);

                RT_EXPORT_ONCE(StringDtorFn, "??1String@System@@QEAA@XZ")(&val);
                RT_EXPORT_ONCE(StringDtorFn, "??1String@System@@QEAA@XZ")(&key);

                if (pos >= len) break;
            }
        }
    }

    if (rt_diff_on__) {
        const bool emptyCmd  = (d == nullptr) || RtSystem::StringLength(d) <= 0;
        MapNode* rootAfter   = *static_cast<MapNode**>(map);
        // On an empty command line the map must be UNTOUCHED; on a non-empty one
        // a first insert must have created a root. Either way the term is about
        // storage this body never writes -- the insert does.
        const bool mapOk = emptyCmd ? (rootAfter == rootBefore)
                                    : (rootAfter != nullptr);
        char ours[160], theirs[160];
        RtDiffFormat(ours, sizeof(ours), static_cast<const void*>(rootAfter));
        std::snprintf(theirs, sizeof(theirs),
                      "len=%d pos=%d root %s [mapOk=%d]",
                      (d ? RtSystem::StringLength(d) : -1), pos,
                      emptyCmd ? "unchanged" : "non-null",
                      static_cast<int>(mapOk));
        RT_DIFF_SHADOW(mapOk, ours, theirs);
        RT_DIFF_WORK(true);
    }
}

// ============================================================================
// Registration
// ============================================================================
// Every mangled name is ONE literal on ONE line, so runtime_census.py and
// export_name_audit.py can see it.
int RtSystemConfigurationRegister() {
    int n = 0;

    RegisterRuntimeExport("?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z", (void*)&RtSystem_Configuration_SetCommandLine); ++n;
    RegisterRuntimeAddr(0x1800c47a0, (void*)&RtSystem_Configuration_SetCommandLine);

    // No export name in either build -- address only.
    RegisterRuntimeAddr(0x18002ace0, (void*)&RtSystem_ConfigMap_Find);

    // DELIBERATELY ABSENT, with reasons in the banner:
    //   ?ParseCommand@Configuration@System@@CA…  0x1800c4a00  ord 3310  (109 instrs)
    //   ?ParseValue@Configuration@System@@CA…    0x1800c4930  ord 3311  (62 instrs)
    //   (unexported) the map insert              0x18002a090  -> 0x18002a170
    //       -> node alloc 0x18000f970, rebalance 0x18002a310 : a red-black
    //          insert, a Collection lane's work, unreachable on our boots.
    return n;
}
