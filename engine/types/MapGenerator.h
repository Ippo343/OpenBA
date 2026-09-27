#pragma once
// engine/types/MapGenerator.h — FORWARDER (ENDGAME B.3.8 block LOOP, rule B8-4)
//
// `MapGenerator` is a real C++ class now: code/Game/Game/MapGenerator.h
// (an Object::External subclass with a compiler vtable; field list, offsets and
// the retail slot map live there). One definition of the name. The old
// global `ExternalRef` / `DynArrayHeader` / `BoolIntPair` helpers had no reader
// outside mapgenerator.cpp; the first two are MapGenerator:: nested types now.
#include "Game/MapGenerator.h"
