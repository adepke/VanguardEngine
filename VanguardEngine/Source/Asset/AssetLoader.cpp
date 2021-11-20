// Copyright (c) 2019-2021 Andrew Depke

#include <Asset/AssetLoader.h>
#include <Asset/TextureLoader.h>
#include <Rendering/Device.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/PrimitiveAssembly.h>
#include <Rendering/MeshFactory.h>
#include <Rendering/Resource.h>
#include <Utility/StringTools.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <vector>
#include <list>
#include <utility>
#include <algorithm>

namespace AssetLoader
{
	Material CreateMaterial(RenderDevice& device, const tinygltf::Material& material, const tinygltf::Model& model)
	{
		// #TODO: Conform to PBR specification, including terms such as emissive factor.

		Material result;
		result.transparent = material.alphaMode != "OPAQUE";

		// Table layout:
		// Base color texture
		// Metallic roughness texture
		// Normal texture
		// Occlusion texture
		// Emissive texture
		// Padding[3]

		// #TODO: Single buffer for all materials.
		BufferDescription tableDesc{
			.updateRate = ResourceFrequency::Static,
			.bindFlags = BindFlag::ConstantBuffer,
			.accessFlags = AccessFlag::CPUWrite,
			.size = 1,
			.stride = 8 * sizeof(uint32_t)
		};

		result.materialBuffer = device.GetResourceManager().Create(tableDesc, VGText("Material table"));

		std::vector<uint32_t> table{};
		table.resize(8);

		const auto CreateTexture = [&](int index, std::wstring_view name, DXGI_FORMAT format, bool mipmap) -> uint32_t
		{
			if (index < 0)
			{
				return 0;
			}

			const auto& texture = model.images[model.textures[index].source];

			TextureDescription description{
				.bindFlags = BindFlag::ShaderResource,
				.accessFlags = AccessFlag::CPUWrite,
				.width = (uint32_t)texture.width,
				.height = (uint32_t)texture.height,
				.format = format,
				.mipMapping = mipmap
			};
			auto resource = device.GetResourceManager().Create(description, name);
			device.GetResourceManager().Write(resource, texture.image);
			if (mipmap)
			{
				device.GetResourceManager().GenerateMipmaps(resource);
			}
			device.GetDirectList().TransitionBarrier(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			return device.GetResourceManager().Get(resource).SRV->bindlessIndex;
		};

		// #TODO: Include asset name in texture name.
		table[0] = CreateTexture(material.pbrMetallicRoughness.baseColorTexture.index, VGText("Base color asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true);
		table[1] = CreateTexture(material.pbrMetallicRoughness.metallicRoughnessTexture.index, VGText("Metallic roughness asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM, true);
		table[2] = CreateTexture(material.normalTexture.index, VGText("Normal asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM, true);
		table[3] = CreateTexture(material.occlusionTexture.index, VGText("Occlusion asset texture"), DXGI_FORMAT_R8_UNORM, false);
		table[4] = CreateTexture(material.emissiveTexture.index, VGText("Emissive asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, false);  // #TODO: Correct format?
		table[5] = 0;
		table[6] = 0;
		table[7] = 0;

		std::vector<uint8_t> tableData{};
		tableData.resize(table.size() * sizeof(uint32_t));
		std::memcpy(tableData.data(), table.data(), tableData.size());

		device.GetResourceManager().Write(result.materialBuffer, tableData);
		device.GetDirectList().TransitionBarrier(result.materialBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		return result;
	}

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

	MeshComponent LoadMesh(RenderDevice& device, MeshFactory& factory, const std::filesystem::path& path)
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
				VGLogError(logAsset, "Unknown asset load file extension '{}'.", (path.has_extension() ? path.extension().generic_wstring() : VGText("[ No extension ]")));
			}
		}

		if (!warning.empty())
		{
			VGLogWarning(logAsset, "GLTF load: {}", Str2WideStr(warning));
		}

		if (!error.empty())
		{
			VGLogError(logAsset, "GLTF load: {}", Str2WideStr(error));
		}

		if (!result)
		{
			VGLogError(logAsset, "Failed to load asset '{}'.", path.filename().generic_wstring());
		}

		else
		{
			VGLog(logAsset, "Loaded asset '{}'.", path.filename().generic_wstring());
		}

		std::vector<PrimitiveAssembly> assemblies;
		std::vector<Material> materials;
		std::vector<uint32_t> materialIndices;
		std::list<std::vector<uint32_t>> indices;  // We convert indices instead of using TinyGLTF's stream. One buffer per assembly. Stable buffers.

		const auto& scene = model.scenes[model.defaultScene];

		for (const auto nodeIndex : scene.nodes)
		{
			const auto& node = model.nodes[nodeIndex];  // #TODO: Nodes can have children.
			const auto& mesh = model.meshes[node.mesh];

			for (const auto& material : model.materials)
			{
				materials.emplace_back(CreateMaterial(device, material, model));
			}

			for (const auto& primitive : mesh.primitives)
			{
				PrimitiveAssembly assembly;

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
				
				std::vector<uint32_t> primitiveIndices;
				primitiveIndices.reserve(indexAccessor.count);

				for (int i = 0; i < indexAccessor.count; ++i)
				{
					if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
					{
						primitiveIndices.emplace_back(static_cast<uint32_t>(*(indexData16 + i)));
					}

					else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
					{
						primitiveIndices.emplace_back(*(indexData32 + i));
					}
				}

				indices.emplace_back(std::move(primitiveIndices));
				assembly.AddIndexStream(std::span{ indices.back().data(), indexAccessor.count });

				for (const auto& [name, idx] : primitive.attributes)
				{
					const auto& accessor = model.accessors[idx];
					const auto& bufferView = model.bufferViews[accessor.bufferView];
					const auto& buffer = model.buffers[bufferView.buffer];
					const auto* data = buffer.data.data() + bufferView.byteOffset;

					switch (accessor.type)
					{
					case TINYGLTF_TYPE_VEC2: assembly.AddVertexStream(name, std::span{ (XMFLOAT2*)data, accessor.count }); break;
					case TINYGLTF_TYPE_VEC3: assembly.AddVertexStream(name, std::span{ (XMFLOAT3*)data, accessor.count }); break;
					case TINYGLTF_TYPE_VEC4: assembly.AddVertexStream(name, std::span{ (XMFLOAT4*)data, accessor.count }); break;
					default: VGAssert(false, "Unknown primitive accessor type."); break;
					}
				}

				assemblies.emplace_back(std::move(assembly));

				materialIndices.emplace_back(primitive.material);
			}
		}

		// Reverse the winding order for all assemblies.
		for (auto& indexBuffer : indices)
		{
			for (int i = 2; i < indexBuffer.size(); i += 3)
			{
				std::iter_swap(indexBuffer.begin() + i - 2, indexBuffer.begin() + i);
			}
		}

		return factory.CreateMeshComponent(assemblies, materials, materialIndices);
	}
}