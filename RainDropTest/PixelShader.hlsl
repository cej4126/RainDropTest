struct PixelShaderInput
{
    float4 pos : SV_Position;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	return float4(1.0f, 0.0f, 0.0f, 1.0f);
}