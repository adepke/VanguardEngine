// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Constants.hlsli"
#include "Reprojection.hlsli"
#include "Volumetrics/LightIntegration.hlsli"
#include "Volumetrics/PhaseFunctions.hlsli"

struct BindData
{
	uint baseShapeNoiseTexture;
	uint detailShapeNoiseTexture;
	uint cameraBuffer;
	uint cameraIndex;
	float solarZenithAngle;
	uint timeSlice;
	float2 outputResolution;
	uint lastFrameTexture;
	uint depthTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VertexIn
{
	uint vertexId : SV_VertexID;
};

struct PixelIn
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

[RootSignature(RS)]
PixelIn VSMain(VertexIn input)
{
	PixelIn output;
	output.uv = float2((input.vertexId << 1) & 2, input.vertexId & 2);
	output.positionCS = float4((output.uv.x - 0.5) * 2.0, -(output.uv.y - 0.5) * 2.0, 0, 1);  // Z of 0 due to the inverse depth.
	
	return output;
}

// Kilometers.
static const float cloudLayerBottom = 1500.0 / 1000.0;
static const float cloudLayerTop = 3500.0 / 1000.0;

float RemapRange(float value, float inMin, float inMax, float outMin, float outMax)
{
	return outMin + (((value - inMin) / (inMax - inMin)) * (outMax - outMin));
}

float SampleBaseShape(Texture3D<float> noiseTexture, float3 position, uint mip)
{
	const float frequency = 0.1f;
	return noiseTexture.SampleLevel(bilinearWrap, position * frequency, mip);
}

float SampleDetailShape(Texture3D<float> noiseTexture, float3 position)
{
	const float frequency = 5.2;
	return noiseTexture.Sample(bilinearWrap, position * frequency);
}

float GetHeightFractionForPoint(float3 position, float2 cloudMinMax)
{
	float heightFraction = (position.z - cloudMinMax.x) / (cloudMinMax.y - cloudMinMax.x);
	return saturate(heightFraction);
}

float GetDensityHeightGradientForPoint(float3 position, float2 weather)
{
	const float fraction = GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop));

	// Cloud type: 0.0=stratus, 0.5=cumulus, 1.0=cumulonimbus
	
	float a = 0.1;
	float b = 0.3;
	float c = 0.5;
	float gradient = saturate(RemapRange(fraction, 0, a, 0, 1)) * saturate(RemapRange(fraction, b, c, 1, 0));

	// Weather sample is composed of cloud density and cloud type.
	return weather.x * gradient;
}

float SampleCloudDensity(Texture3D<float> baseNoise, Texture3D<float> detailNoise, float3 position, bool detailSample, uint mip)
{
	// #TODO: Sample weather texture.
	float2 weather = float2(0.6, 0.5);  // Semi-dense cumulus.

	// #TODO: Coverage needs work.

	// Custom remapping here to shift the density curve.
	float coverage = 1.0 - (0.15 * weather.x);

	float baseShape = SampleBaseShape(baseNoise, position, mip);
	baseShape = RemapRange(baseShape, 0.9, 1.0, 0.0, 1.0);
	float densityHeightGradient = GetDensityHeightGradientForPoint(position, weather);

	float finalShape = baseShape * densityHeightGradient;  // Apply the gradient early to potentially early-out of the detail sample.
	finalShape = RemapRange(finalShape, weather.x * 0.05, 1.0, 0.0, 1.0);
	finalShape *= coverage;  // Improve smaller cloud appearence.

	// Added coverage check since the remap breaks if it's zero. Note that this case only happens during cone
	// sampling, since the base shape acts as a convex hull and cannot be zero when the detail wouldn't be normally.
	if (detailSample && finalShape > 0.0)
	{
		float detailShape = SampleDetailShape(detailNoise, position);

		// Gradient from wispy to billowy shapes by height.
		float heightFraction = GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop));
		detailShape = lerp(detailShape, 1.0 - detailShape, saturate(heightFraction * 10.0));

		// Erode the final shape.
		finalShape = RemapRange(finalShape, detailShape * 0.2, 1.0, 0.0, 1.0);
	}

	return max(finalShape, 0);  // #TEMP: shouldnt need the max here.
}

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
	for (int i = 0; i < noiseKernelSize; ++i)
	{
		const float3 vec = normalize(noise[i]);
		const float3 rotationAxis = cross(vec, lightDirection);
		const float theta = acos(dot(vec, lightDirection));
		noiseKernel[i] = vec * cos(theta) + cross(rotationAxis, vec) * sin(theta) + rotationAxis * dot(rotationAxis, vec) * (1.0 - cos(theta));
	}

	noiseKernel[noiseKernelSize - 1] *= 3;  // Long-distance sample.
}

float SampleCloudDensityCone(Texture3D<float> baseNoise, Texture3D<float> detailNoise, float3 position)
{
	const float stepSize = 70.0 / 1000.0;  // 70m.
	const int coneSamples = noiseKernelSize;
	float density = 0.0;

	// N-1 samples nearby, 1 far away to capture shadows cast by distant clouds.
	// See slide 85 of: https://www.guerrilla-games.com/media/News/Files/The-Real-time-Volumetric-Cloudscapes-of-Horizon-Zero-Dawn.pdf
	for (int i = 0; i < coneSamples; ++i)
	{
		float3 samplePosition = position + (stepSize * (float)i * noiseKernel[i]);

		// Cone sample left the cloud layer, bail out. Need to check here since math breaks in SampleCloudDensity if sampling out of bounds.
		float heightFraction = GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop));
		if (heightFraction > 1.f)
			break;

		// Once the density has reached 0.3, switch to low-detail noise. Refer to slide 86.
		const bool detailSamples = density < 0.3;
		density += SampleCloudDensity(baseNoise, detailNoise, samplePosition, detailSamples, 0);
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
	float a = HenyeyGreensteinPhase(nu, -0.48);
	float b = HenyeyGreensteinPhase(nu, 0.75);
	return (a + b) / 2.0;
}

float ComputeInScatterProbability(float localDensity, float heightFraction)
{
	const float depthProbability = 0.05 + pow(localDensity, max(RemapRange(heightFraction, 0.3, 0.85, 0.5, 2.0), 0.001));
	const float verticalProbability = pow(saturate(0.07 + RemapRange(heightFraction, 0.07, 0.14, 0.6, 1.0)), 0.8);

	return depthProbability * verticalProbability;
}

float ComputeLightEnergy(Texture3D<float> baseNoise, Texture3D<float> detailNoise, float3 position, float densityToLight, float viewDotLight)
{
	// Lighting model inspired by GPU Pro 7 page 119, Frostbite, and Nubis 2017 real-time volumetric cloudscapes.

	const float absorption = 1.5;  // #TODO: Comes from weather.

	// Sample the mean density at the sample position using a higher mip level.
	// #TODO: experiment with accounting for coverage, height gradient, etc, since these affect the density.
	float localDensity = saturate(RemapRange(SampleBaseShape(baseNoise, position, 1), 0.89, 1, 0, 1));

	const float outScatter = ComputeBeersLaw(densityToLight, absorption);
	const float phase = ComputePhaseFunction(viewDotLight);
	const float inScatter = ComputeInScatterProbability(localDensity, GetHeightFractionForPoint(position, float2(cloudLayerBottom, cloudLayerTop)));

	return outScatter * phase * inScatter;
}

void RayMarch(float3 origin, float3 direction, float3 sunDirection, out float3 scatteredLuminance, out float transmittance, out float depth)
{
	Texture3D<float> baseShapeNoiseTexture = ResourceDescriptorHeap[bindData.baseShapeNoiseTexture];
	Texture3D<float> detailShapeNoiseTexture = ResourceDescriptorHeap[bindData.detailShapeNoiseTexture];

	const float zDot = abs(dot(direction, float3(0, 0, 1)));
	const float theta = atan(zDot);
	const float sinTheta = sin(theta);
	const float viewDotLight = dot(direction, sunDirection);

	float dist = 0.f;
	int detailSteps = 0;  // If >0, march in small steps.

	float marchStart;
	float marchEnd;

	scatteredLuminance = 0.xxx;
	transmittance = 1;

	origin *= 1.0 / 1000.0;  // Meters to kilometers.
	const float planetRadius = 6360.0;
	float3 planetCenter = float3(0, 0, -(planetRadius + 1));  // Offset since the world origin is 1km off the planet surface.

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
		return;
	}

	marchStart = max(0, marchStart);
	marchEnd = max(0, marchEnd);

	if (marchEnd <= marchStart)
	{
		return;
	}

	origin = origin + direction * marchStart;

	const int steps = (128 - 64 * zDot);  // 64 at zenith, 128 at horizon.
	const float marchWidth = marchEnd - marchStart;
	const float smallStepMultiplier = 0.2;
	// Dynamic step size causes artifacts when flying through the cloud layer, since the march width varies so much
	// depending on if the ray hits the bottom layer or the top layer.
	//const float largeStepSize = marchWidth / (float)steps;
	const float largeStepSize = 0.08;
	const float smallStepSize = largeStepSize * smallStepMultiplier;
	const int stepTransitionMargin = 6;

	//for (int i = 0; i < steps; ++i)
	for (int i = 0; dist < marchWidth; ++i)  // #TEMP: Reference impl marching to end of cloud layer. Far too expensive.
	{
		if (dist > marchEnd)
			break;  // Left the cloud layer.

		float3 position = origin + direction * dist;

		const bool detailSamples = detailSteps > 0;
		float cloudDensity = SampleCloudDensity(baseShapeNoiseTexture, detailShapeNoiseTexture, position, detailSamples, 0);

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

			float coneDensity = SampleCloudDensityCone(baseShapeNoiseTexture, detailShapeNoiseTexture, position);
			coneDensity = (coneDensity + cloudDensity) / float(noiseKernelSize + 1);
			float energy = ComputeLightEnergy(baseShapeNoiseTexture, detailShapeNoiseTexture, position, coneDensity, viewDotLight);
			energy *= 4.0;  // Model loses a bit too much energy, so artificially bring it back here.
			float3 sunLuminance = float3(1.474f, 1.8504f, 1.91198f) * energy;  // #TODO: Get from atmosphere.
			float stepSize = smallStepSize * 1000.0;  // Kilometers to meters.

			float3 scattCoeff = 0.05.xxx;  // http://www.patarnott.com/satsens/pdf/opticalPropertiesCloudsReview.pdf
			float3 absorCoeff = 0.xxx;  // Cloud albedo ~= 1.
			float3 trans = transmittance.xxx;
			ComputeScatteringIntegration(cloudDensity, sunLuminance, stepSize, scattCoeff, absorCoeff, scatteredLuminance, trans);
			transmittance = trans.x;  // Scattering and absorbtion are uniform, so just use one channel.

			// Update the depth every time we hit a detail sample. Decent approximation.
			// May get better results using the first detail sample distance.
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
}

[RootSignature(RS)]
float4 PSMain(PixelIn input) : SV_Target
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	static const uint crossFilter[] = {
		0, 8, 2, 10,
		12, 4, 14, 6,
		3, 11, 1, 9,
		15, 7, 13, 5
	};

	int2 pixel = input.uv * bindData.outputResolution;
	int index = (pixel.x + 4 * pixel.y) % 16;
	if (index == crossFilter[bindData.timeSlice])
	{
		float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
		float3 rayDirection = ComputeRayDirection(camera, input.uv);

		ComputeNoiseKernel(sunDirection);

		float3 scatteredLuminance;
		float transmittance;
		float depth;
		RayMarch(camera.position.xyz, rayDirection, sunDirection, scatteredLuminance, transmittance, depth);

		RWTexture2D<float> depthTexture = ResourceDescriptorHeap[bindData.depthTexture];
		depthTexture[input.uv] = depth;

		float3 skyColor = 0.xxx;  // Applying as overlay, so no sky color.
		float4 output;
		output.rgb = scatteredLuminance + transmittance * skyColor;
		output.a = 1 - transmittance;

		return output;
	}

	else
	{
		// Not rendering this pixel this frame, so reproject instead.
		Texture2D<float4> lastFrameTexture = ResourceDescriptorHeap[bindData.lastFrameTexture];
		RWTexture2D<float> depthTexture = ResourceDescriptorHeap[bindData.depthTexture];

		float depth = depthTexture[input.uv];
		depth = 1;  // #TEMP
		float2 reprojectedUv = ReprojectUv(camera, input.uv, depth);

		float4 lastFrame = 0.xxxx;
		if (bindData.lastFrameTexture != 0)
		{
			lastFrame = lastFrameTexture.Sample(linearMipPointTransparentBorder, reprojectedUv);
		}

		return lastFrame;
	}
}