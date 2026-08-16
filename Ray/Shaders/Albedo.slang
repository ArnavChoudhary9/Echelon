// Albedo.slang — a built-in RENDERER material shader (ships next to the executable,
// referenced as `shader:Albedo.slang`). Solid albedo color (no texture), lit by the
// scene lights (g_Lights). Widely used, so it lives with the renderer.

import Echelon;   // g_Frame / g_Object / g_Lights + VertexInput/Output

struct MaterialParams
{
    float4 AlbedoColor;
    float  Roughness;
    float  Metallic;
};
ConstantBuffer<MaterialParams> g_Material;

[shader("vertex")]
VertexOutput vertexMain(VertexInput vin)
{
    VertexOutput o;
    float4 world = mul(g_Object.Model, float4(vin.Position, 1.0));
    o.WorldPos   = world.xyz;
    o.ClipPos    = mul(g_Frame.Projection, mul(g_Frame.View, world));
    o.WorldNrm   = mul((float3x3)g_Object.NormalMatrix, vin.Normal);
    o.TexCoord   = vin.TexCoord;
    return o;
}

float3 LightDir(GpuLight L, float3 worldPos)
{
    if (L.Position.w < 0.5)                 // type 0 = directional
        return normalize(-L.Direction.xyz);
    return normalize(L.Position.xyz - worldPos);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput fin) : SV_Target
{
    float3 n      = normalize(fin.WorldNrm);
    float3 albedo = g_Material.AlbedoColor.rgb;

    float3 color = g_Lights.Ambient.rgb * albedo;
    for (int i = 0; i < g_Lights.Count.x; ++i)
    {
        GpuLight L = g_Lights.Lights[i];
        float  ndl = saturate(dot(n, LightDir(L, fin.WorldPos)));
        color += albedo * L.Color.rgb * L.Color.w * ndl;
    }
    return float4(color, g_Material.AlbedoColor.a);
}
