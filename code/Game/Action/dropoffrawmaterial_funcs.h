#pragma once
// DropOffRawMaterial function declarations — clean game logic layer.
// These functions are called from engine/reimpl/dropoffrawmaterial.cpp hook wrappers.
// They must NOT depend on hooks.h — they are portable for standalone exe.

#include <cstdint>

struct DropOffRawMaterial;

// DropOffRawMaterial::ScalarDtor — set vtables, call ~Internal, optionally free
void* DropOffRawMaterial_ScalarDtor_Impl(DropOffRawMaterial* self, uint32_t flags);

// DropOffRawMaterial::BaseDtor — returns TypeInfo (slot 2)
uint32_t DropOffRawMaterial_BaseDtor_Impl();

// DropOffRawMaterial::Serialize — serialize status, entity refs, resource type, flag, count
void DropOffRawMaterial_Serialize_Impl(DropOffRawMaterial* self, void* stream);

// DropOffRawMaterial::AbstractImpl — complex resource transfer logic
// Returns int per bug class 7.
int DropOffRawMaterial_AbstractImpl_Impl(DropOffRawMaterial* self, void* actionStack);

// DropOffRawMaterial::CheckEntity — JMP to Entity_impl_7570 (drops off all inventory)
// NOT HOOKED — tiny (JMP thunk, 5 bytes). Standalone exe only.
void DropOffRawMaterial_CheckEntity_Impl(DropOffRawMaterial* self);

// DropOffRawMaterial::GetTarget — checks +0x38 and +0x30->+0x10
bool DropOffRawMaterial_GetTarget_Impl(DropOffRawMaterial* self, void* param);

// Resolution helper
bool DropOffRawMaterial_ResolveFuncs();
