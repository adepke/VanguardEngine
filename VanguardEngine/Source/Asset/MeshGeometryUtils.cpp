// Copyright (c) 2019-2022 Andrew Depke

#include <Asset/MeshGeometryUtils.h>

#include <meshoptimizer.h>
#include <tiny_gltf.h>

#include <cstring>
#include <utility>

namespace MeshGeometry
{
	bool NormalizeTopology(int topology, std::vector<uint32_t>& indices)
	{
		switch (topology)
		{
		case TINYGLTF_MODE_TRIANGLES:
			// Trim any trailing partial triangle so downstream code can assume index_count % 3 == 0,
			// which meshopt asserts on.
			indices.resize(indices.size() - indices.size() % 3);
			return true;

		case TINYGLTF_MODE_TRIANGLE_STRIP:
		{
			if (indices.size() < 3)
			{
				indices.clear();
				return true;
			}

			std::vector<uint32_t> result;
			result.reserve((indices.size() - 2) * 3);
			for (size_t i = 0; i + 2 < indices.size(); ++i)
			{
				// Odd triangles in a strip have reversed winding.
				if (i & 1)
					result.insert(result.end(), { indices[i + 1], indices[i], indices[i + 2] });
				else
					result.insert(result.end(), { indices[i], indices[i + 1], indices[i + 2] });
			}
			indices = std::move(result);
			return true;
		}

		case TINYGLTF_MODE_TRIANGLE_FAN:
		{
			if (indices.size() < 3)
			{
				indices.clear();
				return true;
			}

			std::vector<uint32_t> result;
			result.reserve((indices.size() - 2) * 3);
			for (size_t i = 1; i + 1 < indices.size(); ++i)
			{
				result.insert(result.end(), { indices[0], indices[i], indices[i + 1] });
			}
			indices = std::move(result);
			return true;
		}

		default:
			return false;
		}
	}

	void GenerateNormals(MeshData& mesh)
	{
		const size_t vertexCount = mesh.vertices.size();

		for (auto& vertex : mesh.vertices)
		{
			vertex.normal = XMFLOAT3{ 0.f, 0.f, 0.f };
		}

		for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
		{
			const uint32_t i0 = mesh.indices[i];
			const uint32_t i1 = mesh.indices[i + 1];
			const uint32_t i2 = mesh.indices[i + 2];

			if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
				continue;

			const auto p0 = XMLoadFloat3(&mesh.vertices[i0].position);
			const auto p1 = XMLoadFloat3(&mesh.vertices[i1].position);
			const auto p2 = XMLoadFloat3(&mesh.vertices[i2].position);

			// The un-normalized cross product is proportional to twice the triangle area, which
			// is the standard cheap approximation of angle weighting.
			const auto faceNormal = XMVector3Cross(XMVectorSubtract(p1, p0), XMVectorSubtract(p2, p0));

			for (const uint32_t index : { i0, i1, i2 })
			{
				auto accumulated = XMLoadFloat3(&mesh.vertices[index].normal);
				accumulated = XMVectorAdd(accumulated, faceNormal);
				XMStoreFloat3(&mesh.vertices[index].normal, accumulated);
			}
		}

		for (auto& vertex : mesh.vertices)
		{
			auto value = XMLoadFloat3(&vertex.normal);
			// Degenerate vertices (isolated, or surrounded only by zero-area triangles) fall back
			// to +Y rather than producing a NaN.
			if (XMVectorGetX(XMVector3LengthSq(value)) < 1e-24f)
			{
				vertex.normal = XMFLOAT3{ 0.f, 1.f, 0.f };
				continue;
			}

			XMStoreFloat3(&vertex.normal, XMVector3Normalize(value));
		}

		mesh.Add(meshAttributeNormal);
	}

	void GenerateTangents(MeshData& mesh)
	{
		const size_t vertexCount = mesh.vertices.size();
		const size_t indexCount = mesh.indices.size();

		// meshoptimizer reads normals and UVs unconditionally, so both must be meaningful and the
		// index buffer must be whole triangles.
		if (vertexCount == 0 || indexCount < 3 || indexCount % 3 != 0)
			return;

		if (!mesh.Has(meshAttributeNormal) || !mesh.Has(meshAttributeTexcoord))
			return;

		// One tangent per corner.
		std::vector<XMFLOAT4> cornerTangents(indexCount);
		meshopt_generateTangents(
			&cornerTangents[0].x, mesh.indices.data(), indexCount,
			&mesh.vertices[0].position.x, vertexCount, sizeof(MeshVertex),
			&mesh.vertices[0].normal.x, sizeof(MeshVertex),
			&mesh.vertices[0].texcoord.x, sizeof(MeshVertex),
			0);

		// Deindex so each corner stands alone, attach its tangent, then let meshopt weld back
		// together only the corners that agree on everything. This duplicates vertices exactly
		// where tangents diverge (UV mirror seams) and nowhere else.
		std::vector<MeshVertex> corners(indexCount);
		for (size_t i = 0; i < indexCount; ++i)
		{
			corners[i] = mesh.vertices[mesh.indices[i]];
			corners[i].tangent = cornerTangents[i];
		}

		std::vector<uint32_t> remap(indexCount);
		const size_t uniqueVertices = meshopt_generateVertexRemap(
			remap.data(), nullptr, indexCount, corners.data(), indexCount, sizeof(MeshVertex));

		mesh.vertices.assign(uniqueVertices, MeshVertex{});
		meshopt_remapVertexBuffer(mesh.vertices.data(), corners.data(), indexCount, sizeof(MeshVertex), remap.data());

		mesh.indices.resize(indexCount);
		meshopt_remapIndexBuffer(mesh.indices.data(), nullptr, indexCount, remap.data());

		mesh.Add(meshAttributeTangent);
	}

	void Optimize(MeshData& mesh)
	{
		const size_t vertexCount = mesh.vertices.size();
		const size_t indexCount = mesh.indices.size();

		if (vertexCount == 0 || indexCount < 3 || indexCount % 3 != 0)
			return;

		// Winding is preserved.
		meshopt_optimizeVertexCache(mesh.indices.data(), mesh.indices.data(), indexCount, vertexCount);

		std::vector<MeshVertex> reordered(vertexCount);
		const size_t uniqueVertices = meshopt_optimizeVertexFetch(
			reordered.data(), mesh.indices.data(), indexCount, mesh.vertices.data(), vertexCount, sizeof(MeshVertex));

		reordered.resize(uniqueVertices);
		mesh.vertices = std::move(reordered);
	}

	BoundingSphere ComputeBoundingSphere(const MeshData& mesh)
	{
		BoundingSphere result;

		if (mesh.vertices.empty())
			return result;

		const auto bounds = meshopt_computeSphereBounds(
			&mesh.vertices[0].position.x, mesh.vertices.size(), sizeof(MeshVertex), nullptr, 0);

		result.center = XMFLOAT3{ bounds.center[0], bounds.center[1], bounds.center[2] };
		result.radius = bounds.radius;

		return result;
	}
}
