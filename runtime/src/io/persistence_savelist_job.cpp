// ============================================================================
// runtime/src/io/persistence_savelist_job.cpp — BATCH 5 (2026-09-12)
//   IO::Persistence's worker, COMMAND 4 — the GET-SAVE-LIST job.
//   retail 0x1800b6290, UNEXPORTED, 780 bytes / 218 instructions.
// ============================================================================
//
// This is one of the five address doors `persistence_worker_win.cpp` opened
// when it ported the worker's `Run` dispatcher (0x1800b7260) and declared, in
// its "WHAT THIS GROWS" section:
//
//     SYMBOL      (unexported) 0x1800b6290  the GET-SAVE-LIST job
//     CONSEQUENCE … At cutover an unported arm resolves NULL and the worker
//                   thread executes address 0 — so these five are a cutover
//                   obligation this file CREATES and does not pay.
//
// This pays one of the five. The other four (SAVE 0x1800b6e00, LOAD
// 0x1800b6aa0, DELETE 0x1800b69c0, GET-SAVE-INFO 0x1800b65b0) are still open;
// that file's addressee note is updated in the same commit rather than left to
// claim all five are outstanding.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES
// ---------------------------------------------------------------------------
//   1. `Array<SaveGameInfo>::SetSize(&p->saves, 0)`      0x1800b62b3 / 0x18002aae0
//      — clears the previous list WITHOUT freeing its buffer (see array.cpp's
//        note on that address: the MemFree on its realloc arm is unreachable
//        with a new size of 0, but the shrink-path element destroy is NOT, and
//        that is what releases the old filename Strings).
//   2. `FindFiles(fs, GetSaveDirectory() + "\\Save\\", "*.sav", false, &found)`
//                                                        0x1800b6315 / 0x1800ad7a0
//   3. per hit: `name = found[i].GetPathFilename(true)`   0x1800b63c5 / 0x1800c6740
//               `stamp = fs->GetFileTimeStamp(found[i])`  0x1800b6445 / 0x1800ae270
//               `Array<SaveGameInfo>::SetSize(&p->saves, count+1, &entry)`
//                                                        0x1800b645f / 0x18002ae20
//   4. `p->status = 1`                                    0x1800b64c8
//   5. release every found Filename and free the array's buffer.
//
// The two string literals are read out of .rdata, not guessed:
//     0x180155850  "*.sav"    the FindFiles pattern
//     0x180155858  "\\Save\\" the suffix appended to the save directory
// They are two of three consecutive literals in one block, the first of which
// (0x180155840, "%sSave\\%s.sav") is the SAVE path format the sibling jobs use
// — so picking the wrong one is easy and would send FindFiles at the wrong
// directory with a format string as a pattern.
//
// ---------------------------------------------------------------------------
// `this` IS THE WORKER SUBOBJECT, NOT THE Persistence
// ---------------------------------------------------------------------------
// `??0Persistence@IO@@` embeds a Thread-derived worker at +0x10 and stamps
// vtable 0x180155860 over it, so every offset in the shipped listing is
// `Persistence offset - 0x10`:  worker+0x38 is `status` (+0x48), worker+0x70
// is `saves` (+0x80), worker+0x78 is `saveCount` (+0x88). This body recovers
// the `Persistence*` and uses the NAMED fields, exactly as
// persistence_worker_win.cpp does, so a future change to the layout breaks the
// static_asserts rather than silently shifting a raw offset here.
//
// ---------------------------------------------------------------------------
// `GetFileTimeStamp` IS GIVEN THE FULL FOUND PATH, NOT THE BASE NAME
// ---------------------------------------------------------------------------
// 0x1800b6437 loads `R8 = found._data + i*8` — the same element
// `GetPathFilename` was called on, NOT the base name it produced. The base name
// goes into the entry's display `name` and the FULL path goes to the
// filesystem. Reading it the other way round would stat a file in the process
// working directory, find nothing, and stamp every save 0.
//
// *** THIS PORT SETTLES WHAT `SaveGameInfo::sortKey` IS. ***
// runtime/types/io/Persistence.h named +0x00 `sortKey` "for what it is used
// as" — `?UpdateList@SaveLoad@Save@@` copies it into SaveInfo+0x30 and a qsort
// comparator orders it descending, unsigned. This is its PRODUCER, and it is
// the raw FILETIME qword `GetFileTimeStamp` returns: a 100 ns tick count since
// 1601, which is exactly why an unsigned descending sort puts the newest save
// first. The header comment is completed in the same commit.
//
// ---------------------------------------------------------------------------
// NO NULL GUARDS ARE ADDED
// ---------------------------------------------------------------------------
// Retail indexes `found._data` without a bounds check, dereferences the
// `GetPathFilename` result without testing it, and frees the buffer without
// testing the count a second time. All transcribed. The only tests present are
// retail's own: the refcount-zero test in each release, and the
// `if (found._data)` before the final MemFree.
//
// ---------------------------------------------------------------------------
// ORACLE — NONE, and the refusal is structural
// ---------------------------------------------------------------------------
// The job's entire effect is a REBUILD of `p->saves` plus `p->status = 1`.
// Running the shipped body afterwards for comparison would rebuild the same
// list from the same directory on top of ours — the second run's `SetSize(0)`
// releases the Strings the first run just adopted, so the comparison is not
// merely vacuous, it destroys the subject it would compare. (Same shape
// avl_trees.cpp records for its inserts: a destructive mutator on shared state
// has no live replay.) And it runs on the WORKER THREAD, where the diff
// registry's counters are not synchronised.
//
// What a boot checks instead is end to end and is not weak: the save browser
// lists the files that are on disk, in modification order, with their base
// names. A wrong pattern, a wrong directory, a wrong stamp or a wrong element
// spec each show up there immediately.
//
// UNVERIFIED UNTIL A SAVE-LIST REFRESH RUNS: an autostart boot that never opens
// the load dialog does not reach this body, and `calls=0` is not verification
// (reference_uncalled_port_is_not_verified.md).
//
// PORTABILITY: no <windows.h>. The Win32 half of this path lives behind
// `FileSystem::FindFiles` and `FileSystem::GetFileTimeStamp`, which are
// `_win` files of their own (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <new>        // placement new into a hidden-return-slot / a raw struct field

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/Persistence.h"
#include "../../types/system/Filename.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"
#include "../collection/array_internal.h"   // ENDGAME B.3.2 step 2 -- kSpec_SaveGameInfo, AsArray

using RtIo::Persistence;
using RtIo::SaveGameInfo;
using RtSystem::Sys;   // the transitional POD->class bridge (pod_bridge.h)

// Declared locally rather than including standalone/resolve.h, which drags in
// <windows.h>. Byte-identical to persistence_worker_win.cpp's copy.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// `Collection::Array<System::Filename>` as FindFiles fills it — the same three
// fields every Array in this engine has. `data` is `System::Filename` now
// (same eight bytes as the old `Filename*`): every element this file owns
// individually, so each one becomes a real destructor call instead of the
// manual release.
struct FilenameArray {
    System::Filename* data;      // +0x00
    int32_t            size;      // +0x08
    int32_t            capacity;  // +0x0c
};
static_assert(offsetof(FilenameArray, size)     == 0x08, "Array::_size @ +0x08");
static_assert(offsetof(FilenameArray, capacity) == 0x0c, "Array::_capacity @ +0x0c");

// These are file-local typedefs for calls OUT through the resolve door (not
// entry-point signatures), so they now name the classes occupying the same
// eight bytes. The resolve mechanism itself (RtResolveCached / ResolveAddr) is
// untouched — this file's whole design is standing on its own against named
// doors, and that stays exactly as it was.
typedef void      (*StringSetFn)(System::String*, const char*);                 // 0x1800c95d0
typedef System::Filename* (*GetSaveDirectoryFn)(const void*, System::Filename*);// 0x1800acfd0
typedef System::String*   (*ConcatCStrFn)(System::String*, const System::String*,
                                          const char*);                          // 0x1800c92d0
typedef void      (*FindFilesFn)(const void*, const System::String*, const System::String*,
                                 bool, void*);                          // 0x1800ad7a0
typedef System::Filename* (*GetPathFilenameFn)(const System::Filename*,
                                               System::Filename*, bool); // 0x1800c6740
typedef uint64_t* (*GetFileTimeStampFn)(const void*, uint64_t*, const System::Filename*);
                                                                        // 0x1800ae270
typedef void      (*MemFreeFn)(void*);                                  // 0x1800c7ad0

// `IO::FileSystem::_singleton` — an exported DATA symbol, so the NAME door is
// the right door and the `0x1801fc6a8` in the listing needs no address door.
void* g_fsSingletonCache = nullptr;
void* FileSystemSingleton() {
    void** slot = reinterpret_cast<void**>(RtResolveCached(
        "?_singleton@FileSystem@IO@@0PEAV12@EA", &g_fsSingletonCache));
    return slot ? *slot : nullptr;
}

void MemFree(void* p) {   // the null test is INSIDE 0x1800c7ad0
    static void* cache = nullptr;
    reinterpret_cast<MemFreeFn>(RtResolveCached(
        "?MemFree@MemoryAllocator@System@@SAXPEAX@Z", &cache))(p);
}

// 0x180155850 and 0x180155858 — see the banner for why the neighbouring
// "%sSave\\%s.sav" at 0x180155840 is NOT one of these.
const char* const kSavePattern = "*.sav";
const char* const kSaveSubDir  = "\\Save\\";

}  // namespace

// ============================================================================
// The worker's command-4 body.                             retail 0x1800b6290
//
// `worker` is `Persistence + 0x10`; see the banner.
// ============================================================================
extern "C" void RtIo_PersistenceWorker_GetSaveList(void* worker) {
    Persistence* const p =
        reinterpret_cast<Persistence*>(reinterpret_cast<char*>(worker) - 0x10);

    // ---- 1. clear the previous list (buffer kept) --------------------------
    // ENDGAME B.3.2 step 2 — 0x18002aae0 is the SAME `Array<SaveGameInfo>` as
    // 0x18002ae20 below with the new size folded to 0 (array.cpp's
    // kSpec_SaveGameInfo banner); NOT trait-expressible (see array_internal.h),
    // so this is the hand-derived row, bridged through `AsArray` rather than a
    // re-typed `Persistence::saves` member (shared with files outside this
    // batch — see persistence_saveinfo_job.cpp's note on the same struct).
    rtcoll::AsArray<&rtcoll::kSpec_SaveGameInfo>(&p->saves)->SetSize(0);  // 0x1800b62b3

    void* const fs = FileSystemSingleton();                     // 0x1800b62b8

    FilenameArray found = { nullptr, 0, 0 };                    // [RBP-0x30]

    // ---- 2. FindFiles(saveDir + "\Save\", "*.sav", recursive=false) -------
    // Scoped to a block, declared in this order, so the block's automatic
    // teardown lands at the same point retail's manual release did
    // (0x1800b631b..0x1800b636c) and runs in retail's own order (dir, then
    // saveDir, then pattern — reverse of declaration).
    {
        System::String pattern;                                     // [RBP+0x48]
        {
            static void* setCache = nullptr;
            reinterpret_cast<StringSetFn>(RtResolveCached(
                "?Set@String@System@@QEAAXPEBD@Z", &setCache))(&pattern, kSavePattern);
        }

        System::Filename saveDir;                                   // [RBP+0x58]
        {
            static void* gsdCache = nullptr;
            reinterpret_cast<GetSaveDirectoryFn>(RtResolveCached(
                "?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ",
                &gsdCache))(fs, &saveDir);                          // 0x1800b62e5
        }

        System::String dir;                                         // [RBP+0x50]
        {
            static void* catCache = nullptr;
            // ??HSystem@@YA?AVString@0@AEBV10@PEBD@Z, 0x1800c92d0. AsString()
            // is the conversion retail performs by pointer cast (Filename.h).
            reinterpret_cast<ConcatCStrFn>(RtResolveCached(
                "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z", &catCache))(
                    &dir, &saveDir.AsString(), kSaveSubDir);
        }

        {
            static void* ffCache = nullptr;
            reinterpret_cast<FindFilesFn>(RtResolveCached(
                "?FindFiles@FileSystem@IO@@QEBAXAEBVString@System@@0_NAEAV?$Array@"
                "VFilename@System@@@Collection@@@Z", &ffCache))(
                    fs, &dir, &pattern, false, &found);              // 0x1800b6315
        }

        // ~dir, ~saveDir, ~pattern are retail's release sequence at
        // 0x1800b631b/0x1800b6343/0x1800b636c, now the three destructors
        // below, firing in exactly that order at the close of this block.
    }

    // ---- 3. one SaveGameInfo per hit --------------------------------------
    for (int32_t i = 0; i < found.size; ++i) {                  // 0x1800b6397
        SaveGameInfo entry;
        entry.sortKey = 0;                                      // 0x1800b63a0
        // `entry.name` (a `RtSystem::String` field) is placement-constructed
        // below rather than zeroed here -- the copy ctor's own "adopt if
        // non-null, else leave null" already produces 0x1800b63a8's effect
        // for both outcomes.

        {
            // `Filename::GetPathFilename(true)` returns a Filename BY VALUE,
            // so the slot is hidden in RDX — retail reuses `pattern`'s dead
            // stack cell for it (`LEA RDX,[RBP+0x48]`). Filled directly: the
            // callee treats the pointer as raw memory, the same as any other
            // hidden-return-slot target.
            System::Filename baseName;
            System::Filename* got;
            {
                static void* gpfCache = nullptr;
                got = reinterpret_cast<GetPathFilenameFn>(RtResolveCached(
                    "?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z", &gpfCache))(
                        &found.data[i], &baseName, true);           // 0x1800b63c5
            }
            // UNGUARDED deref of the returned pointer, as retail writes it.
            // `String(const String&)` (retail 0x180006cb0) is exactly "adopt
            // the buffer, AddRef if non-null" -- what the C body wrote out by
            // hand at 0x1800b63ce. `entry.name` is a `RtSystem::String`
            // field of the (not yet converted) `SaveGameInfo`, reached
            // through the transitional bridge.
            new (&Sys(entry.name)) System::String(got->AsString());
            // ~baseName is retail's release at 0x1800b63da.
        }

        // THE FULL FOUND PATH, not the base name — see the banner.
        {
            static void* tsCache = nullptr;
            uint64_t stamp = 0;
            uint64_t* r = reinterpret_cast<GetFileTimeStampFn>(RtResolveCached(
                "?GetFileTimeStamp@FileSystem@IO@@QEBA?AVDateTime@System@@"
                "AEBVFilename@4@@Z", &tsCache))(fs, &stamp, &found.data[i]);
            entry.sortKey = *r;                                 // 0x1800b644a
        }

        // retail 0x18002ae20 — Array<SaveGameInfo>::SetSize(n, &fill), the
        // append idiom. Hand-derived row (kSpec_SaveGameInfo), bridged the
        // same way as step 1's clear above.
        rtcoll::AsArray<&rtcoll::kSpec_SaveGameInfo>(&p->saves)
            ->SetSizeFill(p->saveCount + 1, &entry);            // 0x1800b645f

        // `entry` is a plain local (`SaveGameInfo` is not yet converted), so
        // there is no outer RAII to run this for us; retail's release at
        // 0x1800b6465 is spelled as the destructor itself through the bridge.
        Sys(entry.name).~String();
    }

    // ---- 4. publish ---------------------------------------------------------
    p->status = 1;                                              // 0x1800b64c8

    // ---- 5. tear the FindFiles array down ----------------------------------
    // `found.data` is a raw buffer of `System::Filename` from an external
    // allocator (not a container with its own RAII), so each element's
    // release is still an explicit call -- now the destructor itself.
    for (int64_t i = 0; i < static_cast<int64_t>(found.size); ++i)
        found.data[i].~Filename();                              // 0x1800b64e4
    if (found.data) MemFree(found.data);                        // 0x1800b6541
}

// ============================================================================
// Registration — ADDRESS ONLY.
//
// Unexported: `persistence_worker_win.cpp` measured this against the retail
// export directory with pefile when it opened the door ("the nearest preceding
// export to every one of them is `?Serialize@PackageFile@IO@@` several hundred
// bytes back"), so there is no mangled name to register under and "unexported"
// is a measurement rather than a reading of an `AEAA`
// (reference_aeaa_is_private_not_unexported.md).
//
// The address is a LITERAL so tools/runtime_census.py can see the door.
// ============================================================================
int RtIoPersistenceSaveListRegister() {
    RegisterRuntimeAddr(0x1800b6290, (void*)&RtIo_PersistenceWorker_GetSaveList);
    return 1;
}
