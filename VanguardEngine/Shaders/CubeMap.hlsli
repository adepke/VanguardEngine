// Copyright (c) 2019-2021 Andrew Depke

#ifndef __CUBEMAP_HLSLI__
#define __CUBEMAP_HLSLI__

// Utility for generating cubemaps in a computer shader.
float3 ComputeDirection(float2 uv, uint z)
{
    switch (z)
    {
        case 0:
            return float3(1.f, -uv.y, -uv.x);
        case 1:
            return float3(-1.f, uv.y, uv.x);
        case 2:
            return float3(uv.x, 1.f, uv.y);
        case 3:
            return float3(uv.x, -1.f, -uv.y);
        case 4:
            return float3(uv.x, uv.y, 1.f);
        case 5:
            return float3(-uv.x, -uv.y, -1.f);
    }
	
    return 0.f;
}

#endif  // __CUBEMAP_HLSLI__