// code/Game/Tutorial/tutorialimagetoolbaricon.cpp
// Clean reimplementation of TutorialImageToolbarIcon vtable methods.
//
// TutorialImageToolbarIcon (0x48 bytes) -- toolbar icon image display.
// Vtable: 0x140160b90, extends TutorialImage.
//
// Fields (from Ghidra decompilation):
//   +0x00: vtable ptr
//   +0x08: External base (0x18 bytes)
//   +0x20: ExternalPtr (image resource ref)
//   +0x28: int32 (serialized)
//   +0x2C: int32 (serialized)
//   +0x30: System::String (toolbar name, serialized)
//   +0x38: int32 string tag / ExternalFactory version (-1 init)
//   +0x40: void* resolved Toolbar ptr (cached)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialImageToolbarIcon.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "Tutorial/exe_bodies.h"
#include "object/TypeInfo.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Dialog.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"        // ENDGAME B.3.6 UI callers (K4)
#include "ui/Sprite.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K4)
#include "Component/ToolbarDescription.h"

// ============================================================
// Exe helpers, resolved PER CALL (ENDGAME B.3.8, B6-5). The External ctor
// dispatch slot and the two vtable addresses this file cached lazily are
// gone: the compiler runs External() and stamps both tables.
// ============================================================

// TutorialImage_sub_7ad0 -- ExternalPtr serialize helper (exe 0x140027ad0)
// called by name: TutBase_SerializeImageRef (Tutorial/exe_bodies.h)

// FUN_140027de0 -- resolves toolbar from String at +0x30
// called by name: TutBase_ResolveToolbarRef (Tutorial/exe_bodies.h)

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400e0c30: External(), TutorialImage's stamp + zeroed fields
// (TutorialImage()), the own stamp, then String +0x30 = 0, tag -1, ref null.
TutorialImageToolbarIcon::TutorialImageToolbarIcon()
    : _toolbarString(nullptr), _toolbarStringTag(-1), _pad3C{}, _toolbarRef(nullptr) {}

// The dtor inside [0] 0x140028090: ~String +0x30; the compiler re-stamps
// TutorialImage and runs ~External.
TutorialImageToolbarIcon::~TutorialImageToolbarIcon() {
    reinterpret_cast<System::String*>(&_toolbarString)->~String();   // B.3.1 bridge
}

// ============================================================
// Slot5: Serialize (0x1400e0af0)
// ============================================================
bool TutorialImageToolbarIcon::Serialize(void* stream) {
    auto p = reinterpret_cast<uint8_t*>(this);

    // First serializer block: serialize +0x28, +0x2C as ints, then ExternalPtr at +0x20
    alignas(8) uint8_t serializerBuf[32] = {};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(p + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(p + 0x2C), nullptr);   // B.3.4 bridge

    TutBase_SerializeImageRef(p + 0x20, serializerBuf);

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge

    // Second serializer block: Filename ctor, String::Serialize, conditional toolbar resolve
    alignas(8) uint8_t serializerBuf2[32] = {};
    ::new (static_cast<void*>(serializerBuf2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Filename temp retail's compiler emitted for this scope.
    System::Filename filenameBuf;
    (void)filenameBuf;

    // String::Serialize on +0x30
    reinterpret_cast<System::String*>(p + 0x30)->Serialize(serializerBuf2);   // B.3.1 bridge

    // If loading (serializer stream mode == 0), resolve toolbar
    void* innerStream = *(void**)(serializerBuf2 + 0x00);
    if (innerStream) {
        int streamMode = static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf2)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        if (streamMode == 0) {
            TutBase_ResolveToolbarRef(p + 0x30);
        }
    }

    reinterpret_cast<IO::Serializer*>(serializerBuf2)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// Slot6: Reconstruct (0x140028070) / Slot8: Construct (0x140028060)
// ============================================================
Object::External* TutorialImageToolbarIcon::Reconstruct() {
    this->~TutorialImageToolbarIcon();
    return ::new (static_cast<void*>(this)) TutorialImageToolbarIcon();
}
Object::External* TutorialImageToolbarIcon::Construct() {
    return ::new (static_cast<void*>(this)) TutorialImageToolbarIcon();
}

// ============================================================
// Slot9: Slot9/Update (0x1400e0940)
// ============================================================
// Creates a UI element from imageRef, finds an ImageElement child,
// resolves toolbar ref, constructs a Sprite from the toolbar icon data,
// and sets the sprite on the ImageElement.
//
// If field40->+0x58 is null (no component list), uses field40->+0x28
// (spriteSheet) and field40->+0x38 (spriteName) directly.
// If non-null, iterates the component list to find a matching
// ToolbarDescription and uses its +0x20/+0x28.
// Returns retIBP, exactly as retail does (every TutorialImage*::Slot9 ends
// `return param_2`). This is NOT cosmetic: TutWave_RefreshStepUI feeds the
// return value straight to TableElement::SetChild as the child pointer, so a
// void return leaves whatever the last call left in RAX and the table gets a
// garbage child, which faults later inside UI::UserInterface::Update.
void* TutorialImageToolbarIcon::CreateElement(void* retIBP, void* dialog) {
    auto p = reinterpret_cast<uint8_t*>(this);

    // 1. Create UI element from imageRef at +0x20
    ::new (retIBP) Object::InternalPtr<UI::Element>(
        reinterpret_cast<const UI::Dialog*>(dialog)->CreateElement(  // B.3.6 bridge
            Object::ExternalPtr<UI::ElementDescription>(
                reinterpret_cast<UI::ElementDescription*>(*(void**)(p + 0x20)))));

    // 2. CheckPtr on the returned IBP

    // 3. FindElementType — find ImageElement child
    void* element = *(void**)retIBP;
    uint8_t foundIBP[16] = {};
    ::new (foundIBP) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::Element*>(element)->FindElementType(UI::ImageElement::TypeId()));  // B.3.6 bridge

    // 4. CheckPtr on found element

    // 5. IsDerivedFrom check — verify it's an ImageElement
    void* foundObj = *(void**)foundIBP;
    void* imageElement = nullptr;
    if (foundObj) {
        if (Object::IsDerivedFromNullSafe(foundObj, UI::ImageElement::TypeId()))
            imageElement = foundObj;
    }

    // 6. Wrap in InternalBasePtr
    void* imageRef = nullptr;
    new (&imageRef) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(imageElement));   // B.3.3 bridge

    // 7. Resolve toolbar ref from String at +0x30
    TutBase_ResolveToolbarRef(p + 0x30);

    // 8. Get resolved toolbar object at +0x40
    uintptr_t toolbar = *(uintptr_t*)(p + 0x40);
    uint64_t spriteSheet = 0;
    const void* spriteName = nullptr;

    if (toolbar) {
        uintptr_t componentList = *(uintptr_t*)(toolbar + 0x58);

        if (componentList == 0) {
            // No component list — use toolbar's direct sprite data
            TutBase_ResolveToolbarRef(p + 0x30);
            TutBase_ResolveToolbarRef(p + 0x30);

            toolbar = *(uintptr_t*)(p + 0x40); // re-read after resolve
            spriteSheet = *(uint64_t*)(toolbar + 0x28);
            spriteName = (const void*)(toolbar + 0x38);
        } else {
            // Has component list — iterate to find ToolbarDescription
            TutBase_ResolveToolbarRef(p + 0x30);

            componentList = *(uintptr_t*)(toolbar + 0x58);
            int count = *(int*)(componentList + 0x30);
            uintptr_t arrayData = *(uintptr_t*)(componentList + 0x28);

            void* matchedComp = nullptr;
            for (int i = 0; i < count; i++) {
                void* comp = *(void**)(arrayData + i * 8);
                if (!comp) continue;

                // Call GetTypeId (vtable +0x10)
                uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

                if (typeId == ToolbarDescription::TypeId()) {
                    // IsDerivedFrom check
                    if (Object::IsDerivedFromNullSafe(comp, ToolbarDescription::TypeId())) {
                        matchedComp = comp;
                    }
                    break;
                }
            }

            if (matchedComp) {
                spriteSheet = *(uint64_t*)((uintptr_t)matchedComp + 0x20);
                spriteName = (const void*)((uintptr_t)matchedComp + 0x28);
            } else {
                // Fallback: use toolbar's direct data (matchedComp stays null)
                spriteSheet = *(uint64_t*)(toolbar + 0x28);
                spriteName = (const void*)(toolbar + 0x38);
            }
        }
    }

    // 9. Construct Sprite from spriteSheet + spriteName
    uint8_t spriteBuf[16] = {};
    if (spriteName)
        ::new (spriteBuf) UI::Sprite(
            Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(spriteSheet)),  // B.3.6 bridge
            *static_cast<const System::String*>(spriteName));


    // 10. Set sprite on the ImageElement
    if (imageRef)
        reinterpret_cast<UI::ImageElement*>(imageRef)->SetSprite(  // B.3.6 bridge
            reinterpret_cast<const UI::Sprite*>(spriteBuf));
    return retIBP;
}

// create 0x140028020: MemAlloc(0x48, 8), then the ctor.
void* TutorialImageToolbarIcon::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialImageToolbarIcon), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialImageToolbarIcon());
}
// createInPlace 0x140028060.
void* TutorialImageToolbarIcon::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialImageToolbarIcon());
}

DEFINE_EXE_TYPE(TutorialImageToolbarIcon, "TutorialImage")
