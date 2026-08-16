// PostColor.slang — a CUSTOM PROJECT shader (lives in the project's Assets, not
// next to the engine executable). Demonstrates that projects can ship their own
// shaders: the renderer resolves `.ehpipeline` pass shaders against the project's
// Assets dir, and the importer still puts the engine shader dir on Slang's search
// path so a project shader can `import Echelon` if it needs the shared ABI.
//
// This is the color pass: gentle exposure (content is display-referred LDR) + a
// subtle vignette, then clamp. Use a combined Sampler2D (GL binds combined samplers).

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
    float3 c = g_Source.Sample(fin.UV).rgb;

    // Subtle vignette so the custom project shader is visibly doing something.
    float2 d = fin.UV - 0.5;
    float  vig = lerp(0.78, 1.0, smoothstep(0.85, 0.30, length(d)));
    c *= vig;

    return float4(saturate(c), 1.0);
}
