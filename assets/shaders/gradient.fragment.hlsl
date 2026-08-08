struct Input
{
	float3 color : TEXCOORD1;
};

struct Output
{
	float4 color : SV_Target0;
};

Output main(Input input)
{
	Output output;
	output.color = float4(input.color, 1.0f);

	return output;
}
