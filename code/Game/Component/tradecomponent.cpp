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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140002cc0 — register entity with world entity list
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

// FUN_140003110 — add entity to world pending list
typedef void (*fn_AddToPending_t)(void* listPtr, int newCount, void* entityPtr);
static fn_AddToPending_t s_fn_AddToPending = nullptr;

// FUN_14005c220 — trade component setup (called after adding to pending list)
typedef void (*fn_TradeSetup_t)(void* self);
static fn_TradeSetup_t s_fn_TradeSetup = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_fn_AddToPending = reinterpret_cast<fn_AddToPending_t>(
        ResolveAddr(Module::Exe, 0x140003110));

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

    if (!s_fn_AddToPending || !s_fn_TradeSetup ||
        !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Add to trade pending list at worldMgr+0x30
    int currentCount = *(int*)(worldMgr + 0x38);
    s_fn_AddToPending((void*)(worldMgr + 0x30), currentCount + 1, &entityPtr);

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
// Reimpl of FUN_14005c220 (TradeComponent_impl_c220: build the trade name list
//   and item table from the TradeDescription template)
// ============================================================
// Reads the TradeDescription off owner+0x18 (type 0x1401c4bb8). Resizes the
// trade-name array (+0x30) to desc+0x50 and fills each with a random merchant
// name from FUN_1400a4c60; resizes the item table (+0x40) to desc+0x40 and copies
// resource ids from desc+0x38 (0x18 stride -> 0xc stride).
typedef void (*fn_TC_ResizeNames_t)(int64_t* field, int count);   // 0x14000e760
typedef void (*fn_TC_ResizeItems_t)(int64_t* field, int count);   // 0x14000e8d0
typedef uint32_t (*fn_TC_Rand_t)(void* random, int n);            // slot 0x1401512e8
typedef void* (*fn_TC_GenName_t)(void* outStr, void* random, char which);  // 0x1400a4c60
typedef void (*fn_TC_StrAssign_t)(void* dst, void* src);          // slot 0x1401514c0
typedef void (*fn_TC_StrDtor_t)(void* str);                       // slot 0x140152af0
static fn_TC_ResizeNames_t s_tc_ResizeNames = nullptr;
static fn_TC_ResizeItems_t s_tc_ResizeItems = nullptr;
static fn_TC_Rand_t       s_tc_Rand = nullptr;
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
    slot = ResolveAddr(Module::Exe, 0x1401512e8);
    if (slot) s_tc_Rand = *reinterpret_cast<fn_TC_Rand_t*>(slot);
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
        uint32_t r = s_tc_Rand ? s_tc_Rand(random, 2) : 0;
        uint8_t strBuf[8] = {0};
        if (s_tc_GenName) s_tc_GenName(strBuf, random, (char)(r == 0));
        void* dst = (void*)((int64_t)i * 0x20 + *(int64_t*)(self + 0x30));
        if (s_tc_StrAssign) s_tc_StrAssign(dst, strBuf);
        if (s_tc_StrDtor) s_tc_StrDtor(strBuf);
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
        if (fn_String_Dtor) fn_String_Dtor((void*)(p - 2));
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
                if (fn_String_Ctor_Copy) fn_String_Ctor_Copy(dstElem, srcElem);   // String at +0x00
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
    printf("[TradeComponent] Funcs resolved: RegisterEntity=%p AddToPending=%p "
           "TradeSetup=%p\n",
           (void*)s_fn_RegisterEntity, (void*)s_fn_AddToPending,
           (void*)s_fn_TradeSetup);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0xa8 (TradeComponent_Factory MemAlloc), flags 0x83 (Ctor 0x14005c430:
// Component base sets +0x20=0x80, leaf OR 0x3 => 0x83).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(TradeComponent, 0xa8, 0x83)
REGISTER_TYPE(TradeComponent, "ControllerComponent",
              &TradeComponent_Factory, &TradeComponent_CreateInPlace);
