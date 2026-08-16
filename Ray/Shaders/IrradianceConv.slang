// IrradianceConv.slang — convolve the environment cubemap into a diffuse irradiance
// cubemap (cosine-weighted hemisphere integral). Rendered once per face.
import IblCommon;

SamplerCube g_EnvCube;

[shader("vertex")]
FsOut vertexMain(uint id : SV_VertexID) { return IblFullscreenVS(id); }

[shader("fragment")]
float4 fragmentMain(FsOut fin) : SV_Target
{
    float3 N = IblFaceDir(fin.UV);

    float3 up0   = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(0, 0, 1);
    float3 right = normalize(cross(up0, N));
    float3 up    = normalize(cross(N, right));

    float3 irradiance = float3(0.0);
    float  samples    = 0.0;
    const float dPhi   = 0.05;
    const float dTheta = 0.05;
    for (float phi = 0.0; phi < 2.0 * ECH_IBL_PI; phi += dPhi)
    {
        for (float theta = 0.0; theta < 0.5 * ECH_IBL_PI; theta += dTheta)
        {
            float3 tangent = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sv = tangent.x * right + tangent.y * up + tangent.z * N;
            irradiance += g_EnvCube.SampleLevel(sv, 0.0).rgb * cos(theta) * sin(theta);
            samples += 1.0;
        }
    }
    irradiance = ECH_IBL_PI * irradiance / max(samples, 1.0);
    return float4(irradiance, 1.0);
}
