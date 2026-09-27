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


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@ImageBuffer@Video@@SAPEAVBase@Object@@XZ", 0x18001e9f0ull, (void*)&RtFactNew_Video_ImageBuffer,
      "?CreateInPlaceObject@ImageBuffer@Video@@SAPEAVBase@Object@@PEAX@Z", 0x18001ea70ull, (void*)&RtFactIP_Video_ImageBuffer, &kClsVideo_ImageBuffer },
};
