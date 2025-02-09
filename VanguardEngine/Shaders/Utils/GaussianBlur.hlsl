// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

// In the future, this implementation could be significantly optimized to use hardware filtering
// to accelerate the pass.
// See: https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/

struct BindData
{
	uint inputTexture;
	uint outputTexture;
	float2 padding;
	float4 weights[PACKED_WEIGHT_SIZE];
};

ConstantBuffer<BindData> bindData : register(b0);

// 64 is the total thread group size
#define GROUP_SIZE (KERNEL_RADIUS * 2 + 64)
#define LOAD_SIZE ((GROUP_SIZE + 64 - 1) / 64)

// Separated channels to reduce bank conflicts.
groupshared float groupRed[GROUP_SIZE];
groupshared float groupGreen[GROUP_SIZE];
groupshared float groupBlue[GROUP_SIZE];
groupshared float groupAlpha[GROUP_SIZE];

void StoreGroupColor(uint index, float4 color)
{
	groupRed[index] = color.r;
	groupGreen[index] = color.g;
	groupBlue[index] = color.b;
	groupAlpha[index] = color.a;
}

float4 LoadGroupColor(uint index)
{
	return float4(groupRed[index], groupGreen[index], groupBlue[index], groupAlpha[index]);
}

struct Input
{
	uint groupIndex : SV_GroupIndex;
	uint3 dispatchId : SV_DispatchThreadID;
	uint3 groupId : SV_GroupID;
	uint3 groupThreadId : SV_GroupThreadID;
};

[RootSignature(RS)]
[numthreads(64, 1, 1)]
void MainHorizontal(Input input)
{
	Texture2D<float4> inputTexture = ResourceDescriptorHeap[bindData.inputTexture];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];
	
	uint width, height;
	inputTexture.GetDimensions(width, height);

	int origin = input.groupId.x * 64 - KERNEL_RADIUS;
	
	for (int i = 0; i < LOAD_SIZE; ++i)
	{
		int localIndex = input.groupThreadId.x * LOAD_SIZE + i;
		
		if (localIndex < GROUP_SIZE)
		{
			int pc = origin + localIndex;

			if (pc >= 0 && pc < width)
			{
				StoreGroupColor(localIndex, inputTexture[uint2(pc, input.dispatchId.y)]);
			}
		}
	}
	
	// Sync all loads.
	GroupMemoryBarrierWithGroupSync();
	
	if (input.dispatchId.x < width && input.dispatchId.y < height)
	{
		float4 sum = 0.xxxx;

		for (int i = 0; i < KERNEL_RADIUS * 2 + 1; ++i)
		{
			int2 pc = input.dispatchId.xy + int2(i - KERNEL_RADIUS, 0);
			pc.x = max(0, pc.x);
			pc.x = min(width - 1, pc.x);

			int localIndex = pc.x - origin;
			
			// Unpack the weight.
			int index = abs(KERNEL_RADIUS - i);
			float4 weights = bindData.weights[index / 4];
			float weight = weights[index % 4];

			sum += LoadGroupColor(localIndex) * weight;
		}

		outputTexture[input.dispatchId.xy] = sum;
	}
}

[RootSignature(RS)]
[numthreads(1, 64, 1)]
void MainVertical(Input input)
{
	Texture2D<float4> inputTexture = ResourceDescriptorHeap[bindData.inputTexture];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];
	
	uint width, height;
	inputTexture.GetDimensions(width, height);

	int origin = input.groupId.y * 64 - KERNEL_RADIUS;
	
	for (int i = 0; i < LOAD_SIZE; ++i)
	{
		int localIndex = input.groupThreadId.y * LOAD_SIZE + i;
		
		if (localIndex < GROUP_SIZE)
		{
			int pc = origin + localIndex;

			if (pc >= 0 && pc < height)
			{
				StoreGroupColor(localIndex, inputTexture[uint2(input.dispatchId.x, pc)]);
			}
		}
	}
	
	// Sync all loads.
	GroupMemoryBarrierWithGroupSync();
	
	if (input.dispatchId.x < width && input.dispatchId.y < height)
	{
		float4 sum = 0.xxxx;

		for (int i = 0; i < KERNEL_RADIUS * 2 + 1; ++i)
		{
			int2 pc = input.dispatchId.xy + int2(0, i - KERNEL_RADIUS);
			pc.y = max(0, pc.y);
			pc.y = min(height - 1, pc.y);

			int localIndex = pc.y - origin;

			// Unpack the weight.
			int index = abs(KERNEL_RADIUS - i);
			float4 weights = bindData.weights[index / 4];
			float weight = weights[index % 4];
			
			sum += LoadGroupColor(localIndex) * weight;
		}

		outputTexture[input.dispatchId.xy] = sum;
	}
}