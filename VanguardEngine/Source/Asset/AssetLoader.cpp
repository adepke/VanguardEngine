// Copyright (c) 2019-2021 Andrew Depke

#include <Asset/AssetLoader.h>
#include <Asset/TextureLoader.h>

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
	MeshComponent LoadMesh(RenderDevice& device, std::filesystem::path path)
	{
		VGScopedCPUStat("Load Mesh");

		Assimp::Importer importer{};

		const aiScene* scene = nullptr;

		{
			VGScopedCPUStat("Importer Read");

			scene = importer.ReadFile(path.generic_string(), 0);
		}

		if (!scene || !scene->mNumMeshes)
		{
			VGLogError(Asset) << "Failed to load asset at '" << path.generic_wstring() << "'.";
			return {};
		}

		if (!scene->mNumMaterials)
		{
			VGLogError(Asset) << "Asset at '" << path.generic_wstring() << "' doesn't contain any materials.";
			return {};
		}

		// #TODO: Load lights.

		constexpr auto postProcessFlags =
			aiProcess_CalcTangentSpace |  // We need tangents and bitangents for normal mapping.
			aiProcess_Triangulate |  // We can only accept triangle faces.
			aiProcess_JoinIdenticalVertices |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_PreTransformVertices |  // Mesh merging.
			aiProcess_OptimizeMeshes;  // Mesh merging.

		{
			VGScopedCPUStat("Post Process");

			scene = importer.ApplyPostProcessing(postProcessFlags);
		}

		auto trimmedPath = path;
		trimmedPath.remove_filename();

		std::vector<Material> materials{};
		materials.reserve(scene->mNumMaterials);

		for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
		{
			materials.emplace_back();
			const auto& mat = *scene->mMaterials[i];

			aiString texturePath;

			const auto SearchTextureType = [&mat, texturePathPtr = &texturePath](auto& textureTypeList)
			{
				for (size_t i = 0; i < std::size(textureTypeList); ++i)
				{
					if (mat.GetTexture(textureTypeList[i], 0, texturePathPtr) == aiReturn_SUCCESS)
						return true;
				}

				return false;
			};

			// #TODO: Assimp sometimes returns an absolute path, sometimes a relative path. We should convert all paths to relative before sending them to LoadTexture.

			// Prefer PBR texture types, but fall back to legacy types (which may still be PBR, just incorrectly set in the asset).

			// Only use the albedo while working on bindless.
			if (SearchTextureType(std::array{ aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }))
			{
				// This was moved back due to materials that don't have an albedo texture causing the default value (bindlessIndex = 0)
				// to be bound, potentially causing a descriptor type mismatch. Instead, only build the material buffer if we have the
				// albedo.

				// Create the material table.
				BufferDescription matTableDesc{
					.updateRate = ResourceFrequency::Static,
					.bindFlags = BindFlag::ConstantBuffer,
					.accessFlags = AccessFlag::CPUWrite,
					.size = 1,
					.stride = sizeof(uint32_t)
				};

				materials[i].materialBuffer = device.GetResourceManager().Create(matTableDesc, VGText("Material table"));

				const auto textureHandle = LoadTexture(device, trimmedPath / texturePath.C_Str());
				const TextureComponent& textureComponent = device.GetResourceManager().Get(textureHandle);
				const auto bindlessIndex = textureComponent.SRV->bindlessIndex;

				std::vector<uint32_t> materialTable{};
				materialTable.emplace_back(bindlessIndex);

				std::vector<uint8_t> materialTableBytes{};
				materialTableBytes.resize(materialTable.size() * sizeof(uint32_t));
				std::memcpy(materialTableBytes.data(), materialTable.data(), materialTableBytes.size());

				device.GetResourceManager().Write(materials[i].materialBuffer, materialTableBytes);

				device.GetDirectList().TransitionBarrier(materials[i].materialBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			}
			
			/*
			if (SearchTextureType(std::array{ aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS, aiTextureType_HEIGHT }))
			{
				materials[i]->normal = LoadTexture(device, trimmedPath / texturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE }))
			{
				//Materials[Iter]->emissive = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_DIFFUSE_ROUGHNESS }))
			{
				materials[i]->roughness = LoadTexture(device, trimmedPath / texturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_METALNESS }))
			{
				materials[i]->metallic = LoadTexture(device, trimmedPath / texturePath.C_Str());
			}

			if (SearchTextureType(std::array{ aiTextureType_AMBIENT_OCCLUSION }))
			{
				//Materials[Iter]->ambientOcclusion = LoadTexture(Device, TrimmedPath / TexturePath.C_Str());
			}
			*/
		}

		// Sum vertices/indices from all the submeshes.
		size_t vertexCount = 0;
		size_t indexCount = 0;

		for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
		{
			const auto* mesh = scene->mMeshes[i];

			// Basic mesh validation.

			if (!mesh->HasPositions())
			{
				VGLogError(Asset) << "Asset at '" << path.generic_wstring() << "' doesn't contain vertex positions.";
				return {};
			}

			if (!mesh->HasNormals())
			{
				VGLogError(Asset) << "Asset at '" << path.generic_wstring() << "' doesn't contain vertex normals.";
				return {};
			}

			if (!mesh->HasTextureCoords(0))
			{
				VGLogError(Asset) << "Asset at '" << path.generic_wstring() << "' doesn't contain vertex UVs.";
				return {};
			}

			if (!mesh->HasTangentsAndBitangents())
			{
				VGLogError(Asset) << "Asset at '" << path.generic_wstring() << "' doesn't contain vertex tangents.";
				return {};
			}

			vertexCount += mesh->mNumVertices;
			indexCount += mesh->mNumFaces * 3;
		}

		std::vector<MeshComponent::Subset> subsets;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		vertices.reserve(vertexCount);
		indices.reserve(indexCount);

		size_t vertexOffset = 0;
		size_t indexOffset = 0;

		{
			VGScopedCPUStat("Mesh Building");

			for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
			{
				const auto* mesh = scene->mMeshes[i];

				MeshComponent::Subset subset{};
				subset.vertexOffset = vertexOffset;
				subset.indexOffset = indexOffset;
				subset.indices = mesh->mNumFaces * 3;

				vertexOffset += mesh->mNumVertices;
				indexOffset += subset.indices;

				for (uint32_t j = 0; j < mesh->mNumVertices; ++j)
				{
					const auto& position = mesh->mVertices[j];
					const auto& normal = mesh->mVertices[j];
					const auto& texCoord = mesh->mTextureCoords[0][j];
					const auto& tangent = mesh->mTangents[j];
					const auto& bitangent = mesh->mBitangents[j];

					Vertex result{
						{ position.x, position.y, position.z },
						{ normal.x, normal.y, normal.z },
						{ texCoord.x, texCoord.y },
						{ tangent.x, tangent.y, tangent.z },
						{ bitangent.x, bitangent.y, bitangent.z }
					};

					vertices.emplace_back(result);
				}

				for (uint32_t j = 0; j < mesh->mNumFaces; ++j)
				{
					const auto& face = mesh->mFaces[j];

					indices.emplace_back(face.mIndices[0]);
					indices.emplace_back(face.mIndices[1]);
					indices.emplace_back(face.mIndices[2]);
				}

				// Copy the submesh material, don't move since multiple submeshes can share a material.
				subset.material = materials[mesh->mMaterialIndex];

				// #TODO: Get the mesh AABB.

				subsets.emplace_back(std::move(subset));
			}
		}

		auto meshComp = std::move(CreateMeshComponent(device, vertices, indices));
		meshComp.subsets = subsets;

		return std::move(meshComp);
	}
}