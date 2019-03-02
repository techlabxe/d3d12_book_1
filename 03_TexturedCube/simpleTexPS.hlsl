struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
  float2 UV : TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 main(VSOutput In) : SV_TARGET
{
	return In.Color * tex.Sample(samp, In.UV);
}