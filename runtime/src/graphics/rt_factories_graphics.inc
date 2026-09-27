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

// --------------------------------------------------------------------------
// Graphics::MaterialInstance
//   ?CreateNewObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@XZ  0x180002900
//   ?CreateInPlaceObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@PEAX@Z  0x1800029d0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsGraphics_MaterialInstance_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180002963ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x18000296dull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x18000297bull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x18000297eull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180002982ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7MaterialInstance@Graphics@@6B@", 0x18000298dull },
    { RtFactOp::Imm, 4, 0x20, 0x0ull, nullptr, 0x180002990ull },
    { RtFactOp::Imm, 2, 0x24, 0x0ull, nullptr, 0x180002993ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180002997ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x18000299bull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x18000299full },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x1800029a3ull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x1800029a7ull },
    { RtFactOp::Imm, 8, 0x50, 0x0ull, nullptr, 0x1800029abull },
    { RtFactOp::Imm, 8, 0x58, 0x0ull, nullptr, 0x1800029afull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsGraphics_MaterialInstance_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x1800029edull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x1800029f7ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180002a05ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180002a08ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180002a0cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7MaterialInstance@Graphics@@6B@", 0x180002a17ull },
    { RtFactOp::Imm, 4, 0x20, 0x0ull, nullptr, 0x180002a1aull },
    { RtFactOp::Imm, 2, 0x24, 0x0ull, nullptr, 0x180002a1dull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180002a21ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180002a25ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x180002a29ull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x180002a2dull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x180002a31ull },
    { RtFactOp::Imm, 8, 0x50, 0x0ull, nullptr, 0x180002a35ull },
    { RtFactOp::Imm, 8, 0x58, 0x0ull, nullptr, 0x180002a39ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountGraphics_MaterialInstance[3];
static const RtFactoryClass kClsGraphics_MaterialInstance = {
    "Graphics::MaterialInstance", 0x60, 8,
    kStepsGraphics_MaterialInstance_New, kStepsGraphics_MaterialInstance_IP,
    0x180002900ull, 0x1800029d0ull, kCountGraphics_MaterialInstance,
};
extern "C" void* RtFactNew_Graphics_MaterialInstance()          { return RtFactoryNew(kClsGraphics_MaterialInstance); }
extern "C" void* RtFactIP_Graphics_MaterialInstance(void* p)    { return RtFactoryInPlace(kClsGraphics_MaterialInstance, p); }

static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@XZ", 0x180012d80ull, (void*)&RtFactNew_Graphics_EdgeMesh,
      "?CreateInPlaceObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x180012e00ull, (void*)&RtFactIP_Graphics_EdgeMesh, &kClsGraphics_EdgeMesh },
    { "?CreateNewObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@XZ", 0x180002900ull, (void*)&RtFactNew_Graphics_MaterialInstance,
      "?CreateInPlaceObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x1800029d0ull, (void*)&RtFactIP_Graphics_MaterialInstance, &kClsGraphics_MaterialInstance },
};
