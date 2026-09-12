// ============================================================================
// runtime/src/collision2d/rt_factories_collision2d.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_collision2d.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Collision2D::CollisionMesh
//   ?CreateNewObject@CollisionMesh@Collision2D@@SAPEAVBase@Object@@XZ  0x180065060
//   ?CreateInPlaceObject@CollisionMesh@Collision2D@@SAPEAVBase@Object@@PEAX@Z  0x180065130
// --------------------------------------------------------------------------
static const RtCtorStep kStepsCollision2D_CollisionMesh_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x1800650c3ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x1800650cdull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x1800650dbull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x1800650deull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x1800650e2ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7CollisionMesh@Collision2D@@6B@", 0x1800650edull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x1800650f0ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x1800650f4ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x1800650f8ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x1800650fcull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x180065100ull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x180065104ull },
    { RtFactOp::Imm, 8, 0x50, 0x0ull, nullptr, 0x180065108ull },
    { RtFactOp::Imm, 8, 0x58, 0x0ull, nullptr, 0x18006510cull },
    { RtFactOp::Imm, 8, 0x60, 0x0ull, nullptr, 0x180065110ull },
    { RtFactOp::Imm, 8, 0x68, 0x0ull, nullptr, 0x180065114ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsCollision2D_CollisionMesh_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x18006514dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180065157ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180065165ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180065168ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18006516cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7CollisionMesh@Collision2D@@6B@", 0x180065177ull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x18006517aull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x18006517eull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180065182ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x180065186ull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x18006518aull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x18006518eull },
    { RtFactOp::Imm, 8, 0x50, 0x0ull, nullptr, 0x180065192ull },
    { RtFactOp::Imm, 8, 0x58, 0x0ull, nullptr, 0x180065196ull },
    { RtFactOp::Imm, 8, 0x60, 0x0ull, nullptr, 0x18006519aull },
    { RtFactOp::Imm, 8, 0x68, 0x0ull, nullptr, 0x18006519eull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountCollision2D_CollisionMesh[3];
static const RtFactoryClass kClsCollision2D_CollisionMesh = {
    "Collision2D::CollisionMesh", 0x70, 8,
    kStepsCollision2D_CollisionMesh_New, kStepsCollision2D_CollisionMesh_IP,
    0x180065060ull, 0x180065130ull, kCountCollision2D_CollisionMesh,
};
extern "C" void* RtFactNew_Collision2D_CollisionMesh()          { return RtFactoryNew(kClsCollision2D_CollisionMesh); }
extern "C" void* RtFactIP_Collision2D_CollisionMesh(void* p)    { return RtFactoryInPlace(kClsCollision2D_CollisionMesh, p); }

// --------------------------------------------------------------------------
// Collision2D::Surface
//   ?CreateNewObject@Surface@Collision2D@@SAPEAVBase@Object@@XZ  0x180064b60
//   ?CreateInPlaceObject@Surface@Collision2D@@SAPEAVBase@Object@@PEAX@Z  0x180064c10
// --------------------------------------------------------------------------
static const RtCtorStep kStepsCollision2D_Surface_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180064bc3ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180064bcdull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180064bdbull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180064bdeull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180064be2ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Surface@Collision2D@@6B@", 0x180064bedull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x180064bf0ull },
    { RtFactOp::Imm, 4, 0x28, 0x0ull, nullptr, 0x180064bf4ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsCollision2D_Surface_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180064c2dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180064c37ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180064c45ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180064c48ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x180064c4cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Surface@Collision2D@@6B@", 0x180064c57ull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x180064c5aull },
    { RtFactOp::Imm, 4, 0x28, 0x0ull, nullptr, 0x180064c5eull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountCollision2D_Surface[3];
static const RtFactoryClass kClsCollision2D_Surface = {
    "Collision2D::Surface", 0x30, 8,
    kStepsCollision2D_Surface_New, kStepsCollision2D_Surface_IP,
    0x180064b60ull, 0x180064c10ull, kCountCollision2D_Surface,
};
extern "C" void* RtFactNew_Collision2D_Surface()          { return RtFactoryNew(kClsCollision2D_Surface); }
extern "C" void* RtFactIP_Collision2D_Surface(void* p)    { return RtFactoryInPlace(kClsCollision2D_Surface, p); }


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@CollisionMesh@Collision2D@@SAPEAVBase@Object@@XZ", 0x180065060ull, (void*)&RtFactNew_Collision2D_CollisionMesh,
      "?CreateInPlaceObject@CollisionMesh@Collision2D@@SAPEAVBase@Object@@PEAX@Z", 0x180065130ull, (void*)&RtFactIP_Collision2D_CollisionMesh, &kClsCollision2D_CollisionMesh },
    { "?CreateNewObject@Surface@Collision2D@@SAPEAVBase@Object@@XZ", 0x180064b60ull, (void*)&RtFactNew_Collision2D_Surface,
      "?CreateInPlaceObject@Surface@Collision2D@@SAPEAVBase@Object@@PEAX@Z", 0x180064c10ull, (void*)&RtFactIP_Collision2D_Surface, &kClsCollision2D_Surface },
};
