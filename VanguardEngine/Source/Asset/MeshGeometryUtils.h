// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <DirectXMath.h>

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace MeshGeometry
{
	// Single interleaved vertex. The CPU loading uses interleaved format to simplify mesh processing
	// and vendor lib usage. Deinterleaved into streams when uploading to GPU.
	struct MeshVertex
	{
		XMFLOAT3 position{ 0.f, 0.f, 0.f };
		XMFLOAT3 normal{ 0.f, 0.f, 0.f };
		XMFLOAT2 texcoord{ 0.f, 0.f };
		XMFLOAT4 tangent{ 0.f, 0.f, 0.f, 0.f };
		XMFLOAT4 color{ 1.f, 1.f, 1.f, 1.f };
	};

	// No padding, otherwise meshoptimizer can have issues.
	static_assert(sizeof(MeshVertex) == 64, "MeshVertex must be tightly packed.");

	enum MeshAttribute : uint32_t
	{
		meshAttributePosition = 1 << 0,
		meshAttributeNormal = 1 << 1,
		meshAttributeTexcoord = 1 << 2,
		meshAttributeTangent = 1 << 3,
		meshAttributeColor = 1 << 4
	};

	// Raw geometry for a single primitive.
	struct MeshData
	{
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;
		uint32_t attributes = meshAttributePosition;

		bool Has(uint32_t attribute) const { return (attributes & attribute) == attribute; }
		void Add(uint32_t attribute) { attributes |= attribute; }
	};

	// Rewrites strips and fans as a triangle list. Returns false for unsupported topologies.
	bool NormalizeTopology(int topology, std::vector<uint32_t>& indices);

	void GenerateNormals(MeshData& mesh);
	void GenerateTangents(MeshData& mesh);

	void Optimize(MeshData& mesh);

	struct BoundingSphere
	{
		XMFLOAT3 center{ 0.f, 0.f, 0.f };
		float radius = 0.f;
	};

	BoundingSphere ComputeBoundingSphere(const MeshData& mesh);

	// Largest scale component in the matrix.
	inline float MaxScaleAxis(const XMMATRIX& matrix)
	{
		const float x = XMVectorGetX(XMVector3Length(matrix.r[0]));
		const float y = XMVectorGetX(XMVector3Length(matrix.r[1]));
		const float z = XMVectorGetX(XMVector3Length(matrix.r[2]));

		return std::max(std::max(x, y), z);
	}
}
