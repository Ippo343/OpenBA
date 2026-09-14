// ============================================================================
// runtime/src/achievement/achievement_state.cpp — Wave 23, lane ACHIEVE
// The five Achievement lifetime/state members Wave 5 lane B left on the DLL
// ============================================================================
//
//   ??1AchievementManager@Achievement@@QEAA@XZ              0x18009eab0
//   ?RemoveAll@AchievementManager@Achievement@@QEAAXXZ      0x18009e9d0
//   ?ReadAchievements@AchievementManager@Achievement@@QEAAXXZ  0x18009eb90
//   ?SetAchievementMet@AchievementManager@Achievement@@QEAA_NH@Z 0x18009eca0
//   ?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@
//        VAchievementInstance@Achievement@@@Object@@XZ      0x18009df80
//
// Every address is in RETAIL Runtime-x64.dll (image base 0x180000000), read in
// Ghidra on 2026-09-11 as BOTH decompiled C and raw disassembly. Where the two
// disagree the disassembly wins (reference_ghidra_c_output_vs_disassembly.md).
//
// ----------------------------------------------------------------------------
// THE DESTRUCTOR, AND THE TRAP THE WAVE 23 BRIEF NAMED
// ----------------------------------------------------------------------------
// The brief warned that porting a dtor "exactly as prescribed" once swapped a
// body that destroys EVERY element for one that destroys only the first. The
// hazard here turns out to be the MIRROR of that, and it is worth writing down
// because the natural C++ instinct gets it wrong in the other direction:
//
//   ??1AchievementManager DESTROYS NO ELEMENTS AT ALL.
//
// Read at 0x18009eab0 the whole body is four effects — a null store into the
// singleton cell and three CALLs, each to a LEAF that frees one buffer:
//
//     MOV qword ptr [0x1801fc670],0x0      ; _singleton = nullptr, FIRST
//     ADD RCX,0x30 / CALL 0x180009700      ; ~Array(_updateTimes)   +0x30
//     LEA RCX,[RBX+0x18] / CALL 0x180008e80; ~Array(_instances)     +0x18
//     LEA RCX,[RBX+0x8]  / JMP  0x180009560; ~Array(_completed)     +0x08
//
// There is no loop and no vtable call anywhere in it. The AchievementInstance
// objects hanging off _instances are NOT deleted here — `Shutdown` calls
// `RemoveAll` for that, and a manager torn down without a prior Shutdown leaks
// them. That is retail's behaviour, it is load-bearing for the ordering below,
// and "helpfully" adding an element loop would DOUBLE-FREE every instance on
// the normal Shutdown-then-destruct path.
//
// Two more things a tidy-up would break:
//   * the singleton is nulled BEFORE the arrays are freed, not after. Anything
//     that reads `_singleton` from inside a free hook sees null, not a
//     half-destroyed object.
//   * the three arrays go in REVERSE declaration order (+0x30, +0x18, +0x08),
//     which is what MSVC emits and what an allocator trace records.
//
// All three callees (0x180009700 / 0x180008e80 / 0x180009560) are the SAME
// eleven-instruction leaf, byte-for-byte: `MemFree(p); _data = 0; {_size,
// _capacity} = 0`. None of the three is registered by Wave 9 lane Q's
// Array<T>::~Array port (which covers 0x180004890, 0x180009460 and seventeen
// others but not these three), so calling them would open three NEW address
// doors. They are INLINED below instead — the same decision, and the same
// argument, that runtime/src/collection/array.cpp's own note makes when it says
// "one C++ function serves all eleven": freeing a buffer does not need to know
// what is in it. THIS DOES NOT PORT THOSE THREE ADDRESSES and this file does
// not claim it does; it declines to open a door, which is the opposite.
//
// ----------------------------------------------------------------------------
// NO ADDED NULL GUARDS, NO ADDED BOUNDS CHECKS
// ----------------------------------------------------------------------------
// `SetAchievementMet` writes `_completed._data[index]` with NO bounds check and
// NO null check on `_data` — `MOVSXD RBX,EDX / MOV RAX,[RCX+8] / MOV byte ptr
// [RBX+RAX],1`, three instructions, no test. `ReadAchievements` derefs the same
// buffer. Both faults are the load-bearing kind achievement_manager.cpp's own
// banner describes: they are how a manager that never got `SetAchievements`
// announces itself. The ports below fault in exactly the same places.
//
// ----------------------------------------------------------------------------
// WHAT THESE TWO TOUCH THAT NOTHING ELSE IN THE NAMESPACE DOES
// ----------------------------------------------------------------------------
// `SetAchievementMet` calls `IO::Persistence::Set` + `WriteSettings`, which
// writes the player's REAL HKCU registry. `ReadAchievements` reads it back.
// achievement_manager.cpp's banner cites that as the reason Wave 5 lane B left
// them alone. It is a reason to be careful with a probe, not a reason not to
// port: the write is retail's, byte for byte, and `tools/ach_reset.sh` already
// exists to undo it. NOTHING in this file runs a differential oracle against
// the DLL for these two, precisely because the DLL's copy would perform the
// registry write a SECOND time (the diff-oracle header's third bullet).
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"
#include "../../types/system/String.h"

using RtAchievement::Achievement;
using RtAchievement::AchievementInstance;
using RtAchievement::AchievementManager;
// ENDGAME B.3.1 step 2: the real class, not the POD.
using System::String;

// Forward-declared rather than pulled in from standalone/resolve.h: that header
// drags in <windows.h> and PLAN.md §4.1 requires runtime/src/ to stay
// platform-neutral. Byte-identical to resolve.h's declarations.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

namespace {

// ---------------------------------------------------------------------------
// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z — retail 0x1800c7ad0
// ---------------------------------------------------------------------------
// The `if (p)` test lives INSIDE this body (`TEST RCX,RCX / JZ ret` is its
// first instruction), and so does the EnterCriticalSection/Free/Leave triple
// the three inlined array destructors spell out. Handing it a possibly-null
// pointer is therefore exactly what retail's leaves do.
//
// Resolved by ADDRESS with a retry-on-miss cache, copied deliberately from
// runtime/src/collection/array.cpp:163 — a `static` initialised from a call
// that returns null once would otherwise freeze that null for the life of the
// process and take the whole heap down with it.
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    fn(p);
}

// The shared body of 0x180009700 / 0x180008e80 / 0x180009560, inlined once.
// `_size` and `_capacity` are cleared together as one qword in retail; both
// really are reset here, unlike SetSize which never shrinks capacity.
template <typename T>
void FreeArrayBuffer(Collection::Array<T>* a) {
    MemFree(a->_data);
    a->_data     = nullptr;
    a->_size     = 0;
    a->_capacity = 0;
}

// ---------------------------------------------------------------------------
// ?_singleton@AchievementManager@Achievement@@0PEAV12@EA — retail 0x1801fc670
// ---------------------------------------------------------------------------
// The destructor's very first store. Read through RT_DATA_SLOT so that while
// the row is UNPROMOTED this resolves to the DLL's own cell — which is the
// cell every unported DLL reader has baked into its .text as a hard-coded RVA.
// See the DATA section of docs/runtime_port/WAVE23_LANE_ACHIEVE.md.
AchievementManager** ManagerSingletonCell() {
    return RT_DATA_SLOT(AchievementManager**,
        "?_singleton@AchievementManager@Achievement@@0PEAV12@EA");
}

// ---------------------------------------------------------------------------
// IO::Persistence — the three members these two bodies drive
// ---------------------------------------------------------------------------
// Resolved BY NAME, not by address, and that is the "whoever owns it now" rule
// achievement_manager.cpp:80 states: these touch state another lane owns
// (Wave 4 lane C ported `Get(bool&)` with its own RT_DIFF instrumentation in
// runtime/src/io/persistence.cpp). Going around the registry would make that
// lane's oracle miss every call this file makes.
//
// `?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z`  retail 0x1800b8290  bool overload
// `?Set@Persistence@IO@@QEAAXPEBD0_N@Z`      retail 0x1800b8380  bool overload
// `?WriteSettings@Persistence@IO@@QEBAXXZ`   retail 0x1800b88e0
//
// Getting the OVERLOAD wrong here is a silent-wrong-answer defect, not a link
// error: the int overload (0x1800b81e0) and the String overload (0x1800b7d30)
// both exist under nearly identical names and would parse "true" as 0.
using PersistGetBoolFn = bool (*)(const void*, const char*, const char*, bool*);
using PersistSetBoolFn = void (*)(void*, const char*, const char*, bool);
using PersistWriteFn   = void (*)(const void*);

void** PersistenceSingletonCell() {
    return RT_DATA_SLOT(void**, "?_singleton@Persistence@IO@@0PEAV12@EA");
}

// ENDGAME B.3.1 step 2: `?Format@String@System@@SA?AV12@PEBDZZ` (0x1800c9730)
// is a static member of `System::String` now, so the by-address door that used
// to stand here is gone and the two call sites below call it directly. Same
// body either way — the registry row at that address registers the shim, and
// the shim is one line over this member — and a direct call is what keeps the
// body off the DLL at cutover.

// The two string literals, read out of the DLL at 0x180154848 and 0x18015484c
// rather than guessed: bytes `25 69 00 00 41 63 68 00` = "%i\0\0Ach\0".
// `SetAchievementMet` LEAs both; `ReadAchievements` LEAs the same pair.
constexpr const char kIndexFormat[] = "%i";
constexpr const char kSection[]     = "Ach";

}  // namespace

// ============================================================================
// void Achievement::AchievementManager::~AchievementManager()
// ??1AchievementManager@Achievement@@QEAA@XZ            retail 0x18009eab0
//
// QEAA — the NON-VIRTUAL spelling, and that is not a detail. AchievementManager
// has no vtable at all (its ctor at 0x18009eb00 zeroes nine slots and stamps
// nothing), so the `??1…@UEAA@XZ` spelling does not exist in this DLL and would
// resolve NULL forever (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
//
// See the banner for why there is no element loop here.
// ============================================================================
void RtAch_AchievementManager_Dtor(AchievementManager* self) {
    *ManagerSingletonCell() = nullptr;          // FIRST, before any free
    FreeArrayBuffer(&self->updateTimes);        // +0x30
    FreeArrayBuffer(&self->instances);          // +0x18
    FreeArrayBuffer(&self->completed);          // +0x08
}

// ============================================================================
// void Achievement::AchievementManager::RemoveAll()
// ?RemoveAll@AchievementManager@Achievement@@QEAAXXZ    retail 0x18009e9d0
//
// THIS is where the instances are destroyed — through slot 0 of each one's own
// vtable with flags = 1, the SCALAR DELETING destructor, which both runs
// ~AchievementInstance and frees the 0x28 bytes `Achievement::CreateInstance`
// allocated.
//
// The slot is emptied as it goes (`MOV [R14+RSI],RBP` at 0x18009ea17, inside
// the null test), so a re-entrant call cannot double-delete. Then the buffer
// itself is freed and BOTH _size and _capacity are zeroed — retail does that as
// one qword store at 0x18009ea69.
//
// The loop bound `_instances._size` is re-read from memory on every iteration
// (`CMP EBX,dword ptr [RDI+0x20]` at 0x18009ea21, not a cached register), so a
// destructor that shrinks the array is seen. Reproduced below rather than
// hoisted, because hoisting it would be a real behavioural difference on the
// one path where it matters.
// ============================================================================
void RtAch_AchievementManager_RemoveAll(AchievementManager* self) {
    for (int32_t i = 0; i < self->instances._size; ++i) {
        AchievementInstance** slot = self->instances._data + i;
        AchievementInstance*  inst = *slot;
        if (inst != nullptr) {
            using DeletingDtorFn = void (*)(void*, int);
            void** vt = *reinterpret_cast<void***>(inst);
            reinterpret_cast<DeletingDtorFn>(vt[0])(inst, 1);
            *slot = nullptr;
        }
    }
    FreeArrayBuffer(&self->instances);
}

// ============================================================================
// void Achievement::AchievementManager::ReadAchievements()
// ?ReadAchievements@AchievementManager@Achievement@@QEAAXXZ  retail 0x18009eb90
//
// Re-reads every "Ach<i>" key out of IO::Persistence into `_completed`.
//
// THE LOOP BOUND IS `_completed._size` (+0x10), NOT the achievement count and
// NOT `_instances._size`. `_completed` is sized by `SetAchievements`; reading a
// manager that never got one derefs a null `_data` on the very first iteration,
// which is the announced fault described in the banner.
//
// Each iteration:
//   1. ZERO the slot first. Retail stores 0 into `_completed._data[i]` BEFORE
//      calling Get (0x18009ebc4), so a key that is absent leaves the slot
//      FALSE rather than leaving whatever was there. `Get` only writes its out
//      parameter when it finds the key, so dropping this store would silently
//      preserve stale awards across a reload.
//   2. Format the index into a String.
//   3. Get(_singleton, "Ach", key.data, &_completed._data[i]).
//   4. Release the String, with retail's asymmetric inline release.
//
// The `_singleton` is read ONCE, before the loop (0x18009eb9a), not per
// iteration.
// ============================================================================
void RtAch_AchievementManager_ReadAchievements(AchievementManager* self) {
    void* persistence = *PersistenceSingletonCell();

    auto getFn = RT_EXPORT_ONCE(PersistGetBoolFn,
                                "?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z");

    for (int32_t i = 0; i < self->completed._size; ++i) {
        self->completed._data[i] = 0;

        String key = String::Format(kIndexFormat, i);

        getFn(persistence, kSection, key._data,
              reinterpret_cast<bool*>(self->completed._data + i));
        // `key` releases at the bottom of the iteration — retail's 0x18009ec1a,
        // now ~String.
    }
}

// ============================================================================
// bool Achievement::AchievementManager::SetAchievementMet(int index)
// ?SetAchievementMet@AchievementManager@Achievement@@QEAA_NH@Z  retail 0x18009eca0
//
// Sets one flag, writes it back through Persistence and flushes the settings
// to the registry. ALWAYS returns true — `MOV AL,0x1` at 0x18009ed3a, no
// failure path, no early out, not even for a negative index.
//
// FIVE things the bytes say that a plausible reimplementation would get wrong:
//
//  1. No bounds check and no null check (see the banner).
//  2. The index is SIGN-extended (`MOVSXD RBX,EDX`), so a negative index writes
//      BEFORE the buffer rather than at a huge positive offset.
//  3. The value handed to `Set` is READ BACK OUT of the array
//      (`MOVZX R9D,byte ptr [RBX+RAX]` at 0x18009ece7, after re-loading
//      `[RDI+8]`), not the literal 1 that was just stored. Identical in value
//      today; it is transcribed as a read because that is what the bytes do and
//      because it is what makes the store at step 1 observable.
//  4. `_singleton@Persistence` is captured BEFORE `String::Format` runs
//      (0x18009ecc7), and both `Set` and `WriteSettings` use that captured
//      value.
//  5. `WriteSettings` is called on EVERY set, not batched.
// ============================================================================
bool RtAch_AchievementManager_SetAchievementMet(AchievementManager* self, int index) {
    self->completed._data[index] = 1;

    void* persistence = *PersistenceSingletonCell();

    String key = String::Format(kIndexFormat, index);

    auto setFn = RT_EXPORT_ONCE(PersistSetBoolFn, "?Set@Persistence@IO@@QEAAXPEBD0_N@Z");
    setFn(persistence, kSection, key._data, self->completed._data[index] != 0);

    auto writeFn = RT_EXPORT_ONCE(PersistWriteFn, "?WriteSettings@Persistence@IO@@QEBAXXZ");
    writeFn(persistence);

    // `key` releases here — retail's 0x18009ed30, now ~String.
    return true;
}

// ============================================================================
// Object::InternalPtr<AchievementInstance> Achievement::Achievement::CreateInstance() const
// ?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@
//     VAchievementInstance@Achievement@@@Object@@XZ     retail 0x18009df80
//
// RETURNS BY VALUE, so the hidden return slot IS A REAL ABI PARAMETER: RCX =
// this, RDX = &retval. The Wave 23 brief flagged this as the shape where an
// fn_* typedef that OMITS the slot is a wild write that still returns the right
// answer (reference_hidden_retslot_wild_write.md). The bytes settle it twice
// over: `MOV RSI,RDX` is the second instruction, `MOV [RSI],RDI` at 0x18009e013
// is the only store to it, and `MOV RAX,RSI` at 0x18009e04f means the function
// ALSO returns the slot pointer in RAX. Both are reproduced; the RAX half costs
// nothing and a caller that reads it cannot then diverge.
//
// The InternalPtr is ONE pointer — the store at 0x18009e013 is a single qword
// and nothing else is written through RSI.
//
// It does NOT call the exported ctor `??0AchievementInstance@Achievement@@QEAA@XZ`
// (0x18009e3b0). It inlines it: allocate 0x28 / align 8, run Object::Internal's
// ctor (which enters the object into the handle table), stamp the vtable, zero
// +0x10/+0x18/+0x20. `achievement_manager.cpp` already ports that exported ctor;
// the two must stay in step but they are genuinely separate bodies in the DLL.
//
// THREE FAITHFUL HAZARDS, kept:
//
//  1. `*(inst + 0x20) = this` at 0x18009e016 is OUTSIDE the null branch. On an
//     allocation failure RDI is 0 and retail writes through null. That is the
//     "don't fix a faithful crash" rule squarely: retail relies on the
//     allocation succeeding, and a guard here would hand the caller a live
//     InternalPtr to an instance with no Achievement instead of failing.
//  2. +0x20 is written TWICE — zeroed at 0x18009e009, then set to `this` at
//     0x18009e016. Collapsing the pair changes the bytes an allocator or
//     watchpoint trace sees.
//  3. The temporary `Array<ExternalPtr<Goal>>` is passed BY VALUE and
//     `GoalList::Initialize` DESTROYS IT (its tail frees the buffer and zeroes
//     the header — read at 0x18009f23c). That is why nothing here frees it.
//     Freeing it as well would be a double free on every instance created.
// ============================================================================
void* RtAch_Achievement_CreateInstance(const Achievement* self, void** retval) {
    using AllocFn = void* (*)(uint64_t, int);
    static AllocFn allocFn = nullptr;
    if (!allocFn) allocFn = reinterpret_cast<AllocFn>(ResolveAddr(Module::Runtime, 0x1800c7a50));

    // MemAlloc(0x28, 8) IS retail's `Enter; Allocate(_singleton, 0x28, 8); Leave`
    // triple, instruction for instruction (0x1800c7a50 decompiles to exactly
    // that). Calling it opens no door retail's own sequence does not.
    auto* inst = static_cast<AchievementInstance*>(allocFn(0x28, 8));

    if (inst != nullptr) {
        using InternalCtorFn = void* (*)(void*);
        auto internalCtor = RT_EXPORT_ONCE(InternalCtorFn, "??0Internal@Object@@QEAA@XZ");
        internalCtor(inst);

        // ??_7AchievementInstance@Achievement@@6B@, ord 1383, DLL 0x180154700 —
        // an EXPORTED data symbol, so the port stamps the DLL's own table
        // rather than cloning slots out of a live object (PLAN.md §2.2 D).
        static void* vtable = nullptr;
        if (!vtable) vtable = ResolveExport("??_7AchievementInstance@Achievement@@6B@");
        *reinterpret_cast<void**>(inst) = vtable;

        inst->goalList.goals._data     = nullptr;   // +0x10
        inst->goalList.goals._size     = 0;         // +0x18 (as one qword in retail)
        inst->goalList.goals._capacity = 0;
        inst->achievement              = nullptr;   // +0x20
    }

    *retval = inst;
    inst->achievement = const_cast<Achievement*>(self);   // faithful: not guarded

    // ENDGAME B.3.2: Collection::Array is a real class now, so this is its
    // default constructor — retail's same three zero stores. Its destructor
    // runs at scope exit, which is what retail does too: the callee below takes
    // the Array BY VALUE and its inlined by-value-parameter teardown
    // (goal_list.cpp) already freed the buffer and zeroed the header, so the
    // scope-exit ~Array is MemFree(nullptr), a no-op.
    Collection::Array<void*> temp;

    using ArrayAssignFn = void* (*)(void*, const void*);
    static ArrayAssignFn arrayAssign = nullptr;
    if (!arrayAssign)
        arrayAssign = reinterpret_cast<ArrayAssignFn>(ResolveAddr(Module::Runtime, 0x180003970));
    arrayAssign(&temp, &self->goals);   // Achievement+0x48

    using GoalListInitFn = void (*)(void*, void*);
    auto goalListInit = RT_EXPORT_ONCE(GoalListInitFn,
        "?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@"
        "VGoal@Achievement@@@Object@@@Collection@@@Z");
    goalListInit(&inst->goalList, &temp);   // consumes AND destroys `temp`

    return retval;   // retail's `MOV RAX,RSI`
}

// ---- Registration ----------------------------------------------------------
// Gated by RT_ACHMGR with achievement_manager.cpp, per the
// class-atomicity rule (PLAN §4.1 decision 5): these five members and the
// eighteen already in that file are the same three classes.
//
// Registered by NAME **and** by ADDRESS. The names are what
// achievement_manager.cpp:174/176, code/Game/Game/game_main.cpp:258 and
// runtime/src/engine/engine_lifecycle.cpp:843/938 already ask for through
// ResolveExport — i.e. these doors are LIVE today and will start landing here.
// The addresses are registered so a call reaching one of these by address (the
// dtor, notably, is reached that way from engine_lifecycle.cpp) also lands on
// us. Every address is spelled as a LITERAL at its call site because
// tools/runtime_census.py counts the literal
// (reference_census_cannot_see_nonliteral_registrations.md).
int RtAchStateRegister() {
    int n = 0;

    RegisterRuntimeExport("??1AchievementManager@Achievement@@QEAA@XZ",
                          (void*)&RtAch_AchievementManager_Dtor); ++n;
    RegisterRuntimeAddr(0x18009eab0, (void*)&RtAch_AchievementManager_Dtor);

    RegisterRuntimeExport("?RemoveAll@AchievementManager@Achievement@@QEAAXXZ",
                          (void*)&RtAch_AchievementManager_RemoveAll); ++n;
    RegisterRuntimeAddr(0x18009e9d0, (void*)&RtAch_AchievementManager_RemoveAll);

    RegisterRuntimeExport("?ReadAchievements@AchievementManager@Achievement@@QEAAXXZ",
                          (void*)&RtAch_AchievementManager_ReadAchievements); ++n;
    RegisterRuntimeAddr(0x18009eb90, (void*)&RtAch_AchievementManager_ReadAchievements);

    RegisterRuntimeExport("?SetAchievementMet@AchievementManager@Achievement@@QEAA_NH@Z",
                          (void*)&RtAch_AchievementManager_SetAchievementMet); ++n;
    RegisterRuntimeAddr(0x18009eca0, (void*)&RtAch_AchievementManager_SetAchievementMet);

    RegisterRuntimeExport("?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@"
                          "VAchievementInstance@Achievement@@@Object@@XZ",
                          (void*)&RtAch_Achievement_CreateInstance); ++n;
    RegisterRuntimeAddr(0x18009df80, (void*)&RtAch_Achievement_CreateInstance);

    return n;
}
