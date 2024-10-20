#include "Common.hlsli"

struct ShaderConstants
{
    uint GPassMainBufferIndex;
};

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<ShaderConstants> ShaderParams : register(b1, space0);

float4 PostProcessPS(in noperspective float4 Position : SV_Position,
                     in noperspective float2 UV : TEXCOORD) : SV_Target0
{
    Texture2D gpassMain = ResourceDescriptorHeap[ShaderParams.GPassMainBufferIndex];
    return float4(gpassMain[Position.xy].xyz, 1.f);
}
