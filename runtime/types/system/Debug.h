#pragma once
// ============================================================================
// runtime/types/system/Debug.h — System::Debug (B3R W0-SM M2)
// ============================================================================
//
// Retail Runtime-x64.dll, image base 0x180000000. Every export of the class is
// declared below with its address; the signatures are read off the mangled
// names in the DLL's export directory. Only `_disableHandler` is DEFINED
// (runtime/src/system/debug.cpp): it is the one member this tree touches, and
// it was the mint's last storage row. Everything else is unported and nothing
// calls it -- declared, deliberately undefined, so a call is a link error.
//
// `_disableHandler` is one byte: `?DisableExceptions@` (0x1800c5870) is
// `MOV byte ptr [0x1801fc708],CL; RET`, every .text access to the cell is
// byte-width, and the next export (?_hideDisplay@Error@) starts one byte later.
// Retail's only READER is `ExceptionHandler` (0x1800c5b6f); its writers are
// `ExceptionHandler` x2, `DisableExceptions` and Video::Device::
// EnumerateVideoModes x2 (0x1800f8671 / 0x1800f8697), whose port writes it.
// The standalone installs its own VEH and never calls `Startup`, so nothing
// here reads the value.
// ============================================================================

struct _EXCEPTION_POINTERS;

namespace System {

class Debug {
public:
    // `W4ErrorResponse@12@`: an int-sized enum nested in Debug. Its values are
    // not read yet, so none is invented here.
    enum ErrorResponse : int;
    // P6A?AW4ErrorResponse@12@PEAXPEADH@Z
    typedef ErrorResponse (*ExceptionCallback)(void* context, char* text, int code);

    static void DisableExceptions(bool disable);                          // 0x1800c5870
    static ExceptionCallback SetExceptionCallback(ExceptionCallback cb);  // 0x1800c5880
    static void Shutdown();                                               // 0x1800c5c30
    static void Startup();                                                // 0x1800c5c50
    static void ForceDump();                                              // 0x1800c5f00
    static void GetDumpPath(int index, char* out);                        // 0x1800c5f10
    Debug& operator=(const Debug& other);                                 // 0x18007c0a0

    // ?_disableHandler@Debug@System@@0_NA (0x1801fc708). PRIVATE STATIC in
    // retail (`0…A`); PUBLIC here because Video::Device::EnumerateVideoModes
    // (runtime/src/video/enumerate_video_modes.cpp) writes it, as retail's does.
    static bool _disableHandler;

private:
    static ErrorResponse DefaultExceptionHandler(void* context, char* text, int code);  // 0x1800c5890
    static long ExceptionHandler(_EXCEPTION_POINTERS* info);              // 0x1800c5b60
    static long MsgBox(const char* text, const char* caption, unsigned int type);  // 0x1800c5c90

    static ExceptionCallback _exceptionCallback;  // ?_exceptionCallback@Debug@System@@0P6A…EA  0x1801fc700
    static void* _exceptionHandler;               // ?_exceptionHandler@Debug@System@@0PEAXEA  0x1801fc710
};

}  // namespace System
