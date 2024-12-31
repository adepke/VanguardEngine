// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Atmosphere/Atmosphere.hlsli"

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

	float geometryDepth = geometryDepthTexture[dispatchId.xy];
	geometryDepth = LinearizeDepth(camera, geometryDepth);
	geometryDepth *= camera.farPlane;
	float cloudsDepth = cloudsDepthTexture[dispatchId.xy] * 1000.f;  // Kilometers to meters.

	float2 uv = (dispatchId.xy + 0.5.xx) / float2(width, height);
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 rayDirection = ComputeRayDirection(camera, uv);
	float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	float3 planetCenter = ComputeAtmospherePlanetCenter(bindData.atmosphere);
	float shadowLength = 0.f;  // Light shafts are not yet supported.

	float3 perspectiveScattering = 0.xxx;
	float3 perspectiveTransmittance = 1.xxx;
	float3 skyScattering = 0.xxx;
	float3 skyTransmittance = 1.xxx;
	
	bool hitSurface = geometryDepth < camera.farPlane || cloudsDepth < 1000000;
	if (hitSurface)
	{
        float depth = min(geometryDepth, cloudsDepth);
		
		// If the geometry wasn't hit, then throw away the min result, as clouds can reach much further.
		if (geometryDepth >= camera.farPlane)
        {
            depth = cloudsDepth;
        }
		
		depth *= 0.001;  // Meters to kilometers.
		float3 position = cameraPosition + rayDirection * depth;

		float3 intersectPoint = position - planetCenter;
		float3 cameraPoint = cameraPosition - planetCenter;

		perspectiveScattering = GetSkyRadianceToPoint(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPoint, intersectPoint, shadowLength, sunDirection, perspectiveTransmittance);
    }

	// Transparent media can show sky behind them, so only check if we didn't hit opaque geometry.
	if (geometryDepth >= camera.farPlane)
	{
		skyScattering = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearWrap, cameraPosition - planetCenter, rayDirection, shadowLength, sunDirection, skyTransmittance);
		if (dot(rayDirection, sunDirection) > cos(sunAngularRadius))
		{
    		skyScattering += skyTransmittance * GetSolarRadiance(bindData.atmosphere);
  		}

  		float4 planetRadiance = GetPlanetSurfaceRadiance(bindData.atmosphere, planetCenter, cameraPosition, rayDirection, sunDirection, transmittanceLut, scatteringLut, irradianceLut, bilinearWrap);
		skyScattering = lerp(skyScattering, planetRadiance.xyz, planetRadiance.w);
    }

	// 10 is the default exposure used in Bruneton's demo.
	float exposure = 10;
	perspectiveScattering *= exposure;
	skyScattering *= exposure;

	float4 cloudsCombined = cloudsScatteringTransmittanceTexture[dispatchId.xy];  // scat=0, trans=1 when no data available
	float3 cloudsScattering = cloudsCombined.xyz;
	float3 cloudsTransmittance = cloudsCombined.www;

	// Composite the geometry with the volumetrics and the atmosphere.
	float3 inputColor = outputTexture[dispatchId.xy].xyz;
	float3 finalColor = inputColor * skyTransmittance + skyScattering;
	finalColor = finalColor * cloudsTransmittance + cloudsScattering;
	finalColor = finalColor * perspectiveTransmittance + perspectiveScattering;
	
	// Overlay cloud shadows.
	float radius = length(cameraPosition - planetCenter);
	float rMu = dot(cameraPosition - planetCenter, rayDirection);
	float mu = rMu / radius;
	bool rayHitsGround = RayIntersectsGround(bindData.atmosphere, radius, mu);  // Won't work if the camera is in space.
	if (rayHitsGround && !hitSurface)
	{
		float2 surfaceIntersect;
		RaySphereIntersection(cameraPosition, rayDirection, planetCenter, bindData.atmosphere.radiusBottom, surfaceIntersect);
		float3 hitPoint = cameraPosition + rayDirection * surfaceIntersect.x;

		Camera sunCamera = cameraBuffer[2];  // #TODO: Remove this terrible hardcoding.
		matrix sunViewProjection = mul(sunCamera.view, sunCamera.projection);
		float4 hitPointSunSpace = mul(float4(hitPoint, 1.0), sunViewProjection);
		float3 projectedCoords = hitPointSunSpace.xyz / hitPointSunSpace.w;
		projectedCoords = projectedCoords * 0.5 + 0.5;

		Texture2D<float> cloudsShadowMap = ResourceDescriptorHeap[bindData.cloudsShadowMap];
		float2 shadowMapSize;
		cloudsShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y);
		const float shadowTexelSize = 1.0 / shadowMapSize.x;

		// PCF-filtered shadow sampling.
		float shadow = 0;
		for (int x = -1; x <= 1; ++x)
		{
			for (int y = -1; y <= 1; ++y)
			{
				float shadowSample = cloudsShadowMap.Sample(downsampleBorder, projectedCoords.xy + float2(x, y) * shadowTexelSize);
				shadow += (shadowSample > 0 && shadowSample < 10000) ? 1 : 0;
			}
		}
		shadow /= 9.0;
		finalColor *= 1.0 / (1.0 + shadow * 0.4);
	}

	outputTexture[dispatchId.xy] = float4(finalColor, 1);
}