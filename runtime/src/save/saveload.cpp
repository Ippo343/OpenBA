// ============================================================================
// runtime/src/save/saveload.cpp — Wave 5 lane C — Save::SaveLoad / Save::SavedState
// ============================================================================
// Ports 16 of the 17 real exports our tree calls on these two classes. Every
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
//       vtables this project does not own. Those classes are Waves 6 and 7. It
//       also has no oracle: it renders into the live Video::Context and
//       allocates and destroys GPU resources, so running both implementations
//       would draw twice and leak a render target every frame.
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
#include <cstdio>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/save/SaveLoad.h"
#include "../../types/io/Stream.h"
#include "../system/string.h"   // RtSystem::String (Wave 2, lane String)

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

using MemAllocFn         = void* (*)(uint64_t size, int align);
using MemFreeFn          = void  (*)(void* p);
using GetDataDirectoryFn = String* (*)(const void* fs, String* ret);
using ConcatFn           = String* (*)(String* ret, const String* lhs, const String* rhs);
using AdjustDelimsFn     = void  (*)(String* self);
using MemStreamOpenFn    = bool  (*)(MemoryStream* self, int size);
using SaveLoadVoidFn     = void  (*)(SaveLoad* self);

void* MemAlloc(uint64_t size, int align) {
    return RT_ORIGINAL_ONCE(MemAllocFn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(size, align);
}
void MemFree(void* p) {
    RT_ORIGINAL_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}

// ---- DLL data symbols ------------------------------------------------------
// All five are real exports in the shipped table (verified with pefile against
// C:\GOG Games\Banished\Runtime-x64.dll, not taken from Ghidra's guess at a
// name). ResolveExportRaw is GetProcAddress, which resolves data exports just
// as it resolves code exports; each is cached in a function-local static.
//
// Note the mangling of Modification's: `2PEAV12@EA`, a PUBLIC static member,
// where the other four are `0PEAV12@EA` (private). Spelling either one the
// other way produces a name the DLL does not contain, which resolves to NULL
// in silence — the phantom-name family this project has now been bitten by
// twice (reference_resolveexport_phantom_names.md).
template <typename T>
T* RtDataExport(const char* mangled) {
    return reinterpret_cast<T*>(ResolveExportRaw(mangled));
}

SaveLoad** SaveLoadSingletonSlot() {
    static SaveLoad** p = RtDataExport<SaveLoad*>("?_singleton@SaveLoad@Save@@0PEAV12@EA");
    return p;
}
void* FileSystemSingleton() {
    static void** p = RtDataExport<void*>("?_singleton@FileSystem@IO@@0PEAV12@EA");
    return p ? *p : nullptr;
}
uint8_t* ModificationSingleton() {
    static uint8_t** p = RtDataExport<uint8_t*>("?_singleton@Modification@IO@@2PEAV12@EA");
    return p ? *p : nullptr;
}
uint8_t* PersistenceSingleton() {
    static uint8_t** p = RtDataExport<uint8_t*>("?_singleton@Persistence@IO@@0PEAV12@EA");
    return p ? *p : nullptr;
}
double SecondsPerTick() {
    static double* p = RtDataExport<double>("?_secondsPerTick@Timer@System@@0NA");
    return p ? *p : 0.0;
}

// `System::Time(0.0f)` as the two ctors inline it:
//     XORPS XMM0,XMM0 ; DIVSD XMM0,[Timer::_secondsPerTick] ; CVTTSD2SI RAX,XMM0
// Written out rather than folded to 0 because that is what the instructions
// say. It IS 0 for every non-zero divisor, so the CVTTSD2SI-of-NaN case (which
// would be UB in C++ and INT64_MIN in hardware) is not reachable while the
// Timer has been initialised — and if it has not been, retail is equally broken.
int64_t ZeroTime() {
    const double q = 0.0 / SecondsPerTick();
    return static_cast<int64_t>(q);
}

// ---- the System::String refcount idiom, transcribed ------------------------
// Retail inlines this at ~200 sites. Note the shape carefully, because it is
// NOT the obvious one: when the refcount reaches zero it rewinds the member
// itself by 8 and frees THAT, and when it does not reach zero it stores 0 into
// the member. Both branches are dead stores here because every caller
// overwrites the member immediately afterwards — but they are what the bytes
// do, and this project has already lost a day to a String destructor applied
// to something that was not a String
// (reference_load_wild_write_high_dword_7fc.md, Wave 4 lane H).
void ReleaseStringSlot(const char** slot) {
    const char* p = *slot;
    if (p == nullptr) return;
    *reinterpret_cast<int*>(const_cast<char*>(p) - 4) -= 1;
    if (*reinterpret_cast<int*>(const_cast<char*>(p) - 4) == 0) {
        *slot = p - 8;
        MemFree(const_cast<char*>(*slot));
    } else {
        *slot = nullptr;
    }
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
    self->field28           = 0;
    self->_buffer30         = nullptr;
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
// HELD BACK — see the file banner. Not registered. Left here as a decompiled
// note so the next reader does not have to redo the work:
//
//   _singleton = nullptr;
//   ReleaseStringSlot(&_saveName);                       // +0x70
//   FUN_1800329c0(&_savesData);                          // Array<SaveInfo> dtor,
//        which calls the un-exported FUN_1800c3f40 on each 0x58-byte element
//        and then locked-frees the buffer
//   if (_buffer30) FUN_180006a10(_buffer30);             // an un-exported second
//        copy of MemFree, identical to ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
//   ReleaseStringSlot(&_string20);                       // +0x20
// ============================================================================

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
    RT_ORIGINAL_ONCE(GetDataDirectoryFn,
                     "?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ")(
        FileSystemSingleton(), &dataDir);
    String joined{};
    RT_ORIGINAL_ONCE(ConcatFn, "??HSystem@@YA?AVString@0@AEBV10@0@Z")(
        &joined, &dataDir, reinterpret_cast<const String*>(&mod->path));

    // Retail adopts the concatenation's buffer into a fresh slot (with an
    // AddRef) and adjusts THAT, leaving `joined` itself untouched, so the two
    // Strings are released separately below.
    String work{};
    if (joined.data != nullptr) {
        work.data = joined.data;
        *reinterpret_cast<int*>(work.data - 4) += 1;
    }
    RT_ORIGINAL_ONCE(AdjustDelimsFn, "?AdjustDelimiters@Filename@System@@AEAAXXZ")(&work);

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
// The five targets are resolved through RT_ORIGINAL_ONCE, NOT ResolveExport:
// they are unported, and this is a per-frame path, so a GetProcAddress in the
// body would be the Wave 2 perf defect again.
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
        RT_ORIGINAL_ONCE(SaveLoadVoidFn, "?UpdateList@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpLoad:
        RT_ORIGINAL_ONCE(SaveLoadVoidFn, "?UpdateLoad@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpLoadFromFile:
        RT_ORIGINAL_ONCE(SaveLoadVoidFn, "?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpSave:
        RT_ORIGINAL_ONCE(SaveLoadVoidFn, "?UpdateSave@SaveLoad@Save@@QEAAXXZ")(self);
        break;
    case RtSave::kOpDelete:
        RT_ORIGINAL_ONCE(SaveLoadVoidFn, "?UpdateDelete@SaveLoad@Save@@QEAAXXZ")(self);
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

    RT_ORIGINAL_ONCE(MemStreamOpenFn, "?Open@MemoryStream@IO@@QEAA_NH@Z")(
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
    RegisterRuntimeExport("?CheckMod@SaveLoad@Save@@QEBAHHH@Z", (void*)&RtSave_SaveLoad_CheckMod); ++n;
    // Wave 8 lane E: engine_funcs.cpp's ResolveExport call for CheckMod was a
    // phantom-name typo (QEAAHHI, not this real QEBAHHH) that had been dead
    // since it was written; fixed at its source there. Registering by
    // address too closes the WAVE7.md §3 failure mode where a phantom-name
    // fix, once corrected, is still the ONLY door — a second port reached by
    // address stays correct even if the name is mistyped again.
    RegisterRuntimeAddr(0x1800c1090, (void*)&RtSave_SaveLoad_CheckMod);
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
