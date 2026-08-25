// BloomTint.slang — PROJECT example COMPUTE shader.
//
// This is the DefaultProject's example of a *custom project shader*: the renderer ships
// the standard bloom (Bloom.slang), and this project pass reads that bloom texture and
// writes a graded copy (a warm cinematic tint) that the tonemap pass then composites.
// It runs from the project's Assets dir, so you can edit it and hot-reload without
// rebuilding the engine or renderer.
//
// The render-pass graph requires a single writer per resource, so this reads `bloom`
// (sampled, binding 0) and writes a separate `bloomGraded` (storage, binding 1) — it does
// NOT write `bloom` in place. Bindings match the .ehpipeline `bloomtint` pass. The
// explicit image format is required — a format-Unknown storage image is a silent no-op
// on strict drivers (e.g. NVIDIA).

[[vk::binding(0, 0)]] Sampler2D                                          u_Bloom;
[[vk::binding(1, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> u_Graded;

static const float3 kTint      = float3(1.06, 1.00, 0.90); // warm: lift R, drop B
static const float  kIntensity = 1.15;                     // gentle overall boost

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    uint w, h;
    u_Graded.GetDimensions(w, h);
    if (tid.x >= w || tid.y >= h) return;

    float2 uv = (float2(tid.xy) + 0.5) / float2(w, h);
    float3 c  = u_Bloom.SampleLevel(uv, 0.0).rgb;
    c = c * kTint * kIntensity;
    u_Graded[tid.xy] = float4(c, 1.0);
}
