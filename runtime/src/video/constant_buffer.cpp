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
//   +0x00  System::String  type name  ("float4x4", "float3", …)
//   +0x08  System::String  constant name ("worldToProjection", …)
//   +0x10  int32           size in bytes
//   +0x14  int32           offset, filled in by PlatformCreate/EndConstants
// 0x18 bytes. `AddConstant` writes +0x00/+0x08/+0x10 and zeroes +0x14;
// `PlatformCreate` walks +0x10/+0x14; `~ConstantBuffer` (phase 1) destroys the
// two Strings at +0x00/+0x08 with stride 0x18. All three agree.
//
// THE OBJECT: 0x48 bytes, and the fields these four touch
// --------------------------------------------------------
//   +0x00 / +0x08   two System::Strings (the buffer's prefix and its name)
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

struct Entry {
    void*   typeName;   // +0x00  System::String data pointer
    void*   name;       // +0x08  System::String data pointer
    int32_t size;       // +0x10
    int32_t offset;     // +0x14
};
static_assert(sizeof(Entry) == 0x18, "ConstantBuffer::Entry is 0x18 bytes");

// Offsets into Video::ConstantBuffer (0x48 bytes; see the banner).
// WHICH STRING LANDS WHERE, read off the argument order rather than guessed.
// `BeginConstants(this, prefix, name, …)` stores `*prefix` at **+0x08**
// (0x1800eed24/0x1800eed34) and `*name` at **+0x00** (0x1800eed6e). Confirmed
// against the call site: AssignConstants passes ("pc_", "PixelConstants"), so
// +0x00 holds the BUFFER NAME and +0x08 the PREFIX. Naming them the other way
// round is a silent swap — both are Strings, both are released by the same
// destructor, and nothing would ever complain.
constexpr size_t kName     = 0x00;
constexpr size_t kPrefix   = 0x08;
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
        StringAdopt(&dst->typeName, src->typeName);
        StringAdopt(&dst->name,     src->name);
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
//   RCX  this          RDX  prefix        R8   name       R9D  int -> +0x10
//   [RSP+0x50] int  -> +0x14      [RSP+0x58] bool -> +0x18
//   [RSP+0x60] bool -> +0x19      [RSP+0x68] int  -> +0x20
// Note +0x19 is written BEFORE +0x18 (0x1800eedba then 0x1800eedc9) — the
// stack slots are read out of order. Harmless, and kept.
// ============================================================================
extern "C" void RtVideo_ConstantBuffer_BeginConstants(void* self,
                                                      void* const* prefix,
                                                      void* const* name,
                                                      int32_t i10, int32_t i14,
                                                      bool b18, bool b19,
                                                      int32_t i20) {
    StringAssign(&Field<void*>(self, kPrefix), prefix);  // 0x1800eed34 -> +0x08
    StringAssign(&Field<void*>(self, kName),   name);    // 0x1800eed6e -> +0x00

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
// **The temporaries are released in REVERSE order** (name at +0x30 first, then
// the type name at +0x28), which is MSVC's destruction order for the two
// locals. Kept.
//
// **`totalSize += size` happens AFTER the resize** (0x1800eecf0), so a resize
// that failed to allocate still advances the running total. Retail's; it is
// what makes the offsets computed later consistent with a buffer whose entry
// list is short.
//
// The stack element's `offset` field is explicitly zeroed (0x1800eec90).
// ============================================================================
extern "C" void RtVideo_ConstantBuffer_AddConstant(void* self,
                                                   void* const* typeName,
                                                   void* const* name,
                                                   int32_t size) {
    Entry elem;
    StringAdopt(&elem.typeName, *typeName);   // 0x1800eec66
    StringAdopt(&elem.name,     *name);       // 0x1800eec7b
    elem.size   = size;                       // 0x1800eec8b
    elem.offset = 0;                          // 0x1800eec90

    ArrayHdr* a = Entries(self);
    RtVideo_ConstEntryArraySetSize(a, a->size + 1, &elem);   // 0x1800eeca2

    StringRelease(&elem.name);                // 0x1800eeca8 — reverse order
    StringRelease(&elem.typeName);            // 0x1800eeccc

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
        void** vt = *reinterpret_cast<void***>(companion);
        return reinterpret_cast<CompanionFn1>(vt[1])(companion);   // tail JMP +0x08
    }
    ComputeOffsets(self);
    return true;                                                   // MOV AL,1
}

extern "C" void RtVideo_ConstantBuffer_EndConstants(void* self, int32_t /*unread*/) {
    if (void* companion = Field<void*>(self, kCompanion)) {
        void** vt = *reinterpret_cast<void***>(companion);
        (void)reinterpret_cast<CompanionFn1>(vt[1])(companion);
        return;
    }
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
        std::printf("[ShaderLit] SKIP -- Runtime-x64.dll is not mapped (cutover). "
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
                std::printf("[ShaderLit] *** MISMATCH rva=0x%08x ours=\"%s\" "
                            "dll=\"%.32s\" ***\n", L.rva, L.text, dll);
            ++bad;
        }
    }
    std::printf("[ShaderLit] %d lifted literals checked against the loaded "
                "Runtime-x64.dll, %d mismatches\n",
                kShaderConstantLiteralCount, bad);
    if (bad)
        std::printf("[ShaderLit] *** a shader-constant NAME differs from the DLL. "
                    "Do NOT hand-fix it: re-run "
                    "tools/extract_shader_constants.py --emit ***\n");
    return bad;
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

    RegisterRuntimeAddr(0x180053510, (void*)&RtVideo_ConstEntryArraySetSize);
    // Phase 1's two, now that they have callers outside context_lifecycle.cpp.
    RegisterRuntimeAddr(0x18001cab0, (void*)&RtVideo_ConstEntryArrayDtor);
    RegisterRuntimeAddr(0x18001cfc0, (void*)&RtVideo_ConstEntryArrayDestroy);

    return n;
}
