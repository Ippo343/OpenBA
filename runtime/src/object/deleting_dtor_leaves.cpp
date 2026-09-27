// ============================================================================
// runtime/src/object/deleting_dtor_leaves.cpp — the six "(NOT ported)" scalar
// destructors the RT_DELDTOR table in deleting_dtor.cpp still resolved to
// NULL. 2026-09-12 cutover walk.
// ============================================================================
//
// The first DLL-absent boot to reach the in-game world quit through the
// pause menu and died in ExternalFactory::Clean: RtDelDtor_SoundEffect ->
// DeletingDtorCore -> `dtor(p)` with dtor == NULL, because
// ??1SoundEffect@Audio@@ had never been ported and the table row resolves it
// by address. The other five rows in the same state would fail the same way
// the moment one of their objects is deleted, so all six are closed here.
//
// Every body is transcribed from Ghidra's decompile of the retail DLL; each is
// a vtable restamp plus calls into bodies this tree already registers:
//
//   ??1ToolTipDialog@UI@@UEAA@XZ          0x1800dac70
//       vptr = ??_7ToolTipDialog@UI@@6B@; ??1DialogInstance@UI@@
//   ??1ReaderWriter@IO@@UEAA@XZ           0x1800b9150
//       vptr = ??_7ReaderWriter@IO@@6B@   (nothing else)
//   ??1Light@Graphics@@UEAA@XZ            0x180058d20
//       ??1Geometry@Graphics@@            (NO restamp: a bare tail-jump)
//   ??1SunLight@Graphics@@UEAA@XZ         0x180108160
//       vptr = ??_7SunLight@Graphics@@6B@;
//       +0xf0, +0xf8: if non-null, slot0(obj, 1)   (deleting dtor, delete)
//       +0x100: if non-null, RenderBuffer::PlatformDestruct then
//               ??3RenderBuffer@Video@@ (operator delete)
//       ??1Geometry@Graphics@@
//   ??1AnimationGroup@Graphics@@UEAA@XZ   0x1800fb620
//       vptr = ??_7AnimationGroup@Graphics@@6B@;
//       0x18001c3b0(this+0x48)  Array<float> dtor
//       0x18001c2d0(this+0x38)  Array<{String,String,float}> elem dtor
//       0x18001c1e0(this+0x20)  Array<AnimationClip> dtor
//       ??1External@Object@@
//   ??1SoundEffect@Audio@@UEAA@XZ         0x1800e0880
//       vptr = ??_7SoundEffect@Audio@@6B@;
//       0x1800516c0(this+0x30)  Array<Array<..>> elem dtor
//       ??1External@Object@@
//
// The second quit walk died one row later, on MaterialInstance -- the table
// also carried five rows annotated "still the DLL's" that nothing registers
// (three others with that note, SpriteSheet / Element / DialogInstance, ARE
// registered by their own files; the notes were stale). Those five are here
// too, transcribed the same way:
//
//   ??1AchievementInstance@Achievement@@UEAA@XZ   0x18009e400
//       vptr = ??_7AchievementInstance@Achievement@@6B@;
//       ??1GoalList@Achievement@@(this+0x10); ??1Internal@Object@@
//   ??1MaterialInstance@Graphics@@UEAA@XZ        0x180103a00
//       vptr = ??_7MaterialInstance@Graphics@@6B@;
//       0x180004d60(this+0x50); 0x180004c80(this+0x40); 0x180004b70(this+0x30)
//       ??1External@Object@@ (tail jump)
//   ??1Base@Object@@UEAA@XZ                      0x1800c0360
//       vptr = ??_7Base@Object@@6B@   (nothing else)
//   ??1Stream@IO@@UEAA@XZ                        0x1800b9a40
//       vptr = ??_7Stream@IO@@6B@; if (+0x10 encoding) MemFree(encoding)
//       (the Enter/Free/Leave trio is the inlined ?MemFree@MemoryAllocator@)
//   ??1ParticleInstance@Graphics@@UEAA@XZ        0x1801041f0
//       vptr = ??_7ParticleInstance@Graphics@@6B@; RemoveState(this);
//       ??1Geometry@Graphics@@
//
// Registered by BOTH doors: the deleting-dtor table reaches them by address,
// the exported names cover every other caller.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
// ENDGAME B.3.5: the two genuine MEMBER calls in this file's bodies
// (`RenderBuffer::PlatformDestruct`, `ParticleInstance::RemoveState`). The
// `??1…`/`??3…`/`??_7…` rows are NOT retired here — B35_RULES section 11 keeps
// every deleting-dtor row, vtable stamp and dtor door byte-identical.
#include "../../types/video/Device.h"
#include "../../types/graphics/ParticleInstance.h"
#include "../../types/achievement/Goal.h"   // ENDGAME B.3.7 callers (K5): Achievement::GoalList

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace {

using DtorFn      = void (*)(void*);
using ArrayDtorFn = void (*)(void*);

// One RT_DATA_SLOT per call site: the macro caches per expansion, so a
// helper taking the name as a VARIABLE would stamp every class with whichever
// vtable resolved first.
#define STAMP(self, MANGLED)     (*reinterpret_cast<const void**>(self) = RT_DATA_SLOT(const void*, MANGLED))

inline ArrayDtorFn ByAddr(uintptr_t a) {
    return reinterpret_cast<ArrayDtorFn>(ResolveAddr(Module::Runtime, a));
}

inline void* Field(void* self, size_t off) {
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + off);
}

}  // namespace

// ??1ToolTipDialog@UI@@UEAA@XZ   0x1800dac70
extern "C" void RtUi_ToolTipDialog_Dtor(void* self) {
    STAMP(self, "??_7ToolTipDialog@UI@@6B@");
    RT_EXPORT_ONCE(DtorFn, "??1DialogInstance@UI@@UEAA@XZ")(self);
}

// ??1ReaderWriter@IO@@UEAA@XZ   0x1800b9150
extern "C" void RtIo_ReaderWriter_Dtor(void* self) {
    STAMP(self, "??_7ReaderWriter@IO@@6B@");
}

// ??1SunLight@Graphics@@UEAA@XZ 0x180108160 -- REMOVED (ENDGAME B.3.9 pilot):
// the name and address were registered TWICE, here and by block RB's one-body
// door `Graphics_SunLight_Dtor` (runtime/src/graphics/sunlight.cpp), and this
// older transcription won by running later (INVENTORY_B finding 1). RB's body
// -- `~SunLight`'s ReleaseFields, shared with the real destructor -- is THE body.
// (Pilot c2 then retired RB's door too: ~SunLight is the only destructor.)

// ??1AnimationGroup@Graphics@@UEAA@XZ   0x1800fb620
extern "C" void RtGraphics_AnimationGroup_Dtor(void* self) {
    STAMP(self, "??_7AnimationGroup@Graphics@@6B@");
    uint8_t* d = static_cast<uint8_t*>(self);
    ByAddr(0x18001c3b0ull)(d + 0x48);
    ByAddr(0x18001c2d0ull)(d + 0x38);
    ByAddr(0x18001c1e0ull)(d + 0x20);
    RT_EXPORT_ONCE(DtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ??1SoundEffect@Audio@@UEAA@XZ   0x1800e0880
extern "C" void RtAudio_SoundEffect_Dtor(void* self) {
    STAMP(self, "??_7SoundEffect@Audio@@6B@");
    ByAddr(0x1800516c0ull)(static_cast<uint8_t*>(self) + 0x30);
    RT_EXPORT_ONCE(DtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ??1AchievementInstance@Achievement@@UEAA@XZ   0x18009e400
extern "C" void RtAchievement_AchievementInstance_Dtor(void* self) {
    STAMP(self, "??_7AchievementInstance@Achievement@@6B@");
    // GoalList is a VALUE type with a real dtor (B37_RULES B7-8), held at +0x10.
    reinterpret_cast< ::Achievement::GoalList*>(static_cast<uint8_t*>(self) + 0x10)
        ->Achievement::GoalList::~GoalList();  // B.3.7 bridge
    RT_EXPORT_ONCE(DtorFn, "??1Internal@Object@@UEAA@XZ")(self);
}

// ??1MaterialInstance@Graphics@@UEAA@XZ   0x180103a00
extern "C" void RtGraphics_MaterialInstance_Dtor(void* self) {
    STAMP(self, "??_7MaterialInstance@Graphics@@6B@");
    uint8_t* d = static_cast<uint8_t*>(self);
    ByAddr(0x180004d60ull)(d + 0x50);
    ByAddr(0x180004c80ull)(d + 0x40);
    ByAddr(0x180004b70ull)(d + 0x30);
    RT_EXPORT_ONCE(DtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ??1Base@Object@@UEAA@XZ   0x1800c0360
extern "C" void RtObject_Base_Dtor(void* self) {
    STAMP(self, "??_7Base@Object@@6B@");
}

// ??1Stream@IO@@UEAA@XZ   0x1800b9a40
extern "C" void RtIo_Stream_Dtor(void* self) {
    STAMP(self, "??_7Stream@IO@@6B@");
    if (void* enc = Field(self, 0x10))
        RT_EXPORT_ONCE(DtorFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(enc);
}

// ??1ParticleInstance@Graphics@@UEAA@XZ   0x1801041f0
extern "C" void RtGraphics_ParticleInstance_Dtor(void* self) {
    STAMP(self, "??_7ParticleInstance@Graphics@@6B@");
    reinterpret_cast<Graphics::ParticleInstance*>(self)->RemoveState();  // B.3.5 bridge
    RT_EXPORT_ONCE(DtorFn, "??1Geometry@Graphics@@UEAA@XZ")(self);
}

int RtObjectDeletingDtorLeavesRegister() {
    RegisterRuntimeExport("??1AchievementInstance@Achievement@@UEAA@XZ", (void*)&RtAchievement_AchievementInstance_Dtor);
    RegisterRuntimeAddr  (0x18009e400ull,                                  (void*)&RtAchievement_AchievementInstance_Dtor);
    RegisterRuntimeExport("??1MaterialInstance@Graphics@@UEAA@XZ",        (void*)&RtGraphics_MaterialInstance_Dtor);
    RegisterRuntimeAddr  (0x180103a00ull,                                  (void*)&RtGraphics_MaterialInstance_Dtor);
    RegisterRuntimeExport("??1Base@Object@@UEAA@XZ",                      (void*)&RtObject_Base_Dtor);
    RegisterRuntimeAddr  (0x1800c0360ull,                                  (void*)&RtObject_Base_Dtor);
    RegisterRuntimeExport("??1Stream@IO@@UEAA@XZ",                        (void*)&RtIo_Stream_Dtor);
    RegisterRuntimeAddr  (0x1800b9a40ull,                                  (void*)&RtIo_Stream_Dtor);
    RegisterRuntimeExport("??1ParticleInstance@Graphics@@UEAA@XZ",        (void*)&RtGraphics_ParticleInstance_Dtor);
    RegisterRuntimeAddr  (0x1801041f0ull,                                  (void*)&RtGraphics_ParticleInstance_Dtor);
    RegisterRuntimeExport("??1ToolTipDialog@UI@@UEAA@XZ",        (void*)&RtUi_ToolTipDialog_Dtor);
    RegisterRuntimeAddr  (0x1800dac70ull,                          (void*)&RtUi_ToolTipDialog_Dtor);
    RegisterRuntimeExport("??1ReaderWriter@IO@@UEAA@XZ",         (void*)&RtIo_ReaderWriter_Dtor);
    RegisterRuntimeAddr  (0x1800b9150ull,                          (void*)&RtIo_ReaderWriter_Dtor);
    RegisterRuntimeExport("??1AnimationGroup@Graphics@@UEAA@XZ", (void*)&RtGraphics_AnimationGroup_Dtor);
    RegisterRuntimeAddr  (0x1800fb620ull,                          (void*)&RtGraphics_AnimationGroup_Dtor);
    RegisterRuntimeExport("??1SoundEffect@Audio@@UEAA@XZ",       (void*)&RtAudio_SoundEffect_Dtor);
    RegisterRuntimeAddr  (0x1800e0880ull,                          (void*)&RtAudio_SoundEffect_Dtor);
    return 9;
}
