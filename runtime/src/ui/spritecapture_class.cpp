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
//   0  deleting dtor      0x18003ccf0  SpriteCapture.h (the class's own)
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
// The CONSTRUCTOR `??0SpriteCapture@UI@@QEAA@XZ` (0x1800d9140) is the class's
// real one (SpriteCapture.h). Construct and Reconstruct INLINE the same twelve
// stores in retail (0x18003cad6..0x18003cb26 is straight-line code); here they
// are `::new (this) SpriteCapture()`, the same stores. ENDGAME B.3.9 block
// UI-D: the factory pair is defined and registered here (B39_RULES D2); the
// slot doors, the ??1 door and the FACTUI2 row whose only reader was the
// minted `??_7SpriteCapture@UI@@6B@` are retired with it.
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

#include "../../types/io/Serializer.h"
#include "../../types/ui/SpriteCapture.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/ui/Sprite.h"
#include "../../types/system/String.h"
#include "../../types/video/Context.h"   // Video::Viewport
#include "../../types/object/External.h"
#include "../../types/collection/Collection.h"
#include <new>

namespace {

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

}  // namespace

// 0x18003c7f0 / 0x18003c810: the class's own BaseTypeInfo node at +0 and +8.
// ############################################################################
// UI::SpriteCapture's slot bodies (ENDGAME B.3.6 block D). The doors below
// delegate with a QUALIFIED call (a live SpriteCapture wears the MINTED table).
// ############################################################################
namespace UI {

// 0x1800d9520: `MOV EAX,0x1 ; RET`.
int32_t SpriteCapture::GetFileVersion() const { return 1; }

// 0x18003cac0: the inlined constructor, then `MOV RAX,RCX`. (The null test
// sat in the retired door; a virtual call always has a `this`.)
Object::External* SpriteCapture::Construct() {
    return ::new (this) SpriteCapture();                     // 18003cad6..18003cb26
}
// 0x18003ca40: slot0(this, 0) — VIRTUALLY — then the same inlined ctor.
Object::External* SpriteCapture::Reconstruct() {
    this->~SpriteCapture();                                         // 18003ca57
    return ::new (this) SpriteCapture();                     // 18003ca5e..18003caae
}
// 0x18003cb40: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void SpriteCapture::Destruct() { this->~SpriteCapture(); }

// ?CreateNewObject@SpriteCapture@UI@@SAPEAVBase@Object@@XZ  0x18003c900
// The FACTUI2 row this replaces: the locked MemAlloc of 0x58 bytes, align 8;
// retail's null branch; then the constructor.
Object::Base* SpriteCapture::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SpriteCapture), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) SpriteCapture();
}
// ?CreateInPlaceObject@SpriteCapture@UI@@SAPEAVBase@Object@@PEAX@Z  0x18003c9c0
Object::Base* SpriteCapture::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) SpriteCapture();
}

// ??1SpriteCapture@UI@@UEAA@XZ   0x1800d91b0 — between the restamp and the
// ~External tail (the compiler's): the three retail ~Array calls, in retail's
// order. ENDGAME B.3.9 UI-D: the retired `RtUi_SpriteCapture_Dtor` door ran
// this same body by hand.
SpriteCapture::~SpriteCapture() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    rtcoll::RtColl_ArrayDtor_180037310(p + 0x40);           // 1800d91d4  ~Array<Sprite>
    rtcoll::RtColl_ArrayDtorElem_18003e4a0(p + 0x30);           // 1800d91de  ~Array<CaptureEntry>
    rtcoll::RtColl_ArrayDtor_18003da00(p + 0x20);           // 1800d91e8  ~Array<ExternalPtr<SpriteSheet>>
}

}  // namespace UI

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
namespace UI {

bool SpriteCapture::Serialize(void* streamv) {
    Stream*  stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };                    // [RBP-0x38], [RBP-0x30]

    void*  factory = stream->encodingFactory;                // 1800d9567
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;     // 1800d9571 CMP / JNZ
    using MakeRwFn = IO::ReaderWriter* (*)(void* factory, Serializer* ser, int zero);   // B.3.x bridge: the factory returns the class
    IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                   // 1800d9582

    int32_t n = _sheets.size;       // 1800d9586  _sheets._size
    rw->SerializeInt(n, nullptr);                            // 1800d959b


    if (ser.stream->mode == 0) {                             // 1800d95a5 (re-read)
        rtcoll::RtColl_ArraySetSize_18003ea50(&_sheets, n);      // 1800d95b2
    }
    RtObject_ExternalPtrArraySerialize_SpriteSheet(reinterpret_cast<rt::io::Serializer*>(&ser), n, reinterpret_cast<RtObject::External**>(_sheets.data));

    rw->SerializeInt(_width, nullptr);                       // 1800d95d5  _width
    rw->SerializeInt(_height, nullptr);                      // 1800d95e9  _height

    n = _entries.size;               // 1800d95ef  _entries._size
    rw->SerializeInt(n, nullptr);                            // 1800d9604

    if (ser.stream->mode == 0) {                             // 1800d960e (re-read)
        rtcoll::RtColl_ArraySetSize_18004ba90(&_entries, n);      // 1800d961b
    }
    RtUi_SpriteCapture_SerializeEntries(&ser, n, _entries.data);   // 0x18004c0d0

    if (stream->mode == 0) {                                 // 1800d9630  the ORIGINAL stream
        rtcoll::RtColl_ArraySetSize_1800370c0(
            &_sprites, _entries.size);         // 1800d9642

        for (int32_t i = 0; i < _sprites.size; ++i) {
            uint8_t* entries = static_cast<uint8_t*>(_entries.data);   // 1800d9660
            const int32_t sheetIndex =
                *reinterpret_cast<int32_t*>(entries + static_cast<size_t>(i) * 0x28);

            uint64_t idxSlot;                                // see the banner
            void*    sheet;
            uint8_t* sprites;
            if (sheetIndex < 0) {                            // 1800d9664 CMP/JGE
                const int32_t minusOne = -1;
                __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);  // 1800d966b
                sprites = static_cast<uint8_t*>(_sprites.data);        // 1800d9672
                sheet   = nullptr;                                       // 1800d9676 (RDI==0)
            } else {
                sheet = reinterpret_cast<void* const*>(
                    static_cast<uint8_t*>(_sheets.data))[sheetIndex]; // 1800d968f
                if (sheet != nullptr) {                                  // 1800d9693
                    // ENDGAME B.3.6 block D: was an RT_EXPORT_ONCE door;
                    // now the member. The entry's +0x08 IS a System::String.
                    const int32_t idx = static_cast<const SpriteSheet*>(sheet)
                        ->GetSpriteIndexFromName(*reinterpret_cast<const System::String*>(  // B.3.6 bridge
                            entries + static_cast<size_t>(i) * 0x28 + 8));  // 1800d96a3
                    __builtin_memcpy(&idxSlot, &idx, sizeof idx);         // 1800d96a8
                } else {
                    const int32_t minusOne = -1;
                    __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne); // 1800d96ad
                }
                sprites = static_cast<uint8_t*>(_sprites.data);         // 1800d96b4
            }
            uint8_t* slot = sprites + static_cast<size_t>(i) * 0x10;
            *reinterpret_cast<void**>(slot)        = sheet;   // 1800d9676 / 1800d96b8
            *reinterpret_cast<uint64_t*>(slot + 8) = idxSlot; // 1800d96c0  QWORD store
        }
    }

    if (rw != nullptr) {                                     // 1800d96da
        delete rw;                                 // slot 0, flag 1
    }
    return true;                                             // 1800d96e9 MOV AL,1
}

}  // namespace UI

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
namespace UI {

void SpriteCapture::Draw(SpriteContext& ctx, int32_t ox, int32_t oy) const {
    const int32_t savedX = ctx._scissorX;                             // 1800d923c
    const int32_t savedY = ctx._scissorY;                             // 1800d9265
    const int32_t savedW = ctx._scissorW;                             // 1800d9268
    const int32_t savedH = ctx._scissorH;                             // 1800d9295

    const float scale = ctx._scale;                                   // 1800d9251
    const int32_t sx = static_cast<int32_t>(static_cast<float>(savedX) * scale);
    const int32_t sy = static_cast<int32_t>(static_cast<float>(savedY) * scale);
    const int32_t sw = static_cast<int32_t>(static_cast<float>(savedW) * scale);
    const int32_t sh = static_cast<int32_t>(static_cast<float>(savedH) * scale);

    // `_entries.size` and `.data` are re-read every iteration, as retail does.
    for (int32_t i = 0; i < _entries.size; ++i) {
        const CaptureEntry& e = static_cast<const CaptureEntry*>(_entries.data)[i];  // 1800d92e3

        if (e.sheetIndex == -1) {                                     // 1800d92eb
            int32_t x1 = e.x1 + ox;
            int32_t x2 = e.x2 + ox;
            int32_t y1 = e.y1 + oy;
            int32_t y2 = e.y2 + oy;

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

            // [RSP+0x58..0x64]; the RETURNED viewport (RDX, [RSP+0x68]) is
            // discarded (1800d9384).
            Video::Viewport rect;
            rect.x = x1; rect.y = y1;
            rect.width = x2 - x1 + 1; rect.height = y2 - y1 + 1;
            (void)ctx.SetScissor(rect);
            continue;
        }

        if (e.sheetIndex == -2) {                                     // 1800d938e
            if (savedX != ctx._scissorX || savedY != ctx._scissorY || // 1800d9393
                savedW != ctx._scissorW || savedH != ctx._scissorH) {
                ctx.Flush();
                ctx.FlushDeferredFonts();
                ctx._scissorX = savedX;                               // 1800d93cf
                ctx._scissorY = savedY;
                ctx._scissorW = savedW;
                ctx._scissorH = savedH;
            }
            continue;
        }

        // A recorded sprite. The Sprite the scissor entries do NOT have.
        const Sprite& sprite = static_cast<const Sprite*>(_sprites.data)[i];
        void* const   sheet  = sprite.spriteSheet;

        const int32_t x1 = e.x1 + ox;
        const int32_t y1 = e.y1 + oy;
        const int32_t x2 = e.x2 + ox;
        const int32_t y2 = e.y2 + oy;
        const uint32_t color = e.color;

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
            ctx.AddSprite(sprite, x1, y1, x2, y2, color, e.rotation);            // 1800d9468
        } else {
            ctx.AddLine(sprite, x1, y1, x2, y2, color);                          // 1800d9441
        }
    }

    // The same restore as the -2 arm, once more at the end (0x1800d94cd).
    if (savedX != ctx._scissorX || savedY != ctx._scissorY ||
        savedW != ctx._scissorW || savedH != ctx._scissorH) {
        ctx.Flush();
        ctx.FlushDeferredFonts();
        ctx._scissorX = savedX;                                       // 1800d94f9
        ctx._scissorY = savedY;
        ctx._scissorW = savedW;
        ctx._scissorH = savedH;
    }
}

}  // namespace UI

// The door. ENDGAME B.3.6 block D: the five RT_EXPORT_ONCE doors this body
// used to call on the context (SetScissor, Flush, FlushDeferredFonts,
// AddSprite, AddLine) are SpriteContext member calls now.
extern "C" void RtUi_SpriteCapture_Draw(const void* selfv, void* ctxv, int ox, int oy) {
    reinterpret_cast<const UI::SpriteCapture*>(selfv)                       // B.3.6 bridge
        ->UI::SpriteCapture::Draw(*static_cast<UI::SpriteContext*>(ctxv), ox, oy);
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

    // Re-read before every call (see above), then dispatched by name.
    auto rw = [ser]() { return reinterpret_cast<IO::ReaderWriter*>(ser->readerWriter); };  // B.3.x bridge
    auto i32 = [](uint8_t* p) -> int32_t& { return *reinterpret_cast<int32_t*>(p); };

    for (int i = 0; i < count; ++i, e += 0x28) {
        rw()->SerializeInt(i32(e + 0x00), nullptr);

        reinterpret_cast<System::String*>(e + 0x08)->Serialize(ser);   // 18004c10b 0x1800c9930

        rw()->SerializeInt(i32(e + 0x10), nullptr);
        rw()->SerializeInt(i32(e + 0x14), nullptr);
        rw()->SerializeInt(i32(e + 0x18), nullptr);
        rw()->SerializeInt(i32(e + 0x1c), nullptr);
        rw()->SerializeFloat(*reinterpret_cast<float*>(e + 0x20), nullptr);
        rw()->SerializeUInt(*reinterpret_cast<uint32_t*>(e + 0x24), nullptr);
    }
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::SpriteCapture's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@SpriteCapture@UI@@ (0x1801fa3f8): name "SpriteCapture", parent
// "Object::External" (retail ?_ParentTypeName@SpriteCapture@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::SpriteCapture, "Object::External", nullptr)
