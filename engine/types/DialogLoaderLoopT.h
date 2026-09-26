#pragma once
// engine/types/DialogLoaderLoopT.h — FORWARDER (ENDGAME B.3.8 block LOOP, B38_RULES D8 / B8-4)
//
// The exe-side POD mirror that stood here (with `m_` spellings of the Runtime
// fields) is retired: every loop the exe builds is now a real subclass of
// the real class runtime/types/engine/UpdateLoop.h (code/Game/Game/{InnerLoaderLoop,GameLoaderLoop,GameLoadDialogLoopT,
// MenuDialogLoopT,GameUpdateLoop}.h), and the last users of the POD's fields
// were converted with them. One definition of the name: the global spelling is
// an alias of the class.
#include "engine/UpdateLoop.h"   // -Iruntime/types
using DialogLoaderLoopT = Engine::DialogLoaderLoopT;
