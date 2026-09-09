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

// Declared locally rather than by including standalone/resolve.h (PLAN §4.1
// decision 1). Byte-identical to array.cpp:133.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// At FILE scope, deliberately: declared inside the `extern "C"` verifier below
// it would acquire C linkage and fail to find resolve.h's C++-linkage symbol.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

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
void StringAssign(void** dst, void* const* src) {
    void* cur = *dst;
    if (cur == *src) return;                       // 0x1800eed34
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
    void* s = *src;
    *dst = s;
    if (s) *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(s) - 4) += 1;
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
                                                      void* const* name,
                                                      void* const* prefix,
                                                      int32_t i10, int32_t i14,
                                                      bool b18, bool b19,
                                                      int32_t i20) {
    StringAssign(&Field<void*>(self, kName),   name);    // 0x1800eed34 -> +0x08
    StringAssign(&Field<void*>(self, kPrefix), prefix);  // 0x1800eed6e -> +0x00

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
                                                   void* const* name,
                                                   void* const* typeName,
                                                   int32_t size) {
    Entry elem;
    StringAdopt(&elem.name,     *name);       // 0x1800eec66 -> elem+0x00
    StringAdopt(&elem.typeName, *typeName);   // 0x1800eec7b -> elem+0x08
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

// ---------------------------------------------------------------------------
// The self-check. Returns the number of MISMATCHES; 0 is a pass, and a skip
// (DLL absent) also returns 0 but says so.
// ---------------------------------------------------------------------------
extern "C" int RtVideoShaderConstantsVerify() {
    // The module base of the loaded Runtime DLL, obtained by resolving a known
    // code address and subtracting its Ghidra VA. ResolveAddr is registry-aware
    // and 0x1800c95d0 IS registered (String::Set), so that would hand back OUR
    // port and compute a nonsense base. `ResolveRuntimeRaw` is the shipped
    // image every time.
    const uintptr_t probeVa = 0x1800c95d0;                 // ?Set@String@System@@
    const uintptr_t loaded = ResolveRuntimeRaw(probeVa);
    if (!loaded) {
        printf("[ShaderLit] SKIP -- Runtime-x64.dll is not mapped (cutover). "
                    "%d lifted literals go UNCHECKED, which is expected: with no "
                    "DLL our copy is the only copy.\n", kShaderConstantLiteralCount);
        return 0;
    }
    const uintptr_t base = loaded - (probeVa - 0x180000000ull);

    int bad = 0;
    for (int i = 0; i < kShaderConstantLiteralCount; ++i) {
        const RtShaderConstantLiteral& L = kShaderConstantLiterals[i];
        const char* dll = reinterpret_cast<const char*>(base + L.rva);
        if (std::memcmp(dll, L.text, static_cast<size_t>(L.length) + 1) != 0) {
            if (bad < 5)
                printf("[ShaderLit] *** MISMATCH rva=0x%08x ours=\"%s\" "
                            "dll=\"%.32s\" ***\n", L.rva, L.text, dll);
            ++bad;
        }
    }
    printf("[ShaderLit] %d lifted literals checked against the loaded "
                "Runtime-x64.dll, %d mismatches\n",
                kShaderConstantLiteralCount, bad);
    if (bad)
        printf("[ShaderLit] *** a shader-constant NAME differs from the DLL. "
                    "Do NOT hand-fix it: re-run "
                    "tools/extract_shader_constants.py --emit ***\n");
    return bad;
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

using StringSetFn    = void  (*)(void** self, const char* text);
using StringFormatFn = void** (*)(void** ret, const char* fmt, ...);

// Build a fresh String temporary from one LIFTED literal.
void StrSetLit(void** s, int litIndex) {
    *s = nullptr;
    RT_EXPORT_ONCE(StringSetFn, "?Set@String@System@@QEAAXPEBD@Z")(
        s, RtVideoShaderConstantLiteral(litIndex));
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
    void* prefix = nullptr;
    void* name   = nullptr;
    StrSetLit(&prefix, prefixLit);          // retail Sets the PREFIX first
    StrSetLit(&name,   nameLit);
    RtVideo_ConstantBuffer_BeginConstants(buf, &name, &prefix,
                                          i10, i14, b18, b19, i20);
    StringRelease(&name);                   // …and releases the NAME first
    StringRelease(&prefix);
}

void AddDecl(void* buf, int typeLit, int nameLit, int32_t size) {
    void* type = nullptr;
    void* name = nullptr;
    StrSetLit(&type, typeLit);              // retail Sets the TYPE first
    StrSetLit(&name, nameLit);
    RtVideo_ConstantBuffer_AddConstant(buf, &name, &type, size);
    StringRelease(&name);                   // …and releases the NAME first
    StringRelease(&type);
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

        void* type = nullptr;
        StrSetLit(&type, kLit_float4x4);
        // Retail leaves this slot uninitialised and lets Format CONSTRUCT into
        // it. A null is the empty String and is equivalent for a constructing
        // Format; it is not equivalent for an assigning one, so do not "fix"
        // this by pre-filling it with anything else.
        void* name = nullptr;
        RT_EXPORT_ONCE(StringFormatFn, "?Format@String@System@@SA?AV12@PEBDZZ")(
            &name, RtVideoShaderConstantLiteral(kLit_transformFmt), 52);
        RtVideo_ConstantBuffer_AddConstant(buf, &name, &type, 208);
        StringRelease(&name);
        StringRelease(&type);

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
// THE PROBE — a lockstep oracle for AssignConstants
// ============================================================================
// `AssignConstants` has no caller in our tree yet (the Context ctor is the next
// link and is not ported), so a boot run would report calls=0 and prove nothing
// — the failure mode this project has already been burned by twice. It is,
// however, a PURE FUNCTION OF A ZEROED OBJECT: it takes no arguments, reads
// nothing but the Context it is handed, and every companion pointer it consults
// is null in a fresh one. So it can be driven directly.
//
// Two zeroed Contexts in .bss, ours built into one and the SHIPPED body into the
// other, then compared FIELD BY FIELD — never with a memcmp, because the two
// sides hold different heap pointers for equal strings and a byte compare would
// fail 100% of the time for a reason that has nothing to do with the port
// (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// Compared per buffer: prefix TEXT, name TEXT, +0x10, +0x14 (the slot chain),
// +0x18, +0x19, +0x1c (the running total), +0x20, the entry COUNT, and for
// every entry its name text, type text, size and assigned offset.
//
// EFFECTIVENESS CONTROL, measured on THEIRS: how many buffers and entries the
// shipped body actually produced. A green comparison over an empty object is
// not a verification.
// NEGATIVE CONTROL: one field of ours is corrupted afterwards and the same
// comparator re-run; if it does not report the corruption, the comparator is
// broken and the green above meant nothing.
// BRANCH COUNTERS: printed from inside the bodies under test.
// ============================================================================
namespace {

const char* Txt(void* s) { return s ? static_cast<const char*>(s) : "<null>"; }

bool TxtEq(void* a, void* b) {
    if (!a || !b) return a == b;
    return std::strcmp(static_cast<const char*>(a),
                       static_cast<const char*>(b)) == 0;
}

const uint32_t kProbeBufOffs[7] = { kOffPixel, kOffMaterial, kOffGlobal,
                                    kOffInstance, kOffLight, kOffTransform,
                                    kOffBone };

int CompareContexts(void* ours, void* theirs, bool quiet) {
    int bad = 0;
    for (uint32_t off : kProbeBufOffs) {
        void* o = BufAt(ours, off);
        void* t = BufAt(theirs, off);
#define VID2_CMP(expr, fmt, ...)                                              \
        do { if (!(expr)) { ++bad; if (!quiet)                                \
                printf("[AssignConst] *** buf+0x%04x " fmt "\n", off,    \
                            __VA_ARGS__); } } while (0)
        VID2_CMP(TxtEq(Field<void*>(o, kPrefix), Field<void*>(t, kPrefix)),
                 "prefix ours=\"%s\" theirs=\"%s\" ***",
                 Txt(Field<void*>(o, kPrefix)), Txt(Field<void*>(t, kPrefix)));
        VID2_CMP(TxtEq(Field<void*>(o, kName), Field<void*>(t, kName)),
                 "name ours=\"%s\" theirs=\"%s\" ***",
                 Txt(Field<void*>(o, kName)), Txt(Field<void*>(t, kName)));
        VID2_CMP(Field<int32_t>(o, kInt10) == Field<int32_t>(t, kInt10),
                 "+0x10 ours=%d theirs=%d ***",
                 Field<int32_t>(o, kInt10), Field<int32_t>(t, kInt10));
        VID2_CMP(Field<int32_t>(o, kInt14) == Field<int32_t>(t, kInt14),
                 "+0x14 slot ours=%d theirs=%d ***",
                 Field<int32_t>(o, kInt14), Field<int32_t>(t, kInt14));
        VID2_CMP(Field<uint8_t>(o, kBool18) == Field<uint8_t>(t, kBool18),
                 "+0x18 ours=%d theirs=%d ***",
                 Field<uint8_t>(o, kBool18), Field<uint8_t>(t, kBool18));
        VID2_CMP(Field<uint8_t>(o, kBool19) == Field<uint8_t>(t, kBool19),
                 "+0x19 ours=%d theirs=%d ***",
                 Field<uint8_t>(o, kBool19), Field<uint8_t>(t, kBool19));
        VID2_CMP(Field<int32_t>(o, kTotalSz) == Field<int32_t>(t, kTotalSz),
                 "+0x1c total ours=%d theirs=%d ***",
                 Field<int32_t>(o, kTotalSz), Field<int32_t>(t, kTotalSz));
        VID2_CMP(Field<int32_t>(o, kInt20) == Field<int32_t>(t, kInt20),
                 "+0x20 ours=%d theirs=%d ***",
                 Field<int32_t>(o, kInt20), Field<int32_t>(t, kInt20));

        const int32_t on = Entries(o)->size, tn = Entries(t)->size;
        VID2_CMP(on == tn, "entry count ours=%d theirs=%d ***", on, tn);
        if (on != tn) continue;
        const Entry* oe = static_cast<const Entry*>(Entries(o)->data);
        const Entry* te = static_cast<const Entry*>(Entries(t)->data);
        for (int32_t i = 0; i < on; ++i) {
            VID2_CMP(TxtEq(oe[i].name, te[i].name),
                     "entry %d name ours=\"%s\" theirs=\"%s\" ***", i,
                     Txt(oe[i].name), Txt(te[i].name));
            VID2_CMP(TxtEq(oe[i].typeName, te[i].typeName),
                     "entry %d type ours=\"%s\" theirs=\"%s\" ***", i,
                     Txt(oe[i].typeName), Txt(te[i].typeName));
            VID2_CMP(oe[i].size == te[i].size,
                     "entry %d size ours=%d theirs=%d ***", i,
                     oe[i].size, te[i].size);
            VID2_CMP(oe[i].offset == te[i].offset,
                     "entry %d offset ours=%d theirs=%d ***", i,
                     oe[i].offset, te[i].offset);
        }
#undef VID2_CMP
    }
    return bad;
}

// Release everything one side allocated, so the probe does not leak ~124
// Strings and 14 arrays into a live process.
void TearDownContext(void* ctx) {
    for (uint32_t off : kProbeBufOffs) {
        void* b = BufAt(ctx, off);
        ArrayHdr* a = Entries(b);
        Entry* e = static_cast<Entry*>(a->data);
        for (int32_t i = 0; i < a->size; ++i) {
            StringRelease(&e[i].name);
            StringRelease(&e[i].typeName);
        }
        if (a->data) MemFree(a->data);
        a->data = nullptr; a->size = 0; a->capacity = 0;
        StringRelease(&Field<void*>(b, kPrefix));
        StringRelease(&Field<void*>(b, kName));
    }
}

// 0x2200 covers every buffer this function touches (the highest is lc_ at
// +0x2098, ending at +0x20e0). Static so the probe allocates nothing itself.
alignas(16) uint8_t g_probeOurs[0x2200];
alignas(16) uint8_t g_probeTheirs[0x2200];

bool g_assignProbeArmed = false;

// ---------------------------------------------------------------------------
// PASS 2 — DRIVING the companion branches instead of waiting for them.
//
// Pass 1 above exercises a fresh Context, where every companion is null, so the
// virtual halves of the slot chain and of EndConstants/PlatformCreate are never
// entered: the branch counters read slot-virtual=0 end-companion=0 and the
// green result covers half the function. Nothing in a boot run would ever enter
// them either — a companion only exists once the backend has created one, and
// AssignConstants runs before that.
//
// So the probe MAKES one. A four-slot fake vtable is enough: slot 1 is what
// EndConstants/PlatformCreate tail-jump to, slot 3 is what the slot chain asks
// for the next register index. Both sides get their own identical fakes, and
// the fake's answer is derived from its own tag rather than from a call
// counter, so it cannot depend on the order in which the two runs happen.
// ---------------------------------------------------------------------------
struct FakeCompanion {
    void**  vtbl;
    int32_t tag;
    int32_t endCalls;
    int32_t slotCalls;
};

bool FakeSlot1(void* self) {                       // EndConstants' delegate
    ++static_cast<FakeCompanion*>(self)->endCalls;
    return true;
}
int32_t FakeSlot3(void* self) {                    // "what slot do I bind to?"
    FakeCompanion* f = static_cast<FakeCompanion*>(self);
    ++f->slotCalls;
    return f->tag * 10 + 3;                        // deterministic, order-free
}
void* g_fakeVtbl[4] = { nullptr, reinterpret_cast<void*>(&FakeSlot1),
                        nullptr, reinterpret_cast<void*>(&FakeSlot3) };

FakeCompanion g_fakeOurs[7];
FakeCompanion g_fakeTheirs[7];

void InstallFakes(void* ctx, FakeCompanion* fakes) {
    for (int i = 0; i < 7; ++i) {
        fakes[i].vtbl = g_fakeVtbl;
        fakes[i].tag = i;
        fakes[i].endCalls = 0;
        fakes[i].slotCalls = 0;
        Field<void*>(BufAt(ctx, kProbeBufOffs[i]), kCompanion) = &fakes[i];
    }
}

}  // namespace

extern "C" void RtVideoAssignConstantsProbeArm(bool on) { g_assignProbeArmed = on; }

extern "C" void RtVideoAssignConstantsProbeMaybeRun() {
    if (!g_assignProbeArmed) return;
    g_assignProbeArmed = false;
    using Fn = void (*)(void*);
    const uintptr_t shipped = ResolveRuntimeRaw(0x1800ef3d0);
    if (!shipped) {
        printf("[AssignConst] SKIP -- the shipped body is not mapped "
                    "(cutover); no oracle is possible without it\n");
        return;
    }

    std::memset(g_probeOurs, 0, sizeof(g_probeOurs));
    std::memset(g_probeTheirs, 0, sizeof(g_probeTheirs));

    g_slotVirtual = g_slotArith = g_endCompanion = g_endArith = 0;
    RtVideo_Context_AssignConstants(g_probeOurs);
    const int slotVirt = g_slotVirtual, slotArith = g_slotArith;
    const int endComp  = g_endCompanion, endArith = g_endArith;

    reinterpret_cast<Fn>(shipped)(g_probeTheirs);

    // Effectiveness, measured on THEIRS.
    int buffersNamed = 0, entriesMade = 0, offsetsAssigned = 0;
    for (uint32_t off : kProbeBufOffs) {
        void* t = BufAt(g_probeTheirs, off);
        if (Field<void*>(t, kName)) ++buffersNamed;
        const int32_t n = Entries(t)->size;
        entriesMade += n;
        const Entry* e = static_cast<const Entry*>(Entries(t)->data);
        for (int32_t i = 0; i < n; ++i)
            if (e[i].offset != 0) ++offsetsAssigned;
    }

    const int bad = CompareContexts(g_probeOurs, g_probeTheirs, false);
    printf("[AssignConst] buffers=7 entries=%d mismatches=%d\n",
                entriesMade, bad);
    printf("[AssignConst] effectiveness (all measured on THEIRS): "
                "buffersNamed=%d entriesMade=%d offsetsAssigned=%d\n",
                buffersNamed, entriesMade, offsetsAssigned);
    printf("[AssignConst] branches taken INSIDE the ported bodies: "
                "slot-virtual=%d slot-arith=%d end-companion=%d end-arith=%d\n",
                slotVirt, slotArith, endComp, endArith);
    if (slotVirt == 0)
        printf("[AssignConst] pass 1 leaves the companion branches "
                    "UNENTERED by construction (a fresh Context has none); "
                    "pass 2 below drives them rather than waiting for them.\n");

    // Negative control: break one field of ours and demand the comparator says
    // so. Two of them, one scalar and one string, because a comparator can be
    // right about one kind and blind to the other.
    void* g = BufAt(g_probeOurs, kOffGlobal);
    const int32_t saveSlot = Field<int32_t>(g, kInt14);
    Field<int32_t>(g, kInt14) = saveSlot + 7;
    const int caught1 = CompareContexts(g_probeOurs, g_probeTheirs, true);
    Field<int32_t>(g, kInt14) = saveSlot;

    Entry* ge = static_cast<Entry*>(Entries(g)->data);
    void* saveName = ge ? ge[0].name : nullptr;
    if (ge) ge[0].name = nullptr;
    const int caught2 = CompareContexts(g_probeOurs, g_probeTheirs, true);
    if (ge) ge[0].name = saveName;

    printf("[AssignConst] negative-controls: scalar=%s string=%s\n",
                caught1 > bad ? "CAUGHT" : "*** MISSED ***",
                caught2 > bad ? "CAUGHT" : "*** MISSED ***");

    TearDownContext(g_probeOurs);
    TearDownContext(g_probeTheirs);

    // ---- PASS 2: the same comparison with a FAKE COMPANION on every buffer,
    // which is the only way these branches are ever entered.
    std::memset(g_probeOurs, 0, sizeof(g_probeOurs));
    std::memset(g_probeTheirs, 0, sizeof(g_probeTheirs));
    InstallFakes(g_probeOurs, g_fakeOurs);
    InstallFakes(g_probeTheirs, g_fakeTheirs);

    g_slotVirtual = g_slotArith = g_endCompanion = g_endArith = 0;
    RtVideo_Context_AssignConstants(g_probeOurs);
    const int slotVirt2 = g_slotVirtual, slotArith2 = g_slotArith;
    const int endComp2  = g_endCompanion, endArith2 = g_endArith;

    reinterpret_cast<Fn>(shipped)(g_probeTheirs);

    const int cmp2 = CompareContexts(g_probeOurs, g_probeTheirs, false);
    int bad2 = cmp2;
    for (int i = 0; i < 7; ++i) {
        if (g_fakeOurs[i].endCalls != g_fakeTheirs[i].endCalls ||
            g_fakeOurs[i].slotCalls != g_fakeTheirs[i].slotCalls) {
            ++bad2;
            printf("[AssignConst] *** fake companion %d: ours end=%d "
                        "slot=%d, theirs end=%d slot=%d ***\n", i,
                        g_fakeOurs[i].endCalls, g_fakeOurs[i].slotCalls,
                        g_fakeTheirs[i].endCalls, g_fakeTheirs[i].slotCalls);
        }
    }
    int endCallsTheirs = 0, slotCallsTheirs = 0;
    for (int i = 0; i < 7; ++i) {
        endCallsTheirs  += g_fakeTheirs[i].endCalls;
        slotCallsTheirs += g_fakeTheirs[i].slotCalls;
    }
    printf("[AssignConst] pass2 (fake companion on all 7): mismatches=%d\n",
                bad2);
    printf("[AssignConst] pass2 effectiveness (on THEIRS): "
                "companion-end-calls=%d companion-slot-calls=%d\n",
                endCallsTheirs, slotCallsTheirs);
    printf("[AssignConst] pass2 branches INSIDE the ported bodies: "
                "slot-virtual=%d slot-arith=%d end-companion=%d end-arith=%d\n",
                slotVirt2, slotArith2, endComp2, endArith2);

    // Negative control for pass 2: the slot values now come from the fake, so
    // corrupt ONE of them and demand the comparator says so. Without this, a
    // comparator that had stopped looking at +0x14 would read green here.
    void* g2 = BufAt(g_probeOurs, kOffTransform);
    const int32_t save2 = Field<int32_t>(g2, kInt14);
    Field<int32_t>(g2, kInt14) = save2 + 1;
    const int caught3 = CompareContexts(g_probeOurs, g_probeTheirs, true);
    Field<int32_t>(g2, kInt14) = save2;
    printf("[AssignConst] pass2 negative-control (slot from the fake): "
                "%s\n", caught3 > bad2 ? "CAUGHT" : "*** MISSED ***");

    TearDownContext(g_probeOurs);
    TearDownContext(g_probeTheirs);
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
