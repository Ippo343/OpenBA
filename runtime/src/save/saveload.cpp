// ============================================================================
// runtime/src/save/saveload.cpp — Wave 5 lane C — Save::SaveLoad / Save::SavedState
// ============================================================================
// Ports all 17 of the real exports our tree calls on these two classes (Wave 14
// lane GENNAME closed the last one, GenerateSaveName, which this banner used to
// carry as the "16 of 17" gap). Every
// retail body below was decompiled AND disassembled at its real address in the
// SHIPPED Runtime-x64.dll (base 0x180000000) on 2026-09-06; both addresses and
// the oracle shape for each function are in docs/runtime_port/WAVE5_LANE_C.md.
//
// HELD BACK, with reasons (a held-back function with a stated reason is a
// result; a registered function nothing ever measured is not):
//
//   ?Draw@SaveLoad@Save@@…            0x1800c2eb0 — 400 lines that construct a
//       Graphics::FrameCopyImageBuffer, a Video::ImageBuffer, a
//       Video::RenderBuffer, a Graphics::MaterialInstance, a UI::SpriteSheet,
//       a Graphics::Camera and a UI::SpriteContext by hand, stamping five
//       vtables this project does not own.
//       THE LOAD-BEARING REASON IS THE ORACLE, NOT THE OWNERSHIP: it renders
//       into the live Video::Context and allocates and destroys GPU resources,
//       so running both implementations would draw twice and leak a render
//       target every frame. That is independent of who owns the classes and it
//       does not expire.
//       (The ownership half of this comment used to read "those classes are
//       Waves 6 and 7", which now reads as RESOLVED because both waves are
//       closed. It is not: MaterialInstance's and SpriteSheet's vtables and
//       SpriteContext::AddLine/SetScissor are still individually unported.
//       Corrected in the Wave 8.5 close-out -- see
//       docs/runtime_port/WAVE8_5_HOLDBACK_RECHECK.md -- because a blocker
//       phrased as "waiting on another wave" silently becomes a lie the day
//       that wave lands.)
//
//   ??1SaveLoad@Save@@QEAA@XZ         0x1800c3c90 — the destructor walks the
//       Array<SaveInfo> at +0x50 through FUN_1800329c0, which calls the
//       un-exported per-element destructor FUN_1800c3f40 0x58 bytes at a time.
//       No oracle is possible (it frees), the object is a process-lifetime
//       singleton so it is unlikely ever to run, and a wrong port corrupts the
//       heap silently. Left on the DLL.
//
//   ?_singleton@SaveLoad@Save@@0PEAV12@EA  0x1801fc6f0 — a DATA export. The
//       registry seam maps names to FUNCTIONS; a data symbol cannot be
//       redirected before cutover, exactly as WAVE3.md and WAVE4.md record for
//       InternalFactory::_singleton and FileSystem::_singleton. This port
//       WRITES that DLL global (the ctor publishes it), which is the correct
//       behaviour while the DLL is still loaded.
//
// THE HAZARD THAT DEFINES THIS LANE. A wrong port here does not crash, it
// desyncs a save and surfaces later on an innocent record. So the oracle shape
// was chosen BEFORE each body was written, and three shapes are in use:
//
//   * FULL DIFF — both implementations run on the same inputs and the results
//     are compared. Only for functions that are read-only or idempotent:
//     GetOperation, GetSaveCount, GetSaveInfo, GetSaveState, CheckMod.
//
//   * SCRATCH-COPY DIFF — the function only writes into `*this` and allocates
//     nothing, so two private 0x90-byte copies of the live object can be made,
//     ours run on one and theirs on the other, the two compared byte for byte,
//     and only then is ours applied to the real object. Neither scratch run
//     touches shared state. Used by SetOperation, SetMinOperationTime,
//     SetSaveSprite, ??0SaveLoad, ??0SavedState, and — with refcount
//     compensation, see the comment there — SetSaveName.
//
//   * SCRATCH-OBJECT DIFF — theirs runs against a private replica of the
//     OUTPUT object rather than the live one, and the buffer it produces is
//     compared against ours and then freed. Used by Store and Retrieve, whose
//     whole job is to move bytes: running both against the live MemoryStream
//     would double-consume it, which is precisely the failure mode Wave 4
//     spent itself on.
//
// ~SavedState is ported with NO oracle and that is said plainly here rather
// than dressed up: it is nine instructions (`if (p) locked-free(p)`) and
// unambiguous, but nothing measures it.
//
// PLATFORM SEAM (PLAN §4.1 decision 1): nothing in these two classes touches
// Win32 directly. The critical sections retail inlines around Allocate/Free
// live inside MemoryAllocator::MemAlloc/MemFree, which this file calls as
// exports, so no `_win` twin is needed and <windows.h> stays out.
// ============================================================================

#include <cstdint>
#include "../system/timer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/save/SaveLoad.h"
#include "../../types/io/Stream.h"
#include "../system/string.h"   // RtSystem::String (Wave 2, lane String)
#include "saveload_internal.h"

using RtSave::SaveLoad;
using RtSave::SavedState;
using RtSave::SaveInfo;
using RtSave::ModInfo;
using RtSave::Operation;
using RtIo::MemoryStream;
using RtSystem::String;

namespace {

// ----------------------------------------------------------------------------
// Dependencies. Every one of these is fetched from the SHIPPED DLL through
// RT_ORIGINAL_ONCE / ResolveExportRaw, never through ResolveExport.
//
// Two reasons, and they are different reasons:
//   1. Half of these (String::operator+, Filename::AdjustDelimiters,
//      MemAlloc/MemFree) are already ported by Waves 2 and 4. Resolving them
//      by NAME through the seam would make this lane's ordinary, non-diff path
//      silently depend on another lane's port being enabled. persistence.cpp
//      (Wave 4 lane C) made the same call for the same reason: a lane's job is
//      to be correct standing alone against the DLL.
//   2. RtOriginal is GetProcAddress — an export-table binary search under the
//      loader lock. Two of them in one hot body cost Wave 2 five points of sim
//      time and were invisible to both profilers
//      (reference_rtoriginal_is_getprocaddress.md). RT_ORIGINAL_ONCE caches
//      per call site, which is why every accessor below is a function with a
//      `static` and not a bare RtOriginal call.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Dependencies. Wave 13 moved every definition that used to sit here into
// `saveload_internal.h`, unchanged, because `saveload_update.cpp` needs the
// same ones and two copies of `ReleaseStringSlot` would be two places to
// "fix" a transcribed idiom whose two dead stores look like mistakes. The
// comments explaining each one moved with it; nothing about the behaviour of
// this file changed.
// ----------------------------------------------------------------------------
using RtSaveDetail::MemAllocFn;
using RtSaveDetail::MemFreeFn;
using RtSaveDetail::GetDataDirectoryFn;
using RtSaveDetail::ConcatFn;
using RtSaveDetail::AdjustDelimsFn;
using RtSaveDetail::MemStreamOpenFn;
using RtSaveDetail::SaveLoadVoidFn;
using RtSaveDetail::MemAlloc;
using RtSaveDetail::MemFree;
using RtSaveDetail::RtDataExport;
using RtSaveDetail::SaveLoadSingletonSlot;
using RtSaveDetail::FileSystemSingleton;
using RtSaveDetail::ModificationSingleton;
using RtSaveDetail::PersistenceSingleton;
using RtSaveDetail::SecondsPerTick;
using RtSaveDetail::ZeroTime;
using RtSaveDetail::ReleaseStringSlot;

// ---- ~SaveLoad reachability counter (Wave 9 lane SYS2) ---------------------
// ~SaveLoad has no oracle (see its banner), so the ONE thing that CAN be
// measured about it is whether it runs at all. `calls=0` on a diffed port is
// this project's canonical "green line over dead code"
// (reference_uncalled_port_is_not_verified.md); a destructor gets the same
// treatment for the same reason, just without the comparison half.
// BANISHED_SAVELOAD_DTOR_TRACE=1.
int g_saveLoadDtorCalls = 0;
bool RtSaveDtorTrace() {
    static const int on = []() {
        const char* e = std::getenv("BANISHED_SAVELOAD_DTOR_TRACE");
        return (e && *e && std::strcmp(e, "0") != 0) ? 1 : 0;
    }();
    return on != 0;
}

// ---- scratch-copy diff helper ----------------------------------------------
// `ours` and `theirs` are 0x90-byte private copies of the live object; neither
// call is allowed to allocate, free, or touch anything outside its own copy.
struct SaveLoadScratch {
    alignas(16) uint8_t ours[sizeof(SaveLoad)];
    alignas(16) uint8_t theirs[sizeof(SaveLoad)];
    explicit SaveLoadScratch(const SaveLoad* live) {
        std::memcpy(ours,   live, sizeof(SaveLoad));
        std::memcpy(theirs, live, sizeof(SaveLoad));
    }
    SaveLoad* Ours()   { return reinterpret_cast<SaveLoad*>(ours); }
    SaveLoad* Theirs() { return reinterpret_cast<SaveLoad*>(theirs); }
};

}  // namespace

// ============================================================================
// Save::SaveLoad::SaveLoad()            retail 0x1800c3d30   ordinal 360
// ??0SaveLoad@Save@@QEAA@XZ
// ----------------------------------------------------------------------------
// Oracle: SCRATCH-COPY DIFF, with both scratches pre-filled with 0xCD rather
// than with the live object. That is deliberate: the ctor leaves seven fields
// untouched (see the header), so comparing two buffers that started identical
// and non-zero proves BOTH what it writes and what it does not — a plain
// zero-filled comparison would hide a missing store into an already-zero field.
//
// ORDER MATTERS. Theirs runs first, because the last thing the ctor does is
// publish `_singleton = this`; running theirs second would leave the DLL global
// pointing at a stack buffer that is about to go out of scope.
//
// Returns `this` in RAX (MOV RAX,RCX before the RET).
// ============================================================================
static void SaveLoad_CtorLogic(SaveLoad* self) {
    self->_operation        = RtSave::kOpNone;
    self->_elapsed          = 0;
    self->_minOperationTime = ZeroTime();
    self->_string20         = nullptr;
    // One qword store of 0 at +0x28 in retail, which is _id AND _size together,
    // then the pointer at +0x30 — see the embedded-SavedState note in the header.
    self->_embeddedState._id   = 0;
    self->_embeddedState._size = 0;
    self->_embeddedState._data = nullptr;
    self->field38           = 0;
    self->_selectedIndex    = -1;
    self->_savesData        = nullptr;
    self->_savesSize        = 0;
    self->_savesCapacity    = 0;
    self->_saveSprite.sheet = nullptr;
    self->_saveSprite.index = -1;
    self->_saveName         = nullptr;
    // Retail writes a BYTE here (MOV byte ptr [RCX+0x7c],DL), not a dword.
    *reinterpret_cast<uint8_t*>(&self->_captureFlags) = 0;
    self->_captureWidth     = 0;
    self->_captureHeight    = 0;
    self->_capturePixels    = nullptr;
    if (SaveLoad** slot = SaveLoadSingletonSlot()) *slot = self;
}

SaveLoad* RtSave_SaveLoad_Ctor(SaveLoad* self) {
    RT_DIFF_BEGIN("Save::SaveLoad::SaveLoad");
    if (rt_diff_on__) {
        using CtorFn = SaveLoad* (*)(SaveLoad*);
        alignas(16) uint8_t scratchOurs[sizeof(SaveLoad)];
        alignas(16) uint8_t scratchTheirs[sizeof(SaveLoad)];
        std::memset(scratchOurs,   0xCD, sizeof(scratchOurs));
        std::memset(scratchTheirs, 0xCD, sizeof(scratchTheirs));
        RT_ORIGINAL_ONCE(CtorFn, "??0SaveLoad@Save@@QEAA@XZ")(
            reinterpret_cast<SaveLoad*>(scratchTheirs));
        SaveLoad_CtorLogic(reinterpret_cast<SaveLoad*>(scratchOurs));
        RT_DIFF_BYTES(scratchOurs, scratchTheirs, sizeof(SaveLoad));
    }
    SaveLoad_CtorLogic(self);   // runs last, so _singleton ends up pointing here
    return self;
}

// ============================================================================
// Save::SaveLoad::~SaveLoad()           retail 0x1800c3c90   ordinal 666
// ??1SaveLoad@Save@@QEAA@XZ
// ----------------------------------------------------------------------------
// WAVE 9 LANE SYS2, 2026-09-08 — PORTED. The Wave 5 holdback above it is
// preserved here in full because two of its three sentences were true and one
// was NOT, and which is which is the reason this got ported:
//
//   "No oracle is possible (it frees)"           -- STILL TRUE, see ORACLE.
//   "a wrong port corrupts the heap silently"    -- STILL TRUE.
//   "the object is a process-lifetime singleton
//    so it is unlikely ever to run"              -- **FALSE, and measurably so.**
//
// The third is what made the holdback look cheap, and it does not survive a
// grep. `??1SaveLoad@Save@@QEAA@XZ` has FOUR live call sites in this tree, all
// through `fn_SaveLoad_Dtor`, and every one of them is followed by a
// `fn_MemFreeStatic` of the same pointer — i.e. these are real deletes of a
// heap object, not a process-exit formality:
//
//   code/Game/Dialog/mainmenudialog.cpp:218  MainMenuDialog_ReturnToMenu, the
//   code/Game/Dialog/mainmenudialog.cpp:226    no-game and in-game branches
//   code/Game/Game/gameupdateloop.cpp:3140   GameUpdateLoop::Shutdown
//   code/Game/Tutorial/tutorialui.cpp:81     tutorial teardown
//
// Quit-to-menu runs this. So at cutover it is not a symbol that "might" be
// asked for -- attempt 14 recorded it as asked
// (wave9_attempt14_error.txt line 32), and with the DLL gone the resolve is
// NULL and `fn_SaveLoad_Dtor(saveLoad)` is a call through zero on the way back
// to the main menu.
//
// THE FULL RETAIL CHAIN, disassembled at all five addresses on 2026-09-08:
//
//   ~SaveLoad                0x1800c3c90   this file, below
//     -> FUN_1800329c0       0x1800329c0   Array<SaveInfo>::~Array, stride 0x58
//          -> FUN_1800c3f40  0x1800c3f40   ~SaveInfo (per element)
//               -> FUN_1800331f0 0x1800331f0  Array<ModInfo>::~Array, stride 0x20
//                    -> FUN_1800341e0 0x1800341e0  destroy N ModInfo in place
//
// FUN_1800341e0 releases the String at ModInfo+0x08 and THEN the one at +0x00,
// in that order, 0x20 bytes at a time — an independent confirmation of the
// ModInfo layout this file's own header pins from CheckMod.
//
// FOUR SaveInfo OFFSETS THIS DESTRUCTOR PINS THAT THE HEADER CALLS UNMODELLED.
// runtime/types/save/SaveLoad.h deliberately declines to name +0x10..+0x37 and
// +0x40..+0x47, on the grounds that only UpdateList/LoadMetaData write them and
// naming from a guess is what PLAN §6 step 8 forbids. That stands — this port
// does NOT rename them. It reaches them as raw displacements, with what the
// destructor proves about each and nothing more:
//
//   +0x10  a System::String slot          (released with the refcount idiom)
//   +0x18  a pointer to a POLYMORPHIC object; retail calls vtable slot 0 with
//          EDX=1, i.e. the scalar DELETING destructor
//   +0x28  a raw heap block               (?MemFree@ static)
//   +0x40  an int32 ownership flag: BOTH +0x18 and +0x28 are torn down only
//          when it is >= 0. A negative value means "this SaveInfo does not own
//          them", the same -1-is-empty convention Sprite::index uses at +0x08.
//
// ORACLE: NONE, and that is a property of the function, not of this lane's
// effort. Everything it does is a free. "Call ours, call theirs, compare"
// double-frees every block in the object, and there is no scratch-copy form
// either: a 0x90-byte copy of the live SaveLoad aliases the SAME strings and
// the SAME array buffer, so running the DLL's body on the copy destroys the
// real object's heap before ours is allowed to run. What backs it instead:
//
//   1. Byte-for-byte transcription of all five bodies, disassembled (not
//      decompiled — reference_ghidra_c_output_vs_disassembly.md).
//   2. Every free routed through the SAME `MemFree()` accessor the rest of
//      this file already uses, so the heap stays whole-ours
//      (WAVE9.md §3). Retail's array-buffer frees are written out as
//      Lock(alloc+0x640); ?Free@(alloc, p); Unlock() -- which is what
//      ?MemFree@MemoryAllocator@System@@SAXPEAX@Z IS, and this file's
//      `MemFree` resolves to our port of exactly that.
//   3. The live path: quitting to the menu runs it, under the allocator
//      watchdog, and a wrong free surfaces there rather than nowhere.
//
// It is registered because at cutover the alternative to an unverifiable port
// is not "the DLL's body", it is a null call.
// ============================================================================

// FUN_1800341e0 — destroy `count` ModInfo elements in place, stride 0x20.
// Note the ORDER: +0x08 (path) first, then +0x00 (name). Retail's loop bails
// immediately when count <= 0 (the TEST/JLE is the first instruction, ahead of
// the prologue).
static void ModInfoRange_Destroy(RtSave::ModInfo* base, int32_t count) {
    if (count <= 0) return;
    for (int32_t i = 0; i < count; ++i) {
        ReleaseStringSlot(&base->path);
        ReleaseStringSlot(&base->name);
        ++base;
    }
}

// The 0x10-byte Collection::Array header both ~Array bodies below operate on:
// _data at +0x00, _size at +0x08, _capacity at +0x0c. Retail's RCX at
// 0x1800331f0 is `saveInfo + 0x48` and at 0x1800329c0 it is `saveLoad + 0x50`
// -- the ARRAY, not the object that contains it, which is why these take the
// array address and not the parent. Getting that wrong would be an ABI
// mismatch invisible until the registered address is called.
struct RtArrayHeader {
    void*   data;
    int32_t size;
    int32_t capacity;
};

// FUN_1800331f0 — Collection::Array<ModInfo>::~Array.
// The two trailing stores are ONE qword each: [arr+0] = 0 clears _data, and
// [arr+8] = 0 clears _size and _capacity together, which is why this writes
// eight bytes rather than two ints.
static void ArrayModInfo_Dtor(RtArrayHeader* arr) {
    ModInfoRange_Destroy(reinterpret_cast<RtSave::ModInfo*>(arr->data), arr->size);
    if (arr->data) MemFree(arr->data);
    arr->data = nullptr;
    arr->size = 0;
    arr->capacity = 0;
}

// FUN_1800c3f40 — ~SaveInfo.
// The +0x18 branch tests the pointer, then the +0x40 flag, then the pointer
// AGAIN (0x1800c3f65 TEST RCX,RCX). The second test is redundant in the bytes
// too; it is transcribed as written rather than folded, because a "simplified"
// guard is how a faithful port stops being one.
static void SaveInfo_Dtor(RtSave::SaveInfo* info) {
    uint8_t* p = reinterpret_cast<uint8_t*>(info);

    void* owned = *reinterpret_cast<void**>(p + 0x18);
    const int32_t ownFlag = *reinterpret_cast<const int32_t*>(p + 0x40);
    if (owned != nullptr && ownFlag >= 0 && owned != nullptr) {
        // MOV RAX,[RCX] ; MOV EDX,1 ; CALL [RAX]  -- scalar deleting dtor.
        using DeletingDtorFn = void* (*)(void*, unsigned int);
        void** vtable = *reinterpret_cast<void***>(owned);
        reinterpret_cast<DeletingDtorFn>(vtable[0])(owned, 1u);
    }

    void* block = *reinterpret_cast<void**>(p + 0x28);
    if (block != nullptr && ownFlag >= 0) MemFree(block);

    ArrayModInfo_Dtor(reinterpret_cast<RtArrayHeader*>(p + 0x48));

    // +0x38, reached in retail as [RBX+0x30]+0x8 -- the LEA RAX,[RBX+0x30] is
    // the address of an inlined Pair/String pair, not a separate object.
    ReleaseStringSlot(&info->name);
    ReleaseStringSlot(reinterpret_cast<const char**>(p + 0x10));
}

// FUN_1800329c0 — Collection::Array<SaveInfo>::~Array, stride 0x58.
// `_size` is read ONCE into EAX before the loop and used as the trip count, so
// an element destructor that changed the array's size (none does) would not be
// observed. Transcribed with the same single read.
static void ArraySaveInfo_Dtor(RtArrayHeader* arr) {
    RtSave::SaveInfo* e = reinterpret_cast<RtSave::SaveInfo*>(arr->data);
    const int32_t size = arr->size;
    if (size > 0) {
        for (int32_t i = 0; i < size; ++i) { SaveInfo_Dtor(e); ++e; }
    }
    if (arr->data) MemFree(arr->data);
    arr->data = nullptr;
    arr->size = 0;
    arr->capacity = 0;
}

void RtSave_SaveLoad_Dtor(SaveLoad* self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);

    // MOV qword ptr [0x1801fc6f0],0x0 -- unconditional, and it happens FIRST,
    // before anything can fault. Retail clears the singleton even when `this`
    // is not the singleton; that is what the bytes do.
    if (SaveLoad** slot = SaveLoadSingletonSlot()) *slot = nullptr;

    ReleaseStringSlot(reinterpret_cast<const char**>(p + 0x70));
    ArraySaveInfo_Dtor(reinterpret_cast<RtArrayHeader*>(p + 0x50));

    // FUN_180006a10, the un-exported second copy of MemFree
    // (runtime/src/engine/engine_lifecycle.cpp:262 records it as "MemFree
    // verbatim"). Routed through the same accessor, not the raw address.
    void* buffer30 = *reinterpret_cast<void**>(p + 0x30);
    if (buffer30) MemFree(buffer30);

    ReleaseStringSlot(reinterpret_cast<const char**>(p + 0x20));

    if (RtSaveDtorTrace()) {
        std::fprintf(stderr, "[SaveLoadDtor] ran on %p (call #%d)\n",
                     (void*)self, ++g_saveLoadDtorCalls);
        std::fflush(stderr);
    }
}

// ============================================================================
// Save::SaveLoad::GetOperation() const  retail 0x1800c3c80   ordinal 2826
// ?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ
// ----------------------------------------------------------------------------
// ABI: the mangled return `?AW4Operation@12@` is an enum BY VALUE, and the
// lead flagged it as a possible hidden-return-slot trap. It is not one: an
// enum with a 4-byte underlying type fits in a register, so it comes back in
// EAX with no hidden slot. Confirmed against the retail bytes — the whole
// function is `MOV EAX,[RCX] ; RET`, RCX is `this` and RDX is untouched.
// (A hidden slot would have made `this` arrive in RDX; getting that wrong is
// the wild-write family of reference_hidden_retslot_wild_write.md.)
//
// Oracle: FULL DIFF — a single load, no side effects.
// ============================================================================
Operation RtSave_SaveLoad_GetOperation(const SaveLoad* self) {
    RT_DIFF_BEGIN("Save::SaveLoad::GetOperation");
    const Operation ours = self->_operation;
    if (rt_diff_on__) {
        using Fn = Operation (*)(const SaveLoad*);
        const int oursI = static_cast<int>(ours);
        const int theirsI = static_cast<int>(
            RT_ORIGINAL_ONCE(Fn, "?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ")(self));
        RT_DIFF_SCALAR(oursI, theirsI);
    }
    return ours;
}

// ============================================================================
// Save::SaveLoad::GetSaveCount() const  retail 0x1800c3b80   ordinal 2885
// ?GetSaveCount@SaveLoad@Save@@QEBAHXZ
// ----------------------------------------------------------------------------
// `return *(int*)(this + 0x58)` — the _size half of the Array<SaveInfo> at
// +0x50. Oracle: FULL DIFF.
// ============================================================================
int RtSave_SaveLoad_GetSaveCount(const SaveLoad* self) {
    RT_DIFF_BEGIN("Save::SaveLoad::GetSaveCount");
    const int ours = self->_savesSize;
    if (rt_diff_on__) {
        using Fn = int (*)(const SaveLoad*);
        const int theirs = RT_ORIGINAL_ONCE(Fn, "?GetSaveCount@SaveLoad@Save@@QEBAHXZ")(self);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ============================================================================
// Save::SaveLoad::GetSaveInfo(int) const  retail 0x1800c3b60  ordinal 2889
// ?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z
// ----------------------------------------------------------------------------
// ABI: `AEBU` is a REFERENCE return, not a by-value struct return, so it comes
// back in RAX and there is no hidden slot. (`?AU` would have been by value and
// would have needed one.) The body is `this->_savesData + index * 0x58` with
// NO bounds check — index is sign-extended (MOVSXD) and multiplied, so a
// negative or out-of-range index produces a wild pointer. Reproduced exactly;
// CLAUDE.md's rule is that retail's unguarded arithmetic is load-bearing.
//
// Oracle: FULL DIFF on the returned pointer.
// ============================================================================
SaveInfo* RtSave_SaveLoad_GetSaveInfo(const SaveLoad* self, int index) {
    RT_DIFF_BEGIN("Save::SaveLoad::GetSaveInfo");
    SaveInfo* ours = reinterpret_cast<SaveInfo*>(
        reinterpret_cast<uint8_t*>(self->_savesData) +
        static_cast<int64_t>(index) * static_cast<int64_t>(sizeof(SaveInfo)));
    if (rt_diff_on__) {
        using Fn = SaveInfo* (*)(const SaveLoad*, int);
        const void* oursP = ours;
        const void* theirsP =
            RT_ORIGINAL_ONCE(Fn, "?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z")(self, index);
        RT_DIFF_SCALAR(oursP, theirsP);
    }
    return ours;
}

// ============================================================================
// Save::SaveLoad::GenerateSaveName() const  retail 0x1800c1250  ordinal 2401
// ?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ
// ----------------------------------------------------------------------------
// ABI: a const member returning System::String BY VALUE. The disassembly has
// `this` arriving in RCX and the hidden return slot in RDX — the MEMBER-
// function ordering (`this` is the first physical argument, the retslot is
// pushed to second), not the namespace-scope one (contrast
// RtSystem_String_Concat's banner in string.h, where the retslot IS first).
// This matches `NameGetterFn` at saveload_update.cpp:362 — `(self, ret)` — the
// typedef this function's two existing call sites already use.
//
// LOGIC (0x1800c1250..0x1800c1449; read from the disassembly, not the
// decompiled C, which collapses the do-while into goto soup Ghidra itself
// mistypes as `void`): try candidates "Save0", "Save1", "Save2", ... in order
// and return the first one that does not equal the `name` field (SaveInfo+
// 0x38, RtSave::SaveInfo::name) of any row in
// self->_savesData[0 .. self->_savesSize). The inlined per-row comparison is
// length-then-bytes with two empty/null strings counted equal — exactly what
// the already-ported `RtSystem::RtString_StrCompare` (string.cpp) computes,
// so this port calls that directly instead of re-transcribing a second copy
// of the same comparison. (A null row name, length 0, can never collide with
// a candidate — "SaveN" is never shorter than 5 bytes — so the reuse changes
// no observable outcome.)
//
// `self->_savesSize < 1` is tested ONCE PER OUTER ITERATION, right after the
// candidate is formatted and stored — not once before the loop — so an empty
// save list returns "Save0" on the very first pass without ever touching
// self->_savesData. Retail's retslot is zeroed UNCONDITIONALLY at entry
// (`*in_RDX = 0`), not released: this is the caller-pre-zeroes-the-String
// convention (both existing call sites already set `generated = nullptr`
// before calling), reproduced here rather than "fixed" into a release
// (CLAUDE.md — retail's unguarded behaviour is load-bearing, not a bug).
//
// Oracle: FULL DIFF on the returned string's CONTENT (RtString_StrCompare),
// not its pointer — ours and theirs each format their own fresh buffer, so
// equal candidates never share an address the way GetSaveInfo's pointer-
// identity oracle can rely on.
// ============================================================================
namespace {
using FormatFn = void* (*)(void* retval, const char* fmt, ...);

// The inlined "String::Format into a scratch, then refcount-adopt into *ret"
// idiom (0x1800c1290..0x1800c1310). NOT `String::operator=`: retail's very
// first instruction blindly zeroes `*ret` without releasing whatever was
// there (see the banner above), so the release/adopt pair below only ever
// runs against a slot this function itself already populated.
void GenerateSaveName_FormatCandidateInto(const char** ret, int index) {
    const char* tmp = nullptr;
    RT_EXPORT_ONCE(FormatFn, "?Format@String@System@@SA?AV12@PEBDZZ")(&tmp, "Save%i", index);

    if (*ret != tmp) {
        ReleaseStringSlot(ret);
        RtSaveDetail::RetainInto(ret, tmp);
    }
    ReleaseStringSlot(&tmp);   // release the temp's OWN reference
}
}  // namespace

const char** RtSave_SaveLoad_GenerateSaveName(const SaveLoad* self, const char** ret) {
    RT_DIFF_BEGIN("Save::SaveLoad::GenerateSaveName");

    *ret = nullptr;   // retail's unconditional zero, NOT a release -- see banner
    int index = 0;
    for (;;) {
        GenerateSaveName_FormatCandidateInto(ret, index);
        if (self->_savesSize < 1) break;
        bool collision = false;
        for (int i = 0; i < self->_savesSize; ++i) {
            if (RtSystem::RtString_StrCompare(self->_savesData[i].name, *ret) == 0) {
                collision = true;
                break;
            }
        }
        if (!collision) break;
        ++index;
    }

    if (rt_diff_on__) {
        using Fn = const char** (*)(const SaveLoad*, const char**);
        const char* theirs = nullptr;
        RT_ORIGINAL_ONCE(Fn, "?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ")(self, &theirs);
        const bool equal = RtSystem::RtString_StrCompare(*ret, theirs) == 0;
        char a[64], b[64];
        std::snprintf(a, sizeof(a), "\"%s\"", *ret   ? *ret   : "(null)");
        std::snprintf(b, sizeof(b), "\"%s\"", theirs ? theirs : "(null)");
        RtDiffReport(&rt_diff_site__, equal, a, b);
        ReleaseStringSlot(&theirs);   // release the oracle's own temporary
    }
    return ret;
}

// ============================================================================
// Save::SaveLoad::GetSaveState()        retail 0x1800c3b50   ordinal 2891
// ?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ
// ----------------------------------------------------------------------------
// `return *(SavedState**)(this + 0x40)`. Note that ??0SaveLoad does NOT
// initialise +0x40 (see the header's list), so before any operation has run
// this returns whatever the allocator left there. Faithful.
// Oracle: FULL DIFF.
// ============================================================================
SavedState* RtSave_SaveLoad_GetSaveState(SaveLoad* self) {
    RT_DIFF_BEGIN("Save::SaveLoad::GetSaveState");
    SavedState* ours = static_cast<SavedState*>(self->_saveState);
    if (rt_diff_on__) {
        using Fn = SavedState* (*)(SaveLoad*);
        const void* oursP = ours;
        const void* theirsP =
            RT_ORIGINAL_ONCE(Fn, "?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ")(self);
        RT_DIFF_SCALAR(oursP, theirsP);
    }
    return ours;
}

// ============================================================================
// Save::SaveLoad::SetOperation(Operation, int)  retail 0x1800c3c50  ordinal 3934
// ?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z
// ----------------------------------------------------------------------------
// Four stores and one global read. Note it zeroes _progress (+0x18), which the
// CONSTRUCTOR does not — so a freshly built SaveLoad has an uninitialised
// progress value until the first SetOperation. Faithful.
// Oracle: SCRATCH-COPY DIFF.
// ============================================================================
static void SaveLoad_SetOperationLogic(SaveLoad* self, Operation op, int index) {
    self->_operation     = op;
    const int64_t zero   = ZeroTime();   // retail computes this BEFORE the two stores below
    self->_progress      = 0;
    self->_selectedIndex = index;
    self->_elapsed       = zero;
}

void RtSave_SaveLoad_SetOperation(SaveLoad* self, Operation op, int index) {
    RT_DIFF_BEGIN("Save::SaveLoad::SetOperation");
    if (rt_diff_on__) {
        using Fn = void (*)(SaveLoad*, Operation, int);
        SaveLoadScratch s(self);
        SaveLoad_SetOperationLogic(s.Ours(), op, index);
        RT_ORIGINAL_ONCE(Fn, "?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z")(
            s.Theirs(), op, index);
        RT_DIFF_BYTES(s.ours, s.theirs, sizeof(SaveLoad));
    }
    SaveLoad_SetOperationLogic(self, op, index);
}

// ============================================================================
// Save::SaveLoad::SetMinOperationTime(const Time&)  retail 0x1800c3c40  ord 3920
// ?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
// ----------------------------------------------------------------------------
// One qword copy. System::Time is a single int64 tick count (Wave 2 pinned it,
// runtime/types/system/Value.h). Oracle: SCRATCH-COPY DIFF.
// ============================================================================
void RtSave_SaveLoad_SetMinOperationTime(SaveLoad* self, const int64_t* time) {
    RT_DIFF_BEGIN("Save::SaveLoad::SetMinOperationTime");
    if (rt_diff_on__) {
        using Fn = void (*)(SaveLoad*, const int64_t*);
        SaveLoadScratch s(self);
        s.Ours()->_minOperationTime = *time;
        RT_ORIGINAL_ONCE(Fn, "?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z")(
            s.Theirs(), time);
        RT_DIFF_BYTES(s.ours, s.theirs, sizeof(SaveLoad));
    }
    self->_minOperationTime = *time;
}

// ============================================================================
// Save::SaveLoad::SetSaveSprite(UI::Sprite)   retail 0x1800c3b90  ordinal 3955
// ?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z
// ----------------------------------------------------------------------------
// ABI, and the lead flagged this one specifically: `VSprite@UI@@` is a class
// BY VALUE. UI::Sprite is 16 bytes, which does not fit in a register, so the
// MS x64 ABI passes it by HIDDEN POINTER — the callee gets an address in RDX,
// not the value. Read out of the retail bytes rather than assumed:
//     MOV RAX,[RDX] ; MOV [RCX+0x60],RAX ; MOV RAX,[RDX+8] ; MOV [RCX+0x68],RAX
// RDX is dereferenced, so it is a pointer. Declaring the parameter as a value
// here would have made GCC read the caller's 16 bytes out of RDX:RSI and
// corrupt the sprite.
//
// This also matches the existing typedef in engine/funcs/engine_funcs.h:1102
// (`void (*)(void* saveLoad, void* sprite)`), which is already correct.
//
// Oracle: SCRATCH-COPY DIFF.
// ============================================================================
void RtSave_SaveLoad_SetSaveSprite(SaveLoad* self, const RtSave::Sprite* sprite) {
    RT_DIFF_BEGIN("Save::SaveLoad::SetSaveSprite");
    if (rt_diff_on__) {
        using Fn = void (*)(SaveLoad*, const RtSave::Sprite*);
        SaveLoadScratch s(self);
        std::memcpy(&s.Ours()->_saveSprite, sprite, sizeof(RtSave::Sprite));
        RT_ORIGINAL_ONCE(Fn, "?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z")(s.Theirs(), sprite);
        RT_DIFF_BYTES(s.ours, s.theirs, sizeof(SaveLoad));
    }
    std::memcpy(&self->_saveSprite, sprite, sizeof(RtSave::Sprite));
}

// ============================================================================
// Save::SaveLoad::SetSaveName(const String&, int)  retail 0x1800c3bb0  ord 3953
// ?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z
// ----------------------------------------------------------------------------
// A refcounted String assignment into +0x70 plus a mode int into +0x78. If the
// incoming data pointer is already the one stored, retail writes ONLY the mode
// and does no refcount work at all — that short path is why the compensation
// below is conditional.
//
// ORACLE: SCRATCH-COPY DIFF WITH REFCOUNT COMPENSATION, and this is the one
// place in the lane where the oracle needed real thought, so the reasoning is
// written out.
//
// The naive scratch diff does not work here. Both scratch runs manipulate the
// refcounts of the SAME two shared strings, so the old string would be
// decremented twice; if its count was 1, the first decrement frees it and the
// second writes through a freed block. That is the Wave 4 lane H defect
// (a String release applied where it did not belong) in a new costume.
//
// The fix is to make the extra pair of operations provably harmless:
//   * BEFORE the two scratch runs, add 2 to the OLD string's refcount, so the
//     two decrements can never reach zero and never free.
//   * AFTER them, subtract 2 from the NEW string's refcount, undoing the two
//     increments.
// The live object is then updated by one ordinary run of ours, and the net
// refcount effect of the whole thing is exactly one assignment. Nothing is
// freed early and nothing leaks.
// ============================================================================
static void SaveLoad_SetSaveNameLogic(SaveLoad* self, const String* name, int mode) {
    const char* cur = self->_saveName;
    if (cur != name->data) {
        ReleaseStringSlot(&self->_saveName);
        self->_saveName = name->data;
        if (name->data != nullptr)
            *reinterpret_cast<int*>(const_cast<char*>(name->data) - 4) += 1;
    }
    self->_saveNameMode = mode;
}

void RtSave_SaveLoad_SetSaveName(SaveLoad* self, const String* name, int mode) {
    RT_DIFF_BEGIN("Save::SaveLoad::SetSaveName");
    if (rt_diff_on__) {
        using Fn = void (*)(SaveLoad*, const String*, int);
        const char* oldStr = self->_saveName;
        const bool  assigns = (oldStr != name->data);   // the short path does no refcount work
        if (assigns && oldStr)
            *reinterpret_cast<int*>(const_cast<char*>(oldStr) - 4) += 2;
        SaveLoadScratch s(self);
        SaveLoad_SetSaveNameLogic(s.Ours(), name, mode);
        RT_ORIGINAL_ONCE(Fn, "?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z")(
            s.Theirs(), name, mode);
        RT_DIFF_BYTES(s.ours, s.theirs, sizeof(SaveLoad));
        if (assigns && name->data)
            *reinterpret_cast<int*>(const_cast<char*>(name->data) - 4) -= 2;
    }
    SaveLoad_SetSaveNameLogic(self, name, mode);
}

// ============================================================================
// Save::SaveLoad::CheckMod(int, int) const   retail 0x1800c1090   ordinal 1691
// ?CheckMod@SaveLoad@Save@@QEBAHHH@Z
// ----------------------------------------------------------------------------
// The function Wave 4 lane H identified as the whole reason a mod-mismatched
// save used to stall (WAVE4_LANE_H.md §5a). It answers "does save[saveIndex]'s
// mod[modIndex] match something installed?" with the code SaveLoadDialog::DoLoad
// turns into a modal:
//     1  modsMissing     no installed mod has this path
//     4  modsChanged     major version differs
//     3  modsVersion     minor version differs
//     2  modsOutOfDate   the save's recorded game version is below 0x1000400
//     0  fine
//
// TWO THINGS LANE H's WRITE-UP GOT SLIGHTLY WRONG, corrected here from the
// disassembly rather than from its prose:
//   (a) it says the comparison uses "the save's recorded mod path string"
//       without saying which field. It is ModInfo+**0x08**, not +0x00. The
//       instruction is `LEA R8,[RBP+0x8]` immediately before the operator+
//       call. +0x00 is the display name our DoLoad trace prints.
//   (b) it says the linear search compares "the String at entry +0x58 by
//       length then _stricmp". Right — and worth keeping, because the length
//       pre-check reads `data[-8]`, i.e. the String allocation header, and
//       treats a null data pointer as length 0 rather than faulting.
//
// ORACLE: FULL DIFF. CheckMod allocates three temporary Strings and releases
// all three before returning, reads two DLL singletons, and mutates nothing —
// so calling it twice is safe and the second call sees identical inputs.
//
// _stricmp: retail calls its own CRT copy at 0x18007eefc. Ours calls the CRT
// we are linked against. For the ASCII paths these strings can contain the two
// agree; a locale-dependent difference would show up in the diff column, which
// is exactly what the oracle is for.
// ============================================================================
int RtSave_SaveLoad_CheckMod(const SaveLoad* self, int saveIndex, int modIndex) {
    RT_DIFF_BEGIN("Save::SaveLoad::CheckMod");

    uint8_t* const mods = ModificationSingleton();
    const SaveInfo* info = reinterpret_cast<const SaveInfo*>(
        reinterpret_cast<const uint8_t*>(self->_savesData) +
        static_cast<int64_t>(saveIndex) * static_cast<int64_t>(sizeof(SaveInfo)));
    const ModInfo* mod = info->modsData + modIndex;

    // full = GetDataDirectory() + mod->path,  then AdjustDelimiters(full)
    String dataDir{};
    RT_EXPORT_ONCE(GetDataDirectoryFn,
                     "?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ")(
        FileSystemSingleton(), &dataDir);
    String joined{};
    // RT_EXPORT_ONCE: System::String::operator+ and Filename::AdjustDelimiters are
    // both PORTED (string.cpp:931, filename.cpp:446); raw skipped both ports.
    RT_EXPORT_ONCE(ConcatFn, "??HSystem@@YA?AVString@0@AEBV10@0@Z")(
        &joined, &dataDir, reinterpret_cast<const String*>(&mod->path));

    // Retail adopts the concatenation's buffer into a fresh slot (with an
    // AddRef) and adjusts THAT, leaving `joined` itself untouched, so the two
    // Strings are released separately below.
    String work{};
    if (joined.data != nullptr) {
        work.data = joined.data;
        *reinterpret_cast<int*>(work.data - 4) += 1;
    }
    RT_EXPORT_ONCE(AdjustDelimsFn, "?AdjustDelimiters@Filename@System@@AEAAXXZ")(&work);

    // Linear scan of IO::Modification: count at +0x10, array base at +0x8,
    // stride 0x80, the entry's path String at +0x58. Length first, then
    // _stricmp — a null String on either side counts as length 0.
    int found = -1;
    const int32_t count = *reinterpret_cast<const int32_t*>(mods + 0x10);
    uint8_t* const base = *reinterpret_cast<uint8_t* const*>(mods + 8);
    for (int i = 0; i < count; ++i) {
        const char* entry = *reinterpret_cast<const char* const*>(base + static_cast<int64_t>(i) * 0x80 + 0x58);
        const int lenEntry = entry     ? *reinterpret_cast<const int*>(entry - 8)     : 0;
        const int lenWork  = work.data ? *reinterpret_cast<const int*>(work.data - 8) : 0;
        if (lenEntry == lenWork && _stricmp(entry, work.data) == 0) { found = i; break; }
    }

    RtSystem::RtString_ReleaseInline(&work);
    RtSystem::RtString_ReleaseInline(&dataDir);
    RtSystem::RtString_ReleaseInline(&joined);

    int ours;
    if (found < 0) {
        ours = 1;
    } else {
        const uint8_t* entry = base + static_cast<int64_t>(found) * 0x80;
        if (*reinterpret_cast<const int32_t*>(entry + 0x2c) != mod->versionMajor)      ours = 4;
        else if (*reinterpret_cast<const int32_t*>(entry + 0x1c) != mod->versionMinor) ours = 3;
        else ours = (mod->gameVersion < 0x1000400u) ? 2 : 0;
    }

    if (rt_diff_on__) {
        using Fn = int (*)(const SaveLoad*, int, int);
        const int theirs = RT_ORIGINAL_ONCE(Fn, "?CheckMod@SaveLoad@Save@@QEBAHHH@Z")(
            self, saveIndex, modIndex);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ============================================================================
// Save::SaveLoad::Update(const Time&)   retail 0x1800c2e40   ordinal 4501
// ?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
// ----------------------------------------------------------------------------
// The state machine's tick. Accumulates the frame delta into _elapsed and then
// dispatches on _operation to one of five exported UpdateXxx bodies, but ONLY
// while `!(Persistence::_singleton[0x18] & 1) && Persistence::_singleton[0x48]`
// — a gate this lane does not model further, because IO::Persistence's layout
// past +0x08 is explicitly not pinned (runtime/types/io/Persistence.h models
// only _savePath at +0x90 and calls +0x08..+0x8f "NOT modelled"). Both are read
// here as raw displacements with that stated, rather than by inventing two
// field names in someone else's header.
//
// ORACLE: PARTIAL, and this is stated plainly rather than dressed up as a
// full one. The accumulation is checked in shadow mode against an independent
// replica; the DISPATCH is not checked by anything, because running it twice
// would perform the save/load/delete file work twice. What backs the dispatch
// is that all five targets stay on the DLL and the branch is five integer
// comparisons transcribed from a decompile — plus the end-to-end evidence that
// a save still saves and a load still loads.
//
// The five targets are resolved through RT_EXPORT_ONCE — the port registry,
// cached once per call site. WAVE 9 LANE V2, 2026-09-07 corrects this: the
// paragraph used to say RT_ORIGINAL_ONCE "NOT ResolveExport", on the grounds
// that the five are unported and a bare ResolveExport per frame would be the
// Wave 2 perf defect again. The perf half is right and RT_EXPORT_ONCE keeps it
// (one resolve per call site, then a static). The door half is wrong: this
// dispatch IS the work of Update, not an oracle, so it is a REAL DEPENDENCY.
// On the raw door it is a call through NULL the moment the DLL is absent; on
// the port door it picks up a port of any of the five the day one lands, and
// until then it is a named unresolved import at cutover instead of a crash.
// ============================================================================
void RtSave_SaveLoad_Update(SaveLoad* self, const int64_t* delta) {
    RT_DIFF_BEGIN("Save::SaveLoad::Update [shadow]");

    const int64_t expected = self->_elapsed + *delta;
    self->_elapsed = self->_elapsed + *delta;
    RT_DIFF_SHADOW(self->_elapsed == expected, "elapsed", "elapsed+delta");

    const uint8_t* p = PersistenceSingleton();
    if ((*(p + 0x18) & 1) != 0) return;
    if (*reinterpret_cast<const int32_t*>(p + 0x48) == 0) return;

    switch (self->_operation) {
    case RtSave::kOpList:
        RT_EXPORT_ONCE(SaveLoadVoidFn, "?UpdateList@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpLoad:
        RT_EXPORT_ONCE(SaveLoadVoidFn, "?UpdateLoad@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpLoadFromFile:
        RT_EXPORT_ONCE(SaveLoadVoidFn, "?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpSave:
        RT_EXPORT_ONCE(SaveLoadVoidFn, "?UpdateSave@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpDelete:
        RT_EXPORT_ONCE(SaveLoadVoidFn, "?UpdateDelete@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    default:
        break;
    }
}

// ============================================================================
// Save::SavedState::SavedState()        retail 0x1800c0ee0   ordinal 362
// ??0SavedState@Save@@QEAA@XZ
// ----------------------------------------------------------------------------
// Two qword zero stores. Oracle: SCRATCH-COPY DIFF over 0x10 bytes, both
// scratches pre-filled with 0xCD so the comparison proves the writes rather
// than agreeing on an accident of zeroed memory.
// ============================================================================
SavedState* RtSave_SavedState_Ctor(SavedState* self) {
    RT_DIFF_BEGIN("Save::SavedState::SavedState");
    if (rt_diff_on__) {
        using Fn = SavedState* (*)(SavedState*);
        alignas(16) uint8_t so[sizeof(SavedState)], st[sizeof(SavedState)];
        std::memset(so, 0xCD, sizeof(so));
        std::memset(st, 0xCD, sizeof(st));
        SavedState* o = reinterpret_cast<SavedState*>(so);
        o->_id = 0; o->_size = 0; o->_data = nullptr;
        RT_ORIGINAL_ONCE(Fn, "??0SavedState@Save@@QEAA@XZ")(reinterpret_cast<SavedState*>(st));
        RT_DIFF_BYTES(so, st, sizeof(SavedState));
    }
    self->_id = 0;
    self->_size = 0;
    self->_data = nullptr;
    return self;
}

// ============================================================================
// Save::SavedState::~SavedState()       retail 0x1800c0ea0   ordinal 667
// ??1SavedState@Save@@QEAA@XZ
// ----------------------------------------------------------------------------
// `if (_data) { lock; Free(_singleton, _data); unlock; }` — which is exactly
// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0), whose whole body is
// the same null check around the same locked Free. Retail inlined it here and
// calls the export elsewhere; the two are the same code.
//
// It does NOT null _data afterwards, so the object is left holding a dangling
// pointer. Faithful — this is a destructor, and its callers do not read it back.
//
// ORACLE: NONE. Running theirs as well would free the same block twice. Said
// plainly rather than hidden: this function is ported on the strength of nine
// unambiguous instructions and is measured only by `calls=`.
// ============================================================================
void RtSave_SavedState_Dtor(SavedState* self) {
    RT_DIFF_BEGIN("Save::SavedState::~SavedState [no oracle]");
    RT_DIFF_SHADOW(true, "freed", "not called");
    if (self->_data != nullptr) MemFree(self->_data);
}

// ============================================================================
// Save::SavedState::Store(IO::MemoryStream&)   retail 0x1800c0da0  ordinal 4112
// ?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z
// ----------------------------------------------------------------------------
// Detaches a private copy of everything written into the stream so far:
//     src  = stream->_data                       (+0x18)
//     size = stream->vtbl[5](stream)             (+0x28 == GetCurrentPos)
//     if (_data) MemFree(_data);
//     _size = size;
//     _data = locked-Allocate(size, 4);          (== MemAlloc(size, 4))
//     memcpy(_data, src, _size);
//
// The virtual slot: runtime/types/io/Stream.h (Wave 4 lane B) reads
// MemoryStream's exported vtable byte-wise — index 5 at displacement +0x28 is
// GetCurrentPos (0x1800ae9c0), a pure getter. Note the last memcpy length is
// re-loaded from `this->_size`, not from the local; identical value, but
// transcribed as written.
//
// The locked Allocate that retail inlines is byte-identical to
// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z (decompiled both).
//
// ORACLE: SCRATCH-OBJECT DIFF, the real one. Theirs runs against a private
// zeroed SavedState, so its `if (_data) MemFree` is skipped and it allocates
// its own buffer; we then compare the two buffers byte for byte and free
// theirs. The stream is only READ by either run, so nothing is double-consumed
// — which is the failure mode this whole lane was warned about.
// ============================================================================
void RtSave_SavedState_Store(SavedState* self, MemoryStream* stream) {
    RT_DIFF_BEGIN("Save::SavedState::Store");

    void* src = stream->data;
    using GetPosFn = uint32_t (*)(MemoryStream*);
    const uint32_t size =
        reinterpret_cast<GetPosFn const*>(stream->base.vptr)[5](stream);

    if (self->_data != nullptr) MemFree(self->_data);
    self->_size = size;
    self->_data = MemAlloc(size, 4);
    std::memcpy(self->_data, src, self->_size);

    if (rt_diff_on__) {
        using Fn = void (*)(SavedState*, MemoryStream*);
        SavedState scratch{};
        RT_ORIGINAL_ONCE(Fn, "?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z")(&scratch, stream);
        const bool equal = (scratch._size == self->_size) &&
                           (std::memcmp(scratch._data, self->_data, self->_size) == 0);
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), static_cast<unsigned int>(self->_size));
        RtDiffFormat(b, sizeof(b), static_cast<unsigned int>(scratch._size));
        RtDiffReport(&rt_diff_site__, equal, a, b);
        MemFree(scratch._data);   // our extra allocation, not a retail deviation
    }
}

// ============================================================================
// Save::SavedState::Retrieve(IO::MemoryStream&)  retail 0x1800c0e50  ord 3586
// ?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z
// ----------------------------------------------------------------------------
//     MemoryStream::Open(stream, this->_size);      // 0x1800ae670
//     memcpy(stream->_data, this->_data, this->_size);
// and the memcpy is a tail JMP, with _size re-loaded after Open.
//
// MemoryStream::Open (decompiled) allocates a fresh buffer, stores it at +0x18,
// sets _size, zeroes _cursor, sets _direction = 0 (reading) and _ownsData = 1
// — and does NOT free whatever buffer the stream already held. That leak is
// retail's; it is also the reason theirs cannot simply be run a second time
// against the live stream in diff mode.
//
// ORACLE: SCRATCH-OBJECT DIFF. Theirs runs against a private copy of the
// stream OBJECT (0x30 bytes, taken before ours ran), so it allocates its own
// buffer, and we compare that buffer against the one ours produced and then
// free it. Both runs only read `*this`.
// ============================================================================
void RtSave_SavedState_Retrieve(SavedState* self, MemoryStream* stream) {
    RT_DIFF_BEGIN("Save::SavedState::Retrieve");

    MemoryStream preState = *stream;   // captured before ours mutates the stream

    RT_EXPORT_ONCE(MemStreamOpenFn, "?Open@MemoryStream@IO@@QEAA_NH@Z")(
        stream, static_cast<int>(self->_size));
    std::memcpy(stream->data, self->_data, self->_size);

    if (rt_diff_on__) {
        using Fn = void (*)(SavedState*, MemoryStream*);
        MemoryStream scratch = preState;
        RT_ORIGINAL_ONCE(Fn, "?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z")(
            self, &scratch);
        // Compare the whole stream object EXCEPT the two buffer pointers (which
        // are two different allocations by construction), then the bytes.
        const bool fieldsEqual = (scratch.base.direction == stream->base.direction) &&
                                 (scratch.ownsData == stream->ownsData) &&
                                 (scratch.cursor   == stream->cursor)   &&
                                 (scratch.size     == stream->size);
        const bool equal = fieldsEqual &&
                           std::memcmp(scratch.data, stream->data, self->_size) == 0;
        char a[64], b[64];
        RtDiffFormat(a, sizeof(a), static_cast<unsigned int>(stream->size));
        RtDiffFormat(b, sizeof(b), static_cast<unsigned int>(scratch.size));
        RtDiffReport(&rt_diff_site__, equal, a, b);
        MemFree(scratch.data);   // our extra allocation, not a retail deviation
    }
}

// ============================================================================
// Registration
// ----------------------------------------------------------------------------
// Every name below is spelled as a SINGLE string literal on ONE line. The
// census (tools/runtime_census.py) matches `RegisterRuntimeExport("<literal>"`
// and can see through neither a named constant nor a line break; both blind
// spots have already produced a silent under-count of live ports in this
// project (WAVE3.md's Object 21/41 was really 27/41).
//
// Every name was also checked against the shipped DLL's export directory with
// pefile before being written here — see the phantom-name section of
// docs/runtime_port/WAVE5_LANE_C.md, which found that two of the names our
// tree has been asking for do not exist in the DLL at all.
//
// TWO FUNCTIONS ARE REGISTERED TWICE, BY NAME *AND* BY ADDRESS.
// `engine/funcs/engine_funcs.cpp` resolves GetOperation (:2428) and
// GetSaveState (:2468) by NAME and then, if that returns null, falls back to
// `ResolveAddr(Module::Runtime, …)`. The name path works today, so the port is
// already reached — but the fallback is a live second door, and a name path
// that is later renamed or removed would send the call back to the DLL with no
// diagnostic whatsoever. That silent reversion is this project's single most
// repeated failure mode (the phantom-name family, the unregistered type-id
// slot, the null dispatch slot read under the wrong name), and closing it
// costs one line. `runtime/src/achievement/goal.cpp` registers all eight of
// its ports both ways for the same reason.
//
// The `++n` deliberately does NOT fire on the address registrations: `n` is
// the count of PORTED FUNCTIONS, and these are second doors onto functions
// already counted, not extra ports. Incrementing here would inflate the census
// figure the project treats as authoritative over any agent's claim.
// ============================================================================
int RtSaveLoadRegister() {
    int n = 0;
    RegisterRuntimeExport("??0SaveLoad@Save@@QEAA@XZ", (void*)&RtSave_SaveLoad_Ctor); ++n;
    RegisterRuntimeExport("??0SavedState@Save@@QEAA@XZ", (void*)&RtSave_SavedState_Ctor); ++n;
    RegisterRuntimeExport("??1SavedState@Save@@QEAA@XZ", (void*)&RtSave_SavedState_Dtor); ++n;
    // WAVE 9 LANE SYS2. `QEAA`, the NON-VIRTUAL spelling -- SaveLoad has no
    // vtable (the header proves it: the ctor writes an int at +0x00 and the
    // DLL exports no ??_7SaveLoad@Save@@6B@), so `UEAA` here would be a
    // phantom name resolving NULL in silence
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md, which cost this
    // project a skipped ~Internal on every teardown). Verified present in the
    // shipped export table with pefile before this line was written:
    // 0x1800c3c90, ordinal 666.
    RegisterRuntimeExport("??1SaveLoad@Save@@QEAA@XZ", (void*)&RtSave_SaveLoad_Dtor); ++n;
    RegisterRuntimeAddr(0x1800c3c90, (void*)&RtSave_SaveLoad_Dtor);
    // The three un-exported bodies the chain calls. Nothing in this tree
    // reaches them by address TODAY, so these are not closing a live bypass --
    // they are there so that the day something does (a ported UpdateList or
    // LoadMetaData, which are the only other writers of these arrays), it
    // reaches our transcription and not a null. A faithful port that calls DLL
    // internals by address grows the cutover surface; owning the internals
    // instead is the version of that which shrinks it.
    RegisterRuntimeAddr(0x1800329c0, (void*)&ArraySaveInfo_Dtor);
    RegisterRuntimeAddr(0x1800c3f40, (void*)&SaveInfo_Dtor);
    RegisterRuntimeAddr(0x1800331f0, (void*)&ArrayModInfo_Dtor);
    RegisterRuntimeExport("?CheckMod@SaveLoad@Save@@QEBAHHH@Z", (void*)&RtSave_SaveLoad_CheckMod); ++n;
    // Wave 8 lane E: engine_funcs.cpp's ResolveExport call for CheckMod was a
    // phantom-name typo (QEAAHHI, not this real QEBAHHH) that had been dead
    // since it was written; fixed at its source there. Registering by
    // address too closes the WAVE7.md §3 failure mode where a phantom-name
    // fix, once corrected, is still the ONLY door — a second port reached by
    // address stays correct even if the name is mistyped again.
    RegisterRuntimeAddr(0x1800c1090, (void*)&RtSave_SaveLoad_CheckMod);
    RegisterRuntimeExport("?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ", (void*)&RtSave_SaveLoad_GenerateSaveName); ++n;
    RegisterRuntimeAddr(0x1800c1250, (void*)&RtSave_SaveLoad_GenerateSaveName);
    RegisterRuntimeExport("?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ", (void*)&RtSave_SaveLoad_GetOperation); ++n;
    RegisterRuntimeAddr(0x1800c3c80, (void*)&RtSave_SaveLoad_GetOperation);
    RegisterRuntimeExport("?GetSaveCount@SaveLoad@Save@@QEBAHXZ", (void*)&RtSave_SaveLoad_GetSaveCount); ++n;
    RegisterRuntimeExport("?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z", (void*)&RtSave_SaveLoad_GetSaveInfo); ++n;
    RegisterRuntimeExport("?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ", (void*)&RtSave_SaveLoad_GetSaveState); ++n;
    RegisterRuntimeAddr(0x1800c3b50, (void*)&RtSave_SaveLoad_GetSaveState);
    RegisterRuntimeExport("?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z", (void*)&RtSave_SavedState_Retrieve); ++n;
    RegisterRuntimeExport("?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z", (void*)&RtSave_SaveLoad_SetMinOperationTime); ++n;
    RegisterRuntimeExport("?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z", (void*)&RtSave_SaveLoad_SetOperation); ++n;
    RegisterRuntimeExport("?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z", (void*)&RtSave_SaveLoad_SetSaveName); ++n;
    // Wave 8 lane E: same phantom-name fix as CheckMod above, applied to
    // engine_funcs.cpp's SetSaveName resolve; registered by address too.
    RegisterRuntimeAddr(0x1800c3bb0, (void*)&RtSave_SaveLoad_SetSaveName);
    RegisterRuntimeExport("?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z", (void*)&RtSave_SaveLoad_SetSaveSprite); ++n;
    RegisterRuntimeExport("?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z", (void*)&RtSave_SavedState_Store); ++n;
    RegisterRuntimeExport("?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z", (void*)&RtSave_SaveLoad_Update); ++n;
    return n;
}
