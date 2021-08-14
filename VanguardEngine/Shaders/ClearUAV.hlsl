// Copyright (c) 2019-2021 Andrew Depke

#define RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(b0, num32BitConstants = 4)," \
    "UAV(u0)"

struct ClearUAVInfo
{
    uint bufferSize;
    float3 padding;
};

ConstantBuffer<ClearUAVInfo> clearUAVInfo : register(b0);
RWStructuredBuffer<uint> bufferUint : register(u0);

[RootSignature(RS)]
[numthreads(64, 1, 1)]
void ClearUAVMain(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x < clearUAVInfo.bufferSize)
    {
        bufferUint[dispatchId.x] = 0;
    }
}