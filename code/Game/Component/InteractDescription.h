#pragma once
// ============================================================================
// code/Game/Component/InteractDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "InteractDescription", parent "Description", ti 0x1401c7938
//   create 0x140035200: MemAlloc(0x58, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140035240.
//   table 0x140163930, 10 slots:
//     [0] 0x140035280 scalar deleting dtor   [1] 0x1400351f0 TypeName   [2] 0x1400351e0 Type
//     [3] 0x1401236a0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140123500 Serialize (`MOV AL,1`)
//     [6] 0x140035250 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140035270 GetComponentType: InteractComponent's TypeId
//     [9] 0x140035240 Construct (folded with createInPlace)
//   sizeof 0x58.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class InteractDescription final : public Description {
    DECLARE_EXE_TYPE(InteractDescription)

public:
    InteractDescription();

    ~InteractDescription() override;                                       // [0] 0x140035280
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x1401236a0
    bool              Serialize(void* stream) override;                    // [5] 0x140123500
    Object::External* Reconstruct() override;                              // [6] 0x140035250
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140035270
    virtual Object::External* Construct();                                 // [9] 0x140035240

    // {data, count, cap}: the header of one 0x18-stride node array.
    struct NodeArray {
        void*   data;
        int32_t count;
        int32_t cap;
    };

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _descRef;                             // +0x20 ExternalPtr<Pathfinding::PointList>
    InteractDescription::NodeArray _useNodes;   // +0x28 0x18-stride node array {data, count, cap}
    InteractDescription::NodeArray _buildNodes; // +0x38 idem
    InteractDescription::NodeArray _createNodes;// +0x48 idem
};

static_assert(sizeof(InteractDescription) == 0x58, "InteractDescription is 0x58 (create 0x140035200 MemAlloc(0x58))");
static_assert(alignof(InteractDescription) == 8, "InteractDescription alignment");
static_assert(offsetof(InteractDescription, _descRef) == 0x20, "InteractDescription::_descRef @ +0x20");
static_assert(offsetof(InteractDescription, _useNodes) == 0x28, "InteractDescription::_useNodes @ +0x28");
static_assert(offsetof(InteractDescription, _buildNodes) == 0x38, "InteractDescription::_buildNodes @ +0x38");
static_assert(offsetof(InteractDescription, _createNodes) == 0x48, "InteractDescription::_createNodes @ +0x48");
