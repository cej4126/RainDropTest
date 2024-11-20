#ifndef COMMONTYPES
#define COMMONTYPES

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
    
    uint NumDirectionalLights;
    float DeltaTime;
};

struct PerObjectData
{
    float4x4 World;
    float4x4 InvWorld;
    float4x4 WorldViewProjection;
    
    float4 BaseColor;
    float3 Emissive;
    float EmissiveIntensity;
    float AmbientOcclusion;
    float Metallic;
    float Roughness;
    uint _pad;
};

struct Plane
{
    float3 Normal;
    float Distance;
};

struct Sphere
{
    float3 Center;
    float Radius;
};

struct Cone
{
    float3 Tip;
    float Height;
    float3 Direction;
    float Radius;
};

// Frustum cone in view space
struct Frustum
{
    float3 ConeDirection;
    float UnitRadius;
};

#ifndef __cplusplus
struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID;
    uint3 GroupThreadID : SV_GroupThreadID;
    uint3 DispatchThreadID : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};
#endif

struct LightCullingDispatchParameters
{
    // Number of groups dispatched. (This parameter is not available as an HLSL system value!)
    uint2 NumThreadGroups;

    // Total number of threads dispatched. (Also not available as an HLSL system value!)
    // NOTE: This value may be less than the actual number of threads executed 
    //       if the screen size is not evenly divisible by the block size.
    uint2 NumThreads;

    // Number of lights for culling (doesn't include directional lights, because those can't be culled).
    uint NumLights;

    // The index of currenct depth buffer in SRV descriptor heap
    uint DepthBufferSrvIndex;
};

// Contains light cullign data that's formatted and ready to be copied
// to a D3D constant/structured buffer as contiguous chunk.
struct LightCullingLightInfo
{
    float3 Position;
    float Range;

    float3 Direction;
    // If this is set to -1 then the light is a point light.
    float   CosPenumbra;
};

// Contains light data that's formatted and ready to be copied
// to a D3D constant/structured buffer as a contiguous chunk.
struct LightParameters
{
    float3 Position;
    float Intensity;

    float3 Direction;
    float Range;

    float3 Color;
    float CosUmbra; // Cosine of the hald angle of umbra

    float3 Attenuation;
    float CosPenumbra; // Cosine of the hald angle of penumbra
};

struct DirectionalLightParameters
{
    float3 Direction;
    float Intensity;
    float3 Color;
    float _pad;
};

#ifdef __cplusplus
static_assert((sizeof(PerObjectData) % 16) == 0,
              "Make sure PerObjectData is formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(LightParameters) % 16) == 0,
              "Make sure LightParameters is formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(LightCullingLightInfo) % 16) == 0,
              "Make sure LightCullingLightInfo is formatted in 16-byte chunks without any implicit padding.");
static_assert((sizeof(DirectionalLightParameters) % 16) == 0,
              "Make sure DirectionalLightParameters is formatted in 16-byte chunks without any implicit padding.");
#endif

#endif // COMMONTYPES
