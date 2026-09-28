#pragma once
// ============================================================================
// code/Game/Component/CitizenComponent.h — ENDGAME B.3.8 block CIT
//
// `CitizenComponent` (a citizen's full state: health, happiness, inventory,
// housing, work, the age/clothing mesh) as a real C++ class with a
// compiler-generated vtable (B38_RULES.md D1-D4). The engine/types POD is a
// forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CitizenComponent", parent "Component", ti 0x1401c7158
//   create 0x140030110: MemAlloc(0x290, 8) + ctor 0x140114770
//   ctor   0x140114770: Component ctor, stamp 0x140162580, scalars (+0x30 = -1,
//                       +0xa8 = -1, +0x1d8 = -1 (qword), +0x1e0 = 4, the rest 0),
//                       Graphics::MeshInstance::MeshInstance() at +0xb8,
//                       flags |= 0xcb
//   dtor   0x1401146f0: restamp, ~MeshInstance(+0xb8) (import 0x1401514a8),
//                       MemFree(+0x98) + zero +0x98/+0xa0, InternalBasePtr dtor
//                       at +0x28 (import 0x140152af0), stamp 0x140162920,
//                       ~Internal; scalar 0x140030270 = dtor + MemFree on &1
//   table  0x140162580, 16 slots:
//     [0] 0x140030270 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140114160 Persist                [4] 0x140114050 Startup
//     [6] 0x140114000 OnAdded (town list)    [7] 0x140113ca0 OnRemove
//     [9] 0x140112e70 Update                 [11] 0x1401140f0 Slot11 (post-load)
//     [15] 0x140112260 OnMessage (NeedsUpdate; EDX msg, R8 pointer, R9D int)
//     every other slot = Component's
//
// THE +0xB8 MeshInstance: retail constructs and destroys a real
// Graphics::MeshInstance there. The tree has always kept it as zero-filled raw
// bytes (the inert factory memset) that the ported bodies drive by offset; this
// class keeps that (PORT GAP, ctor and dtor — B38 D1/D2), a behaviour change for
// its own titled commit.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class CitizenComponent final : public Component {
    DECLARE_EXE_TYPE(CitizenComponent)

public:
    CitizenComponent();                                           // 0x140114770

    // PORT GAP (retail 0x1401146f0) — B38 D2: retail runs ~MeshInstance(+0xb8),
    // frees the +0x98 array and releases the +0x28 InternalBasePtr; the tree's
    // Inert_ScalarDtor never did.
    ~CitizenComponent() override = default;                       // [0]  0x140030270
    void Persist(void* stream) override;                          // [3]  0x140114160
    void Startup() override;                                      // [4]  0x140114050
    void OnAdded() override;                                      // [6]  0x140114000
    void OnRemove() override;                                     // [7]  0x140113ca0
    void Update(void* time) override;                             // [9]  0x140112e70
    void Slot11() override;                                       // [11] 0x1401140f0
    void OnMessage(int message, void* data,
                   uint32_t arg0, uint32_t arg1) override;        // [15] 0x140112260

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint64_t data28;       // +0x28
    int32_t  data30;       // +0x30 (ctor -1)
    uint32_t data34;       // +0x34
    uint32_t data38;       // +0x38
    uint32_t data3C;       // +0x3C
    uint32_t data40;       // +0x40
    uint32_t data44;       // +0x44
    uint32_t data48;       // +0x48
    uint32_t data4C;       // +0x4C
    uint32_t data50;       // +0x50
    uint32_t data54;       // +0x54
    uint64_t data58;       // +0x58
    uint64_t data60;       // +0x60
    uint64_t data68;       // +0x68 housing
    uint64_t data70;       // +0x70
    uint64_t data78;       // +0x78
    uint32_t data80;       // +0x80
    uint8_t  _pad84[4];    // +0x84
    uint32_t data88;       // +0x88
    uint8_t  _pad8C[4];    // +0x8C
    uint64_t data90;       // +0x90
    uint64_t data98;       // +0x98 array data (freed by the retail dtor)
    uint64_t dataA0;       // +0xA0 array count / capacity
    int32_t  dataA8;       // +0xA8 (ctor -1)
    uint32_t dataAC;       // +0xAC
    uint32_t dataB0;       // +0xB0
    uint8_t  _padB4[4];    // +0xB4

    uint8_t  meshInstance[0x120]; // +0xB8 Graphics::MeshInstance storage (see above)

    uint64_t data1D8;      // +0x1D8 toolId / scene object id (ctor -1)
    uint64_t data1E0;      // +0x1E0 (ctor 4)
    uint64_t data1E8;      // +0x1E8
    uint64_t data1F0;      // +0x1F0
    uint64_t data1F8;      // +0x1F8
    uint64_t data200;      // +0x200
    uint64_t data208;      // +0x208
    uint32_t data210;      // +0x210
    uint8_t  _pad214[4];   // +0x214
    uint64_t data218;      // +0x218
    uint8_t  data220;      // +0x220
    uint8_t  _pad221[7];   // +0x221
    uint64_t data228;      // +0x228
    uint64_t data230;      // +0x230
    uint64_t data238;      // +0x238 workplace
    uint8_t  _pad240[8];   // +0x240 not written by the retail ctor
    uint64_t data248;      // +0x248
    uint64_t data250;      // +0x250
    uint64_t data258;      // +0x258
    uint64_t data260;      // +0x260
    uint64_t data268;      // +0x268
    uint64_t data270;      // +0x270
    uint64_t data278;      // +0x278 CharacterComponent
    uint64_t data280;      // +0x280
    uint64_t data288;      // +0x288 CitizenDescription
};

static_assert(sizeof(CitizenComponent) == 0x290, "CitizenComponent is 0x290 (create 0x140030110)");
static_assert(alignof(CitizenComponent) == 8, "CitizenComponent alignment");
static_assert(offsetof(CitizenComponent, data28)       == 0x28,  "");
static_assert(offsetof(CitizenComponent, data30)       == 0x30,  "");
static_assert(offsetof(CitizenComponent, data54)       == 0x54,  "");
static_assert(offsetof(CitizenComponent, data80)       == 0x80,  "");
static_assert(offsetof(CitizenComponent, data88)       == 0x88,  "");
static_assert(offsetof(CitizenComponent, data98)       == 0x98,  "");
static_assert(offsetof(CitizenComponent, dataA8)       == 0xA8,  "");
static_assert(offsetof(CitizenComponent, dataAC)       == 0xAC,  "");
static_assert(offsetof(CitizenComponent, meshInstance) == 0xB8,  "");
static_assert(offsetof(CitizenComponent, data1D8)      == 0x1D8, "");
static_assert(offsetof(CitizenComponent, data1E0)      == 0x1E0, "");
static_assert(offsetof(CitizenComponent, data210)      == 0x210, "");
static_assert(offsetof(CitizenComponent, data218)      == 0x218, "");
static_assert(offsetof(CitizenComponent, data220)      == 0x220, "");
static_assert(offsetof(CitizenComponent, data228)      == 0x228, "");
static_assert(offsetof(CitizenComponent, data238)      == 0x238, "");
static_assert(offsetof(CitizenComponent, data248)      == 0x248, "");
static_assert(offsetof(CitizenComponent, data278)      == 0x278, "");
static_assert(offsetof(CitizenComponent, data288)      == 0x288, "");
