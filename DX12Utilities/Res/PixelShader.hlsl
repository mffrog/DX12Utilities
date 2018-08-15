struct PSInput {
	float4 position : SV_POSITION;
	float4 pos : POSITION;
	float4 color : COLOR;
};

struct PSOutput {
	float4 color : SV_TARGET0;
	float4 position : SV_TARGET1;
};

PSOutput main(PSInput input)
{
	PSOutput output;
	output.color = input.color;
	output.position = (input.pos + float4(1.0f, 1.0f, 1.0f, 1.0f)) * 0.5f;
	return output;
}