# Runtime DLL Decompilation Goals

Tracking Runtime-x64.dll functions that need decompilation for current work.

## Infrastructure Status

- `runtime/types/` — binary-compatible struct headers (common.h, video/ImageBuffer.h exist)
- `runtime/funcs/runtime_funcs.{h,cpp}` — internal function pointer resolution (placeholder, calls work)
- `runtime/src/<namespace>/` — clean reimpl source dirs (16 namespaces, all empty)
- `runtime/reimpl/` — hook wrappers (empty)
- **Makefile** — build rules for all runtime dirs already in place, include paths set (`-Iruntime/types -Iruntime/funcs`)

No additional toolchain or build setup needed. Files placed in `runtime/src/ui/` will compile automatically.

---

## Priority: UI::Sprite Pipeline

**Context:** BuildRequirementUI::Slot4 reimpl no longer crashes (array push fix), but
resource icons/sprites don't display. All function pointers resolve, all component
lookups succeed, all element pointers valid. Need to understand the sprite pipeline
to find what's wrong.

### 1. UI::Sprite Constructors

| Address | Name | Notes |
|---------|------|-------|
| `0x18011b9c0` | `UI::Sprite::Sprite` | 3 xrefs — likely the main ctor (ExternalPtr<SpriteSheet> + String& name) |
| `0x18011ba10` | `UI::Sprite::Sprite` | Overload (copy ctor?) |
| `0x18011ba30` | `UI::Sprite::Sprite` | Overload (default ctor?) |

**Goal:** Understand Sprite struct layout (16 bytes? 24 bytes?), what fields it stores,
and how it resolves the sprite name to an index. Does it call `GetSpriteIndexFromName`?
Does it fail silently if the name is empty/invalid?

**Export name (main ctor):**
`??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z`

### 2. SetSprite Methods

| Address | Name | Notes |
|---------|------|-------|
| `0x180042980` | `UI::LabelElement::SetSprite` | Used for resource requirement labels |
| `0x180118600` | `UI::ImageElement::SetSprite` | Used for building icon |
| `0x1800429d0` | `UI::LabelElement::GetSprite` | May reveal Sprite storage layout |
| `0x1801182a0` | `UI::ImageElement::GetSprite` | May reveal Sprite storage layout |

**Export names:**
- `?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z`
- `?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z`

**Goal:** Understand how SetSprite stores the sprite data on the element. Does it
copy the 16-byte Sprite struct? Does it trigger a layout/render update? Is there
a "dirty" flag that needs setting?

### 3. Sprite Index Resolution

| Address | Name | Notes |
|---------|------|-------|
| `0x1800031f0` | `GetSpriteIndexFromName` | 47 xrefs — core lookup function |
| `0x1800032d0` | `GetSpriteNameFromIndex` | Reverse lookup |
| `0x180003190` | `GetSpriteCount` | SpriteSheet query |

**Goal:** Understand the name→index mapping. If Sprite ctor calls this and the name
string is wrong, what happens? Returns -1? Returns 0? Crashes?

### 4. SpriteSheet

| Address | Name | Notes |
|---------|------|-------|
| `0x1800035d0` | `UI::SpriteSheet::SpriteSheet` | Constructor, 4 xrefs |
| `0x18011c740` | `SetSpriteSheet` | May show how sheet is stored on elements |

---

## Priority: Component Layout at DAT_1401c7cb8

The component found via type ID at exe address `0x1401c7cb8` is used in both
BuildUI (+0x30 for sprite name) and BuildRequirementUI (+0x38 for sprite name).
Need to determine the actual struct layout.

**Questions:**
- What class does this type ID represent? (labeled `g_TypeId_ResourceType` but
  comment says "NOTE: same as g_TypeId_ResourceType; this is the CitizenComponent")
- What fields are at +0x20, +0x28, +0x30, +0x38?
- Are +0x30 and +0x38 both valid System::String fields?
- Does the original BuildRequirementUI really use +0x38, or is there a register
  tracking subtlety in the disassembly?

**Approach:** Decompile a few xref sites of DAT_1401c7cb8 in the exe to understand
the component's field layout. Also check ToolKit resource files for field definitions.

---

## Type Headers Needed

| Type | Namespace | Est. Size | Notes |
|------|-----------|-----------|-------|
| `Sprite` | `UI` | 16 bytes? | SpriteSheet ref + index? |
| `SpriteSheet` | `UI` | Unknown | Sprite atlas metadata |
| `LabelElement` | `UI` | Unknown | Extends Element, has sprite+text |
| `ImageElement` | `UI` | Unknown | Extends Element, has sprite |

These go in `runtime/types/ui/` once field layouts are confirmed.
