// Copyright (c) 2019-2021 Andrew Depke

#include <Asset/AssetLoader.h>
#include <Asset/TextureLoader.h>
#include <Rendering/RenderComponents.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <vector>
#include <utility>
#include <algorithm>

namespace AssetLoader
{
	template <typename T, typename U, typename V>
	auto FindVertexAttribute(const char* name, U&& model, V&& primitive) -> std::pair<const T*, size_t>
	{
		if (const auto iterator = primitive.attributes.find(name); iterator != primitive.attributes.end())
		{
			const auto& accessor = model.accessors[iterator->second];
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			const auto& buffer = model.buffers[bufferView.buffer];
			const auto* data = reinterpret_cast<const T*>(buffer.data.data() + bufferView.byteOffset);

			return { data, accessor.count };
		}

		return { nullptr, 0 };
	}

	MeshComponent LoadMesh(RenderDevice& device, const std::filesystem::path& path)
	{
		VGScopedCPUStat("Load Mesh");

		std::string error;
		std::string warning;

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;

		bool result = false;

		{
			VGScopedCPUStat("Import");

			if (path.has_extension() && path.extension() == ".gltf")
			{
				result = loader.LoadASCIIFromFile(&model, &error, &warning, path.generic_string());
			}

			else if (path.has_extension() && path.extension() == ".glb")
			{
				result = loader.LoadBinaryFromFile(&model, &error, &warning, path.generic_string());
			}

			else
			{
				VGLogError(Asset) << "Unknown asset load file extension '" << (path.has_extension() ? path.extension() : "[ No extension ]") << "'.";
			}
		}

		if (!warning.empty())
		{
			//VGLogWarning(Asset) << "GLTF load: " << warning;
		}

		if (!error.empty())
		{
			//VGLogError(Asset) << "GLTF load: " << error;
		}

		if (!result)
		{
			//VGLogError(Asset) << "Failed to load asset '" << path.filename().generic_string() << "'.";
		}

		else
		{
			//VGLog(Asset) << "Loaded asset '" << path.filename().generic_string() << "'.";
		}

		std::vector<MeshComponent::Subset> subsets;
		std::vector<uint32_t> indices;
		std::vector<Vertex> vertices;
		size_t indexOffset = 0;
		size_t vertexOffset = 0;

		const auto& scene = model.scenes[model.defaultScene];

		for (const auto nodeIndex : scene.nodes)
		{
			const auto& node = model.nodes[nodeIndex];  // #TODO: Nodes can have children.
			const auto& mesh = model.meshes[node.mesh];

			// Load the model materials.

			std::vector<Material> materials;
			materials.reserve(model.materials.size());

			for (int i = 0; i < model.materials.size(); ++i)
			{
				// #TODO: Implement full PBR textures and other material features.				

				const auto& material = model.materials[i];
				materials.emplace_back();

				materials[i].transparent = material.alphaMode != "OPAQUE";

				// Create the material table.
				BufferDescription matTableDesc{
					.updateRate = ResourceFrequency::Static,
					.bindFlags = BindFlag::ConstantBuffer,
					.accessFlags = AccessFlag::CPUWrite,
					.size = 1,
					.stride = 2 * sizeof(uint32_t)
				};

				materials[i].materialBuffer = device.GetResourceManager().Create(matTableDesc, VGText("Material table"));

				TextureDescription sRGBTexture{};
				sRGBTexture.bindFlags = BindFlag::ShaderResource;
				sRGBTexture.accessFlags = AccessFlag::CPUWrite;
				sRGBTexture.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // Albedo and emissive textures are sRGB.

				TextureDescription linearTexture{};
				linearTexture.bindFlags = BindFlag::ShaderResource;
				linearTexture.accessFlags = AccessFlag::CPUWrite;
				linearTexture.format = DXGI_FORMAT_R8G8B8A8_UNORM;

				std::vector<uint32_t> materialTable{};

				const auto& albedoTextureMeta = material.pbrMetallicRoughness.baseColorTexture;
				if (albedoTextureMeta.index > 0)
				{
					const auto& albedoTexture = model.images[model.textures[albedoTextureMeta.index].source];

					sRGBTexture.width = albedoTexture.width;
					sRGBTexture.height = albedoTexture.height;

					// #TODO: Derive name from asset name + texture type.
					auto textureResource = device.GetResourceManager().Create(sRGBTexture, VGText("Albedo asset texture"));
					device.GetResourceManager().Write(textureResource, albedoTexture.image);
					device.GetDirectList().TransitionBarrier(textureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					const auto& textureComponent = device.GetResourceManager().Get(textureResource);
					const auto bindlessIndex = textureComponent.SRV->bindlessIndex;

					materialTable.emplace_back(bindlessIndex);
				}

				else
				{
					materialTable.emplace_back(0);
				}

				const auto& emissiveTextureMeta = material.emissiveTexture;
				if (emissiveTextureMeta.index > 0)
				{
					const auto& emissiveTexture = model.images[model.textures[emissiveTextureMeta.index].source];

					sRGBTexture.width = emissiveTexture.width;
					sRGBTexture.height = emissiveTexture.height;

					// #TODO: Derive name from asset name + texture type.
					auto textureResource = device.GetResourceManager().Create(sRGBTexture, VGText("Emissive asset texture"));
					device.GetResourceManager().Write(textureResource, emissiveTexture.image);
					device.GetDirectList().TransitionBarrier(textureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					const auto& textureComponent = device.GetResourceManager().Get(textureResource);
					const auto bindlessIndex = textureComponent.SRV->bindlessIndex;

					materialTable.emplace_back(bindlessIndex);
				}

				else
				{
					materialTable.emplace_back(0);
				}

				std::vector<uint8_t> materialTableBytes{};
				materialTableBytes.resize(materialTable.size() * sizeof(uint32_t));
				std::memcpy(materialTableBytes.data(), materialTable.data(), materialTableBytes.size());

				device.GetResourceManager().Write(materials[i].materialBuffer, materialTableBytes);
				device.GetDirectList().TransitionBarrier(materials[i].materialBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			}

			subsets.reserve(mesh.primitives.size());

			for (const auto& primitive : mesh.primitives)
			{
				const auto& indexAccessor = model.accessors[primitive.indices];

				VGAssert(
					indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ||
					indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT,
					"Indices must be unsigned 16 or 32 bit ints.");
				VGAssert(indexAccessor.bufferView >= 0 && indexAccessor.bufferView < model.bufferViews.size(), "Index buffer view is invalid.");

				const auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
				const auto& indexBuffer = model.buffers[indexBufferView.buffer];
				const auto* indexData16 = reinterpret_cast<const uint16_t*>(indexBuffer.data.data() + indexBufferView.byteOffset);
				const auto* indexData32 = reinterpret_cast<const uint32_t*>(indexBuffer.data.data() + indexBufferView.byteOffset);

				indices.reserve(indices.size() + indexAccessor.count);

				for (int i = 0; i < indexAccessor.count; ++i)
				{
					if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
					{
						indices.emplace_back(static_cast<uint32_t>(*(indexData16 + i)));
					}

					else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
					{
						indices.emplace_back(*(indexData32 + i));
					}
				}

				const auto [positionData, positionCount] = FindVertexAttribute<XMFLOAT3>("POSITION", model, primitive);
				const auto [normalData, normalCount] = FindVertexAttribute<XMFLOAT3>("NORMAL", model, primitive);
				const auto [texcoordData, texcoordCount] = FindVertexAttribute<XMFLOAT2>("TEXCOORD_0", model, primitive);
				const auto [tangentData, tangentCount] = FindVertexAttribute<XMFLOAT3>("TANGENT", model, primitive);

				vertices.reserve(vertices.size() + positionCount);

				for (int i = 0; i < positionCount; ++i)
				{
					vertices.push_back({
						.position = *(positionData + i),
						.normal = normalData ? *(normalData + i) : XMFLOAT3{ 0.f, 0.f, 0.f },
						.uv = texcoordData ? *(texcoordData + i) : XMFLOAT2{ 0.f, 0.f },
						.tangent = tangentData ? *(tangentData + i) : XMFLOAT3{ 0.f, 0.f, 0.f },
						.bitangent = XMFLOAT3{ 0.f, 0.f, 0.f }  // #TODO: Bitangents.
					});
				}

				subsets.push_back({
					.vertexOffset = vertexOffset,
					.indexOffset = indexOffset,
					.indices = indexAccessor.count,
					.material = materials[primitive.material]
				});

				indexOffset += indexAccessor.count;
				vertexOffset += positionCount;
			}
		}

		// Reverse the model's winding order.
		for (int i = 2; i < indices.size(); i += 3)
		{
			std::iter_swap(indices.begin() + i - 2, indices.begin() + i);
		}

		auto meshComponent = std::move(CreateMeshComponent(device, vertices, indices));
		meshComponent.subsets = std::move(subsets);

		return std::move(meshComponent);
	}
}