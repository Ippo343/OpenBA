// NOT RETAIL. ext.ambient_occlusion (docs/extensions/AO.md §3, §6.1-6.2; handins/AOCORE.md).
//
// The main depth (D24S8, read through its R24_UNORM_X8_TYPELESS view) -> a 1x R32_FLOAT copy of the
// RAW hardware depth (0 at near, 1 at far; the sky keeps the clear, exactly 1.0). One fullscreen
// triangle, no vertex buffer (SV_VertexID). Compiled offline by tools/ao_shaders.py.
//
// The linearisation itself (AO.md §6.2: viewDepth = (n f/(f-n)) / (f/(f-n) - z)) is NOT done here:
// ASSAO's PSPrepareDepths* apply exactly that formula from the projection it is handed
// (ScreenSpaceToViewSpaceDepth, DepthUnpackConsts = (-P[3][2], P[2][2]) of the row-swapped matrix),
// so a linear target here would be linearised twice. The resolve's job is MSAA -> 1x.

Texture2DMS<float> g_DepthMS : register(t0);
Texture2D<float>   g_Depth1x : register(t0);

struct VSOut { float4 pos : SV_Position; };

VSOut VSFullscreen(uint id : SV_VertexID)
{
    VSOut o;
    const float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

// MSAA: sample 0 (the spike's S4; a min over samples would need the count as a define and darkens
// silhouettes no better: ASSAO's own edge-aware blur handles the per-pixel edges).
float PSResolveDepthMS(VSOut i) : SV_Target
{
    return g_DepthMS.Load(int2(i.pos.xy), 0);
}

// 1x: a plain copy (ASSAO cannot read the main depth while it is the bound DSV, and the copy keeps
// one code path for every sample count).
float PSResolveDepth1x(VSOut i) : SV_Target
{
    return g_Depth1x.Load(int3(i.pos.xy, 0));
}
