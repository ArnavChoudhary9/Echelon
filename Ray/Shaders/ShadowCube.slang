// ShadowCube.slang — point-light shadow pass. Renders scene geometry into one face
// of a distance cubemap (R32F) per invocation; the renderer sets g_ShadowPass to the
// face's view-proj + the light position/far plane, and renders all 6 faces. The stored
// value is world distance from the light normalized by the far plane. Sampled later by
// PBR.slang via PcfShadowCube().

import Echelon;   // g_Object + g_ShadowPass + VertexInput

struct VSOut
{
    float4 ClipPos  : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

[shader("vertex")]
VSOut vertexMain(VertexInput vin)
{
    VSOut o;
    float4 world = mul(g_Object.Model, float4(vin.Position, 1.0));
    o.WorldPos   = world.xyz;
    o.ClipPos    = mul(g_ShadowPass.LightViewProj, world);
    return o;
}

[shader("fragment")]
float4 fragmentMain(VSOut fin) : SV_Target
{
    float dist = length(fin.WorldPos - g_ShadowPass.LightPosFar.xyz);
    float norm = dist / max(g_ShadowPass.LightPosFar.w, 1e-4);
    return float4(norm, 0.0, 0.0, 1.0);   // R = normalized distance from light
}
