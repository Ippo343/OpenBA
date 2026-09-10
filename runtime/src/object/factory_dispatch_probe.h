#pragma once
// ============================================================================
// runtime/src/object/factory_dispatch_probe.h — WAVE 11 FOLLOW-UP 5, lane TINFO
// ============================================================================
// One call at every site in this tree that calls through
// `BaseTypeInfo::factoryFn` / `inPlaceFactoryFn`, so the question "can promoting
// a minted `?_typeInfo@` row put one of the 104 factory ABORT TRAPS in front of
// a live dispatch?" is answered by measurement. Read the banner of
// factory_dispatch_probe.cpp — including its two controls — before trusting a
// zero from it.
//
// ADDING A SITE: append to the enum, add a row to `g_site[]` in the .cpp (the
// array is sized by RT_FACTORY_DISPATCH_SITE_COUNT, so a missed row will not
// compile), and call Observe with the `info` you are about to dispatch through.
// ============================================================================

enum {
    RT_FACTORY_DISPATCH_EXTERNAL_LOAD = 0,   // Object::External::Load
    RT_FACTORY_DISPATCH_FACTORY_CREATE,      // Object::Factory::CreateObject
    RT_FACTORY_DISPATCH_ADD_CONTROLLER,      // UI::DialogInstance::AddControllerTab
    RT_FACTORY_DISPATCH_SITE_COUNT
};

// Called with the BaseTypeInfo the site is about to dispatch through, BEFORE
// the call, so a dispatch that ends in a trap stub is still counted.
extern "C" void RtFactoryDispatchObserve(int site, const void* info);

// Printed from standalone/main.cpp's shutdown block. Always, including zero.
extern "C" void RtFactoryDispatchReport();
