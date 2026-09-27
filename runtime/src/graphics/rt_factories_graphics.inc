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
// Graphics::FrameCopyImageBuffer
//   ?CreateNewObject@FrameCopyImageBuffer@Graphics@@SAPEAVBase@Object@@XZ  0x18001f0a0
//   ?CreateInPlaceObject@FrameCopyImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z  0x18001f120
// --------------------------------------------------------------------------
static const RtCtorStep kStepsGraphics_FrameCopyImageBuffer_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0FrameCopyImageBuffer@Graphics@@QEAA@XZ", 0x18001f0ffull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsGraphics_FrameCopyImageBuffer_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0FrameCopyImageBuffer@Graphics@@QEAA@XZ", 0x18001f123ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountGraphics_FrameCopyImageBuffer[3];
static const RtFactoryClass kClsGraphics_FrameCopyImageBuffer = {
    "Graphics::FrameCopyImageBuffer", 0x88, 8,
    kStepsGraphics_FrameCopyImageBuffer_New, kStepsGraphics_FrameCopyImageBuffer_IP,
    0x18001f0a0ull, 0x18001f120ull, kCountGraphics_FrameCopyImageBuffer,
};
extern "C" void* RtFactNew_Graphics_FrameCopyImageBuffer()          { return RtFactoryNew(kClsGraphics_FrameCopyImageBuffer); }
extern "C" void* RtFactIP_Graphics_FrameCopyImageBuffer(void* p)    { return RtFactoryInPlace(kClsGraphics_FrameCopyImageBuffer, p); }

// --------------------------------------------------------------------------
// Graphics::GeneratedImageBuffer
//   ?CreateNewObject@GeneratedImageBuffer@Graphics@@SAPEAVBase@Object@@XZ  0x18001edf0
//   ?CreateInPlaceObject@GeneratedImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z  0x18001ee00
// --------------------------------------------------------------------------
static uint64_t kCountGraphics_GeneratedImageBuffer[3];
static const RtFactoryClass kClsGraphics_GeneratedImageBuffer = {
    "Graphics::GeneratedImageBuffer", 0x0, 0,
    nullptr, nullptr,
    0x18001edf0ull, 0x18001ee00ull, kCountGraphics_GeneratedImageBuffer,
};
extern "C" void* RtFactNew_Graphics_GeneratedImageBuffer()          { return RtFactoryNew(kClsGraphics_GeneratedImageBuffer); }
extern "C" void* RtFactIP_Graphics_GeneratedImageBuffer(void* p)    { return RtFactoryInPlace(kClsGraphics_GeneratedImageBuffer, p); }

// --------------------------------------------------------------------------
// Graphics::GraphicsMesh
//   ?CreateNewObject@GraphicsMesh@Graphics@@SAPEAVBase@Object@@XZ  0x180012480
//   ?CreateInPlaceObject@GraphicsMesh@Graphics@@SAPEAVBase@Object@@PEAX@Z  0x180012500
// --------------------------------------------------------------------------
static const RtCtorStep kStepsGraphics_GraphicsMesh_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0GraphicsMesh@Graphics@@QEAA@XZ", 0x1800124e1ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsGraphics_GraphicsMesh_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0GraphicsMesh@Graphics@@QEAA@XZ", 0x180012503ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountGraphics_GraphicsMesh[3];
static const RtFactoryClass kClsGraphics_GraphicsMesh = {
    "Graphics::GraphicsMesh", 0x98, 8,
    kStepsGraphics_GraphicsMesh_New, kStepsGraphics_GraphicsMesh_IP,
    0x180012480ull, 0x180012500ull, kCountGraphics_GraphicsMesh,
};
extern "C" void* RtFactNew_Graphics_GraphicsMesh()          { return RtFactoryNew(kClsGraphics_GraphicsMesh); }
extern "C" void* RtFactIP_Graphics_GraphicsMesh(void* p)    { return RtFactoryInPlace(kClsGraphics_GraphicsMesh, p); }

// --------------------------------------------------------------------------
// Graphics::Material
//   ?CreateNewObject@Material@Graphics@@SAPEAVBase@Object@@XZ  0x1800119c0
//   ?CreateInPlaceObject@Material@Graphics@@SAPEAVBase@Object@@PEAX@Z  0x180011a70
// --------------------------------------------------------------------------
static const RtCtorStep kStepsGraphics_Material_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180011a23ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180011a2dull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180011a3bull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180011a3eull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180011a42ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Material@Graphics@@6B@", 0x180011a4dull },
    { RtFactOp::Imm, 4, 0x20, 0x0ull, nullptr, 0x180011a50ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180011a53ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180011a57ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsGraphics_Material_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180011a8dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180011a97ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180011aa5ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180011aa8ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180011aacull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Material@Graphics@@6B@", 0x180011ab7ull },
    { RtFactOp::Imm, 4, 0x20, 0x0ull, nullptr, 0x180011abaull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180011abdull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180011ac1ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountGraphics_Material[3];
static const RtFactoryClass kClsGraphics_Material = {
    "Graphics::Material", 0x38, 8,
    kStepsGraphics_Material_New, kStepsGraphics_Material_IP,
    0x1800119c0ull, 0x180011a70ull, kCountGraphics_Material,
};
extern "C" void* RtFactNew_Graphics_Material()          { return RtFactoryNew(kClsGraphics_Material); }
extern "C" void* RtFactIP_Graphics_Material(void* p)    { return RtFactoryInPlace(kClsGraphics_Material, p); }

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

// --------------------------------------------------------------------------
// Graphics::ReflectionImageBuffer
//   ?CreateNewObject@ReflectionImageBuffer@Graphics@@SAPEAVBase@Object@@XZ  0x1800222c0
//   ?CreateInPlaceObject@ReflectionImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z  0x180022370
// --------------------------------------------------------------------------
static const RtCtorStep kStepsGraphics_ReflectionImageBuffer_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0ImageBuffer@Video@@QEAA@XZ", 0x180022321ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7GeneratedImageBuffer@Graphics@@6B@", 0x18002232eull },
    { RtFactOp::Imm, 4, 0x68, 0x0ull, nullptr, 0x180022333ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7ReflectionImageBuffer@Graphics@@6B@", 0x18002233dull },
    { RtFactOp::Imm, 8, 0x70, 0x0ull, nullptr, 0x180022340ull },
    { RtFactOp::Imm, 8, 0x78, 0x0ull, nullptr, 0x180022344ull },
    { RtFactOp::Imm, 8, 0x80, 0x0ull, nullptr, 0x180022348ull },
    { RtFactOp::Imm, 8, 0x88, 0x3f800000ull, nullptr, 0x18002234full },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsGraphics_ReflectionImageBuffer_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0ImageBuffer@Video@@QEAA@XZ", 0x18002238cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7GeneratedImageBuffer@Graphics@@6B@", 0x180022399ull },
    { RtFactOp::Imm, 4, 0x68, 0x0ull, nullptr, 0x18002239eull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7ReflectionImageBuffer@Graphics@@6B@", 0x1800223a8ull },
    { RtFactOp::Imm, 8, 0x70, 0x0ull, nullptr, 0x1800223abull },
    { RtFactOp::Imm, 8, 0x78, 0x0ull, nullptr, 0x1800223afull },
    { RtFactOp::Imm, 8, 0x80, 0x0ull, nullptr, 0x1800223b3ull },
    { RtFactOp::Imm, 8, 0x88, 0x3f800000ull, nullptr, 0x1800223baull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountGraphics_ReflectionImageBuffer[3];
static const RtFactoryClass kClsGraphics_ReflectionImageBuffer = {
    "Graphics::ReflectionImageBuffer", 0x90, 8,
    kStepsGraphics_ReflectionImageBuffer_New, kStepsGraphics_ReflectionImageBuffer_IP,
    0x1800222c0ull, 0x180022370ull, kCountGraphics_ReflectionImageBuffer,
};
extern "C" void* RtFactNew_Graphics_ReflectionImageBuffer()          { return RtFactoryNew(kClsGraphics_ReflectionImageBuffer); }
extern "C" void* RtFactIP_Graphics_ReflectionImageBuffer(void* p)    { return RtFactoryInPlace(kClsGraphics_ReflectionImageBuffer, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@XZ", 0x180012d80ull, (void*)&RtFactNew_Graphics_EdgeMesh,
      "?CreateInPlaceObject@EdgeMesh@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x180012e00ull, (void*)&RtFactIP_Graphics_EdgeMesh, &kClsGraphics_EdgeMesh },
    { "?CreateNewObject@FrameCopyImageBuffer@Graphics@@SAPEAVBase@Object@@XZ", 0x18001f0a0ull, (void*)&RtFactNew_Graphics_FrameCopyImageBuffer,
      "?CreateInPlaceObject@FrameCopyImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x18001f120ull, (void*)&RtFactIP_Graphics_FrameCopyImageBuffer, &kClsGraphics_FrameCopyImageBuffer },
    { "?CreateNewObject@GeneratedImageBuffer@Graphics@@SAPEAVBase@Object@@XZ", 0x18001edf0ull, (void*)&RtFactNew_Graphics_GeneratedImageBuffer,
      "?CreateInPlaceObject@GeneratedImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x18001ee00ull, (void*)&RtFactIP_Graphics_GeneratedImageBuffer, &kClsGraphics_GeneratedImageBuffer },
    { "?CreateNewObject@GraphicsMesh@Graphics@@SAPEAVBase@Object@@XZ", 0x180012480ull, (void*)&RtFactNew_Graphics_GraphicsMesh,
      "?CreateInPlaceObject@GraphicsMesh@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x180012500ull, (void*)&RtFactIP_Graphics_GraphicsMesh, &kClsGraphics_GraphicsMesh },
    { "?CreateNewObject@Material@Graphics@@SAPEAVBase@Object@@XZ", 0x1800119c0ull, (void*)&RtFactNew_Graphics_Material,
      "?CreateInPlaceObject@Material@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x180011a70ull, (void*)&RtFactIP_Graphics_Material, &kClsGraphics_Material },
    { "?CreateNewObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@XZ", 0x180002900ull, (void*)&RtFactNew_Graphics_MaterialInstance,
      "?CreateInPlaceObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x1800029d0ull, (void*)&RtFactIP_Graphics_MaterialInstance, &kClsGraphics_MaterialInstance },
    { "?CreateNewObject@ReflectionImageBuffer@Graphics@@SAPEAVBase@Object@@XZ", 0x1800222c0ull, (void*)&RtFactNew_Graphics_ReflectionImageBuffer,
      "?CreateInPlaceObject@ReflectionImageBuffer@Graphics@@SAPEAVBase@Object@@PEAX@Z", 0x180022370ull, (void*)&RtFactIP_Graphics_ReflectionImageBuffer, &kClsGraphics_ReflectionImageBuffer },
};
