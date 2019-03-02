struct VSOutput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 main(VSOutput In) : SV_TARGET
{
	float4 color = tex.Sample(samp, In.UV);
  if (color.a < 0.5)
  {
    discard;
  }
  return color;
}