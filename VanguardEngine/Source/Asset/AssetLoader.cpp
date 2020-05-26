// Copyright (c) 2019-2020 Andrew Depke

#include <Asset/AssetLoader.h>
#include <Rendering/Renderer.h>

#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <vector>
#include <utility>

MeshComponent AssetLoader::Load(std::filesystem::path Path)
{
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

	// #TODO: Material loading.

	// Sum vertices/indices from all the submeshes.
	size_t VertexCount = 0;
	size_t IndexCount = 0;

	for (int Iter = 0; Iter < Scene->mNumMeshes; ++Iter)
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

	for (int MeshIter = 0; MeshIter < Scene->mNumMeshes; ++MeshIter)
	{
		const auto* Mesh = Scene->mMeshes[MeshIter];

		MeshComponent::Subset Subset{};
		Subset.VertexOffset = VertexOffset;
		Subset.IndexOffset = IndexOffset;
		Subset.Indices = Mesh->mNumFaces * 3;

		VertexOffset += Mesh->mNumVertices;
		IndexOffset += Subset.Indices;

		for (int Iter = 0; Iter < Mesh->mNumVertices; ++Iter)
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

		for (int Iter = 0; Iter < Mesh->mNumFaces; ++Iter)
		{
			const auto& Face = Mesh->mFaces[Iter];

			Indices.emplace_back(Face.mIndices[0]);
			Indices.emplace_back(Face.mIndices[1]);
			Indices.emplace_back(Face.mIndices[2]);
		}

		// #TODO: Get the mesh AABB.

		Subsets.emplace_back(std::move(Subset));
	}

	auto MeshComp = std::move(CreateMeshComponent(*Renderer::Get().Device, Vertices, Indices));
	MeshComp.Subsets = Subsets;

	return std::move(MeshComp);
}