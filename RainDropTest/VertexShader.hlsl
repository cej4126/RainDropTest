#include "CommonTypes.hlsli"

struct PSInput
{
    float3 position : POSITION;
};

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);


float4 main(PSInput input) : SV_POSITION
{
    float4 new_pos = mul(GlobalData.ViewProjection, float4(input.position, 1.f));
    
    // test
    //new_pos += 4.f;
    
    return new_pos;
}