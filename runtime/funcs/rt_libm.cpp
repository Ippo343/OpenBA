// runtime/funcs/rt_libm.cpp — see rt_libm.h for why this seam exists.
#include "rt_libm.h"

#include "../src/crt/msvc_libm.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace {

void InstallPorted() {
    RtLibm::Sqrt  = &MsvcLibm::Sqrtf;
    RtLibm::Sin   = &MsvcLibm::Sinf;
    RtLibm::Cos   = &MsvcLibm::Cosf;
    RtLibm::Atan2 = &MsvcLibm::Atan2f;
    RtLibm::Pow   = &MsvcLibm::Powf;
}

}  // namespace

namespace RtLibm {

// Static-init defaults are the ported bodies, so a caller that runs before
// Init() already gets retail's arithmetic rather than our own libm's.
float (*Sqrt)(float)           = &MsvcLibm::Sqrtf;
float (*Sin)(float)            = &MsvcLibm::Sinf;
float (*Cos)(float)            = &MsvcLibm::Cosf;
float (*Atan2)(float, float)   = &MsvcLibm::Atan2f;
float (*Pow)(float, float)     = &MsvcLibm::Powf;


void Init() {
    InstallPorted();
}

}  // namespace RtLibm
