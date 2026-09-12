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
// Wave 24 lane INTERNAL8. ?GetTypeIdForFileExtension@Factory@Object@@ — the
// ONE real row among the census's four `ordinal` gap rows (the other three are
// phantoms of already-ported exports). Own switch: it is an in-order walk of
// the WHOLE type tree called from two exe component paths, so "the extension
// lookup is wrong" has to be separable from lane A's type-system registrations.
extern int RtObjectFactoryFileExtensionRegister(); // factory_file_extension.cpp
extern int RtObjectInternalRegister();     // internal.cpp      (lane B)
extern int RtObjectExternalRegister();     // external.cpp      (lane C)
extern int RtObjectExternalSetRegister();  // external_set.cpp  (Wave 9 lane RES)
extern int RtObjectExternalPathRegister(); // external_path_win.cpp (Wave 9 lane RRES)
extern int RtObjectExternalFindRegister(); // external_find.cpp     (Wave 9 lane RRES)
// Wave 10 lane EXTPTR: Object::ExternalPtr<T>::Serialize — ONE 748-byte body at
// 25 unexported addresses, of which 4 are registered. It is what unblocks
// ?Serialize@ElementDescription@UI@@ (runtime/src/ui/elementdescription_serialize.cpp).
extern int RtObjectExternalPtrSerializeRegister();  // external_ptr_serialize.cpp
extern int RtObjectInternalPtrArraySerializeRegister();  // internal_ptr_array_serialize.cpp
// Wave 11 lane OBJDATA: the seven `?Type@` / `?TypeName@` accessors that were
// the entire TRAP set of four minted `??_7…@Object@@6B@` vtable rows. Not
// nested under TYPESYS/INTERNAL/EXTERNAL because the seven span all three
// (Base, Internal, External, CompiledExternal) and the row they unblock is one
// per class; its own switch keeps the four rows A/B-able together.
extern int RtObjectTypeInfoAccessorsRegister();     // object_typeinfo_accessors.cpp
// Wave 11 lane OBJLIFE: the three Object FACTORY destructors (~Factory,
// ~InternalFactory, ~ExternalFactory) plus the five unexported helper bodies
// they reach. Its own switch because all three are reachable only from
// Engine::Shutdown, which our tree never calls -- so this is the one part of
// the Object port whose absence a gameplay run cannot detect, and its presence
// should be as cheap to A/B as its absence.
extern int RtObjectFactoryDtorsRegister();          // factory_dtors.cpp
// Wave 11 lane OBJEXT. The two WRITERS of the path stack lane RRES ported the
// readers of (external_path.h names them as its explicit deferral), plus the
// four Collection::Array<PathStackEntry> bodies that were its stated blocker.
extern int RtObjectExternalPathStackRegister();     // external_path_stack.cpp
// Wave 11 lane OBJEXT. ExternalFactory::GetCompiledPath and ::FindOrLoad --
// one six-term concatenation MSVC emitted twice.
extern int RtObjectExternalCompiledRegister();      // external_compiled.cpp
// Wave 11 follow-up 2 lane IOSTREAM. ExternalFactory::Add -- the row OBJEXT
// OPENED when it ported FindOrLoad, which calls Add unconditionally on both
// branches. Blocked until now on the IO::VisitorStream LAYOUT (not, as the
// holdback said, on its constructor: that is exported).
extern int RtObjectExternalAddRegister();           // external_add.cpp
// Wave 11 follow-up 2 lane IOSTREAM. External::SaveText -- the TextFileStream
// half of the same blocker: exported ctor, missing LAYOUT.
extern int RtObjectExternalSaveTextRegister();      // external_savetext_win.cpp
// Wave 11 follow-up 2 lane IOSTREAM. External::Load -- the CompressedFileStream
// half, and a full deserialisation driver rather than a stream wrapper.
extern int RtObjectExternalLoadRegister();          // external_load.cpp
// Wave 11 follow-up 3 lane AVL. The `Collection::Set<ReferenceEntry>` family --
// the 0x10-BYTE-element instantiation, node size 0x30 -- and the one body that
// needs it: ExternalFactory::SerializeReferenceTable, held back since Wave 3.
// TWO switches, because the two halves carry different risk. The Set family is
// pure container code with a lockstep oracle; the Serialize body is on the
// save/load path, which is this project's most-cited failure mode.
extern int RtObjectReferenceSetRegister();          // reference_set.cpp
extern int RtObjectExternalRefTableRegister();      // external_reftable.cpp

void RtObjectPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_OBJECT")) {
        printf("[RtPort] BANISHED_RT_OBJECT=0 -- Object namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_TYPESYS"))  n += RtObjectTypeSystemRegister();
    if (RtWaveEnabled("BANISHED_RT_FILEEXT"))  n += RtObjectFactoryFileExtensionRegister();
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
    n += RtObjectInternalPtrArraySerializeRegister();   // internal_ptr_array_serialize.cpp
    // Wave 11 lane OBJDATA. BANISHED_RT_OBJTYPEINFO=0 leaves all seven
    // accessors on the DLL, which re-traps two slots each in
    // ??_7CompiledExternal / ??_7External / ??_7Internal and one in ??_7Base —
    // the same switch coupling reg_mint_promoted.cpp records for
    // ??_7SpriteSheet and BANISHED_RT_DELDTOR, and for the same reason.
    if (RtWaveEnabled("BANISHED_RT_OBJTYPEINFO"))
        n += RtObjectTypeInfoAccessorsRegister();
    if (RtWaveEnabled("BANISHED_RT_FACTORYDTOR")) {
        n += RtObjectFactoryDtorsRegister();
        // The synthesised oracle for those three. Opt-in
        // (BANISHED_RT_FACTORYDTOR_PROBE=1) and run right after the
        // registration, because the "ours" side calls the ported bodies
        // directly by symbol and the "theirs" side calls RtOriginal, so
        // neither depends on registration order — but the report reads better
        // next to the line that registered them.
        extern void RtObjectFactoryDtorsProbe();
        RtObjectFactoryDtorsProbe();
    }
    // Wave 11 lane OBJEXT. Nested under BANISHED_RT_EXTERNAL because both are
    // ExternalFactory state and behaviour, each with its own switch so its
    // doors can be A/B'd without turning the whole class off.
    //
    // ORDER IS LOAD-BEARING between these two and nothing else: PATHSTACK
    // registers 0x18002e220 and friends, and COMPILED's FindOrLoad reaches
    // ExpandRelativePath, which reads the path stack those bodies maintain.
    // Registration order does not change which body runs (both are reached by
    // name/address at CALL time, not at init), but keeping the container
    // underneath its user is how every other nest in this file reads.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_PATHSTACK"))
        n += RtObjectExternalPathStackRegister();
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_COMPILED"))
        n += RtObjectExternalCompiledRegister();
    // Wave 11 follow-up 2 lane IOSTREAM. Nested under BANISHED_RT_EXTERNAL for
    // the same reason as its four neighbours -- it is ExternalFactory state --
    // with its own switch because it is the one body in the class that drives a
    // stack-constructed IO stream, and BANISHED_RT_EXTADD=0 A/Bs exactly that.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_EXTADD"))
        n += RtObjectExternalAddRegister();
    // Same nest, its own switch. SaveText is the only body in the class that
    // WRITES a file, so BANISHED_RT_SAVETEXT=0 A/Bs the write path alone --
    // which is also how its byte-comparison oracle gets its "theirs" side.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_SAVETEXT"))
        n += RtObjectExternalSaveTextRegister();
    // Same nest, its own switch. Load is the only body in the class that
    // CREATES objects (through BaseTypeInfo::factoryFn) and the only one on the
    // resource read path, so BANISHED_RT_EXTLOAD=0 is the A/B that matters most
    // in this group.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_EXTLOAD"))
        n += RtObjectExternalLoadRegister();
    // Wave 11 follow-up 3 lane AVL. Same nest, its own switch. The Set family
    // is registered FIRST because SerializeReferenceTable's read path is its
    // only caller in this tree -- registration order does not decide which body
    // runs (both are reached at CALL time), but keeping the container
    // underneath its user is how every other nest in this file reads.
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_REFSET"))
        n += RtObjectReferenceSetRegister();
    // Its own switch again: BANISHED_RT_REFTABLE=0 leaves the save/load body on
    // the DLL while keeping the Set family ours, which is the A/B that
    // separates "the container is wrong" from "the driver is wrong".
    if (RtWaveEnabled("BANISHED_RT_EXTERNAL") && RtWaveEnabled("BANISHED_RT_REFTABLE"))
        n += RtObjectExternalRefTableRegister();
    printf("[RtPort] Object: %d exports registered\n", n);
}
