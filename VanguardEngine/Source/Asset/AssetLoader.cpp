// Copyright (c) 2019-2020 Andrew Depke

#include <Asset/AssetLoader.h>
#include <Asset/TextureLoader.h>
#include <Utility/ArraySize.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/texture.h>
#include <assimp/light.h>
#include <assimp/postprocess.h>

#include <vector>
#include <utility>

namespace AssetLoader
{
	MeshComponent LoadMesh(RenderDevice& Device, std::filesystem::path Path)
	{
		VGScopedCPUStat("Load Mesh");

		Assimp::Importer Importer{};

		auto* Scene = Importer.ReadFile(Path.generic_string(), 0);
		if (!Scene || !Scene->mNumMeshes)
		{
			VGLogError(Asset) << "Failed to load asset at '" << Path.generic_wstring() << "'.";
			return {};
		}

		if (!Scene->mNumMaterials)
		{
			VGLogError(Asset) << "Asset at '" << Path.generic_wstring() << "' doesn't contain any materials.";
			return {};
		}

		// #TODO: Load lights.

		constexpr auto PostProcessFlags =
			aiProcess_CalcTangentSpace |  // We need tangents and bitangents for normal mapping.
			aiProcess_Triangulate |  // We can only accept triangle faces.
			aiProcess_JoinIdenticalVertices |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_PreTransformVertices |  // Mesh merging.
			aiProcess_OptimizeMeshes;  // Mesh merging.

		Scene = Importer.ApplyPostProcessing(PostProcessFlags);

		auto TrimmedPath = Path;
		TrimmedPath.remove_filename();

		std::vector<std::shared_ptr<Material>> Materials{};
		Materials.reserve(Scene->mNumMaterials);

		for (uint32_t Iter = 0; Iter < Scene->mNumMaterials; ++Iter)
		{
			const auto& Mat = *Scene->mMaterials[Iter];

			Materials.emplace_back(std::make_shared<Material>());

			aiString TexturePath;

			const auto SearchTextureType = [&Mat, TexturePathPtr = &TexturePath](auto& TextureTypeList)
			{
				for (size_t Iter = 0; Iter < ArraySize(TextureTypeList); ++Iter)
				{
					if (Mat.GetTexture(TextureTypeList[Iter], 0, TexturePathPtr) == aiReturn_SUCCESS)
						return true;
				}

				return false;
			};

			// #TODO: Assimp sometimes returns an absolute path, sometimes a relative path. We should convert all paths to relative before sending them to LoadTexture.

			// Prefer PBR texture types, but fall back to legacy types (which may still be PBR, just incorrectly set in the asset).

			if (SearchTextureType(std::array{ aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }))
			{
				Materials[Iter]->Albedo = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}
			
			if (SearchTextureType(std::array{ aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS, aiTextureType_HEIGHT }))
			{
				Materials[Iter]->Normal = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE }))
			{
				//Materials[Iter]->Emissive = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_DIFFUSE_ROUGHNESS }))
			{
				Materials[Iter]->Roughness = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_METALNESS }))
			{
				Materials[Iter]->Metallic = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_AMBIENT_OCCLUSION }))
			{
				//Materials[Iter]->AmbientOcclusion = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}
		}

		// Sum vertices/indices from all the submeshes.
		size_t VertexCount = 0;
		size_t IndexCount = 0;

		for (uint32_t Iter = 0; Iter < Scene->mNumMeshes; ++Iter)
		{
			const auto* Mesh = Scene->mMeshes[Iter];

			// Basic mesh validation.

			if (!Mesh->HasPositions())
			{
				VGLogError(Asset) << "Asset at '" << Path.generic_wstring() << "' doesn't contain vertex positions.";
				return {};
			}

			if (!Mesh->HasNormals())
			{
				VGLogError(Asset) << "Asset at '" << Path.generic_wstring() << "' doesn't contain vertex normals.";
				return {};
			}

			if (!Mesh->HasTextureCoords(0))
			{
				VGLogError(Asset) << "Asset at '" << Path.generic_wstring() << "' doesn't contain vertex UVs.";
				return {};
			}

			if (!Mesh->HasTangentsAndBitangents())
			{
				VGLogError(Asset) << "Asset at '" << Path.generic_wstring() << "' doesn't contain vertex tangents.";
				return {};
			}

			VertexCount += Mesh->mNumVertices;
			IndexCount += Mesh->mNumFaces * 3;
		}

		std::vector<MeshComponent::Subset> Subsets;
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;

		Vertices.reserve(VertexCount);
		Indices.reserve(IndexCount);

		size_t VertexOffset = 0;
		size_t IndexOffset = 0;

		for (uint32_t MeshIter = 0; MeshIter < Scene->mNumMeshes; ++MeshIter)
		{
			const auto* Mesh = Scene->mMeshes[MeshIter];

			MeshComponent::Subset Subset{};
			Subset.VertexOffset = VertexOffset;
			Subset.IndexOffset = IndexOffset;
			Subset.Indices = Mesh->mNumFaces * 3;

			VertexOffset += Mesh->mNumVertices;
			IndexOffset += Subset.Indices;

			for (uint32_t Iter = 0; Iter < Mesh->mNumVertices; ++Iter)
			{
				const auto& Position = Mesh->mVertices[Iter];
				const auto& Normal = Mesh->mVertices[Iter];
				const auto& TexCoord = Mesh->mTextureCoords[0][Iter];
				const auto& Tangent = Mesh->mTangents[Iter];
				const auto& Bitangent = Mesh->mBitangents[Iter];

				Vertex Result{
					{ Position.x, Position.y, Position.z },
					{ Normal.x, Normal.y, Normal.z },
					{ TexCoord.x, TexCoord.y },
					{ Tangent.x, Tangent.y, Tangent.z },
					{ Bitangent.x, Bitangent.y, Bitangent.z }
				};

				Vertices.emplace_back(Result);
			}

			for (uint32_t Iter = 0; Iter < Mesh->mNumFaces; ++Iter)
			{
				const auto& Face = Mesh->mFaces[Iter];

				Indices.emplace_back(Face.mIndices[0]);
				Indices.emplace_back(Face.mIndices[1]);
				Indices.emplace_back(Face.mIndices[2]);
			}

			// Copy the submesh material, don't move since multiple submeshes can share a material.
			Subset.Mat = Materials[Mesh->mMaterialIndex];

			// #TODO: Get the mesh AABB.

			Subsets.emplace_back(std::move(Subset));
		}

		auto MeshComp = std::move(CreateMeshComponent(Device, Vertices, Indices));
		MeshComp.Subsets = Subsets;

		return std::move(MeshComp);
	}
}