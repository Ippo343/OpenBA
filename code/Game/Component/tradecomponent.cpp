// code/Game/Component/tradecomponent.cpp
// Clean reimplementation of TradeComponent lifecycle methods from Application-x64.exe.
//
// TradeComponent is the largest ControllerComponent variant (0xa8 bytes) with
// trade state, item arrays, and a controller sub-object at +0x88.
// Three non-trivial vtable overrides:
//   Shutdown (0x14005bd50) — delegated to original exe (too complex for safe reimpl)
//   Draw     (0x14005c1d0) — add entity to trade pending list + call setup
//   OnCreate (0x14005c190) — register entity with world trade list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/tradecomponent_funcs.h"
#include "Component/TradeDescription.h"
#include "Component/coll_bridge.h"
#include "Work/tradework_funcs.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"   // WorkBase_FindComponent_3050
#include "system/MemoryAllocator.h"

// ============================================================
// Helpers
// ============================================================
// FUN_140002cc0 CemeteryListErase_Reimpl — the world entity-list erase (by value).
// FUN_140003110 — add entity to world pending list (GameColl SetSizeFill).
// FUN_14005c220 TradeComponent_Setup (below) — called after adding to pending list.

// 0x14005c1d0 door: registered by address (standalone/reg_component_economy.cpp).
void TradeComponent_Draw(TradeComponent* self) {
    self->TradeComponent::OnAdded();
}


// 0x14005c190 door: registered by address (standalone/reg_component_economy.cpp).
void TradeComponent_OnCreate(TradeComponent* self) {
    self->TradeComponent::OnRemove();
}


// ============================================================
// TradeComponent slots 13 and 14 — the work-enable pair
// ============================================================
// These are the two remaining TradeComponent-specific vtable entries (the full
// vtable is 0x14015acd0; only slots 3/6/7/13/14 are TradeComponent's own, the rest
// are Component base). Both were inert no-op stubs, which is what made the trading
// post report "work disabled" permanently and ignore the work button:
//
//   slot 13 (0x14005c140) is the QUERY the UI reads for the work toggle's state.
//     It returns 3 when the workplace is accepting and 1 when it is not — never 0.
//     An inert stub returns 0, which is neither, and reads as "disabled here".
//   slot 14 (0x14005c000) is the TOGGLE ACTION. It flips the workplace's accepting
//     flag and, on enable, allocates a TradeWork and posts it to the work
//     scheduler; on disable it cancels the posted work.
//
// They must be promoted together. Slot 13 alone would report a state that slot 14
// is not able to change; slot 14 alone would change a state nothing reads.

// FUN_140002e10 AttendanceComponent_GetJobHostComponent — find the WorkPlaceComponent
//   (type 0x1401c80e0) on an entity.
// FUN_140003050 WorkBase_FindComponent_3050 — the component the work scheduler is
//   posted through.
// FUN_14012ec60 CitizenAI_WorkScheduler_Post — post a Work to the scheduler (stop
//   existing slot, then install).
// FUN_14012eb50 WorkComponent_RegisterWorkSlot — cancel whatever work is currently
//   posted on that scheduler.
// FUN_14012fc00 WorkPlaceComponent_SetAccepting — commit the new accepting state.

// The first call into the work pair prints the five dependencies once, as it always has.
static void TC_ReportWorkDepsOnce() {
    static bool s_reported = false;
    if (s_reported) return;
    s_reported = true;
    fprintf(stderr, "[TradeComp] work deps: findWP=%p findSched=%p post=%p "
            "cancel=%p setAccept=%p\n",
            (void*)&AttendanceComponent_GetJobHostComponent, (void*)&WorkBase_FindComponent_3050,
            (void*)&CitizenAI_WorkScheduler_Post, (void*)&WorkComponent_RegisterWorkSlot,
            (void*)&WorkPlaceComponent_SetAccepting);
    fflush(stderr);
}

// 0x14005c140 door: registered by address (standalone/reg_component_economy.cpp).
uint8_t TradeComponent_Slot13(TradeComponent* selfP) {
    return static_cast<uint8_t>(selfP->TradeComponent::CommandMask(0));
}


// 0x14005c000 door: registered by address (standalone/reg_component_economy.cpp).
void TradeComponent_Slot14(TradeComponent* selfP) {
    selfP->TradeComponent::Command(0);
}


// ============================================================
// Reimpl of FUN_14005c220 (TradeComponent_impl_c220: build the trade name list
//   and item table from the TradeDescription template)
// ============================================================
// Reads the TradeDescription off owner+0x18 (type 0x1401c4bb8). Resizes the
// trade-name array (+0x30) to desc+0x50 and fills each with a random merchant
// name from FUN_1400a4c60; resizes the item table (+0x40) to desc+0x40 and copies
// resource ids from desc+0x38 (0x18 stride -> 0xc stride).
// Helpers: 0x14000e760 TradeComponent_ResizeNames, 0x14000e8d0 TradeComponent_ResizeItems
// (below), 0x1400a4c60 NameDB_PickAndGenerate, import slots 0x1401514c0
// System::String::operator= and 0x140152af0 System::String::~String.
void TradeComponent_Setup(void* selfP) {
    uintptr_t self = (uintptr_t)selfP;
    if (GameUpdateLoop::_singleton == nullptr) return;

    void* descObj = *(void**)(self + 0x18);
    void* desc = (Object::IsDerivedFromNullSafe(descObj, TradeDescription::TypeId())) ? descObj : nullptr;

    // Resize the name array (+0x30) to desc+0x50
    TradeComponent_ResizeNames((int64_t*)(self + 0x30), *(int*)((uintptr_t)desc + 0x50));

    // Fill each name slot with a random merchant name
    int nameCount = *(int*)(self + 0x38);
    void* random = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0xd8);
    for (int i = 0; i < nameCount; i++) {
        uint32_t r = reinterpret_cast<System::Random*>(random)->Rand(2u);   // B.3.4b bridge
        uint8_t strBuf[8] = {0};
        NameDB_PickAndGenerate(strBuf, random, (char)(r == 0));   // 0x1400a4c60
        void* dst = (void*)((int64_t)i * 0x20 + *(int64_t*)(self + 0x30));
        *reinterpret_cast<System::String*>(dst) =
            *reinterpret_cast<const System::String*>(strBuf);      // B.3.1 bridge (slot 0x1401514c0)
        reinterpret_cast<System::String*>(strBuf)->~String();    // B.3.1 bridge (slot 0x140152af0)
    }

    // Report the OUTCOME, not the call. A populated +0x30 is the whole point of
    // this function — TradeUI_RefreshBuyTable indexes it — and "Setup ran" is not
    // evidence it filled anything: desc null, or a desc+0x50 of 0, both leave the
    // array NULL and read identically from a bare entry trace.
    fprintf(stderr, "[TradeComp] Setup: desc=%p names=%d array=%p\n",
            desc, nameCount, (void*)*(int64_t*)(self + 0x30));
    // ...and the names themselves. A populated array is NOT evidence the names are
    // real: while 0x1400a4c60 was unregistered, its door was null, strBuf stayed
    // zeroed, and every slot got an EMPTY string — which reads identically to a
    // healthy array from the line above.
    {
        int64_t arr = *(int64_t*)(self + 0x30);
        for (int i = 0; arr && i < nameCount; i++) {
            const char* nm = *(const char**)(arr + (int64_t)i * 0x20);
            fprintf(stderr, "[TradeComp]   name[%d]=\"%s\"\n", i, nm ? nm : "(null)");
        }
    }

    // Resize the item table (+0x40) to desc+0x40
    TradeComponent_ResizeItems((int64_t*)(self + 0x40), *(int*)((uintptr_t)desc + 0x40));

    // Copy resource ids from desc+0x38 (0x18 stride) into item table (0xc stride)
    int itemCount = *(int*)(self + 0x48);
    int64_t srcBase = *(int64_t*)((uintptr_t)desc + 0x38);
    int64_t dstBase = *(int64_t*)(self + 0x40);
    for (int i = 0; i < itemCount; i++) {
        *(uint32_t*)(dstBase + (int64_t)i * 0xc) = *(uint32_t*)(srcBase + (int64_t)i * 0x18);
        *(int*)(dstBase + (int64_t)i * 0xc + 4) = i;
    }
}

// ============================================================
// Reimpl of FUN_14000e6b0 (TradeComponent inner ptr-array copy/clone)
// ============================================================
// Each trade-name element (0x20 bytes) holds, at +0x10, an inner 8-byte-stride
// pointer array (ptr@+0, count@+8, capacity@+0xc). This frees the destination's
// existing buffer, allocates a fresh count*8 (8-aligned) buffer, and copies the
// source pointers in. Faithful 1:1 port of TradeComponent_helper_e6b0; closure =
// MemAlloc/MemFree only.
int64_t* TradeComponent_CopyPtrArray(int64_t* dst, int64_t* src) {
    System::MemoryAllocator::MemFree((void*)dst[0]);
    dst[1] = 0;
    dst[0] = 0;
    int count = (int)src[1];
    *(int*)(dst + 1) = count;
    if (count > 0) {
        *(int*)((int64_t)dst + 0xc) = count;
        void* buf = System::MemoryAllocator::MemAlloc((uint64_t)count * 8, 8);
        dst[0] = (int64_t)buf;
    }
    int n = (int)dst[1];
    for (int64_t i = 0, off = 0; i < n; i++, off += 8) {
        if ((void*)(dst[0] + off) != nullptr)
            *(int64_t*)(dst[0] + off) = *(int64_t*)(off + src[0]);
    }
    return dst;
}

// ============================================================
// Reimpl of FUN_14000ef10 (TradeComponent name-element array destructor)
// ============================================================
// Destroy `count` trailing trade-name elements (0x20 stride). For each: free the
// inner ptr-array buffer at +0x10, zero its ptr/count, then run String::~String on
// the element's String at +0x00. Faithful 1:1 port of TradeComponent_sub_ef10;
// closure = MemFree + System::String::~String (runtime export).
void TradeComponent_DtorNameArray(void* /*unused*/, int64_t arrayBase, int count) {
    if (count <= 0) return;
    // puVar1 starts at arrayBase + 0x10 (the inner ptr-array of element 0).
    int64_t* p = (int64_t*)(arrayBase + 0x10);
    for (int64_t k = count; k != 0; k--) {
        System::MemoryAllocator::MemFree((void*)p[0]);
        p[0] = 0;
        p[1] = 0;
        // String::~String on (p - 2) == element start (+0x00).
        reinterpret_cast<System::String*>(p - 2)->~String();   // B.3.1 bridge
        p += 4;   // advance 0x20 bytes (4 * 8)
    }
}

// ============================================================
// Reimpl of FUN_14000e760 (TradeComponent name-element array resize, 0x20 stride)
// ============================================================
// Grow/shrink the (count@+8, capacity@+0xc) name array of 0x20-byte elements. On
// growth past capacity, allocate cap*0x20 (8-aligned; cap = 3*newCount on a +1 grow
// from non-empty), then for each retained element copy its String (+0x00) + int
// (+0x08) and clone its inner ptr-array (+0x10) via CopyPtrArray; new elements are
// zeroed. Shrink path destroys the dropped elements (DtorNameArray). Faithful 1:1
// port of TradeComponent_impl_e760; closure = e6b0 + ef10 + String::String/Mem*.
void TradeComponent_ResizeNames(int64_t* field, int count) {
    int oldCount = (int)field[1];
    if (count == oldCount) return;

    int8_t* oldBuf = (int8_t*)field[0];
    *(int*)(field + 1) = count;

    if (*(int*)((int64_t)field + 0xc) < count) {
        *(int*)((int64_t)field + 0xc) = count;
        if (count == oldCount + 1 && oldCount != 0)
            *(int*)((int64_t)field + 0xc) = count * 3;
        void* buf = System::MemoryAllocator::MemAlloc(
            (uint64_t)(*(int*)((int64_t)field + 0xc)) * 0x20, 8);
        field[0] = (int64_t)buf;
    }

    int8_t* newBuf = (int8_t*)field[0];
    int startIdx = (oldBuf == newBuf) ? oldCount : 0;
    for (int64_t i = startIdx; i < count; i++) {
        int8_t* dstElem = newBuf + i * 0x20;
        if (i < oldCount) {
            int8_t* srcElem = oldBuf + i * 0x20;
            if (dstElem) {
                ::new (static_cast<void*>(dstElem)) System::String(
                    *reinterpret_cast<const System::String*>(srcElem));   // B.3.1 bridge: String at +0x00
                *(int*)(dstElem + 8) = *(int*)(srcElem + 8);                       // int at +0x08
                *(int64_t*)(dstElem + 0x10) = 0;
                *(int64_t*)(dstElem + 0x18) = 0;
                if ((dstElem + 0x10) != (srcElem + 0x10))
                    TradeComponent_CopyPtrArray((int64_t*)(dstElem + 0x10),
                                                (int64_t*)(srcElem + 0x10));
            }
        } else {
            if (dstElem) {
                *(int64_t*)(dstElem + 0x00) = 0;
                *(int64_t*)(dstElem + 0x08) = 0;
                *(int64_t*)(dstElem + 0x10) = 0;
                *(int64_t*)(dstElem + 0x18) = 0;
            }
        }
    }

    if (oldBuf == newBuf) {
        if (count < oldCount)
            TradeComponent_DtorNameArray(nullptr, (int64_t)(oldBuf + (int64_t)count * 0x20),
                                         oldCount - count);
    } else {
        TradeComponent_DtorNameArray(nullptr, (int64_t)oldBuf, oldCount);
        System::MemoryAllocator::MemFree(oldBuf);
    }
}

// ============================================================
// Reimpl of FUN_14000e8d0 (TradeComponent item-table resize)
// ============================================================
// Grow/shrink the (count@+8, capacity@+0xc) item array of 0xc-byte entries.
// On growth past capacity, allocate cap*0xc (4-aligned), copy old entries (3 ints
// each), zero-init new entries' third int, free the old buffer. Faithful 1:1 port
// of TradeComponent_sub_e8d0; closure = MemAlloc/MemFree only.
void TradeComponent_ResizeItems(int64_t* field, int count) {
    int oldCount = *(int*)(field + 1);
    if (count == oldCount) return;

    void* oldBuf = (void*)field[0];
    *(int*)(field + 1) = count;

    if (*(int*)((int64_t)field + 0xc) < count) {
        *(int*)((int64_t)field + 0xc) = count;
        if (count == oldCount + 1 && oldCount != 0) {
            *(int*)((int64_t)field + 0xc) = count * 3;
        }
        void* newBuf = System::MemoryAllocator::MemAlloc(
            (uint64_t)(*(int*)((int64_t)field + 0xc)) * 0xc, 4);
        field[0] = (int64_t)newBuf;
    }

    int startIdx = (oldBuf == (void*)field[0]) ? oldCount : 0;
    for (int64_t i = startIdx; i < count; i++) {
        int64_t off = i * 0xc;
        if (i < oldCount) {
            uint32_t* dst = (uint32_t*)(field[0] + off);
            if (dst) {
                dst[0] = *(uint32_t*)(off + (int64_t)oldBuf);
                dst[1] = *(uint32_t*)(off + 4 + (int64_t)oldBuf);
                dst[2] = *(uint32_t*)(off + 8 + (int64_t)oldBuf);
            }
        } else {
            int64_t dst = field[0] + off;
            if (dst) *(uint32_t*)(dst + 8) = 0;
        }
    }

    if (oldBuf != (void*)field[0]) {
        System::MemoryAllocator::MemFree(oldBuf);
    }
}

// ===== ENDGAME B.3.8 MI-B — TradeComponent as a REAL class =====
// (TradeComponent.h). Before: COMPONENT_INERT_FACTORY expanded by hand (48-slot
// table, slots 1/3/6/13/14/16/17 patched per create, Inert_ScalarDtor in slot 0, a
// CtorInit writing only +0x2c = -1, and Dispatcher<TradeCtrlTag> stamped into raw
// bytes at +0x88 with its back-pointer at +0xa0). After: the compiler's 18-slot
// vtable, a real ctor/dtor chain, the controller a member object at retail's +0x88
// (B38_RULES D1, D2, D6).
#include "Object/factory_registration.h"

void WorkBase_TreeNodeFree_e320(void* root, void* node);                       // work_base_reimpl.cpp, 0x14000e320

// 0x14005c430. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// +0x28 = 0; +0x2c = -1 (the "no merchant selected" sentinel: tradeui's `>= 0` test
// and comboEnabled key off it); +0x30..+0x58 = 0; +0x60 = 0 (qword), +0x68 = 0
// (dword); +0x71 = 0; +0x78 = +0x80 = 0; the member at +0x88 with its back-pointer
// (+0xa0); then `_flags |= 3` (0x83).
TradeComponent::TradeComponent()
    : tradeState(0), tradeId(-1), field30(nullptr), field38(nullptr), field40(nullptr),
      field48(nullptr), field50(nullptr), field58(nullptr), field60(nullptr), field68(0),
      _pad6C{}, tradeFlag(0), _pad72{}, field78(nullptr), field80(nullptr),
      _controller(this) {
    OrFlagsInCtor(0x3);
}

// 0x14005c340: frees the +0x60 buy-order BST, +0x50, +0x40 and the +0x30 name array,
// then the restamps and ~Internal (the compiler chain).
//   +0x60: the root's two subtrees through 0x14000e320 (children at +0x18/+0x20;
//          ported as WorkBase_TreeNodeFree_e320), then the root; +0x60 = 0, +0x68 = 0
//   +0x50: MemFree, +0x50 = +0x58 = 0
//   +0x40: MemFree, +0x40 = +0x48 = 0
//   +0x30: the name array's elements through 0x14000ef10 (count +0x38), MemFree,
//          +0x30 = +0x38 = 0
TradeComponent::~TradeComponent() {
    if (void* root = field60) {
        WorkBase_TreeNodeFree_e320(&field60, *reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(root) + 0x18));
        WorkBase_TreeNodeFree_e320(&field60, *reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(root) + 0x20));
        Object::detail::InternalMemFree(root);
    }
    field60 = nullptr;
    field68 = 0;
    Object::detail::InternalMemFree(field50);
    field50 = nullptr;
    field58 = nullptr;
    Object::detail::InternalMemFree(field40);
    field40 = nullptr;
    field48 = nullptr;
    TradeComponent_DtorNameArray(nullptr, reinterpret_cast<int64_t>(field30),
                                 *reinterpret_cast<int32_t*>(&field38));
    Object::detail::InternalMemFree(field30);
    field30 = nullptr;
    field38 = nullptr;
}

// create 0x14000db60: MemAlloc(0xa8, 8), then the ctor.
void* TradeComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TradeComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TradeComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* TradeComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TradeComponent());
}

// [3] 0x14005bd50 — TradeComponent::Persist lives in tradecomponent_serialize.cpp.
// ============================================================
// TradeComponent::OnAdded (vtable slot [6]; Ghidra "Draw", 0x14005c1d0)
// ============================================================
// Adds the owning entity to the world trade pending list, then
// calls TradeSetup to initialize trade state.
//
// Original disassembly:
//   MOV RAX,[RCX+0x10]          ; entity ptr from owner
//   MOV RBX,RCX                 ; save self
//   LEA RDX,[RSP+0x20]          ; stack InternalBasePtr
//   MOV [RSP+0x20],RAX          ; store entity ptr
//   MOV RAX,[DAT_1401c3dd8]     ; game state singleton
//   MOV RCX,[RAX+0xf0]          ; world manager
//   MOV EDX,[RCX+0x38]          ; current count
//   ADD EDX,1                   ; newCount = count + 1
//   ADD RCX,0x30                ; trade pending list
//   LEA R8,[RSP+0x20]           ; entity ptr
//   CALL FUN_140003110           ; add to pending list
//   MOV RCX,RBX                 ; self
//   CALL FUN_14005c220           ; trade setup

void TradeComponent::OnAdded() {
    if (GameUpdateLoop::_singleton == nullptr)
        return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)_entity;

    uintptr_t gameState = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Add to trade pending list at worldMgr+0x30
    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    int currentCount = *(int*)(worldMgr + 0x38);
    GameColl::AsArray<GameColl::Ptr8>((void*)(worldMgr + 0x30))
        .SetSizeFill(currentCount + 1, &entityPtr);

    // Call trade setup
    TradeComponent_Setup(this);   // 0x14005c220
}
// ============================================================
// TradeComponent::OnRemove (vtable slot [7]; Ghidra "OnCreate", 0x14005c190)
// ============================================================
// Registers the owning entity with the world trade entity list.
//
// Original disassembly:
//   MOV RAX,[RCX+0x10]          ; entity ptr from owner
//   LEA RDX,[RSP+0x08]          ; stack InternalBasePtr
//   MOV [RSP+0x08],RAX          ; store entity ptr
//   MOV RAX,[DAT_1401c3dd8]     ; game state singleton
//   MOV RCX,[RAX+0xf0]          ; world manager
//   ADD RCX,0x30                ; trade entity list
//   CALL FUN_140002cc0           ; register entity

void TradeComponent::OnRemove() {
    if (GameUpdateLoop::_singleton == nullptr)
        return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)_entity;

    uintptr_t gameState = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr+0x30 (trade entity list)
    CemeteryListErase_Reimpl((void*)(worldMgr + 0x30), &entityPtr);   // 0x140002cc0
}
// [13] 0x14005c140 / [14] 0x14005c000 — the work-enable pair; they go together (a
// live query over a stub toggle reports a state nothing can change). Neither
// reads its command id.
// TradeComponent::Slot13 (vtable index 13, 0x14005c140) — work-state query.
//   return -(WPC[0x50] != 0) & 2 | 1   =>  3 when accepting, 1 when not.
uint32_t TradeComponent::CommandMask(int /*command*/) {
    TC_ReportWorkDepsOnce();
    uintptr_t self = reinterpret_cast<uintptr_t>(this);

    uint64_t wpc[2] = {0, 0};
    AttendanceComponent_GetJobHostComponent(*(void**)(self + 0x10), &wpc);   // 0x140002e10
    if (!wpc[0]) return 1;

    return (*(char*)(wpc[0] + 0x50) != 0) ? 3 : 1;
}
// TradeComponent::Slot14 (vtable index 14, 0x14005c000) — work-state toggle.
// Reads the workplace's CURRENT accepting flag (+0x50) and inverts it:
//   currently OFF -> allocate a TradeWork, post it, commit accepting = 1
//   currently ON  -> cancel the posted work,          commit accepting = 0
// TradeComponent+0x70 caches the new state for the panel.
void TradeComponent::Command(int /*command*/) {
    TC_ReportWorkDepsOnce();
    uintptr_t self = reinterpret_cast<uintptr_t>(this);

    int64_t sched = 0;
    WorkBase_FindComponent_3050(*(uintptr_t*)(self + 0x10), &sched);   // 0x140003050

    int64_t wpc = 0;
    AttendanceComponent_GetJobHostComponent(*(void**)(self + 0x10), &wpc);   // 0x140002e10

    if (!wpc) return;

    // enable == "the workplace is NOT currently accepting", i.e. this is a toggle.
    bool enable = (*(char*)(wpc + 0x50) == '\0');
    *(bool*)(self + 0x70) = enable;

    char newState;
    if (enable) {
        void* work = nullptr;
        // MemAlloc(0x58, 8) + the from-entity TradeWork ctor 0x1400f6ea0, as retail.
        // ENDGAME B.3.8 MI-B: the REAL TradeWork (Work/TradeWork.h) — its default ctor
        // runs TradeWork_ResolveFuncs (the reason the hand build had to call it) and
        // zeroes +0x48/+0x50, the compiler stamps its table, and Work::InitFromEntity is
        // Work_Ctor25's body (0x1400f72e0). No more read-back of 0x140161a10.
        void* mem = System::MemoryAllocator::MemAlloc(0x58, 8);
        if (mem) {
            int64_t entityIBP = *(int64_t*)(self + 0x10);
            TradeWork* tw = new (mem) TradeWork();
            tw->InitFromEntity(&entityIBP);
            work = mem;
        }
        uint64_t workIBP[1] = {0};
        new (workIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
        CitizenAI_WorkScheduler_Post(static_cast<uintptr_t>(sched), 1, '\0', workIBP);   // 0x14012ec60
        newState = '\x01';
    } else {
        WorkComponent_RegisterWorkSlot(reinterpret_cast<void*>(sched));   // 0x14012eb50
        newState = '\0';
    }

    WorkPlaceComponent_SetAccepting(wpc, newState != '\0');   // 0x14012fc00

    fprintf(stderr, "[TradeComp] Slot14 toggle: wpc=%p was=%d now=%d\n",
            (void*)wpc, !enable, (int)newState);
    fflush(stderr);
}

DEFINE_EXE_TYPE(TradeComponent, "ControllerComponent")
