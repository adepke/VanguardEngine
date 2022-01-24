// Copyright (c) 2019-2022 Andrew Depke

#include "PostProcess_RS.hlsli"

struct Input
{
	uint vertexID : SV_VertexID;
};

struct Output
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

[RootSignature(RS)]
Output main(Input input)
{
	Output output;
	output.uv = float2((input.vertexID << 1) & 2, input.vertexID & 2);
	output.positionCS = float4((output.uv.x - 0.5) * 2.0, -(output.uv.y - 0.5) * 2.0, 0, 1);

	return output;
}