#pragma once
// SendComponentMessage function declarations — clean game logic layer.
// These functions are called from engine/reimpl/sendcomponentmessage.cpp hook wrappers.

#include <cstdint>

struct SendComponentMessage;

// SendComponentMessage::ScalarDtor — set vtables, call ~Internal, optionally free
void* SendComponentMessage_ScalarDtor_Impl(SendComponentMessage* self, uint32_t flags);

// SendComponentMessage::Serialize — serialize status, two entity refs, type, msgId, params
void SendComponentMessage_Serialize_Impl(SendComponentMessage* self, void* stream);

// SendComponentMessage::AbstractImpl — find component by type, send message via vtable+0x78
// Returns int (NOT bool) per bug class 7.
int SendComponentMessage_AbstractImpl_Impl(SendComponentMessage* self, void* actionStack);

// SendComponentMessage::GetTarget — returns true if param matches +0x20 or +0x28
bool SendComponentMessage_GetTarget_Impl(SendComponentMessage* self, void* param);

// Resolution helper
bool SendComponentMessage_ResolveFuncs();
