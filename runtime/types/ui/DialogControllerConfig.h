#pragma once
// ============================================================================
// runtime/types/ui/DialogControllerConfig.h — UI::DialogControllerConfig
// ============================================================================
//
// ENDGAME B.3 step 6, block B. A BASE FOR EXE CONFIG CLASSES and nothing more:
// it adds no field and no virtual to `Object::External`, and four of its eight
// slots stay `_purecall`.
//
// PARENT: `Object::External`, proved by
//   0x18003b440  ?_ParentTypeName@DialogControllerConfig@UI@@CAPEBDXZ -> "Object::External"
//
// `??_7DialogControllerConfig@UI@@6B@` @ 0x180156650, EIGHT slots
// (B36_SLOTS.md section 1):
//
//   [0] +0x00  0x18003b600  (unexported) vector deleting dtor
//   [1] +0x08  0x18003b470  ?TypeName@DialogControllerConfig@UI@@UEBAPEBDXZ
//   [2] +0x10  0x18003b450  ?Type@DialogControllerConfig@UI@@UEBAIXZ
//   [3] +0x18  0x18007ec28  _purecall   (External::GetFileVersion, still pure)
//   [4] +0x20  0x1800066e0  ?Compile@External@Object@@  INHERITED — not declared
//   [5] +0x28  0x18007ec28  _purecall   (Serialize)
//   [6] +0x30  0x18007ec28  _purecall   (Reconstruct)
//   [7] +0x38  0x18007ec28  _purecall   (Destruct)
//
// sizeof 0x20 = sizeof(Object::External): the deleting dtor's `MOV EDX,0x20`,
// and the four exe subclasses (ControlUIConfig, ResourceLimitUIConfig,
// StatusIconUIConfig, UpgradeUIConfig — EXE_SUBCLASSES.md 1C) place their own
// fields at +0x20 and build their bases through `fn_External_External`.
//
// ABSTRACT twice over: slots 3/5/6/7 are pure in retail, and slot 0 is the
// mint's row (B6-3). No real constructor.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../object/External.h"

namespace UI {

class DialogControllerConfig : public Object::External {
public:
    // [0] 0x18003b600 — pure on purpose; the RT_DELDTOR row is not retired.
    Object::Base* DeletingDtor(unsigned flags) override = 0;

    const char* TypeName() const override;                        // [1] 0x18003b470
    uint32_t    Type() const override;                            // [2] 0x18003b450
    // [3] GetFileVersion, [5] Serialize, [6] Reconstruct, [7] Destruct: PURE,
    //     inherited from Object::External unchanged. [4] Compile: External's.

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    DialogControllerConfig(const DialogControllerConfig& other);            // 0x18003b520
    DialogControllerConfig& operator=(const DialogControllerConfig& other); // 0x18003b590
    static uint32_t      TypeId();                                // 0x18003b460
    static Object::Base* CreateNewObject();                       // 0x18003b480
    static Object::Base* CreateInPlaceObject(void* storage);      // 0x18003b490

protected:
    // NOT retail's `??0DialogControllerConfig@UI@@QEAA@XZ` (0x18003b4a0) — B6-3.
    DialogControllerConfig() = default;
    // `??1DialogControllerConfig@UI@@UEAA@XZ` 0x18003b4f0. Defined EMPTY: the
    // registered door `RtUi_DialogControllerConfig_Dtor` stamps the minted
    // table and chains to ~External by name.
    ~DialogControllerConfig();

    // RULE B5-15 / B6-7, COPIED from B36_SLOTS section 3:
    //   0x18003b430  ?_TypeName@       -> "DialogControllerConfig"
    //   0x18003b440  ?_ParentTypeName@ -> "Object::External"
    static const char* _TypeName();
    static const char* _ParentTypeName();
};

static_assert(sizeof(DialogControllerConfig) == 0x20,
              "UI::DialogControllerConfig is 0x20 bytes (MOV EDX,0x20 in 0x18003b600; "
              "it adds nothing to Object::External)");
static_assert(sizeof(DialogControllerConfig) == sizeof(Object::External),
              "no field of its own");
static_assert(alignof(DialogControllerConfig) == 8, "qword-aligned (the vptr)");

}  // namespace UI
