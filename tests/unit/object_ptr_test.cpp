// ============================================================================
// tests/unit/object_ptr_test.cpp — ENDGAME B.3.1 step 1, the smart pointers.
//
// WHAT IS AND IS NOT COVERED HERE
// -------------------------------
// `Object::ExternalPtr<T>` and `Object::InternalPtr<T>`/`InternalBasePtr` are
// NON-OWNING views: one raw pointer, no refcount, nothing to run on copy or
// destruction (docs/runtime_port/endgame_b/SMARTPTR_INVENTORY.md §1, §5.2).
// So the whole of their observable behaviour on the host is the trivial
// semantics tested below — and those semantics are load-bearing, not trivia:
// `Collection::Array` bulk-moves these eight bytes with a plain qword copy and
// an explicit "NO refcount touch" (runtime/src/collection/array.cpp:983), so a
// user-provided copy ctor or destructor would be SILENTLY BYPASSED. The
// `is_trivially_*` checks below are the regression test for that hazard.
//
// `Serialize` is NOT tested here and cannot be: it needs a live
// `IO::Serializer` with a real `ReaderWriter` vtable, `Object::ExternalFactory`
// (two AVL trees and a singleton), `Object::Base::IsDerivedFrom` and the
// `?_typeInfo@T@…` records — none of which exists on the host. It is covered by
// the real run (`make sa-run`, and the resave gate on the coordinator's merge),
// which is what has always verified it.
//
// `CheckPtr` and `AssignId` are deliberately untestable no-ops: they are bare
// `RET`s in retail AND in the profile build, so there is nothing to assert
// beyond "they compile and do nothing", which the calls below do.
// ============================================================================
#include "unit.h"

#include <type_traits>

#include "../../runtime/types/object/External.h"
#include "../../runtime/types/object/Internal.h"

namespace {

// Stand-ins for the pointees. Nothing is dereferenced through a live object
// here; only the pointer value is ever observed.
RtObject::External  g_extA;
RtObject::External  g_extB;
RtObject::Internal  g_intA;
RtObject::Internal  g_intB;

using ExtPtr  = Object::ExternalPtr<RtObject::External>;
using BasePtr = Object::InternalBasePtr;
using IntPtr  = Object::InternalPtr<RtObject::Internal>;

}  // namespace

// ---------------------------------------------------------------------------
// Layout and triviality — the array-bulk-copy hazard (SMARTPTR_INVENTORY §5.4)
// ---------------------------------------------------------------------------
UNIT_TEST(object_ptr_sizeof_is_one_pointer) {
    CHECK_EQ(sizeof(ExtPtr), sizeof(void*));
    CHECK_EQ(sizeof(ExtPtr), (size_t)8);
    CHECK_EQ(sizeof(Object::ExternalPtr<RtObject::ExternalList>), (size_t)8);
    CHECK_EQ(sizeof(BasePtr), (size_t)8);
    CHECK_EQ(sizeof(IntPtr), (size_t)8);   // the typed wrapper adds nothing
    CHECK_EQ(alignof(ExtPtr), alignof(void*));
    CHECK_EQ(alignof(BasePtr), alignof(void*));
}

UNIT_TEST(object_ptr_is_trivially_copyable_and_destructible) {
    // If any of these ever fails, every Array<T> resize in the engine has
    // started silently skipping whatever the new copy ctor/dtor was to do.
    CHECK(std::is_trivially_copyable<ExtPtr>::value);
    CHECK(std::is_trivially_destructible<ExtPtr>::value);
    CHECK(std::is_trivially_copy_constructible<ExtPtr>::value);
    CHECK(std::is_trivially_copy_assignable<ExtPtr>::value);

    CHECK(std::is_trivially_copyable<BasePtr>::value);
    CHECK(std::is_trivially_destructible<BasePtr>::value);
    CHECK(std::is_trivially_copy_constructible<BasePtr>::value);
    CHECK(std::is_trivially_copy_assignable<BasePtr>::value);

    CHECK(std::is_trivially_copyable<IntPtr>::value);
    CHECK(std::is_trivially_destructible<IntPtr>::value);
}

// ---------------------------------------------------------------------------
// ExternalPtr<T>
// ---------------------------------------------------------------------------
UNIT_TEST(external_ptr_default_ctor_is_null) {
    // Retail's ExternalList::Find writes the not-found case as one `*ret = 0`
    // qword store, so a default-constructed pointer must be null.
    ExtPtr p;
    CHECK(p.Get() == nullptr);
    CHECK(!p);
    CHECK(!static_cast<bool>(p));
}

UNIT_TEST(external_ptr_value_ctor_and_accessors) {
    ExtPtr p(&g_extA);
    CHECK(p.Get() == &g_extA);
    CHECK(static_cast<bool>(p));
    CHECK(p.operator->() == &g_extA);
    CHECK(&(*p) == &g_extA);
}

UNIT_TEST(external_ptr_copy_is_a_plain_alias) {
    ExtPtr a(&g_extA);
    ExtPtr b(a);                 // copy ctor: no refcount to touch
    CHECK(b.Get() == &g_extA);
    CHECK(a == b);
    CHECK(!(a != b));

    ExtPtr c;
    c = a;                       // copy assign
    CHECK(c.Get() == &g_extA);

    c = &g_extB;                 // assign from a raw pointer
    CHECK(c.Get() == &g_extB);
    CHECK(a != c);
    CHECK(a.Get() == &g_extA);   // the source is untouched
}

UNIT_TEST(external_ptr_survives_a_bulk_byte_copy) {
    // Exactly what Collection::Array<T>::operator= does to these eight bytes.
    ExtPtr src(&g_extA);
    ExtPtr dst;
    std::memcpy(&dst, &src, sizeof(ExtPtr));
    CHECK(dst.Get() == &g_extA);
    CHECK(src.Get() == &g_extA);
}

UNIT_TEST(external_ptr_self_assignment_is_a_no_op) {
    ExtPtr a(&g_extA);
    ExtPtr& r = a;
    a = r;
    CHECK(a.Get() == &g_extA);
}

// ---------------------------------------------------------------------------
// InternalBasePtr / InternalPtr<T>
// ---------------------------------------------------------------------------
UNIT_TEST(internal_base_ptr_default_ctor_is_null) {
    // ??0InternalBasePtr@Object@@QEAA@XZ (0x1800062c0): `mov qword [rcx], 0`.
    BasePtr p;
    CHECK(p.Get() == nullptr);
    CHECK(!p);
}

UNIT_TEST(internal_base_ptr_value_ctor_and_accessors) {
    // ??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z (0x1800062a0).
    BasePtr p(&g_intA);
    CHECK(p.Get() == &g_intA);
    CHECK(static_cast<bool>(p));
    CHECK(p.operator->() == &g_intA);
    CHECK(&(*p) == &g_intA);
}

UNIT_TEST(internal_base_ptr_copy_is_a_plain_alias) {
    // ?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z (0x1800062b0) is
    // `mov rax,[rdx]; mov [rcx],rax`. The two pointers are then ALIASES —
    // no refcount, no ownership transfer.
    BasePtr a(&g_intA);
    BasePtr b(a);
    CHECK(b.Get() == &g_intA);
    CHECK(a == b);

    BasePtr c;
    c = a;
    CHECK(c.Get() == &g_intA);

    BasePtr d(&g_intB);
    CHECK(a != d);
}

UNIT_TEST(internal_base_ptr_shim_copy_matches_assignment) {
    // The registered C shim (RtObject_InternalBasePtr_Copy) goes through this
    // forwarder because retail's `Copy` is protected (`IEAA`).
    BasePtr src(&g_intB);
    BasePtr dst;
    BasePtr::Shim_Copy(&dst, &src);
    CHECK(dst.Get() == &g_intB);
}

UNIT_TEST(internal_base_ptr_checkptr_and_assignid_are_no_ops) {
    // Bare RETs in retail AND in the profile build. They must never validate:
    // a stale InternalBasePtr does not self-invalidate in any shipped build
    // (memory reference_checkptr_is_a_ret_in_every_build).
    BasePtr p(&g_intA);
    BasePtr::Shim_CheckPtr(&p);
    BasePtr::Shim_AssignId(&p);
    CHECK(p.Get() == &g_intA);   // unchanged, and no validation happened

    BasePtr stale(&g_intA);
    stale = BasePtr(reinterpret_cast<RtObject::Internal*>(0x1234));
    BasePtr::Shim_CheckPtr(&stale);
    CHECK(stale.Get() == reinterpret_cast<RtObject::Internal*>(0x1234));
}

UNIT_TEST(internal_ptr_typed_wrapper_shares_the_base_bytes) {
    IntPtr p(&g_intA);
    CHECK(p.Get() == &g_intA);
    CHECK(static_cast<bool>(p));
    CHECK(static_cast<const BasePtr&>(p).Get() == &g_intA);

    IntPtr q;
    CHECK(q.Get() == nullptr);
    q = p;
    CHECK(q.Get() == &g_intA);

    // and the same bulk-copy invariant as the base
    IntPtr r;
    std::memcpy(&r, &p, sizeof(IntPtr));
    CHECK(r.Get() == &g_intA);
}
