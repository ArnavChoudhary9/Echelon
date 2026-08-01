// Basic.slang — a lit shader with reflection-driven MATERIAL parameters.
//
// g_Material and u_Albedo* are NOT part of the fixed constant system: the
// renderer/material discovers them via reflection (any UBO that is not g_Frame/
// g_Object is a material block; reflected textures are material textures) and
// packs/binds values generically. Edit these freely — no C++ change needed.

import Echelon;

struct MaterialParams
{
    float4 AlbedoColor;   // offset 0
    float  Roughness;     // offset 16
    float  Metallic;      // offset 20
};
ConstantBuffer<MaterialParams> g_Material;

Texture2D    u_AlbedoTex;
SamplerState u_AlbedoSampler;

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

[shader("fragment")]
float4 fragmentMain(VertexOutput fin) : SV_Target
{
    float3 n     = normalize(fin.WorldNrm);
    float3 L     = normalize(float3(0.4, 0.8, 0.6));
    float  ndl   = saturate(dot(n, L));

    float4 tex   = u_AlbedoTex.Sample(u_AlbedoSampler, fin.TexCoord);
    float3 albedo = g_Material.AlbedoColor.rgb * tex.rgb;
    float3 color  = albedo * (0.2 + 0.8 * ndl);
    return float4(color, g_Material.AlbedoColor.a);
}
