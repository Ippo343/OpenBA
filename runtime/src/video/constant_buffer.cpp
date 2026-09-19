// ============================================================================
// runtime/src/video/constant_buffer.cpp — Wave 9 lane VID2 phase 3
//   Video::ConstantBuffer's four declaration members, the Array<ConstantInfo> resize
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
//   0x180053510      Array<ConstantInfo>::SetSizeFill
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
//                   the buffer's TYPE NAME ("PixelConstants"). This lane first
//                   had the two labels backwards; B.3.5 settled them outright
//                   from ?GetPrefix@ and ?GetTypeName@ (see below), and the
//                   fields now live on the real class in
//                   runtime/types/video/ConstantBuffer.h
//   +0x10 / +0x14   two int32 set by BeginConstants
//   +0x18 / +0x19   two bools set by BeginConstants
//   +0x1c           int32 running total size — AddConstant ADDS to it
//   +0x20           int32 set by BeginConstants
//   +0x28           Array<ConstantInfo> {data, size, capacity}
//   +0x40           the ConstantBufferData companion
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/video/Context.h"        // Video::Context::AssignConstants
#include "../../types/video/ConstantBuffer.h"  // Video::ConstantBuffer
// ENDGAME B.3.5 block J: the real Video::ConstantBufferData, block A's abstract
// base, whose four slots this file dispatches through.
#include "../../types/video/CompanionData.h"

// ENDGAME B.3.5 block D1. `Video::ConstantBuffer` and `Video::Context` are real
// classes now; all five bodies below MOVED onto their members and every
// registered door kept its name, its mangled-name row and its address row.
//
// THE TEN `constexpr size_t k*` OFFSETS THIS FILE USED TO DECLARE ARE GONE.
// They were one of FOUR partial views of the same 0x48 bytes (B35_RULES section
// 13 item 4); the fields they named are now members of
// `Video::ConstantBuffer`, with the retail accessor that PROVES each one cited
// on the declaration. The two whose labels this file corrected — +0x00 is the
// PREFIX and +0x08 the buffer's TYPE NAME — are confirmed a second way this
// pass, by the accessors themselves: `?GetPrefix@ConstantBuffer@Video@@`
// (0x1800182e0) is `MOV RAX,RCX / RET`, i.e. the String AT +0x00, and
// `?GetTypeName@` (0x1800182f0) is `LEA RAX,[RCX+8]`. The out-of-line
// `BeginConstants(this, typeName, prefix, …)` argument order follows from that
// and is what the member signature now spells.
//
// The local `struct Entry` is gone too: it is retail's
// `ConstantBuffer::ConstantInfo`, named by
// `?GetConstant@ConstantBuffer@Video@@QEBAAEBUConstantInfo@12@H@Z` (`AEBU` — a
// nested STRUCT), whose `LEA RDX,[RAX+RAX*2] / LEA RAX,[RAX+RDX*8]` is the same
// 0x18 stride this file had already derived three other ways.
//
// FOUR DEAD BRANCH COUNTERS DELETED (`g_endCompanion`, `g_endArith`,
// `g_slotVirtual`, `g_slotArith`). They were written and never read — the probe
// that was to print them does not exist in this file — so they measured nothing;
// a counter nothing reads is not an instrument.

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

using ConstantInfo = Video::ConstantBuffer::ConstantInfo;

// The `Collection::Array<ConstantInfo>` header, as the unexported resize at
// 0x180053510 sees it. It is the same three fields
// `Video::ConstantBuffer::{_constantData, _constantCount, _constantCapacity}`
// name at +0x28 / +0x30 / +0x34; the resize is registered BY ADDRESS and is
// handed the array, not the buffer, so it keeps its own view of the triple.
struct ArrayHdr { ConstantInfo* data; int32_t size; int32_t capacity; };
static_assert(sizeof(ArrayHdr) == 0x10, "Collection::Array is 16 bytes");

// WHICH STRING LANDS WHERE, inside a `ConstantInfo`. The two fields are
// indistinguishable by shape — both are refcounted System::Strings, both are
// released by the same destructor — so getting the LABELS backwards costs
// nothing at run time and misleads every later reader. Settled by the INLINED
// AddConstant in `?AssignConstants@`, which builds the element in the open:
//     String::Set(&t1, "float2") ; String::Set(&t2, "texelSize")
//     elem+0x00 <- t2 ("texelSize")   elem+0x08 <- t1 ("float2")
// so +0x00 is the CONSTANT NAME and +0x08 the TYPE NAME, and the out-of-line
// `AddConstant(this, name, typeName, size)` takes them in that order (RDX to
// +0x00, R8 to +0x08). The stores were always right; the labels were not. The
// same applies to the BUFFER's own two Strings — and there the accessors settle
// it outright: see the banner at the top of this file.

// The `Collection::Array<ConstantInfo>` living at +0x28 of a ConstantBuffer.
// Reached as a triple rather than through the three members because the
// address-registered resize below is handed the ARRAY, not the buffer.
inline ArrayHdr* Entries(Video::ConstantBuffer* self) {
    return reinterpret_cast<ArrayHdr*>(&self->_constantData);
}
static_assert(offsetof(Video::ConstantBuffer, _constantData) == 0x28,
              "the Array<ConstantInfo> triple starts at +0x28");

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
// 0x180053510 — Collection::Array<ConstantBuffer::ConstantInfo>::SetSize(n, fill)
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
    const ConstantInfo* fill = static_cast<const ConstantInfo*>(fillv);

    const int32_t oldSize = a->size;
    if (newSize == oldSize) return;                       // 0x18005353f

    ConstantInfo* oldData = a->data;
    a->size = newSize;

    if (newSize > a->capacity) {
        a->capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) a->capacity = newSize * 3;
        a->data = static_cast<ConstantInfo*>(MemAlloc(static_cast<uint64_t>(
                      static_cast<int64_t>(a->capacity) * 0x18), 8));
    }

    ConstantInfo* newData = a->data;
    const int32_t start = (oldData == newData) ? oldSize : 0;   // 0x180053586

    for (int32_t i = start; i < newSize; ++i) {
        ConstantInfo* dst = newData + i;
        if (dst == nullptr) continue;                     // retail's ADD/JZ
        const ConstantInfo* src = (i < oldSize) ? (oldData + i) : fill;
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
// The two `const void*` parameters are retail's `const System::String&`. They
// stay untyped for the reason ConstantBuffer.h's banner gives: the DESTINATIONS
// (+0x00 and +0x08) are bare data pointers rather than `System::String` members,
// because a `System::String` member would give `Video::ConstantBuffer` a
// destructor and take `Video::Context`'s trivial copyability with it (RULE
// B5-4). The caller-side temporaries below are real Strings.
void Video::ConstantBuffer::BeginConstants(const void* typeName, const void* prefix,
                                           int32_t ordinalIndex, int32_t uploadIndex,
                                           bool bindToVertex, bool banked,
                                           int32_t bankStride) {
    StringAssign(&_typeName,
                 static_cast<const System::String*>(typeName)->_data);  // 0x1800eed34 -> +0x08
    StringAssign(&_prefix,
                 static_cast<const System::String*>(prefix)->_data);    // 0x1800eed6e -> +0x00

    _ordinalIndex  = ordinalIndex;
    _uploadIndex   = uploadIndex;
    _banked        = static_cast<uint8_t>(banked);
    _bankStride    = bankStride;
    _bindToVertex  = static_cast<uint8_t>(bindToVertex);

    RtVideo_ConstEntryArrayDtor(Entries(this));         // tail JMP 0x18001cab0
}

extern "C" void RtVideo_ConstantBuffer_BeginConstants(void* self,
                                                      const System::String* name,
                                                      const System::String* prefix,
                                                      int32_t i10, int32_t i14,
                                                      bool b18, bool b19,
                                                      int32_t i20) {
    static_cast<Video::ConstantBuffer*>(self)
        ->Video::ConstantBuffer::BeginConstants(name, prefix, i10, i14, b18, b19, i20);
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
void Video::ConstantBuffer::AddConstant(const void* name, const void* typeName,
                                        int32_t size) {
    ConstantInfo elem;
    StringAdopt(&elem.name,
                static_cast<const System::String*>(name)->_data);      // 0x1800eec66 -> elem+0x00
    StringAdopt(&elem.typeName,
                static_cast<const System::String*>(typeName)->_data);  // 0x1800eec7b -> elem+0x08
    elem.size   = size;                       // 0x1800eec8b
    elem.offset = 0;                          // 0x1800eec90

    ArrayHdr* a = Entries(this);
    RtVideo_ConstEntryArraySetSize(a, a->size + 1, &elem);   // 0x1800eeca2

    StringRelease(&elem.typeName);            // 0x1800eeca8 (+0x30) — reverse
    StringRelease(&elem.name);                // 0x1800eeccc (+0x28)

    _registerCount += size;                   // 0x1800eecf0
}

extern "C" void RtVideo_ConstantBuffer_AddConstant(void* self,
                                                   const System::String* name,
                                                   const System::String* typeName,
                                                   int32_t size) {
    static_cast<Video::ConstantBuffer*>(self)
        ->Video::ConstantBuffer::AddConstant(name, typeName, size);
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

void ComputeOffsets(Video::ConstantBuffer* self) {
    const int32_t n = self->_constantCount;
    int32_t running = 0;
    for (int32_t i = 0; i < n; ++i) {
        ConstantInfo* data = self->_constantData;   // re-read, as retail does
        data[i].offset = running;
        data = self->_constantData;                 // re-read
        running += data[i].size;
    }
}
// ENDGAME B.3.5 block J, COORDINATOR RULING: the four hand-inlined companion
// dispatches in this file are VIRTUAL CALLS on `Video::ConstantBufferData` (block
// A, runtime/types/video/CompanionData.h). `_platformData` stays a pinned `void*`
// at +0x40 that dx11 reads, and the object behind it is a
// `VideoDX11::ConstantBufferData` that does not derive in C++ (RULE B5-8's declared
// leave), so each receiver takes one `// B.3.5 bridge` cast — but the DISPATCH is
// unchanged: the same live vptr at +0x00, indexed at the same byte offset, with the
// index checked by the compiler now. `vt[1]` is slot 1 `Create()`, `vt[3]` is slot 3
// `SlotIndexPlusOne()`, and `vt[+0x10]` is slot 2 `Bind(...)`.
//
// The `CompanionFn1` typedef is gone with its two users.
}  // namespace

namespace {
// B.3.5 bridge — `_platformData` is a pinned `void*` field, by ABI.
inline Video::ConstantBufferData* Companion(void* platformData) {
    return reinterpret_cast<Video::ConstantBufferData*>(platformData);
}
}  // namespace

bool Video::ConstantBuffer::PlatformCreate() {
    if (void* companion = _platformData) {
        return Companion(companion)->Create();                     // tail JMP +0x08
    }
    ComputeOffsets(this);
    return true;                                                   // MOV AL,1
}

void Video::ConstantBuffer::EndConstants(int32_t /*unread*/) {
    if (void* companion = _platformData) {
        (void)Companion(companion)->Create();
        return;
    }
    ComputeOffsets(this);
}

// ============================================================================
// ?GetNextUploadIndex@ConstantBuffer@Video@@QEBAHXZ  retail 0x1800eef90, ord 2831
// ============================================================================
// PORTED THIS PASS, and it was hiding in plain sight: `?AssignConstants@` builds
// the whole seven-buffer slot chain out of this function, and until B.3.5 the
// tree carried its body as `constant_buffer.cpp`'s file-local `NextSlotAfter`
// with no door of any kind. Read as disassembly this pass:
//
//   1800eef90  MOV RAX,RCX               ; save `this`
//   1800eef93  MOV RCX,[RCX+0x40]        ; _platformData
//   1800eef97  TEST RCX,RCX / JNZ +8
//   1800eef9c  MOV EAX,[RAX+0x14] / INC EAX / RET      ; _uploadIndex + 1
//   1800eefa2  MOV RAX,[RCX] / JMP [RAX+0x18]          ; companion slot 3
//
// so with a backend companion attached the BACKEND decides the next register
// slot and the arithmetic branch is skipped. During the Context constructor
// every companion is still null, which is why the arithmetic branch is the live
// one on the boot path.
int32_t Video::ConstantBuffer::GetNextUploadIndex() const {
    void* companion = _platformData;
    if (!companion) return _uploadIndex + 1;
    return Companion(companion)->SlotIndexPlusOne();   // +0x18, slot 3
}

// ============================================================================
// ?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z  retail 0x1800eef70
// ============================================================================
// Three instructions: `MOV RCX,[RCX+0x40] / MOV R10,[RCX] / JMP [R10+0x10]` —
// `_platformData->Bind(context, data, first, count)` through
// `Video::ConstantBufferData` slot 2, unguarded.
//
// THE BODY MOVED HERE FROM `runtime/src/video/companion_data.cpp`, which is
// ENDGAME B.3.5 block A's file: the class is this block's, so the member belongs
// with the rest of it, and A's registered door is now a one-line delegation. The
// REGISTRATION did not move — `?Bind@ConstantBuffer@Video@@…` and 0x1800eef70
// are still registered by `companion_data.cpp`, exactly where B35_FILEMAP says
// they are.
//
// ENDGAME B.3.5 block J: this IS block J, and it converts. `+0x10` is slot 2 of
// `Video::ConstantBufferData`, which CompanionData.h names `Bind`; the virtual call
// reads the same live vptr at the same offset. The receiver keeps its bridge cast
// because `_platformData` is a pinned `void*`.
//
// AND THE SLOT'S DECLARED SIGNATURE WAS WRONG, which this conversion is what
// found. CompanionData.h had the fourth parameter as `void* unused2`; retail's own
// mangling of THIS function — `?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z`
// — says `(Context&, void*, int, int)`, and `?BindResources@Context@Video@@`'s five
// dispatches build that argument with `XOR R9D,R9D`, a DWORD zero. Corrected there
// (with the disassembly), so `first` passes as the int it always was instead of
// through a pointer cast.
void Video::ConstantBuffer::Bind(Context& context, void* data,
                                int32_t first, int32_t count) {
    Companion(_platformData)->Bind(&context, data, first, count);
}

extern "C" bool RtVideo_ConstantBuffer_PlatformCreate(void* self) {
    return static_cast<Video::ConstantBuffer*>(self)
        ->Video::ConstantBuffer::PlatformCreate();
}

extern "C" void RtVideo_ConstantBuffer_EndConstants(void* self, int32_t unread) {
    static_cast<Video::ConstantBuffer*>(self)
        ->Video::ConstantBuffer::EndConstants(unread);
}

extern "C" int32_t RtVideo_ConstantBuffer_GetNextUploadIndex(const void* self) {
    return static_cast<const Video::ConstantBuffer*>(self)
        ->Video::ConstantBuffer::GetNextUploadIndex();
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

// The `slot = prev->companion ? prev->companion->vtbl[3]() : prev->+0x14 + 1`
// helper this file used to carry IS `?GetNextUploadIndex@ConstantBuffer@Video@@`
// (0x1800eef90) — see the member above. The two `int*` branch counters it took
// are gone with the other three dead counters.

void BeginBuffer(Video::ConstantBuffer* buf, int prefixLit, int nameLit, int32_t i10,
                 int32_t i14, bool b18, bool b19, int32_t i20) {
    System::String prefix = StrLit(prefixLit);   // retail Sets the PREFIX first
    System::String name   = StrLit(nameLit);
    buf->Video::ConstantBuffer::BeginConstants(&name, &prefix,
                                               i10, i14, b18, b19, i20);
    // …and releases the NAME first: reverse declaration order, which is the
    // destruction order the two locals now get for free.
}

void AddDecl(Video::ConstantBuffer* buf, int typeLit, int nameLit, int32_t size) {
    System::String type = StrLit(typeLit);       // retail Sets the TYPE first
    System::String name = StrLit(nameLit);
    buf->Video::ConstantBuffer::AddConstant(&name, &type, size);
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

// The buffer offsets are retail's own `LEA R15,[…]` immediates and stay the
// evidence for which buffer each call acts on; `BufAt` turns one into the real
// embedded member. (bc_'s is the one that is easy to get wrong: MSVC does
// `ADD R15,0x2008` and then passes R15 itself, which reads like `this`.)
inline Video::ConstantBuffer* BufAt(Video::Context* ctx, uint32_t off) {
    return reinterpret_cast<Video::ConstantBuffer*>(
        reinterpret_cast<uint8_t*>(ctx) + off);
}
}  // namespace

void Video::Context::AssignConstants() {
    for (const BufRow& b : kBufRows) {
        Video::ConstantBuffer* buf = BufAt(this, b.bufOff);
        const int32_t slot = (b.prevOff < 0)
            ? b.i14
            : BufAt(this, static_cast<uint32_t>(b.prevOff))
                  ->Video::ConstantBuffer::GetNextUploadIndex();
        BeginBuffer(buf, b.prefixLit, b.nameLit, b.i10, slot,
                    b.b18 != 0, b.b19 != 0, b.i20);
        for (int i = 0; i < b.ndecls; ++i)
            AddDecl(buf, b.decls[i].typeLit, b.decls[i].nameLit,
                    b.decls[i].size);
        if (b.platformCreate)
            buf->Video::ConstantBuffer::PlatformCreate();
        else
            buf->Video::ConstantBuffer::EndConstants(0);
    }

    // ---- tc_ / TransformConstants -----------------------------------------
    // The only buffer whose constant NAME is formatted rather than a literal,
    // and the only one with b19 set and a non-zero +0x20 (0x40).
    {
        Video::ConstantBuffer* buf = BufAt(this, kOffTransform);
        const int32_t slot = BufAt(this, kOffLight)
                                 ->Video::ConstantBuffer::GetNextUploadIndex();
        BeginBuffer(buf, kLit_tc_, kLit_TransformConstants, 5, slot,
                    true, true, 0x40);

        System::String type = StrLit(kLit_float4x4);
        // Retail leaves this slot uninitialised and lets Format CONSTRUCT into
        // it; `Format` returns a fresh String by value, which is the same thing.
        System::String name =
            System::String::Format(RtVideoShaderConstantLiteral(kLit_transformFmt), 52);
        buf->Video::ConstantBuffer::AddConstant(&name, &type, 208);
        // name then type, retail's release order — reverse declaration order.

        buf->Video::ConstantBuffer::EndConstants(0);  // the arg is UNSET at the
                                                      // retail call site, and
                                                      // the callee overwrites
                                                      // RDX before reading it
    }

    // ---- bc_ / BoneConstants, and the tail EndConstants --------------------
    {
        Video::ConstantBuffer* buf = BufAt(this, kOffBone);
        const int32_t slot = BufAt(this, kOffTransform)
                                 ->Video::ConstantBuffer::GetNextUploadIndex();
        BeginBuffer(buf, kLit_bc_, kLit_BoneConstants, 6, slot, true, false, 0);
        for (const DeclRow& d : kBoneDecls)
            AddDecl(buf, d.typeLit, d.nameLit, d.size);
        buf->Video::ConstantBuffer::EndConstants(0xcc0);   // retail's tail JMP
    }
}

extern "C" void RtVideo_Context_AssignConstants(void* self) {
    static_cast<Video::Context*>(self)->Video::Context::AssignConstants();
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
    // ENDGAME B.3.5 block D1 — NEW ROW. `?GetNextUploadIndex@` (0x1800eef90,
    // ordinal 2831) was the body this file carried as the file-local
    // `NextSlotAfter` with no door at all; `?AssignConstants@` drives it seven
    // times per Context construction. The name was checked against
    // standalone/rt_export_addrs.inc, and 0x1800eef90 appeared nowhere in the
    // tree before this row.
    RegisterRuntimeExport("?GetNextUploadIndex@ConstantBuffer@Video@@QEBAHXZ",
                          (void*)&RtVideo_ConstantBuffer_GetNextUploadIndex); ++n;
    RegisterRuntimeAddr(0x1800eef90, (void*)&RtVideo_ConstantBuffer_GetNextUploadIndex);

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
