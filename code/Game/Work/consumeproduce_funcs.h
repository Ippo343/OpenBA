#pragma once
// ConsumeProduce function declarations -- clean game logic layer.
// Called from engine/reimpl/consumeproduce.cpp hook wrappers.
// Must NOT depend on hooks.h -- portable for standalone exe.

#include <cstdint>

struct ConsumeProduceAction;

// ConsumeProduce::Ctor -- initialize Work base + ConsumeProduce fields.
// Original: 0x1400e9340
void* ConsumeProduce_Ctor_Impl(ConsumeProduceAction* self);

// ConsumeProduce::ScalarDtor -- reset vtable chain, destroy base, optionally free.
// Original: 0x14002a190 (scalar), 0x1400e9190 (base dtor body)
void* ConsumeProduce_ScalarDtor_Impl(ConsumeProduceAction* self, uint32_t flags);

// ConsumeProduce::Serialize -- Work::Serialize + IBP array + int array + fields.
// Original: 0x1400e9020
void ConsumeProduce_Serialize_Impl(ConsumeProduceAction* self, void* stream);

// ConsumeProduce::Execute (vtable slot 4, 0x1400e7840):
// Complex production work logic -- pathfinding, storage queries, citizen AI.
// Returns: 1 = retry/not ready, 2 = action pushed.
// Uses int return type (NOT bool) per bug class 7.
int ConsumeProduce_Execute_Impl(ConsumeProduceAction* self, void* entityIBP);

// ConsumeProduce::OnComplete (vtable slot 6) -- store null IBP, update slot state.
// Returns int per bug class 7. Original: 0x1400e77c0
int ConsumeProduce_OnComplete_Impl(ConsumeProduceAction* self, void* param2, void* param3, uint32_t param4);

// ConsumeProduce::OnCancel (vtable slot 7) -- delegates to OnComplete with cleared flag.
// Original: 0x1400e7780
void ConsumeProduce_OnCancel_Impl(ConsumeProduceAction* self, void* param2, void* param3, uint32_t param4);

// ConsumeProduce::Cleanup (vtable slot 8) -- release assigned citizens.
// Original: 0x1400e8f30
void ConsumeProduce_Cleanup_Impl(ConsumeProduceAction* self);

// ConsumeProduce::Validate (vtable slot 9) -- check assigned tasks are still valid.
// Returns int per bug class 7. Original: 0x1400e7530
int ConsumeProduce_Validate_Impl(ConsumeProduceAction* self, void* param2);

// ConsumeProduce::GetTarget (vtable slot 10) -- return target position.
// Hidden return ptr ABI: RCX=self, RDX=retbuf.
// Original: 0x1400e8fb0
void* ConsumeProduce_GetTarget_Impl(ConsumeProduceAction* self, void* retbuf);

// Resolution helper -- resolve all function pointers. Called once.
bool ConsumeProduce_ResolveFuncs();
