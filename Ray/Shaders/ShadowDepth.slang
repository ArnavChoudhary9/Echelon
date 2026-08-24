// ShadowDepth.slang — depth-only shadow pass for directional & spot lights.
// Renders scene geometry from the light's viewpoint; only depth is written (the
// framebuffer has no color attachment). The renderer sets g_ShadowPass.LightViewProj
// per shadow view. Sampled later by PBR.slang via PcfShadow2D().

import Ray;   // g_Object + g_ShadowPass + VertexInput

struct VSOut
{
    float4 ClipPos : SV_Position;
};

[shader("vertex")]
VSOut vertexMain(VertexInput vin)
{
    VSOut o;
    float4 world = mul(g_Object.Model, float4(vin.Position, 1.0));
    o.ClipPos    = mul(g_ShadowPass.LightViewProj, world);
    return o;
}

// Depth-only: no color output.
[shader("fragment")]
void fragmentMain(VSOut i)
{
}
