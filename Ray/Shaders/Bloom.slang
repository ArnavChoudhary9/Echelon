// Bloom.slang — PROJECT compute pass. Bright-pass + Gaussian blur of the HDR scene
// into a (fixed-size) bloom texture, later composited by the tonemap pass. This is the
// project's example COMPUTE shader (the render-pass example is PostColor/Tonemap/FXAA).
//
// Bindings match the .ehpipeline pass inputs: hdr = sampled (binding 0), bloom = storage
// image (binding 1). ExecuteComputePass binds each at its authored YAML binding.

[[vk::binding(0, 0)]] Sampler2D                                u_Hdr;
// Explicit image format — NVIDIA's imageStore requires a format-qualified image
// (a format-Unknown storage image is a silent no-op on strict drivers).
[[vk::binding(1, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> u_Bloom;

static const float kThreshold = 1.05;   // bright-pass knee (HDR luminance)
static const float kIntensity = 1.0;

// Keep only the energy above the threshold, preserving hue.
float3 Prefilter(float3 c)
{
    float bright = max(c.r, max(c.g, c.b));
    float contrib = max(0.0, bright - kThreshold) / max(bright, 1e-4);
    return c * contrib;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    uint w, h;
    u_Bloom.GetDimensions(w, h);
    if (tid.x >= w || tid.y >= h) return;

    float2 texel = 1.0 / float2(w, h);
    float2 uv    = (float2(tid.xy) + 0.5) * texel;

    // Wide-ish Gaussian (bloom is low-res, so a small kernel spreads far in screen space).
    const int R = 2;
    float3 sum  = float3(0.0);
    float  wsum = 0.0;
    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            float2 off = float2(x, y) * texel * 2.0;
            float  g   = exp(-float(x * x + y * y) * 0.35);
            sum  += Prefilter(u_Hdr.SampleLevel(uv + off, 0.0).rgb) * g;
            wsum += g;
        }
    }

    float3 bloom = (sum / max(wsum, 1e-4)) * kIntensity;
    u_Bloom[tid.xy] = float4(bloom, 1.0);
}
