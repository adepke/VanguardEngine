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
#include "Atmosphere/SkyAmbient.hlsli"

struct CloudLightInput
{
	float precipitation;
	float opticalDepthToSun;  // Cone-sampled from hit towards sun
	float ambientVisibility;  // Sky visibility, approximated via height fraction
	float viewDotLight;
};

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
	
	return max(finalShape, 0);  // #TODO: Should be able to remove the max.
}

// Approximate the volume's "outwards" vector. This is extremely expensive and run often, a very
// large contender for places to optimize and rethink.
float3 ComputeCloudNormal(Texture2D<float3> weatherTexture, Texture3D<float> baseNoise, Texture3D<float> detailNoise,
	Texture3D<float4> curlNoise, float3 position, float2 wind, float time)
{
	const float h = 0.1f;  // km. larger = smoother normal, smaller = more local
	const uint mip = 2;  // Use a coarse mip
	const float dxp = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, position + float3(h, 0, 0), wind, time, false, mip);
	const float dxn = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, position - float3(h, 0, 0), wind, time, false, mip);
	const float dyp = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, position + float3(0, h, 0), wind, time, false, mip);
	const float dyn = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, position - float3(0, h, 0), wind, time, false, mip);
	const float dzp = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, position + float3(0, 0, h), wind, time, false, mip);
	const float dzn = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, position - float3(0, 0, h), wind, time, false, mip);

	const float3 gradient = float3(dxp - dxn, dyp - dyn, dzp - dzn);
	const float gradLen = length(gradient);
	
	// Default to vertical normal in a uniform-ish region.
	if (gradLen < 1e-4f)
		return float3(0, 0, 1);

	// Normal points from interior (high density) to exterior (low density).
	return -gradient / gradLen;
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
	const float3 rotationAxisRaw = cross(float3(1, 0, 0), lightDirection);
	const float sinTheta = length(rotationAxisRaw);
	const float cosTheta = clamp(lightDirection.x, -1.0, 1.0);  // dot(+X, L).

	for (int i = 0; i < noiseKernelSize; ++i)
	{
		const float3 vec = noise[i];
		if (sinTheta > 0.00001)
		{
			const float3 k = rotationAxisRaw / sinTheta;
			noiseKernel[i] = vec * cosTheta + cross(k, vec) * sinTheta + k * dot(k, vec) * (1.0 - cosTheta);
		}
		else
		{
			// Light is parallel to the cone axis: identity, or mirrored for an anti-parallel light.
			noiseKernel[i] = cosTheta > 0.0 ? vec : float3(-vec.x, vec.y, -vec.z);
		}
	}

	noiseKernel[noiseKernelSize - 1] *= 3;  // Long-distance sample.
}

// Cloud particle coefficients, per meter at density 1. Coefficients try to approximate real
// cloud behavior. Multiple sources were used to derive these numbers, but I did not do an
// extensive read into any of them and instead I'm using more artistic license here.
// References:
// 0.05/m: http://www.patarnott.com/satsens/pdf/opticalPropertiesCloudsReview.pdf
// 0.026/m: https://amt.copernicus.org/articles/14/4959/2021
#ifndef CLOUDS_LOW_DETAIL
static const float3 cloudScatteringCoeff = 0.03.xxx;
#else
static const float3 cloudScatteringCoeff = 0.04.xxx;  // Compensates the sparse low-detail march.
#endif
static const float3 cloudAbsorptionCoeff = 0.xxx;  // Cloud albedo ~= 1.
static const float3 cloudExtinctionCoeff = cloudScatteringCoeff + cloudAbsorptionCoeff;

// Floor of the sky-ambient visibility estimate at the very bottom of the cloud layer.
static const float cloudAmbientFloor = 0.25;

// Effective water-droplet diameter. Typical cumulus cloud droplets are 5-15 um. Larger
// values (20-50 um) produce a sharper forward peak / silver lining.
static const float cloudDropletDiameter = 12.0;  // Microns

// Wrenninge multiple-scattering octave attenuations.
//   a = scattering attenuation per octave: scattering coefficient *= a^n.
//   b = extinction attenuation per octave: optical depth *= b^n.
//   c = phase attenuation per octave: phase function blends toward isotropic with c^n weight.
// Reducing these values produces a more diffuse/flatter multi-scatter contribution. Increasing
// them makes successive octaves preserve more of the single-scatter character.
static const float msScattAttenuation = 0.5;
static const float msExtinctionAttenuation = 0.7;  // Must be > scattering attenuation.
static const float msPhaseAttenuation = 0.5;

#ifndef CLOUDS_MS_OCTAVES
#define CLOUDS_MS_OCTAVES 3
#endif
#if CLOUDS_MS_OCTAVES < 1
#error CLOUDS_MS_OCTAVES must be at least 1.
#endif

float SampleCloudOpticalDepthCone(Texture2D<float3> weatherTexture, Texture3D<float> baseNoise, Texture3D<float> detailNoise,
	Texture3D<float4> curlNoise, float3 position, float2 wind, float time, float densityMultiplier)
{
	const float stepSizeMeters = 375.0;
	const int coneSamples = noiseKernelSize;
	float opticalDepth = 0.0;

	// N-1 samples nearby, 1 far away to capture shadows cast by distant clouds.
	// See slide 85 of: https://www.guerrilla-games.com/media/News/Files/The-Real-time-Volumetric-Cloudscapes-of-Horizon-Zero-Dawn.pdf
	for (int i = 0; i < coneSamples; ++i)
	{
		float3 samplePosition = position + ((stepSizeMeters / 1000.0) * (float)i * noiseKernel[i]);

		// Cone sample left the cloud layer, bail out. Need to check here since math breaks in SampleCloudDensity if sampling out of bounds.
		float heightFraction = GetHeightFractionForPoint(samplePosition, float2(cloudLayerBottom, cloudLayerTop));
		if (heightFraction > 1.f)
			break;
		
		// Quadrature weighting to appropriately capture the extra influence of the long distance sample.
		const float segmentMeters = (i == coneSamples - 1) ? stepSizeMeters * 2.0 * (float)(coneSamples - 1) : stepSizeMeters;

		// Switch to cheap low detail samples if the media is sufficiently dense.
		const bool detailSamples = opticalDepth < 0.3;
		const float density = SampleCloudDensity(weatherTexture, baseNoise, detailNoise, curlNoise, samplePosition, wind, time, detailSamples, 0) * densityMultiplier;
		opticalDepth += cloudExtinctionCoeff.x * density * segmentMeters;
	}

	return max(opticalDepth, 0);
}

float ComputeBeersLaw(float value, float absorption)
{
	return exp(-value * absorption);  // Absorption increases for rain clouds.
}

float ComputePhaseFunction(float nu)
{
	// Dual-lobe from Frostbite, better accounts for back scattering.
	// #TODO: Experiment with a triple HG phase.
	/*
	float a = HenyeyGreensteinPhase(nu, -0.48);
	float b = HenyeyGreensteinPhase(nu, 0.75);
	return (a + b) / 2.0;
	*/
	
	// Testing a new phase that accounts for water droplet size. Appears to be current state of the art.
	return JendersieDEonPhase(nu, cloudDropletDiameter);
}

// Blends the single-scatter phase towards isotropic as light continues to bounce.
float ComputePhaseFunctionMS(float nu, float singleScatterWeight)
{
	const float isotropic = IsotropicPhase(0.f);  // Theta ignored.
	const float singleScatter = ComputePhaseFunction(nu);
	return lerp(isotropic, singleScatter, singleScatterWeight);
}

CloudLightInput PrepareCloudLighting(Texture2D<float3> weatherTexture, float3 position, float opticalDepthToSun, float viewDotLight)
{
	CloudLightInput light;
	light.precipitation = SampleWeather(weatherTexture, position).z;
	light.opticalDepthToSun = opticalDepthToSun;
	light.viewDotLight = viewDotLight;
	const float heightFraction = GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop));
	light.ambientVisibility = lerp(cloudAmbientFloor, 1.0, heightFraction);

	return light;
}

// Per-octave light energy, contributed by sun and sky. These are multipliers instead of total energy.
struct CloudLightWeight
{
	float direct;
	float ambient;
};

CloudLightWeight ComputeLightEnergy(CloudLightInput lighting, float extinctionAttenuationN, float directPhase)
{
	const float outScatter = ComputeBeersLaw(lighting.opticalDepthToSun * extinctionAttenuationN, lighting.precipitation);

	// Use the powder effect for in-scatter to provide silver lining.
	const float powder = 1.0 - exp(-2.0 * lighting.opticalDepthToSun * extinctionAttenuationN);
	const float powderBlend = lerp(powder, 1.0, saturate(lighting.viewDotLight * 0.5 + 0.5));

	CloudLightWeight w;
	w.direct  = outScatter * powderBlend * directPhase;
	w.ambient = lighting.ambientVisibility;
	return w;
}

#if defined(CLOUDS_DEBUG_MARCHCOUNT)
#define MARCH_RESULT int
#define RETURN_EARLYOUT 0
#elif defined(CLOUDS_DEBUG_NORMALVECTOR)
#define MARCH_RESULT float3
#define RETURN_EARLYOUT 0.xxx
#else
// Standard rendering.
#define MARCH_RESULT void
#define RETURN_EARLYOUT
#endif

// ComputeNoiseKernel must have been called prior to this function to setup the cone sampling.
// Jitter is in the domain of [-1, 1]. gapStart/gapEnd optionally describe a chunk to skip through,
// like if the camera is within the cloud layer and the ray crosses out, then back in towards the horizon.
MARCH_RESULT RayMarchInternal(Texture3D<float> baseShapeNoiseTexture, Texture3D<float> detailShapeNoiseTexture, Texture3D<float4> curlNoiseTexture,
	StructuredBuffer<float3> atmosphereIrradiance, Texture2D<float3> weatherTexture, float3 origin, float3 direction, float jitter,
	float marchStart, float marchEnd, float gapStart, float gapEnd, float3 sunDirection, float2 wind, float time, float density,
	out float3 scatteredLuminance, out float transmittance, out float depth)
{
	// Clear again in case the outer caller didn't.
	scatteredLuminance = 0.xxx;
	transmittance = 1;
	depth = 1000000;  // Assume very far away.

	const float zDot = abs(dot(direction, float3(0, 0, 1)));
	const float viewDotLight = dot(direction, sunDirection);

#if !defined(CLOUDS_ONLY_DEPTH)
	// Load common lighting info for the march. The sky SH comes from a probe in the cloud layer,
	// no need to have more than 1 probe in the clouds, so sky ambience can be loaded here too.
	// Note no Lambertian consine factor is applied since clouds are a volume not a surface.
	const float3 sunIrradiance = LoadSunIrradianceCloud(atmosphereIrradiance);
	const SH::L2_RGB skySH = LoadSkySHCloud(atmosphereIrradiance);

	// Precompute the phase function constants, since it only varies by V*L. Need to profile, but if
	// this is fairly expensive, consider moving to offline lookup table.
	float msScattAttenN[CLOUDS_MS_OCTAVES];
	float msExtAttenN[CLOUDS_MS_OCTAVES];
	float msDirectPhase[CLOUDS_MS_OCTAVES];
	{
		float a = 1.f;
		float b = 1.f;
		float c = 1.f;
		[unroll]
		for (int octave = 0; octave < CLOUDS_MS_OCTAVES; ++octave)
		{
			msScattAttenN[octave] = a;
			msExtAttenN[octave] = b;
			msDirectPhase[octave] = ComputePhaseFunctionMS(viewDotLight, c);
			a *= msScattAttenuation;
			b *= msExtinctionAttenuation;
			c *= msPhaseAttenuation;
		}
	}
#endif  // !CLOUDS_ONLY_DEPTH

	// Low detail reduces step count as well.
#ifndef CLOUDS_LOW_DETAIL
	const int baseStepCount = 150;
	const float smallStepMultiplier = 0.2;
#else
	const int baseStepCount = 14;
	const float smallStepMultiplier = 0.35;
#endif

	const int steps = (baseStepCount - (baseStepCount * 0.4 * zDot));  // Slightly more than half at zenith, baseStepCount at horizon.
	const float marchWidth = marchEnd - marchStart;  // Includes the gap.
	// Clamp the gap to the march range.
	gapStart = min(gapStart, marchEnd);
	gapEnd = min(gapEnd, marchEnd);
	// Don't factor the gap (if any) into the step size.
	const float gapWidth = max(gapEnd - gapStart, 0.f);
	float largeStepSize = lerp(0.2f, 0.12f, zDot) + 0.5f * (max(marchWidth - gapWidth, 0.f) / (float)steps);
	float smallStepSize = largeStepSize * smallStepMultiplier;
	const int stepTransitionMargin = 6;

	// Make the gap relative to the march origin, matching the domain of dist below.
	gapStart -= marchStart;
	gapEnd -= marchStart;

	// Apply jitter, such as from blue noise.
	marchStart += largeStepSize * jitter;

	// Move the origin to be at the march start location.
	origin = origin + direction * marchStart;

	float dist = 0.f;
	int detailSteps = 0;  // If >0, march in small steps.

#if CLOUDS_MS_OCTAVES > 1
	float3 scatteredLuminanceMS[CLOUDS_MS_OCTAVES - 1];
	float3 transmittanceMS[CLOUDS_MS_OCTAVES - 1];
	[unroll]
	for (int octave = 0; octave < CLOUDS_MS_OCTAVES - 1; ++octave)
	{
		scatteredLuminanceMS[octave] = 0.xxx;
		transmittanceMS[octave] = 1.xxx;
	}
#endif

#ifdef CLOUDS_DEBUG_MARCHCOUNT
	int loopCount = 0;
#elif CLOUDS_DEBUG_NORMALVECTOR
	// Get the normal of the surface, not of the interior.
	float3 debugFirstNormal = 0.0.xxx;
	bool debugFirstNormalCaptured = false;
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
		
		if (dist > marchWidth)
			break;  // Left the cloud layer.

		// Skip over the gap.
		if (gapWidth > 0.f && dist >= gapStart && dist < gapEnd)
		{
			dist = gapEnd;
		}

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
			
			// Scale the density from the non-cone sample, which doesn't already factor in the multiplier.
			cloudDensity *= density;

			// Depth-only rendering does not need to evaluate the lighting model.
#if !defined(CLOUDS_ONLY_DEPTH)
			const float opticalDepthToSun = SampleCloudOpticalDepthCone(weatherTexture, baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, position, wind, time, density);

			const float3 cloudNormal = ComputeCloudNormal(weatherTexture, baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, position, wind, time);
			const float3 skyAmbient = max(SH::CalculateIrradiance(skySH, cloudNormal), 0.0.xxx) / SH::Pi;

			// The light input is used for all MS octaves.
			CloudLightInput lightInput = PrepareCloudLighting(weatherTexture, position, opticalDepthToSun, viewDotLight);
			
#ifdef CLOUDS_DEBUG_NORMALVECTOR
			if (!debugFirstNormalCaptured)
			{
				debugFirstNormal = cloudNormal;
				debugFirstNormalCaptured = true;
			}
#endif

			float stepSize = smallStepSize * 1000.0;  // Kilometers to meters.

			const float3 scattCoeff = cloudScatteringCoeff;
			const float3 extCoeff = cloudExtinctionCoeff * lightInput.precipitation;

			// Multiple-scattering approximation from Wrenninge
			// See: https://gitea.yiem.net/QianMo/Real-Time-Rendering-4th-Bibliography-Collection/raw/branch/main/Chapter%201-24/[1909]%20[SIGGRAPH%202013]%20Oz-%20The%20Great%20and%20Volumetric.pdf
			{
				// Octave 0
				const CloudLightWeight w0 = ComputeLightEnergy(lightInput, msExtAttenN[0], msDirectPhase[0]);
				const float3 octaveEnergy0 = sunIrradiance * w0.direct + skyAmbient * w0.ambient;
				float3 trans = transmittance.xxx;
				ComputeScatteringIntegration(cloudDensity, octaveEnergy0, stepSize, scattCoeff, extCoeff, scatteredLuminance, trans);
				transmittance = trans.x;  // Scattering and extinction are uniform, so just use one channel.

#if CLOUDS_MS_OCTAVES > 1
				// Octaves 1..N-1
				[unroll]
				for (int octave = 1; octave < CLOUDS_MS_OCTAVES; ++octave)
				{
					const CloudLightWeight w = ComputeLightEnergy(lightInput, msExtAttenN[octave], msDirectPhase[octave]);
					const float3 octaveEnergy = sunIrradiance * w.direct + skyAmbient * w.ambient;
					ComputeScatteringIntegration(cloudDensity, octaveEnergy, stepSize, scattCoeff * msScattAttenN[octave], extCoeff * msExtAttenN[octave], scatteredLuminanceMS[octave - 1], transmittanceMS[octave - 1]);
				}
#endif
			}
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

	// Sum the higher-order multiple-scattering octaves into the final radiance.
	// Intentionally don't MS accumulate the transmittance as well.
#if CLOUDS_MS_OCTAVES > 1
	[unroll]
	for (int msSum = 0; msSum < CLOUDS_MS_OCTAVES - 1; ++msSum)
	{
		scatteredLuminance += scatteredLuminanceMS[msSum];
	}
#endif

#ifdef CLOUDS_DEBUG_MARCHCOUNT
	return loopCount;
#endif
#ifdef CLOUDS_DEBUG_NORMALVECTOR
	return debugFirstNormal;
#endif
}

MARCH_RESULT RayMarchClouds(Texture3D<float> baseShapeNoiseTexture, Texture3D<float> detailShapeNoiseTexture, Texture3D<float4> curlNoiseTexture, StructuredBuffer<float3> atmosphereIrradiance,
	Texture2D<float3> weatherTexture, Texture2D<float> geometryDepthTexture, Texture2D<float> blueNoiseTexture, Camera camera, float2 baseUv, float2 jitteredUv,
	uint2 outputResolution, float3 direction, float3 sunDirection, float2 wind, float time, float density, out float3 scatteredLuminance, out float transmittance,
	out float depth)
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

	// A view ray can intersect the cloud layer in two disjoint segments, if it is inside the layer and the ray
	// leaves, then re-enters towards the horizon. In this case, there's a significant gap of air to jump past.
	// If we didn't handle this, these horizon clouds would disappear when inside the cloud layer.
	float gapStart = 0.f;
	float gapEnd = 0.f;

	float2 topBoundaryIntersect;
	if (RaySphereIntersection(origin, direction, planetCenter, planetRadius + cloudLayerTop, topBoundaryIntersect))
	{
		// Start a shell entry point, or at 0 if camera is inside layer.
		marchStart = max(topBoundaryIntersect.x, 0);
		marchEnd = topBoundaryIntersect.y;

		float2 bottomBoundaryIntersect;
		if (RaySphereIntersection(origin, direction, planetCenter, planetRadius + cloudLayerBottom, bottomBoundaryIntersect))
		{
			if (all(bottomBoundaryIntersect > 0))
			{
				// Special case: ray leaves the bottom layer and re-enters later, causing a gap.
				gapStart = bottomBoundaryIntersect.x;
				gapEnd = bottomBoundaryIntersect.y;
			}
			else if (bottomBoundaryIntersect.y > 0)
			{
				marchStart = max(marchStart, bottomBoundaryIntersect.y);
			}
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
	// Sample at the unjittered UV in 4 taps to get a conservative mask. Without this, there's a single pixel
	// seam around the edges of geometry where clouds are behind them. Compose pass does exact per pixel occlusion.
	const float2 footprintOffset = 0.5f / float2(outputResolution);
	float rawGeometryDepth = geometryDepthTexture.Sample(linearMipPointClampMinimum, baseUv + float2(-footprintOffset.x, -footprintOffset.y));
	rawGeometryDepth = min(rawGeometryDepth, geometryDepthTexture.Sample(linearMipPointClampMinimum, baseUv + float2(footprintOffset.x, -footprintOffset.y)));
	rawGeometryDepth = min(rawGeometryDepth, geometryDepthTexture.Sample(linearMipPointClampMinimum, baseUv + float2(-footprintOffset.x, footprintOffset.y)));
	rawGeometryDepth = min(rawGeometryDepth, geometryDepthTexture.Sample(linearMipPointClampMinimum, baseUv + float2(footprintOffset.x, footprintOffset.y)));
	float geometryDepth = LinearizeDepth(camera, rawGeometryDepth) * camera.farPlane;
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

#if defined(CLOUDS_DEBUG_MARCHCOUNT) || defined(CLOUDS_DEBUG_NORMALVECTOR)
	return RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture, origin, direction,
		jitter, marchStart, marchEnd, gapStart, gapEnd, sunDirection, wind, time, density, scatteredLuminance, transmittance, depth);
#else
	RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture, origin, direction, jitter,
		marchStart, marchEnd, gapStart, gapEnd, sunDirection, wind, time, density, scatteredLuminance, transmittance, depth);
#endif
}

#endif  // __CLOUDS_CORE_HLSLI__