// Tonemap.slang — cinematic post: composite bloom, apply a filmic (ACES) curve, and
// encode linear→sRGB. Fullscreen pass. Input 0 = linear HDR scene, input 1 = bloom.
// Now that the scene renders in LINEAR HDR (PBR), a real filmic curve + gamma is correct
// (this replaces the old near-identity/adjustment passes).

Sampler2D u_Scene;   // input 0 — linear HDR
Sampler2D u_Bloom;   // input 1 — bloom

struct FsOut
{
    float4 Pos : SV_Position;
    float2 UV  : TEXCOORD0;
};

[shader("vertex")]
FsOut vertexMain(uint id : SV_VertexID)
{
    FsOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.UV  = uv;
    o.Pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

// Narkowicz ACES filmic approximation.
float3 ACESFilm(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fragmentMain(FsOut fin) : SV_Target
{
    const float exposure     = 1.0;
    const float bloomStrength = 0.6;

    float3 hdr   = u_Scene.Sample(fin.UV).rgb;
    float3 bloom = u_Bloom.Sample(fin.UV).rgb;

    float3 color = (hdr + bloom * bloomStrength) * exposure;
    color = ACESFilm(color);
    color = pow(color, float3(1.0 / 2.2));   // linear → sRGB (display-referred)
    return float4(color, 1.0);
}
