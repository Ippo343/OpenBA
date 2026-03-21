#pragma once
// CreateRawMaterial function declarations -- clean game logic layer.
// Called from engine/reimpl/createrawmaterial.cpp hook wrappers.
// Must NOT depend on hooks.h -- portable for standalone exe.

#include <cstdint>

struct CreateRawMaterial;

// CreateRawMaterial::ScalarDtor -- set vtables, call ~Internal, optionally free
void* CreateRawMaterial_ScalarDtor_Impl(CreateRawMaterial* self, uint32_t flags);

// CreateRawMaterial::Serialize -- versioned serialization with 3 version gates
void CreateRawMaterial_Serialize_Impl(CreateRawMaterial* self, void* stream);

// CreateRawMaterial::AbstractImpl -- create raw material entities from a resource building
// Returns int (not bool) per bug class 7.
int CreateRawMaterial_AbstractImpl_Impl(CreateRawMaterial* self, void* actionStack);

// CreateRawMaterial::GetTarget -- checks +0x28->+0x10, +0x30, +0x38
int CreateRawMaterial_GetTarget_Impl(CreateRawMaterial* self, void* param);

// Resolution helper
bool CreateRawMaterial_ResolveFuncs();
