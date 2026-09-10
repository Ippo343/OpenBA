// ============================================================================
// runtime/src/object/rt_object_port.cpp — RtObjectPortInit()  (Wave 3)
// ============================================================================
// One entry point for the Object namespace: the reflection system (Base /
// Type / BaseTypeInfo / Factory), the internal handle table (Internal /
// InternalBasePtr / InternalFactory) and the external resource table
// (External / ExternalFactory / ExternalList).
//
// Same shape as RtSystemPortInit(): each lane owns exactly ONE source file and
// exposes exactly ONE Register function, so no two lanes ever edit the same
// file. This file and runtime/funcs/rt_port.h are the only shared surface and
// they are already written.
//
// Switches, outermost first:
//   BANISHED_RT_OBJECT=0    leave the whole Object namespace on the DLL
//   BANISHED_RT_TYPESYS=0   ... just Base / Type / BaseTypeInfo / Factory
//   BANISHED_RT_INTERNAL=0  ... just Internal / InternalBasePtr / InternalFactory
//   BANISHED_RT_EXTERNAL=0  ... just External / ExternalFactory / ExternalList
//
// STATE WARNING (PLAN.md §4.1 decision 5, §4.3 risk 3). Unlike Waves 1 and 2,
// these classes own state and are constructed by the DLL. Registration
// redirects OUR call sites only; the DLL's own internal calls still land in
// the DLL, and objects the DLL built carry DLL vtables. Every port in this
// wave must therefore operate on the LIVE DLL singleton's own tables — the
// allocator pattern from Wave 2 — and must never keep a private replica that
// the DLL can desynchronise. A class ported half-way with two vtables in play
// is the swap-remove UAF family again
// (reference_internalfactory_swapremove_wild_write.md).
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtObjectTypeSystemRegister();   // type_system.cpp   (lane A)
extern int RtObjectInternalRegister();     // internal.cpp      (lane B)
extern int RtObjectExternalRegister();     // external.cpp      (lane C)
extern int RtObjectExternalSetRegister();  // external_set.cpp  (Wave 9 lane RES)
extern int RtObjectExternalPathRegister(); // external_path_win.cpp (Wave 9 lane RRES)
extern int RtObjectExternalFindRegister(); // external_find.cpp     (Wave 9 lane RRES)
// Wave 10 lane EXTPTR: Object::ExternalPtr<T>::Serialize — ONE 748-byte body at
// 25 unexported addresses, of which 4 are registered. It is what unblocks
// ?Serialize@ElementDescription@UI@@ (runtime/src/ui/elementdescription_serialize.cpp).
extern int RtObjectExternalPtrSerializeRegister();  // external_ptr_serialize.cpp
// Wave 11 lane OBJDATA: the seven `?Type@` / `?TypeName@` accessors that were
// the entire TRAP set of four minted `??_7…@Object@@6B@` vtable rows. Not
// nested under TYPESYS/INTERNAL/EXTERNAL because the seven span all three
// (Base, Internal, External, CompiledExternal) and the row they unblock is one
// per class; its own switch keeps the four rows A/B-able together.
extern int RtObjectTypeInfoAccessorsRegister();     // object_typeinfo_accessors.cpp

void RtObjectPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_OBJECT")) {
        printf("[RtPort] BANISHED_RT_OBJECT=0 -- Object namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_TYPESYS"))  n += RtObjectTypeSystemRegister();
    if (RtWaveEnabled("BANISHED_RT_INTERNAL")) n += RtObjectInternalRegister();
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL")) n += RtObjectExternalRegister();
    // Wave 9 lane RES: the resource tree's own Set bodies, plus Clean. Nested
    // under BANISHED_RT_EXTERNAL because it is the same class's state, with its
    // own switch so the twelve unexported Set doors can be A/B'd on their own.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_RESSET"))
        n += RtObjectExternalSetRegister();
    // Wave 9 lane RRES: the path half of the same class -- ExpandRelativePath,
    // the two path-stack readers, and Filename::GetPathExtension. Same nesting
    // and the same reason as lane RES's switch above: it is ExternalFactory
    // state, and BANISHED_RT_RESPATH=0 A/Bs the four doors on their own.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_RESPATH"))
        n += RtObjectExternalPathRegister();
    // Wave 9 lane RRES follow-on: the private cache lookup the path half
    // feeds. Its own switch, so the one door can be A/B'd against the four
    // above without turning the whole class off.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_RESFIND"))
        n += RtObjectExternalFindRegister();
    // Wave 10 lane EXTPTR. Nested under BANISHED_RT_EXTERNAL because it is the
    // ExternalPtr half of the same class, with its own switch so the four
    // address doors can be A/B'd on their own: BANISHED_RT_EXTPTR=0 leaves all
    // four ExternalPtr<T>::Serialize instantiations on the DLL, which is also
    // the way to A/B ?Serialize@ElementDescription@ against its callees.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_EXTPTR"))
        n += RtObjectExternalPtrSerializeRegister();
    // Wave 11 lane OBJDATA. BANISHED_RT_OBJTYPEINFO=0 leaves all seven
    // accessors on the DLL, which re-traps two slots each in
    // ??_7CompiledExternal / ??_7External / ??_7Internal and one in ??_7Base —
    // the same switch coupling reg_mint_promoted.cpp records for
    // ??_7SpriteSheet and BANISHED_RT_DELDTOR, and for the same reason.
    if (RtWaveEnabled("BANISHED_RT_OBJTYPEINFO"))
        n += RtObjectTypeInfoAccessorsRegister();
    printf("[RtPort] Object: %d exports registered\n", n);
}
