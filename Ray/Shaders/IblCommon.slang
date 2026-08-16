// IblCommon.slang — shared helpers for the IBL precompute passes (Sky, IrradianceConv,
// Prefilter, BrdfLUT). Fullscreen-triangle VS, cube-face direction reconstruction (GL
// cubemap texel convention), a procedural sky, and GGX importance sampling.
// The renderer sets g_IblGen per face/mip and binds g_EnvCube where needed.

public static const float ECH_IBL_PI = 3.14159265359;

public struct IblGenParams
{
    public float4 MaAxis;   // cube face major axis
    public float4 ScVec;    // cube face s-axis  (horizontal)
    public float4 TcVec;    // cube face t-axis  (vertical)
    public float4 Params;   // x = roughness, y = mip count, zw reserved
    public float4 Sun;      // xyz = direction toward the sun, w = intensity
};
public ConstantBuffer<IblGenParams> g_IblGen;

public struct FsOut
{
    public float4 Pos : SV_Position;
    public float2 UV  : TEXCOORD0;
};

// Fullscreen triangle fed via a real vertex buffer (POSITION attribute), NOT gl_VertexID
// — attributeless draws don't rasterize on some drivers (e.g. NVIDIA GL). UV in [0,1].
public FsOut IblFullscreenVS(float3 position)
{
    FsOut o;
    o.Pos = float4(position.xy, 0.0, 1.0);
    o.UV  = position.xy * 0.5 + 0.5;
    return o;
}

// World direction for a cube-face texel, matching GL's cubemap sampling convention.
public float3 IblFaceDir(float2 uv)
{
    float uc = uv.x * 2.0 - 1.0;
    float vc = uv.y * 2.0 - 1.0;
    return normalize(g_IblGen.MaAxis.xyz + uc * g_IblGen.ScVec.xyz + vc * g_IblGen.TcVec.xyz);
}

// Analytic sky: gradient horizon→zenith, a darker ground below, plus a sun disk.
public float3 IblSkyColor(float3 dir)
{
    float t = saturate(dir.y * 0.5 + 0.5);
    float3 horizon = float3(0.55, 0.68, 0.82);
    float3 zenith  = float3(0.16, 0.34, 0.62);
    float3 ground  = float3(0.10, 0.09, 0.085);
    float3 sky = lerp(horizon, zenith, t);
    float3 col = (dir.y < 0.0) ? lerp(horizon, ground, saturate(-dir.y * 3.0)) : sky;
    float sd = pow(saturate(dot(dir, normalize(g_IblGen.Sun.xyz))), 350.0);
    col += float3(1.0, 0.95, 0.85) * sd * g_IblGen.Sun.w;
    return col;
}

// Hammersley low-discrepancy sequence.
public float IblRadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
public float2 IblHammersley(uint i, uint n)
{
    return float2(float(i) / float(n), IblRadicalInverseVdC(i));
}

// GGX importance sample → a half vector around N in world space.
public float3 IblImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi      = 2.0 * ECH_IBL_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    float3 up      = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitan   = cross(N, tangent);
    return normalize(tangent * H.x + bitan * H.y + N * H.z);
}

public float IblGeometrySchlickGGX(float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2.0;   // IBL k
    return NdotV / (NdotV * (1.0 - k) + k);
}
public float IblGeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return IblGeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * IblGeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
