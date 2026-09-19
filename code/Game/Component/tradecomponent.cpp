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
#include "Component/coll_bridge.h"
#include "Work/tradework_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140002cc0 — register entity with world entity list
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

// FUN_140003110 — add entity to world pending list

// FUN_14005c220 — trade component setup (called after adding to pending list)
typedef void (*fn_TradeSetup_t)(void* self);
static fn_TradeSetup_t s_fn_TradeSetup = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_fn_TradeSetup = reinterpret_cast<fn_TradeSetup_t>(
        ResolveAddr(Module::Exe, 0x14005c220));

    s_initialized = true;
}

// ============================================================
// TradeComponent::Draw (vtable slot [6], 0x14005c1d0)
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

void TradeComponent_Draw(TradeComponent* self) {
    EnsureHelpers();

    if (!s_fn_TradeSetup ||
        !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Add to trade pending list at worldMgr+0x30
    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    int currentCount = *(int*)(worldMgr + 0x38);
    GameColl::AsArray<GameColl::Ptr8>((void*)(worldMgr + 0x30))
        .SetSizeFill(currentCount + 1, &entityPtr);

    // Call trade setup
    s_fn_TradeSetup(self);
}

// ============================================================
// TradeComponent::OnCreate (vtable slot [7], 0x14005c190)
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

void TradeComponent_OnCreate(TradeComponent* self) {
    EnsureHelpers();

    if (!s_fn_RegisterEntity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr+0x30 (trade entity list)
    s_fn_RegisterEntity((void*)(worldMgr + 0x30), &entityPtr);
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

// FUN_140002e10 — find the WorkPlaceComponent (type 0x1401c80e0) on an entity.
typedef void (*fn_FindWorkPlace_t)(int64_t entity, void* outIBP);      // 0x140002e10
// FUN_140003050 — find the component the work scheduler is posted through.
typedef void (*fn_FindWorkSched_t)(int64_t entity, void* outIBP);      // 0x140003050
// FUN_1400f72e0 — Work base ctor (Work_Ctor25), takes the owning entity IBP.
typedef void (*fn_WorkCtor25_t)(void* work, void* entityIBP);          // 0x1400f72e0
// FUN_14012ec60 — post a Work to the scheduler (stop existing slot, then install).
typedef void (*fn_PostWork_t)(int64_t sched, uint32_t code, char flag, void* workIBP); // 0x14012ec60
// FUN_14012eb50 — cancel whatever work is currently posted on that scheduler.
typedef void (*fn_CancelWork_t)(int64_t sched);                        // 0x14012eb50
// FUN_14012fc00 — commit the new accepting state onto the WorkPlaceComponent.
typedef void (*fn_SetAccepting_t)(int64_t workPlaceComp, char accepting); // 0x14012fc00

static fn_FindWorkPlace_t  s_tc_FindWorkPlace = nullptr;
static fn_FindWorkSched_t  s_tc_FindWorkSched = nullptr;
static fn_WorkCtor25_t     s_tc_WorkCtor25 = nullptr;
static fn_PostWork_t       s_tc_PostWork = nullptr;
static fn_CancelWork_t     s_tc_CancelWork = nullptr;
static fn_SetAccepting_t   s_tc_SetAccepting = nullptr;
static void**              s_tc_TradeWorkVtable = nullptr;
static bool s_tcWorkResolved = false;

static void EnsureTradeWorkHelpers() {
    if (s_tcWorkResolved) return;
    s_tc_FindWorkPlace = reinterpret_cast<fn_FindWorkPlace_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_tc_FindWorkSched = reinterpret_cast<fn_FindWorkSched_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_tc_WorkCtor25    = reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_tc_PostWork      = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_tc_CancelWork    = reinterpret_cast<fn_CancelWork_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_tc_SetAccepting  = reinterpret_cast<fn_SetAccepting_t>(ResolveAddr(Module::Exe, 0x14012fc00));
    // The TradeWork vtable is built and registered by tradework.cpp:955
    // (RegisterTradeWorkVtable, wired at address_registry.cpp:1674).
    s_tc_TradeWorkVtable = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161a10));
    s_tcWorkResolved = true;

    // Report the OUTCOME of each dependency, not "resolve ran". A single null here
    // silently turns the toggle into a no-op that still repaints — believable and
    // wrong, the failure mode this subsystem keeps producing.
    fprintf(stderr, "[TradeComp] work deps: findWP=%p findSched=%p workCtor=%p post=%p "
            "cancel=%p setAccept=%p vtable=%p\n",
            (void*)s_tc_FindWorkPlace, (void*)s_tc_FindWorkSched, (void*)s_tc_WorkCtor25,
            (void*)s_tc_PostWork, (void*)s_tc_CancelWork, (void*)s_tc_SetAccepting,
            (void*)s_tc_TradeWorkVtable);
    fflush(stderr);
}

// TradeComponent::Slot13 (vtable index 13, 0x14005c140) — work-state query.
//   return -(WPC[0x50] != 0) & 2 | 1   =>  3 when accepting, 1 when not.
uint8_t TradeComponent_Slot13(TradeComponent* selfP) {
    EnsureTradeWorkHelpers();
    uintptr_t self = (uintptr_t)selfP;
    if (!s_tc_FindWorkPlace) return 1;

    uint64_t wpc[2] = {0, 0};
    s_tc_FindWorkPlace(*(int64_t*)(self + 0x10), &wpc);
    if (!wpc[0]) return 1;

    return (*(char*)(wpc[0] + 0x50) != 0) ? 3 : 1;
}

// TradeComponent::Slot14 (vtable index 14, 0x14005c000) — work-state toggle.
// Reads the workplace's CURRENT accepting flag (+0x50) and inverts it:
//   currently OFF -> allocate a TradeWork, post it, commit accepting = 1
//   currently ON  -> cancel the posted work,          commit accepting = 0
// TradeComponent+0x70 caches the new state for the panel.
void TradeComponent_Slot14(TradeComponent* selfP) {
    EnsureTradeWorkHelpers();
    uintptr_t self = (uintptr_t)selfP;
    if (!s_tc_FindWorkPlace || !s_tc_FindWorkSched || !s_tc_SetAccepting) return;

    int64_t sched = 0;
    s_tc_FindWorkSched(*(int64_t*)(self + 0x10), &sched);

    int64_t wpc = 0;
    s_tc_FindWorkPlace(*(int64_t*)(self + 0x10), &wpc);

    if (!wpc) return;

    // enable == "the workplace is NOT currently accepting", i.e. this is a toggle.
    bool enable = (*(char*)(wpc + 0x50) == '\0');
    *(bool*)(self + 0x70) = enable;

    char newState;
    if (enable) {
        void* work = nullptr;
        // This allocation deliberately mirrors retail rather than going through
        // TradeWork_Factory, but TradeWork's file-static function pointers are resolved
        // ONLY on the factory path (TradeWork_Factory / TradeWork_CreateInPlace). Posting
        // a hand-built work therefore handed the scheduler an object whose helpers were
        // all still null, and WorkScheduler_Insert -> TradeWork::GetTarget faulted on the
        // first one it used. Resolve explicitly here, exactly as the factory would.
        // See reference_creatework_must_call_work_resolvefuncs.
        TradeWork_ResolveFuncs();
        if (fn_MemAlloc && g_pMemAllocSingleton && s_tc_WorkCtor25 && s_tc_TradeWorkVtable) {
            work = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
            if (work) {
                int64_t entityIBP = *(int64_t*)(self + 0x10);
                s_tc_WorkCtor25(work, &entityIBP);
                *(void***)work = s_tc_TradeWorkVtable;
                *(uint64_t*)((uintptr_t)work + 0x48) = 0;
                *(uint32_t*)((uintptr_t)work + 0x50) = 0;
            }
        }
        uint64_t workIBP[1] = {0};
        if (fn_InternalBasePtr_Ctor) new (workIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge
        if (s_tc_PostWork) s_tc_PostWork(sched, 1, '\0', workIBP);
        newState = '\x01';
    } else {
        if (s_tc_CancelWork) s_tc_CancelWork(sched);
        newState = '\0';
    }

    s_tc_SetAccepting(wpc, newState);

    fprintf(stderr, "[TradeComp] Slot14 toggle: wpc=%p was=%d now=%d\n",
            (void*)wpc, !enable, (int)newState);
    fflush(stderr);
}

// ============================================================
// Reimpl of FUN_14005c220 (TradeComponent_impl_c220: build the trade name list
//   and item table from the TradeDescription template)
// ============================================================
// Reads the TradeDescription off owner+0x18 (type 0x1401c4bb8). Resizes the
// trade-name array (+0x30) to desc+0x50 and fills each with a random merchant
// name from FUN_1400a4c60; resizes the item table (+0x40) to desc+0x40 and copies
// resource ids from desc+0x38 (0x18 stride -> 0xc stride).
typedef void (*fn_TC_ResizeNames_t)(int64_t* field, int count);   // 0x14000e760
typedef void (*fn_TC_ResizeItems_t)(int64_t* field, int count);   // 0x14000e8d0
typedef void* (*fn_TC_GenName_t)(void* outStr, void* random, char which);  // 0x1400a4c60
typedef void (*fn_TC_StrAssign_t)(void* dst, void* src);          // slot 0x1401514c0
typedef void (*fn_TC_StrDtor_t)(void* str);                       // slot 0x140152af0
static fn_TC_ResizeNames_t s_tc_ResizeNames = nullptr;
static fn_TC_ResizeItems_t s_tc_ResizeItems = nullptr;
static fn_TC_GenName_t    s_tc_GenName = nullptr;
static fn_TC_StrAssign_t  s_tc_StrAssign = nullptr;
static fn_TC_StrDtor_t    s_tc_StrDtor = nullptr;
static uint32_t*          s_tc_pTId_TradeDesc = nullptr;  // 0x1401c4bb8
static bool s_tcSetupResolved = false;
static void EnsureTradeSetupHelpers() {
    if (s_tcSetupResolved) return;
    s_tc_ResizeNames = reinterpret_cast<fn_TC_ResizeNames_t>(ResolveAddr(Module::Exe, 0x14000e760));
    s_tc_ResizeItems = reinterpret_cast<fn_TC_ResizeItems_t>(ResolveAddr(Module::Exe, 0x14000e8d0));
    s_tc_GenName     = reinterpret_cast<fn_TC_GenName_t>(ResolveAddr(Module::Exe, 0x1400a4c60));
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x1401514c0);
    if (slot) s_tc_StrAssign = *reinterpret_cast<fn_TC_StrAssign_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152af0);
    if (slot) s_tc_StrDtor = *reinterpret_cast<fn_TC_StrDtor_t*>(slot);
    s_tc_pTId_TradeDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4bb8));
    s_tcSetupResolved = true;
}

void TradeComponent_Setup(void* selfP) {
    EnsureTradeSetupHelpers();
    uintptr_t self = (uintptr_t)selfP;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    void* descObj = *(void**)(self + 0x18);
    void* desc = (fn_IsDerivedFrom && s_tc_pTId_TradeDesc &&
                  fn_IsDerivedFrom(descObj, *s_tc_pTId_TradeDesc)) ? descObj : nullptr;

    // Resize the name array (+0x30) to desc+0x50
    if (s_tc_ResizeNames)
        s_tc_ResizeNames((int64_t*)(self + 0x30), *(int*)((uintptr_t)desc + 0x50));

    // Fill each name slot with a random merchant name
    int nameCount = *(int*)(self + 0x38);
    void* random = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xd8);
    for (int i = 0; i < nameCount; i++) {
        uint32_t r = reinterpret_cast<System::Random*>(random)->Rand(2u);   // B.3.4b bridge
        uint8_t strBuf[8] = {0};
        if (s_tc_GenName) s_tc_GenName(strBuf, random, (char)(r == 0));
        void* dst = (void*)((int64_t)i * 0x20 + *(int64_t*)(self + 0x30));
        if (s_tc_StrAssign) s_tc_StrAssign(dst, strBuf);
        if (s_tc_StrDtor) s_tc_StrDtor(strBuf);
    }

    // Report the OUTCOME, not the call. A populated +0x30 is the whole point of
    // this function — TradeUI_RefreshBuyTable indexes it — and "Setup ran" is not
    // evidence it filled anything: desc null, or a desc+0x50 of 0, both leave the
    // array NULL and read identically from a bare entry trace.
    fprintf(stderr, "[TradeComp] Setup: desc=%p names=%d array=%p\n",
            desc, nameCount, (void*)*(int64_t*)(self + 0x30));
    // ...and the names themselves. A populated array is NOT evidence the names are
    // real: while 0x1400a4c60 was unregistered, s_tc_GenName was null, strBuf stayed
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
    if (s_tc_ResizeItems)
        s_tc_ResizeItems((int64_t*)(self + 0x40), *(int*)((uintptr_t)desc + 0x40));

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
    if (fn_MemFreeStatic) fn_MemFreeStatic((void*)dst[0]);
    dst[1] = 0;
    dst[0] = 0;
    int count = (int)src[1];
    *(int*)(dst + 1) = count;
    if (count > 0) {
        *(int*)((int64_t)dst + 0xc) = count;
        void* buf = (fn_MemAlloc && g_pMemAllocSingleton)
            ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 8, 8) : nullptr;
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
        if (fn_MemFreeStatic) fn_MemFreeStatic((void*)p[0]);
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
        void* buf = (fn_MemAlloc && g_pMemAllocSingleton)
            ? fn_MemAlloc(g_pMemAllocSingleton,
                          (uint64_t)(*(int*)((int64_t)field + 0xc)) * 0x20, 8) : nullptr;
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
        if (fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
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
        void* newBuf = (fn_MemAlloc && g_pMemAllocSingleton)
            ? fn_MemAlloc(g_pMemAllocSingleton,
                          (uint64_t)(*(int*)((int64_t)field + 0xc)) * 0xc, 4)
            : nullptr;
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
        if (fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void TradeComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[TradeComponent] Funcs resolved: RegisterEntity=%p "
           "TradeSetup=%p\n",
           (void*)s_fn_RegisterEntity,
           (void*)s_fn_TradeSetup);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0xa8 (TradeComponent_Factory MemAlloc), flags 0x83 (Ctor 0x14005c430:
// Component base sets +0x20=0x80, leaf OR 0x3 => 0x83).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// ── Promotion from inert, 2026-07-31 ─────────────────────────────────────────
// TradeComponent was COMPONENT_INERT_FACTORY, which zero-fills the body and
// leaves every vtable slot a no-op stub. That produced two faults at once in
// TradeUI_RefreshBuyTable (tradeui.cpp:849):
//
//   * +0x30 (merchant-name array) stayed NULL, because slot 6 was a stub and
//     TradeComponent_Draw — which is what calls TradeComponent_Setup, the only
//     thing that ever allocates that array — never ran.
//   * +0x2c (selected merchant index) read 0, and 0 passes the `>= 0` guard at
//     tradeui.cpp:848, so the panel indexed a NULL array.
//
// The real ctor (0x14005c430, Ghidra-verified) writes `+0x2c = 0xffffffff`.
// -1 IS the "no merchant selected" sentinel — the `>= 0` test at tradeui.cpp:848
// and the `comboEnabled` at :858 both key off it. A zero-filled body turns that
// sentinel into a valid-looking index 0: exactly the inert-sentinel trap.
//
// Only +0x2c is applied here. Every other field the ctor writes is already 0 from
// the zero-fill, and +0x20's `| 3` is covered by the Flags argument (0x83).
// DELIBERATELY NOT applied: +0x88 (the controller sub-object's own vtable) and
// +0xa0 (its self back-pointer). Those two go together, and publishing a
// controller vtable we have not ported would turn today's null-guarded skips into
// calls through a half-built object. They stay inert until someone ports the
// controller.
static void TradeComponent_CtorInit(void* self) {
    if (!self) return;
    *(int32_t*)((uintptr_t)self + 0x2c) = -1;   // ctor 0x14005c430: no merchant selected
}

static uint32_t TradeComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TradeComponent");
    return cached;
}
static void* TradeComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

// Ghidra calls 0x14005c1d0 "Draw"; it is the OnAdded slot (index 6), dispatched by
// ComponentEntity_OnAddedPass at register time because Component+0x20 bit 0 is set
// (Flags 0x83). Same mislabel family as the OnCreate/OnRemove pair.
static void TradeComponent_OnAddedVSlot(void* s) {
    TradeComponent_Draw(reinterpret_cast<TradeComponent*>(s));
}

static uint8_t TradeComponent_Slot13VSlot(void* s) {
    return TradeComponent_Slot13(reinterpret_cast<TradeComponent*>(s));
}
static void TradeComponent_Slot14VSlot(void* s) {
    TradeComponent_Slot14(reinterpret_cast<TradeComponent*>(s));
}

// ── Controller state machine (tradecomponent_states.cpp) ─────────────────────────
// The controller sub-object the note above left inert. Retail's TradeComponent vtable
// 0x14015acd0 holds slot 16 = 0x1400026c0 (`MOV EAX,2; RET`) and slot 17 = 0x14000dba0
// (`LEA RAX,[RCX+0x88]; RET`), so the node lives at +0x88, its back-pointer at +0xa0,
// and the machine starts in state 2. Its own opt-in flag rather than riding
// BANISHED_LIVE_TRADE_COMPONENT: the panel gate is verified, this is not.
extern "C" void** TradeCtrl_NodeVtable();
extern "C" void   TradeCtrl_PopulateTable();

static int   TradeComponent_GetDefaultState(void*) { return 2; }
static void* TradeComponent_CreateState(void* self) {
    return reinterpret_cast<uint8_t*>(self) + 0x88;
}
static void* TradeComponent_LiveGetTypeInfo(void*) {
    static void* cached = nullptr;
    if (!cached) cached = ::Object::GetTypeInfoByName("TradeComponent");
    return cached;
}

// Wire the node ON THE INSTANCE. This must happen after MakeInertTyped/InitInertTyped
// (FillTypedVtable rewrites the vtable slots, erasing anything written earlier) —
// see reference_live_vtable_slot_must_be_wired_in_installlive.
static void TradeComponent_InstallController(void* mem) {
    TradeComponent_LiveVtable[1]  = reinterpret_cast<void*>(&TradeComponent_LiveGetTypeInfo);
    TradeComponent_LiveVtable[16] = reinterpret_cast<void*>(&TradeComponent_GetDefaultState);
    TradeComponent_LiveVtable[17] = reinterpret_cast<void*>(&TradeComponent_CreateState);
    TradeCtrl_PopulateTable();
    if (!mem) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(mem);
    *reinterpret_cast<void**>(p + 0x88) = reinterpret_cast<void*>(TradeCtrl_NodeVtable());
    *reinterpret_cast<void**>(p + 0xa0) = mem;   // node+0x18 = owning component
}

extern "C" void TradeComponent_Serialize(TradeComponent* self, void* stream);
static void TradeComponent_SerializeVSlot(void* s, void* stream) {
    TradeComponent_Serialize(reinterpret_cast<TradeComponent*>(s), stream);
}
static void TradeComponent_FinishLiveVtable() {
    // Slot 3 (stream Serialize, retail 0x14005bd50, tradecomponent_serialize.cpp)
    // wired UNCONDITIONALLY — save/load wave 2026-08-28. Slot-3 wiring is
    // symmetric (the same vtable serves save and load), so wiring cannot desync
    // the stream; leaving it base-only silently reset the trading post's
    // merchant table / item lists on every load.
    TradeComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&TradeComponent_SerializeVSlot);
    TradeComponent_LiveVtable[6] =
        reinterpret_cast<void*>(&TradeComponent_OnAddedVSlot);
    // 13 and 14 go in together — see the note on the pair above. A live query
    // over a stub toggle reports a state nothing can change.
    TradeComponent_LiveVtable[13] =
        reinterpret_cast<void*>(&TradeComponent_Slot13VSlot);
    TradeComponent_LiveVtable[14] =
        reinterpret_cast<void*>(&TradeComponent_Slot14VSlot);
}

extern "C" void* TradeComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0xa8, 0x83,
        TradeComponent_LiveVtable,
        reinterpret_cast<void*>(&TradeComponent_LiveGetTypeId));
    TradeComponent_FinishLiveVtable();
    TradeComponent_CtorInit(p);
    TradeComponent_InstallController(p);
    return p;
}

static void* TradeComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0xa8, 0x83,
        TradeComponent_LiveVtable,
        reinterpret_cast<void*>(&TradeComponent_LiveGetTypeId));
    TradeComponent_FinishLiveVtable();
    TradeComponent_CtorInit(mem);
    TradeComponent_InstallController(mem);
    return mem;
}

REGISTER_TYPE(TradeComponent, "ControllerComponent",
              &TradeComponent_Factory, &TradeComponent_CreateInPlace);
