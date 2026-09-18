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

#include "Tutorial/tutorialimagetoolbaricon_funcs.h"
#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Vtable pointers (resolved once)
// ============================================================
static void* s_vtable_TutorialImageToolbarIcon = nullptr;
static void* s_vtable_TutorialImage = nullptr;

static void EnsureVtables() {
    if (!s_vtable_TutorialImageToolbarIcon) {
        s_vtable_TutorialImageToolbarIcon = (void*)ResolveAddr(Module::Exe, TutorialImageToolbarIconAddr::Vtable);
        s_vtable_TutorialImage = (void*)ResolveAddr(Module::Exe, TutorialImageAddr::Vtable);
    }
}

// ============================================================
// Local function pointers (resolved once)
// ============================================================

// External::External() ctor -- dispatch slot 0x140151e98
typedef void (*fn_External_Ctor_t)(void* self);
static fn_External_Ctor_t s_fn_External_Ctor = nullptr;

// TutorialImage_sub_7ad0 -- ExternalPtr serialize helper (exe function)
typedef void (*fn_ExternalPtr_SerializeImage_t)(void* externalPtrField, void* serializer);
static fn_ExternalPtr_SerializeImage_t s_fn_ExternalPtr_SerializeImage = nullptr;

// FUN_140027de0 -- resolves toolbar from String at +0x30
typedef void (*fn_ResolveToolbar_t)(void* stringField);
static fn_ResolveToolbar_t s_fn_ResolveToolbar = nullptr;

static void EnsureFuncs() {
    if (!s_fn_External_Ctor) {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151e98));
        if (dispatch && *dispatch)
            s_fn_External_Ctor = reinterpret_cast<fn_External_Ctor_t>(*dispatch);
    }
    if (!s_fn_ExternalPtr_SerializeImage) {
        s_fn_ExternalPtr_SerializeImage = reinterpret_cast<fn_ExternalPtr_SerializeImage_t>(
            ResolveAddr(Module::Exe, 0x140027ad0));
    }
    if (!s_fn_ResolveToolbar) {
        s_fn_ResolveToolbar = reinterpret_cast<fn_ResolveToolbar_t>(
            ResolveAddr(Module::Exe, 0x140027de0));
    }
}

// ============================================================
// Slot0: Dtor (0x140028090)
// ============================================================
void* TutorialImageToolbarIcon_Dtor(void* self, uint32_t flags) {
    EnsureVtables();
    auto p = reinterpret_cast<uint8_t*>(self);

    // Set own vtable
    *(void**)p = s_vtable_TutorialImageToolbarIcon;

    // Destroy String at +0x30 (B.3.1 bridge: the object is still untyped bytes)
    reinterpret_cast<System::String*>(p + 0x30)->~String();

    // Set base vtable
    *(void**)p = s_vtable_TutorialImage;

    // Call External::~External
    if (fn_External_Dtor)
        fn_External_Dtor(self);

    // Conditional free
    if (flags & 1) {
        if (fn_MemFreeStatic)
            fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// Slot5: Serialize (0x1400e0af0)
// ============================================================
void TutorialImageToolbarIcon_Serialize(void* self, void* stream) {
    EnsureFuncs();
    auto p = reinterpret_cast<uint8_t*>(self);

    // First serializer block: serialize +0x28, +0x2C as ints, then ExternalPtr at +0x20
    uint8_t serializerBuf[32] = {};
    if (g_fn_Serializer_Ctor)
        g_fn_Serializer_Ctor(serializerBuf, stream, 0);

    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializerBuf, p + 0x28, nullptr);
        g_fn_Serialize_Int(serializerBuf, p + 0x2C, nullptr);
    }

    if (s_fn_ExternalPtr_SerializeImage)
        s_fn_ExternalPtr_SerializeImage(p + 0x20, serializerBuf);

    if (g_fn_Serializer_Dtor)
        g_fn_Serializer_Dtor(serializerBuf);

    // Second serializer block: Filename ctor, String::Serialize, conditional toolbar resolve
    uint8_t serializerBuf2[32] = {};
    if (g_fn_Serializer_Ctor)
        g_fn_Serializer_Ctor(serializerBuf2, stream, 0);

    // Filename temp retail's compiler emitted for this scope.
    System::Filename filenameBuf;
    (void)filenameBuf;

    // String::Serialize on +0x30
    reinterpret_cast<System::String*>(p + 0x30)->Serialize(serializerBuf2);

    // If loading (serializer stream mode == 0), resolve toolbar
    void* innerStream = *(void**)(serializerBuf2 + 0x00);
    if (innerStream) {
        int streamMode = *(int*)(reinterpret_cast<uint8_t*>(innerStream) + 0x08);
        if (streamMode == 0) {
            // Resolve toolbar from the string
            if (s_fn_ResolveToolbar)
                s_fn_ResolveToolbar(p + 0x30);
        }
    }

    if (g_fn_Serializer_Dtor)
        g_fn_Serializer_Dtor(serializerBuf2);
}

// ============================================================
// Slot6: Cleanup (0x140028070)
// ============================================================
void TutorialImageToolbarIcon_Cleanup(void* self) {
    // Call own Dtor with flags=0 via vtable slot 0
    void** vtable = *(void***)self;
    typedef void* (*fn_ScalarDtor_t)(void*, uint32_t);
    fn_ScalarDtor_t scalarDtor = reinterpret_cast<fn_ScalarDtor_t>(vtable[0]);
    scalarDtor(self, 0);

    // Reinitialize via real Ctor
    TutorialImageToolbarIcon_Ctor(self);
}

// ============================================================
// Slot8: Ctor/Init (real: 0x1400e0c30)
// ============================================================
void* TutorialImageToolbarIcon_Ctor(void* self) {
    EnsureVtables();
    EnsureFuncs();

    auto p = reinterpret_cast<uint8_t*>(self);

    // Call External::External ctor
    if (s_fn_External_Ctor)
        s_fn_External_Ctor(self);

    // Set base vtable, zero base fields
    *(void**)p = s_vtable_TutorialImage;
    *(uint64_t*)(p + 0x20) = 0;
    *(uint64_t*)(p + 0x28) = 0;

    // Set own vtable
    *(void**)p = s_vtable_TutorialImageToolbarIcon;

    // Initialize String at +0x30 (zero), tag at +0x38 (-1), toolbar ref at +0x40 (null)
    *(uint64_t*)(p + 0x30) = 0;
    *(int32_t*)(p + 0x38) = -1;
    *(uint64_t*)(p + 0x40) = 0;

    return self;
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
void* TutorialImageToolbarIcon_Slot9(void* self, void* retIBP, void* dialog) {
    EnsureFuncs();
    auto p = reinterpret_cast<uint8_t*>(self);

    // 1. Create UI element from imageRef at +0x20
    if (fn_Dialog_CreateElement)
        fn_Dialog_CreateElement(dialog, retIBP, *(void**)(p + 0x20));

    // 2. CheckPtr on the returned IBP
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(retIBP);

    // 3. FindElementType — find ImageElement child
    void* element = *(void**)retIBP;
    uint8_t foundIBP[16] = {};
    if (fn_Element_FindElementType && g_TypeId_ImageElement)
        fn_Element_FindElementType(element, foundIBP, *g_TypeId_ImageElement);

    // 4. CheckPtr on found element
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(foundIBP);

    // 5. IsDerivedFrom check — verify it's an ImageElement
    void* foundObj = *(void**)foundIBP;
    void* imageElement = nullptr;
    if (fn_IsDerivedFrom && g_TypeId_ImageElement && foundObj) {
        if (fn_IsDerivedFrom(foundObj, *g_TypeId_ImageElement))
            imageElement = foundObj;
    }

    // 6. Wrap in InternalBasePtr
    void* imageRef = nullptr;
    if (fn_InternalBasePtr_Ctor)
        new (&imageRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(imageElement));   // B.3.3 bridge

    // 7. Resolve toolbar ref from String at +0x30
    if (s_fn_ResolveToolbar)
        s_fn_ResolveToolbar(p + 0x30);

    // 8. Get resolved toolbar object at +0x40
    uintptr_t toolbar = *(uintptr_t*)(p + 0x40);
    uint64_t spriteSheet = 0;
    const void* spriteName = nullptr;

    if (toolbar) {
        uintptr_t componentList = *(uintptr_t*)(toolbar + 0x58);

        if (componentList == 0) {
            // No component list — use toolbar's direct sprite data
            if (s_fn_ResolveToolbar)
                s_fn_ResolveToolbar(p + 0x30);
            if (s_fn_ResolveToolbar)
                s_fn_ResolveToolbar(p + 0x30);

            toolbar = *(uintptr_t*)(p + 0x40); // re-read after resolve
            spriteSheet = *(uint64_t*)(toolbar + 0x28);
            spriteName = (const void*)(toolbar + 0x38);
        } else {
            // Has component list — iterate to find ToolbarDescription
            if (s_fn_ResolveToolbar)
                s_fn_ResolveToolbar(p + 0x30);

            componentList = *(uintptr_t*)(toolbar + 0x58);
            int count = *(int*)(componentList + 0x30);
            uintptr_t arrayData = *(uintptr_t*)(componentList + 0x28);

            void* matchedComp = nullptr;
            for (int i = 0; i < count; i++) {
                void* comp = *(void**)(arrayData + i * 8);
                if (!comp) continue;

                // Call GetTypeId (vtable +0x10)
                uintptr_t vtbl = *(uintptr_t*)comp;
                typedef uint32_t (*fn_GetTypeId_t)(void*);
                fn_GetTypeId_t getTypeId = (fn_GetTypeId_t)(*(void**)(vtbl + 0x10));
                uint32_t typeId = getTypeId(comp);

                if (g_TypeId_ToolbarDescription_FC &&
                    typeId == *g_TypeId_ToolbarDescription_FC) {
                    // IsDerivedFrom check
                    if (fn_IsDerivedFrom &&
                        fn_IsDerivedFrom(comp, *g_TypeId_ToolbarDescription_FC)) {
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
    if (fn_Sprite_Sprite && spriteName)
        fn_Sprite_Sprite(spriteBuf, spriteSheet, spriteName);


    // 10. Set sprite on the ImageElement
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&imageRef);
    if (fn_ImageElement_SetSprite && imageRef)
        fn_ImageElement_SetSprite(imageRef, spriteBuf);
    return retIBP;
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160b90 (9 slots, Object::External base), size 0x48, parent "TutorialImage".
// GetFileVersion (+0x18) = 0x1001. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialImageToolbarIcon_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialImageToolbarIcon");
    return c;
}
static int TutorialImageToolbarIcon_GetFileVersion(void*) { return 0x1001; }

static void* g_TutorialImageToolbarIcon_Vtable[10] = {
    (void*)&TutorialImageToolbarIcon_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialImageToolbarIcon_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialImageToolbarIcon_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialImageToolbarIcon_Serialize,       // +0x28 Serialize
    (void*)&TutorialImageToolbarIcon_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialImageToolbarIcon_Ctor,   // +0x40 Ctor/CreateInPlace (0x140028060)
    (void*)&TutorialImageToolbarIcon_Slot9,   // +0x48 BuildElement (0x1400e0940)
};

extern "C" void RegisterTutorialImageToolbarIconVtable() {
    RegisterExeAddr(0x140160b90, (uintptr_t)g_TutorialImageToolbarIcon_Vtable);
}
extern "C" void* TutorialImageToolbarIcon_CreateInPlace(void* mem) {
    return TutorialImageToolbarIcon_Ctor(mem);
}
extern "C" void* TutorialImageToolbarIcon_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    return p ? TutorialImageToolbarIcon_Ctor(p) : nullptr;
}
REGISTER_TYPE(TutorialImageToolbarIcon, "TutorialImage", &TutorialImageToolbarIcon_Factory, &TutorialImageToolbarIcon_CreateInPlace);
