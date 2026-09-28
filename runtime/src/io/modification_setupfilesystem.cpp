// ============================================================================
// runtime/src/io/modification_setupfilesystem.cpp — BATCH 5 (2026-09-12)
//   `?SetupFileSystem@Modification@IO@@QEAAXXZ`
//   retail 0x1800aebb0, ordinal 4030, 2617 bytes / 709 instructions.
// ============================================================================
//
//     void IO::Modification::SetupFileSystem()
//
// APPLY THE MOD LIST. Called when the mod set has changed (`_dirty`), it
// rebuilds the packfile stack from the enabled mods and then RELOADS exactly
// those already-loaded resources whose compiled asset has moved to a different
// packfile — leaving everything else alone. It is the whole of "Ok" in the mods
// menu, and it runs at boot on any install with mods enabled, which is why it
// is on the cutover closure's critical path.
//
// Shape, in five acts:
//
//   1. SNAPSHOT      For every loaded `Object::External`, record
//                    {compiledPath -> packfileId} in a stack-local
//                    `Collection::Set<Pair<Filename,uint32>>`.
//   2. RESTACK       `FileSystem::ClosePackages()`, clear the user-pack list,
//                    refill it from the ENABLED mods in REVERSE order (writing
//                    each mod's index into `Mod::order` on the way),
//                    `FileSystem::LoadPackages()`.
//   3. DIFF          Walk the externals again. For each, look its compiled path
//                    up in the snapshot; if the packfile id CHANGED:
//                      * the asset still exists (in a packfile, or loose on
//                        disk)  -> queue {typeId, External*, path} for reload
//                      * it does not                   -> `ForceUnload` it
//   4. RELOAD        Per queued entry: virtual slot 7 on the object, then
//                    re-run its type's IN-PLACE FACTORY over the same storage,
//                    then re-point `_path` and `External::Load` it.
//   5. TEARDOWN      `_dirty = false`; release the queue and the snapshot.
//
// ---------------------------------------------------------------------------
// WHAT THIS PORT NEEDED, AND WHAT IT DID NOT OPEN
// ---------------------------------------------------------------------------
// Every callee is ALREADY OURS or is ported in this same commit. Enumerated
// rather than asserted (reference_porting_can_grow_the_cutover_surface.md):
//
//   0x1800be9d0 FindNext           ported, external.cpp
//   0x180006250 External::GetPath  ported, external.cpp
//   0x1800bec10 GetCompiledPath    ported, external_compiled.cpp
//   0x1800ab050 GetPackfileId      ported, filesystem.cpp
//   0x1800abab0 ClosePackages      ported, filesystem_packages.cpp
//   0x1800ab460 LoadPackages       ported, filesystem_packages.cpp
//   0x1800ab180 ExistsInPackfile   ported, filesystem.cpp
//   0x1800ad7a0 (not reached here)
//   0x1800bc620 External::Load     ported, external_load.cpp
//   0x1800c6970 AdjustDelimiters   ported, filename.cpp
//   0x1800cab50 WideString ctor    ported, widestring.cpp
//   0x1800098d0 ~Array<Filename>   ported, modification.cpp
//   0x180026880 Array<Filename>::SetSize  ported, filesystem_find_win.cpp
//   0x1800beec0 ForceUnload        ported THIS COMMIT, object/external_forceunload.cpp
//   0x180027380 Set::Insert        ported THIS COMMIT, collection/set_avl_pathid.cpp
//   0x180027970 Set subtree free   ported THIS COMMIT, collection/set_avl_pathid.cpp
//   0x180028cd0 Array SetSize      ported THIS COMMIT, collection/array.cpp
//   0x1800c7ad0 MemFree            ported, system/allocator.cpp
//
// The remaining callees are VIRTUAL and are the objects' own: slot 2
// (`GetTypeId`) and slot 7 on `Object::External`, and `BaseTypeInfo::
// inPlaceFactoryFn` (+0x28) on the type record. Those are dispatch, not doors.
//
// ---------------------------------------------------------------------------
// THE SNAPSHOT'S COMPARATOR IS CASE-SENSITIVE
// ---------------------------------------------------------------------------
// Act 3's descent is INLINED here (0x1800aee50..0x1800aeeef) and is the same
// plain signed `strcmp` act 1's `Set::Insert` uses — NOT the `_stricmp` the
// resource tree and the packfile index use. Both sides of the comparison are
// `GetCompiledPath` outputs from the same `External::GetPath`, so they agree
// by construction; the sensitivity still has to be right, because a
// case-insensitive descent would take a DIFFERENT PATH through a tree that was
// built with a case-sensitive one and could miss an entry that is present. It
// is transcribed in `SnapshotFind` below from this body's own listing.
//
// ---------------------------------------------------------------------------
// THE RELOAD IS `inPlaceFactoryFn`, NOT `factoryFn`
// ---------------------------------------------------------------------------
//   1800af330  MOV RAX,[RAX+0x10]     node + 0x10 == TypeRecord::info
//   1800af337  CALL [RAX+0x28]        BaseTypeInfo::inPlaceFactoryFn(storage)
//                                     with RCX = the EXISTING External*
// `+0x20` (the allocating factory) would return a NEW object and leak the old
// one, and every pointer the rest of the engine holds to the old one would go
// stale. Retail re-constructs IN PLACE so the identity survives the reload,
// which is the entire reason a mod change does not invalidate live handles.
// `BaseTypeInfo::inPlaceFactoryFn` at +0x28 is pinned in
// runtime/types/object/TypeInfo.h.
//
// THE TYPE LOOKUP'S MISS ARM PRODUCES NULL AND RETAIL DEREFERENCES IT
// (0x1800af32e `XOR EAX,EAX` then 0x1800af330 `MOV RAX,[RAX+0x10]`). Faithful;
// no guard is added. The queue is built from live objects whose type ids came
// from their own vtables, so a miss means the type tree has lost a type that
// exists — a state the engine does not survive anyway.
//
// ---------------------------------------------------------------------------
// ACT 2 RUNS THE MOD LIST BACKWARDS, AND THE INDEX IT WRITES IS THE LOOP INDEX
// ---------------------------------------------------------------------------
//   1800aed49  EBX = _count - 1;  RDI = EBX << 7   (sizeof(Mod) == 0x80)
//   1800aed64  MOV [RDI+RAX+0x20],EBX              Mod::order = i
//   1800aed68  CMP byte [RDI+RAX+0x50],0           Mod::enabled
//   1800aed7e  Array<Filename>::SetSize(fs->userPackFilenames, count+1,
//                                       &mods[i].file)
// So the LAST mod in the list is appended FIRST, i.e. the pack stack ends up in
// reverse list order, and `Mod::order` is re-stamped on EVERY mod (enabled or
// not) as a side effect. Both are retail's and both are visible to the player:
// the stack order is mod precedence.
//
// ---------------------------------------------------------------------------
// ACT 3'S "DOES IT STILL EXIST" TEST IS TWO GATES, NOT ONE
// ---------------------------------------------------------------------------
//   1800aef90  if (fs->existsInPackfileGate && ExistsInPackfile(p)) -> RELOAD
//   1800aefab  if (fs->platformExistsGate) {
//                  WideString w(p); ok = GetFileAttributesW(w) != -1;
//                  release w;  if (ok) -> RELOAD
//              }
//              otherwise -> ForceUnload
// This is `FileSystem::Exists` OPEN-CODED with its two gate bytes (+0x18,
// +0x19, both named in runtime/types/io/FileSystem.h from `Exists`'s own
// `CMP byte [RCX+0x18],0`). It is NOT a call to `Exists`, and the difference
// matters: when BOTH gates are off, `Exists` would be asked and this code is
// not — it goes straight to `ForceUnload`, unloading everything whose packfile
// id moved. Transcribed as written.
//
// ---------------------------------------------------------------------------
// WHY THIS IS **NOT** A `_win` FILE EVEN THOUGH IT CALLS GetFileAttributesW
// ---------------------------------------------------------------------------
// PLAN.md §4.1 decision 1 draws the platform seam where the ORIGINAL tree draws
// it, and `IO\Modification.cpp` has no `_win` twin: retail inlined one Win32
// call into an otherwise platform-neutral body. Splitting the whole function
// into a `_win` file to carry one `GetFileAttributesW` would move 700
// instructions of portable logic behind the seam. Instead the one call is
// isolated in `PlatformFileExists` below, which is the only thing in this file
// that includes <windows.h> — the same treatment `Engine::Startup`'s single
// registry probe gets.
//
// ---------------------------------------------------------------------------
// ORACLE — NONE, and the refusal is structural
// ---------------------------------------------------------------------------
// The function CLOSES AND REOPENS EVERY PACKFILE and then destroys and
// re-constructs live objects in place. Running the shipped body a second time
// for comparison would do all of that again on top of our result — and the
// second pass's snapshot would be taken from the state the first pass left, so
// its diff would be EMPTY and the comparison vacuously green. Worse, act 4 is
// not idempotent on a failed reload: an object re-constructed in place twice
// has been through its own destructor twice.
//
// What a boot checks, and it is not weak: this function runs whenever the mod
// list is dirty, and a defect anywhere in it is immediately visible as the
// wrong packfile stack (wrong assets), a resource that vanished (a wrong
// ForceUnload), or a crash in act 4 (a wrong factory slot). The
// `BANISHED_AUTOMENU_BUTTON=7` mods route in CLAUDE.md drives it end to end.
//
// UNVERIFIED ON AN UNMODDED BOOT: with `_dirty` false the body returns at
// 0x1800aebdb before doing anything, so a plain autostart run reaches the first
// instruction and nothing else. `calls=0` is not verification
// (reference_uncalled_port_is_not_verified.md).
// ============================================================================

#include <windows.h>          // GetFileAttributesW ONLY — see the banner.

#include <cstddef>
#include <cstdint>
#include <new>                // placement new -- see ReloadEntry::file below

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/Modification.h"
// TypeInfo.h MUST come before External.h. External.h:343 includes it from
// INSIDE `namespace RtObject`, so if it is first seen through that include the
// guard leaves every name as `RtObject::RtObject::TypeNode`. Including it here
// at global scope first makes the nested include a no-op and the names come out
// as `RtObject::TypeNode`.
#include "../../types/object/TypeInfo.h"
#include "../../types/object/External.h"
#include "../../types/system/Filename.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"

// Declared locally rather than including standalone/resolve.h; byte-identical
// to the copies in array.cpp and persistence_savelist_job.cpp.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtString_ReleaseInline;
using RtSystem::Sys;      // the transitional POD->class bridge (pod_bridge.h)

namespace {

// ---------------------------------------------------------------------------
// The snapshot's element and node. The Set is a STACK LOCAL of this function
// ([RBP-0x49] root, [RBP-0x41] count), built by 0x180027380 and torn down
// in-line at the end, so both layouts are this file's business.
// ---------------------------------------------------------------------------
struct PathIdNode {
    char*       key;      // +0x00  compiled path (refcounted System::Filename)
    uint32_t    value;    // +0x08  packfile id
    uint32_t    _pad0c;
    PathIdNode* parent;   // +0x10
    PathIdNode* left;     // +0x18
    PathIdNode* right;    // +0x20
    int16_t     balance;  // +0x28
    int16_t     height;   // +0x2a
    uint32_t    _pad2c;
};
static_assert(offsetof(PathIdNode, value) == 0x08, "0x1800aef12 CMP [RBX+0x8],EAX");
static_assert(offsetof(PathIdNode, left)  == 0x18, "0x1800aee9c MOV RBX,[RBX+0x18]");
static_assert(offsetof(PathIdNode, right) == 0x20, "0x1800aeeeb MOV RBX,[RBX+0x20]");
static_assert(sizeof(PathIdNode) == 0x30, "node alloc at 0x1800278c0 asks for 0x30");

struct PathIdSet {
    PathIdNode* root;   // [RBP-0x49]
    int32_t     count;  // [RBP-0x41]
    int32_t     _pad;
};

// The reload queue's element — `{uint32 typeId, External*, Filename}`, stride
// 0x18, built at [RBP-0x71]. THE FOUR BYTES AT +0x04 ARE NEVER WRITTEN by
// anybody (retail's first store is a DWORD and the next is at +0x08); the
// matching `kSpec_ModExternal18` in array.cpp declares the same window.
// `file` STAYS the POD (`RtSystem::Filename`), not `System::Filename`: the
// array holding these is grown and appended to by `queueAppend`
// (0x180028cd0), a raw byte-copy resolve door with no idea what a C++ object
// is, so a member with a destructor would be double-released the moment that
// copy and this struct's own local both thought they owned the buffer. Retail
// itself treats it that way -- see the ENDGAME B.3.1 note on the queueing
// code below, which promotes `entry.file` to the class only for the span
// where IT constructs and releases it.
struct ReloadEntry {
    uint32_t             typeId;    // +0x00
    uint32_t             _hole04;   // +0x04 — NEVER WRITTEN
    RtObject::External*  external;  // +0x08
    Filename             file;      // +0x10
};
static_assert(offsetof(ReloadEntry, external) == 0x08, "0x1800af058 MOV [RBP-0x69],R14");
static_assert(offsetof(ReloadEntry, file)     == 0x10, "0x1800af064 MOV [RBP-0x61],R13");
static_assert(sizeof(ReloadEntry) == 0x18, "0x180028d2d stride, cap*3*8");

struct ReloadArray {
    ReloadEntry* data;      // [RSP+0x30]
    int32_t      size;      // [RSP+0x38]
    int32_t      capacity;
};

// ---- the doors --------------------------------------------------------
// ENDGAME B.3.1 step 2: these are file-local typedefs for calls OUT through a
// resolve door, not entry-point signatures, so they name the classes that
// occupy the same eight bytes (filesystem_setupdirs_win.cpp's exemplar
// treatment of local Fn typedefs). The callee keeps the retail POD shape; the
// `reinterpret_cast<...Fn>` at each call site is the one that was always
// there. `AdjustDelimitersFn` is gone -- every call site below was the tail of
// a mutating Filename entry point and is now spelled as the class's own ctor
// / operator= (Filename.h's DELIMITERS note).
typedef const RtObject::External* (*FindNextFn)(const void*, uint32_t,
                                                const RtObject::External*);
typedef System::Filename* (*GetPathFn)(const void*, System::Filename*);
typedef System::Filename* (*GetCompiledPathFn)(System::Filename*, const System::Filename*);
typedef uint32_t  (*GetPackfileIdFn)(const void*, const System::Filename*);
typedef void      (*NoArgFn)(void*);
typedef bool      (*ExistsInPackfileFn)(const void*, const System::Filename*);
typedef void*     (*ExternalLoadFn)(const System::Filename*, void*);
typedef void      (*ForceUnloadFn)(void*, const System::Filename*);
typedef void*     (*WideStringCtorFn)(void*, const String*);
typedef void      (*MemFreeFn)(void*);

typedef void*     (*SetInsertFn)(void*, void**, char* const*, const uint32_t*);  // 0x180027380
typedef void      (*SubtreeFreeFn)(void*, void*);                                // 0x180027970
typedef void*     (*ArrayFilenameDtorFn)(void*);                                 // 0x1800098d0
typedef void      (*ArrayFilenameSetSizeFn)(void*, int, const Filename*);        // 0x180026880
typedef void      (*ReloadArraySetSizeFn)(void*, int, const void*);              // 0x180028cd0

void MemFree(void* p) {   // the null test is INSIDE 0x1800c7ad0
    static void* c = nullptr;
    reinterpret_cast<MemFreeFn>(RtResolveCached(
        "?MemFree@MemoryAllocator@System@@SAXPEAX@Z", &c))(p);
}

// Retail's inlined String release, with its asymmetry: the FREEING branch
// leaves the slot holding `data - 8` and only the still-referenced branch
// nulls it. Every one of this body's ~12 release sites has that shape.
void ReleaseInline(char** slot) {
    char* p = *slot;
    if (!p) return;
    if (--reinterpret_cast<int32_t*>(p)[-1] == 0) {
        *slot = p - 8;
        MemFree(p - 8);
    } else {
        *slot = nullptr;
    }
}

// ENDGAME B.3.1 step 2: the file-local `ReleaseThenAdopt` (retail's inlined
// `Filename::operator=`, the `!=` guard on the buffer at 0x1800af06c and
// 0x1800af341) is gone -- both call sites are now the class's own
// `operator=`/copy ctor, which carry the identical guard (Filename.h's
// DELIMITERS note).

// MSVC's inlined `strcmp`, transcribed from this body's own listing: the scan
// compares UNSIGNED bytes and the answer is a SIGNED-char subtraction
// (`MOVSX` at 0x1800aee89/0x1800aee8e and the mirror at 0x1800aeed8). The two
// halves disagree only on bytes >= 0x80, which is why they are not collapsed.
int StrCmpSigned(const char* a, const char* b) {
    size_t i = 0;
    if (static_cast<unsigned char>(*a) == static_cast<unsigned char>(*b)) {
        while (a[i] != '\0' && b[i] != '\0') {
            ++i;
            if (static_cast<unsigned char>(a[i]) != static_cast<unsigned char>(b[i])) break;
        }
    }
    return static_cast<int>(static_cast<signed char>(a[i])) -
           static_cast<int>(static_cast<signed char>(b[i]));
}

// Act 3's inlined descent, 0x1800aee50..0x1800aeeef. The two null arms are
// ASYMMETRIC and that is retail's: a null NODE key skips the left test
// entirely, a null SEARCH key takes the left branch on the way down and
// reports EQUAL at the right test. Identical in shape to `rtpathid::
// InsertCore`, which is what makes the lookup agree with the insert.
PathIdNode* SnapshotFind(PathIdNode* node, const char* search) {
    while (node != nullptr) {
        const char* nodeKey = node->key;

        bool goLeft = false;
        if (nodeKey != nullptr) {                              // 0x1800aee53
            if (search == nullptr)                      goLeft = true;
            else if (StrCmpSigned(search, nodeKey) < 0) goLeft = true;
        }
        if (goLeft) { node = node->left; continue; }           // 0x1800aee9c

        if (search == nullptr) return node;                    // 0x1800aeea5
        if (nodeKey != nullptr && StrCmpSigned(nodeKey, search) >= 0)
            return node;                                       // 0x1800aeee9
        node = node->right;                                    // 0x1800aeeeb
    }
    return nullptr;
}

// THE ONE WIN32 CALL, isolated — see the banner. `GetFileAttributesW` on a
// UTF-16 copy of the path; anything but INVALID_FILE_ATTRIBUTES counts as
// present, INCLUDING a directory. Retail compares against -1 and nothing else
// (0x1800aefce `CMP EAX,-0x1 / SETNZ`).
bool PlatformFileExists(const System::Filename* path) {
    struct { void* data; } ws = { nullptr };
    // Filename read AS a String -- the same same-layout cast retail performs
    // by pointer cast throughout (Filename.h header note); WideString is not
    // a B.3.1 class, so it still wants the POD String pointer.
    RT_EXPORT_ONCE(WideStringCtorFn, "??0WideString@System@@QEAA@AEBVString@1@@Z")(
        &ws, reinterpret_cast<const String*>(path));           // 0x1800aefbf
    const DWORD attrs = ::GetFileAttributesW(static_cast<LPCWSTR>(ws.data));
    const bool ok = (attrs != 0xFFFFFFFFu);                    // 0x1800aefce
    RtString_ReleaseInline(reinterpret_cast<String*>(&ws));    // 0x1800aefd5
    return ok;
}

// B3X W0: the two virtuals this body calls go by name --
// `Object::Base::Type()` (slot 2, +0x10) and `Object::External::Destruct()`
// (slot 7, +0x38, the pre-reload teardown the object does to itself).

}  // namespace

// ============================================================================
// ?SetupFileSystem@Modification@IO@@QEAAXXZ                 retail 0x1800aebb0
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 2: the body is now
// `IO::Modification::SetupFileSystem()`, at the bottom of this file. The door
// keeps its name, its by-name AND by-address registration, and delegates. No
// qualified `IO::` call: `IO::Modification` has no vtable at all, so
// B34_BRIEFS section A.1's re-entry hazard cannot arise.
void RtIo_Modification_SetupFileSystem(Modification* self) {
    reinterpret_cast<IO::Modification*>(self)->SetupFileSystem();
}

}  // namespace RtIO

// ============================================================================
// Registration — BOTH DOORS. `QEAA` is public and the name is ordinal 4030 in
// the retail export directory, verified with pefile before this was written.
// Both spelled as LITERALS so tools/runtime_census.py can see them.
// ============================================================================
int RtIoModificationSetupFileSystemRegister() {
    int n = 0;
    RegisterRuntimeExport("?SetupFileSystem@Modification@IO@@QEAAXXZ",
                          (void*)&RtIO::RtIo_Modification_SetupFileSystem); ++n;
    RegisterRuntimeAddr(0x1800aebb0,
                        (void*)&RtIO::RtIo_Modification_SetupFileSystem);
    return n;
}

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS — `IO::Modification::SetupFileSystem`'s member body
// ============================================================================
namespace IO {

void Modification::SetupFileSystem() {
    // A member of `IO::X` must be DEFINED in a namespace ENCLOSING the class
    // (B34_BRIEFS section A.2), so this body cannot live inside `namespace
    // RtIO`, where every helper it calls sits in a nested anonymous namespace.
    // ONE function-scope using-directive brings them in unchanged and keeps
    // this a MOVE rather than a rewrite. Safe against B34_RULES section 7.2's
    // grep hazard: the only name the two namespaces share is `Modification`,
    // and inside a member the injected-class-name wins outright, so a collision
    // would be a compile error, never a silent rebind.
    using namespace RtIO;
    if (_dirty == 0) return;                              // 0x1800aebd8

    // The three singletons, all exported DATA symbols, all class statics now
    // (ENDGAME B.3.9 SS1, B39_RULES D7) — read fresh every call, no lazy cache
    // (B9-10).

    // *** `RtIO::` IN FULL, AND THE COMPILER IS WHY. *** This body lives in a
    // `namespace IO` block (B34_BRIEFS section A.2), and since B.3.4 PACKAGE+FS
    // commit 4 a bare `FileSystem` there binds to the REAL CLASS
    // `IO::FileSystem` instead of to the POD the five field reads below use.
    // That is B34_RULES section 7.2's grep hazard; it presents as a build error
    // rather than a silent rebind, which is exactly why the rule says to spell
    // the POD namespace out.
    RtIO::FileSystem* const fs =
        reinterpret_cast<RtIO::FileSystem*>(::IO::FileSystem::_singleton);   // ENDGAME B.3.9 IO (D7): the class static.
    void* const extFactory = ::Object::ExternalFactory::_singleton;
    // `Object::Factory::_singleton` is captured HERE, at 0x1800aebef, and used
    // only in act 4. Retail spills it to [RBP-0x39] across the whole function
    // rather than re-reading it, and so does this.
    const RtObject::Factory* const factory =
        reinterpret_cast<RtObject::Factory*>(::Object::Factory::_singleton);

    // `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` + 0x00 — the type id
    // FindNext filters on. Read through the NAME door so a promotion of the row
    // is followed rather than the DLL's node being pinned; this is the same
    // address (0x1801f9b38) `?Type@External@Object@@UEBAIXZ` is two
    // instructions of.
    // ENDGAME B.3.9 OBJ: the class record (the minted one is retired).
    const uint32_t kExternalTypeId = ::Object::External::TypeId();

    static void* findNextCache        = nullptr;
    static void* getPathCache         = nullptr;
    static void* getCompiledCache     = nullptr;
    static void* packfileIdCache      = nullptr;
    static void* closePackagesCache   = nullptr;
    static void* loadPackagesCache    = nullptr;
    static void* existsInPackCache    = nullptr;
    static void* externalLoadCache    = nullptr;
    static void* forceUnloadCache     = nullptr;

    const FindNextFn FindNext = reinterpret_cast<FindNextFn>(RtResolveCached(
        "?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z",
        &findNextCache));
    const GetPathFn GetPath = reinterpret_cast<GetPathFn>(RtResolveCached(
        "?GetPath@External@Object@@QEBA?AVFilename@System@@XZ", &getPathCache));
    const GetCompiledPathFn GetCompiledPath =
        reinterpret_cast<GetCompiledPathFn>(RtResolveCached(
            "?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z",
            &getCompiledCache));
    const GetPackfileIdFn GetPackfileId = reinterpret_cast<GetPackfileIdFn>(
        RtResolveCached("?GetPackfileId@FileSystem@IO@@QEBAIAEBVFilename@System@@@Z",
                        &packfileIdCache));
    // No AdjustDelimiters resolve any more -- every call site was the tail of
    // a mutating Filename entry point and is now the class's own ctor /
    // operator= (see the typedefs block's banner above).

    // ========================= ACT 1 — SNAPSHOT =============================
    PathIdSet snapshot = { nullptr, 0, 0 };                    // [RBP-0x49]

    static SetInsertFn setInsert = nullptr;
    if (!setInsert) setInsert = reinterpret_cast<SetInsertFn>(
        ResolveAddr(Module::Runtime, 0x180027380));

    for (const RtObject::External* ext = FindNext(extFactory, kExternalTypeId, nullptr);
         ext != nullptr; ) {
        System::Filename compiled;                             // [RBP+0x77]
        {
            // GetPath's hidden return slot: `raw` holds null (default ctor),
            // so the callee constructing into it overwrites nothing, and
            // GetPath returns the same pointer it was handed (retail's `got`).
            System::Filename raw;                              // [RBP+0x7f]
            GetPath(ext, &raw);                                 // 0x1800aec29
            GetCompiledPath(&compiled, &raw);                   // 0x1800aec36
        }  // ~raw releases here, exactly where retail's 0x1800aec3c release sat

        const uint32_t id = GetPackfileId(fs, &compiled);      // 0x1800aec9f
        void* outSlot = nullptr;                               // [RBP-0x4?]
        setInsert(&snapshot, &outSlot, &compiled._data, &id);  // 0x1800aecb7

        ext = FindNext(extFactory, kExternalTypeId, ext);      // 0x1800aecc8
        // ~compiled releases here, exactly where retail's 0x1800aecd3 release sat.
    }

    // ========================= ACT 2 — RESTACK ==============================
    reinterpret_cast<NoArgFn>(RtResolveCached(
        "?ClosePackages@FileSystem@IO@@QEAAXXZ", &closePackagesCache))(fs);

    {   // ~Array<Filename>(fs->userPackFilenames) — BY ADDRESS, the registered
        // door modification.cpp owns.
        static ArrayFilenameDtorFn dtor = nullptr;
        if (!dtor) dtor = reinterpret_cast<ArrayFilenameDtorFn>(
            ResolveAddr(Module::Runtime, 0x1800098d0));
        dtor(&fs->userPackFilenames);                          // 0x1800aed44
    }

    {
        static ArrayFilenameSetSizeFn append = nullptr;
        if (!append) append = reinterpret_cast<ArrayFilenameSetSizeFn>(
            ResolveAddr(Module::Runtime, 0x180026880));

        // BACKWARDS, and `order` is stamped on every mod — see the banner.
        for (int32_t i = _count - 1; i >= 0; --i) {       // 0x1800aed49
            Mod* const m = &_mods[i];
            m->order = i;                                      // 0x1800aed64
            if (m->enabled != 0)                               // 0x1800aed68
                append(&fs->userPackFilenames,
                       fs->userPackFilenameCount + 1, &m->file);
        }
    }

    reinterpret_cast<NoArgFn>(RtResolveCached(
        "?LoadPackages@FileSystem@IO@@QEAAXXZ", &loadPackagesCache))(fs);

    // ========================= ACT 3 — DIFF =================================
    ReloadArray queue = { nullptr, 0, 0 };                     // [RSP+0x30]

    static ReloadArraySetSizeFn queueAppend = nullptr;
    if (!queueAppend) queueAppend = reinterpret_cast<ReloadArraySetSizeFn>(
        ResolveAddr(Module::Runtime, 0x180028cd0));

    for (const RtObject::External* ext = FindNext(extFactory, kExternalTypeId, nullptr);
         ext != nullptr; ) {
        System::Filename compiled;                             // [RBP+0x7f]
        {
            System::Filename raw;                              // [RBP-0x59]
            GetPath(ext, &raw);                                 // 0x1800aedc7
            GetCompiledPath(&compiled, &raw);                   // 0x1800aedd4
        }  // ~raw releases here, exactly where retail's 0x1800aedda release sat

        PathIdNode* const hit = SnapshotFind(snapshot.root, compiled._data);

        // `TEST RBX,RBX` at 0x1800aeefd is redundant on the found path and is
        // kept; the id compare is what decides.
        if (hit != nullptr && hit->value != GetPackfileId(fs, &compiled)) {
            System::Filename compiled2;                         // [RBP+0x77]
            {
                System::Filename raw2;                          // [RBP-0x51]
                GetPath(ext, &raw2);                             // 0x1800aef22
                GetCompiledPath(&compiled2, &raw2);              // 0x1800aef2f
            }  // ~raw2 releases here, retail's 0x1800aef35 release

            // `FileSystem::Exists` OPEN-CODED with its two gate bytes; see the
            // banner for why this is not a call to Exists.
            bool present = false;
            if (fs->existsInPackfileGate != 0) {               // 0x1800aef90
                present = reinterpret_cast<ExistsInPackfileFn>(RtResolveCached(
                    "?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z",
                    &existsInPackCache))(fs, &compiled2);
            }
            if (!present && fs->platformExistsGate != 0)       // 0x1800aefab
                present = PlatformFileExists(&compiled2);

            if (present) {
                // ---- queue the reload, 0x1800af03d --------------------------
                ReloadEntry entry;
                entry.typeId   = reinterpret_cast<const ::Object::Base*>(ext)->Type();   // 0x1800af052 (B.3.x bridge)
                entry.external = const_cast<RtObject::External*>(ext);
                // entry._hole04 is DELIBERATELY left uninitialised: retail's
                // first store is a DWORD and nothing ever writes +0x04..+0x07.
                {
                    System::Filename raw3;                      // [RBP-0x79]
                    GetPath(ext, &raw3);                         // 0x1800af044
                    // `entry.file` is raw, uninitialised bytes here (a fresh
                    // ReloadEntry), so placement-construct. Release-then-adopt
                    // on a null destination is a plain adopt, and the ctor's
                    // OWN body already carries the duplicated AdjustDelimiters
                    // (filename.cpp's `Filename(const Filename&)`, retail
                    // 0x1800067d0, calls it twice) -- so this one construction
                    // is retail's 0x1800af064..0x1800af0b2 in full.
                    new (&Sys(entry.file)) System::Filename(raw3);
                }  // ~raw3 releases here, retail's release after 0x1800af044

                queueAppend(&queue, queue.size + 1, &entry);   // 0x1800af0c7

                // queueAppend is a raw byte-copy resolve door (not a converted
                // class), so it never AddRefs: the array's copy inherits
                // entry.file's own reference, and dropping that reference here
                // is retail's release of the local -- ownership transfer via
                // copy-then-release, not a double free.
                Sys(entry.file).~Filename();
            } else {
                // ---- drop it, 0x1800af195 ----------------------------------
                System::Filename raw4;                          // [RBP-0x?]
                GetPath(ext, &raw4);                             // 0x1800af19d
                reinterpret_cast<ForceUnloadFn>(RtResolveCached(
                    "?ForceUnload@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z",
                    &forceUnloadCache))(extFactory, &raw4);     // 0x1800af1a9
                // ~raw4 releases here, retail's release right after ForceUnload
            }

            // ~compiled2 releases here, retail's 0x1800af243 release
        }

        ext = FindNext(extFactory, kExternalTypeId, ext);      // 0x1800af275
        // ~compiled releases here, retail's 0x1800af281 release
    }

    // ========================= ACT 4 — RELOAD ===============================
    for (int32_t i = 0; i < queue.size; ++i) {                 // 0x1800af2e6
        ReloadEntry* const e = &queue.data[i];
        RtObject::External* const ext = e->external;

        reinterpret_cast<::Object::External*>(ext)->Destruct();   // 0x1800af306 (B.3.x bridge)

        // the type-tree descent, inlined; a MISS yields null and is then
        // dereferenced. See the banner.
        const uint32_t typeId = e->typeId;                     // 0x1800af309
        RtObject::TypeNode* node = factory->root;              // 0x1800af30c
        RtObject::TypeNode* found = nullptr;
        while (node != nullptr) {
            if (typeId < node->key)  { node = node->left;  continue; }
            if (node->key >= typeId) { found = node; break; }
            node = node->right;
        }
        RtObject::BaseTypeInfo* const info = found->value.info;   // 0x1800af330
        // IN-PLACE factory, +0x28 — not the allocating one at +0x20.
        reinterpret_cast<void* (*)(void*)>(info->inPlaceFactoryFn)(ext);

        // re-point `External::_path` (+0x08) at the queued filename. A single
        // AdjustDelimiters after release-then-adopt on an object that already
        // owns a string is `Filename::operator=(const Filename&)`, 0x180006770
        // (Filename.h's DELIMITERS note). `ext->_path` is a `RtSystem::Filename`
        // MEMBER of the not-yet-converted `Object::External`, reached through
        // the bridge; `e->file` is the `ReloadEntry` POD member (see its banner).
        Sys(ext->_path) = Sys(e->file);                         // 0x1800af341/0x1800af3b3

        System::Filename compiled;                            // [RBP+0x6f]
        GetCompiledPath(&compiled, &Sys(e->file));              // 0x1800af3c0
        reinterpret_cast<ExternalLoadFn>(RtResolveCached(
            "?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z",
            &externalLoadCache))(&compiled, ext);              // 0x1800af3cc
        // ~compiled releases here, retail's 0x1800af3d2 release
    }

    // ========================= ACT 5 — TEARDOWN =============================
    _dirty = 0;                                           // 0x1800af447

    // Each surviving entry's Filename dtor, 0x1800af460 -- the queue's own
    // storage is raw bytes (see ReloadEntry's banner), so the release is
    // spelled as the explicit destructor rather than a member's own RAII.
    for (int64_t i = 0; i < static_cast<int64_t>(queue.size); ++i)
        Sys(queue.data[i].file).~Filename();
    if (queue.data) MemFree(queue.data);
    queue.data = nullptr;
    queue.size = 0;

    // `Set::~Set`, INLINED by retail: it recurses on the root's two children
    // and then frees the ROOT ITSELF by hand rather than calling the recursive
    // helper on the root. Transcribed, not tidied — a `SubtreeFree(root)` would
    // be the same net effect through a different call shape and would stop this
    // matching the listing.
    if (snapshot.root != nullptr) {
        static SubtreeFreeFn subtreeFree = nullptr;
        if (!subtreeFree) subtreeFree = reinterpret_cast<SubtreeFreeFn>(
            ResolveAddr(Module::Runtime, 0x180027970));
        PathIdNode* const root = snapshot.root;
        subtreeFree(&snapshot, root->left);                    // 0x1800af52d
        subtreeFree(&snapshot, root->right);                   // 0x1800af53a
        ReleaseInline(&root->key);
        MemFree(root);
    }
}

}  // namespace IO
