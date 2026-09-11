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
// RtString_GetLength / RtSystem_String_SubString / RtString_ReleaseInline, the
// three already-ported String primitives the two Parse bodies below need.
// Spelled "./string.h" rather than "string.h": this file lives in the same
// directory as that header, and the unqualified form would also shadow the C
// library's <string.h> for anything included after it.
#include "./string.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtSystem::String;

namespace {

typedef void  (*StringDtorFn)(String*);
typedef void  (*ParseFn)(String* ret, int* pos, const String* src);
typedef void* (*MapInsertFn)(void* map, void** ret, const String* key, const String* val);
// Wave 22 lane ERRCFG. THREE parameters: the hidden String return slot is the
// SECOND, between `this` and the key. See GetCommandLineString's banner for why
// omitting it is a wild write that still returns the right answer.
typedef String* (*GetCmdLineStrFn)(void* self, String* ret, const String* key);

// ---------------------------------------------------------------------------
// The node the two tree walkers agree on. Only three of its fields are touched
// by anything in this file; the rest of the node is deliberately NOT described,
// because nothing here reads it and inventing a layout for it would be a claim
// no disassembly in front of me supports.
// ---------------------------------------------------------------------------
struct MapNode {
    char*    key;                    // +0x00  String::data of the key
    char*    value;                  // +0x08  String::data of the value
    uint8_t  _unknown10[0x18 - 0x10];
    MapNode* left;                   // +0x18
    MapNode* right;                  // +0x20
};
static_assert(offsetof(MapNode, key)   == 0x00, "0x18002ace0: MOV R9,[R8]");
// +0x08 added 2026-09-11 by Wave 22 lane ERRCFG. Pinned by GetCommandLineString
// at 0x1800c533b (`MOV RCX,[RAX+0x8]`) immediately followed by `INC dword
// [RCX-0x4]` at 0x1800c534c -- a String refcount bump, so the qword at +0x08 is
// a System::String's `data`, not an arbitrary pointer. The remaining eight
// bytes of the node's head stay undescribed for the reason above.
static_assert(offsetof(MapNode, value) == 0x08, "0x1800c533b: MOV RCX,[RAX+0x8]");
static_assert(offsetof(MapNode, left)  == 0x18, "0x18002ad3f: MOV R8,[R8+0x18]");
static_assert(offsetof(MapNode, right) == 0x20, "0x18002ad8d: MOV R8,[R8+0x20]");

// Retail's inlined comparison, written out instead of calling std::strcmp.
// See §3 of the banner for why the sign convention is load-bearing.
//
// The disassembly's shape is preserved: a first-byte fast path, then a walk
// that stops at the first mismatch OR at either terminator, then ONE signed
// difference taken at the index the walk stopped on.
//
// THE INDEX IS THE WHOLE FUNCTION, and the first version of this got it wrong
// by one (integrator, 2026-09-08). Retail increments RDX BEFORE the comparison
// that decides whether to loop:
//
//   ad18:  TEST CL,CL                    ; a[i] == 0 ?      -> stop AT i
//          CMP  byte [RAX],0             ; b[i] == 0 ?      -> stop AT i
//          MOVZX ECX,[R11+RAX+1]         ; c = a[i+1]
//          INC  RAX / INC RDX            ; i <- i+1   <<<<< BEFORE the compare
//          CMP  CL,byte [RAX]            ; a[i] vs b[i]
//          JZ   ad18                     ; equal -> loop
//   ad31:  result = (signed)a[RDX] - (signed)b[RDX]
//
// so the loop EXITS ON A MISMATCH WITH RDX ALREADY POINTING AT IT. The earlier
// version tested `a[i+1] == b[i+1]` in the while condition and incremented only
// inside the body, so on the exiting (mismatching) comparison it never
// incremented and returned `a[i] - b[i]` at an index where the two bytes were
// known EQUAL. It therefore returned 0 for every pair of distinct strings that
// share a first byte -- which is every key in a settings map, since they are all
// prefixed by their section name.
//
// That made the find a "return the first node whose key starts with the same
// letter" — see the caller below for what it cost.
int TreeStrCmp(const char* a, const char* b) {
    size_t i = 0;
    if (a[0] == b[0]) {
        while (a[i] != '\0' && b[i] != '\0') {
            ++i;
            if (a[i] != b[i]) break;
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
// 0x1800c4a00 / 0x1800c4930 — ParseCommand and ParseValue.  Wave 9 lane ADDR.
// ============================================================================
// The banner's §2 listed these two as DELIBERATELY ABSENT and named them as the
// next fault at cutover for anyone who launches with an argument. They are
// closed here.
//
//   ?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z  ord 3310
//   ?ParseValue@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z    ord 3311
//
// Both names re-verified against Runtime-x64.dll's export directory with
// pefile before a line was written, and both addresses confirmed to be the
// export directory's own entries for those ordinals rather than a nearby label.
//
// ---------------------------------------------------------------------------
// THE SIGNATURE: `CA` STATICS RETURNING `String` BY VALUE
// ---------------------------------------------------------------------------
// `CA` is a private STATIC member, so there is no `this`. The return type is
// `?AVString@2@` — a class BY VALUE — so MSVC passes a hidden return slot as
// the first argument and returns that same pointer in RAX:
//
//     RCX = String* ret        (caller-allocated, uninitialised)
//     RDX = int*    pos        (the `AEAH`, an int&)
//     R8  = const String* src  (the `AEBV32@`)
//     RAX = ret                (0x1800c49df / 0x1800c4b36: MOV RAX,R14)
//
// The return type below is `String*` and the body returns `ret` for exactly
// that reason. Declaring it `void` would compile and would even work for every
// caller in this tree (all of them discard the result), but it would stop
// setting RAX — and a hidden return slot a signature omits is the wild-write
// shape reference_hidden_retslot_wild_write.md records. The two typedefs
// already in the tree (engine_funcs.h:323/329 and `ParseFn` above) both model
// the slot correctly, so nothing had to be corrected.
//
// ---------------------------------------------------------------------------
// THREE DETAILS THAT ARE FAITHFUL AND MUST NOT BE "FIXED"
// ---------------------------------------------------------------------------
// 1. `GetLength` IS RE-CALLED ON EVERY ITERATION. Retail reloads it inside each
//    loop (0x1800c4989, 0x1800c49b9, ...) rather than hoisting it. Reproduced
//    literally. It cannot change under us — nothing here writes `src` — so this
//    is about shape, not behaviour, and hoisting it would be the first step of
//    a rewrite rather than a port.
//
// 2. THE `src[i]` READS AT 0x1800c4a92 AND 0x1800c4b08 ARE UNGUARDED, and that
//    is CORRECT, not a missing bounds check. Both can run with `i == length`.
//    A System::String's buffer is `length + 1` bytes with a NUL written by
//    every producer (types/system/String.h), so `src[length]` is a legal read
//    of '\0' — which is neither '/' nor ' ' nor ':', so both tests fall the
//    right way. A guard here would not "make it safe", it would change the
//    result at end-of-string. CLAUDE.md's rule applies directly: retail's
//    unguarded deref is load-bearing.
//
// 3. THE EMPTY RETURN WRITES A NULL `data` DIRECTLY. At 0x1800c4a98 retail
//    stores RBP (which is 0) into [R14] and jumps to the epilogue — it does NOT
//    call a String constructor. Reproduced as `ret->data = nullptr`. A port
//    that default-constructs a String here would allocate a 9-byte block retail
//    never allocates, and `SetCommandLine` above would then release a block
//    that on the retail path never existed.
//
// ---------------------------------------------------------------------------
// DEPENDENCIES: BOTH ARE ALREADY PORTED, SO THIS CLOSES DOORS WITHOUT OPENING
// ---------------------------------------------------------------------------
//   ?GetLength@String@System@@QEBAHXZ        0x1800063c0 ord 2750 — string.cpp
//   ?SubString@String@System@@QEBA?AV12@HH@Z 0x1800c9230 ord 4124 — string.cpp,
//       registered by NAME and by ADDRESS (string.cpp:1143 / :1183)
// They are called here as our own C++ functions, so no new resolve door of
// either kind is created. Measured: this closes 2 ADDRESS doors
// (engine_funcs.cpp:1727/1729) and 2 NAME doors (the two RT_EXPORT_ONCE sites
// in SetCommandLine above) and opens ZERO.
//
// `SubString`'s end index is INCLUSIVE (setupdir_helpers.cpp:68 records the
// same trap), which is why both bodies pass `pos - 1` / `end - 1`, not `pos`.
// ============================================================================

namespace {

// Retail's blank test, inlined at four sites across the two functions.
inline bool IsBlank(char c) { return c == ' ' || c == '\t'; }

// The oracle's verdict for a returned String. Compares the two `data` pointers
// as STRINGS, never as pointers — they are separate allocations by
// construction — and compares only bytes both sides actually wrote: the length
// and `length` characters. The NUL and the refcount are excluded, which is
// what reference_oracle_must_not_compare_unwritten_bytes.md is about.
bool SameString(const String* a, const String* b) {
    if ((a->data == nullptr) != (b->data == nullptr)) return false;
    if (a->data == nullptr) return true;
    const int la = RtSystem::RtString_GetLength(a);
    const int lb = RtSystem::RtString_GetLength(b);
    if (la != lb) return false;
    for (int i = 0; i < la; ++i)
        if (a->data[i] != b->data[i]) return false;
    return true;
}

}  // namespace

// ?ParseValue@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z  0x1800c4930
//
// Skip blanks, then take the run up to the next blank.
extern "C" String* RtSystem_Configuration_ParseValue(String* ret, int* pos,
                                                     const String* src) {
    RT_DIFF_BEGIN("System::Configuration::ParseValue");
    const int posIn = *pos;

    while (*pos < RtSystem::RtString_GetLength(src) && IsBlank(src->data[*pos]))
        ++*pos;

    const int start = *pos;                                  // 0x1800c4992

    while (*pos < RtSystem::RtString_GetLength(src) && !IsBlank(src->data[*pos]))
        ++*pos;

    RtSystem::RtSystem_String_SubString(src, ret, start, *pos - 1);   // inclusive

    // ORACLE. Pure in the only sense that matters: given `src` and an input
    // `pos` it writes a fresh String and a new `pos` and touches no process
    // state, so it is exactly replayable. `pos` is an ALIASED in/out parameter,
    // so the original is replayed from the SNAPSHOT `posIn`, never from the
    // value our body just advanced
    // (reference_oracle_must_snapshot_aliased_inputs.md).
    if (rt_diff_on__) {
        int theirPos = posIn;
        String theirRet;
        theirRet.data = nullptr;
        RT_ORIGINAL_ONCE(ParseFn,
            "?ParseValue@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z")(
                &theirRet, &theirPos, src);

        const bool eq = (theirPos == *pos) && SameString(ret, &theirRet);
        // The verdict is TWO terms, so both are printed on either outcome and
        // the failing one is readable without a second run
        // (reference_shadow_predicate_must_name_its_failing_term.md).
        char ours[192], theirs[192];
        std::snprintf(ours, sizeof(ours), "pos=%d str=\"%.80s\"",
                      *pos, ret->data ? ret->data : "(null)");
        std::snprintf(theirs, sizeof(theirs), "pos=%d str=\"%.80s\"",
                      theirPos, theirRet.data ? theirRet.data : "(null)");
        RT_DIFF_SHADOW(eq, ours, theirs);

        // The oracle's String is a real allocation on the DLL's heap. Release
        // it through the same inlined idiom retail uses, or the diff run leaks
        // one block per call.
        RtSystem::RtString_ReleaseInline(&theirRet);
    }

    // EFFECTIVENESS CONTROL: how many calls actually consumed input. A run
    // whose ParseValue rows are all `work=0` has proved nothing, however green
    // (reference_probe_needs_an_effectiveness_control.md).
    RT_DIFF_WORK(*pos != posIn);
    return ret;
}

// ?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z  0x1800c4a00
//
// Skip blanks, scan to '/', and if there is one take the run after it up to a
// blank or ':'. If there is no '/', return an empty String.
extern "C" String* RtSystem_Configuration_ParseCommand(String* ret, int* pos,
                                                       const String* src) {
    RT_DIFF_BEGIN("System::Configuration::ParseCommand");
    const int posIn = *pos;

    while (*pos < RtSystem::RtString_GetLength(src) && IsBlank(src->data[*pos]))
        ++*pos;

    while (*pos < RtSystem::RtString_GetLength(src) && src->data[*pos] != '/')
        ++*pos;

    // 0x1800c4a92. Unguarded by design — detail 2 above. When the scan ran off
    // the end this reads the NUL, which is not '/', so the empty return is
    // taken, which is precisely what retail does.
    if (src->data[*pos] != '/') {
        ret->data = nullptr;                                 // detail 3
        RT_DIFF_WORK(false);
        return ret;
    }
    ++*pos;                                                  // step over the '/'

    while (*pos < RtSystem::RtString_GetLength(src) && IsBlank(src->data[*pos]))
        ++*pos;

    const int start = *pos;                                  // 0x1800c4ad2

    while (*pos < RtSystem::RtString_GetLength(src) &&
           src->data[*pos] != ' ' && src->data[*pos] != ':')
        ++*pos;

    // 0x1800c4b02. `end` is where the run stopped. The DELIMITER is CONSUMED
    // (pos = end + 1) but is NOT part of the returned string. Reading
    // src->data[end] is the second unguarded read of detail 2.
    const int end = *pos;
    if (src->data[end] == ' ' || src->data[end] == ':')
        *pos = end + 1;

    RtSystem::RtSystem_String_SubString(src, ret, start, end - 1);   // inclusive

    if (rt_diff_on__) {
        int theirPos = posIn;
        String theirRet;
        theirRet.data = nullptr;
        RT_ORIGINAL_ONCE(ParseFn,
            "?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z")(
                &theirRet, &theirPos, src);

        const bool eq = (theirPos == *pos) && SameString(ret, &theirRet);
        char ours[192], theirs[192];
        std::snprintf(ours, sizeof(ours), "pos=%d str=\"%.80s\"",
                      *pos, ret->data ? ret->data : "(null)");
        std::snprintf(theirs, sizeof(theirs), "pos=%d str=\"%.80s\"",
                      theirPos, theirRet.data ? theirRet.data : "(null)");
        RT_DIFF_SHADOW(eq, ours, theirs);
        RtSystem::RtString_ReleaseInline(&theirRet);
    }

    RT_DIFF_WORK(*pos != posIn);
    return ret;
}

// ============================================================================
// ?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z
//                                                  retail 0x1800c5300  ord 2495
// ============================================================================
// Wave 22 lane ERRCFG.
//
// THE SIGNATURE IS THREE REGISTERS, NOT TWO. `?AVString@2@` in the mangling is
// a `System::String` returned BY VALUE, so MSVC inserts a hidden return slot as
// the first ordinary argument:
//
//     RCX = this        RDX = String* ret        R8 = const String& key
//
// and retail says so on its first three instructions -- `MOV [RSP+0x10],RDX`
// parks the return slot in its home space, `MOV RSI,RDX` keeps it, and
// `MOV RDX,R8` shuffles the real argument down before the find. The function
// returns `RAX = RSI`, the slot it was handed. An `fn_*` typedef that omitted
// the slot would put `key` in RDX, hand the find a `String*` that is really a
// `String**`, and then write the answer through whatever the caller happened to
// leave in R8 -- a wild write that would STILL appear to return the right
// answer, because RAX comes back correct either way
// (reference_hidden_retslot_wild_write.md). The typedef above spells all three.
//
// WHAT IT DOES
// ------------
//     node = ConfigMap_Find(this + 0x10, key);        // 0x18002ace0, ported above
//     temp = node ? node->value : nullptr;            // 0x1800c533b
//     if (temp) ++temp[-4];                           // 0x1800c534c  temp's ref
//     ret->data = nullptr;                            // 0x1800c537f
//     if (temp) { ret->data = temp; ++temp[-4]; }     // 0x1800c538a  ret's ref
//     <release temp>                                  // 0x1800c53a9 / 0x1800c53d5
//     return ret;
//
// The refcount goes +2 then -1, never through zero, and the net is the +1 the
// returned String owns. The transient +2 is written out rather than folded into
// a single AddRef: nothing in this project can observe the difference, but
// those two arms ARE the arms retail compiled, and a reader diffing this
// against 0x1800c5300 should find them.
//
// THE TWO `TEST DIL,n` ARMS AND THE GARBAGE THEY GUARD. Retail keeps a
// destructor-state byte at [RSP+0x20] (1 = the found-arm temporary is live,
// 2 = the not-found-arm temporary is live, |4 = the return value is live), and
// each arm loads the OTHER arm's UNINITIALISED stack slot into RCX/RBX before
// falling into the common tail. That is not a bug and it is not transcribed:
// the state bit guarding each release is clear in the arm that did not create
// the temporary, so the garbage is never dereferenced. Modelled here as one
// `temp`, which is what that state machine computes.
//
// ORACLE -- a real differential one, and it needs no snapshot.
// -----------------------------------------------------------
// This body is a PURE READ. It does not touch the map, so unlike SetCommandLine
// a replay is not a double insert, and unlike ParseValue there is no aliased
// in/out parameter to snapshot. The shipped body is run afterwards into a
// second String and the two `data` POINTERS are compared -- as pointers, not as
// text, which is the SHARPER test here precisely because neither side
// allocates: both must hand back the identical buffer that lives in the map. A
// text comparison would pass on two different nodes holding equal strings,
// which is exactly the failure the ported find had once already (see §3).
//
// The replay takes a third reference, released afterwards through retail's own
// idiom so the refcount ends where our body left it. Without that release a
// diff run leaks one reference per call and no buffer in the configuration map
// could ever be freed.
extern "C" String* RtSystem_Configuration_GetCommandLineString(
        void* self, String* ret, const String* key) {
    RT_DIFF_BEGIN("System::Configuration::GetCommandLineString");

    MapNode* node = static_cast<MapNode*>(
        RtSystem_ConfigMap_Find(static_cast<uint8_t*>(self) + 0x10, key));

    // 0x1800c533b: the found node's VALUE, and the temporary's reference.
    char* temp = (node != nullptr) ? node->value : nullptr;
    if (temp != nullptr) ++RtSystem::StringRefCount(temp);

    // 0x1800c537f: the slot is zeroed FIRST and unconditionally, then adopted.
    ret->data = nullptr;
    RtSystem::RtString_AdoptInline(ret, temp);

    // 0x1800c53a9 / 0x1800c53d5: the temporary dies. Through the shared
    // primitive, so the freeing branch's dangling-pointer asymmetry (String.h)
    // is reproduced rather than re-typed.
    {
        String tmpObj;
        tmpObj.data = temp;
        RtSystem::RtString_ReleaseInline(&tmpObj);
    }

    if (rt_diff_on__) {
        String theirRet;
        theirRet.data = nullptr;
        RT_ORIGINAL_ONCE(GetCmdLineStrFn,
            "?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z")(
                self, &theirRet, key);

        const bool eq = (ret->data == theirRet.data);
        char ours[128], theirs[128];
        std::snprintf(ours, sizeof(ours), "data=%p \"%.60s\"",
                      (void*)ret->data, ret->data ? ret->data : "(null)");
        std::snprintf(theirs, sizeof(theirs), "data=%p \"%.60s\"",
                      (void*)theirRet.data, theirRet.data ? theirRet.data : "(null)");
        RT_DIFF_SHADOW(eq, ours, theirs);

        RtSystem::RtString_ReleaseInline(&theirRet);
    }

    // EFFECTIVENESS CONTROL. A run in which every lookup MISSED has exercised
    // the null arm only and said nothing about the arm that matters; `work`
    // counts the hits (reference_probe_needs_an_effectiveness_control.md).
    RT_DIFF_WORK(ret->data != nullptr);
    return ret;
}

// ============================================================================
// ??1Configuration@System@@QEAA@XZ                       retail 0x1800c5170
// ============================================================================
// Wave 22 lane ERRCFG. Thirteen instructions, two of which are the whole body:
//
//   1800c5186  ADD  RCX,0x10
//   1800c518f  CALL 0x18000f7d0        ; Set<Pair<String,String>>::Clear(this+0x10)
//   1800c5195  MOV  RCX,RBX
//   1800c519d  JMP  0x180010ab0        ; ~Array(+elem)(this)      -- TAIL CALL
//
// The name was checked against the export directory with pefile before a line
// was written: `??1` is the destructor and `QEAA` is the NON-VIRTUAL spelling,
// and it is ordinal 556 at 0x1800c5170. `??1...UEAA@XZ` does not exist in this
// DLL and would have resolved NULL in silence
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
//
// NOT PRESCRIBED, READ. `reference_a_written_recipe_can_be_wrong.md` records a
// wave that ported two dtors exactly as prescribed and swapped a body that
// destroys EVERY element for one that destroys only the FIRST, so both callees
// were opened rather than assumed:
//
//   0x18000f7d0  loads [this]; on non-null it calls 0x18000f890 TWICE, once
//                with [node+0x18] and once with [node+0x20] -- i.e. it recurses
//                down BOTH subtrees -- then frees the node and zeroes
//                {ptr, int}. It is the whole container, not one element.
//   0x180010ab0  reads the COUNT from [this+0x8] and the base from [this] and
//                hands both to 0x180011010 (the per-element loop), then frees
//                the block under the allocator lock and zeroes {ptr, ptr}.
//                Again the whole array.
//
// BOTH ARE ALREADY PORTED AND REGISTERED BY ADDRESS IN THIS TREE, which is why
// this row is thirteen lines rather than a Collection lane. That was the
// STATED blocker on this row and re-checking it first is the point
// (reference_holdback_blockers_are_stale_check_them_first.md):
//
//   0x18000f7d0 -> rtsetclear::Clear               runtime/src/collection/set_clear.cpp:212
//   0x180010ab0 -> RtColl_ArrayDtorElem_180010ab0  runtime/src/collection/array_destroy.cpp:902
//
// so `ResolveAddr(Module::Runtime, ...)` -- which consults the port registry
// BEFORE the DLL (reference_resolveaddr_consults_port_registry_first.md) --
// reaches OUR bodies today and at cutover alike. This body therefore adds no
// new dependency on Runtime-x64.dll.
//
// THE THIRD SUB-OBJECT IS NOT DESTROYED, AND THAT IS RETAIL'S CHOICE.
// ??0Configuration (engine_lifecycle.cpp) initialises THREE {pointer, int}
// pairs -- at +0x00, +0x10 and +0x20. This dtor touches +0x10 and +0x00 and
// leaves +0x20 alone. That is a retail leak of the same family as the two
// already flagged inside Engine::Shutdown (Input::Device and Audio::Device),
// and it is reproduced rather than tidied: "fixing" it would free a block
// retail still considers live, which is strictly worse than the leak.
//
// ORACLE: none, and none is manufactured. A destructor's entire output is that
// its subject is gone, so a replay would run the same two frees over the same
// freed blocks -- a double free, not a comparison. What it gets instead is the
// call-site counter: engine_lifecycle.cpp's ported Engine::Shutdown reaches
// this name through RT_EXPORT_ONCE, i.e. through a DOOR, so registering moves
// that call to this body, and `work` counts the objects actually destroyed.
extern "C" void RtSystem_Configuration_Dtor(void* self) {
    RT_DIFF_BEGIN("System::Configuration::~Configuration");

    typedef void (*CleanupFn)(void*);

    // 0x1800c518f -- the string->string map at +0x10.
    if (CleanupFn setClear = reinterpret_cast<CleanupFn>(
            ResolveAddr(Module::Runtime, 0x18000f7d0)))
        setClear(static_cast<uint8_t*>(self) + 0x10);

    // 0x1800c519d -- the tail call, on `this` itself.
    if (CleanupFn arrayDtor = reinterpret_cast<CleanupFn>(
            ResolveAddr(Module::Runtime, 0x180010ab0)))
        arrayDtor(self);

    RT_DIFF_WORK(true);
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

    // CLOSED 2026-09-08 by Wave 9 lane ADDR. Both by NAME (the two
    // RT_EXPORT_ONCE sites in SetCommandLine above) and by ADDRESS
    // (engine_funcs.cpp:1727 and :1729, which the address-door census listed as
    // unregistered). BOTH DOORS, ALWAYS -- registering only the name would
    // leave those two ResolveAddr sites resolving to null at cutover, which is
    // the shape reference_by_address_door_dies_at_cutover.md records.
    RegisterRuntimeExport("?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z", (void*)&RtSystem_Configuration_ParseCommand); ++n;
    RegisterRuntimeAddr(0x1800c4a00, (void*)&RtSystem_Configuration_ParseCommand);

    RegisterRuntimeExport("?ParseValue@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z", (void*)&RtSystem_Configuration_ParseValue); ++n;
    RegisterRuntimeAddr(0x1800c4930, (void*)&RtSystem_Configuration_ParseValue);

    // ---- Wave 22 lane ERRCFG -----------------------------------------------
    // BOTH DOORS for both rows. The NAME closes the census row and moves
    // filesystem_setupdirs_win.cpp's two RT_EXPORT_ONCE call sites and
    // engine_lifecycle.cpp's Engine::Shutdown; the ADDRESS closes the
    // `ResolveAddr(Module::Runtime, ...)` door that would otherwise resolve to
    // null at cutover (reference_by_address_door_dies_at_cutover.md).
    RegisterRuntimeExport("?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z", (void*)&RtSystem_Configuration_GetCommandLineString); ++n;
    RegisterRuntimeAddr(0x1800c5300, (void*)&RtSystem_Configuration_GetCommandLineString);

    RegisterRuntimeExport("??1Configuration@System@@QEAA@XZ", (void*)&RtSystem_Configuration_Dtor); ++n;
    RegisterRuntimeAddr(0x1800c5170, (void*)&RtSystem_Configuration_Dtor);

    // DELIBERATELY ABSENT, with reasons in the banner:
    //   (unexported) the map insert              0x18002a090  -> 0x18002a170
    //       -> node alloc 0x18000f970, rebalance 0x18002a310 : a red-black
    //          insert, a Collection lane's work.
    //
    //          CORRECTED 2026-09-08 by Wave 9 lane MOD: this line used to end
    //          "unreachable on our boots", which is true of THIS FILE'S call
    //          site (an empty command line, see the banner's §2) but NOT of the
    //          address. persistence_registry_win.cpp:266 reaches the same
    //          insert from ReadSettings, which every boot runs, and
    //          persistence.cpp:59 wraps it as CallMapInsert. A reachability
    //          claim belongs to a CALL SITE, not to an address -- the same
    //          scope error reference_never_written_needs_by_whom.md records for
    //          "never written", moved one level up. Whoever picks this row up
    //          must not read "unreachable" and defer it. See WAVE9_LANE_MOD.md.
    return n;
}
