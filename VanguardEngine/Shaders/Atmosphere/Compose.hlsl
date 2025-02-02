// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/ShadowVolume.hlsli"
#include "Atmosphere/Visibility.hlsli"

struct BindData
{
	AtmosphereData atmosphere;
	uint cameraBuffer;
	uint cameraIndex;
	uint cloudsScatteringTransmittanceTexture;
	uint cloudsDepthTexture;
	uint cloudsShadowMap;
	uint geometryDepthTexture;
	uint outputTexture;
	uint transmissionTexture;
	uint scatteringTexture;
	uint irradianceTexture;
	float solarZenithAngle;
    float globalWeatherCoverage;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Texture2D<float4> cloudsScatteringTransmittanceTexture = ResourceDescriptorHeap[bindData.cloudsScatteringTransmittanceTexture];
	Texture2D<float> cloudsDepthTexture = ResourceDescriptorHeap[bindData.cloudsDepthTexture];
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

	float geometryDepth = geometryDepthTexture[dispatchId.xy];
	geometryDepth = LinearizeDepth(camera, geometryDepth);
	geometryDepth *= camera.farPlane;
	float cloudsDepth = cloudsDepthTexture[dispatchId.xy] * 1000.f;  // Kilometers to meters.

	float2 uv = (dispatchId.xy + 0.5.xx) / float2(width, height);
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 rayDirection = ComputeRayDirection(camera, uv);
	float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	float3 planetCenter = ComputeAtmospherePlanetCenter(bindData.atmosphere);
	
    float shadowLength = 0.f;
	
#if defined(RENDER_LIGHT_SHAFTS) && (RENDER_LIGHT_SHAFTS > 0)
	// Ray march through the cloud shadow volume to compute the length of the ray in shadow, in kilometers.
    float2 marchedShadowVolume = RayMarchCloudShadowVolume(camera, uv, sunCamera, ResourceDescriptorHeap[bindData.cloudsShadowMap]);
	
	// #TODO: Extrapolate the shadow length by taking the shadow length / total length, then multiply by ray length.
	shadowLength = marchedShadowVolume.y;
	
	// #TODO: Apply shadow fadein hack here.
	// ...
#endif
	
	// The following sequence is a correct composition of volumetrics and geometry. It is not optimized at all
	// however, so some of the work can likely be cut out.
	
    float3 finalColor;
    finalColor = 0.xxx;
	
    bool hitSurface = geometryDepth < camera.farPlane;
    if (hitSurface)
    {
		// Hit solid geometry, the direct lighting is already done in the forward pass.
		// Just need to apply aerial perspective here.
		
        float depth = geometryDepth * 0.001;  // Meters to kilometers.
        float3 hitPosition = cameraPosition + rayDirection * depth;
        float3 cameraPoint = cameraPosition - planetCenter;
		
        float3 perspectiveTransmittance;
        float3 perspectiveScattering = GetSkyRadianceToPoint(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPoint, hitPosition - planetCenter, shadowLength, sunDirection, perspectiveTransmittance);
	
        float3 inputColor = outputTexture[dispatchId.xy].xyz;
        finalColor = inputColor * perspectiveTransmittance + perspectiveScattering;
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
			
            float3 surfacePoint = cameraPosition + rayDirection * intersectionDistance;
            float3 surfaceNormal = normalize(surfacePoint - planetCenter);
            float3 cameraPoint = cameraPosition - planetCenter;
		
            float3 sunIrradiance;
            float3 skyIrradiance;
            GetSunAndSkyIrradiance(bindData.atmosphere, transmittanceLut, irradianceLut, bilinearWrap, surfacePoint - planetCenter, surfaceNormal, sunDirection, sunIrradiance, skyIrradiance);
		
            float sunVisibility = CalculateSunVisibility(surfacePoint, sunCamera, ResourceDescriptorHeap[bindData.cloudsShadowMap]);
            float skyVisibility = CalculateSkyVisibility(surfacePoint, bindData.globalWeatherCoverage);
			
            float3 radiance = bindData.atmosphere.surfaceColor * (1.f / pi) * ((sunIrradiance * sunVisibility) + (skyIrradiance * skyVisibility));
			
			// Last, apply aerial perspective to the planet surface.
            float3 perspectiveTransmittance;
            float3 perspectiveScattering = GetSkyRadianceToPoint(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPoint, surfacePoint - planetCenter, shadowLength, sunDirection, perspectiveTransmittance);
			
            float3 inputColor = radiance;
            finalColor = inputColor * perspectiveTransmittance + perspectiveScattering;
			
			// Exposure.
            finalColor *= atmosphereRadianceExposure;
        }
		
		else
        {
			// Didn't hit the planet, use the color of the sky as the base color. The sun's direct lighting also is factored in.
			
            float3 skyTransmittance;
            float3 skyScattering = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPosition - planetCenter, rayDirection, shadowLength, sunDirection, skyTransmittance);
            
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
				
                skyScattering += skyTransmittance * GetSolarRadiance(bindData.atmosphere) * sunVisibility;
            }
			
            finalColor = skyScattering;
			
			// Exposure.
            finalColor *= atmosphereRadianceExposure;
        }
    }
	
	// Solid geometry and the background atmosphere have been composed together. Now, render the volumetric clouds on top.
    if (cloudsDepth < 1000000)
    {
		// Hit a cloud, need to apply the in-scattered light of the media over material behind it, and add additional aerial perspective to the media.
		
        float4 cloudsCombined = cloudsScatteringTransmittanceTexture.Sample(bilinearClamp, uv);  // scat=0, trans=1 when no data available
		
        float3 cloudsScattering = cloudsCombined.xyz;
        float3 cloudsTransmittance = cloudsCombined.www;
		
        float depth = cloudsDepth * 0.001; // Meters to kilometers.
        float3 hitPosition = cameraPosition + rayDirection * depth;
        float3 cameraPoint = cameraPosition - planetCenter;
		
		// Aerial perspective.
        float3 perspectiveTransmittance;
        float3 perspectiveScattering = GetSkyRadianceToPoint(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPoint, hitPosition - planetCenter, shadowLength, sunDirection, perspectiveTransmittance);
		
		// Exposure, but not to clouds, only to the aerial scattering! This is because the cloud light energy already factors in exposure.
        perspectiveScattering *= atmosphereRadianceExposure;
        
		// finalColor now contains the background color, composite on top of it.
        finalColor = finalColor * cloudsTransmittance + cloudsScattering;
        finalColor = finalColor * perspectiveTransmittance + perspectiveScattering;
    }

#if defined(CLOUDS_RENDER_SHADOWMAP) && (CLOUDS_RENDER_SHADOWMAP > 0)
	// Explicitly drawing the cloud shadow map can be helpful for debugging.
	
	float radius = length(cameraPosition - planetCenter);
	float rMu = dot(cameraPosition - planetCenter, rayDirection);
	float mu = rMu / radius;
	bool rayHitsGround = RayIntersectsGround(bindData.atmosphere, radius, mu);  // Won't work if the camera is in space.
	if (rayHitsGround && !hitSurface)
    {
        float2 surfaceIntersect;
        RaySphereIntersection(cameraPosition, rayDirection, planetCenter, bindData.atmosphere.radiusBottom, surfaceIntersect);
        float3 hitPoint = cameraPosition + rayDirection * surfaceIntersect.x;

        matrix sunViewProjection = mul(sunCamera.view, sunCamera.projection);
        float4 hitPointSunSpace = mul(float4(hitPoint, 1.0), sunViewProjection);
        float3 projectedCoords = hitPointSunSpace.xyz / hitPointSunSpace.w;
        projectedCoords = projectedCoords * 0.5 + 0.5;

        Texture2D<float> cloudsShadowMap = ResourceDescriptorHeap[bindData.cloudsShadowMap];

        float shadowSample = cloudsShadowMap.Sample(downsampleBorder, projectedCoords.xy);
        float shadow = (shadowSample > 0 && shadowSample < 10000) ? 1 : 0;
		
        finalColor *= 1.0 / (1.0 + shadow * 0.4);
    }
#endif

	outputTexture[dispatchId.xy] = float4(finalColor, 1);
}