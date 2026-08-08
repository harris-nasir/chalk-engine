struct Input
{
  float x : TEXTCOORD0;
  float y : TEXTCOORD1;
  float z : TEXTCOORD2;
};

struct Output
{
  float4 position : SV_Position;
};


Output main(Input input)
{
  Output output;
  output.position = float4(input.x, input.y, input.z, 1.0f);

  return output;
}
