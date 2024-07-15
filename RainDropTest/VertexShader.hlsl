
struct PSInput
{
    float3 position : POSITION;
};

float4 main(PSInput input) : SV_POSITION
{
    float4 new_pos = float4(input.position, 1.0f);
    // test
    new_pos += 1.f;
    
    return new_pos;
}