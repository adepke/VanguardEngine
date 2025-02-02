// Copyright (c) 2019-2022 Andrew Depke

#ifndef __CONSTANTS_HLSLI__
#define __CONSTANTS_HLSLI__

static const float pi = 3.1415926535897932384626433832795;
static const float alphaTestThreshold = 0.5;

// Planet
// Technically this is the negative bottom radius, but passing the entire atmosphere data struct around
// just for this value is not very practical.
static const float3 planetCenter = float3(0.f, 0.f, -6360.f);

// Atmosphere
static const float atmosphereRadianceExposure = 10.f;  // 10 is the default exposure used in Bruneton's demo.

// Clouds
static const float cloudLayerBottom = 1400.0 / 1000.0;  // Kilometers.
static const float cloudLayerTop = 5000.0 / 1000.0;  // Kilometers.

#endif  // __CONSTANTS_HLSLI__