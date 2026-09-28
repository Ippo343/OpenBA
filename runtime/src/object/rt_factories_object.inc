// ============================================================================
// runtime/src/object/rt_factories_object.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_object.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Object::ExtensionCompiler
//   ?CreateNewObject@ExtensionCompiler@Object@@SAPEAVBase@2@XZ  0x18002c260
//   ?CreateInPlaceObject@ExtensionCompiler@Object@@SAPEAVBase@2@PEAX@Z  0x18002c270
// --------------------------------------------------------------------------
static uint64_t kCountObject_ExtensionCompiler[3];
static const RtFactoryClass kClsObject_ExtensionCompiler = {
    "Object::ExtensionCompiler", 0x0, 0,
    nullptr, nullptr,
    0x18002c260ull, 0x18002c270ull, kCountObject_ExtensionCompiler,
};
extern "C" void* RtFactNew_Object_ExtensionCompiler()          { return RtFactoryNew(kClsObject_ExtensionCompiler); }
extern "C" void* RtFactIP_Object_ExtensionCompiler(void* p)    { return RtFactoryInPlace(kClsObject_ExtensionCompiler, p); }

// --------------------------------------------------------------------------
// Object::External
//   ?CreateNewObject@External@Object@@SAPEAVBase@2@XZ  0x18002c980
//   ?CreateInPlaceObject@External@Object@@SAPEAVBase@2@PEAX@Z  0x18002c990
// --------------------------------------------------------------------------
static uint64_t kCountObject_External[3];
static const RtFactoryClass kClsObject_External = {
    "Object::External", 0x0, 0,
    nullptr, nullptr,
    0x18002c980ull, 0x18002c990ull, kCountObject_External,
};
extern "C" void* RtFactNew_Object_External()          { return RtFactoryNew(kClsObject_External); }
extern "C" void* RtFactIP_Object_External(void* p)    { return RtFactoryInPlace(kClsObject_External, p); }

// --------------------------------------------------------------------------
// Object::ExternalCompiler
//   ?CreateNewObject@ExternalCompiler@Object@@SAPEAVBase@2@XZ  0x18002c0e0
//   ?CreateInPlaceObject@ExternalCompiler@Object@@SAPEAVBase@2@PEAX@Z  0x18002c0f0
// --------------------------------------------------------------------------
static uint64_t kCountObject_ExternalCompiler[3];
static const RtFactoryClass kClsObject_ExternalCompiler = {
    "Object::ExternalCompiler", 0x0, 0,
    nullptr, nullptr,
    0x18002c0e0ull, 0x18002c0f0ull, kCountObject_ExternalCompiler,
};
extern "C" void* RtFactNew_Object_ExternalCompiler()          { return RtFactoryNew(kClsObject_ExternalCompiler); }
extern "C" void* RtFactIP_Object_ExternalCompiler(void* p)    { return RtFactoryInPlace(kClsObject_ExternalCompiler, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@ExtensionCompiler@Object@@SAPEAVBase@2@XZ", 0x18002c260ull, (void*)&RtFactNew_Object_ExtensionCompiler,
      "?CreateInPlaceObject@ExtensionCompiler@Object@@SAPEAVBase@2@PEAX@Z", 0x18002c270ull, (void*)&RtFactIP_Object_ExtensionCompiler, &kClsObject_ExtensionCompiler },
    { "?CreateNewObject@External@Object@@SAPEAVBase@2@XZ", 0x18002c980ull, (void*)&RtFactNew_Object_External,
      "?CreateInPlaceObject@External@Object@@SAPEAVBase@2@PEAX@Z", 0x18002c990ull, (void*)&RtFactIP_Object_External, &kClsObject_External },
    { "?CreateNewObject@ExternalCompiler@Object@@SAPEAVBase@2@XZ", 0x18002c0e0ull, (void*)&RtFactNew_Object_ExternalCompiler,
      "?CreateInPlaceObject@ExternalCompiler@Object@@SAPEAVBase@2@PEAX@Z", 0x18002c0f0ull, (void*)&RtFactIP_Object_ExternalCompiler, &kClsObject_ExternalCompiler },
};
