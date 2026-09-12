// ============================================================================
// runtime/src/picking/rt_factories_picking.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_picking.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Picking::PickingMesh
//   ?CreateNewObject@PickingMesh@Picking@@SAPEAVBase@Object@@XZ  0x180071ba0
//   ?CreateInPlaceObject@PickingMesh@Picking@@SAPEAVBase@Object@@PEAX@Z  0x180071c60
// --------------------------------------------------------------------------
static const RtCtorStep kStepsPicking_PickingMesh_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180071c03ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180071c0dull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180071c1bull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180071c1eull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180071c22ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7PickingMesh@Picking@@6B@", 0x180071c2dull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x180071c30ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180071c34ull },
    { RtFactOp::Imm, 1, 0x30, 0x0ull, nullptr, 0x180071c38ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x180071c3bull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x180071c3full },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsPicking_PickingMesh_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180071c7dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180071c87ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180071c95ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180071c98ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180071c9cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7PickingMesh@Picking@@6B@", 0x180071ca7ull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x180071caaull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180071caeull },
    { RtFactOp::Imm, 1, 0x30, 0x0ull, nullptr, 0x180071cb2ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x180071cb5ull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x180071cb9ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountPicking_PickingMesh[3];
static const RtFactoryClass kClsPicking_PickingMesh = {
    "Picking::PickingMesh", 0x48, 8,
    kStepsPicking_PickingMesh_New, kStepsPicking_PickingMesh_IP,
    0x180071ba0ull, 0x180071c60ull, kCountPicking_PickingMesh,
};
extern "C" void* RtFactNew_Picking_PickingMesh()          { return RtFactoryNew(kClsPicking_PickingMesh); }
extern "C" void* RtFactIP_Picking_PickingMesh(void* p)    { return RtFactoryInPlace(kClsPicking_PickingMesh, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@PickingMesh@Picking@@SAPEAVBase@Object@@XZ", 0x180071ba0ull, (void*)&RtFactNew_Picking_PickingMesh,
      "?CreateInPlaceObject@PickingMesh@Picking@@SAPEAVBase@Object@@PEAX@Z", 0x180071c60ull, (void*)&RtFactIP_Picking_PickingMesh, &kClsPicking_PickingMesh },
};
