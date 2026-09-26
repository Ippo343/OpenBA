#pragma once
// ============================================================================
// runtime/types/achievement/Goal.h — Achievement::Goal / GoalInstance / GoalList
// ============================================================================
// Wave 5, lane A of the Runtime-x64.dll port pinned these layouts
// (docs/runtime_port/WAVE5_LANE_A.md); ENDGAME B.3.7 block B3 turned them into
// real C++ classes under retail's names (B37_RULES.md §1, §2, rules B7-1,
// B7-2, B7-4, B7-5, B7-6, B7-8, B7-13).
//
// EVIDENCE RULE (PLAN §6): every offset below is read out of a disassembled
// RETAIL Runtime-x64.dll body, and the address of that body is named next to
// it. The profile build's NAMES win, its OFFSETS must not
// (reference_encoding_vtable_offsets_differ_between_builds.md).
//
// THE THREE CLASSES, AND HOW THEY RELATE
// --------------------------------------
//   Achievement::Goal          the resource: a rule loaded off disk. Derives
//                              Object::External (literal "Object::External",
//                              ?_ParentTypeName@Goal@Achievement@@ 0x180001b30).
//   Achievement::GoalInstance  the live tracker for one Goal in one game.
//                              Derives Object::Internal (literal
//                              "Object::Internal", 0x180001e30).
//   Achievement::GoalList      a bare Collection::Array of GoalInstance
//                              handles. NOT an Object — no vtable. A VALUE TYPE
//                              held by AchievementInstance at +0x10 (B7-8).
//
// THE EXE SUBCLASSES ALL OF Goal/GoalInstance (BuildingGoal, PopulationGoal,
// StatGoal, TimeGoal, FinishTutorialGoal; TimeGoalInstance,
// FinishTutorialInstance — engine/types/AchievementGoals.h, code/Game/Goal/,
// code/Game/Tutorial/), with HAND-BUILT tables. So both classes are
// ABSTRACT-BY-POLICY (RULE B7-13): slot 0 `DeletingDtor(unsigned) = 0` with
// the `deleting_dtor.cpp` row untouched, NO real constructor (the ctor doors in
// runtime/src/achievement/goal.cpp stamp the MINTED tables), and every call
// from an exe slot body to a VIRTUAL of these bases is QUALIFIED
// (`->Achievement::Goal::Serialize(stream)`), because an unqualified call
// dispatches through the exe table back into the calling slot.
//
// NO VIRTUAL DESTRUCTOR (ENDGAME B.1 decision 2). The destructors are
// NON-virtual and PUBLIC: public because the exe subclasses' slot-0 bodies —
// which are free functions over hand-built tables, not C++ subclasses yet
// (step 8) — run them explicitly as QUALIFIED member calls
// (`->Achievement::Goal::~Goal()`), which is retail's own shape (each exe
// slot 0 CALLs `??1Goal@Achievement@@UEAA@XZ`). Deletion of a whole object is
// still `Object::Destroy(p)` (slot 0), never `delete p`.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "../collection/Collection.h"
#include "../object/External.h"
#include "../object/Internal.h"
#include "../object/Type.h"
#include "../system/String.h"

namespace UI { class StringTable; }
namespace System { class Time; }
namespace IO { class Serializer; }

// The retail Collection rows these classes' arrays ARE (defined in
// runtime/src/collection/array.cpp, also declared in array_internal.h — a
// repeated `extern` declaration is legal). Naming the row in the member's
// TYPE is what retires the `rtcoll::AsArray<&kSpec_…>` bridge casts the old
// bodies needed (the goal_exe_helpers.cpp `ExplicitSpec` precedent).
namespace rtcoll {
extern const ArraySpec kSpec_PtrZero;   // 0x180009d70 / 0x180008d60  Array<InternalPtr<T>>
extern const ArraySpec kSpec_U64Zero;   // 0x180005110                Array<ExternalPtr<Goal>>
extern const ArraySpec kSpec_Bool;      // 0x180008b90                Array<bool>
}

namespace Achievement {

class Goal;
class GoalInstance;
class GoalList;

// Collection::Array<Object::ExternalPtr<Achievement::Goal>> — the element type
// named by `?GetGoalList@Achievement@1@QEBAAEBV?$Array@V?$ExternalPtr@VGoal@…`
// and by GoalList::Initialize's by-value parameter. Its SetSize is
// 0x180005110 (`kSpec_U64Zero`: 8-byte assign, zero-filled tail) and its
// destructor 0x180004890 (J1: free the buffer, zero the header — the spec has
// no element destroy, which is right: an ExternalPtr owns nothing).
using GoalArray = Collection::Array<Object::ExternalPtr<Goal>,
                                    Collection::DefaultAllocator,
                                    Collection::ExplicitSpec<&rtcoll::kSpec_U64Zero>>;

// Collection::Array<Object::InternalPtr<Achievement::GoalInstance>> — SetSize
// 0x180009d70 (`kSpec_PtrZero`: `Assign8Z`'s zero-then-copy double store,
// zeroed tail), operator= 0x180003a30.
using GoalInstanceArray = Collection::Array<Object::InternalPtr<GoalInstance>,
                                            Collection::DefaultAllocator,
                                            Collection::ExplicitSpec<&rtcoll::kSpec_PtrZero>>;

// ============================================================================
// Achievement::Goal — 0x30 bytes = Object::External (0x20) + 0x10. TEN slots.
//
// `??_7Goal@Achievement@@6B@` @ 0x1801546b0 (B37_SLOTS.md §1):
//   [0] +0x00 0x180001d70  vector deleting dtor (MOV EDX,0x30)
//   [1] +0x08 0x180001b60  ?TypeName@Goal@Achievement@@UEBAPEBDXZ
//   [2] +0x10 0x180001b40  ?Type@Goal@Achievement@@UEBAIXZ
//   [3] +0x18 0x18009ee10  ?GetFileVersion@Goal@Achievement@@UEBAHXZ
//   [4] +0x20 0x1800066e0  ?Compile@External@Object@@…  INHERITED (B7-1.7)
//   [5] +0x28 0x18009ee20  ?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z
//   [6] +0x30 _purecall    Reconstruct — inherited pure from External
//   [7] +0x38 _purecall    Destruct    — inherited pure from External
//   [8] +0x40 _purecall    NEW here, pure — see the note on slot 8 below
//   [9] +0x48 0x18009eeb0  ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ
//
// *** SLOT 8 IS NOT `Construct`. *** B37_RULES B7-1.6 names it `Construct` by
// analogy with the External leaves. The bytes say otherwise:
// `?CheckGoalState@GoalInstance@Achievement@@` (0x18009f060) is
//     MOV RCX,[RCX+0x18]   ; this->_goal (the Goal)
//     MOV RAX,[RCX]
//     JMP [RAX+0x40]       ; Goal slot 8, TAIL CALL
// so Goal slot 8 takes only `this` and RETURNS the `Goal::GoalState` enum that
// GoalInstance::CheckGoalState returns — and every exe override of slot 8
// (BuildingGoal_Slot8, TimeGoal_Slot8, StatGoal's check at 0x1400da720, …)
// returns 0/1/2. A `Construct` (External* return, null-test-then-build) cannot
// be tail-called for an enum. The retail NAME of the slot is not attested (the
// slot is `_purecall` in the DLL and the exe has no symbols), so it is
// declared under the descriptive name `CheckGoalState` — the member it is the
// body of — and this note is the record of that choice.
//
// Layout PINS:
//   ??0Goal@Achievement@@QEAA@XZ (0x18009ed50): Base, External, then Goal's
//     vptr; six qword stores covering 0x00..0x2f exactly — sizeof 0x30, and
//     +0x20/+0x28 are Goal's.
//   ?GetStringTable@Goal@Achievement@@ (0x180001b90): `*retval = [this+0x20]`,
//     and its mangled return names +0x20 `ExternalPtr<UI::StringTable>`.
//   ?GetText@Goal@Achievement@@ (0x180001ba0) copies [this+0x28] with an addref:
//     a System::String. ??1Goal (0x18009edb0) releases it, then tail-jumps
//     ~External.
// ============================================================================
class Goal : public Object::External {
public:
    // `?AW4GoalState@Goal@2@` — the enum GoalInstance::CheckGoalState returns
    // in EAX (a W4 enum: NO hidden return slot). The three values are the three
    // EAX comparisons in GoalInstance::Update (0x18009f010); any other value
    // leaves the flags untouched. The enumerator NAMES are ours.
    enum GoalState : int32_t {
        kNotMet = 0,
        kMet    = 1,
        kFailed = 2,
    };

    // ---- the ten slots, in retail order ------------------------------------
    // [0] 0x180001d70 — pure on purpose (B7-13); the RT_DELDTOR row fills the
    //     minted table's slot 0 and is not retired.
    Object::Base* DeletingDtor(unsigned flags) override = 0;
    const char* TypeName() const override;                        // [1] 0x180001b60
    uint32_t    Type() const override;                            // [2] 0x180001b40
    int32_t     GetFileVersion() const override;                  // [3] 0x18009ee10 — 0x10000
    //          Compile                                           // [4] External's, inherited
    bool        Serialize(void* stream) override;                 // [5] 0x18009ee20 (IO::Stream&)
    //          Reconstruct / Destruct                            // [6] [7] pure, inherited
    virtual GoalState CheckGoalState() const = 0;                 // [8] pure — name NOT attested
    // [9] 0x18009eeb0 — RULE B7-4, the one retslot VIRTUAL of the batch.
    //     `Object::Type` is a class returned BY VALUE, so MSVC passes a hidden
    //     return slot in RDX after `this` in RCX and hands it back in RAX. GCC's
    //     ms_abi puts the hidden slot BEFORE `this` (B6-2), so the slot is
    //     spelled here as an explicit out-parameter IN RETAIL ORDER. Every exe
    //     override (TimeGoal_Slot9, FinishTutorialGoal_Slot9) takes the same
    //     `(this, ret)` shape. tests/unit/achievement_goal_abi_test.cpp pins it.
    virtual Object::Type* GetInstanceType(Object::Type* ret) const;

    // ---- non-slot exported members -----------------------------------------
    // ?GetStringTable@Goal@Achievement@@QEBA?AV?$ExternalPtr@VStringTable@UI@@@Object@@XZ
    //   0x180001b90 — one store. Non-virtual retslot: the member returns the
    //   prvalue; a door (none is registered today) would placement-new it.
    Object::ExternalPtr<UI::StringTable> GetStringTable() const { return _stringTable; }
    // ?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ  0x180001ba0
    System::String GetText() const;

    static uint32_t TypeId();                                     // 0x180001b50 — not ported
    static Object::Base* CreateNewObject();                       // 0x180001b70 — not ported
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180001b80 — not ported
    Goal(const Goal& other);                                      // 0x180001be0 — not ported
    Goal& operator=(const Goal& other);                           // 0x180001c80 — not ported

    // ??1Goal@Achievement@@UEAA@XZ  0x18009edb0. NON-virtual, PUBLIC (banner).
    // Retail: re-stamp Goal's vptr, release `_text`, tail-jump ~External. The
    // body is EMPTY because the compiler emits all three: the vptr store on
    // entry, `~String` on `_text` (a class member), then `~External()`.
    ~Goal();

    // The door's way onto the shared body (ENDGAME B.3.8 block RB, B38_RULES
    // D1): `RtAch_Goal_Ctor` stamps the MINTED tables, then runs this.
    struct DoorBody { static void Init(Goal* g) { g->InitFields(); } };

protected:
    // ??0Goal@Achievement@@QEAA@XZ 0x18009ed50 — the REAL constructor (ENDGAME
    // B.3.8 block RB, RULE B8-1): External() and the stamps are the
    // compiler's; the two zero stores are ONE body with the door.
    Goal() { InitFields(); }

private:
    // Raw member writes: the door runs this over raw storage.
    void InitFields() {
        _stringTable._ptr = nullptr;                    // +0x20
        *reinterpret_cast<void**>(&_text) = nullptr;    // +0x28
    }

private:
    // RULE B5-15 / B7-2, COPIED from B37_SLOTS.md §3 — recorded, not DEFINE_TYPE'd:
    //   0x180001b20  ?_TypeName@Goal@Achievement@@       -> "Goal"
    //   0x180001b30  ?_ParentTypeName@Goal@Achievement@@ -> "Object::External"
    static const char* _TypeName();
    static const char* _ParentTypeName();

public:
    // ---- the 0x10 bytes Goal adds ------------------------------------------
    // PUBLIC for the reason Object::External's are: unconverted exe bodies
    // (engine/types/AchievementGoals.h's `_goalBase[0x28]`) sit on these bytes.
    Object::ExternalPtr<UI::StringTable> _stringTable;   // +0x20
    System::String                       _text;          // +0x28  the goal's display text
};
static_assert(sizeof(Goal) == 0x30, "Achievement::Goal is 0x30 bytes (slot 0 MOV EDX,0x30; ctor 0x18009ed50)");
static_assert(alignof(Goal) == 8, "Achievement::Goal aligns to 8");
static_assert(offsetof(Goal, _stringTable) == 0x20, "Goal::_stringTable @ +0x20 (GetStringTable 0x180001b90)");
static_assert(offsetof(Goal, _text)        == 0x28, "Goal::_text @ +0x28 (GetText 0x180001ba0, ~Goal 0x18009edb0)");
static_assert(sizeof(Object::ExternalPtr<UI::StringTable>) == 8, "one pointer");
static_assert(sizeof(System::String) == 8, "one pointer");

// ============================================================================
// Achievement::GoalInstance — 0x20 bytes = Object::Internal (0x10) + 0x10.
// SEVEN slots.
//
// `??_7GoalInstance@Achievement@@6B@` @ 0x180154678 (B37_SLOTS.md §1):
//   [0] +0x00 0x1800020c0  vector deleting dtor (MOV EDX,0x20)
//   [1] +0x08 0x180001e60  ?TypeName@GoalInstance@Achievement@@UEBAPEBDXZ
//   [2] +0x10 0x180001e40  ?Type@GoalInstance@Achievement@@UEBAIXZ
//   [3] +0x18 0x18009ef70  ?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
//   [4] +0x20 0x18009f000  ?Startup@GoalInstance@Achievement@@UEAAXXZ        (one RET)
//   [5] +0x28 0x18009f010  ?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z
//   [6] +0x30 0x18009f060  ?CheckGoalState@GoalInstance@Achievement@@UEBA?AW4GoalState@Goal@2@XZ
// (B7-1.8 agrees.)
//
// Layout PINS:
//   ??0GoalInstance (0x18009ef00): CALL ??0Internal, stamp, `MOV dword
//     [RBX+0x10],EAX` (a DWORD: +0x14 is never written), `[RBX+0x18] = 0`.
//   IsMet (0x18009eed0) / HasFailed (0x18009eec0) / Update read +0x10 as bits.
//   ?GetGoal@GoalInstance… (0x180001fe0) `*retval = [this+0x18]` names +0x18
//     `ExternalPtr<Achievement::Goal>`; SetGoal (0x18009eee0) `MOV
//     [RCX+0x18],RDX` — the ExternalPtr argument arrives BARE in RDX (B7-5).
// +0x14 IS REAL PADDING: no body reads or writes it.
// ============================================================================
class GoalInstance : public Object::Internal {
public:
    // The +0x10 flag bits (three independent readers: IsMet, HasFailed, Update).
    static constexpr uint32_t kFlagMet    = 0x1;   // set when CheckGoalState returns kMet
    static constexpr uint32_t kFlagFailed = 0x2;   // set on kFailed; LATCHES

    // ---- the seven slots, in retail order ----------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x1800020c0 — pure (B7-13)
    const char* TypeName() const override;                        // [1] 0x180001e60
    uint32_t    Type() const override;                            // [2] 0x180001e40
    void        Persist(void* stream) override;                   // [3] 0x18009ef70 (IO::Stream&)
    virtual void Startup();                                       // [4] 0x18009f000
    virtual void Update(const System::Time& time, const GoalList& list);  // [5] 0x18009f010
    virtual Goal::GoalState CheckGoalState() const;               // [6] 0x18009f060

    // ---- non-slot exported members -----------------------------------------
    bool HasFailed() const;                                       // 0x18009eec0
    bool IsMet() const;                                           // 0x18009eed0
    // ?GetGoal@GoalInstance…QEBA?AV?$ExternalPtr@VGoal@… 0x180001fe0 — retslot door
    Object::ExternalPtr<Goal> GetGoal() const { return _goal; }
    // ?SetGoal@GoalInstance…QEAAXV?$ExternalPtr@VGoal@… 0x18009eee0 — the
    // ExternalPtr arrives BARE in RDX (B7-5); not ported (no caller).
    void SetGoal(Object::ExternalPtr<Goal> goal);

    static uint32_t TypeId();                                     // 0x180001e50 — not ported
    static void* operator new(std::size_t bytes);                 // 0x180001e70 — not ported
    static void* operator new(std::size_t bytes, void* place);    // 0x180001ee0 — not ported
    static void  operator delete(void* p);                        // 0x180001ef0 — not ported
    static Object::Base* CreateNewObject();                       // 0x180001f50 — not ported
    GoalInstance(const GoalInstance& other);                      // 0x180001ff0 — not ported
    GoalInstance& operator=(const GoalInstance& other);           // 0x180002050 — not ported

    // ??1GoalInstance@Achievement@@UEAA@XZ  0x18009ef40. NON-virtual, PUBLIC.
    // Retail: re-stamp, tail-jump ~Internal. EMPTY body — the compiler emits
    // the stamp and the `~Internal()` (InternalFactory swap-remove) itself.
    ~GoalInstance();

    // The door's way onto the shared body (ENDGAME B.3.8 block RB).
    struct DoorBody { static void Init(GoalInstance* g) { g->InitFields(); } };

protected:
    // ??0GoalInstance@Achievement@@QEAA@XZ 0x18009ef00 — the REAL constructor
    // (ENDGAME B.3.8 block RB, RULE B8-1); ONE body with `RtAch_GoalInstance_Ctor`.
    GoalInstance() { InitFields(); }

private:
    void InitFields() {
        _flags     = 0;         // +0x10 a DWORD — +0x14 is never written
        _goal._ptr = nullptr;   // +0x18
    }

private:
    //   0x180001e20  ?_TypeName@GoalInstance@Achievement@@       -> "GoalInstance"
    //   0x180001e30  ?_ParentTypeName@GoalInstance@Achievement@@ -> "Object::Internal"
    static const char* _TypeName();
    static const char* _ParentTypeName();

public:
    uint32_t                  _flags;   // +0x10  kFlagMet | kFlagFailed
    uint32_t                  _pad14;   // +0x14  NEVER written — real padding
    Object::ExternalPtr<Goal> _goal;    // +0x18
};
static_assert(sizeof(GoalInstance) == 0x20, "Achievement::GoalInstance is 0x20 bytes (slot 0 MOV EDX,0x20)");
static_assert(alignof(GoalInstance) == 8, "Achievement::GoalInstance aligns to 8");
static_assert(offsetof(GoalInstance, _flags) == 0x10, "GoalInstance::_flags @ +0x10 (IsMet 0x18009eed0)");
static_assert(offsetof(GoalInstance, _pad14) == 0x14, "GoalInstance::_pad14 @ +0x14");
static_assert(offsetof(GoalInstance, _goal)  == 0x18, "GoalInstance::_goal @ +0x18 (GetGoal 0x180001fe0)");

// ============================================================================
// Achievement::GoalList — 0x10 bytes, EXACTLY one Collection::Array.
// A VALUE TYPE with real constructors (RULE B7-8): held by value at
// AchievementInstance+0x10, deep copy, owning destructor.
//
// PINS
//   ??0GoalList@Achievement@@QEAA@XZ (0x18009f0b0): zero +0x00 and +0x08.
//   ??0GoalList@…@QEAA@AEBV01@@Z (0x180001730): zero +0x00/+0x08, then CALL
//     0x180003a30 = Array<InternalPtr<GoalInstance>>::operator= (RDX, the
//     source, passed straight through). The copy is DEEP IN THE BUFFER and
//     SHALLOW IN THE ELEMENTS: 0x180003a30 copies each 8-byte handle (zero
//     then store — kSpec_PtrZero's Assign8Z) into a fresh buffer, so two lists
//     then OWN the same instances and both destructors would delete them.
//     That is retail's, and nothing in retail or here copies a live list.
//   ??4GoalList (0x180001760): CALL 0x180003a30; return this.
//   ??1GoalList@Achievement@@QEAA@XZ (0x18009f420) — `QEAA`, NON-virtual:
//     for i in [0, _size) (bound and base re-read every pass): if the element
//     is non-null, call ITS slot 0 with flag 1 (the scalar DELETING dtor — the
//     list OWNS its instances) and null it; then free the buffer and zero the
//     header (one qword over _size/_capacity).
//   ?GetGoalCount (0x180001700) `MOV EAX,[RCX+8]`; ?GetGoal (0x180001710)
//     `MOV RAX,[RCX]; MOVSXD RCX,R8D; MOV RCX,[RAX+RCX*8]; MOV RAX,RDX; MOV
//     [RDX],RCX; RET` — index in R8 because the return slot is RDX.
//
// GoalList holds INTERNAL pointers to instances; each instance holds an
// EXTERNAL pointer to its Goal. The asymmetry is real.
// ============================================================================
class GoalList {
public:
    GoalList();                                  // ??0 0x18009f0b0
    GoalList(const GoalList& other);             // ??0(const&) 0x180001730
    GoalList& operator=(const GoalList& other);  // ??4 0x180001760
    ~GoalList();                                 // ??1 0x18009f420 (QEAA)

    // ?GetGoalCount@GoalList@Achievement@@QEBAHXZ  0x180001700
    int GetGoalCount() const { return _goals.GetSize(); }
    // ?GetGoal@GoalList…QEBA?AV?$InternalPtr@VGoalInstance@… 0x180001710 —
    // retslot, non-virtual: the registered door placement-news this prvalue.
    // No bounds check in retail; none here.
    Object::InternalPtr<GoalInstance> GetGoal(int index) const { return _goals._data[index]; }

    // ?AreGoalsMetExcept@GoalList…QEBA_NV?$InternalPtr@VGoalInstance@… 0x18009f070
    // The InternalPtr is BY VALUE and arrives by HIDDEN POINTER (`MOV
    // RDX,[RDX]` at 0x18009f07c — B7-5), so it is spelled as the pointer, in
    // retail order.
    bool AreGoalsMetExcept(const Object::InternalPtr<GoalInstance>* except) const;

    // ?Initialize@GoalList…QEAAXV?$Array@V?$ExternalPtr@VGoal@… 0x18009f0e0
    // The Array is BY VALUE and arrives by HIDDEN POINTER (`MOV EDX,[RDX+0x8]`
    // — B7-5); the CALLEE owns and tears down that temporary.
    void Initialize(GoalArray* goals);

    // Exported, NOT PORTED — nothing in the tree calls them (by name, by
    // address, or by exe import slot). Declared so no caller batch has to edit
    // this header; a link error names the first caller.
    void Update(const System::Time& time);      // ?Update@GoalList…     0x18009f270
    bool AreGoalsMet() const;                   // ?AreGoalsMet@GoalList… 0x18009f2e0
    bool AnyGoalFailed() const;                 // ?AnyGoalFailed@…       0x18009f330
    void Serialize(IO::Serializer& s);          // ?Serialize@GoalList…   0x18009f370

    GoalInstanceArray _goals;   // +0x00 data, +0x08 size, +0x0c capacity
};
static_assert(sizeof(GoalList) == 0x10, "Achievement::GoalList is one Collection::Array (AchievementInstance+0x10..+0x1f)");
static_assert(alignof(GoalList) == 8, "Achievement::GoalList aligns to 8");
static_assert(offsetof(GoalList, _goals) == 0x00, "GoalList::_goals @ +0x00");
static_assert(sizeof(Object::InternalPtr<GoalInstance>) == 8, "GetGoal's 8-byte stride");
static_assert(!std::is_polymorphic<GoalList>::value, "GoalList has no vtable (QEAA dtor)");
static_assert(!std::is_trivially_copyable<GoalList>::value,
              "B7-8: the copy is DEEP (0x180001730 -> 0x180003a30) — never memcpy a GoalList");

}  // namespace Achievement
