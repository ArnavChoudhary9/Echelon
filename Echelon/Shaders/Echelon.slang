// ============================================================================
// Echelon.slang — the shader constant system (fixed ABI shared by all shaders).
//
// What ALWAYS goes in/out and what is ALWAYS available is defined here. Anything
// else (material parameters, extra textures, extra interpolants) is up to the
// individual shader and is discovered by the renderer via reflection.
//
// System constant buffers are resolved by the renderer *by name* (g_Frame,
// g_Object), never by hardcoded binding — Slang assigns bindings, the renderer
// matches the name (see RayRenderer). Material UBOs are any *other* buffer.
// ============================================================================

// ---- Always in: the canonical vertex (locations 0/1/2 = Position/Normal/TexCoord) ----
public struct VertexInput
{
    public float3 Position : POSITION;    // location 0
    public float3 Normal   : NORMAL;      // location 1
    public float2 TexCoord : TEXCOORD0;   // location 2
};

// ---- Always out: SV_Position is required; the rest are conventional interpolants ----
public struct VertexOutput
{
    public float4 ClipPos  : SV_Position; // required
    public float3 WorldNrm : NORMAL;
    public float2 TexCoord : TEXCOORD0;
    public float3 WorldPos : TEXCOORD1;
};

// ---- Always available: per-frame constants (bound once per frame) ----
public struct FrameConstants
{
    public float4x4 View;
    public float4x4 Projection;
    public float4x4 ViewProjection;
    public float4   CameraPosition;   // xyz + pad
    public float4   TimeParams;       // x = time, y = dt
};

// ---- Always available: per-object constants (rewritten per draw) ----
public struct ObjectConstants
{
    public float4x4 Model;
    public float4x4 NormalMatrix;     // inverse-transpose of Model
    public uint4    ObjectId;         // x = per-object id (renderer-assigned, e.g. entity id); yzw reserved
};

public ConstantBuffer<FrameConstants>  g_Frame;
public ConstantBuffer<ObjectConstants> g_Object;

// ---- Always available: scene lights (bound once per scene) ----
// Scaffold for lighting/PBR: the renderer gathers LightComponents into this block
// and resolves it by name (like g_Frame/g_Object). Shaders that don't reference
// g_Lights are unaffected (Slang strips the unused block). A future PBR shader
// simply reads g_Lights.Lights[0..Count.x].
public static const int ECH_MAX_LIGHTS = 16;

public struct GpuLight
{
    public float4 Position;    // xyz = world position (point/spot); w = type (0=dir,1=point,2=spot)
    public float4 Direction;   // xyz = direction (dir/spot);        w = range (point/spot)
    public float4 Color;       // rgb = color;                       w = intensity
    public float4 SpotParams;  // x = cos(inner), y = cos(outer);    zw reserved
};

public struct LightConstants
{
    public float4   Ambient;                     // rgb = ambient color; w reserved
    public int4     Count;                       // x = active light count; yzw reserved (int4 for std140)
    public GpuLight Lights[ECH_MAX_LIGHTS];
};

public ConstantBuffer<LightConstants> g_Lights;

// ---- Always available: shadow mapping (one caster per light type) ----
// The renderer renders scene depth from up to one directional, one spot, and one
// point light, and resolves this block + the shadow samplers by name. A shader that
// samples them gets soft shadows; shaders that don't are unaffected (Slang strips them).
public struct ShadowConstants
{
    public float4x4 DirViewProj;    // world → directional-light clip space
    public float4x4 SpotViewProj;   // world → spot-light clip space
    public float4   DirParams;      // x = caster light index (-1 = none), y = depth bias, z = normal bias, w = strength
    public float4   SpotParams;     // x = caster index, y = depth bias, z = normal bias, w = strength
    public float4   PointPosIndex;  // xyz = point light world pos, w = caster index (-1 = none)
    public float4   PointParams;    // x = far plane, y = depth bias, z = strength, w = reserved
    public float4   MapParams;      // x = dir texel (1/res), y = spot texel, z = point texel, w = reserved
};
public ConstantBuffer<ShadowConstants> g_Shadows;

public Sampler2D   g_ShadowDir;     // directional light depth map (NDC depth)
public Sampler2D   g_ShadowSpot;    // spot light depth map (NDC depth)
public SamplerCube g_ShadowPoint;   // point light: world distance / far, in the R channel

// Per-view constants used ONLY by the shadow depth pass (rendering FROM a light).
// The renderer rewrites this for each shadow view; ordinary scene shaders never touch it.
public struct ShadowPassConstants
{
    public float4x4 LightViewProj;  // world → the current shadow view's clip space
    public float4   LightPosFar;    // xyz = point light world pos, w = far plane (point cube pass)
};
public ConstantBuffer<ShadowPassConstants> g_ShadowPass;

// ---- Always available: image-based lighting (procedural-sky environment) ----
public struct IblConstants
{
    public float4 Params;   // x = prefilter max mip, y = ambient intensity, z = IBL enabled (0/1), w = reserved
};
public ConstantBuffer<IblConstants> g_Ibl;

public SamplerCube g_IrradianceMap;   // diffuse irradiance
public SamplerCube g_PrefilterMap;    // prefiltered specular (mip level ≈ roughness)
public Sampler2D   g_BrdfLUT;         // split-sum BRDF integration LUT (RG)

// ---- Shared helpers ----

/** @brief Transform an object-space position to clip space through the system CBs. */
public float4 ObjectToClip(float3 objectPos)
{
    float4 world = mul(g_Object.Model, float4(objectPos, 1.0));
    return mul(g_Frame.Projection, mul(g_Frame.View, world));
}

/**
 * @brief 2D percentage-closer shadow test. Returns visibility in [0,1] (1 = fully lit).
 * Uses the same light view-proj the depth map was rendered with; a normal offset plus
 * slope-scaled depth bias suppress acne. Fragments outside the map are treated as lit.
 */
public float PcfShadow2D(Sampler2D map, float4x4 viewProj, float3 worldPos, float3 N, float3 L,
                         float depthBias, float normalBias, float texel)
{
    float3 p  = worldPos + N * normalBias;
    float4 lp = mul(viewProj, float4(p, 1.0));
    if (lp.w <= 0.0) return 1.0;

    float3 proj = lp.xyz / lp.w;
    proj = proj * 0.5 + 0.5;                       // GL NDC [-1,1] → [0,1]
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;

    float bias = max(depthBias * (1.0 - dot(N, L)), depthBias * 0.15);
    float vis  = 0.0;
    [unroll] for (int x = -1; x <= 1; ++x)
        [unroll] for (int y = -1; y <= 1; ++y)
        {
            float d = map.Sample(proj.xy + float2(x, y) * texel).r;
            vis += (proj.z - bias <= d) ? 1.0 : 0.0;
        }
    return vis / 9.0;
}

/**
 * @brief Cube distance shadow test for a point light. The cube stores world distance
 * from the light normalized by the far plane; we compare against the fragment's distance.
 */
public float PcfShadowCube(SamplerCube map, float3 fromLight, float currentDist, float farPlane, float bias)
{
    const float3 offsets[8] = {
        float3( 1,  1,  1), float3( 1, -1,  1), float3(-1, -1,  1), float3(-1,  1,  1),
        float3( 1,  1, -1), float3( 1, -1, -1), float3(-1, -1, -1), float3(-1,  1, -1)
    };
    float3 dir = normalize(fromLight);
    float diskRadius = 0.015;
    float vis = 0.0;
    [unroll] for (int i = 0; i < 8; ++i)
    {
        float sampled = map.Sample(dir + offsets[i] * diskRadius).r * farPlane;
        vis += (currentDist - bias <= sampled) ? 1.0 : 0.0;
    }
    return vis / 8.0;
}
