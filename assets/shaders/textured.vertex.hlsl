// SDL3 GPU's SPIR-V binding convention puts vertex-stage uniform buffers in
// descriptor set 1 (set 0 is reserved for vertex-stage samplers/textures/
// storage buffers); D3D11 has no concept of descriptor sets at all, and its
// shader model (vs_5_0) predates the `space` register qualifier entirely, so
// this only applies on the DXC->SPIR-V path. DXC predefines __spirv__ there.
#ifdef __spirv__
cbuffer Transform : register(b0, space1)
#else
cbuffer Transform : register(b0)
#endif
{
  row_major float4x4 transform;
};

struct Input
{
  float3 position : TEXCOORD0;
  float2 uv : TEXCOORD1;
};

struct Output
{
  float4 position : SV_Position;
  float2 uv : TEXCOORD1;
};

Output main(Input input)
{
  Output output;
  output.position = mul(transform, float4(input.position, 1.0f));
  output.uv = input.uv;

  return output;
}
