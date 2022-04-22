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

bool IsVisible(ObjectData object, Camera camera)
{
	float3 center = float3(object.worldMatrix._m30, object.worldMatrix._m31, object.worldMatrix._m32);
	float radius = object.boundingSphereRadius * 4.f;  // #TODO: Something weird with bounding sphere size...

	// Project sphere into view space.
	float4 viewSpace = mul(float4(center, 1.f), camera.view);
	center = (viewSpace / viewSpace.w).xyz;  // Perspective division.

	matrix projectionTranspose = transpose(camera.projection);
	float4 r0 = float4(projectionTranspose._m00, projectionTranspose._m01, projectionTranspose._m02, projectionTranspose._m03);
	float4 r1 = float4(projectionTranspose._m10, projectionTranspose._m11, projectionTranspose._m12, projectionTranspose._m13);
	float4 r2 = float4(projectionTranspose._m20, projectionTranspose._m21, projectionTranspose._m22, projectionTranspose._m23);
	float4 r3 = float4(projectionTranspose._m30, projectionTranspose._m31, projectionTranspose._m32, projectionTranspose._m33);

	// https://fgiesen.wordpress.com/2012/08/31/frustum-planes-from-the-projection-matrix/
	// Frustum plane inequalities:
	// -w <= x <= w
	// -w <= y <= w
	// 0 <= z <= w
	// #TODO: More efficient frustum culling: https://github.com/zeux/niagara/blob/master/src/shaders/drawcull.comp.glsl

	bool visible = true;
	visible = visible && -radius <= mul(center, r3 + r0);  // Left plane
	visible = visible && -radius <= mul(center, r3 - r0);  // Right plane
	visible = visible && -radius <= mul(center, r3 + r1);  // Bottom plane
	visible = visible && -radius <= mul(center, r3 - r1);  // Top plane
	visible = visible && -radius <= mul(center, r3);  // Near plane
	visible = visible && -radius <= mul(center, r3 - r2);  // Far plane

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
		uint objectId = inputBuffer[index].batchId;  // #TODO: Sufficient for now, won't work with instancing.
		ObjectData object = objectBuffer[objectId];
		if (IsVisible(object, camera))
		{
			outputBuffer.Append(inputBuffer[index]);
		}
	}
}