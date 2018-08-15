struct PSInput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
SamplerState sampler0 : register(s0);

float4 main(PSInput input) : SV_TARGET
{
	return tex1.Sample(sampler0, input.texcoord);
}