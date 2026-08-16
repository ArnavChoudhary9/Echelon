// Sky.slang — renders the procedural sky into one face of the environment cubemap.
import IblCommon;

[shader("vertex")]
FsOut vertexMain(uint id : SV_VertexID) { return IblFullscreenVS(id); }

[shader("fragment")]
float4 fragmentMain(FsOut fin) : SV_Target
{
    return float4(IblSkyColor(IblFaceDir(fin.UV)), 1.0);
}
