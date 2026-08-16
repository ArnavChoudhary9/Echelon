// BrdfLUT.slang — precompute the split-sum BRDF integration LUT (RG). x axis = N·V,
// y axis = roughness. Sampled by PBR.slang for the specular IBL term.
import IblCommon;

[shader("vertex")]
FsOut vertexMain(uint id : SV_VertexID) { return IblFullscreenVS(id); }

[shader("fragment")]
float4 fragmentMain(FsOut fin) : SV_Target
{
    float NdotV     = max(fin.UV.x, 1e-4);
    float roughness = fin.UV.y;

    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    float3 N = float3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;
    const uint SAMPLES = 512u;
    for (uint i = 0u; i < SAMPLES; ++i)
    {
        float2 Xi = IblHammersley(i, SAMPLES);
        float3 H  = IblImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        float ndl = max(L.z, 0.0);
        float ndh = max(H.z, 0.0);
        float vdh = max(dot(V, H), 0.0);
        if (ndl > 0.0)
        {
            float G    = IblGeometrySmith(N, V, L, roughness);
            float gvis = (G * vdh) / max(ndh * NdotV, 1e-4);
            float fc   = pow(1.0 - vdh, 5.0);
            A += (1.0 - fc) * gvis;
            B += fc * gvis;
        }
    }
    return float4(A / float(SAMPLES), B / float(SAMPLES), 0.0, 1.0);
}
