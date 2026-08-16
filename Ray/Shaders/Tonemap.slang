// Tonemap.slang — color / exposure pass.
//
// The scene here is authored display-referred (LDR) — the pre-passgraph renderer wrote
// these colors straight to the screen. So this is a GENTLE, near-identity pass: apply
// exposure, then clamp. It deliberately does NOT apply an aggressive Reinhard curve or
// an extra gamma(1/2.2) encode — doing that to display-referred colors double-encodes
// them (over-bright background, hue-shifted oranges). Swap in a real filmic curve +
// gamma once the scene is rendered in linear HDR with lighting.
//
// Use a combined Sampler2D (see Basic.slang / OpenGLDescriptorSet — GL binds combined
// image samplers only).

Sampler2D g_Source;

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

[shader("vertex")]
VSOut vertexMain(uint vid : SV_VertexID)
{
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.UV       = uv;
    o.Position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

[shader("fragment")]
float4 fragmentMain(VSOut fin) : SV_Target
{
    const float exposure = 1.0;   // dial the overall brightness here
    float3 c = g_Source.Sample(fin.UV).rgb * exposure;
    return float4(saturate(c), 1.0);
}
