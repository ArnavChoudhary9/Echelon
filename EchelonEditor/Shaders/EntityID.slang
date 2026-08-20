// EntityID.slang — EDITOR-OWNED object-id pass shader (GPU picking).
//
// Nothing renderer-specific: this is an ordinary Graphics-pass override shader that
// the editor injects via RendererAPI::SetAuxiliaryPipeline. It draws every scene mesh
// and writes its per-object id (the renderer packs the entt id into g_Object.ObjectId)
// into an R32_UINT target. The editor clears that target to 0, so this outputs
// (ObjectId.x + 1) and treats a read-back 0 as "nothing here". Depth-tested by the
// pass so the front-most surface wins; sampled back only via glReadPixels.

import Echelon;   // g_Frame + g_Object + VertexInput + ObjectToClip

struct VSOut
{
    float4 ClipPos : SV_Position;
};

[shader("vertex")]
VSOut vertexMain(VertexInput vin)
{
    VSOut o;
    o.ClipPos = ObjectToClip(vin.Position);   // uses g_Frame + g_Object.Model
    return o;
}

[shader("fragment")]
uint fragmentMain(VSOut i) : SV_Target
{
    return g_Object.ObjectId.x + 1u;   // +1 so the cleared 0 means "no entity"
}
