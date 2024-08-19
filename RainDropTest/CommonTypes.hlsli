struct GlobalShaderData
{
    float4x4 View;
    float4x4 InverseView;
    float4x4 Projection;
    float4x4 InverseProjection;
    float4x4 ViewProjection;
    float4x4 InverseViewProjection;
    
    float3 CameraPosition;
    float ViewWidth;
    float3 CameraDirection;
    float ViewHeight;
};

struct PerObjectData
{
    float4x4 World;
    float4x4 InvWorld;
    float4x4 WorldViewPorjection;
};