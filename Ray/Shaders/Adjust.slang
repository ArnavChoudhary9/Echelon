// Adjust.slang — a compute post-process that modifies an HDR color target IN PLACE.
//
// Demonstrates a compute pass in the render pipeline: the target is bound as a
// read/write storage image (RWTexture2D) at binding 0 — matching the pass's
// `inputs: [{ resource: hdr, binding: 0, storage: true }]`. Reflection does not
// describe storage images, so the binding is authored in the .ehpipeline.

RWTexture2D<float4> g_Target;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    uint width, height;
    g_Target.GetDimensions(width, height);
    if (tid.x >= width || tid.y >= height)
        return;

    float4 c = g_Target[tid.xy];
    c.rgb *= float3(1.15, 1.0, 0.85);   // subtle warm tint (visible, reversible signal)
    g_Target[tid.xy] = c;
}
