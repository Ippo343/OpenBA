// ============================================================================
// runtime/src/graphics/rt_factories_graphics.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_graphics.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Graphics::EdgeMesh
//   ?CreateNewObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@XZ  0x180012d80
//   ?CreateInPlaceObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@PEAX@Z  0x180012e00
// --------------------------------------------------------------------------
static const RtCtorStep kStepsGraphics_EdgeMesh_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0EdgeMesh@Graphics@@QEAA@XZ", 0x180012de1ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsGraphics_EdgeMesh_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0EdgeMesh@Graphics@@QEAA@XZ", 0x180012e03ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountGraphics_EdgeMesh[3];
static const RtFactoryClass kClsGraphics_EdgeMesh = {
    "Graphics::EdgeMesh", 0x180, 8,
    kStepsGraphics_EdgeMesh_New, kStepsGraphics_EdgeMesh_IP,
    0x180012d80ull, 0x180012e00ull, kCountGraphics_EdgeMesh,
};
extern "C" void* RtFactNew_Graphics_EdgeMesh()          { return RtFactoryNew(kClsGraphics_EdgeMesh); }
extern "C" void* RtFactIP_Graphics_EdgeMesh(void* p)    { return RtFactoryInPlace(kClsGraphics_EdgeMesh, p); }

static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@XZ", 0x180012d80ull, (void*)&RtFactNew_Graphics_EdgeMesh,
      "?CreateInPlaceObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x180012e00ull, (void*)&RtFactIP_Graphics_EdgeMesh, &kClsGraphics_EdgeMesh },
};
