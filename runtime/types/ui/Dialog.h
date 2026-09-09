#pragma once
// ============================================================================
// runtime/types/ui/Dialog.h — UI::Dialog, the RESOURCE side of a dialog
// ============================================================================
//
// Wave 7 lane B (docs/runtime_port/WAVE7_LANE_B.md).
//
// `UI::Dialog` is the parsed `.rsc` description of a dialog: a root element
// description, a list of controller configurations, a sprite sheet, and three
// lookup arrays (named templates, indexed templates, indexed references).
// `UI::DialogInstance` (DialogInstance.h) is the live object built FROM one.
// One Dialog serves every instance of that dialog.
//
// `??_7Dialog@UI@@6B@` is EXPORTED, at 0x1801562a0.
// `?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A` — the class's
// `Object::BaseTypeInfo`, used by `IsDerivedFrom` — is exported as a DATA
// symbol at 0x1801faa88 (ordinal 4847). It is not a function and nothing in
// this lane ports it; our tree reads the DLL's.
//
// ---------------------------------------------------------------------------
// WHAT THIS HEADER PROVES, AND WHAT IT DOES NOT
// ---------------------------------------------------------------------------
// This lane ports exactly ONE Dialog method (`?GetTemplate@`), so the layout
// below is deliberately partial: it records every offset three decompiled
// bodies pin, and says "unknown" everywhere else rather than inventing names.
// The three bodies are
//
//   ??0DialogInstance@UI@@QEAA@…@Z   0x180114870  — reads +0x20/+0x28/+0x38/
//                                                   +0x40/+0x48/+0x50
//   ?CreateElement@Dialog@UI@@…      0x180112ed0  — reads +0x58/+0x60
//   ?GetTemplate@Dialog@UI@@…        0x1800388b0  — reads +0x68
//   ?GetReference@Dialog@UI@@…       0x1800388d0  — reads +0x78
//
//   off   field              proven by
//   ----  -----------------  ----------------------------------------------
//   0x00  base, 0x20 bytes   NOT PROVEN by this lane. The first Dialog-owned
//                            field is at +0x20, and Object::External is
//                            exactly 0x20 bytes (runtime/types/object/
//                            External.h), so the base is very likely
//                            Object::External — but no body in this lane
//                            touches +0x00..+0x1f, so it is opaque here.
//   0x20  _spriteSheet       DialogInstance ctor copies it to inst+0x58
//   0x28  _spriteName        DialogInstance ctor hands &dialog[0x28] to
//                            SpriteSheet::GetSpriteIndexFromName
//   0x38  _rootDescription   DialogInstance ctor calls its vtable +0x40 with
//                            (out = inst+0x50, this dialog) — i.e. the same
//                            "make me an element" virtual that
//                            ElementDescription::CreateInstance is reached
//                            through in ?CreateElement@
//   0x40  _defaultFlags      DialogInstance ctor seeds inst->_flags from it
//   0x48  _controllerConfigs Array data. The ctor's loop steps 0x20 bytes per
//                            entry and reads four fields out of each — see
//                            DialogControllerConfigEntry below
//   0x50  _controllerCount   the same loop's bound
//   0x58  _namedTemplates    Array data, stride 0x10: {External*, String}.
//                            ?CreateElement@ linear-scans it comparing the
//                            ExternalPtr's path identifier against entry+0x08
//   0x60  _namedTemplateCount that scan's bound
//   0x68  _templates         ?GetTemplate@ is one line:
//                            `*out = ((void**)this[0x68])[index]`
//   0x78  _references        ?GetReference@ is one line:
//                            `*out = ((void**)this[0x78])[index]`
//
// +0x68 and +0x78 are almost certainly `Collection::Array<ExternalPtr<...>>`
// (data / size / capacity), because that is what every other indexed list in
// this class is — but neither accessor bounds-checks, so neither reads the
// size, so neither PROVES a size field is there. They are declared as bare
// pointers plus explicitly-named unknown bytes.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtUI {

// One entry of Dialog::_controllerConfigs. Stride 0x20, pinned by the
// DialogInstance ctor's loop (0x180114870), which reads it as
//
//     AddController(inst, entry[0x00], inst->_dataSource,
//                   entry[0x10], entry[0x08], &entry[0x18]);
//
// and whose cursor advances by 0x20 per iteration.
//
// *** THESE TWO FIELDS WERE NAMED BACKWARDS UNTIL 2026-09-08 (lane DLG2). ***
// The header shipped `_config` at +0x08 and `_elementDesc` at +0x10, with a
// comment above it rationalising the swap as "the config is passed AFTER the
// description". That comment was the error, not a caveat about it — and lane
// UI3 had to work around it at its own call site rather than fix it here. THE
// SHIPPED BYTES SETTLE IT:
//
//     180114a07  MOV EDX,[R9]           -> arg 1, the type id
//     180114a0a  LEA RAX,[R9+0x18]
//     180114a0e  MOV [RSP+0x28],RAX     -> arg 6 (5th after `this`), String&
//     180114a13  MOV RAX,[R9+0x08]
//     180114a17  MOV [RSP+0x20],RAX     -> arg 5  *** entry+0x08 ***
//     180114a1c  MOV R9,[R9+0x10]       -> arg 4  *** entry+0x10 ***
//     180114a20  MOV R8,[RSI+0x30]      -> arg 3, the data source
//     180114a27  CALL 0x180113470
//
// and the callee's own mangled name gives the parameter order:
//   ?AddController@DialogInstance@UI@@QEAAX I PEAVBase@Object@@
//        V?$ExternalPtr@VDialogControllerConfig@UI@@@4@     <- arg 4
//        V?$ExternalPtr@VElementDescription@UI@@@4@         <- arg 5
//        AEBVString@System@@@Z                              <- arg 6
//
// arg 4 is the CONFIG and it comes from entry+0x10; arg 5 is the
// ELEMENT DESCRIPTION and it comes from entry+0x08. Both are read as 8-byte
// VALUES (`MOV RAX,[R9+8]`), which is also what makes `ExternalPtr<T>` an
// 8-byte register-passed type here. Do not re-swap these on the strength of
// the reading order in the source; pin to 0x180114a13 / 0x180114a1c.
struct DialogControllerConfigEntry {
    uint32_t _typeId;        // +0x00
    uint32_t _pad04;         // +0x04  never read
    void*    _elementDesc;   // +0x08  ExternalPtr<ElementDescription>  -> arg 5
    void*    _config;        // +0x10  ExternalPtr<DialogControllerConfig> -> arg 4
    char*    _name;          // +0x18  System::String (refcounted char*)
};
static_assert(offsetof(DialogControllerConfigEntry, _elementDesc) == 0x08,
              "config entry: the DESCRIPTION is at +0x08 (0x180114a13 -> arg 5)");
static_assert(offsetof(DialogControllerConfigEntry, _config) == 0x10,
              "config entry: the CONFIG is at +0x10 (0x180114a1c -> arg 4)");
static_assert(sizeof(DialogControllerConfigEntry) == 0x20,
              "Dialog::_controllerConfigs stride is 0x20 (ctor 0x180114870 loop)");

// One entry of Dialog::_namedTemplates. Stride 0x10, pinned by
// ?CreateElement@ (0x180112ed0): the scan starts at `data + 8`, compares that
// String against the requested path identifier, steps 0x10 per iteration, and
// on a hit re-indexes `data + i*0x10` to read the ExternalPtr at +0x00.
struct DialogNamedTemplate {
    void* _description;   // +0x00  ExternalPtr<ElementDescription>
    char* _name;          // +0x08  System::String
};
static_assert(sizeof(DialogNamedTemplate) == 0x10,
              "Dialog::_namedTemplates stride is 0x10 (CreateElement 0x180112ed0)");

struct Dialog {
    uint8_t  _base[0x20];              // +0x00  opaque; see the note above
    void*    _spriteSheet;             // +0x20  UI::SpriteSheet*
    char*    _spriteName;              // +0x28  System::String
    uint8_t  _unknown30[0x08];         // +0x30  never read in this lane
    void*    _rootDescription;         // +0x38  Object::External* (has a +0x40 virtual)
    uint32_t _defaultFlags;            // +0x40
    uint32_t _unknown44;               // +0x44
    DialogControllerConfigEntry* _controllerConfigs;  // +0x48
    int32_t  _controllerCount;         // +0x50
    int32_t  _controllerCapacity;      // +0x54  Array<> shape; not read here
    DialogNamedTemplate* _namedTemplates;            // +0x58
    int32_t  _namedTemplateCount;      // +0x60
    int32_t  _namedTemplateCapacity;   // +0x64  Array<> shape; not read here
    void**   _templates;               // +0x68  ExternalPtr<ElementDescription>[]
    uint8_t  _unknown70[0x08];         // +0x70  presumed size/capacity, unproven
    void**   _references;              // +0x78  ExternalPtr<Object::External>[]
    // +0x80 — WAVE 10 LANE DLG2. The default ctor `??0Dialog@UI@@QEAA@XZ`
    // (0x180112db0) ends with `MOV qword ptr [RCX+0x80],RDX` at 0x180112e2a, so
    // there IS a qword here and the class is at least 0x88 bytes. Nothing in
    // this tree READS it, so it keeps the "unknown" spelling every other
    // unproven field in this struct uses rather than acquiring a made-up name.
    uint8_t  _unknown80[0x08];         // +0x80  ctor zeroes it; never read
};

static_assert(offsetof(Dialog, _spriteSheet)         == 0x20, "Dialog::_spriteSheet @ +0x20");
static_assert(offsetof(Dialog, _spriteName)          == 0x28, "Dialog::_spriteName @ +0x28");
static_assert(offsetof(Dialog, _rootDescription)     == 0x38, "Dialog::_rootDescription @ +0x38");
static_assert(offsetof(Dialog, _defaultFlags)        == 0x40, "Dialog::_defaultFlags @ +0x40");
static_assert(offsetof(Dialog, _controllerConfigs)   == 0x48, "Dialog::_controllerConfigs @ +0x48");
static_assert(offsetof(Dialog, _controllerCount)     == 0x50, "Dialog::_controllerCount @ +0x50");
static_assert(offsetof(Dialog, _namedTemplates)      == 0x58, "Dialog::_namedTemplates @ +0x58");
static_assert(offsetof(Dialog, _namedTemplateCount)  == 0x60, "Dialog::_namedTemplateCount @ +0x60");
static_assert(offsetof(Dialog, _templates)           == 0x68, "Dialog::_templates @ +0x68 (GetTemplate)");
static_assert(offsetof(Dialog, _references)          == 0x78, "Dialog::_references @ +0x78 (GetReference)");

static_assert(offsetof(Dialog, _unknown80)          == 0x80, "Dialog +0x80 (ctor 0x180112e2a)");

// STILL NO sizeof ASSERT, deliberately. Lane DLG2's ctor proves the class is at
// least 0x88 bytes — that is where the last store lands — but a ctor's WRITTEN
// EXTENT is a LOWER BOUND on a class size, never the size itself: trailing
// members a default ctor leaves alone are invisible to it. Nothing in this tree
// allocates a `UI::Dialog` (retail's are stack temporaries inside the DLL and
// heap objects the DLL's own factory makes), so any number here would still be
// a guess dressed up as a fact.
//
// The ONE FOUR-BYTE HOLE: +0x44..+0x47. 0x180112e0b is a DWORD store to +0x40
// and the next store is a QWORD at +0x48, so retail never writes +0x44 in the
// ctor. `runtime/src/ui/dialog_ctor_dtor.cpp`'s oracle excludes those four
// bytes from its compare and says so; a port that zeroed +0x40 as a qword
// would be writing over caller stack retail leaves alone.

}  // namespace RtUI
