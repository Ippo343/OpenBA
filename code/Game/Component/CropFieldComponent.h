#pragma once
// ============================================================================
// code/Game/Component/CropFieldComponent.h — ENDGAME B.3.8 block MI-B
//
// `CropFieldComponent` (a crop field: its slot grid and the plant/tend/harvest
// cycle) as a real C++ class with a compiler-generated vtable and its state
// controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6). The engine/types POD is
// a forwarder.
//
// RETAIL (B38_SLOTS.md §2 / §6, bytes read 2026-09-26):
//   reflected "CropFieldComponent", parent "ControllerComponent", ti 0x1401c7510
//   create 0x140032ed0: MemAlloc(0x68, 8) + ctor 0x14011afd0
//   ctor 0x14011afd0: Component ctor; member at +0x28 (0x14015f070, 0x140162e70),
//     back-pointer +0x40; `MOV word [RBX+0x48],0x100` (+0x48 = 0, +0x49 = 1);
//     +0x50 = +0x58 = +0x60 = 0; `_flags |= 0x80`.
//   dtor 0x14011af60: MemFree(+0x58), +0x58 = +0x60 = 0 (the slot grid), restamps,
//     ~Internal. [0] 0x1400330a0 = dtor + MemFree on &1.
//   table 0x140162ec0, 18 slots: [3] 0x14011ae70 Persist, [4] 0x14011ae60 Startup
//     (`MOV qword [RCX+0x50],0`), [13] 0x14011ada0 CommandMask, [14] 0x14011acc0
//     Command, [15] 0x14011ab50 OnMessage, [16] 0x1400026c0 `MOV EAX,2`, [17]
//     0x140004080 `LEA RAX,[RCX+0x28]`; every other slot = ControllerComponent's.
//   The tree's slots 18-21 (ControllerLive_ScalarDeletingDtor and the description's
//     type info / id / version) were CropFieldDescription's table 0x140162f50 read
//     past this table's end (B38 D3.5): gone.
//   member table 0x140162e70: the table-driven controller over the class's state
//     table (ControllerLive::Dispatcher<CropFieldCtrlTag>).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"
#include "object/Internal.h"   // Object::InternalPtr<T>

class ComponentDescription;       // Component/ComponentDescription.h
class ComponentEntity;            // Entity/ComponentEntity.h
class NaturalResourceComponent;   // Component/NaturalResourceComponent.h
namespace IO { class Serializer; }

struct CropFieldCtrlTag { using Owner = class CropFieldComponent; };   // the handlers' receiver (controller_state_table.h)

// One cell of the field's slot grid (0x20 bytes). Persist 0x14011ae70's element
// helper 0x1400336c0 serializes int, int, InternalBasePtr (IsDerivedFrom
// ComponentEntity 0x1401c5d70), InternalBasePtr (IsDerivedFrom
// NaturalResourceComponent 0x1401c7b30), bool.
struct CropSlot {
    int32_t cell;                                            // +0x00 map cell index
    int32_t state;                                           // +0x04 0 empty, 1 growing, 2 done
    Object::InternalPtr<ComponentEntity>          entity;    // +0x08 the planted entity
    Object::InternalPtr<NaturalResourceComponent> resource;  // +0x10 its natural resource
    bool    plantHere;                                       // +0x18 this cell gets a crop
    uint8_t _pad19[7];                                       // +0x19
};
static_assert(sizeof(CropSlot) == 0x20, "CropSlot stride 0x20 (0x1400335b0 MemAlloc(cap*0x20))");
static_assert(offsetof(CropSlot, state)     == 0x04, "CropSlot +0x04");
static_assert(offsetof(CropSlot, entity)    == 0x08, "CropSlot +0x08");
static_assert(offsetof(CropSlot, resource)  == 0x10, "CropSlot +0x10");
static_assert(offsetof(CropSlot, plantHere) == 0x18, "CropSlot +0x18");

// The slot grid's Collection::Array<CropSlot> header (data / count / capacity).
struct CropSlotArray {
    CropSlot* data;       // +0x00
    int32_t   count;      // +0x08
    int32_t   capacity;   // +0x0C
};
static_assert(sizeof(CropSlotArray) == 0x10, "Array header");

class CropFieldComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(CropFieldComponent)

public:
    using StateController = ::ControllerLive::TableController<CropFieldCtrlTag>;

    CropFieldComponent();                                                // 0x14011afd0

    // 0x14011af60
    ~CropFieldComponent() override;                                     // [0]  0x1400330a0
    void          Persist(void* stream) override;                       // [3]  0x14011ae70
    void          Startup() override;                                   // [4]  0x14011ae60
    uint32_t      CommandMask(int command) override;                    // [13] 0x14011ada0
    void          Command(int command) override;                        // [14] 0x14011acc0
    void          OnMessage(int message, void* data,
                            uint32_t arg0, uint32_t arg1) override;     // [15] 0x14011ab50
    int         GetDefaultState() const override { return 2; }          // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }       // [17] 0x140004080

    // ---- non-virtual members (retail addresses; RCX = this) ----
    void  ValidateEntityRefs();                                         // 0x14011aa20
    float Maturity();                                                   // 0x14011b410
    void  PushAccepting();   // the WorkPlaceComponent accepting push 0x14011a920 / 0x14011a7b0 share
    // The state handlers of the controller's table (RCX = [node+0x18] = this).
    char  State2Enter();                                                // 0x14011a920
    char  Event3Plant(void* sm);                                        // 0x14011a7b0
    char  State2PerFrame(void* sm);                                     // 0x14011a860
    char  State9Enter();                                                // 0x14011a310
    char  State9PerFrame(void* sm);                                     // 0x14011a1e0
    char  State10Enter();                                               // 0x14011a100
    char  State10Update();                                              // 0x140119f70
    char  State10PerFrame(void* sm);                                    // 0x140119fd0
    char  State11Enter();                                               // 0x140119e60
    char  State11PerFrame(void* sm);                                    // 0x140119d80

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    // Types from Persist 0x14011ae70: +0x48 / +0x49 Serialize(bool&), +0x50
    // ExternalPtr<ComponentDescription> (0x140002340), the grid count Serialize(int&).
    StateController _controller;   // +0x28 the embedded controller (D6)
    bool     workEnabled;          // +0x48 the panel's master switch (Command 2 flips it)
    bool     hasCrop;              // +0x49 a crop type is set (state 9 enter; ctor 1)
    uint8_t  _pad4A[6];            // +0x4a
    ComponentDescription* cropDescription; // +0x50 ExternalPtr<ComponentDescription> (Persist 0x140002340):
                                   //       the crop the field is set to grow (Startup clears it)
    CropSlotArray slots;           // +0x58 the slot grid (Array<CropSlot>)
};

static_assert(sizeof(CropFieldComponent) == 0x68, "CropFieldComponent is 0x68 (create 0x140032ed0)");
static_assert(alignof(CropFieldComponent) == 8, "CropFieldComponent alignment");
static_assert(offsetof(CropFieldComponent, _controller) == 0x28,
              "CropFieldComponent's controller member at +0x28 (slot 17 0x140004080)");
static_assert(offsetof(CropFieldComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x14011b02c)");
static_assert(offsetof(CropFieldComponent, workEnabled) == 0x48, "CropFieldComponent +0x48 (ctor 0x14011b030)");
static_assert(offsetof(CropFieldComponent, hasCrop) == 0x49, "CropFieldComponent +0x49");
static_assert(offsetof(CropFieldComponent, cropDescription) == 0x50, "CropFieldComponent +0x50");
static_assert(offsetof(CropFieldComponent, slots) == 0x58, "CropFieldComponent +0x58");
static_assert(offsetof(CropFieldComponent, slots) + offsetof(CropSlotArray, count) == 0x60,
              "CropFieldComponent +0x60 the grid count");
