// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ShaderStructs.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Asset/MeshGeometryUtils.h>

inline XMMATRIX BuildObjectWorldMatrix(const TransformComponent& transform)
{
	const auto scaling = XMVectorSet(transform.scale.x, transform.scale.y, transform.scale.z, 0.f);
	const auto translation = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);

	const auto scalingMat = XMMatrixScalingFromVector(scaling);
	const auto rotationMat = XMMatrixRotationX(-transform.rotation.x) * XMMatrixRotationY(-transform.rotation.y) * XMMatrixRotationZ(-transform.rotation.z);
	const auto translationMat = XMMatrixTranslationFromVector(translation);

	return scalingMat * rotationMat * translationMat;
}

inline ObjectData BuildObjectData(const TransformComponent& transform, const MeshComponent& mesh, size_t subsetIndex)
{
	const auto& subset = mesh.subsets[subsetIndex];

	const auto positionOffset = (uint32_t)(mesh.globalOffset.position + subset.localOffset.position);
	const auto extraOffset = (uint32_t)(mesh.globalOffset.extra + subset.localOffset.extra);

	// The subset transform is in local space.
	const auto worldMatrix = XMMatrixMultiply(XMLoadFloat4x4(&subset.transform), BuildObjectWorldMatrix(transform));

	ObjectData instance;
	instance.worldMatrix = worldMatrix;
	instance.vertexMetadata = mesh.metadata;
	instance.materialIndex = (uint32_t)subset.materialIndex;
	instance.boundingSphereRadius = subset.boundingSphereRadius * MeshGeometry::MaxScaleAxis(worldMatrix);
	instance.boundingSphereCenter = subset.boundingSphereCenter;

	// Apply offsets
	const auto old = instance.vertexMetadata.channelOffsets[0][0];
	for (int i = 0; i < vertexChannels / 4 + 1; ++i)
	{
		instance.vertexMetadata.channelOffsets[i].AddAll(extraOffset);
	}
	instance.vertexMetadata.channelOffsets[0][0] = old + positionOffset;

	return instance;
}
