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

struct DirectionalLightParameters
{
    float3 Direction;
    float Intensity;
    float3 Color;
    float _pad;
};

#ifdef __cplusplus
static_assert((sizeof(GlobalShaderData) % 16) == 0,
              "Make sure GlobalShaderData is formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(PerObjectData) % 16) == 0,
              "Make sure PerObjectData is formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(DirectionalLightParameters) % 16) == 0,
              "Make sure DirectionalLightParameters is formatted in 16-byte chunks without any implicit padding.");
#endif