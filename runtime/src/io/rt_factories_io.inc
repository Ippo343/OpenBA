// ============================================================================
// runtime/src/io/rt_factories_io.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_io.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// IO::PackageFile
//   ?CreateNewObject@PackageFile@IO@@SAPEAVBase@Object@@XZ  0x180025240
//   ?CreateInPlaceObject@PackageFile@IO@@SAPEAVBase@Object@@PEAX@Z  0x1800252c0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsIO_PackageFile_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0PackageFile@IO@@QEAA@XZ", 0x1800252a1ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsIO_PackageFile_IP[] = {
    { RtFactOp::Call, 8, 0, 0, "??0PackageFile@IO@@QEAA@XZ", 0x1800252c3ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountIO_PackageFile[3];
static const RtFactoryClass kClsIO_PackageFile = {
    "IO::PackageFile", 0xf0, 8,
    kStepsIO_PackageFile_New, kStepsIO_PackageFile_IP,
    0x180025240ull, 0x1800252c0ull, kCountIO_PackageFile,
};
extern "C" void* RtFactNew_IO_PackageFile()          { return RtFactoryNew(kClsIO_PackageFile); }
extern "C" void* RtFactIP_IO_PackageFile(void* p)    { return RtFactoryInPlace(kClsIO_PackageFile, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@PackageFile@IO@@SAPEAVBase@Object@@XZ", 0x180025240ull, (void*)&RtFactNew_IO_PackageFile,
      "?CreateInPlaceObject@PackageFile@IO@@SAPEAVBase@Object@@PEAX@Z", 0x1800252c0ull, (void*)&RtFactIP_IO_PackageFile, &kClsIO_PackageFile },
};
