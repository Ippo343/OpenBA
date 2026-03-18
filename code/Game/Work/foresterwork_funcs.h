#pragma once
// ForesterWork function declarations — clean game logic layer.
// Called from engine/reimpl/foresterwork.cpp hook wrappers.
// Must NOT depend on hooks.h — portable for standalone exe.

#include <cstdint>

struct ForesterWork;

// ForesterWork::ScalarDtor — reset vtable chain, free BST nodes, call ~Internal, optionally free
void* ForesterWork_ScalarDtor_Impl(ForesterWork* self, uint32_t flags);

// ForesterWork::Serialize — Work::Serialize + AutoPickupWork block + BST serialize + int field
void ForesterWork_Serialize_Impl(ForesterWork* self, void* stream);

// ForesterWork::Slot4 — main work-assignment logic (599 instructions).
// Finds nearby trees, checks terrain, assigns forester to cut/plant tasks.
// Returns: 0 = idle, 1 = not ready, 2 = action pushed.
// Uses int return type (NOT bool) per bug class 7.
int ForesterWork_Slot4_Impl(ForesterWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

// ForesterWork::Slot6 — draw helper: removes entity from BST by key
void ForesterWork_Slot6_Impl(ForesterWork* self, void* entityIBP);

// ForesterWork::Slot8 — CancelAll: loops tree rightmost, cancels each work item
void ForesterWork_Slot8_Impl(ForesterWork* self);

// ForesterWork::Slot9 — RemoveInvalid: validates tree entries, removes dead ones.
// Returns true if all entries are valid (tree is empty after removing invalids).
int ForesterWork_Slot9_Impl(ForesterWork* self, void* param2);

// ForesterWork::Slot10 — GetWorkEntity: hidden return ptr pattern (RCX=this, RDX=outPtr).
// Returns entity reference from the work's entity chain.
void* ForesterWork_Slot10_Impl(ForesterWork* self, void* outPtr);

// Resolve all ForesterWork-specific function pointers. Call once at init.
bool ForesterWork_ResolveFuncs();
