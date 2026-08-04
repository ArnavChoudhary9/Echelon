// Error.slang — the "something is wrong" shader. Renders solid magenta so a
// missing/failed material or shader is immediately obvious in the viewport.
// Engine-owned; a renderer builds its own fallback pipeline from it.

import Echelon;

[shader("vertex")]
VertexOutput vertexMain(VertexInput vin)
{
    VertexOutput o;
    float4 world = mul(g_Object.Model, float4(vin.Position, 1.0));
    o.WorldPos   = world.xyz;
    o.ClipPos    = mul(g_Frame.Projection, mul(g_Frame.View, world));
    o.WorldNrm   = vin.Normal;
    o.TexCoord   = vin.TexCoord;
    return o;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput fin) : SV_Target
{
    return float4(1.0, 0.0, 1.0, 1.0); // magenta — missing/failed material
}
