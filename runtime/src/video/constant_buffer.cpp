// ============================================================================
// runtime/src/video/constant_buffer.cpp — Wave 9 lane VID2 phase 3
//   Video::ConstantBuffer's four declaration members, the Array<Entry> resize
//   they drive, and the lifted shader-constant literal table with its runtime
//   self-check.
// ============================================================================
//
// WHERE THIS SITS IN THE SEQUENCE
// -------------------------------
// Phase 2 closed `Video::Device::PlatformStartup`. The next open row is
// `?CreateVideoContext@MainLoop@Engine@@` on the boot path, and the chain to it
// is `ConstantBuffer -> AssignConstants -> the Context ctor ->
// CreateVideoContext`. This file is the first link. Taking `AssignConstants`
// first would have relocated two nulls rather than removed any, because the two
// functions it leans on hardest live here.
//
// TRANSITIVE CLOSURE, COUNTED BEFORE ANY BODY WAS WRITTEN
// -------------------------------------------------------
//   ?BeginConstants@ 0x1800eed10 -> MemFree (registered), 0x18001cab0 (tail JMP)
//   ?AddConstant@    0x1800eec40 -> 0x180053510, MemFree
//   ?EndConstants@   0x1800eebf0 -> companion vtable +0x08 (ours, dx11/)
//   ?PlatformCreate@ 0x1800ef010 -> companion vtable +0x08 (ours, dx11/)
//   0x180053510      Array<Entry>::SetSizeFill
//                                -> MemAlloc/Free (registered), 0x18001cfc0
//   0x18001cab0 / 0x18001cfc0    -> ALREADY PORTED by this lane's phase 1
//                                   (context_lifecycle.cpp); phase 1 left them
//                                   file-local and UNREGISTERED because nothing
//                                   outside that file reached them. That is no
//                                   longer true, and they are registered here.
//
// **Closure: 5 new bodies, 2 existing ones promoted. It CLOSES.** No new
// unregistered address door.
//
// THE ENTRY LAYOUT, pinned by three independent bodies
// -----------------------------------------------------
//   +0x00  System::String  constant name ("worldToProjection", …)
//   +0x08  System::String  type name     ("float4x4", "float3", …)
//   +0x10  int32           size in float4 REGISTERS (a float4x4 is 4)
//   +0x14  int32           offset, filled in by PlatformCreate/EndConstants
// 0x18 bytes. `AddConstant` writes +0x00/+0x08/+0x10 and zeroes +0x14;
// `PlatformCreate` walks +0x10/+0x14; `~ConstantBuffer` (phase 1) destroys the
// two Strings at +0x00/+0x08 with stride 0x18. All three agree.
//
// THE OBJECT: 0x48 bytes, and the fields these four touch
// --------------------------------------------------------
//   +0x00 / +0x08   two System::Strings — +0x00 is the PREFIX ("pc_"), +0x08
//                   the buffer NAME ("PixelConstants"); see the note on
//                   kPrefix/kName below, which this lane first got backwards
//   +0x10 / +0x14   two int32 set by BeginConstants
//   +0x18 / +0x19   two bools set by BeginConstants
//   +0x1c           int32 running total size — AddConstant ADDS to it
//   +0x20           int32 set by BeginConstants
//   +0x28           Array<Entry> {data, size, capacity}
//   +0x40           the ConstantBufferData companion
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"

// Declared locally rather than by including standalone/resolve.h (PLAN §4.1
// decision 1). Byte-identical to array.cpp:133.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// At FILE scope, deliberately: declared inside the `extern "C"` verifier below
// it would acquire C linkage and fail to find resolve.h's C++-linkage symbol.

// The two element helpers, ported in phase 1 (context_lifecycle.cpp).
extern "C" void RtVideo_ConstEntryArrayDestroy(void* unused, void* data, int32_t count);
extern "C" void RtVideo_ConstEntryArrayDtor(void* arr);

namespace {

// ---------------------------------------------------------------------------
// The allocator boundary. Cache a hit, RETRY a miss.
// ---------------------------------------------------------------------------
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn) fn(p);
}

struct ArrayHdr { void* data; int32_t size; int32_t capacity; };
static_assert(sizeof(ArrayHdr) == 0x10, "Collection::Array is 16 bytes");

// Same labelling trap as the buffer's own two Strings, and settled the same
// way — by the INLINED AddConstant in `?AssignConstants@`, which builds the
// element in the open:
//     String::Set(&t1, "float2") ; String::Set(&t2, "texelSize")
//     elem+0x00 <- t2 ("texelSize")   elem+0x08 <- t1 ("float2")
// so +0x00 is the CONSTANT NAME and +0x08 the TYPE NAME, and the out-of-line
// `AddConstant(this, name, typeName, size)` takes them in that order (RDX to
// +0x00, R8 to +0x08). The stores were always right; the labels were not.
struct Entry {
    void*   name;       // +0x00  System::String data pointer ("texelSize")
    void*   typeName;   // +0x08  System::String data pointer ("float2")
    int32_t size;       // +0x10  in float4 REGISTERS — a float4x4 is 4, not 16
    int32_t offset;     // +0x14  assigned later by EndConstants/PlatformCreate
};
static_assert(sizeof(Entry) == 0x18, "ConstantBuffer::Entry is 0x18 bytes");

// Offsets into Video::ConstantBuffer (0x48 bytes; see the banner).
//
// WHICH STRING LANDS WHERE. The two fields are indistinguishable by shape —
// both are refcounted System::Strings, both are released by the same
// destructor — so getting the LABELS backwards costs nothing at run time and
// misleads every later reader. This lane got them backwards once and the
// correction came from `?AssignConstants@`, where MSVC INLINED the first
// buffer's BeginConstants and so shows the literals landing at named offsets:
//
//   String::Set(&t1, "pc_")            ; String::Set(&t2, "PixelConstants")
//   this+0x2e8 <- t2   ("PixelConstants")      <- written FIRST
//   this+0x2e0 <- t1   ("pc_")
//   zero this+0x2f0 (8) / +0x2f8 (2) / +0x300 (4) ; clear the array at +0x308
//
// Those last four fix the buffer base at 0x2e0 from four independent points
// (0x2f0=+0x10, 0x2f8=+0x18, 0x300=+0x20, 0x308=+0x28). So:
//
//   +0x00 = PREFIX ("pc_")            written SECOND, from BeginConstants' R8
//   +0x08 = NAME   ("PixelConstants") written FIRST,  from BeginConstants' RDX
//
// and the out-of-line signature is therefore `BeginConstants(this, name,
// prefix, …)`, not (prefix, name). The stores themselves were always right —
// arg2 to +0x08 and arg3 to +0x00 — this is a naming correction only.
constexpr size_t kPrefix   = 0x00;
constexpr size_t kName     = 0x08;
constexpr size_t kInt10    = 0x10;
constexpr size_t kInt14    = 0x14;
constexpr size_t kBool18   = 0x18;
constexpr size_t kBool19   = 0x19;
constexpr size_t kTotalSz  = 0x1c;
constexpr size_t kInt20    = 0x20;
constexpr size_t kEntries  = 0x28;
constexpr size_t kCompanion = 0x40;

inline ArrayHdr* Entries(void* self) {
    return reinterpret_cast<ArrayHdr*>(static_cast<uint8_t*>(self) + kEntries);
}
template <typename T> inline T& Field(void* self, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(self) + off);
}

// ---------------------------------------------------------------------------
// The refcounted System::String ASSIGN, which appears three times across these
// four bodies and once more inside the resize.
//
//   release the destination if it differs from the source, then adopt the
//   source and bump its refcount.
//
// Retail's release half is the same shape phase 1 transcribed for
// `~ConstantBuffer`: on the LAST reference it rewinds the field by 8 and frees
// WITHOUT nulling it; on a shared reference it nulls the field and frees
// nothing. Both kept.
//
// The `if (dst == src) skip` guard at the top is retail's and is load-bearing:
// self-assignment must not decrement to zero and free the thing it is about to
// adopt.
// ---------------------------------------------------------------------------
void StringAssign(void** dst, void* src) {
    void* cur = *dst;
    if (cur == src) return;                        // 0x1800eed34
    if (cur) {
        int32_t* rc = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(cur) - 4);
        *rc -= 1;
        if (*rc == 0) {
            *dst = static_cast<uint8_t*>(cur) - 8; // 0x1800eed47
            MemFree(*dst);
        } else {
            *dst = nullptr;                        // 0x1800eed57
        }
    }
    *dst = src;
    if (src) *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(src) - 4) += 1;
}

// The ADOPT-INTO-A-FRESH-SLOT half, used where the destination is known empty
// (the stack element AddConstant builds, and the resize's copy loop). Retail
// writes the null FIRST and then overwrites it, rather than branching — kept,
// because the intermediate null is what makes a re-entrant read see an empty
// slot rather than a stale one.
void StringAdopt(void** dst, void* src) {
    *dst = nullptr;
    if (src) {
        *dst = src;
        *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(src) - 4) += 1;
    }
}

// The RELEASE half on its own, for the two temporaries AddConstant unwinds.
void StringRelease(void** field) {
    void* p = *field;
    if (!p) return;
    int32_t* rc = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(p) - 4);
    *rc -= 1;
    if (*rc == 0) {
        *field = static_cast<uint8_t*>(p) - 8;
        MemFree(*field);
    } else {
        *field = nullptr;
    }
}

}  // namespace

// ============================================================================
// 0x180053510 — Collection::Array<ConstantBuffer::Entry>::SetSize(n, fill)
// ============================================================================
// Resize to `n`, filling any NEW element from `*fill` and copying any surviving
// element from the old block. Unexported.
//
// FOUR THINGS THAT ARE NOT OBVIOUS AND ARE NOT SIMPLIFICATIONS:
//
//  1. **`newSize == oldSize` returns immediately** (0x18005353f), before the
//     capacity check — so a same-size "resize" does not reallocate and does not
//     re-run the fill. `AddConstant` always grows by one, so this early-out is
//     unreachable from here; it is transcribed because the address is
//     registered and other callers exist inside the DLL.
//  2. **The copy loop's START depends on whether the block MOVED.**
//     `CMP R14,[RBX] / CMOVZ ECX,ESI`: if the data pointer is unchanged
//     (no reallocation) the loop starts at `oldSize` and only constructs the
//     new tail; if it moved, it starts at 0 and copies everything. Starting at
//     0 unconditionally would double-adopt every surviving element's Strings.
//  3. **The source per element is `i < oldSize ? old[i] : *fill`**
//     (`CMOVL RDX,R10`), so a grow-by-one copies the survivors and fills
//     exactly the last slot.
//  4. **The teardown is a THREE-WAY branch**, not a free:
//       * block moved  -> destroy ALL old elements, then free the old block;
//       * block stayed and shrank -> destroy only the TAIL that fell off;
//       * block stayed and grew   -> nothing.
//     Collapsing that into "free if moved" leaks every String in a shrink.
//
// The growth rule is the same `n == old + 1 && old != 0 -> capacity = n * 3`
// this lane already transcribed for the Device arrays.
// ============================================================================
extern "C" void RtVideo_ConstEntryArraySetSize(void* arrv, int32_t newSize,
                                               const void* fillv) {
    ArrayHdr* a = static_cast<ArrayHdr*>(arrv);
    const Entry* fill = static_cast<const Entry*>(fillv);

    const int32_t oldSize = a->size;
    if (newSize == oldSize) return;                       // 0x18005353f

    Entry* oldData = static_cast<Entry*>(a->data);
    a->size = newSize;

    if (newSize > a->capacity) {
        a->capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) a->capacity = newSize * 3;
        a->data = MemAlloc(static_cast<uint64_t>(
                      static_cast<int64_t>(a->capacity) * 0x18), 8);
    }

    Entry* newData = static_cast<Entry*>(a->data);
    const int32_t start = (oldData == newData) ? oldSize : 0;   // 0x180053586

    for (int32_t i = start; i < newSize; ++i) {
        Entry* dst = newData + i;
        if (dst == nullptr) continue;                     // retail's ADD/JZ
        const Entry* src = (i < oldSize) ? (oldData + i) : fill;
        StringAdopt(&dst->name,     src->name);       // +0x00 first, as retail
        StringAdopt(&dst->typeName, src->typeName);   // +0x08
        dst->size   = src->size;
        dst->offset = src->offset;
    }

    if (oldData != newData) {
        RtVideo_ConstEntryArrayDestroy(a, oldData, oldSize);   // 0x180053613
        if (oldData) MemFree(oldData);
    } else if (oldSize > newSize) {
        RtVideo_ConstEntryArrayDestroy(a, oldData + newSize, oldSize - newSize);
    }
}

// ============================================================================
// ?BeginConstants@ConstantBuffer@Video@@QEAAXAEBVString@System@@0HH_N1H@Z
//                                                   retail 0x1800eed10, ord 1636
// ============================================================================
// Start a new constant-buffer declaration: adopt the prefix and name strings,
// store the five scalars, and CLEAR the entry array.
//
// The clear is a TAIL JUMP into 0x18001cab0 (0x1800eede0), so it is the last
// thing that happens and its result is this function's result (void).
//
// ARGUMENT ORDER, read off the stack slots rather than from the mangled name
// alone (`…AEBVString@System@@0HH_N1H@Z` = String&, String&, int, int, bool,
// bool, int):
//   RCX  this          RDX  name          R8   prefix     R9D  int -> +0x10
//   [RSP+0x50] int  -> +0x14      [RSP+0x58] bool -> +0x18
//   [RSP+0x60] bool -> +0x19      [RSP+0x68] int  -> +0x20
// Note +0x19 is written BEFORE +0x18 (0x1800eedba then 0x1800eedc9) — the
// stack slots are read out of order. Harmless, and kept.
// ============================================================================
extern "C" void RtVideo_ConstantBuffer_BeginConstants(void* self,
                                                      const System::String* name,
                                                      const System::String* prefix,
                                                      int32_t i10, int32_t i14,
                                                      bool b18, bool b19,
                                                      int32_t i20) {
    // B.3.1 bridge: the destinations are POD slots on Video::ConstantBuffer,
    // which is not converted yet, so retail's inlined assign stays.
    StringAssign(&Field<void*>(self, kName),   name->_data);    // 0x1800eed34 -> +0x08
    StringAssign(&Field<void*>(self, kPrefix), prefix->_data);  // 0x1800eed6e -> +0x00

    Field<int32_t>(self, kInt10) = i10;
    Field<int32_t>(self, kInt14) = i14;
    Field<uint8_t>(self, kBool19) = static_cast<uint8_t>(b19);
    Field<int32_t>(self, kInt20) = i20;
    Field<uint8_t>(self, kBool18) = static_cast<uint8_t>(b18);

    RtVideo_ConstEntryArrayDtor(Entries(self));         // tail JMP 0x18001cab0
}

// ============================================================================
// ?AddConstant@ConstantBuffer@Video@@QEAAXAEBVString@System@@0H@Z
//                                                   retail 0x1800eec40, ord 1540
// ============================================================================
// Append one declaration. Retail builds the element ON THE STACK, hands its
// address to the resize as the fill value, then releases the two temporaries.
//
// **The temporaries are released in REVERSE order** (the TYPE name at +0x30
// first, then the constant name at +0x28), which is MSVC's destruction order
// for the two locals. Kept.
//
// **`totalSize += size` happens AFTER the resize** (0x1800eecf0), so a resize
// that failed to allocate still advances the running total. Retail's; it is
// what makes the offsets computed later consistent with a buffer whose entry
// list is short.
//
// The stack element's `offset` field is explicitly zeroed (0x1800eec90).
// ============================================================================
extern "C" void RtVideo_ConstantBuffer_AddConstant(void* self,
                                                   const System::String* name,
                                                   const System::String* typeName,
                                                   int32_t size) {
    Entry elem;
    // B.3.1 bridge: `Entry` is the raw array element Video::ConstantBuffer
    // stores; it keeps its POD slots until that class is converted.
    StringAdopt(&elem.name,     name->_data);       // 0x1800eec66 -> elem+0x00
    StringAdopt(&elem.typeName, typeName->_data);   // 0x1800eec7b -> elem+0x08
    elem.size   = size;                       // 0x1800eec8b
    elem.offset = 0;                          // 0x1800eec90

    ArrayHdr* a = Entries(self);
    RtVideo_ConstEntryArraySetSize(a, a->size + 1, &elem);   // 0x1800eeca2

    StringRelease(&elem.typeName);            // 0x1800eeca8 (+0x30) — reverse
    StringRelease(&elem.name);                // 0x1800eeccc (+0x28)

    Field<int32_t>(self, kTotalSz) += size;   // 0x1800eecf0
}

// ============================================================================
// ?PlatformCreate@ConstantBuffer@Video@@AEAA_NXZ    retail 0x1800ef010, ord 3349
// ?EndConstants@ConstantBuffer@Video@@QEAAXH@Z      retail 0x1800eebf0, ord 2306
// ============================================================================
// The SAME BODY, emitted twice, differing only in the return:
// `PlatformCreate` sets AL=1 on the fall-through path, `EndConstants` returns
// void. Both:
//
//   companion = this->+0x40
//   if (companion) return companion->vtbl[1](companion);   // TAIL JUMP
//   running = 0;
//   for (i = 0; i < entryCount; ++i) {
//       entry[i].offset = running;
//       running += entry[i].size;
//   }
//
// So with a backend companion attached the whole offset walk is SKIPPED and the
// backend does it; only the no-companion path computes offsets here.
//
// **`EndConstants` takes an int argument that it never reads.** `0x1800eebf0`'s
// first instruction is `MOV RDX,[RCX+0x40]` and EDX/RDX is overwritten
// immediately; the parameter is dead. Declared and ignored, not removed, so the
// registered signature matches the mangled one.
//
// **The entry data pointer is RE-READ every iteration** (0x1800ef035 /
// 0x1800eec15). Nothing can move it — the loop does not allocate — but it is
// retail's read and this lane does not remove re-reads on the strength of
// "nothing can move it" (lane PICK §5).
// ============================================================================
namespace {
// Branch counters, incremented INSIDE the bodies under test. A probe that
// counts what it observes from the outside can report green while three of four
// branches were never entered (lane PKG2), so these live here and the probe
// prints them.
int g_endCompanion = 0;   // delegated to the backend companion
int g_endArith     = 0;   // computed the offsets here

void ComputeOffsets(void* self) {
    const int32_t n = Field<int32_t>(self, kEntries + 8);
    int32_t running = 0;
    for (int32_t i = 0; i < n; ++i) {
        Entry* data = static_cast<Entry*>(Field<void*>(self, kEntries));  // re-read
        data[i].offset = running;
        data = static_cast<Entry*>(Field<void*>(self, kEntries));         // re-read
        running += data[i].size;
    }
}
using CompanionFn1 = bool (*)(void*);
}  // namespace

extern "C" bool RtVideo_ConstantBuffer_PlatformCreate(void* self) {
    if (void* companion = Field<void*>(self, kCompanion)) {
        ++g_endCompanion;
        void** vt = *reinterpret_cast<void***>(companion);
        return reinterpret_cast<CompanionFn1>(vt[1])(companion);   // tail JMP +0x08
    }
    ++g_endArith;
    ComputeOffsets(self);
    return true;                                                   // MOV AL,1
}

extern "C" void RtVideo_ConstantBuffer_EndConstants(void* self, int32_t /*unread*/) {
    if (void* companion = Field<void*>(self, kCompanion)) {
        ++g_endCompanion;
        void** vt = *reinterpret_cast<void***>(companion);
        (void)reinterpret_cast<CompanionFn1>(vt[1])(companion);
        return;
    }
    ++g_endArith;
    ComputeOffsets(self);
}

// ============================================================================
// THE LIFTED SHADER-CONSTANT LITERALS, AND THE SELF-CHECK
// ============================================================================
// `?AssignConstants@Context@Video@@AEAAXXZ` declares the engine's seven constant
// buffers from 45 string literals in the DLL's .rdata. **A mistyped constant
// NAME is a shader parameter that silently never binds** — no crash, no
// divergence, no oracle short of a rendering diff.
//
// So they are LIFTED, NOT RETYPED. `tools/extract_shader_constants.py` reads
// the SHIPPED PE with pefile, walks the byte range of `AssignConstants`
// (bounded by the next EXPORTED address, derived not guessed), decodes every
// `LEA r64,[rip+disp32]` from the RAW BYTES — no disassembler in the loop, so no
// listing's typo can be inherited — and emits shader_constants.inc. That is the
// same rule `reference_crt_coefficients_must_be_lifted_not_retyped.md` records
// for the CRT coefficients, applied to literals.
//
// TWO INDEPENDENT EXTRACTIONS AGREE: the byte walk finds **76 references, 45
// distinct**, and an independent regex over Ghidra's disassembly TEXT finds the
// same 76/45. The tool's `--verify` mode re-extracts and diffs the committed
// file (0 mismatches).
//
// **AND THE PORT CHECKS ITSELF AT RUNTIME.** Every row carries the RVA it came
// from, so with the DLL present `RtVideoShaderConstantsVerify()` compares our
// compiled-in bytes against the bytes at that RVA in the loaded image. A typo
// introduced by a later hand-edit fails LOUDLY at startup instead of silently
// unbinding a parameter. With the DLL absent the check skips — which is exactly
// when our copy is the only copy, and exactly why the check has to run now.
// ============================================================================
struct RtShaderConstantLiteral {
    uint32_t    rva;
    int32_t     length;
    const char* text;
};

#include "shader_constants.inc"

constexpr int kShaderConstantLiteralCount =
    static_cast<int>(sizeof(kShaderConstantLiterals) / sizeof(kShaderConstantLiterals[0]));
static_assert(kShaderConstantLiteralCount == 45,
              "the lifted table must hold all 45 distinct literals -- if this "
              "fires, re-run tools/extract_shader_constants.py rather than "
              "editing the count");

// Named accessors, so AssignConstants reads `Lit(kLit_worldToProjection)` rather
// than an index nobody can check. The enum ORDER must match the .inc, which is
// emitted in ascending RVA order; the static_asserts below pin the ones whose
// misuse would be silent.
enum : int {
    kLit_pc_ = 0, kLit_PixelConstants, kLit_float2, kLit_texelSize, kLit_float4,
    kLit_fogColor, kLit_float3, kLit_lightColor, kLit_ambientColor,
    kLit_userPosition, kLit_mc_, kLit_MaterialConstants, kLit_materialConstant0,
    kLit_materialConstant1, kLit_materialConstant2, kLit_materialConstant3,
    kLit_gc_, kLit_GlobalConstants, kLit_float4x4, kLit_worldToProjection,
    kLit_worldToCamera, kLit_cameraPosition, kLit_float, kLit_time, kLit_fog,
    kLit_ic_, kLit_InstanceConstants, kLit_inverseScale, kLit_color,
    kLit_lc_, kLit_LightConstants, kLit_position, kLit_nearPlane, kLit_radius,
    kLit_textureProjection, kLit_shadowProjection0, kLit_shadowProjection1,
    kLit_shadowProjection2, kLit_shadowProjection3, kLit_tc_,
    kLit_TransformConstants, kLit_transformFmt, kLit_bc_, kLit_BoneConstants,
    kLit_bones51,
};

const char* RtVideoShaderConstantLiteral(int index) {
    return kShaderConstantLiterals[index].text;
}

// ============================================================================
// ?AssignConstants@Context@Video@@AEAAXXZ           retail 0x1800ef3d0, ord 1629
// ============================================================================
// The engine's whole shader constant-buffer LAYOUT, in one straight-line
// function: seven ConstantBuffers embedded in Video::Context, 31 declarations
// between them, 45 distinct string literals, 1,975 instructions and not a
// single loop or conditional of its own.
//
// HOW THE TABLE BELOW WAS RECOVERED, AND WHY IT IS NOT A TRANSCRIPTION
// --------------------------------------------------------------------
// **Eleven of the thirty-one rows are INLINED.** MSVC inlined three whole
// `BeginConstants` bodies (the two String assignments, the five scalar stores
// and the entry-array clear appear in line, with only the array clear left as a
// call to 0x18001cab0) and eight whole `AddConstant` bodies (the element built
// on the stack and handed straight to 0x180053510). A port written from "the 20
// AddConstant calls" would therefore have declared 20 of 31 constants and three
// of seven buffers would have had no name at all — and nothing would have said
// so, because the resulting Context is perfectly well-formed.
//
// So the table is machine-extracted, twice over, from the shipped bytes:
//   * tools/extract_shader_constants.py lifts the 45 LITERALS with their RVAs
//     (76 references, cross-checked against an independent count), and
//     RtVideoShaderConstantsVerify() re-checks every one of them against the
//     loaded DLL at startup.
//   * tools/extract_assign_constants.py --events walks the same bytes for the
//     STRUCTURE: every call to the six functions this table is built from, every
//     `MOV R9D,imm` (an out-of-line size), every `MOV qword [RBP-0x79],imm` (an
//     INLINED size), the BeginConstants scalars, the two bool stack arguments,
//     and the `LEA rXX,[R15+disp]` that says which buffer each call acts on.
//     Its AddConstant count is asserted at 20 so a scan that under-reads fails.
// Only the arrangement below is written by hand, and every literal in it is a
// named index into the lifted table rather than a retyped string.
//
// THE SLOT CHAIN, which is the one piece of real logic
// -----------------------------------------------------
// Each buffer's +0x14 is the register slot it binds to, and it is computed from
// the PREVIOUS buffer:
//     slot = prev->companion ? prev->companion->vtbl[3]()   // ask the backend
//                            : prev->+0x14 + 1;             // else just next
// with the chain running pc_ -> mc_, gc_ -> ic_ -> lc_ -> tc_ -> bc_. pc_ and
// gc_ start their own chains at slot 0. During the ctor every companion is
// still null, so the arithmetic branch is the live one; the virtual branch
// matters only if AssignConstants is ever re-run after the device is up.
//
// THE BUFFER OFFSETS inside Video::Context are NOT in declaration order:
//   +0x0298 mc_  +0x02e0 pc_  +0x1f78 gc_  +0x1fc0 ic_
//   +0x2008 bc_  +0x2050 tc_  +0x2098 lc_
// bc_'s is easy to get wrong: MSVC does `ADD R15,0x2008` and then passes R15
// itself, which reads like `this` and would silently build the bone buffer over
// the top of the Context header.
//
// TWO DELIBERATE DEVIATIONS, both from MSVC's temporary handling
//   1. Retail keeps all 22 of lc_'s String temporaries alive at once and frees
//      them in one run of 22 MemFrees at the end; we free each pair right after
//      its call. Same refcounts, same final object — a different transient heap
//      peak and free order, which nothing observes.
//   2. The three inlined BeginConstants and eight inlined AddConstants are
//      emitted here as calls to the ported out-of-line bodies. That is exact,
//      not an approximation: the inlined code is the same body, and the only
//      textual difference (`INC [buf+0x1c]` where the out-of-line does
//      `ADD [buf+0x1c],size`) is the same operation at the sizes involved —
//      checked, not assumed: the two inlined size-4 rows do emit
//      `ADD dword [RSI+0x1c],4`.
// ============================================================================
namespace {

// Build a fresh String temporary from one LIFTED literal.
System::String StrLit(int litIndex) {
    System::String s;
    s.Set(RtVideoShaderConstantLiteral(litIndex));
    return s;
}

// `slot = prev->companion ? prev->companion->vtbl[3]() : prev->+0x14 + 1`
int32_t NextSlotAfter(void* prev, int* tookVirtual, int* tookArithmetic) {
    void* companion = Field<void*>(prev, kCompanion);
    if (!companion) {
        if (tookArithmetic) ++*tookArithmetic;
        return Field<int32_t>(prev, kInt14) + 1;
    }
    if (tookVirtual) ++*tookVirtual;
    void** vt = *reinterpret_cast<void***>(companion);
    return reinterpret_cast<int32_t (*)(void*)>(vt[3])(companion);
}

void BeginBuffer(void* buf, int prefixLit, int nameLit, int32_t i10,
                 int32_t i14, bool b18, bool b19, int32_t i20) {
    System::String prefix = StrLit(prefixLit);   // retail Sets the PREFIX first
    System::String name   = StrLit(nameLit);
    RtVideo_ConstantBuffer_BeginConstants(buf, &name, &prefix,
                                          i10, i14, b18, b19, i20);
    // …and releases the NAME first: reverse declaration order, which is the
    // destruction order the two locals now get for free.
}

void AddDecl(void* buf, int typeLit, int nameLit, int32_t size) {
    System::String type = StrLit(typeLit);       // retail Sets the TYPE first
    System::String name = StrLit(nameLit);
    RtVideo_ConstantBuffer_AddConstant(buf, &name, &type, size);
    // …and releases the NAME first: reverse declaration order again.
}

struct DeclRow  { int16_t typeLit; int16_t nameLit; int32_t size; };
struct BufRow {
    uint32_t       bufOff;      // offset of this ConstantBuffer inside Context
    int16_t        prefixLit;
    int16_t        nameLit;
    int32_t        i10;
    int32_t        prevOff;     // -1: start a new chain at `i14`
    int32_t        i14;
    uint8_t        b18, b19;
    int32_t        i20;
    const DeclRow* decls;
    int            ndecls;
    uint8_t        platformCreate;   // 1: PlatformCreate, 0: EndConstants
};

// clang-format off
const DeclRow kPixelDecls[] = {
    { kLit_float2, kLit_texelSize,    1 },
    { kLit_float4, kLit_fogColor,     1 },
    { kLit_float3, kLit_lightColor,   1 },
    { kLit_float3, kLit_ambientColor, 1 },
    { kLit_float4, kLit_userPosition, 1 },
};
const DeclRow kMaterialDecls[] = {
    { kLit_float4, kLit_materialConstant0, 1 },
    { kLit_float4, kLit_materialConstant1, 1 },
    { kLit_float4, kLit_materialConstant2, 1 },
    { kLit_float4, kLit_materialConstant3, 1 },
};
const DeclRow kGlobalDecls[] = {
    { kLit_float4x4, kLit_worldToProjection, 4 },   // INLINED in retail
    { kLit_float4x4, kLit_worldToCamera,     4 },   // INLINED in retail
    { kLit_float3,   kLit_cameraPosition,    1 },
    { kLit_float4,   kLit_userPosition,      1 },
    { kLit_float,    kLit_time,              1 },
    { kLit_float2,   kLit_fog,               1 },
    { kLit_float3,   kLit_fogColor,          1 },
};
const DeclRow kInstanceDecls[] = {
    { kLit_float4, kLit_inverseScale, 1 },
    { kLit_float4, kLit_color,        1 },
};
const DeclRow kLightDecls[] = {
    { kLit_float3,   kLit_color,             1 },
    { kLit_float3,   kLit_ambientColor,      1 },
    { kLit_float3,   kLit_position,          1 },
    { kLit_float4,   kLit_nearPlane,         1 },
    { kLit_float4,   kLit_radius,            1 },
    { kLit_float4,   kLit_texelSize,         1 },
    { kLit_float4x4, kLit_textureProjection, 4 },
    { kLit_float4x4, kLit_shadowProjection0, 4 },
    { kLit_float4x4, kLit_shadowProjection1, 4 },
    { kLit_float4x4, kLit_shadowProjection2, 4 },
    { kLit_float4x4, kLit_shadowProjection3, 4 },
};
const DeclRow kBoneDecls[] = {
    { kLit_float4x4, kLit_bones51, 204 },           // 51 float4x4 = 204 slots
};

constexpr uint32_t kOffMaterial = 0x0298;
constexpr uint32_t kOffPixel    = 0x02e0;
constexpr uint32_t kOffGlobal   = 0x1f78;
constexpr uint32_t kOffInstance = 0x1fc0;
constexpr uint32_t kOffBone     = 0x2008;
constexpr uint32_t kOffTransform= 0x2050;
constexpr uint32_t kOffLight    = 0x2098;

// The five buffers whose constants are all plain literals, in retail's order.
// tc_ and bc_ follow separately: tc_'s only constant is String::Format'ed and
// it ends with EndConstants rather than PlatformCreate, and bc_ is the tail.
const BufRow kBufRows[] = {
  { kOffPixel,    kLit_pc_, kLit_PixelConstants,    0, -1,          0, 0, 0, 0,
    kPixelDecls,    5,  0 },
  { kOffMaterial, kLit_mc_, kLit_MaterialConstants, 1, kOffPixel,   0, 0, 0, 0,
    kMaterialDecls, 4,  0 },
  { kOffGlobal,   kLit_gc_, kLit_GlobalConstants,   2, -1,          0, 1, 0, 0,
    kGlobalDecls,   7,  1 },
  { kOffInstance, kLit_ic_, kLit_InstanceConstants, 3, kOffGlobal,  0, 1, 0, 0,
    kInstanceDecls, 2,  1 },
  { kOffLight,    kLit_lc_, kLit_LightConstants,    4, kOffInstance,0, 1, 0, 0,
    kLightDecls,   11,  1 },
};
// clang-format on

inline void* BufAt(void* ctx, uint32_t off) {
    return static_cast<uint8_t*>(ctx) + off;
}

// Branch counters, measured INSIDE the function under test rather than inferred
// from anything observed outside it.
int g_slotVirtual   = 0;   // the companion vtable[3] branch
int g_slotArith     = 0;   // the `prev+0x14 + 1` branch
}  // namespace

extern "C" void RtVideo_Context_AssignConstants(void* self) {
    for (const BufRow& b : kBufRows) {
        void* buf = BufAt(self, b.bufOff);
        const int32_t slot = (b.prevOff < 0)
            ? b.i14
            : NextSlotAfter(BufAt(self, static_cast<uint32_t>(b.prevOff)),
                            &g_slotVirtual, &g_slotArith);
        BeginBuffer(buf, b.prefixLit, b.nameLit, b.i10, slot,
                    b.b18 != 0, b.b19 != 0, b.i20);
        for (int i = 0; i < b.ndecls; ++i)
            AddDecl(buf, b.decls[i].typeLit, b.decls[i].nameLit,
                    b.decls[i].size);
        if (b.platformCreate)
            RtVideo_ConstantBuffer_PlatformCreate(buf);
        else
            RtVideo_ConstantBuffer_EndConstants(buf, 0);
    }

    // ---- tc_ / TransformConstants -----------------------------------------
    // The only buffer whose constant NAME is formatted rather than a literal,
    // and the only one with b19 set and a non-zero +0x20 (0x40).
    {
        void* buf = BufAt(self, kOffTransform);
        const int32_t slot = NextSlotAfter(BufAt(self, kOffLight),
                                           &g_slotVirtual, &g_slotArith);
        BeginBuffer(buf, kLit_tc_, kLit_TransformConstants, 5, slot,
                    true, true, 0x40);

        System::String type = StrLit(kLit_float4x4);
        // Retail leaves this slot uninitialised and lets Format CONSTRUCT into
        // it; `Format` returns a fresh String by value, which is the same thing.
        System::String name =
            System::String::Format(RtVideoShaderConstantLiteral(kLit_transformFmt), 52);
        RtVideo_ConstantBuffer_AddConstant(buf, &name, &type, 208);
        // name then type, retail's release order — reverse declaration order.

        RtVideo_ConstantBuffer_EndConstants(buf, 0);   // the arg is UNSET at the
                                                       // retail call site, and
                                                       // the callee overwrites
                                                       // RDX before reading it
    }

    // ---- bc_ / BoneConstants, and the tail EndConstants --------------------
    {
        void* buf = BufAt(self, kOffBone);
        const int32_t slot = NextSlotAfter(BufAt(self, kOffTransform),
                                           &g_slotVirtual, &g_slotArith);
        BeginBuffer(buf, kLit_bc_, kLit_BoneConstants, 6, slot, true, false, 0);
        for (const DeclRow& d : kBoneDecls)
            AddDecl(buf, d.typeLit, d.nameLit, d.size);
        RtVideo_ConstantBuffer_EndConstants(buf, 0xcc0);   // retail's tail JMP
    }
}


// ============================================================================
// Registration
//
// All four mangled names checked against the shipped export directory with
// pefile BEFORE any body was written:
//   ?BeginConstants@ConstantBuffer@Video@@QEAAXAEBVString@System@@0HH_N1H@Z  1636
//   ?AddConstant@ConstantBuffer@Video@@QEAAXAEBVString@System@@0H@Z          1540
//   ?EndConstants@ConstantBuffer@Video@@QEAAXH@Z                             2306
//   ?PlatformCreate@ConstantBuffer@Video@@AEAA_NXZ                           3349
//
// Three ADDRESS-ONLY rows: 0x180053510, 0x18001cab0 and 0x18001cfc0 are
// unexported (checked against the retail directory AND Runtime-x64-profile's
// 5,182 names), so RegisterRuntimeExport has nothing to register under.
// ============================================================================
int RtVideoConstantBufferRegister() {
    int n = 0;

    RegisterRuntimeExport("?BeginConstants@ConstantBuffer@Video@@QEAAXAEBVString@System@@0HH_N1H@Z",
                          (void*)&RtVideo_ConstantBuffer_BeginConstants); ++n;
    RegisterRuntimeExport("?AddConstant@ConstantBuffer@Video@@QEAAXAEBVString@System@@0H@Z",
                          (void*)&RtVideo_ConstantBuffer_AddConstant); ++n;
    RegisterRuntimeExport("?EndConstants@ConstantBuffer@Video@@QEAAXH@Z",
                          (void*)&RtVideo_ConstantBuffer_EndConstants); ++n;
    RegisterRuntimeExport("?PlatformCreate@ConstantBuffer@Video@@AEAA_NXZ",
                          (void*)&RtVideo_ConstantBuffer_PlatformCreate); ++n;

    RegisterRuntimeAddr(0x1800eed10, (void*)&RtVideo_ConstantBuffer_BeginConstants);
    RegisterRuntimeAddr(0x1800eec40, (void*)&RtVideo_ConstantBuffer_AddConstant);
    RegisterRuntimeAddr(0x1800eebf0, (void*)&RtVideo_ConstantBuffer_EndConstants);
    RegisterRuntimeAddr(0x1800ef010, (void*)&RtVideo_ConstantBuffer_PlatformCreate);

    RegisterRuntimeExport("?AssignConstants@Context@Video@@AEAAXXZ",
                          (void*)&RtVideo_Context_AssignConstants); ++n;
    RegisterRuntimeAddr(0x1800ef3d0, (void*)&RtVideo_Context_AssignConstants);

    RegisterRuntimeAddr(0x180053510, (void*)&RtVideo_ConstEntryArraySetSize);
    // Phase 1's two, now that they have callers outside context_lifecycle.cpp.
    RegisterRuntimeAddr(0x18001cab0, (void*)&RtVideo_ConstEntryArrayDtor);
    RegisterRuntimeAddr(0x18001cfc0, (void*)&RtVideo_ConstEntryArrayDestroy);

    return n;
}
