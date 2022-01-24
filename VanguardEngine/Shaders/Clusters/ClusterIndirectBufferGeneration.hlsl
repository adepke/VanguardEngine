// Copyright (c) 2019-2022 Andrew Depke

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"DescriptorTable(" \
		"UAV(u0))," \
	"UAV(u1)"

RWStructuredBuffer<uint> denseClusterList : register(u0);
RWStructuredBuffer<uint3> indirectBuffer : register(u1);

[RootSignature(RS)]
[numthreads(1, 1, 1)]
void BufferGenerationMain()
{
	uint activeClusters = denseClusterList.IncrementCounter();
	indirectBuffer[0] = uint3(activeClusters, 1, 1);  // Dispatch one group for each active cluster.
}