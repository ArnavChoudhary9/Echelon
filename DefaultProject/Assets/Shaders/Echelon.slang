// ============================================================================
// Echelon.slang — the shader constant system (fixed ABI shared by all shaders).
//
// What ALWAYS goes in/out and what is ALWAYS available is defined here. Anything
// else (material parameters, extra textures, extra interpolants) is up to the
// individual shader and is discovered by the renderer via reflection.
//
// System constant buffers are resolved by the renderer *by name* (g_Frame,
// g_Object), never by hardcoded binding — Slang assigns bindings, the renderer
// matches the name (see RayRenderer). Material UBOs are any *other* buffer.
// ============================================================================

// ---- Always in: the canonical vertex (locations 0/1/2 = Position/Normal/TexCoord) ----
public struct VertexInput
{
    public float3 Position : POSITION;    // location 0
    public float3 Normal   : NORMAL;      // location 1
    public float2 TexCoord : TEXCOORD0;   // location 2
};

// ---- Always out: SV_Position is required; the rest are conventional interpolants ----
public struct VertexOutput
{
    public float4 ClipPos  : SV_Position; // required
    public float3 WorldNrm : NORMAL;
    public float2 TexCoord : TEXCOORD0;
    public float3 WorldPos : TEXCOORD1;
};

// ---- Always available: per-frame constants (bound once per frame) ----
public struct FrameConstants
{
    public float4x4 View;
    public float4x4 Projection;
    public float4x4 ViewProjection;
    public float4   CameraPosition;   // xyz + pad
    public float4   TimeParams;       // x = time, y = dt
};

// ---- Always available: per-object constants (rewritten per draw) ----
public struct ObjectConstants
{
    public float4x4 Model;
    public float4x4 NormalMatrix;     // inverse-transpose of Model
};

public ConstantBuffer<FrameConstants>  g_Frame;
public ConstantBuffer<ObjectConstants> g_Object;

// ---- Shared helpers ----

/** @brief Transform an object-space position to clip space through the system CBs. */
public float4 ObjectToClip(float3 objectPos)
{
    float4 world = mul(g_Object.Model, float4(objectPos, 1.0));
    return mul(g_Frame.Projection, mul(g_Frame.View, world));
}
