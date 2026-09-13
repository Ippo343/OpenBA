// ============================================================================
// runtime/src/ui/spritecapture_class.cpp — UI::SpriteCapture, the class surface
// ============================================================================
//
// runtime/src/ui/spritecapture.cpp (Wave 10 lane UI4) ported the RECORDING
// half of this class — AddSprite, AddScissor, BeginAddingSprites,
// EndAddingSprites — and runtime/types/ui/SpriteCapture.h carries the layout it
// recovered. This file is the rest: the nine-slot vtable, the destructor, and
// the PLAYBACK body that turns a recording back into draw calls.
//
// ??_7SpriteCapture@UI@@6B@ (retail 0x1801566a0):
//   0  deleting dtor      0x18003ccf0  deleting_dtor.cpp row (~Class 0x1800d91b0,
//                                      sizeof 0x58 — both read off its own bytes)
//   1  TypeName           0x18003c810  here
//   2  Type               0x18003c7f0  here
//   3  GetFileVersion     0x1800d9520  here  (returns 1)
//   4  External::Compile  0x1800066e0  already ported (the base's slot)
//   5  Serialize          0x1800d9530  here
//   6  Reconstruct        0x18003ca40  here
//   7  Destruct           0x18003cb40  here
//   8  Construct          0x18003cac0  here
// plus ??1SpriteCapture@UI@@UEAA@XZ       0x1800d91b0  here
//  and ?Draw@SpriteCapture@UI@@QEBAXAEAVSpriteContext@2@HH@Z 0x1800d9230  here.
//
// The CONSTRUCTOR is already ported and registered, table-driven, by
// runtime/src/ui/ui_factory_wave28.cpp (`Ctor_SpriteCapture`, twelve stores,
// `??0SpriteCapture@UI@@QEAA@XZ` 0x1800d9140). Construct and Reconstruct below
// INLINE the same twelve stores rather than calling it — that is what retail
// does (0x18003cad6..0x18003cb26 is straight-line code, not a CALL), so the
// stores are transcribed again here.
//
// THREE ARRAYS, THREE DIFFERENT DESTRUCTORS. ~SpriteCapture destroys
// `_sprites` (+0x40) with 0x180037310, `_entries` (+0x30) with 0x18003e4a0 and
// `_sheets` (+0x20) with 0x18003da00, IN THAT ORDER — reverse declaration
// order, as a C++ destructor must. They are three DIFFERENT functions, not one
// called three times: `_entries` holds a System::String per element and
// `_sheets` holds an ExternalPtr per element, so each walk destroys a different
// element type before freeing the buffer. Only 0x180037310 is ported in our
// tree today (runtime/src/collection/array.cpp); the other two are reached
// through ResolveAddr, which is registry-first, and are listed as open
// dependencies rather than silently collapsed into the one we have.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/SpriteCapture.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()          { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()      { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtSpriteCapture() { return RT_DATA_SLOT(void*, "??_7SpriteCapture@UI@@6B@"); }

const uint8_t* SpriteCaptureTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@SpriteCapture@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;
    int32_t _pad0c;
    void*   encodingFactory;
};
struct Serializer {
    Stream* stream;
    void*   readerWriter;
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

using Slot0Fn = void* (*)(void* self, unsigned flag);
void CallSlot0(void* self, unsigned flag) {
    reinterpret_cast<Slot0Fn>(VTableOf(self)[0])(self, flag);
}

// The twelve stores retail inlines into Construct (0x18003cad6..0x18003cb26)
// and, identically, into Reconstruct (0x18003ca5e..0x18003caae).
void InlineCtorStores(uint8_t* p) {
    *reinterpret_cast<void**>(p)        = VtBase();          // 18003cad6
    *reinterpret_cast<void**>(p)        = VtExternal();      // 18003cae0
    *reinterpret_cast<void**>(p + 0x08) = nullptr;           // 18003caf5
    *reinterpret_cast<void**>(p + 0x10) = nullptr;           // 18003caf8
    *reinterpret_cast<void**>(p + 0x18) = nullptr;           // 18003cafc
    *reinterpret_cast<void**>(p)        = VtSpriteCapture(); // 18003cb00
    *reinterpret_cast<void**>(p + 0x20) = nullptr;           // 18003cb0a
    *reinterpret_cast<void**>(p + 0x28) = nullptr;           // 18003cb0e
    *reinterpret_cast<void**>(p + 0x30) = nullptr;           // 18003cb12
    *reinterpret_cast<void**>(p + 0x38) = nullptr;           // 18003cb16
    *reinterpret_cast<void**>(p + 0x40) = nullptr;           // 18003cb1a
    *reinterpret_cast<void**>(p + 0x48) = nullptr;           // 18003cb1e
    *reinterpret_cast<void**>(p + 0x50) = nullptr;           // 18003cb22
}

// The int32 slot every scalar goes through. See the QWORD/DWORD note on Draw's
// sibling in runtime/src/ui/animatedimagedescription.cpp.
using ScalarFn = void (*)(void* rw, void* value, const char* name);

}  // namespace

// 0x18003c7f0 / 0x18003c810: the class's own BaseTypeInfo node at +0 and +8.
extern "C" uint32_t RtUi_SpriteCapture_Type(const void*) {
    const uint8_t* ti = SpriteCaptureTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_SpriteCapture_TypeName(const void*) {
    const uint8_t* ti = SpriteCaptureTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1800d9520: `MOV EAX,0x1 ; RET`.
extern "C" int32_t RtUi_SpriteCapture_GetFileVersion(const void*) { return 1; }

// 0x18003cac0
extern "C" void* RtUi_SpriteCapture_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                    // 18003cad1 TEST/JZ
    InlineCtorStores(reinterpret_cast<uint8_t*>(selfv));
    return selfv;                                            // 18003cb26
}
// 0x18003ca40: slot0(this, 0), then the same inlined constructor, open-coded.
extern "C" void* RtUi_SpriteCapture_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                     // 18003ca57
    InlineCtorStores(reinterpret_cast<uint8_t*>(selfv));
    return selfv;                                            // 18003caae
}
// 0x18003cb40: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
extern "C" void RtUi_SpriteCapture_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1SpriteCapture@UI@@UEAA@XZ   0x1800d91b0
// ============================================================================
extern "C" void RtUi_SpriteCapture_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(p) = VtSpriteCapture();        // 1800d91cd

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180037310ull)(p + 0x40);           // 1800d91d4  ~Array<Sprite>
    RtAddr<ArrayDtorFn>(0x18003e4a0ull)(p + 0x30);           // 1800d91de  ~Array<CaptureEntry>
    RtAddr<ArrayDtorFn>(0x18003da00ull)(p + 0x20);           // 1800d91e8  ~Array<ExternalPtr<SpriteSheet>>

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d91f6 JMP
}

// ============================================================================
// ?Serialize@SpriteCapture@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d9530
// ============================================================================
//   rw = encoding factory, read (+0x18) or write (+0x20) side by stream->mode
//   n = _sheets._size          rw->kScalarInt(&n)
//   if (read) Array<ExternalPtr<SpriteSheet>>::SetSize(&_sheets, n)   0x18003ea50
//   element walk (&ser, n, _sheets._data)                             0x18003eb60
//   rw->kScalarInt(&_width)    rw->kScalarInt(&_height)
//   n = _entries._size         rw->kScalarInt(&n)
//   if (read) Array<CaptureEntry>::SetSize(&_entries, n)              0x18004ba90
//   element walk (&ser, n, _entries._data)                            0x18004c0d0
//   if (read) REBUILD _sprites from _entries + _sheets                (below)
//   if (rw) rw->vt[0](rw, 1)
//   return true
//
// ONE SCRATCH SLOT, TWO COUNTS. Retail serialises both array counts through
// the SAME stack dword ([RBP+0x30]); the second write overwrites the first, and
// nothing reads the first afterwards. Reproduced with one local for the same
// reason: a second local would be a different program with the same output
// today and a different one the moment the order changes.
//
// ---------------------------------------------------------------------------
// THE REBUILD, and the two QWORD-FROM-A-DWORD-SLOT stores in it
// ---------------------------------------------------------------------------
// `_sprites` is NOT serialised. On the read side retail reconstructs it from
// the two streams that are: for each entry, the sheet it names and the sprite
// index that entry's `name` resolves to inside that sheet. The loop bound is
// `_sprites._size` AFTER the SetSize, not `_entries._size` — the same number,
// re-read from the array (0x1800d96d0).
//
// 0x1800d966b and 0x1800d96a8 write a DWORD to two DIFFERENT uninitialised
// stack slots; 0x1800d967a and 0x1800d96bc read all EIGHT bytes back and
// 0x1800d96c0 stores all eight into `sprite.index`. The WIDTH is the faithful
// part and is reproduced; the upper four bytes are whatever the frame held,
// exactly as in AnimatedImageDescription::CreateInstance.
extern "C" bool RtUi_SpriteCapture_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };                    // [RBP-0x38], [RBP-0x30]

    void*  factory = stream->encodingFactory;                // 1800d9567
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;     // 1800d9571 CMP / JNZ
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                   // 1800d9582

    void** rvt = VTableOf(rw);
    ScalarFn scalar = reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8]);

    int32_t n = *reinterpret_cast<int32_t*>(p + 0x28);       // 1800d9586  _sheets._size
    scalar(rw, &n, nullptr);                                 // 1800d959b

    using SetSizeFn = void (*)(void* array, int n);
    using WalkFn    = void (*)(Serializer* ser, int count, void* data);

    if (ser.stream->mode == 0) {                             // 1800d95a5 (re-read)
        RtAddr<SetSizeFn>(0x18003ea50ull)(p + 0x20, n);      // 1800d95b2
    }
    RtAddr<WalkFn>(0x18003eb60ull)(&ser, n, *reinterpret_cast<void**>(p + 0x20));

    scalar(rw, p + 0x50, nullptr);                           // 1800d95d5  _width
    scalar(rw, p + 0x54, nullptr);                           // 1800d95e9  _height

    n = *reinterpret_cast<int32_t*>(p + 0x38);               // 1800d95ef  _entries._size
    scalar(rw, &n, nullptr);                                 // 1800d9604

    if (ser.stream->mode == 0) {                             // 1800d960e (re-read)
        RtAddr<SetSizeFn>(0x18004ba90ull)(p + 0x30, n);      // 1800d961b
    }
    RtAddr<WalkFn>(0x18004c0d0ull)(&ser, n, *reinterpret_cast<void**>(p + 0x30));

    if (stream->mode == 0) {                                 // 1800d9630  the ORIGINAL stream
        RtAddr<SetSizeFn>(0x1800370c0ull)(
            p + 0x40, *reinterpret_cast<int32_t*>(p + 0x38));         // 1800d9642

        using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
        for (int32_t i = 0; i < *reinterpret_cast<int32_t*>(p + 0x48); ++i) {
            uint8_t* entries = *reinterpret_cast<uint8_t**>(p + 0x30);   // 1800d9660
            const int32_t sheetIndex =
                *reinterpret_cast<int32_t*>(entries + static_cast<size_t>(i) * 0x28);

            uint64_t idxSlot;                                // see the banner
            void*    sheet;
            uint8_t* sprites;
            if (sheetIndex < 0) {                            // 1800d9664 CMP/JGE
                const int32_t minusOne = -1;
                __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);  // 1800d966b
                sprites = *reinterpret_cast<uint8_t**>(p + 0x40);        // 1800d9672
                sheet   = nullptr;                                       // 1800d9676 (RDI==0)
            } else {
                sheet = reinterpret_cast<void* const*>(
                    *reinterpret_cast<uint8_t**>(p + 0x20))[sheetIndex]; // 1800d968f
                if (sheet != nullptr) {                                  // 1800d9693
                    const int32_t idx = RT_EXPORT_ONCE(
                        GetIdxFn,
                        "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z")(
                            sheet,
                            entries + static_cast<size_t>(i) * 0x28 + 8); // 1800d96a3
                    __builtin_memcpy(&idxSlot, &idx, sizeof idx);         // 1800d96a8
                } else {
                    const int32_t minusOne = -1;
                    __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne); // 1800d96ad
                }
                sprites = *reinterpret_cast<uint8_t**>(p + 0x40);         // 1800d96b4
            }
            uint8_t* slot = sprites + static_cast<size_t>(i) * 0x10;
            *reinterpret_cast<void**>(slot)        = sheet;   // 1800d9676 / 1800d96b8
            *reinterpret_cast<uint64_t*>(slot + 8) = idxSlot; // 1800d96c0  QWORD store
        }
    }

    if (rw != nullptr) {                                     // 1800d96da
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;                                             // 1800d96e9 MOV AL,1
}

// ============================================================================
// ?Draw@SpriteCapture@UI@@QEBAXAEAVSpriteContext@2@HH@Z   0x1800d9230
// ============================================================================
// Playback: walk `_entries` and re-issue each recorded thing into a live
// SpriteContext at an (ox, oy) offset. Three kinds of entry, told apart by
// `sheetIndex` exactly as runtime/types/ui/SpriteCapture.h's banner says:
//
//   -1  a scissor        -> clamp the recorded rect into the context's own
//                           scaled clip and call SpriteContext::SetScissor
//   -2  a full-screen scissor, i.e. "put the clip back where it was"
//   >=0 a sprite         -> AddSprite (with rotation) or AddLine
//
// THE FOUR SAVED CLIP VALUES LIVE IN THE HOME SLOTS. Retail reads the
// context's clip rectangle ONCE, at entry (+0x20..+0x2c), keeps it across the
// whole loop, and restores it at the end and at every -2 entry. Three of the
// four sit in the caller's shadow space and one in a register that the sprite
// arm clobbers and reloads (0x1800d946d). Reproduced as four plain locals; the
// storage choice is register allocation, the VALUES are the program.
//
// THE CLIP IS SCALED AND THE ENTRY IS NOT. `ctx->+0x00` is a float scale; the
// four clip ints are multiplied by it and truncated, and the recorded entry
// coordinates are used raw, offset by (ox, oy). The clip's third and fourth
// members are a WIDTH and a HEIGHT, not x2/y2 — proved by `ADD EAX,R10D` at
// 0x1800d931f, which forms the right edge as `left + width`.
//
// SetScissor RETURNS A Video::Viewport BY VALUE, so RDX is the hidden return
// slot and the argument rectangle is in R8 (0x1800d936e / 0x1800d937a). The
// returned viewport is DISCARDED. Reading that pair the other way round is the
// wild-write family this project keeps paying for.
extern "C" void RtUi_SpriteCapture_Draw(const void* selfv, void* ctxv, int ox, int oy) {
    const uint8_t* p   = reinterpret_cast<const uint8_t*>(selfv);
    uint8_t*       ctx = reinterpret_cast<uint8_t*>(ctxv);

    const int32_t savedX = *reinterpret_cast<int32_t*>(ctx + 0x20);   // 1800d923c
    const int32_t savedY = *reinterpret_cast<int32_t*>(ctx + 0x24);   // 1800d9265
    const int32_t savedW = *reinterpret_cast<int32_t*>(ctx + 0x28);   // 1800d9268
    const int32_t savedH = *reinterpret_cast<int32_t*>(ctx + 0x2c);   // 1800d9295

    const float scale = *reinterpret_cast<const float*>(ctx);         // 1800d9251
    const int32_t sx = static_cast<int32_t>(static_cast<float>(savedX) * scale);
    const int32_t sy = static_cast<int32_t>(static_cast<float>(savedY) * scale);
    const int32_t sw = static_cast<int32_t>(static_cast<float>(savedW) * scale);
    const int32_t sh = static_cast<int32_t>(static_cast<float>(savedH) * scale);

    using SetScissorFn = void* (*)(void* ctx, int32_t* ret, const int32_t* rect);
    using AddSpriteRotFn = void (*)(void* ctx, const void* sprite,
                                    int x1, int y1, int x2, int y2,
                                    uint32_t color, float rotation);
    using AddLineFn = void (*)(void* ctx, const void* sprite,
                               int x1, int y1, int x2, int y2, uint32_t color);
    using FlushFn = void (*)(void* ctx);

    for (int32_t i = 0; i < *reinterpret_cast<const int32_t*>(p + 0x38); ++i) {
        const uint8_t* entries = *reinterpret_cast<const uint8_t* const*>(p + 0x30);
        const uint8_t* e = entries + static_cast<size_t>(i) * 0x28;   // 1800d92e3

        const int32_t sheetIndex = *reinterpret_cast<const int32_t*>(e);

        if (sheetIndex == -1) {                                       // 1800d92eb
            int32_t x1 = *reinterpret_cast<const int32_t*>(e + 0x10) + ox;
            int32_t x2 = *reinterpret_cast<const int32_t*>(e + 0x18) + ox;
            int32_t y1 = *reinterpret_cast<const int32_t*>(e + 0x14) + oy;
            int32_t y2 = *reinterpret_cast<const int32_t*>(e + 0x1c) + oy;

            const int32_t right  = sw + sx;                           // 1800d931f
            if (x1 < sx)    x1 = sx;                                  // 1800d9318 CMOVL
            if (x1 > right) x1 = right;                               // 1800d9322 CMOVG
            if (x2 < sx)    x2 = sx;
            if (x2 > right) x2 = right;

            const int32_t bottom = sh + sy;                           // 1800d9347
            if (y1 < sy)     y1 = sy;
            if (y1 > bottom) y1 = bottom;
            if (y2 < sy)     y2 = sy;
            if (y2 > bottom) y2 = bottom;

            int32_t rect[4] = { x1, y1, x2 - x1 + 1, y2 - y1 + 1 };   // [RSP+0x58..0x64]
            int32_t discarded[4];                                     // [RSP+0x68] hidden slot
            RT_EXPORT_ONCE(SetScissorFn,
                           "?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z")(
                               ctx, discarded, rect);                 // 1800d9384
            continue;
        }

        if (sheetIndex == -2) {                                       // 1800d938e
            if (savedX != *reinterpret_cast<int32_t*>(ctx + 0x20) ||  // 1800d9393
                savedY != *reinterpret_cast<int32_t*>(ctx + 0x24) ||
                savedW != *reinterpret_cast<int32_t*>(ctx + 0x28) ||
                savedH != *reinterpret_cast<int32_t*>(ctx + 0x2c)) {
                RT_EXPORT_ONCE(FlushFn, "?Flush@SpriteContext@UI@@AEAAXXZ")(ctx);
                RT_EXPORT_ONCE(FlushFn,
                               "?FlushDeferredFonts@SpriteContext@UI@@AEAAXXZ")(ctx);
                *reinterpret_cast<int32_t*>(ctx + 0x20) = savedX;     // 1800d93cf
                *reinterpret_cast<int32_t*>(ctx + 0x24) = savedY;
                *reinterpret_cast<int32_t*>(ctx + 0x28) = savedW;
                *reinterpret_cast<int32_t*>(ctx + 0x2c) = savedH;
            }
            continue;
        }

        // A recorded sprite. The Sprite the scissor entries do NOT have.
        const uint8_t* sprites = *reinterpret_cast<const uint8_t* const*>(p + 0x40);
        const void*  sprite    = sprites + static_cast<size_t>(i) * 0x10;
        void* const  sheet     = *reinterpret_cast<void* const*>(sprite);

        const int32_t x1 = *reinterpret_cast<const int32_t*>(e + 0x10) + ox;
        const int32_t y1 = *reinterpret_cast<const int32_t*>(e + 0x14) + oy;
        const int32_t x2 = *reinterpret_cast<const int32_t*>(e + 0x18) + ox;
        const int32_t y2 = *reinterpret_cast<const int32_t*>(e + 0x1c) + oy;
        const uint32_t color = *reinterpret_cast<const uint32_t*>(e + 0x24);

        // The chain retail walks from the sheet to the thing whose +0x6c
        // selects sprite-vs-line. It is five unguarded loads, transcribed as
        // five loads and NOT named: no body in this batch says what any of the
        // intermediate objects is, and a name invented here would be believed.
        const uint8_t* q = *reinterpret_cast<const uint8_t* const*>(
                               reinterpret_cast<const uint8_t*>(sheet) + 0x30);  // 1800d93fb
        q = *reinterpret_cast<const uint8_t* const*>(q + 0x28);                  // 1800d9406
        q = *reinterpret_cast<const uint8_t* const*>(q + 0x28);                  // 1800d9410
        q = *reinterpret_cast<const uint8_t* const*>(q);                         // 1800d9417
        q = *reinterpret_cast<const uint8_t* const*>(q + 0x38);                  // 1800d941a

        if (*reinterpret_cast<const int32_t*>(q + 0x6c) != 1) {                  // 1800d9423
            const float rotation = *reinterpret_cast<const float*>(e + 0x20);
            RT_EXPORT_ONCE(AddSpriteRotFn,
                           "?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHIM@Z")(
                               ctx, sprite, x1, y1, x2, y2, color, rotation);    // 1800d9468
        } else {
            RT_EXPORT_ONCE(AddLineFn,
                           "?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z")(
                               ctx, sprite, x1, y1, x2, y2, color);              // 1800d9441
        }
    }

    // The same restore as the -2 arm, once more at the end (0x1800d94cd).
    if (savedX != *reinterpret_cast<int32_t*>(ctx + 0x20) ||
        savedY != *reinterpret_cast<int32_t*>(ctx + 0x24) ||
        savedW != *reinterpret_cast<int32_t*>(ctx + 0x28) ||
        savedH != *reinterpret_cast<int32_t*>(ctx + 0x2c)) {
        RT_EXPORT_ONCE(FlushFn, "?Flush@SpriteContext@UI@@AEAAXXZ")(ctx);
        RT_EXPORT_ONCE(FlushFn, "?FlushDeferredFonts@SpriteContext@UI@@AEAAXXZ")(ctx);
        *reinterpret_cast<int32_t*>(ctx + 0x20) = savedX;             // 1800d94f9
        *reinterpret_cast<int32_t*>(ctx + 0x24) = savedY;
        *reinterpret_cast<int32_t*>(ctx + 0x28) = savedW;
        *reinterpret_cast<int32_t*>(ctx + 0x2c) = savedH;
    }
}

// ============================================================================
// The `_entries` element serialize walk   0x18004c0d0   (batch D1)
// ============================================================================
// Serialize above already calls this by address. The element is
// UI::SpriteCapture::CaptureEntry, stride 0x28 — the same stride array.cpp's
// kSpec_CaptureEntry carries — and the walk reads it field by field:
//
//   18004c0fe  kScalarInt   (+0xa0)   e+0x00   sheet index
//   18004c10b  0x1800c9930            e+0x08   System::String name
//   18004c11d  kScalarInt             e+0x10
//   18004c131  kScalarInt             e+0x14
//   18004c145  kScalarInt             e+0x18
//   18004c159  kScalarInt             e+0x1c
//   18004c16d  kScalarFloat (+0x80)   e+0x20
//   18004c181  kScalarUInt  (+0x98)   e+0x24
//
// `ser->readerWriter` is RE-READ before every one of the seven virtual calls
// (`MOV RCX,[RDI+8]` each time). That is not redundancy to tidy away: a
// ReaderWriter is free to replace itself on the Serializer, and the visitor
// pass does exactly that.
extern "C" void RtUi_SpriteCapture_SerializeEntries(void* serv, int count, void* basev) {
    if (count <= 0) return;                                   // 18004c0d0 TEST/JLE
    Serializer* ser = static_cast<Serializer*>(serv);
    uint8_t*    e   = static_cast<uint8_t*>(basev);

    using StrSerFn = void (*)(void* str, Serializer* ser);
    StrSerFn strSer = RtAddr<StrSerFn>(0x1800c9930ull);

    for (int i = 0; i < count; ++i, e += 0x28) {
        void* rw = ser->readerWriter;
        void** rvt = VTableOf(rw);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, e + 0x00, nullptr);

        strSer(e + 0x08, ser);                                // 18004c10b

        rw = ser->readerWriter; rvt = VTableOf(rw);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, e + 0x10, nullptr);
        rw = ser->readerWriter; rvt = VTableOf(rw);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, e + 0x14, nullptr);
        rw = ser->readerWriter; rvt = VTableOf(rw);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, e + 0x18, nullptr);
        rw = ser->readerWriter; rvt = VTableOf(rw);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, e + 0x1c, nullptr);
        rw = ser->readerWriter; rvt = VTableOf(rw);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarFloat / 8])(rw, e + 0x20, nullptr);
        rw = ser->readerWriter; rvt = VTableOf(rw);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, e + 0x24, nullptr);
    }
}

int RtUiSpriteCaptureClassRegister() {
    int n = 0;

    // Batch D1. Unexported in both builds (a file-static of the Serialize
    // translation unit), so the address is the only door.
    RegisterRuntimeAddr(0x18004c0d0ull, (void*)&RtUi_SpriteCapture_SerializeEntries); ++n;

    RegisterRuntimeExport("?Type@SpriteCapture@UI@@UEBAIXZ",
                          (void*)&RtUi_SpriteCapture_Type);                 ++n;
    RegisterRuntimeAddr(0x18003c7f0ull, (void*)&RtUi_SpriteCapture_Type);

    RegisterRuntimeExport("?TypeName@SpriteCapture@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_SpriteCapture_TypeName);             ++n;
    RegisterRuntimeAddr(0x18003c810ull, (void*)&RtUi_SpriteCapture_TypeName);

    RegisterRuntimeExport("?GetFileVersion@SpriteCapture@UI@@UEBAHXZ",
                          (void*)&RtUi_SpriteCapture_GetFileVersion);       ++n;
    RegisterRuntimeAddr(0x1800d9520ull, (void*)&RtUi_SpriteCapture_GetFileVersion);

    RegisterRuntimeExport("?Construct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpriteCapture_Construct);            ++n;
    RegisterRuntimeAddr(0x18003cac0ull, (void*)&RtUi_SpriteCapture_Construct);

    RegisterRuntimeExport("?Reconstruct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpriteCapture_Reconstruct);          ++n;
    RegisterRuntimeAddr(0x18003ca40ull, (void*)&RtUi_SpriteCapture_Reconstruct);

    RegisterRuntimeExport("?Destruct@SpriteCapture@UI@@UEAAXXZ",
                          (void*)&RtUi_SpriteCapture_Destruct);             ++n;
    RegisterRuntimeAddr(0x18003cb40ull, (void*)&RtUi_SpriteCapture_Destruct);

    RegisterRuntimeExport("??1SpriteCapture@UI@@UEAA@XZ",
                          (void*)&RtUi_SpriteCapture_Dtor);                 ++n;
    RegisterRuntimeAddr(0x1800d91b0ull, (void*)&RtUi_SpriteCapture_Dtor);

    RegisterRuntimeExport("?Serialize@SpriteCapture@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_SpriteCapture_Serialize);            ++n;
    RegisterRuntimeAddr(0x1800d9530ull, (void*)&RtUi_SpriteCapture_Serialize);

    RegisterRuntimeExport("?Draw@SpriteCapture@UI@@QEBAXAEAVSpriteContext@2@HH@Z",
                          (void*)&RtUi_SpriteCapture_Draw);                 ++n;
    RegisterRuntimeAddr(0x1800d9230ull, (void*)&RtUi_SpriteCapture_Draw);

    return n;
}
