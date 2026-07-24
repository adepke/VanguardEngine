// Copyright (c) 2019-2022 Andrew Depke

#ifndef __VISIBILITYMOMENTS_HLSLI__
#define __VISIBILITYMOMENTS_HLSLI__

static const float visibilityMarchMax = 50.f;  // Kilometers.

// Integrated shadow smaller than this is considered unshadowed. In kilometers.
static const float visibilityMomentEpsilon = 0.001f;

float2 EncodeVisibilityMoments(float m0, float m1)
{
	return float2(m0, m1 / visibilityMarchMax);
}

// Centroid distance (km) of the shadow distribution along the ray, or 0 if unshadowed.
float VisibilityMomentsCentroid(float2 moments)
{
	return moments.x > visibilityMomentEpsilon ? (moments.y * visibilityMarchMax) / moments.x : 0.f;
}

// Collapse moments to a single contiguous segment (shadowStart, shadowLength).
float2 VisibilityMomentsToSegment(float2 moments)
{
	const float shadowLength = moments.x;
	float shadowStart = 0.f;
	if (shadowLength > visibilityMomentEpsilon)
	{
		const float shadowCentroid = (moments.y * visibilityMarchMax) / shadowLength;
		shadowStart = max(shadowCentroid - shadowLength * 0.5f, 0.f);
	}
	return float2(shadowStart, shadowLength);
}

#endif  // __VISIBILITYMOMENTS_HLSLI__
