// ============================================================================
// runtime/src/video/rt_factories_video.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_video.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Video::ImageBuffer
//   ?CreateNewObject@ImageBuffer@Video@@SAPEAVBase@Object@@XZ  0x18001e9f0
//   ?CreateInPlaceObject@ImageBuffer@Video@@SAPEAVBase@Object@@PEAX@Z  0x18001ea70
// --------------------------------------------------------------------------
static const RtCtorStep kStepsVideo_ImageBuffer_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0ImageBuffer@Video@@QEAA@XZ", 0x18001ea4full },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsVideo_ImageBuffer_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0ImageBuffer@Video@@QEAA@XZ", 0x18001ea73ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountVideo_ImageBuffer[3];
static const RtFactoryClass kClsVideo_ImageBuffer = {
    "Video::ImageBuffer", 0x68, 8,
    kStepsVideo_ImageBuffer_New, kStepsVideo_ImageBuffer_IP,
    0x18001e9f0ull, 0x18001ea70ull, kCountVideo_ImageBuffer,
};
extern "C" void* RtFactNew_Video_ImageBuffer()          { return RtFactoryNew(kClsVideo_ImageBuffer); }
extern "C" void* RtFactIP_Video_ImageBuffer(void* p)    { return RtFactoryInPlace(kClsVideo_ImageBuffer, p); }

// --------------------------------------------------------------------------
// Video::PixelProgram
//   ?CreateNewObject@PixelProgram@Video@@SAPEAVBase@Object@@XZ  0x18004e9d0
//   ?CreateInPlaceObject@PixelProgram@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004ea50
// --------------------------------------------------------------------------
static const RtCtorStep kStepsVideo_PixelProgram_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0PixelProgram@Video@@QEAA@XZ", 0x18004ea2full },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsVideo_PixelProgram_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0PixelProgram@Video@@QEAA@XZ", 0x18004ea53ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountVideo_PixelProgram[3];
static const RtFactoryClass kClsVideo_PixelProgram = {
    "Video::PixelProgram", 0x58, 8,
    kStepsVideo_PixelProgram_New, kStepsVideo_PixelProgram_IP,
    0x18004e9d0ull, 0x18004ea50ull, kCountVideo_PixelProgram,
};
extern "C" void* RtFactNew_Video_PixelProgram()          { return RtFactoryNew(kClsVideo_PixelProgram); }
extern "C" void* RtFactIP_Video_PixelProgram(void* p)    { return RtFactoryInPlace(kClsVideo_PixelProgram, p); }

// --------------------------------------------------------------------------
// Video::RenderState
//   ?CreateNewObject@RenderState@Video@@SAPEAVBase@Object@@XZ  0x18004b660
//   ?CreateInPlaceObject@RenderState@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004b6e0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsVideo_RenderState_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0RenderState@Video@@QEAA@XZ", 0x18004b6bfull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsVideo_RenderState_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0RenderState@Video@@QEAA@XZ", 0x18004b6e3ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountVideo_RenderState[3];
static const RtFactoryClass kClsVideo_RenderState = {
    "Video::RenderState", 0x80, 8,
    kStepsVideo_RenderState_New, kStepsVideo_RenderState_IP,
    0x18004b660ull, 0x18004b6e0ull, kCountVideo_RenderState,
};
extern "C" void* RtFactNew_Video_RenderState()          { return RtFactoryNew(kClsVideo_RenderState); }
extern "C" void* RtFactIP_Video_RenderState(void* p)    { return RtFactoryInPlace(kClsVideo_RenderState, p); }

// --------------------------------------------------------------------------
// Video::ShaderProgram
//   ?CreateNewObject@ShaderProgram@Video@@SAPEAVBase@Object@@XZ  0x18004e0d0
//   ?CreateInPlaceObject@ShaderProgram@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004e0e0
// --------------------------------------------------------------------------
static uint64_t kCountVideo_ShaderProgram[3];
static const RtFactoryClass kClsVideo_ShaderProgram = {
    "Video::ShaderProgram", 0x0, 0,
    nullptr, nullptr,
    0x18004e0d0ull, 0x18004e0e0ull, kCountVideo_ShaderProgram,
};
extern "C" void* RtFactNew_Video_ShaderProgram()          { return RtFactoryNew(kClsVideo_ShaderProgram); }
extern "C" void* RtFactIP_Video_ShaderProgram(void* p)    { return RtFactoryInPlace(kClsVideo_ShaderProgram, p); }

// --------------------------------------------------------------------------
// Video::VertexLayout
//   ?CreateNewObject@VertexLayout@Video@@SAPEAVBase@Object@@XZ  0x18004ddc0
//   ?CreateInPlaceObject@VertexLayout@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004de40
// --------------------------------------------------------------------------
static const RtCtorStep kStepsVideo_VertexLayout_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0VertexLayout@Video@@QEAA@XZ", 0x18004de1full },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsVideo_VertexLayout_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0VertexLayout@Video@@QEAA@XZ", 0x18004de43ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountVideo_VertexLayout[3];
static const RtFactoryClass kClsVideo_VertexLayout = {
    "Video::VertexLayout", 0x48, 8,
    kStepsVideo_VertexLayout_New, kStepsVideo_VertexLayout_IP,
    0x18004ddc0ull, 0x18004de40ull, kCountVideo_VertexLayout,
};
extern "C" void* RtFactNew_Video_VertexLayout()          { return RtFactoryNew(kClsVideo_VertexLayout); }
extern "C" void* RtFactIP_Video_VertexLayout(void* p)    { return RtFactoryInPlace(kClsVideo_VertexLayout, p); }

// --------------------------------------------------------------------------
// Video::VertexProgram
//   ?CreateNewObject@VertexProgram@Video@@SAPEAVBase@Object@@XZ  0x18004e650
//   ?CreateInPlaceObject@VertexProgram@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004e6d0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsVideo_VertexProgram_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0VertexProgram@Video@@QEAA@XZ", 0x18004e6afull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsVideo_VertexProgram_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0VertexProgram@Video@@QEAA@XZ", 0x18004e6d3ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountVideo_VertexProgram[3];
static const RtFactoryClass kClsVideo_VertexProgram = {
    "Video::VertexProgram", 0x58, 8,
    kStepsVideo_VertexProgram_New, kStepsVideo_VertexProgram_IP,
    0x18004e650ull, 0x18004e6d0ull, kCountVideo_VertexProgram,
};
extern "C" void* RtFactNew_Video_VertexProgram()          { return RtFactoryNew(kClsVideo_VertexProgram); }
extern "C" void* RtFactIP_Video_VertexProgram(void* p)    { return RtFactoryInPlace(kClsVideo_VertexProgram, p); }

// --------------------------------------------------------------------------
// Video::VideoProgram
//   ?CreateNewObject@VideoProgram@Video@@SAPEAVBase@Object@@XZ  0x18004ed40
//   ?CreateInPlaceObject@VideoProgram@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004edc0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsVideo_VideoProgram_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0VideoProgram@Video@@QEAA@XZ", 0x18004ed9full },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsVideo_VideoProgram_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0VideoProgram@Video@@QEAA@XZ", 0x18004edc3ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountVideo_VideoProgram[3];
static const RtFactoryClass kClsVideo_VideoProgram = {
    "Video::VideoProgram", 0x48, 8,
    kStepsVideo_VideoProgram_New, kStepsVideo_VideoProgram_IP,
    0x18004ed40ull, 0x18004edc0ull, kCountVideo_VideoProgram,
};
extern "C" void* RtFactNew_Video_VideoProgram()          { return RtFactoryNew(kClsVideo_VideoProgram); }
extern "C" void* RtFactIP_Video_VideoProgram(void* p)    { return RtFactoryInPlace(kClsVideo_VideoProgram, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@ImageBuffer@Video@@SAPEAVBase@Object@@XZ", 0x18001e9f0ull, (void*)&RtFactNew_Video_ImageBuffer,
      "?CreateInPlaceObject@ImageBuffer@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18001ea70ull, (void*)&RtFactIP_Video_ImageBuffer, &kClsVideo_ImageBuffer },
    { "?CreateNewObject@PixelProgram@Video@@SAPEAVBase@Object@@XZ", 0x18004e9d0ull, (void*)&RtFactNew_Video_PixelProgram,
      "?CreateInPlaceObject@PixelProgram@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18004ea50ull, (void*)&RtFactIP_Video_PixelProgram, &kClsVideo_PixelProgram },
    { "?CreateNewObject@RenderState@Video@@SAPEAVBase@Object@@XZ", 0x18004b660ull, (void*)&RtFactNew_Video_RenderState,
      "?CreateInPlaceObject@RenderState@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18004b6e0ull, (void*)&RtFactIP_Video_RenderState, &kClsVideo_RenderState },
    { "?CreateNewObject@ShaderProgram@Video@@SAPEAVBase@Object@@XZ", 0x18004e0d0ull, (void*)&RtFactNew_Video_ShaderProgram,
      "?CreateInPlaceObject@ShaderProgram@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18004e0e0ull, (void*)&RtFactIP_Video_ShaderProgram, &kClsVideo_ShaderProgram },
    { "?CreateNewObject@VertexLayout@Video@@SAPEAVBase@Object@@XZ", 0x18004ddc0ull, (void*)&RtFactNew_Video_VertexLayout,
      "?CreateInPlaceObject@VertexLayout@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18004de40ull, (void*)&RtFactIP_Video_VertexLayout, &kClsVideo_VertexLayout },
    { "?CreateNewObject@VertexProgram@Video@@SAPEAVBase@Object@@XZ", 0x18004e650ull, (void*)&RtFactNew_Video_VertexProgram,
      "?CreateInPlaceObject@VertexProgram@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18004e6d0ull, (void*)&RtFactIP_Video_VertexProgram, &kClsVideo_VertexProgram },
    { "?CreateNewObject@VideoProgram@Video@@SAPEAVBase@Object@@XZ", 0x18004ed40ull, (void*)&RtFactNew_Video_VideoProgram,
      "?CreateInPlaceObject@VideoProgram@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18004edc0ull, (void*)&RtFactIP_Video_VideoProgram, &kClsVideo_VideoProgram },
};
