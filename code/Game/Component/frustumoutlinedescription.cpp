// code/Game/Component/frustumoutlinedescription.cpp
// Clean reimplementation of FrustumOutlineDescription (UI element description block).
//
// FrustumOutlineDescription (0x80 bytes) — camera-frustum outline UI element descriptor.
// Extends ElementDescription (Object::External base, 0x20). Vtable at 0x14015c300.
// Parent type "ElementDescription".
//   +0x68: ExternalPtr (sprite sheet)
//   +0x70: String      (sprite name)
//   +0x78: int          (default -1)
//   +0x7c: int
//
// Factory  : MemAlloc 0x80 + ElementDescription ctor + vtable + defaults.
// Serialize (vtable +0x28, 0x14006c460):
//   ElementDescription::Serialize(self, stream)  [base]
//   then: ExternalPtr @ +0x68  +  String @ +0x70  +  int @ +0x78  +  int @ +0x7c
// GetVersion (vtable +0x18, 0x14006c500): ElementDescription::GetFileVersion(self) | 0x100.
// CreateInPlace (vtable +0x48, 0x140012330): ctor + zero +0x68/+0x70 + (+0x78 = -1).
// Dtor (vtable +0x00, 0x140012500): ~String(+0x70) + ElementDescription::~ + free.
//
// Field knowledge harvested from engine/reimpl/frustumoutlinedescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "system/String.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

static const uintptr_t kFrustumOutlineDescriptionVtable = 0x14015c300;

static void FrustumOutlineDescription_Init(void* self) {
    fn_ElementDescription_Ctor(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kFrustumOutlineDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x68) = 0;             // ExternalPtr (sprite sheet)
    *(uint64_t*)(s + 0x70) = 0;             // String (sprite name)
    *(int64_t*) (s + 0x78) = (int64_t)-1;   // int +0x78 = -1 (covers +0x7c too)
}

void FrustumOutlineDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ElementDescription_Serialize || !g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !fn_ExternalPtr_Serialize || !g_fn_Serialize_Int) {
        return;
    }
    fn_ElementDescription_Serialize(self, stream);           // base
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    fn_ExternalPtr_Serialize(s + 0x68, serializer);          // ExternalPtr +0x68
    reinterpret_cast<System::String*>(s + 0x70)->Serialize(serializer);             // B.3.1 bridge: String      +0x70
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x78), nullptr);       // int         +0x78   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x7c), nullptr);       // int         +0x7c   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* FrustumOutlineDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kFrustumOutlineDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    reinterpret_cast<System::String*>(s + 0x70)->~String();            // B.3.1 bridge: String +0x70
    if (fn_ElementDescription_Dtor) fn_ElementDescription_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t FrustumOutlineDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FrustumOutlineDescription");
    return cached;
}

// GetVersion (vtable +0x18): ElementDescription::GetFileVersion() | 0x100.
static uint32_t FrustumOutlineDescription_GetVersion(void* self) {
    uint32_t base = fn_ElementDescription_GetFileVersion
                        ? fn_ElementDescription_GetFileVersion(self) : 0;
    return base | 0x100u;
}

static int   FrustumOutlineDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* FrustumOutlineDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* FrustumOutlineDescription_CreateInPlace(void* memory);

// The FrustumOutlineElement factory (code/Game/Component/frustumoutlineelement.cpp):
// MemAlloc(0xd8) + UI::Element ctor (installs the base Runtime Element vtable) +
// UI::Sprite default-ctor at +0xc0 + *(+0xd0) = -1.
extern "C" void* FrustumOutlineElement_Factory_SA(void);

// ============================================================
// CreateElement (vtable +0x40, exe 0x14006c340)
// ============================================================
// The element-tree build calls the ElementDescription's create-element slot
// (+0x40) to instantiate each element. For the minimap's `frustum` element this
// produces a FrustumOutlineElement; leaving it a ReturnNull stub made building
// `Dialog/Map.rsc:content` (Group{imageMap,imageOverlay,frustum}) yield a null
// child, so the whole content sub-tree was never inserted at `userGroup0`.
//
// Faithful port of FrustumOutlineDescription_CreateElement (0x14006c340):
//   1. Alloc + construct the FrustumOutlineElement.
//   2. Wrap in an InternalBasePtr, run ElementDescription::CreateInstance(desc,&ibp)
//      (applies the generic ElementDescription fields to the new element).
//   3. width  <- desc+0x78  ->  element+0xd0
//      height <- desc+0x7c  ->  element+0xd4
//   4. Set element+0xc0 sprite via Sprite::Sprite(spriteSheet@desc+0x68,
//      name@desc+0x70) — the frustum outline's line sprite (now wired below).
//   5. Return the element pointer through param_2 (RDX out-buffer).
static void* FrustumOutlineDescription_CreateElement(void* self, void** param_2) {
    void* elem = FrustumOutlineElement_Factory_SA();

    uint8_t ibp[16] = {};
    if (fn_InternalBasePtr_Ctor) new (ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
    if (fn_ElementDescription_CreateInstance)
        fn_ElementDescription_CreateInstance(self, ibp);
    void* e = *reinterpret_cast<void**>(ibp);

    if (e) {
        uint8_t* d = reinterpret_cast<uint8_t*>(self);
        uint8_t* el = reinterpret_cast<uint8_t*>(e);
        // Build the outline's line Sprite from the description's spriteSheet
        // (+0x68 ExternalPtr) + name (+0x70 String) and store at element+0xc0.
        // The factory installed a default (spriteless) Sprite, and
        // SpriteContext::AddLine no-ops when the sprite's spritesheet is null —
        // so without this the frustum outline never draws. Guard on a non-null
        // spriteSheet so a Map.rsc that omits the sprite stays crash-safe.
        uint64_t spriteSheet = *reinterpret_cast<uint64_t*>(d + 0x68);
        if (spriteSheet && fn_Sprite_Sprite) {
            uint64_t spriteRet[2] = {0, 0};
            fn_Sprite_Sprite(spriteRet, spriteSheet, d + 0x70);
            *reinterpret_cast<uint64_t*>(el + 0xc0) = spriteRet[0];
            *reinterpret_cast<uint64_t*>(el + 0xc8) = spriteRet[1];
        }
        *reinterpret_cast<int*>(el + 0xd0) = *reinterpret_cast<int*>(d + 0x78);  // width
        *reinterpret_cast<int*>(el + 0xd4) = *reinterpret_cast<int*>(d + 0x7c);  // height
    }
    if (param_2) *param_2 = e;
    return param_2;
}

static void* g_FrustumOutlineDescription_Vtable[11] = {
    reinterpret_cast<void*>(&FrustumOutlineDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&FrustumOutlineDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&FrustumOutlineDescription_GetVersion),         // +0x18 GetVersion
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_Return0),   // +0x20 thunk
    reinterpret_cast<void*>(&FrustumOutlineDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_Return0),   // +0x38 thunk
    reinterpret_cast<void*>(&FrustumOutlineDescription_CreateElement),     // +0x40 CreateElement
    reinterpret_cast<void*>(&FrustumOutlineDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

// Defined in frustumoutlineelement.cpp — the port of the frustum ground-corner
// projection (exe 0x14006c5b0). Registering it makes fn_MapUI_ProjectCornersToGround
// / fn_FrustumProject resolve, which also enables minimap click-to-move-camera
// (MapUI_HandleClick early-returns while that pointer is null).
extern "C" void FrustumOutline_ProjectCorners(float* out);

extern "C" void RegisterFrustumOutlineDescriptionVtable() {
    RegisterExeAddr(kFrustumOutlineDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_FrustumOutlineDescription_Vtable));
    RegisterExeAddr(0x14006c5b0,
                    reinterpret_cast<uintptr_t>(&FrustumOutline_ProjectCorners));
}

extern "C" void* FrustumOutlineDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x80, 8);
    if (!p) return nullptr;
    FrustumOutlineDescription_Init(p);
    return p;
}

static void* FrustumOutlineDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    FrustumOutlineDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(FrustumOutlineDescription, "ElementDescription",
              &FrustumOutlineDescription_Factory, &FrustumOutlineDescription_CreateInPlace);

