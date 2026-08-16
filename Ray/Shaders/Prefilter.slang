// Prefilter.slang — prefilter the environment cubemap for specular IBL. Rendered once
// per face per mip; g_IblGen.Params.x carries the roughness for the current mip.
import IblCommon;

SamplerCube g_EnvCube;

[shader("vertex")]
FsOut vertexMain(uint id : SV_VertexID) { return IblFullscreenVS(id); }

[shader("fragment")]
float4 fragmentMain(FsOut fin) : SV_Target
{
    float3 N = IblFaceDir(fin.UV);
    float3 R = N;
    float3 V = N;
    float roughness = g_IblGen.Params.x;

    const uint SAMPLES = 128u;
    float3 prefiltered = float3(0.0);
    float  totalWeight = 0.0;
    for (uint i = 0u; i < SAMPLES; ++i)
    {
        float2 Xi = IblHammersley(i, SAMPLES);
        float3 H  = IblImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);
        float  ndl = max(dot(N, L), 0.0);
        if (ndl > 0.0)
        {
            prefiltered += g_EnvCube.SampleLevel(L, 0.0).rgb * ndl;
            totalWeight += ndl;
        }
    }
    return float4(prefiltered / max(totalWeight, 1e-4), 1.0);
}
