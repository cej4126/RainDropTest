
struct PSInput
{
    float3 position : POSITION;
};


struct GlobalShaderData
{
    float4x4 g_mWorldViewProj;
    float4x4 g_mInvView;
};
ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);


float4 main(PSInput input) : SV_POSITION
{
    float4 new_pos = mul(GlobalData.g_mWorldViewProj, float4(input.position, 1.f));
    
    // test
    //new_pos += 4.f;
    
    return new_pos;
}