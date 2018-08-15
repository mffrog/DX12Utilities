struct PSInput {
	float4 position : SV_POSITION;
	float4 pos : POSITION;
	float4 color : COLOR;
};

cbuffer World : register(b0) {
	float4x4 world;
};

PSInput main( float3 pos : POSITION, float4 color : COLOR)
{
	PSInput input;
	input.position = mul(float4(pos, 1.0f),world);
	input.pos = input.position;
	input.color = color;
	return input;
}