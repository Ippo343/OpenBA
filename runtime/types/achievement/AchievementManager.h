#pragma once
// ============================================================================
// runtime/types/achievement/AchievementManager.h
// Achievement::Achievement / AchievementInstance / AchievementManager
// ============================================================================
// Wave 5, lane B pinned these layouts; ENDGAME B.3.7 block B3 turned them into
// real C++ classes under retail's names (B37_RULES.md §1, §2, rules B7-3,
// B7-5, B7-6, B7-13). The old `RtAchievement::` PODs and the lane-local
// `GoalListView` / `GoalInstanceView` are GONE: every user was inside this
// block's file set and converted with it (B7-6: `GoalListView` folds into
// `Achievement::GoalList`, Goal.h).
//
// Every offset below is pinned by a named instruction in RETAIL
// Runtime-x64.dll (image base 0x180000000). Runtime-x64-profile.dll is used for
// NAMES ONLY (reference_encoding_vtable_offsets_differ_between_builds.md).
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Goal.h"
#include "../collection/Collection.h"
#include "../object/External.h"
#include "../object/Internal.h"
#include "../system/String.h"
#include "../system/Value.h"

namespace UI { class StringTable; class SpriteSheet; }
namespace IO { class Stream; }

namespace Achievement {

class AchievementInstance;

// ============================================================================
// Achievement::Achievement — an Object::External subclass, 0x58 bytes. NINE
// slots. RULE B7-3: POLYMORPHIC (the inventory missed it — its vftable is
// mangled with the back-reference `??_7Achievement@0@6B@`, B7-11), and the exe
// builds `Scenario` on it (0x14015f020, code/Game/Game/scenario.cpp), so it is
// ABSTRACT-BY-POLICY: slot 0 `= 0`, ctor door kept, no real constructor.
//
// `??_7Achievement@0@6B@` @ 0x180154740 (B37_SLOTS.md §1):
//   [0] +0x00 0x180001680  vector deleting dtor (MOV EDX,0x58)
//   [1] +0x08 0x180001040  ?TypeName@Achievement@1@UEBAPEBDXZ
//   [2] +0x10 0x180001020  ?Type@Achievement@1@UEBAIXZ
//   [3] +0x18 0x18009e170  ?GetFileVersion@Achievement@1@UEBAHXZ      (`return 3`)
//   [4] +0x20 0x1800066e0  ?Compile@External@Object@@…  INHERITED
//   [5] +0x28 0x18009e070  ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z
//   [6] +0x30 0x180001270  ?Reconstruct@Achievement@1@UEAAPEAVExternal@Object@@XZ
//   [7] +0x38 0x180001370  ?Destruct@Achievement@1@UEAAXXZ
//   [8] +0x40 0x1800012f0  ?Construct@Achievement@1@UEAAPEAVExternal@Object@@XZ  (B5-16)
// Literal (B7-2): ?_ParentTypeName@Achievement@1@ 0x180001010 -> "Object::External".
//
// FIELDS, each by the one-instruction accessor that reads it, cross-checked
// against ?Serialize@Achievement@1@ (0x18009e070), which streams them in this
// order: ExternalPtr, String, String, ExternalPtr, String, then the goal
// array's count (+0x50) and body (+0x48).
// ============================================================================
class Achievement : public Object::External {
public:
    // ---- the nine slots, in retail order -----------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;  // [0] 0x180001680 — pure (B7-3/B7-13)
    const char* TypeName() const override;                    // [1] 0x180001040
    uint32_t    Type() const override;                        // [2] 0x180001020
    int32_t     GetFileVersion() const override;              // [3] 0x18009e170
    //          Compile                                       // [4] External's, inherited
    bool        Serialize(void* stream) override;             // [5] 0x18009e070 (IO::Stream&)
    Object::External* Reconstruct() override;                 // [6] 0x180001270
    void        Destruct() override;                          // [7] 0x180001370
    virtual Object::External* Construct();                    // [8] 0x1800012f0

    // ---- non-slot exported members (all `@Achievement@1@`, B7-11) ----------
    // The retslot ones return the prvalue; each registered door keeps
    // `(this, ret)` and placement-news it (B5-12).
    Object::ExternalPtr<UI::StringTable> GetStringTable() const { return _stringTable; }  // 0x180001380
    Object::ExternalPtr<UI::SpriteSheet> GetSpriteSheet() const { return _spriteSheet; }  // 0x180001390
    System::String GetSpriteName() const;                     // 0x1800013a0
    System::String GetTitle() const;                          // 0x1800013e0
    System::String GetDescription() const;                    // 0x180001420
    const GoalArray& GetGoalList() const { return _goals; }   // 0x180001460 — `LEA RAX,[RCX+0x48]`
    // ?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@VAchievementInstance@… 0x18009df80
    Object::InternalPtr<AchievementInstance> CreateInstance() const;

    static uint32_t TypeId();                                 // 0x180001030 — not ported
    static void* operator new(std::size_t bytes);             // 0x180001050 — not ported
    static void* operator new(std::size_t bytes, void* place);// 0x1800010c0 — not ported
    static void  operator delete(void* p);                    // 0x1800010d0 — not ported
    // 0x180001130 / 0x1800011f0 — ported as the generated factory rows in
    // rt_factories_achievement.inc (RtFactNew_/RtFactIP_Achievement_Achievement),
    // which stamp the MINTED tables; not members.
    static Object::Base* CreateNewObject();
    static Object::Base* CreateInPlaceObject(void* place);
    Achievement(const Achievement& other);                    // 0x180001470 — not ported
    Achievement& operator=(const Achievement& other);         // 0x180001550 — not ported

    // ??1Achievement@0@UEAA@XZ  0x18009e180. NON-virtual, PUBLIC (the exe's
    // Scenario slot 0 runs it explicitly — scenario.cpp:112, a caller-batch
    // site). Retail: re-stamp, ~Array(+0x48), release +0x40, +0x30, +0x28,
    // then ~External. The body is EMPTY: the compiler destroys the members in
    // REVERSE DECLARATION ORDER — `_goals`, `_spriteName`, `_description`,
    // `_title` (the two ExternalPtrs are trivial) — which is exactly retail's
    // order, then runs ~External().
    ~Achievement();

    // The door's way onto the shared body (ENDGAME B.3.8 block RB, B38_RULES
    // D1): `RtAch_Achievement_Ctor` stamps the MINTED tables and does the
    // inlined External stores, then runs this. Nothing else may.
    struct DoorBody { static void Init(Achievement* a) { a->InitFields(); } };

protected:
    // ??0Achievement@0@QEAA@XZ 0x18009e240 — the REAL constructor (ENDGAME
    // B.3.8 block RB, RULE B8-1): External() and the three stamps are the
    // compiler's; the seven zero stores +0x20..+0x50 are ONE body with the
    // door. (The members' own default ctors wrote the same zeros first.)
    Achievement() { InitFields(); }

private:
    // The seven qword zero stores of 0x18009e240, as the field writes they are.
    // Raw member writes, not assignments: the door runs this over raw storage.
    void InitFields() {
        _stringTable._ptr = nullptr;              // +0x20
        *reinterpret_cast<void**>(&_title) = nullptr;         // +0x28
        *reinterpret_cast<void**>(&_description) = nullptr;   // +0x30
        _spriteSheet._ptr = nullptr;              // +0x38
        *reinterpret_cast<void**>(&_spriteName) = nullptr;    // +0x40
        _goals._data = nullptr;                   // +0x48
        _goals._size = 0;                         // +0x50 } one qword
        _goals._capacity = 0;                     //       }
    }

private:
    //   0x180001000  ?_TypeName@Achievement@1@       -> "Achievement"
    //   0x180001010  ?_ParentTypeName@Achievement@1@ -> "Object::External"
    static const char* _TypeName();
    static const char* _ParentTypeName();

public:
    Object::ExternalPtr<UI::StringTable> _stringTable;   // +0x20  GetStringTable 0x180001380
    System::String                       _title;         // +0x28  GetTitle       0x1800013e0
    System::String                       _description;   // +0x30  GetDescription 0x180001420
    Object::ExternalPtr<UI::SpriteSheet> _spriteSheet;   // +0x38  GetSpriteSheet 0x180001390
    System::String                       _spriteName;    // +0x40  GetSpriteName  0x1800013a0
    GoalArray                            _goals;         // +0x48  GetGoalList    0x180001460
};
static_assert(sizeof(Achievement) == 0x58, "Achievement::Achievement is 0x58 bytes (slot 0 MOV EDX,0x58; ctor's last store +0x50)");
static_assert(alignof(Achievement) == 8, "Achievement::Achievement aligns to 8");
static_assert(offsetof(Achievement, _stringTable) == 0x20, "Achievement::_stringTable @ +0x20");
static_assert(offsetof(Achievement, _title)       == 0x28, "Achievement::_title @ +0x28");
static_assert(offsetof(Achievement, _description) == 0x30, "Achievement::_description @ +0x30");
static_assert(offsetof(Achievement, _spriteSheet) == 0x38, "Achievement::_spriteSheet @ +0x38");
static_assert(offsetof(Achievement, _spriteName)  == 0x40, "Achievement::_spriteName @ +0x40");
static_assert(offsetof(Achievement, _goals)       == 0x48, "Achievement::_goals @ +0x48 (Serialize reads its count at +0x50)");

// ============================================================================
// Achievement::AchievementInstance — an Object::Internal subclass, 0x28 bytes.
// FOUR slots. The exe subclasses it in retail (table 0x14015e350, built in
// GameUpdateLoop's slot-6 Load ON THE LOADER THREAD); the standalone never
// registers that table, so the object keeps the Runtime one. CONCRETE since
// ENDGAME B.3.8 block RB (slot 0 is the exe's scalar deleting dtor), so LOOP
// can build it as `::new (p) Achievement::AchievementInstance()` (D8).
//
// `??_7AchievementInstance@Achievement@@6B@` @ 0x180154700:
//   [0] +0x00 0x180001a90  vector deleting dtor (MOV EDX,0x28; ~GoalList(+0x10), ~Internal)
//   [1] +0x08 0x1800017c0  TypeName   [2] +0x10 0x1800017a0  Type
//   [3] +0x18 0x18009e4a0  ?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
// Literal: 0x180001790 -> "Object::Internal".
//
// ??0AchievementInstance (0x18009e3b0): CALL ??0Internal; stamp 0x180154700;
// zero the GoalList (+0x10, two qwords) and +0x20. SetAchievement (0x18009e440)
// `MOV [RCX+0x20],RDX` — the ExternalPtr<Achievement> arrives BARE (B7-5);
// GetGoalList (0x180001960) `LEA RAX,[RCX+0x10]`. CreateInstance allocates 0x28.
// ============================================================================
class AchievementInstance : public Object::Internal {
public:
    // [0] CONCRETE (ENDGAME B.3.8 block RB, B38_RULES D1/D2): the exe
    // instantiates the class itself (its table copy 0x14015e350, built in
    // GameUpdateLoop's Load), whose slot 0 is the exe scalar deleting dtor
    // 0x140019d90: CALL [??1AchievementInstance]; if (flags & 1) MemFree.
    // The DLL's 0x180001a90 stays the mint's RT_DELDTOR row for the doors.
    Object::Base* DeletingDtor(unsigned flags) override {
        this->~AchievementInstance();
        if (flags & 1) Object::detail::InternalMemFree(this);
        return this;
    }
    const char* TypeName() const override;                    // [1] 0x1800017c0
    uint32_t    Type() const override;                        // [2] 0x1800017a0
    void        Persist(void* stream) override;               // [3] 0x18009e4a0 (IO::Stream&)

    // ?GetAchievement@AchievementInstance…QEBA?AV?$ExternalPtr@VAchievement@1@… 0x180001950
    Object::ExternalPtr<Achievement> GetAchievement() const { return _achievement; }
    const GoalList& GetGoalList() const { return _goalList; }  // 0x180001960
    bool HasFailed() const;                                   // 0x18009e2b0
    bool IsMet() const;                                       // 0x18009e2f0
    void Update(const System::Time& time);                    // 0x18009e340
    // ?SetAchievement@…QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z 0x18009e440
    // — ExternalPtr by value arrives BARE in RDX (B7-5): a register value here.
    void SetAchievement(Object::ExternalPtr<Achievement> achievement);

    static uint32_t TypeId();                                 // 0x1800017b0 — not ported
    static void* operator new(std::size_t bytes);             // 0x1800017d0 — not ported
    static void* operator new(std::size_t bytes, void* place);// 0x180001840 — not ported
    static void  operator delete(void* p);                    // 0x180001850 — not ported
    static Object::Base* CreateNewObject();                   // 0x1800018b0 — factory row
    AchievementInstance(const AchievementInstance& other);    // 0x180001970 — not ported
    AchievementInstance& operator=(const AchievementInstance& other);  // 0x1800019f0 — not ported

    // ??1AchievementInstance@Achievement@@UEAA@XZ  0x18009e400. NON-virtual,
    // PUBLIC. Retail: re-stamp, ~GoalList(this+0x10), tail-jump ~Internal.
    // EMPTY body: the compiler destroys `_goalList` (a real GoalList — its
    // ~GoalList deletes every instance) and then runs ~Internal(). The
    // REGISTERED door for this address is `RtAchievement_AchievementInstance_Dtor`
    // in runtime/src/object/deleting_dtor_leaves.cpp (the coordinator's file)
    // and is untouched by this block.
    ~AchievementInstance();

    // ??0AchievementInstance@Achievement@@QEAA@XZ 0x18009e3b0 — the REAL
    // constructor (ENDGAME B.3.8 block RB, RULE B8-1), PUBLIC: the class is
    // concrete. Internal() and the stamp are the compiler's; the four zero
    // stores are ONE body with the door `RtAch_AchievementInstance_Ctor`.
    AchievementInstance() { InitFields(); }

    struct DoorBody { static void Init(AchievementInstance* a) { a->InitFields(); } };

private:
    void InitFields() {
        _goalList._goals._data     = nullptr;   // +0x10
        _goalList._goals._size     = 0;         // +0x18 } one qword
        _goalList._goals._capacity = 0;         //       }
        _achievement._ptr          = nullptr;   // +0x20
    }

private:
    //   0x180001780  ?_TypeName@AchievementInstance@…       -> "AchievementInstance"
    //   0x180001790  ?_ParentTypeName@AchievementInstance@… -> "Object::Internal"
    static const char* _TypeName();
    static const char* _ParentTypeName();

public:
    GoalList                         _goalList;      // +0x10 .. +0x1f  held BY VALUE (B7-8)
    Object::ExternalPtr<Achievement> _achievement;   // +0x20
};
static_assert(sizeof(AchievementInstance) == 0x28,
              "AchievementInstance is 0x28 bytes (slot 0 MOV EDX,0x28; CreateInstance allocates 0x28)");
static_assert(alignof(AchievementInstance) == 8, "AchievementInstance aligns to 8");
static_assert(offsetof(AchievementInstance, _goalList)    == 0x10, "AchievementInstance::_goalList @ +0x10 (GetGoalList 0x180001960)");
static_assert(offsetof(AchievementInstance, _achievement) == 0x20, "AchievementInstance::_achievement @ +0x20 (SetAchievement 0x18009e440)");

// ============================================================================
// Achievement::AchievementManager — 0x48 bytes, a process singleton.
// NON-POLYMORPHIC (B37_SLOTS §7: the ctor 0x18009eb00 zeroes +0x00..+0x40 and
// stamps no vtable).
//
// Allocated by `Engine::Startup` (`MemAlloc(0x48)` at 0x1800dbe42) with the
// constructor INLINED there, so the ctor stays the door in
// runtime/src/engine/engine_lifecycle.cpp (block B1's file); `_singleton`
// stays an RT_DATA_SLOT / manifest row until the end of B.3 (carry-over 5).
// `PlatformStartup` is defined on the member in B1's platform_startup_win.cpp
// (the B.3.7 fragment pass); its registered door there delegates to it.
//
// THREE OF THE SIX FIELD NAMES ARE THE ENGINE'S OWN, from the profile build's
// assert strings (`Achievement\AchievementManager.cpp`): `_achievements` (+0x00),
// `_completed` and `_lastAchievementUpdate` ("!_completed[_lastAchievementUpdate]").
// `_instances`, `_eventHandler` and `_updateTimes` are OUR names.
//
// FIELD PINS, one retail body each:
//   +0x00 GetAchievementCount (0x1800079b0) = this->_achievements->_count —
//         UNGUARDED: a manager that never had SetAchievements faults on a read
//         of 0x28, which is the recorded symptom and the only signal.
//   +0x08 IsAchievementAwarded (0x1800079c0) one byte of `_completed`.
//   +0x18 GetAchievementInstance (0x18009e8f0) `*(this+0x18)[i]`; +0x20 its _size.
//   +0x28 SetEventHandler (0x18009e6b0).
//   +0x30 Update (0x18009e6c0) `(*(this+0x30))[i] += time` — an
//         Array<System::Time> of ACCUMULATED per-achievement time.
//   +0x40 Update's rotating cursor.
// The profile build's COPY CONSTRUCTOR (0x180029a40) walks the whole object in
// exactly this shape: three deep-copied Arrays, two raw pointers, one int.
//
// THE THREE ARRAYS NAME THEIR RETAIL ROWS IN THEIR TYPES (0x180008b90
// kSpec_Bool, 0x180008d60 kSpec_PtrZero, 0x180008ff0 kSpec_U64Zero — none is
// trait-equivalent, achievement_manager.cpp says why for each), which is what
// lets SetSize be a plain member call instead of an `AsArray` bridge. Their
// destructors are each 0x1800c7ad0's free-then-zero with no element destroy,
// so the compiler-emitted member destruction IS ??1AchievementManager's tail.
// ============================================================================
class AchievementManager {
public:
    using CompletedArray = Collection::Array<bool, Collection::DefaultAllocator,
                                             Collection::ExplicitSpec<&rtcoll::kSpec_Bool>>;
    using InstanceArray  = Collection::Array<Object::InternalPtr<AchievementInstance>,
                                             Collection::DefaultAllocator,
                                             Collection::ExplicitSpec<&rtcoll::kSpec_PtrZero>>;
    using TimeArray      = Collection::Array<System::Time, Collection::DefaultAllocator,
                                             Collection::ExplicitSpec<&rtcoll::kSpec_U64Zero>>;
    using EventHandler   = void (*)(int);

    // ??0AchievementManager@Achievement@@QEAA@XZ 0x18009eb00 — the DOOR in
    // runtime/src/engine/engine_lifecycle.cpp (B1). Declared, defined nowhere.
    AchievementManager();
    // ??1AchievementManager@Achievement@@QEAA@XZ 0x18009eab0 — real (see the body).
    ~AchievementManager();
    AchievementManager(const AchievementManager& other);            // 0x1800079e0 — not ported
    AchievementManager& operator=(const AchievementManager& other); // 0x180007a70 — not ported

    static AchievementManager* Get();                          // 0x1800079d0
    int  GetAchievementCount() const;                          // 0x1800079b0
    bool IsAchievementAwarded(int index) const;                // 0x1800079c0
    Object::ExternalPtr<Achievement> GetAchievement(int index) const;                  // 0x18009e8a0 retslot
    Object::InternalPtr<AchievementInstance> GetAchievementInstance(int index) const;  // 0x18009e8f0 retslot
    void SetEventHandler(EventHandler handler);                // 0x18009e6b0
    // ?SetAchievements@…QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z
    // 0x18009e860 — the ExternalPtr arrives BARE in RDX (B7-5: `MOV [RCX],RDX`).
    void SetAchievements(Object::ExternalPtr<Object::ExternalList> list);
    void Reset();                                              // 0x18009e5c0
    void Update(const System::Time& time);                     // 0x18009e6c0
    void Persist(IO::Stream& stream);                          // 0x18009e910
    void RemoveAll();                                          // 0x18009e9d0
    void ReadAchievements();                                   // 0x18009eb90
    bool SetAchievementMet(int index);                         // 0x18009eca0
    bool Startup();                                            // 0x18009eaa0 — not ported (no caller)
    void Shutdown();                                           // 0x18009ea90 — not ported (no caller)

    // The four private `AEAA` platform hooks (exported — this DLL exports its
    // privates, reference_aeaa_is_private_not_unexported.md). PUBLIC here, as
    // in Application.h: each registered door is a free function that has to
    // call its member, and access control is not ABI (B.3.7 fragment).
    void PlatformConstruct();                                  // 0x18009eb50 — not ported
    void PlatformDestruct();                                   // 0x18009eb60 — not ported
    bool PlatformStartup();                                    // 0x18009eb70 — platform_startup_win.cpp (member since the B.3.7 fragment)
    void PlatformShutdown();                                   // 0x18009eb80 — not ported

public:
    Object::ExternalList* _achievements;           // +0x00 `_achievements`
    CompletedArray        _completed;              // +0x08 `_completed`
    InstanceArray         _instances;              // +0x18 (our name)
    EventHandler          _eventHandler;           // +0x28 (our name)
    TimeArray             _updateTimes;            // +0x30 (our name)
    int32_t               _lastAchievementUpdate;  // +0x40 `_lastAchievementUpdate`
    int32_t               _pad44;                  // +0x44 never read or written
};
static_assert(sizeof(AchievementManager) == 0x48,
              "AchievementManager is 0x48 bytes (Engine::Startup MemAlloc(0x48) at 0x1800dbe42)");
static_assert(alignof(AchievementManager) == 8, "AchievementManager aligns to 8");
static_assert(offsetof(AchievementManager, _achievements)          == 0x00, "_achievements @ +0x00");
static_assert(offsetof(AchievementManager, _completed)             == 0x08, "_completed @ +0x08");
static_assert(offsetof(AchievementManager, _instances)             == 0x18, "_instances @ +0x18");
static_assert(offsetof(AchievementManager, _eventHandler)          == 0x28, "_eventHandler @ +0x28");
static_assert(offsetof(AchievementManager, _updateTimes)           == 0x30, "_updateTimes @ +0x30");
static_assert(offsetof(AchievementManager, _lastAchievementUpdate) == 0x40, "_lastAchievementUpdate @ +0x40");
static_assert(!std::is_polymorphic<AchievementManager>::value, "no vtable (ctor 0x18009eb00)");
static_assert(sizeof(bool) == 1, "_completed is one byte per achievement");

}  // namespace Achievement
