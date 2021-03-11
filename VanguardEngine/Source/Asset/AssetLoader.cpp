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

			// Safety checks.
			VGAssert(sizeof(T) / sizeof(float) == accessor.type, "Mismatched vertex attribute data type.");

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
			VGLogWarning(Asset) << "GLTF load: " << warning;
		}

		if (!error.empty())
		{
			VGLogError(Asset) << "GLTF load: " << error;
		}

		if (!result)
		{
			VGLogError(Asset) << "Failed to load asset '" << path.filename().generic_string() << "'.";
		}

		else
		{
			VGLog(Asset) << "Loaded asset '" << path.filename().generic_string() << "'.";
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
				// #TODO: Conform to PBR specification, including terms such as emissive factor.			

				const auto& material = model.materials[i];
				materials.emplace_back();

				materials[i].transparent = material.alphaMode != "OPAQUE";

				// Table layout:
				// Base color texture
				// Metallic roughness texture
				// Normal texture
				// Occlusion texture
				// Emissive texture
				// Padding[3]

				// Create the material table.
				BufferDescription matTableDesc{
					.updateRate = ResourceFrequency::Static,
					.bindFlags = BindFlag::ConstantBuffer,
					.accessFlags = AccessFlag::CPUWrite,
					.size = 1,
					.stride = 8 * sizeof(uint32_t)
				};

				materials[i].materialBuffer = device.GetResourceManager().Create(matTableDesc, VGText("Material table"));

				TextureDescription textureDescription{};
				textureDescription.bindFlags = BindFlag::ShaderResource;
				textureDescription.accessFlags = AccessFlag::CPUWrite;

				std::vector<uint32_t> materialTable{};

				// #TODO: Reduce code duplication.

				const auto& baseColorTextureMeta = material.pbrMetallicRoughness.baseColorTexture;
				if (baseColorTextureMeta.index > 0)
				{
					const auto& baseColorTexture = model.images[model.textures[baseColorTextureMeta.index].source];

					textureDescription.width = baseColorTexture.width;
					textureDescription.height = baseColorTexture.height;
					textureDescription.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
					textureDescription.mipMapping = true;

					// #TODO: Derive name from asset name + texture type.
					auto textureResource = device.GetResourceManager().Create(textureDescription, VGText("Base color asset texture"));
					device.GetResourceManager().Write(textureResource, baseColorTexture.image);
					device.GetResourceManager().GenerateMipmaps(textureResource);
					device.GetDirectList().TransitionBarrier(textureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					const auto& textureComponent = device.GetResourceManager().Get(textureResource);
					const auto bindlessIndex = textureComponent.SRV->bindlessIndex;

					materialTable.emplace_back(bindlessIndex);
				}

				else
				{
					materialTable.emplace_back(0);
				}

				const auto& metallicRoughnessTextureMeta = material.pbrMetallicRoughness.metallicRoughnessTexture;
				if (metallicRoughnessTextureMeta.index > 0)
				{
					const auto& metallicRoughnessTexture = model.images[model.textures[metallicRoughnessTextureMeta.index].source];

					textureDescription.width = metallicRoughnessTexture.width;
					textureDescription.height = metallicRoughnessTexture.height;
					textureDescription.format = DXGI_FORMAT_R8G8B8A8_UNORM;
					textureDescription.mipMapping = true;

					// #TODO: Derive name from asset name + texture type.
					auto textureResource = device.GetResourceManager().Create(textureDescription, VGText("Metallic roughness asset texture"));
					device.GetResourceManager().Write(textureResource, metallicRoughnessTexture.image);
					device.GetResourceManager().GenerateMipmaps(textureResource);
					device.GetDirectList().TransitionBarrier(textureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					const auto& textureComponent = device.GetResourceManager().Get(textureResource);
					const auto bindlessIndex = textureComponent.SRV->bindlessIndex;

					materialTable.emplace_back(bindlessIndex);
				}

				else
				{
					materialTable.emplace_back(0);
				}

				const auto& normalTextureMeta = material.normalTexture;
				if (normalTextureMeta.index > 0)
				{
					const auto& normalTexture = model.images[model.textures[normalTextureMeta.index].source];

					textureDescription.width = normalTexture.width;
					textureDescription.height = normalTexture.height;
					textureDescription.format = DXGI_FORMAT_R8G8B8A8_UNORM;
					textureDescription.mipMapping = true;

					// #TODO: Derive name from asset name + texture type.
					auto textureResource = device.GetResourceManager().Create(textureDescription, VGText("Normal asset texture"));
					device.GetResourceManager().Write(textureResource, normalTexture.image);
					device.GetResourceManager().GenerateMipmaps(textureResource);
					device.GetDirectList().TransitionBarrier(textureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

					const auto& textureComponent = device.GetResourceManager().Get(textureResource);
					const auto bindlessIndex = textureComponent.SRV->bindlessIndex;

					materialTable.emplace_back(bindlessIndex);
				}

				else
				{
					materialTable.emplace_back(0);
				}

				const auto& occlusionTextureMeta = material.occlusionTexture;
				if (occlusionTextureMeta.index > 0)
				{
					const auto& occlusionTexture = model.images[model.textures[occlusionTextureMeta.index].source];

					textureDescription.width = occlusionTexture.width;
					textureDescription.height = occlusionTexture.height;
					textureDescription.format = DXGI_FORMAT_R8_UNORM;
					textureDescription.mipMapping = false;

					// #TODO: Derive name from asset name + texture type.
					auto textureResource = device.GetResourceManager().Create(textureDescription, VGText("Occlusion asset texture"));
					device.GetResourceManager().Write(textureResource, occlusionTexture.image);
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

					textureDescription.width = emissiveTexture.width;
					textureDescription.height = emissiveTexture.height;
					textureDescription.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // #TODO: Is this the correct format?
					textureDescription.mipMapping = false;

					// #TODO: Derive name from asset name + texture type.
					auto textureResource = device.GetResourceManager().Create(textureDescription, VGText("Emissive asset texture"));
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

				// Padding.
				materialTable.emplace_back(0);
				materialTable.emplace_back(0);
				materialTable.emplace_back(0);
				
				VGAssert(materialTable.size() == 8, "");

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
				const auto [tangentData, tangentCount] = FindVertexAttribute<XMFLOAT4>("TANGENT", model, primitive);

				vertices.reserve(vertices.size() + positionCount);
				const auto verticesSize = vertices.size();

				for (int i = 0; i < positionCount; ++i)
				{
					// Tangent has to be handled differently because we're trimming the fourth dimension.
					XMFLOAT3 tangent = { 0.f, 0.f, 0.f };
					if (tangentData)
					{
						const auto tangent4D = *(tangentData + i);
						tangent = { tangent4D.x, tangent4D.y, tangent4D.z };
					}

					vertices.push_back({
						.position = *(positionData + i),
						.normal = normalData ? *(normalData + i) : XMFLOAT3{ 0.f, 0.f, 0.f },
						.uv = texcoordData ? *(texcoordData + i) : XMFLOAT2{ 0.f, 0.f },
						.tangent = tangent,
						.bitangent = XMFLOAT3{ 0.f, 0.f, 0.f }
					});

					// Compute bitangent.
					if (tangentData)
					{
						const auto normal = XMLoadFloat3(&vertices[verticesSize + i].normal);
						const auto tangent = XMLoadFloat3(&vertices[verticesSize + i].tangent);

						XMStoreFloat3(&vertices[verticesSize + i].bitangent, XMVector3Cross(normal, tangent));
					}
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