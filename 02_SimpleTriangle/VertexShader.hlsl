struct VSInput
{
  float4 Position : POSITION;
  float4 Color : COLOR;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
};

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  result.Position = In.Position;
  result.Color = In.Color;
  return result;
}