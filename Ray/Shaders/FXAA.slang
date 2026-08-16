// FXAA.slang — fast approximate anti-aliasing as a fullscreen post pass.
//
// Classic FXAA (Timothy Lottes): edge-detect on luma, then blend along the edge
// direction. Runs on the display-referred LDR image (after the color pass), sampling
// its neighbours — a natural fit for the pass graph and no MSAA framebuffer needed.
// Requires a linear + clamp sampler (Ray binds m_LinearSampler).

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

static float Luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }

[shader("fragment")]
float4 fragmentMain(VSOut fin) : SV_Target
{
    const float EDGE_MIN  = 1.0 / 128.0;   // absolute luma threshold
    const float EDGE_MUL  = 1.0 / 8.0;     // relative luma threshold
    const float SPAN_MAX  = 8.0;

    uint w, h;
    g_Source.GetDimensions(w, h);
    float2 texel = 1.0 / float2(w, h);
    float2 uv    = fin.UV;

    float3 rgbM = g_Source.Sample(uv).rgb;
    float lM  = Luma(rgbM);
    float lNW = Luma(g_Source.Sample(uv + float2(-1.0, -1.0) * texel).rgb);
    float lNE = Luma(g_Source.Sample(uv + float2( 1.0, -1.0) * texel).rgb);
    float lSW = Luma(g_Source.Sample(uv + float2(-1.0,  1.0) * texel).rgb);
    float lSE = Luma(g_Source.Sample(uv + float2( 1.0,  1.0) * texel).rgb);

    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));

    // No perceptible edge → pass the center texel through unchanged.
    if (lMax - lMin < max(EDGE_MIN, lMax * EDGE_MUL))
        return float4(rgbM, 1.0);

    float2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));

    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.25 * EDGE_MUL, 1e-6);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -SPAN_MAX, SPAN_MAX) * texel;

    float3 rgbA = 0.5 * (g_Source.Sample(uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                         g_Source.Sample(uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    float3 rgbB = rgbA * 0.5 + 0.25 * (g_Source.Sample(uv + dir * -0.5).rgb +
                                       g_Source.Sample(uv + dir *  0.5).rgb);

    float lB = Luma(rgbB);
    return float4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}
