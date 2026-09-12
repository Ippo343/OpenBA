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
// Object::CompiledExternal
//   ?CreateNewObject@CompiledExternal@Object@@SAPEAVBase@2@XZ  0x18000b6c0
//   ?CreateInPlaceObject@CompiledExternal@Object@@SAPEAVBase@2@PEAX@Z  0x18000b6d0
// --------------------------------------------------------------------------
static uint64_t kCountObject_CompiledExternal[3];
static const RtFactoryClass kClsObject_CompiledExternal = {
    "Object::CompiledExternal", 0x0, 0,
    nullptr, nullptr,
    0x18000b6c0ull, 0x18000b6d0ull, kCountObject_CompiledExternal,
};
extern "C" void* RtFactNew_Object_CompiledExternal()          { return RtFactoryNew(kClsObject_CompiledExternal); }
extern "C" void* RtFactIP_Object_CompiledExternal(void* p)    { return RtFactoryInPlace(kClsObject_CompiledExternal, p); }

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

// --------------------------------------------------------------------------
// Object::ExternalList
//   ?CreateNewObject@ExternalList@Object@@SAPEAVBase@2@XZ  0x180007460
//   ?CreateInPlaceObject@ExternalList@Object@@SAPEAVBase@2@PEAX@Z  0x180007510
// --------------------------------------------------------------------------
static const RtCtorStep kStepsObject_ExternalList_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x1800074c3ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x1800074cdull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x1800074dbull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x1800074deull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x1800074e2ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7ExternalList@Object@@6B@", 0x1800074edull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x1800074f0ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x1800074f4ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsObject_ExternalList_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x18000752dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180007537ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180007545ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180007548ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18000754cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7ExternalList@Object@@6B@", 0x180007557ull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x18000755aull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x18000755eull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountObject_ExternalList[3];
static const RtFactoryClass kClsObject_ExternalList = {
    "Object::ExternalList", 0x30, 8,
    kStepsObject_ExternalList_New, kStepsObject_ExternalList_IP,
    0x180007460ull, 0x180007510ull, kCountObject_ExternalList,
};
extern "C" void* RtFactNew_Object_ExternalList()          { return RtFactoryNew(kClsObject_ExternalList); }
extern "C" void* RtFactIP_Object_ExternalList(void* p)    { return RtFactoryInPlace(kClsObject_ExternalList, p); }

// --------------------------------------------------------------------------
// Object::Internal
//   ?CreateNewObject@Internal@Object@@SAPEAVBase@2@XZ  0x18002fc80
//   ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z  0x1800070c0
// --------------------------------------------------------------------------
static uint64_t kCountObject_Internal[3];
static const RtFactoryClass kClsObject_Internal = {
    "Object::Internal", 0x0, 0,
    nullptr, nullptr,
    0x18002fc80ull, 0x1800070c0ull, kCountObject_Internal,
};
extern "C" void* RtFactNew_Object_Internal()          { return RtFactoryNew(kClsObject_Internal); }
extern "C" void* RtFactIP_Object_Internal(void* p)    { return RtFactoryInPlace(kClsObject_Internal, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@CompiledExternal@Object@@SAPEAVBase@2@XZ", 0x18000b6c0ull, (void*)&RtFactNew_Object_CompiledExternal,
      "?CreateInPlaceObject@CompiledExternal@Object@@SAPEAVBase@2@PEAX@Z", 0x18000b6d0ull, (void*)&RtFactIP_Object_CompiledExternal, &kClsObject_CompiledExternal },
    { "?CreateNewObject@ExtensionCompiler@Object@@SAPEAVBase@2@XZ", 0x18002c260ull, (void*)&RtFactNew_Object_ExtensionCompiler,
      "?CreateInPlaceObject@ExtensionCompiler@Object@@SAPEAVBase@2@PEAX@Z", 0x18002c270ull, (void*)&RtFactIP_Object_ExtensionCompiler, &kClsObject_ExtensionCompiler },
    { "?CreateNewObject@External@Object@@SAPEAVBase@2@XZ", 0x18002c980ull, (void*)&RtFactNew_Object_External,
      "?CreateInPlaceObject@External@Object@@SAPEAVBase@2@PEAX@Z", 0x18002c990ull, (void*)&RtFactIP_Object_External, &kClsObject_External },
    { "?CreateNewObject@ExternalCompiler@Object@@SAPEAVBase@2@XZ", 0x18002c0e0ull, (void*)&RtFactNew_Object_ExternalCompiler,
      "?CreateInPlaceObject@ExternalCompiler@Object@@SAPEAVBase@2@PEAX@Z", 0x18002c0f0ull, (void*)&RtFactIP_Object_ExternalCompiler, &kClsObject_ExternalCompiler },
    { "?CreateNewObject@ExternalList@Object@@SAPEAVBase@2@XZ", 0x180007460ull, (void*)&RtFactNew_Object_ExternalList,
      "?CreateInPlaceObject@ExternalList@Object@@SAPEAVBase@2@PEAX@Z", 0x180007510ull, (void*)&RtFactIP_Object_ExternalList, &kClsObject_ExternalList },
    { "?CreateNewObject@Internal@Object@@SAPEAVBase@2@XZ", 0x18002fc80ull, (void*)&RtFactNew_Object_Internal,
      "?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z", 0x1800070c0ull, (void*)&RtFactIP_Object_Internal, &kClsObject_Internal },
};
