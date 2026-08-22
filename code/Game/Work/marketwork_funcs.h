#pragma once
// Function declarations for deeply reimplemented MarketWork methods.
// Called from engine/reimpl/marketwork.cpp wrappers.

#include "MarketWork.h"
#include <cstdint>

// ============================================================
// MarketWork -- Market trading work action (0x58 bytes)
// Inherits: Object::Internal -> Work (0x48) -> MarketWork
// Vtable: 0x140161710 (12 slots)
//
// CRITICAL: MarketWork uses a DIFFERENT BST node layout than
// HuntWork/PastureWork/HerbalistWork:
//   +0x00: key (InternalBasePtr)
//   +0x08: data pointer
//   +0x10: data2
//   +0x18: parent
//   +0x20: left child
//   +0x28: right child
//   Node size = 0x38 bytes
// vs HerbalistWork BST nodes which use +0x08=parent, +0x10=left, +0x18=right
// ============================================================

// Slot4 (vtable +0x20) is MarketWork_Execute_Impl -- see its declaration below.
// (The old note here said it "calls original for now"; there is no original to
// call any more, the standalone exe is the only build.)

// Slot6 (vtable +0x30): Add entity to BST. DISABLED hook (relative CALL).
// Calls MarketWork_sub_aaa0(self+0x48, param2) then returns false.
bool MarketWork_Slot6_Impl(MarketWork* self, void* param2);

// Slot7 (vtable +0x38): Not a function in Ghidra, skip.
// Too small / not hookable. Implemented for standalone exe only.
// Returns int (bug class 7 -- action executor reads EAX).
int MarketWork_Slot7_Impl(MarketWork* self, void* param2);

// Slot8 (vtable +0x40): CancelAll. DISABLED hook (short Jcc).
// Loops through BST at +0x48 while count (+0x50) > 0, cancels each work item.
// Uses DIFFERENT BST offsets: left child at +0x20 (not +0x10).
// Returns int (bug class 7 -- action executor reads EAX).
int MarketWork_Slot8_Impl(MarketWork* self);

// Slot9 (vtable +0x48): RemoveInvalid. DISABLED hook (MOV RAX,RSP).
// Iterates BST, validates entries, removes invalid ones.
// Uses DIFFERENT BST offsets: left=+0x20, right=+0x28, parent=+0x18.
bool MarketWork_Slot9_Impl(MarketWork* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity. ENABLED hook.
// Returns entity from this->_field10's sub-object at +0x48.
void* MarketWork_Slot10_Impl(MarketWork* self, void* outPtr);

// Serialize (vtable +0x18): Work::Serialize + versioned BST serialization.
// ENABLED hook. Version=1 serializer with v0/v1 migration path.
void MarketWork_Serialize_Impl(MarketWork* self, void* stream);

// Execute (vtable index 4, +0x20): retail 0x1400eff80 -- the market hauling /
// demand planner. Ghidra misnames it "MarketWork_Serialize"; it is not
// serialization (slot 3, 0x1400f1d70, is the real Serialize).
//
// Signature recovered from the ONLY call site, the citizen idle claim-walk at
// retail 0x1400f7b60 (Helper_sub_7b60, citizenai_housing_idle.cpp:506, vcall at
// :527) -- THREE args, and the return is a TRI-STATE:
//     2 = claimed, the caller's walk STOPS
//     1 = examined but declined; the caller stamps work+0x2c with sched+8
//     0 = keep walking, no stamp
// Slot 9 (Validate) is dispatched immediately before this, unconditionally.
// param_3 is sched+0x130; retail slot 4 never reads it.
//
// STAGE 1 ONLY so far: retail 0x1400eff80..0x1400f021f (guards, the two cheap
// "return 1" rejections and the "already carrying -> return 2" arm). Everything
// from 0x1400f021f on returns 0.
int MarketWork_Execute_Impl(MarketWork* self, void* citizenHolder, void* ctx);

// ScalarDtor body: Sets vtables down chain, frees tree nodes, calls base dtor.
// Uses DIFFERENT tree cleanup (FUN_14002af40) -- frees node data at +0x08.
void MarketWork_DtorBody(MarketWork* self);

// ScalarDtor (0x14002a910): Calls dtor body, conditional free.
void* MarketWork_ScalarDtor_Impl(MarketWork* self, uint32_t flags);

// Slot11 (vtable +0x58): 9 bytes, returns (this->_field50 > 0). Too small to hook.
bool MarketWork_Slot11_Impl(MarketWork* self);

// Resolve all function pointers needed by MarketWork.
// Called once during hook installation.
bool MarketWork_ResolveFuncs();
