// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"

struct BindData
{
	uint inputBuffer;
	uint outputBuffer;
	uint objectBuffer;
	uint cameraBuffer;
	uint cameraIndex;
	uint drawCount;
};

ConstantBuffer<BindData> bindData : register(b0);

struct MeshIndirectArgument
{
	uint batchId;
	uint indexCountPerInstance;
	uint instanceCount;
	uint startIndexLocation;
	int baseVertexLocation;
	uint startInstanceLocation;
	float2 padding;
};

float4 normalizePlane(float4 p)
{
	return p / length(p.xyz);
}

bool IsVisible(ObjectData object, Camera camera)
{
	float3 center = float3(object.worldMatrix._m30, object.worldMatrix._m31, object.worldMatrix._m32);
	float radius = object.boundingSphereRadius * 10.f;

	// Project sphere into view space.
	center = mul(float4(center, 1.f), camera.view).xyz;
	center.z = -center.z;

	matrix projectionTranspose = camera.projection;//transpose(camera.projection);
	float4 r0 = float4(projectionTranspose._m00, projectionTranspose._m01, projectionTranspose._m02, projectionTranspose._m03);
	float4 r1 = float4(projectionTranspose._m10, projectionTranspose._m11, projectionTranspose._m12, projectionTranspose._m13);
	float4 r3 = float4(projectionTranspose._m30, projectionTranspose._m31, projectionTranspose._m32, projectionTranspose._m33);
	float4 frustumX = normalizePlane(r3 + r0);
	float4 frustumY = normalizePlane(r3 + r1);

	bool visible = true;

	// Frustum culling. Credit: https://github.com/zeux/niagara/blob/master/src/shaders/drawcull.comp.glsl
	visible = visible && center.z * frustumX.z - abs(center.x) * frustumX.x > -radius;  // Left/right planes.
	visible = visible && center.z * frustumY.z - abs(center.y) * frustumY.y > -radius;  // Top/bottom planes.
	visible = visible && center.z + radius > camera.nearPlane && center.z - radius < camera.farPlane;  // Near/far planes.

	return visible;
}

[RootSignature(RS)]
[numthreads(64, 1, 1)]
void Main(uint dispatchId : SV_DispatchThreadID)
{
	StructuredBuffer<MeshIndirectArgument> inputBuffer = ResourceDescriptorHeap[bindData.inputBuffer];
	AppendStructuredBuffer<MeshIndirectArgument> outputBuffer = ResourceDescriptorHeap[bindData.outputBuffer];
	StructuredBuffer<ObjectData> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	uint index = dispatchId.x;
	if (index < bindData.drawCount)
	{
		uint objectId = inputBuffer[index].batchId;  // #TEMP: Sufficient for now, won't work with instancing.
		ObjectData object = objectBuffer[objectId];
		if (IsVisible(object, camera))
		{
			outputBuffer.Append(inputBuffer[index]);
		}
	}
}