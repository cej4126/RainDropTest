 //*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "Common.hlsli"

struct VSParticleIn
{
    float4 color : COLOR;
    uint id : SV_VERTEXID;
};

struct VSParticleDrawOut
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct GSParticleDrawOut
{
    float2 tex : TEXCOORD0;
    float4 color : COLOR;
    float4 pos : SV_POSITION;
};

struct PSParticleDrawIn
{
    float2 tex : TEXCOORD0;
    float4 color : COLOR;
};

struct PosVelo
{
    float4 pos;
    float4 velo;
};

StructuredBuffer<PosVelo> g_bufPosVelo;
ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);

cbuffer cb1
{
    static float g_fParticleRad = 0.1f;
};

cbuffer cbImmutable
{
    static float3 g_positions[4] =
    {
        float3(-1, 1, 0),
		float3(1, 1, 0),
		float3(-1, -1, 0),
		float3(1, -1, 0),
    };

    static float2 g_texcoords[4] =
    {
        float2(0, 0),
		float2(1, 0),
		float2(0, 1),
		float2(1, 1),
    };
};

//
// Vertex shader for drawing the point-sprite particles.
//
VSParticleDrawOut VSParticleDraw(VSParticleIn input)
{
    VSParticleDrawOut output;

    output.pos = g_bufPosVelo[input.id].pos.xyz;

    output.color = input.color;

    return output;
}

//
// GS for rendering point sprite particles.  Takes a point and turns 
// it into 2 triangles.
//
[maxvertexcount(4)]
void GSParticleDraw(point VSParticleDrawOut input[1], inout TriangleStream<GSParticleDrawOut> SpriteStream)
{
    GSParticleDrawOut output;

	// Emit two new triangles.
    for (int i = 0; i < 4; i++)
    {
        float4 position = float4(g_positions[i] * g_fParticleRad, 1.f);
        //position = mul(GlobalData.InverseView, position) + float4(input[0].pos, 1.f);
        output.pos = mul(GlobalData.ViewProjection, position);

        output.color = input[0].color;
        output.tex = g_texcoords[i];
        SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}

//
// PS for drawing particles. Use the texture coordinates to generate a 
// radial gradient representing the particle.
//
float4 PSParticleDraw(PSParticleDrawIn input) : SV_Target
{
    float intensity = (0.5f - length(float2(0.5f, 0.5f) - input.tex)) * 2.0f;
	
    clip(intensity - 0.5f);
    return float4(input.color.xyz * intensity, 1.0f);
}

