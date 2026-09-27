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
#include "../../types/object/External.h"   // Object::ExternalFactory::_singleton
#include "../../types/object/Internal.h"   // Object::InternalFactory::_singleton

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
    // ENDGAME B.3.9 OBJ (B39_RULES D7): the minted storage rows ARE the class
    // statics from here on (name door, address door, manifest publish). Every
    // *PortInit runs before the mint; a call after it aborts.
    RtMintRedirectStorage("?_singleton@ExternalFactory@Object@@0PEAV12@EA",
                          &::Object::ExternalFactory::_singleton);
    RtMintRedirectStorage("?_singleton@InternalFactory@Object@@0PEAV12@EA",
                          &::Object::InternalFactory::_singleton);
    RtMintRedirectStorage("?_singleton@Factory@Object@@0PEAV12@EA",
                          &::Object::Factory::_singleton);
    RtMintRedirectStorage("0x1801fc6d0", &::Object::Factory::_compilerModule);
    int n = 0;
    n += RtObjectTypeSystemRegister();
    n += RtObjectFactoryFileExtensionRegister();
    n += RtObjectInternalRegister();
    n += RtObjectExternalRegister();
    // Wave 9 lane RES: the resource tree's own Set bodies, plus Clean. Nested
    // under the old DLL-era switch because it is the same class's state, with its
    // own switch so the twelve unexported Set doors can be A/B'd on their own.
        n += RtObjectExternalSetRegister();
    // Wave 9 lane RRES: the path half of the same class -- ExpandRelativePath,
    // the two path-stack readers, and Filename::GetPathExtension. Same nesting
    // and the same reason as lane RES's switch above: it is ExternalFactory
    // state, and the old DLL-era switch A/Bs the four doors on their own.
        n += RtObjectExternalPathRegister();
    // Wave 9 lane RRES follow-on: the private cache lookup the path half
    // feeds. Its own switch, so the one door can be A/B'd against the four
    // above without turning the whole class off.
        n += RtObjectExternalFindRegister();
    // Wave 10 lane EXTPTR. Nested under the old DLL-era switch because it is the
    // ExternalPtr half of the same class, with its own switch so the four
    // address doors can be A/B'd on their own: the old DLL-era switch leaves all
    // four ExternalPtr<T>::Serialize instantiations on the DLL, which is also
    // the way to A/B ?Serialize@ElementDescription@ against its callees.
        n += RtObjectExternalPtrSerializeRegister();
    n += RtObjectInternalPtrArraySerializeRegister();   // internal_ptr_array_serialize.cpp
    // Wave 11 lane OBJDATA. the old DLL-era switch leaves all seven
    // accessors on the DLL, which re-traps two slots each in
    // ??_7CompiledExternal / ??_7External / ??_7Internal and one in ??_7Base —
    // the same switch coupling reg_mint_promoted.cpp records for
    // ??_7SpriteSheet and the old DLL-era switch, and for the same reason.
        n += RtObjectTypeInfoAccessorsRegister();
        n += RtObjectFactoryDtorsRegister();
    // Wave 11 lane OBJEXT. Nested under the old DLL-era switch because both are
    // ExternalFactory state and behaviour, each with its own switch so its
    // doors can be A/B'd without turning the whole class off.
    //
    // ORDER IS LOAD-BEARING between these two and nothing else: PATHSTACK
    // registers 0x18002e220 and friends, and COMPILED's FindOrLoad reaches
    // ExpandRelativePath, which reads the path stack those bodies maintain.
    // Registration order does not change which body runs (both are reached by
    // name/address at CALL time, not at init), but keeping the container
    // underneath its user is how every other nest in this file reads.
        n += RtObjectExternalPathStackRegister();
        n += RtObjectExternalCompiledRegister();
    // Wave 11 follow-up 2 lane IOSTREAM. Nested under the old DLL-era switch for
    // the same reason as its four neighbours -- it is ExternalFactory state --
    // with its own switch because it is the one body in the class that drives a
    // stack-constructed IO stream, and the old DLL-era switch A/Bs exactly that.
        n += RtObjectExternalAddRegister();
    // Same nest, its own switch. SaveText is the only body in the class that
    // WRITES a file, so the old DLL-era switch A/Bs the write path alone --
    // which is also how its byte-comparison oracle gets its "theirs" side.
        n += RtObjectExternalSaveTextRegister();
    // Same nest, its own switch. Load is the only body in the class that
    // CREATES objects (through BaseTypeInfo::factoryFn) and the only one on the
    // resource read path, so the old DLL-era switch is the A/B that matters most
    // in this group.
        n += RtObjectExternalLoadRegister();
    // Wave 11 follow-up 3 lane AVL. Same nest, its own switch. The Set family
    // is registered FIRST because SerializeReferenceTable's read path is its
    // only caller in this tree -- registration order does not decide which body
    // runs (both are reached at CALL time), but keeping the container
    // underneath its user is how every other nest in this file reads.
        n += RtObjectReferenceSetRegister();
    // Its own switch again: the old DLL-era switch leaves the save/load body on
    // the DLL while keeping the Set family ours, which is the A/B that
    // separates "the container is wrong" from "the driver is wrong".
        n += RtObjectExternalRefTableRegister();

    // ---- BATCH 5 (2026-09-12) ---------------------------------------------
    // Registered UNCONDITIONALLY: DIRECTION.md §2 — a port registers
    // unconditionally, and no new the old DLL-era switch* flag is minted for one.
    //
    //   external_forceunload.cpp        ?ForceUnload@ExternalFactory@       0x1800beec0
    //   internalfactory_persist.cpp     ?PersistObject@InternalFactory@     0x1800bff10
    //
    // ForceUnload is deliberately NOT put behind the old DLL-era switch with the
    // Set family: its only mutation is one dword store, its lookup is lane
    // RES's already-registered `rtresset::Find`, and its only caller
    // (IO::Modification::SetupFileSystem) is ported in this same commit. Gating
    // it would mean flipping that switch left SetupFileSystem calling a
    // function whose address is not registered at cutover — a worse failure
    // than the one the switch exists to make cheap to bisect (the argument
    // array.cpp already makes for its two non-trivial UI specs).
    extern int RtObjectExternalForceUnloadRegister();
    extern int RtObjectInternalFactoryPersistRegister();
    n += RtObjectExternalForceUnloadRegister();
    n += RtObjectInternalFactoryPersistRegister();

    // ---- VTABLE BATCH 9 (2026-09-12) ---------------------------------------
    // Registered UNCONDITIONALLY, same reasoning as BATCH 5 above.
    //   external_list.cpp     Object::ExternalList
    // (ext_compilers.cpp -- Object::ExtensionCompiler + Object::ExternalCompiler
    // Type/TypeName and their ??1 doors -- is gone: ENDGAME B.3.9 retired both
    // minted tables, which were those bodies' only reader; retail's factories
    // for both return null, so nothing ever built one.)
    extern int RtObjectExternalListRegister();
    n += RtObjectExternalListRegister();

    // ---- BATCH A_operator_delete (2026-09-12), unconditional ---------------
    // class_operator_delete.cpp — the 28 remaining `??3<Class>@<Ns>@@SAXPEAX@Z`
    // class operator deletes across Collision2D/Graphics/Achievement/Audio/
    // Object/Video/Picking/Pathfinding. One shared body (rtcoll::MemFree,
    // verified byte-identical to the UI batch's reference row modulo RIP
    // displacements — see the file banner). Hooked here rather than split
    // across eight namespace port files because Collision2D and Pathfinding
    // have no rt_*_port.cpp of their own.
    extern int RtClassOperatorDeleteRegister();
    n += RtClassOperatorDeleteRegister();

    printf("[RtPort] Object: %d exports registered\n", n);
}

// ============================================================================
