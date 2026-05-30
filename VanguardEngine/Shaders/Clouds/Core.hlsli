// Copyright (c) 2019-2022 Andrew Depke

#ifndef __CLOUDS_CORE_HLSLI__
#define __CLOUDS_CORE_HLSLI__

#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Math.hlsli"
#include "Constants.hlsli"
#include "Volumetrics/LightIntegration.hlsli"
#include "Volumetrics/PhaseFunctions.hlsli"
#include "Atmosphere/Atmosphere.hlsli"

float3 SampleWeather(Texture2D<float3> weatherTexture, float3 position)
{
	const float frequency = 0.015;
	return weatherTexture.Sample(bilinearWrap, position.xy * frequency + (0.5.xx));
}

float SampleBaseShape(Texture3D<float> noiseTexture, float3 position, uint mip)
{
	const float frequency = 0.18;
	return noiseTexture.SampleLevel(bilinearWrap, position * frequency, mip);
}

float SampleDetailShape(Texture3D<float> noiseTexture, float3 position, float3 curlOffset)
{
	const float frequency = 5.5;
	return noiseTexture.Sample(bilinearWrap, position * frequency + curlOffset);
}

float3 SampleCurlNoise(Texture3D<float4> noiseTexture, float3 position)
{
	const float frequency = 3.0;  // Should be lower than detail noise frequency.
	return noiseTexture.SampleLevel(bilinearWrap, position * frequency, 0).xyz;
}

float GetHeightFractionForPoint(float3 position, float2 cloudMinMax)
{
	// #TODO: Refactor.
	const float planetRadius = 6360.0;  // #TODO: Get from atmosphere data.

	float3 planetVector = position - planetCenter;

	float heightFraction = (length(planetVector) - planetRadius - cloudMinMax.x) / (cloudMinMax.y - cloudMinMax.x);
	return saturate(heightFraction);
}

float GetDensityHeightGradientForPoint(float3 position, float cloudType)
{
	const float fraction = GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop));

	// Cloud type: 0.0=stratocumulus, 0.5=cumulus, 1.0=cumulonimbus
	float a, b, c;

	// Stratocumulus
	a = 0.2;
	b = 0.28;
	c = 0.39;
	float stratocumulus = saturate(RemapRange(fraction, 0.1, a, 0, 1)) * saturate(RemapRange(fraction, b, c, 1, 0));

	// Cumulus
	a = 0.19;
	b = 0.38;
	c = 0.78;
	float cumulus = saturate(RemapRange(fraction, 0.08, a, 0, 1)) * saturate(RemapRange(fraction, b, c, 1, 0));

	// Cumulonimbus
	a = 0.12;
	b = 0.8;
	c = 0.95;
	float cumulonimbus = saturate(RemapRange(fraction, 0, a, 0, 1)) * saturate(RemapRange(fraction, b, c, 1, 0));

	float gradient = lerp(stratocumulus, cumulus, saturate(cloudType * 2.0));
	gradient = lerp(gradient, cumulonimbus, saturate(cloudType * 2.0 - 1.0));

	return gradient;
}

float SampleCloudDensity(Texture2D<float3> weatherTexture, Texture3D<float> baseNoise, Texture3D<float> detailNoise,
	Texture3D<float4> curlNoise, float3 position, float2 wind, float time, bool detailSample, uint mip)
{
#ifdef CLOUDS_LOW_DETAIL
	detailSample = false;
#endif

	float3 weather = SampleWeather(weatherTexture, position);
	float coverage = weather.x;
	const float type = weather.y;

	const float heightFraction = GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop));
	// Shorter clouds taper off towards the top, while staying more flat on the bottom.
	const float shortCoverage = pow(coverage, (heightFraction * 3.8 + 0.1));
	// Taller clouds form an anvil-like shape.
	const float tallCoverage = pow(coverage, 1.0 - 0.8 * abs(heightFraction - 0.6));
	coverage = lerp(shortCoverage, tallCoverage, type + 0.1);

	const float heightGradient = GetDensityHeightGradientForPoint(position, type);

	// Apply wind distortion for sampling density noise.
	const float timeDilation = 0.3;
	position.xy += wind * time * timeDilation;
	position.xy += heightFraction * wind * 9.0 * timeDilation;

	float baseShape = SampleBaseShape(baseNoise, position, mip);
	float finalShape = baseShape * heightGradient;  // Apply the gradient early to potentially early-out of the detail sample.
	finalShape = RemapRange(finalShape, 1.0 - coverage, 1.0, 0.0, 1.0);
	finalShape = finalShape * coverage;  // Improve appearance of smaller clouds.

	// Added coverage check since the remap breaks if it's zero. Note that this case only happens during cone
	// sampling, since the base shape acts as a convex hull and cannot be zero when the detail wouldn't be normally.
	if (detailSample && finalShape > 0.0)
	{
		// Perturb the detail shape by wind distortion - stronger towards the bottom of the clouds.
		const float curlDistortion = 0.4;
		const float3 curl = SampleCurlNoise(curlNoise, position);
		const float3 curlOffset = curl * (1.0 - heightFraction) * curlDistortion;
		
		float detailShape = SampleDetailShape(detailNoise, position, curlOffset);

		// Gradient from wispy to billowy shapes by height.
		detailShape = lerp(detailShape, 1.0 - detailShape, saturate(heightFraction * 10.0));

		// Erode the final shape.
		finalShape = RemapRange(finalShape, detailShape * 0.2, 1.0, 0.0, 1.0);
	}

	const float densityMultiplier = 2.416;
	
	return max(finalShape, 0) * densityMultiplier;  // #TODO: Should be able to remove the max.
}

// The noise kernel is a global segment, cached once when the ray march begins.
static const int noiseKernelSize = 6;
static float3 noiseKernel[noiseKernelSize];

void ComputeNoiseKernel(float3 lightDirection)
{
	// Normalized vectors in a 45 degree cone centered around the x axis.
	static const float3 noise[] = {
		float3(0.75156066, -0.22399792, 0.62046878),
		float3(0.86879559, 0.27513754, -0.41169595),
		float3(0.72451426, -0.68184453, 0.10083214),
		float3(0.80962046, 0.16187219, 0.56419156),
		float3(0.95949856, 0.25681095, 0.11580438),
		float3(1, 0, 0)  // Centered to accurately sample occluding clouds.
	};

	// Rotate the noise vectors towards the light vector.
	// https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
	// Use cos/sin derived from the dot product directly; acos+sin loses precision and adds two transcendentals per kernel entry.
	for (int i = 0; i < noiseKernelSize; ++i)
	{
		const float3 vec = noise[i];
		const float3 rotationAxis = cross(vec, lightDirection);
		const float cosTheta = clamp(dot(vec, lightDirection), -1.0, 1.0);
		const float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
		noiseKernel[i] = vec * cosTheta + cross(rotationAxis, vec) * sinTheta + rotationAxis * dot(rotationAxis, vec) * (1.0 - cosTheta);
	}

	noiseKernel[noiseKernelSize - 1] *= 3;  // Long-distance sample.
}

float SampleCloudDensityCone(Texture2D<float3> weatherTexture, Texture3D<float> baseNoise, Texture3D<float> detailNoise,
	Texture3D<float4> curlNoise, float3 position, float2 wind, float time)
{
	const float stepSize = 375.0 / 1000.0;  // 375m.
	const int coneSamples = noiseKernelSize;
	float density = 0.0;

	// N-1 samples nearby, 1 far away to capture shadows cast by distant clouds.
	// See slide 85 of: https://www.guerrilla-games.com/media/News/Files/The-Real-time-Volumetric-Cloudscapes-of-Horizon-Zero-Dawn.pdf
	for (int i = 0; i < coneSamples; ++i)
	{
		float3 samplePosition = position + (stepSize * (float)i * noiseKernel[i]);

		// Cone sample left the cloud layer, bail out. Need to check here since math breaks in SampleCloudDensity if sampling out of bounds.
		float heightFraction = GetHeightFractionForPoint(samplePosition, float2(cloudLayerBottom, cloudLayerTop));
		if (heightFraction > 1.f)
			break;

		// Apply an increased contribution to the long-distance occluding sample.
		const float densityMultiplier = max(1.2 * (i - coneSamples + 3), 1);

		// Once the density has reached 0.3, switch to low-detail noise. Refer to slide 86.
		const bool detailSamples = density < 0.3;
		density += SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, samplePosition, wind, time, detailSamples, 0) * densityMultiplier;
	}

	return max(density, 0);
}

float ComputeBeersLaw(float value, float absorption)
{
	return exp(-value * absorption);  // Absorption increases for rain clouds.
}

float ComputePhaseFunction(float nu)
{
	// Dual-lobe from Frostbite, better accounts for back scattering.
	// #TODO: Experiment with a triple HG phase.
	float a = HenyeyGreensteinPhase(nu, -0.48);
	float b = HenyeyGreensteinPhase(nu, 0.75);
	return (a + b) / 2.0;
}

float ComputeInScatterProbability(float localDensity, float heightFraction)
{
	const float depthProbability = 0.05 + pow(localDensity, max(RemapRange(heightFraction, 0.3, 0.85, 0.5, 2.0), 0.001));
	//const float verticalProbability = pow(RemapRange(heightFraction, 0.07, 0.14, 0.1, 1.0), 0.8);  // Nubis' implementation.
	const float verticalProbability = pow(RemapRange(heightFraction, 0.02, 0.15, 0.1, 1.0), 1.8);

	return depthProbability * verticalProbability;
}

float ComputeLightEnergy(Texture2D<float3> weatherTexture, Texture3D<float> baseNoise, Texture3D<float> detailNoise,
	Texture3D<float4> curlNoise, float3 position, float densityToLight, float viewDotLight, float2 wind, float time)
{
	// Lighting model inspired by GPU Pro 7 page 119, Frostbite, and Nubis 2017 real-time volumetric cloudscapes.

	float3 weather = SampleWeather(weatherTexture, position);
	float absorption = weather.z;

	// Sample the mean density at the sample position using a higher mip level.
	// #TODO: We might be able to get away with not using the full density sample, try just applying coverage and height gradient?
	float localDensity = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, position, wind, time, false, 2);
	
	// Restrict the minimum local density, otherwise very thin edges will have nearly 0 density and thus no in scattering light.
	localDensity += 2;

	const float outScatter = ComputeBeersLaw(densityToLight * 0.8, absorption);
	const float phase = ComputePhaseFunction(viewDotLight);
	const float inScatter = ComputeInScatterProbability(localDensity, GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop)));

	return outScatter * phase * inScatter;
}

#ifdef CLOUDS_DEBUG_MARCHCOUNT
#define MARCH_RESULT int
#define RETURN_EARLYOUT 0
#else
// Standard rendering.
#define MARCH_RESULT void
#define RETURN_EARLYOUT
#endif

// ComputeNoiseKernel must have been called prior to this function to setup the cone sampling.
// Jitter is in the domain of [-1, 1].
MARCH_RESULT RayMarchInternal(Texture3D<float> baseShapeNoiseTexture, Texture3D<float> detailShapeNoiseTexture, Texture3D<float4> curlNoiseTexture,
	StructuredBuffer<float3> atmosphereIrradiance, Texture2D<float3> weatherTexture, float3 origin, float3 direction, float jitter,
	float marchStart, float marchEnd, float3 sunDirection, float2 wind, float time, out float3 scatteredLuminance, out float transmittance,
	out float depth)
{
	// Clear again in case the outer caller didn't.
	scatteredLuminance = 0.xxx;
	transmittance = 1;
	depth = 1000000;  // Assume very far away.

	const float zDot = abs(dot(direction, float3(0, 0, 1)));
	const float viewDotLight = dot(direction, sunDirection);
	
	// Low detail reduces step count as well.
#ifndef CLOUDS_LOW_DETAIL
	const int baseStepCount = 150;
	const float smallStepMultiplier = 0.2;
#else
	const int baseStepCount = 14;
	const float smallStepMultiplier = 0.35;
#endif

	const int steps = (baseStepCount - (baseStepCount * 0.4 * zDot));  // Slightly more than half at zenith, baseStepCount at horizon.
	const float marchWidth = marchEnd - marchStart;
	float largeStepSize = lerp(0.2f, 0.12f, zDot) + 0.5f * (marchWidth / (float)steps);
	float smallStepSize = largeStepSize * smallStepMultiplier;
	const int stepTransitionMargin = 6;
	
	// Apply jitter, such as from blue noise.
	marchStart += largeStepSize * jitter;

	// Move the origin to be at the march start location.
	origin = origin + direction * marchStart;

	float dist = 0.f;
	int detailSteps = 0;  // If >0, march in small steps.

	// 2nd octave accumulators for the multiple-scattering approximation. Total in-scattered radiance is the sum of all octaves.
	float3 scatteredLuminance2 = 0.xxx;
	float3 transmittance2 = 1.xxx;

#ifdef CLOUDS_DEBUG_MARCHCOUNT
	int loopCount = 0;
#endif

#ifdef CLOUDS_MARCH_GROUND_TRUTH_DETAIL
	// Marching in ground truth detail is very expensive, especially for shadow mapping when the sun is low in the sky.

	// Fixed size steps that are very small for extra detail.
	largeStepSize = 0.08f;
	smallStepSize = largeStepSize * smallStepMultiplier;

	for (int i = 0; dist < marchWidth; ++i)
#else
	for (int i = 0; i < steps; ++i)
#endif
	{
		// If the march count debugging is enabled, save i each iteration.
#ifdef CLOUDS_DEBUG_MARCHCOUNT
		loopCount = i + 1;
#endif
		
		if (dist > marchEnd)
			break;  // Left the cloud layer.

		float3 position = origin + direction * dist;

		const bool detailSamples = detailSteps > 0;
		float cloudDensity = SampleCloudDensity(weatherTexture, baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, position, wind, time, detailSamples, 0);

		// If we're in open space, take large steps. If we're in a cloud or just recently left one, take small steps.
		if (cloudDensity > 0.0)
		{
			if (detailSteps == 0)
			{
				// Just entered a cloud, step back to ensure we didn't miss any detail.

				// If we start marching inside of a cloud, we don't want to accumulate any cloud behind the camera (negative distance).
				dist = max(dist - largeStepSize, 0);
				i -= 1;  // Repeat the step.
				detailSteps = stepTransitionMargin;

				// We don't want the density sample contributing since we might've missed a chunk and need to backstep.
				continue;
			}

			else
			{
				dist += smallStepSize;
			}

			detailSteps = stepTransitionMargin;

			float coneDensity = SampleCloudDensityCone(weatherTexture, baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, position, wind, time);
			coneDensity = (coneDensity + cloudDensity) / float(noiseKernelSize + 1);
			
#ifdef CLOUDS_LOW_DETAIL
			coneDensity *= 5;
#endif
			
			// Depth-only rendering does not need to evaluate the lighting model.
#if !defined(CLOUDS_ONLY_DEPTH) && !defined(CLOUDS_DEPTH_ACCURATE_MODEL)
			float lightEnergy = ComputeLightEnergy(weatherTexture, baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, position, coneDensity, viewDotLight, wind, time);

			float3 cameraPositionAtmoSpace = position;  // Position is already in kilometers.
			float3 cameraPoint = cameraPositionAtmoSpace - planetCenter;

			// Clouds don't have a surface normal, but they don't need one. Light from the sun hits the media at any angle,
			// and scatters within it regardless. Therefore, set the normal to be aligned with the sun such that no direct
			// irradiance is lost.
			float3 normal = sunDirection;

			const float3 separatedSunIrradianceClouds = atmosphereIrradiance[2];
			const float3 separatedSkyIrradianceClouds = atmosphereIrradiance[3];

			float3 sunIrradiance;
			float3 skyIrradiance;
			RecomposeSeparableSunAndSkyIrradiance(cameraPoint, normal, sunDirection, separatedSunIrradianceClouds,
				separatedSkyIrradianceClouds, sunIrradiance, skyIrradiance);

			// The approximate unattenuated energy hitting the clouds is the full combined sun and sky irradiance.
			// Note that sun/sky visibility is NOT used here, as the clouds are the ones blocking the light from the atmosphere.
			float3 energy = sunIrradiance + skyIrradiance;
			energy *= lightEnergy * 0.118;  // Attenuate the atmospheric irradiance by the cloud lighting model.
			
			//float stepSize = smallStepSize * 1000.0;  // Kilometers to meters.
			// Don't perform a physically accurate scattering integration, as numerical precision issues are happening.
			// Instead, a bit of artistic license is leveraged here.
			float stepSize = smallStepSize * 200;

			// Coefficients try to approximate real cloud behavior. Multiple sources were used to derive these numbers,
			// but I did not do an extensive read into any of them and instead I'm using more artistic license here.
			// References:
			// 0.05: http://www.patarnott.com/satsens/pdf/opticalPropertiesCloudsReview.pdf
			// 0.026: https://amt.copernicus.org/articles/14/4959/2021
			float3 scattCoeff = 0.124.xxx;
			float3 absorCoeff = 0.xxx;  // Cloud albedo ~= 1.
			float3 trans = transmittance.xxx;
#ifdef CLOUDS_LOW_DETAIL
			scattCoeff = 0.04.xxx;
#endif
			ComputeScatteringIntegration(cloudDensity, energy, stepSize, scattCoeff, absorCoeff, scatteredLuminance, trans);
			transmittance = trans.x;  // Scattering and absorbtion are uniform, so just use one channel.

			// Multiple scattering approximation from Wrenninge.
			// Octave 2 is integrated independently of octave 1 with attenuated scattering and extinction, sum at the end.
			// See: https://gitea.yiem.net/QianMo/Real-Time-Rendering-4th-Bibliography-Collection/raw/branch/main/Chapter%201-24/[1909]%20[SIGGRAPH%202013]%20Oz-%20The%20Great%20and%20Volumetric.pdf
			const float msScattAtten = 0.5;
			const float msExtAtten = 0.5;
			ComputeScatteringIntegration(cloudDensity, energy, stepSize, scattCoeff * msScattAtten, absorCoeff * msExtAtten,
				scatteredLuminance2, transmittance2);
#else
			// Very simple approximation of transmittance.
			float simpleExtinction = 0.08;
			transmittance *= exp(-simpleExtinction * smallStepSize * 1000.0).xxx;
#endif  // CLOUDS_ONLY_DEPTH

			// Update the depth until about 50% light transmittance, this is a decent approximation given that clouds have no surface.
			// #TODO: Use Frostbite's improved depth approximation, also look at bitsquid's method.
			if (transmittance > 0.5f || depth > 100000)
				depth = marchStart + dist;
		}

		else
		{
			if (detailSteps > 0)
				dist += smallStepSize;  // Just left a cloud, continue to walk in small steps for a little bit.
			else
				dist += largeStepSize;

			detailSteps = max(detailSteps - 1, 0);
		}

		// Fully opaque sample, any additional steps won't contribute any visual difference, so early out.
		if (transmittance < 0.01f)
			break;
	}

	// Sum the second-octave multiple-scattering contribution into the final radiance.
	// Intentionally don't MS accumulate the transmittance as well.
	scatteredLuminance += scatteredLuminance2;

#ifdef CLOUDS_DEBUG_MARCHCOUNT
	return loopCount;
#endif
}

MARCH_RESULT RayMarchClouds(Texture3D<float> baseShapeNoiseTexture, Texture3D<float> detailShapeNoiseTexture, Texture3D<float4> curlNoiseTexture,
	StructuredBuffer<float3> atmosphereIrradiance, Texture2D<float3> weatherTexture, Texture2D<float> geometryDepthTexture, Texture2D<float> blueNoiseTexture,
	Camera camera, float2 baseUv, float2 jitteredUv, uint2 outputResolution, float3 direction, float3 sunDirection, float2 wind, float time,
	out float3 scatteredLuminance, out float transmittance, out float depth)
{
	// Necessary in case this outer call early-outs.
	scatteredLuminance = 0.xxx;
	transmittance = 1;
	depth = 1000000;  // Assume very far away.
	
	float marchStart;
	float marchEnd;

	float3 origin = camera.position.xyz;

#ifndef CLOUDS_CAMERA_IN_KILOMETERS
	origin *= 1.0 / 1000.0;  // Meters to kilometers.
#endif

#ifdef CLOUDS_RENDER_ORTHOGRAPHIC
	// Find two perpendicular vectors in the plane defined by the ray direction. PlaneA is defined along the Y axis
	// since the sun will never have a Y component to its vector.
	const float3 planeA = float3(0.f, 1.f, 0.f);
	const float3 planeB = cross(direction, planeA);
	const float2 uvScaled = jitteredUv * 2.0 - 1.0;

	// Not sure why the 0.5 is needed.. oh well
	origin += uvScaled.x * -planeA * CLOUDS_ORTHOGRAPHIC_SCALE * 0.5f;
	origin += uvScaled.y * planeB * CLOUDS_ORTHOGRAPHIC_SCALE * 0.5f;
#endif

	const float planetRadius = 6360.0;  // #TODO: Get from atmosphere data.

	float2 topBoundaryIntersect;
	if (RaySphereIntersection(origin, direction, planetCenter, planetRadius + cloudLayerTop, topBoundaryIntersect))
	{
		float2 bottomBoundaryIntersect;
		if (RaySphereIntersection(origin, direction, planetCenter, planetRadius + cloudLayerBottom, bottomBoundaryIntersect))
		{
			float top = all(topBoundaryIntersect > 0) ? min(topBoundaryIntersect.x, topBoundaryIntersect.y) : max(topBoundaryIntersect.x, topBoundaryIntersect.y);
			float bottom = all(bottomBoundaryIntersect > 0) ? min(bottomBoundaryIntersect.x, bottomBoundaryIntersect.y) : max(bottomBoundaryIntersect.x, bottomBoundaryIntersect.y);
			if (all(bottomBoundaryIntersect > 0))
				top = max(0, min(topBoundaryIntersect.x, topBoundaryIntersect.y));
			marchStart = min(bottom, top);
			marchEnd = max(bottom, top);
		}

		else
		{
			// Inside the cloud layer, only advance the ray start if we're outside of the atmosphere.
			marchStart = max(topBoundaryIntersect.x, 0);
			marchEnd = topBoundaryIntersect.y;
		}
	}

	else
	{
		// Outside of the cloud layer.
		return RETURN_EARLYOUT;
	}

	// Stop short if we hit the planet.
	float2 planetIntersect;
	if (RaySphereIntersection(origin, direction, planetCenter, planetRadius, planetIntersect))
	{
		marchEnd = min(marchEnd, planetIntersect.x);
	}

	marchStart = max(0, marchStart);
	marchEnd = max(0, marchEnd);

	// Early out of the march if we hit opaque geometry.
	// Note the use of the minimum filter, which provides a more conservative rendering against the geometry mask, to prevent a thin
	// border of unrendered clouds appearing around geometry.
	float geometryDepth = geometryDepthTexture.Sample(linearMipPointClampMinimum, jitteredUv);
	geometryDepth = LinearizeDepth(camera, geometryDepth) * camera.farPlane;
	if (geometryDepth < camera.farPlane)
	{
		geometryDepth *= 0.001;  // Meters to kilometers.
		marchEnd = min(marchEnd, geometryDepth);
	}

	if (marchEnd <= marchStart)
	{
		return RETURN_EARLYOUT;
	}

	// Offset the origin with blue noise to prevent banding artifacts. See: https://www.diva-portal.org/smash/get/diva2:1223894/FULLTEXT01.pdf
	uint blueNoiseWidth, blueNoiseHeight;
	blueNoiseTexture.GetDimensions(blueNoiseWidth, blueNoiseHeight);
	const float upscaleResolutionMultiplier = 4.f;
	// Sample blue noise at one pixel per upscaled sample, so scale the coordinates by the resolution scale.
	float2 blueNoiseSamplePos = jitteredUv * outputResolution * upscaleResolutionMultiplier;
	blueNoiseSamplePos = blueNoiseSamplePos / float2(blueNoiseWidth, blueNoiseHeight);
	float rayOffset = blueNoiseTexture.Sample(pointWrap, blueNoiseSamplePos);
	float jitter = rayOffset;  // Note: don't rescale to [-1, 1], as this could render participating media behind the camera.
	
#ifdef CLOUDS_LOW_DETAIL
	// Low detail clouds cannot afford a well-jittered sample.
	jitter *= 0.4;
#endif

	// Precompute the noise kernel once per pixel. RayMarchInternal expects this to be set up before being called.
	ComputeNoiseKernel(sunDirection);

#ifdef CLOUDS_DEBUG_MARCHCOUNT
	return RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture, origin, direction,
		jitter, marchStart, marchEnd, sunDirection, wind, time, scatteredLuminance, transmittance, depth);
#else
	RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture, origin, direction, jitter,
		marchStart, marchEnd, sunDirection, wind, time, scatteredLuminance, transmittance, depth);
#endif
}

#endif  // __CLOUDS_CORE_HLSLI__