// Flat.slang — the default shader. Uses only the system constant buffers
// (g_Frame / g_Object); no material parameters. Shades from the world normal.

import Echelon;

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
    float3 n    = normalize(fin.WorldNrm);
    float3 base = n * 0.5 + 0.5;   // remap normal to a visible color
    return float4(base, 1.0);
}
