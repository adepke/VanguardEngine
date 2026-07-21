// Copyright (c) 2019-2022 Andrew Depke

#ifndef __CONSTANTS_HLSLI__
#define __CONSTANTS_HLSLI__

static const float pi = 3.1415926535897932384626433832795;
static const float alphaTestThreshold = 0.5;

// Sun
static const float sunAngularRadius = 0.004675f;

// Planet
// Technically this is the negative bottom radius, but passing the entire atmosphere data struct around
// just for this value is not very practical.
static const float3 planetCenter = float3(0.f, 0.f, -6360.f);

// Clouds
static const float cloudLayerBottom = 1400.0 / 1000.0;  // Kilometers.
static const float cloudLayerTop = 5000.0 / 1000.0;  // Kilometers.

#endif  // __CONSTANTS_HLSLI__
