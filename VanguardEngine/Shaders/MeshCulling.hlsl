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
	uint cullingLevel;
	uint hiZTexture;
	uint hiZMipLevels;
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

// Credit: 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere
bool ProjectSphere(float3 center, float radius, Camera camera, out float4 aabb)
{
	// Near plane culling.
	if (center.z < camera.nearPlane + radius)
		return false;

	float2 cx = -center.xz;
	float2 vx = float2(sqrt(dot(cx, cx) - radius * radius), radius);
	float2 minx = mul(cx, float2x2(vx.x, vx.y, -vx.y, vx.x));
	float2 maxx = mul(cx, float2x2(vx.x, -vx.y, vx.y, vx.x));

	float2 cy = -center.yz;
	float2 vy = float2(sqrt(dot(cy, cy) - radius * radius), radius);
	float2 miny = mul(cy, float2x2(vy.x, vy.y, -vy.y, vy.x));
	float2 maxy = mul(cy, float2x2(vy.x, -vy.y, vy.y, vy.x));

	float p00 = camera.lastFrameProjection._m00;
	float p11 = camera.lastFrameProjection._m11;
	aabb = float4(minx.x / minx.y * p00, miny.x / miny.y * p11, maxx.x / maxx.y * p00, maxy.x / maxy.y * p11);
	aabb = aabb.xwzy * float4(0.5, -0.5, 0.5, -0.5) + 0.5.xxxx;

	return true;
}

bool IsVisible(ObjectData object, Camera camera)
{
	float3 center = float3(object.worldMatrix._m30, object.worldMatrix._m31, object.worldMatrix._m32);
	float radius = object.boundingSphereRadius * 4.f;  // #TODO: Something weird with bounding sphere size...

	// Project sphere into view space.
	float4 viewSpace = mul(float4(center, 1.f), camera.view);
	center = (viewSpace / viewSpace.w).xyz;  // Perspective division.

	matrix projectionTranspose = transpose(camera.lastFrameProjection);
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

	if (visible && bindData.cullingLevel > 1)
	{
		// Convention here is +Z going outwards from camera.
		center.z *= -1;
		radius *= 0.25;  // Revert the weird 4x scaling above.

		float4 aabb;
		if (ProjectSphere(center, radius, camera, aabb))
		{
			Texture2D<float> hiZTexture = ResourceDescriptorHeap[bindData.hiZTexture];
			uint width, height, mipCount;
			hiZTexture.GetDimensions(0, width, height, mipCount);
			mipCount = min(mipCount, bindData.hiZMipLevels) - 1;

			float projectedWidth = (aabb.z - aabb.x) * width;
			float projectedHeight = (aabb.w - aabb.y) * height;

			float level = min(floor(log2(max(projectedWidth, projectedHeight))), mipCount);
			float2 uv = (aabb.xy + aabb.zw) * 0.5;
			float depth = hiZTexture.SampleLevel(linearMipPointClampMinimum, uv, level);
			float depthSphere = camera.nearPlane / (center.z - radius);

			visible = visible && depthSphere >= depth;  // Inverse Z.
		}
	}

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