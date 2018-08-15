struct PSInput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

PSInput main( float4 pos : POSITION, float2 texcoord : TEXCOORD)
{
	PSInput input;
	input.position = pos;
	input.texcoord = texcoord;
	return input;
}