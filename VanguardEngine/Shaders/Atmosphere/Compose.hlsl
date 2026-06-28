// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/Visibility.hlsli"

struct BindData
{
	uint cameraBuffer;
	uint cameraIndex;
	uint atmosphereBuffer;
	uint cloudsScatteringTransmittanceTexture;
	uint cloudsDepthTexture;
	uint cloudsVisibilityTexture;
	uint cloudsCirrusTexture;
	uint weatherTexture;
	uint geometryDepthTexture;
	uint outputTexture;
	uint transmissionTexture;
	uint scatteringTexture;
	uint irradianceTexture;
	float solarZenithAngle;
	float globalWeatherCoverage;
	float time;
	float2 wind;
};

ConstantBuffer<BindData> bindData : register(b0);

float3 SampleCirrusClouds(Texture2D<float4> cirrusTexture, float3 planetCenter, float3 cameraPosition, float3 rayDirection, out float3 hitPosition)
{
	// Cirrus clouds are in the range of 15k-30k feet. So pick a nice value in the middle.
	const float cirrusHeight = 6705.f / 1000.f;
	
	hitPosition = 0.xxx;
	
	const float planetRadius = 6360.0;
	float2 topBoundaryIntersect;
	if (!RaySphereIntersection(cameraPosition, rayDirection, planetCenter, planetRadius + cirrusHeight, topBoundaryIntersect))
	{
		// Outside the cirrus layer.
		return 0.xxx;
	}
	
	float distanceToLayer = topBoundaryIntersect.y;
	hitPosition = rayDirection * distanceToLayer + cameraPosition;
	
	// Convert the hit position to global space instead of in atmosphere-local space, otherwise the spherical
	// coordinates will not be correct. Note that the returned hit position is local space, so that the distance through
	// the atmosphere is correct.
	float3 hitGlobalSpace = hitPosition + planetCenter;
	
	// Convert the cartesian direction to normalized spherical coordinates.
	// Note that the X axis is used as the polar axis, instead of Z to focus spherical distortion towards the horizon,
	// instead of straight up.
	const float radius = length(hitGlobalSpace);
	const float theta = atan(hitGlobalSpace.y / hitGlobalSpace.z);
	const float phi = acos(hitGlobalSpace.x / radius);
	
	const float uvScale = 120.f;
	float2 uv = float2(-phi * uvScale, -theta * uvScale);
	
	// Apply wind by scrolling the UV coordinates. Wind tends to move faster as you get higher in the atmosphere,
	// so scale faster than the rest of the clouds.
	uv += bindData.wind * bindData.time * 0.038;
	
	float opacityScale = smoothstep(0.f, 0.4f, bindData.globalWeatherCoverage) * 0.35f;
	
	return cirrusTexture.Sample(bilinearWrap, uv).aaa * opacityScale;
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	StructuredBuffer<AtmosphereData> atmosphereBuffer = ResourceDescriptorHeap[bindData.atmosphereBuffer];
	Texture2D<float4> cloudsScatteringTransmittanceTexture = ResourceDescriptorHeap[bindData.cloudsScatteringTransmittanceTexture];
	Texture2D<float> cloudsDepthTexture = ResourceDescriptorHeap[bindData.cloudsDepthTexture];
	Texture2D<float2> cloudsVisibilityTexture = ResourceDescriptorHeap[bindData.cloudsVisibilityTexture];
	Texture2D<float4> cloudsCirrusTexture = ResourceDescriptorHeap[bindData.cloudsCirrusTexture];
	Texture2D<float3> weatherTexture = ResourceDescriptorHeap[bindData.weatherTexture];
	Texture2D<float> geometryDepthTexture = ResourceDescriptorHeap[bindData.geometryDepthTexture];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	Texture2D<float4> transmittanceLut = ResourceDescriptorHeap[bindData.transmissionTexture];
	Texture3D<float4> scatteringLut = ResourceDescriptorHeap[bindData.scatteringTexture];
	Texture2D<float4> irradianceLut = ResourceDescriptorHeap[bindData.irradianceTexture];

	uint width, height;
	outputTexture.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	Camera camera = cameraBuffer[bindData.cameraIndex];
	Camera sunCamera = cameraBuffer[2];  // #TODO: Remove this terrible hardcoding.
	
	AtmosphereData atmosphere = atmosphereBuffer[0];
	
	float2 uv = (dispatchId.xy + 0.5.xx) / float2(width, height);
	
	float geometryDepth = geometryDepthTexture[dispatchId.xy];
	geometryDepth = LinearizeDepth(camera, geometryDepth);
	geometryDepth *= camera.farPlane;
	float cloudsDepth = cloudsDepthTexture.Sample(bilinearClamp, uv) * 1000.f;  // Kilometers to meters.

	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 rayDirection = ComputeRayDirection(camera, uv);
	float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	float3 planetCenter = ComputeAtmospherePlanetCenter(atmosphere);
	
	// Atomspheric in-scatter is omitted along the ray in the bounds of [shadowStart, shadowStart + shadowLength].
	float shadowStart  = 0.f;
	float shadowLength = 0.f;

#if defined(RENDER_LIGHT_SHAFTS) && (RENDER_LIGHT_SHAFTS > 0)
	float2 visibilitySample = cloudsVisibilityTexture.Sample(bilinearClamp, uv);
	shadowStart  = visibilitySample.x;
	shadowLength = visibilitySample.y;

	// Soften the shadows a bit, except when looking at the sun. Shadows cast by clouds when looking at the sun
	// should be more dramatic to make the effect obvious.
	const float muS = dot(rayDirection, sunDirection);
	shadowLength *= smoothstep(0.85, 1.0, muS) + 1.0;

	// Hack the light shadows to fade in when the sun is at the horizon.
	float lightshaftFadeHack = smoothstep(0.01, 0.04, dot(normalize(cameraPosition - planetCenter), sunDirection));
	shadowLength = max(0.f, shadowLength * lightshaftFadeHack);
#endif
	
	// The following sequence is a correct composition of volumetrics and geometry. It is not optimized at all
	// however, so some of the work can likely be cut out.
	
	float3 finalColor = 0.xxx;
	float lastDepth = -1;  // The depth needs to be tracked to compose volumetrics.
	// Track whether the current endpoint is the actual planet surface vs. an object in the air. Drives
	// endpointIsGround passed to GetSkyRadianceToPoint to keep continuous geometry crossing the horizon
	// line in a single LUT half. See Atmosphere.hlsli::GetSkyRadianceToPoint.
	bool lastDepthIsGround = false;

	bool hitSurface = geometryDepth < camera.farPlane;
	if (hitSurface)
	{
		// Hit solid geometry, the direct lighting is already done in the forward pass.

		float depth = geometryDepth * 0.001;  // Meters to kilometers.
		lastDepth = depth;
		lastDepthIsGround = false;  // Can't hit planet surface if a mesh is in front.

		float3 inputColor = outputTexture[dispatchId.xy].xyz;
		finalColor = inputColor;
	}

	else
	{
		// Didn't hit any geometry, but could've hit the planet surface.

		float3 p = cameraPosition - planetCenter;
		float pDotRay = dot(p, rayDirection);
		float intersectionDistance = -pDotRay - sqrt(planetCenter.z * planetCenter.z - (dot(p, p) - (pDotRay * pDotRay)));

		if (intersectionDistance > 0.f)
		{
			// Hit the planet, compute the sun and sky light reflecting off, with the aerial perspective to that point.

			float3 hitPosition = cameraPosition + rayDirection * intersectionDistance;
			lastDepth = intersectionDistance;
			lastDepthIsGround = true;  // True planet surface hit.
			float3 surfaceNormal = normalize(hitPosition - planetCenter);
			
			float3 sunIrradiance;
			float3 skyIrradiance;
			GetSunAndSkyIrradiance(atmosphere, transmittanceLut, irradianceLut, bilinearWrap, hitPosition - planetCenter, surfaceNormal, sunDirection, sunIrradiance, skyIrradiance);
		
			// The irradiance on the planet surface is heavily influenced by visibility.
			float sunVisibility = CalculateSunVisibility(hitPosition, sunDirection, weatherTexture);
			float skyVisibility = CalculateSkyVisibility(hitPosition, bindData.globalWeatherCoverage);
			
			finalColor = atmosphere.surfaceColor * (1.f / pi) * ((sunIrradiance * sunVisibility) + (skyIrradiance * skyVisibility));
		}
		
		else
		{
			// Didn't hit the planet, use the cirrus cloud layer as the background. Since cirrus clouds are transparent,
			// compute the background aerial perspective first and composite on top.
			
			float3 hitPosition;
			float3 cirrusColor = SampleCirrusClouds(cloudsCirrusTexture, planetCenter, cameraPosition, rayDirection, hitPosition);
			lastDepth = length(hitPosition) - 0.00001;  // Subtract a small number so that no hit corresponds with a negative depth.
			lastDepthIsGround = false;
			
			// Start at the cirrus layer and end in space. Note that there's no shadow above the cirrus clouds, so
			// shadow segment is (0, 0).
			float3 perspectiveTransmittance;
			float3 perspectiveScattering = GetSkyRadiance(atmosphere, transmittanceLut, scatteringLut, bilinearWrap, hitPosition - planetCenter, rayDirection, 0.f, 0.f, sunDirection, perspectiveTransmittance);
			
			// Branchless version of: if lastDepth < 0: perspectiveScattering = 0
			// Necessary so that negative depth (no hit) doesn't contribute any aerial perspective.
			perspectiveScattering = max(min(lastDepth * 10.xxx, perspectiveScattering), 0.xxx);
			
			// Using the sun direction as the normal vector causes artifacts when the sun is setting.
			float3 surfaceNormal = float3(0, 0, 1);
			
			float3 sunIrradiance;
			float3 skyIrradiance;
			GetSunAndSkyIrradiance(atmosphere, transmittanceLut, irradianceLut, bilinearWrap, hitPosition - planetCenter, surfaceNormal, sunDirection, sunIrradiance, skyIrradiance);
			
			// The irradiance on the cirrus clouds is not impacted by visibility, so skip that computation.
			cirrusColor *= (sunIrradiance + skyIrradiance);
			
			// Composite. No need to apply transmittance, the prior color is empty.
			finalColor = cirrusColor + perspectiveScattering;
			
			// If the view ray intersects the sun disk, add the direct radiance of the sun on top.
			if (dot(rayDirection, sunDirection) > cos(sunAngularRadius))
			{
				float sunVisibility = 1.f;
				
				// Instead of using CalculateSunVisibility as an approximation for the sun visibility, we can be much more accurate
				// here by just sampling the cloud transmittance map directly. We can do this here since this is a screen space rendering
				// of the sun disk.
				if (cloudsDepth < 1000000)
				{
					float4 cloudsCombined = cloudsScatteringTransmittanceTexture.Sample(bilinearClamp, uv);  // scat=0, trans=1 when no data available
					
					// Hack since for some reason fully occluding clouds are not 0% transmittance. This ensures that when
					// the sun is hidden behind thick clouds, absolutely no direct sun is visible.
					sunVisibility = max(cloudsCombined.w - 0.01f, 0.f);
				}
				
				// Intentionally blow away prior work, the sun is so bright it doesn't matter what came before.
				finalColor = GetSolarRadiance(atmosphere) * sunVisibility;
			}
		}
	}

	// After solid geometry has been rendered as a background, compose volumetrics on top.
	if (cloudsDepth < 1000000 && (!hitSurface || geometryDepth > cloudsDepth))
	{
		// Hit a cloud, need to apply the in-scattered light of the media over material behind it.
	
		float4 cloudsCombined = cloudsScatteringTransmittanceTexture.Sample(bilinearClamp, uv);  // scat=0, trans=1 when no data available
		float3 cloudsScattering = cloudsCombined.xyz;
		float3 cloudsTransmittance = cloudsCombined.www;
		
		// Don't let the depth be 0, as this will cause a NaN in the aerial perspective equation.
		float depth = max(cloudsDepth, 0.01) * 0.001;  // Meters to kilometers.
		float3 backPosition = cameraPosition + rayDirection * lastDepth;
		float3 cloudPosition = cameraPosition + rayDirection * depth;
		lastDepth = depth;

		// Debug rendering should not have aerial perspective applied.
#if defined(CLOUDS_DEBUG_MARCHCOUNT) || defined(CLOUDS_DEBUG_NORMALVECTOR)
		finalColor = finalColor * cloudsTransmittance + cloudsScattering;
#elif defined(CLOUDS_DEBUG_TRANSMITTANCE)
		finalColor = cloudsTransmittance;
#else
		// Compute the aerial perspective between the last depth position behind the cloud, and the cloud itself.
		// Shadow segment is (0, 0): we don't model cloud-cast shadow within the cloud-to-back-endpoint segment
		// because the cloud itself is the dominant occluder there.
		float3 perspectiveTransmittance;
		float3 perspectiveScattering = GetSkyRadianceToPoint(atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cloudPosition - planetCenter, backPosition - planetCenter, lastDepthIsGround, 0.f, 0.f, sunDirection, perspectiveTransmittance);

		// Composite.
		finalColor = finalColor * perspectiveTransmittance + perspectiveScattering;
		finalColor = finalColor * cloudsTransmittance + cloudsScattering;
#endif
		
		// Clouds aren't a surface hit, reset for next composition stage.
		// Note this is after the radiance computation since the previous hit was important.
		lastDepthIsGround = false;
	}
	
	// Done composing intermediary volumetrics, now apply final aerial perspective on top.
	
	float3 perspectiveScattering;
	float3 perspectiveTransmittance;
	
	if (lastDepth >= 0.f)
	{
		float3 hitPosition = cameraPosition + rayDirection * lastDepth;

		perspectiveScattering = GetSkyRadianceToPoint(atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPosition - planetCenter, hitPosition - planetCenter, lastDepthIsGround, shadowStart, shadowLength, sunDirection, perspectiveTransmittance);
	}

	else
	{
		perspectiveScattering = GetSkyRadiance(atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPosition - planetCenter, rayDirection, shadowStart, shadowLength, sunDirection, perspectiveTransmittance);
	}
	
	finalColor = finalColor * perspectiveTransmittance + perspectiveScattering;
	
	outputTexture[dispatchId.xy] = float4(finalColor, 1);
}