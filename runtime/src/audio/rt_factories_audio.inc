// ============================================================================
// runtime/src/audio/rt_factories_audio.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_audio.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Audio::SoundBuffer
//   ?CreateNewObject@SoundBuffer@Audio@@SAPEAVBase@Object@@XZ  0x18000b9a0
//   ?CreateInPlaceObject@SoundBuffer@Audio@@SAPEAVBase@Object@@PEAX@Z  0x18000ba90
// --------------------------------------------------------------------------
static const RtCtorStep kStepsAudio_SoundBuffer_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x18000ba03ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x18000ba0dull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x18000ba1bull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x18000ba1eull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18000ba22ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7CompiledExternal@Object@@6B@", 0x18000ba2dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7SoundBuffer@Audio@@6B@", 0x18000ba37ull },
    { RtFactOp::Imm, 2, 0x20, 0x0ull, nullptr, 0x18000ba3aull },
    { RtFactOp::Imm, 4, 0x24, 0x0ull, nullptr, 0x18000ba3eull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x18000ba41ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x18000ba45ull },
    { RtFactOp::Imm, 8, 0x54, 0x0ull, nullptr, 0x18000ba4bull },
    { RtFactOp::Imm, 8, 0x5c, 0x0ull, nullptr, 0x18000ba4full },
    { RtFactOp::Imm, 8, 0x64, 0x0ull, nullptr, 0x18000ba53ull },
    { RtFactOp::Imm, 8, 0x6c, 0x0ull, nullptr, 0x18000ba57ull },
    { RtFactOp::Imm, 8, 0x74, 0x0ull, nullptr, 0x18000ba5bull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x18000ba5full },
    { RtFactOp::Imm, 4, 0x40, 0x0ull, nullptr, 0x18000ba63ull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x18000ba66ull },
    { RtFactOp::Imm, 4, 0x50, 0x0ull, nullptr, 0x18000ba6aull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsAudio_SoundBuffer_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x18000baadull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x18000bab7ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x18000bac5ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x18000bac8ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18000baccull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7CompiledExternal@Object@@6B@", 0x18000bad7ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7SoundBuffer@Audio@@6B@", 0x18000bae1ull },
    { RtFactOp::Imm, 2, 0x20, 0x0ull, nullptr, 0x18000bae4ull },
    { RtFactOp::Imm, 4, 0x24, 0x0ull, nullptr, 0x18000bae8ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x18000baebull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x18000baefull },
    { RtFactOp::Imm, 8, 0x54, 0x0ull, nullptr, 0x18000baf5ull },
    { RtFactOp::Imm, 8, 0x5c, 0x0ull, nullptr, 0x18000baf9ull },
    { RtFactOp::Imm, 8, 0x64, 0x0ull, nullptr, 0x18000bafdull },
    { RtFactOp::Imm, 8, 0x6c, 0x0ull, nullptr, 0x18000bb01ull },
    { RtFactOp::Imm, 8, 0x74, 0x0ull, nullptr, 0x18000bb05ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x18000bb09ull },
    { RtFactOp::Imm, 4, 0x40, 0x0ull, nullptr, 0x18000bb0dull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x18000bb10ull },
    { RtFactOp::Imm, 4, 0x50, 0x0ull, nullptr, 0x18000bb14ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountAudio_SoundBuffer[3];
static const RtFactoryClass kClsAudio_SoundBuffer = {
    "Audio::SoundBuffer", 0x80, 8,
    kStepsAudio_SoundBuffer_New, kStepsAudio_SoundBuffer_IP,
    0x18000b9a0ull, 0x18000ba90ull, kCountAudio_SoundBuffer,
};
extern "C" void* RtFactNew_Audio_SoundBuffer()          { return RtFactoryNew(kClsAudio_SoundBuffer); }
extern "C" void* RtFactIP_Audio_SoundBuffer(void* p)    { return RtFactoryInPlace(kClsAudio_SoundBuffer, p); }

// --------------------------------------------------------------------------
// Audio::SoundEffect
//   ?CreateNewObject@SoundEffect@Audio@@SAPEAVBase@Object@@XZ  0x180050430
//   ?CreateInPlaceObject@SoundEffect@Audio@@SAPEAVBase@Object@@PEAX@Z  0x1800504f0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsAudio_SoundEffect_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180050493ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x18005049dull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x1800504abull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x1800504aeull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x1800504b2ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7SoundEffect@Audio@@6B@", 0x1800504bdull },
    { RtFactOp::Imm, 8, 0x20, 0x3f800000ull, nullptr, 0x1800504c0ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x1800504c8ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x1800504ccull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x1800504d0ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsAudio_SoundEffect_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x18005050dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180050517ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180050525ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180050528ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18005052cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7SoundEffect@Audio@@6B@", 0x180050537ull },
    { RtFactOp::Imm, 8, 0x20, 0x3f800000ull, nullptr, 0x18005053aull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x180050542ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180050546ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x18005054aull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountAudio_SoundEffect[3];
static const RtFactoryClass kClsAudio_SoundEffect = {
    "Audio::SoundEffect", 0x40, 8,
    kStepsAudio_SoundEffect_New, kStepsAudio_SoundEffect_IP,
    0x180050430ull, 0x1800504f0ull, kCountAudio_SoundEffect,
};
extern "C" void* RtFactNew_Audio_SoundEffect()          { return RtFactoryNew(kClsAudio_SoundEffect); }
extern "C" void* RtFactIP_Audio_SoundEffect(void* p)    { return RtFactoryInPlace(kClsAudio_SoundEffect, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@SoundBuffer@Audio@@SAPEAVBase@Object@@XZ", 0x18000b9a0ull, (void*)&RtFactNew_Audio_SoundBuffer,
      "?CreateInPlaceObject@SoundBuffer@Audio@@SAPEAVBase@Object@@PEAX@Z", 0x18000ba90ull, (void*)&RtFactIP_Audio_SoundBuffer, &kClsAudio_SoundBuffer },
    { "?CreateNewObject@SoundEffect@Audio@@SAPEAVBase@Object@@XZ", 0x180050430ull, (void*)&RtFactNew_Audio_SoundEffect,
      "?CreateInPlaceObject@SoundEffect@Audio@@SAPEAVBase@Object@@PEAX@Z", 0x1800504f0ull, (void*)&RtFactIP_Audio_SoundEffect, &kClsAudio_SoundEffect },
};
