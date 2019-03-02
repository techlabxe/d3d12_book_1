struct VSInput
{
  float4 Position : POSITION;
  float4 Color : COLOR;
  float2 UV : TEXCOORD0;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
  float2 UV : TEXCOORD0;
};

cbuffer ShaderParameter : register(b0)
{
  float4x4 world;
  float4x4 view;
  float4x4 proj;
}

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  float4x4 mtxWVP = mul(world, mul(view, proj));
  result.Position = mul(In.Position, mtxWVP);
  result.Color = In.Color;
  result.UV = In.UV;
  return result;
}