#pragma once
// src/render/dx11/foliage_aa/cutout_rewrite.h -- the cutout pattern match and the in-memory
// rewrite of an alpha-tested ps_4_0 blob (FAA-SPIKE; no D3D, no retail bytes: it transforms the
// blob it is handed).
//
// The pattern (FOLIAGE_AA.md §3):
//   sample*   rA.xyzw, uv, tK, sK
//   [mad      rA.c, ..., rA.c]          the season term (optional)
//   add       rT.c, rA.c, l(-cut)       (either operand order)
//   lt        rT.c, rT.c, l(0)
//   discard_nz rT.c
//   ...
//   mov       o0.w, l(0)                exactly once ("colour" programs)
//   ret                                 last and only
#include "foliage_aa/dxbc.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace FoliageAA {

struct Match {
    bool        container = false, program = false, identity = false, checksum = false;
    std::string err;
    int         discards = 0, flowControl = 0, rets = 0;
    bool        pattern = false;
    std::string why;
    int         iSample = -1, iMad = -1, iAdd = -1, iLt = -1, iDiscard = -1, iO0w = -1, iTemps = -1,
                iRet = -1;
    int         o0wCount = 0;
    float       cut = 0.0f;
    uint32_t    temps = 0;
    Dxbc::Operand alpha;      // the add's alpha source (rA.c)
    uint32_t    alphaComp = 0;
    bool        colour = false;   // pattern + `mov o0.w, l(0)` exactly once
    uint32_t    sampleOp = 0;
    Dxbc::Program prog;
    Dxbc::Container cont;
};

void Analyse(const uint8_t* blob, size_t size, Match* m);

// kMode 2..6 of faa_spike.h. Returns false when the emit fails its own re-parse.
bool Transform(const Match& m, int mode, float fillK, bool fillAniso, std::vector<uint8_t>* out);

}  // namespace FoliageAA
