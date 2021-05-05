// Copyright (c) 2019-2021 Andrew Depke

#ifndef __BASE_HLSLI__
#define __BASE_HLSLI__

Texture2D<float4> textures[] : register(t0, space0);
Texture3D<float4> textures3D[] : register(t0, space1);
RWTexture2D<float4> texturesRW[] : register(u0, space0);
RWTexture3D<float4> textures3DRW[] : register(u0, space1);

#endif  // __BASE_HLSLI__