// ============================================================================
// runtime/src/pathfinding/rt_factories_pathfinding.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_pathfinding.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Pathfinding::PointList
//   ?CreateNewObject@PointList@Pathfinding@@SAPEAVBase@Object@@XZ  0x180070f30
//   ?CreateInPlaceObject@PointList@Pathfinding@@SAPEAVBase@Object@@PEAX@Z  0x180070ff0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsPathfinding_PointList_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180070f93ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180070f9dull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180070fabull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180070faeull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180070fb2ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7PointList@Pathfinding@@6B@", 0x180070fbdull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x180070fc0ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180070fc4ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180070fc8ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x180070fccull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x180070fd0ull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x180070fd4ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsPathfinding_PointList_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x18007100dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180071017ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180071025ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180071028ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18007102cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7PointList@Pathfinding@@6B@", 0x180071037ull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x18007103aull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x18007103eull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180071042ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x180071046ull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x18007104aull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x18007104eull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountPathfinding_PointList[3];
static const RtFactoryClass kClsPathfinding_PointList = {
    "Pathfinding::PointList", 0x50, 8,
    kStepsPathfinding_PointList_New, kStepsPathfinding_PointList_IP,
    0x180070f30ull, 0x180070ff0ull, kCountPathfinding_PointList,
};
extern "C" void* RtFactNew_Pathfinding_PointList()          { return RtFactoryNew(kClsPathfinding_PointList); }
extern "C" void* RtFactIP_Pathfinding_PointList(void* p)    { return RtFactoryInPlace(kClsPathfinding_PointList, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@PointList@Pathfinding@@SAPEAVBase@Object@@XZ", 0x180070f30ull, (void*)&RtFactNew_Pathfinding_PointList,
      "?CreateInPlaceObject@PointList@Pathfinding@@SAPEAVBase@Object@@PEAX@Z", 0x180070ff0ull, (void*)&RtFactIP_Pathfinding_PointList, &kClsPathfinding_PointList },
};
