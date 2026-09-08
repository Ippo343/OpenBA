#!/usr/bin/env python3
"""Emit the 37 repetitive halves of runtime/src/object/deleting_dtor.cpp.

Run once; the output is committed as ordinary source. It exists so the 37 rows,
the 37 entry points and the 37 registrations are typed ONCE and cannot drift
against each other, and so every registration is a LITERAL address that
tools/runtime_census.py can see (reference_census_cannot_see_nonliteral_
registrations.md -- 12 live ports were invisible for exactly this reason).

    python tools/gen_deleting_dtor.py > /tmp/frag.txt
"""

# (deleting dtor VA, class tag, elemSize, ~Class VA, is ~Class ported today,
#  probe-safe = its ~Class is a pure vtable restamp with no other effect)
ROWS = [
    (0x180001680, "Achievement",             0x058, 0x18009e180, 1, 0),
    (0x180001a90, "AchievementInstance",     0x028, 0x18009e400, 0, 0),
    (0x180001d70, "Goal",                    0x030, 0x18009edb0, 1, 0),
    (0x1800020c0, "GoalInstance",            0x020, 0x18009ef40, 1, 0),
    (0x180002d60, "MaterialInstance",        0x060, 0x180103a00, 0, 0),
    (0x1800037b0, "SpriteSheet",             0x058, 0x18011d670, 0, 0),
    (0x180006440, "Internal",                0x010, 0x1800bfdb0, 1, 0),
    (0x1800065b0, "Base",                    0x008, 0x1800c0360, 0, 1),
    (0x180006640, "External",                0x020, 0x1800bc950, 1, 0),
    (0x18000b7e0, "CompiledExternal",        0x020, 0x1800bb980, 0, 0),
    (0x18000c610, "Stream",                  0x018, 0x1800b9a40, 0, 0),
    (0x180011800, "AnimationEvent",          0x020, 0x1800a2120, 1, 0),
    (0x180015e90, "Geometry",                0x0c0, 0x1800a41c0, 1, 0),
    (0x18001ed20, "ImageBuffer",             0x068, 0x1800f2610, 1, 0),
    (0x180020160, "Particle",                0x048, 0x180103c70, 0, 0),
    (0x180020310, "ParticleInstance",        0x0d8, 0x1801041f0, 0, 0),
    (0x180020ce0, "ParticleSystemInstance",  0x0e0, 0x1800a5520, 0, 0),
    (0x1800244a0, "CompressedMemoryStream",  0x098, 0x1800a9560, 1, 0),
    (0x180031900, "UpdateLoop",              0x028, 0x1800dddb0, 1, 0),
    (0x180035c10, "Element",                 0x0c0, 0x1801161a0, 0, 0),
    (0x18003b800, "DialogController",        0x030, 0x1800cd2f0, 1, 0),
    (0x1800458c0, "DialogInstance",          0x0b8, 0x180114d40, 0, 0),
    (0x18004d4f0, "LoaderLoop",              0x048, 0x1800dc170, 0, 0),
    (0x18004d670, "DialogLoaderLoopT",       0x188, 0x1800db060, 0, 0),
    (0x18004d8b0, "DialogLoopT",             0x168, 0x1800db340, 0, 0),
    (0x180058590, "RenderBufferData",        0x010, 0x1800f8db0, 1, 1),
    (0x180058610, "VideoProgramData",        0x010, 0x1800f8d90, 1, 1),
    (0x180058690, "RenderStateData",         0x010, 0x1800f8d70, 1, 1),
    (0x180058710, "PixelProgramData",        0x010, 0x1800f8dd0, 1, 1),
    (0x180058790, "VertexProgramData",       0x010, 0x1800f8d10, 1, 1),
    (0x180058810, "VertexLayoutData",        0x010, 0x1800f8d30, 1, 1),
    (0x180058890, "VertexBufferData",        0x010, 0x1800f8d50, 1, 1),
    (0x180058910, "IndexBufferData",         0x010, 0x1800f8df0, 1, 1),
    (0x180058990, "ConstantBufferData",      0x010, 0x1800f8e50, 1, 1),
    (0x180058a10, "ImageBufferData",         0x010, 0x1800f8e10, 1, 1),
    (0x180058a90, "ContextData",             0x010, 0x1800f8e30, 1, 1),
    (0x180058b10, "DeviceData",              0x010, 0x1800f8cf0, 1, 1),
]

NS = {
    "Achievement": "Achievement::Achievement",
    "AchievementInstance": "Achievement::AchievementInstance",
    "Goal": "Achievement::Goal",
    "GoalInstance": "Achievement::GoalInstance",
    "MaterialInstance": "Graphics::MaterialInstance",
    "SpriteSheet": "UI::SpriteSheet",
    "Internal": "Object::Internal",
    "Base": "Object::Base",
    "External": "Object::External",
    "CompiledExternal": "Object::CompiledExternal",
    "Stream": "IO::Stream",
    "AnimationEvent": "Graphics::AnimationEvent",
    "Geometry": "Graphics::Geometry",
    "ImageBuffer": "Video::ImageBuffer",
    "Particle": "Graphics::Particle",
    "ParticleInstance": "Graphics::ParticleInstance",
    "ParticleSystemInstance": "Graphics::ParticleSystemInstance",
    "CompressedMemoryStream": "IO::CompressedMemoryStream",
    "UpdateLoop": "Engine::UpdateLoop",
    "Element": "UI::Element",
    "DialogController": "UI::DialogController",
    "DialogInstance": "UI::DialogInstance",
    "LoaderLoop": "Engine::LoaderLoop",
    "DialogLoaderLoopT": "Engine::DialogLoaderLoopT",
    "DialogLoopT": "Engine::DialogLoopT",
}
for _a, _t, _s, _d, _p, _q in ROWS:
    NS.setdefault(_t, "Video::" + _t)


def main():
    print("// ---- the 37 rows -----------------------------------------------------")
    print("static const DelDtorSpec kSpecs[] = {")
    for a, tag, size, d, ported, safe in ROWS:
        print('    RT_DELDTOR(0x%09x, 0x%09x, 0x%03x, "%s"),%s'
              % (a, d, size, NS[tag], "   // probe subject" if safe else ""))
    print("};")
    print("static DtorFn g_dtorCache[kSpecCount];")
    print("static Site   g_sites[kSpecCount];")
    print()
    print("// ---- the 37 entry points ---------------------------------------------")
    for i, (a, tag, size, d, ported, safe) in enumerate(ROWS):
        print("// %-24s ~%s = 0x%09x  (%s)"
              % (NS[tag], tag, d, "PORTED" if ported else "still the DLL's"))
        print("void* RtDelDtor_%s(void* self, unsigned flags) {" % tag)
        print("    return DeletingDtorCore(self, flags, %d);" % i)
        print("}")
    print()
    print("// ---- the 37 registrations --------------------------------------------")
    for a, tag, size, d, ported, safe in ROWS:
        print("    RegisterRuntimeAddr(0x%09x, (void*)&RtDelDtor_%s);" % (a, tag))
    print()
    print("// ---- probe subjects (their ~Class is a pure vtable restamp) -----------")
    print("static const int kProbeSubjects[] = { %s };"
          % ", ".join(str(i) for i, r in enumerate(ROWS) if r[5]))


if __name__ == "__main__":
    main()
