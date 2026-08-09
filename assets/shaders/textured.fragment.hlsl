// Fragment-stage samplers/textures live in descriptor set 2 under SDL3 GPU's
// SPIR-V convention (see textured.vertex.hlsl for the full explanation);
// D3D11 has no descriptor sets and predates the `space` qualifier.
#ifdef __spirv__
Texture2D tex : register(t0, space2);
SamplerState samp : register(s0, space2);
#else
Texture2D tex : register(t0);
SamplerState samp : register(s0);
#endif

struct Input
{
  float4 position : SV_Position;
  float2 uv : TEXCOORD1;
};

struct Output
{
  float4 color : SV_Target0;
};

Output main(Input input)
{
  Output output;
  output.color = tex.Sample(samp, input.uv);

  return output;
}
