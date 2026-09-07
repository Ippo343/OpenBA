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
// and whose cursor advances by 0x20 per iteration. The argument order is not a
// transcription slip: the config ExternalPtr at +0x08 is passed AFTER the
// element-description ExternalPtr at +0x10, because the six-argument
// `AddController` overload takes (typeId, dataSource, config, elementDesc,
// name) and MSVC's mangled name spells that order.
struct DialogControllerConfigEntry {
    uint32_t _typeId;        // +0x00
    uint32_t _pad04;         // +0x04  never read
    void*    _config;        // +0x08  ExternalPtr<DialogControllerConfig>
    void*    _elementDesc;   // +0x10  ExternalPtr<ElementDescription>
    char*    _name;          // +0x18  System::String (refcounted char*)
};
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

// NO sizeof ASSERT, deliberately: nothing in this lane allocates a Dialog and
// nothing reads past +0x7f, so any number here would be a guess dressed up as
// a fact.

}  // namespace RtUI
