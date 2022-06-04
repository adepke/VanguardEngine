// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Constants.hlsli"
#include "Volumetrics/LightIntegration.hlsli"
#include "Volumetrics/PhaseFunctions.hlsli"

struct BindData
{
	uint baseShapeNoiseTexture;
	uint detailShapeNoiseTexture;
	uint cameraBuffer;
	uint cameraIndex;
	float solarZenithAngle;
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

float SampleBaseShape(Texture3D<float> noiseTexture, float3 position)
{
	const float frequency = 0.1f;
	return noiseTexture.Sample(bilinearWrap, position * frequency);
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
	
	float stratusGradient = sin(fraction * 30 - 1);  // Base shape
	stratusGradient *= 1 - smoothstep(0.15, 0.2, fraction + 0.09);  // Top mask
	stratusGradient *= smoothstep(0.65, 0.7, fraction + 0.63);  // Bottom mask

	float cumulusGradient = sin(fraction * 4.5);  // Base shape
	cumulusGradient *= 1 - smoothstep(0.4, 0.55, fraction - 0.06);  // Top mask

	float cumulonimbusGradient = smoothstep(0., 0.05, fraction);  // Bottom mask
	cumulonimbusGradient *= 1 - smoothstep(0.7, 1.0, fraction - 0.02);  // Top mask

	// Compose the final gradient based off cloud type.
	float gradient = 0.f;
	if (weather.y < 0.5)
	{
		gradient = lerp(stratusGradient, cumulusGradient, weather.y * 2.0);
	}
	else
	{
		gradient = lerp(cumulusGradient, cumulonimbusGradient, weather.y * 2.0 - 1.0);
	}

	// Weather sample is composed of cloud density and cloud type.
	return weather.x * gradient;
}

float SampleCloudDensity(Texture3D<float> baseNoise, Texture3D<float> detailNoise, float3 position, bool detailSample)
{
	// #TODO: Sample weather texture.
	float2 weather = float2(0.6, 0.5);  // Semi-dense cumulus.

	// #TODO: Coverage needs work.

	// Custom remapping here to shift the density curve.
	float coverage = 1.0 - (0.15 * weather.x);

	float baseShape = SampleBaseShape(baseNoise, position);
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

	return saturate(finalShape);  // #TEMP: Noise is sometimes negative... patchy fix.
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
	const float stepSize = 60.0 / 1000.0;  // 60m, seems like it should be pretty good.
	const int coneSamples = noiseKernelSize;
	float density = 0.0;

	// N-1 samples nearby, 1 far away to capture shadows cast by distant clouds.
	// See slide 85 of: https://www.guerrilla-games.com/media/News/Files/The-Real-time-Volumetric-Cloudscapes-of-Horizon-Zero-Dawn.pdf
	for (int i = 0; i < coneSamples; ++i)
	{
		float3 samplePosition = position + (stepSize * (float)i * noiseKernel[i]);
		// Once the density has reached 0.3, switch to low-detail noise. Refer to slide 86.
		const bool detailSamples = density < 0.3;
		density += SampleCloudDensity(baseNoise, detailNoise, samplePosition, detailSamples);
	}

	return density;
}

float ComputeBeersPowder(float value, float absorption, float viewDotLight)
{
	const float beers = exp(-value * absorption);  // Absorption increases for rain clouds.
	float powder = 1.0 - exp(-value * 2.0);
	powder = lerp(1, powder, saturate((viewDotLight * 0.5) + 0.5));  // Fix the output range.
	return beers * powder;
}

float ComputePhaseFunction(float nu)
{
	// Dual-lobe from Frostbite, better accounts for back scattering.
	float a = HenyeyGreensteinPhase(nu, -0.48);
	float b = HenyeyGreensteinPhase(nu, 0.75);
	return (a + b) / 2.0;
}

float ComputeLightEnergy(float3 position, float density, float viewDotLight)
{
	// Lighting model described in GPU Pro 7 page 119. Various changes made to proposed model.

	const float absorption = 1.5;  // #TODO: Comes from weather.

	float energy = ComputeBeersPowder(density, absorption, viewDotLight) * ComputePhaseFunction(viewDotLight);
	return energy * 2;  // #TEMP
}

void RayMarch(float3 origin, float3 direction, float3 sunDirection, out float3 scatteredLuminance, out float transmittance)
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
		float cloudDensity = SampleCloudDensity(baseShapeNoiseTexture, detailShapeNoiseTexture, position, detailSamples);

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
			float energy = ComputeLightEnergy(position, coneDensity, viewDotLight);
			float3 sunLuminance = float3(1.474f, 1.8504f, 1.91198f) * energy;  // #TODO: Get from atmosphere.
			float stepSize = smallStepSize * 1000.0;  // Kilometers to meters.

			float3 scattCoeff = 0.05.xxx;  // http://www.patarnott.com/satsens/pdf/opticalPropertiesCloudsReview.pdf
			float3 absorCoeff = 0.xxx;  // Cloud albedo ~= 1.
			float3 trans = transmittance.xxx;
			ComputeScatteringIntegration(cloudDensity, sunLuminance, stepSize, scattCoeff, absorCoeff, scatteredLuminance, trans);
			transmittance = trans.x;  // Scattering and absorbtion are uniform, so just use one channel.
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

	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 rayDirection = ComputeRayDirection(camera, input.uv);

	ComputeNoiseKernel(sunDirection);

	float3 scatteredLuminance;
	float transmittance;
	RayMarch(camera.position.xyz, rayDirection, sunDirection, scatteredLuminance, transmittance);

	float3 skyColor = 0.xxx;  // Applying as overlay, so no sky color.
	float4 output;
	output.rgb = scatteredLuminance + transmittance * skyColor;
	output.a = 1 - transmittance;

	return output;
}