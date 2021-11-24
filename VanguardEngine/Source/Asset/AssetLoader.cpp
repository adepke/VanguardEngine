// Copyright (c) 2019-2021 Andrew Depke

#include <Asset/AssetLoader.h>
#include <Asset/TextureLoader.h>
#include <Asset/AssetManager.h>
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
		Material result;
		result.transparent = material.alphaMode != "OPAQUE";

		// #TODO: Conform to PBR specification, including terms such as emissive factor.
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
		device.GetDirectList().TransitionBarrier(result.materialBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		AssetManager::Get().EnqueueMaterialLoad(material, result.materialBuffer);

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

		//tinygltf::Model model;
		tinygltf::TinyGLTF loader;

		// #TEMP
		auto& model = AssetManager::Get().model;

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

		if (model.scenes.size() > 1)
		{
			VGLogWarning(logAsset, "Asset '{}' contains more than one scene, ignoring all except scene {}.", path.filename().generic_wstring(), model.defaultScene);
		}

		const auto& scene = model.scenes[model.defaultScene];
		if (scene.nodes.size() == 0)
		{
			VGLogWarning(logAsset, "Asset '{}' does not contain any nodes in the scene.", path.filename().generic_wstring());
			return {};
		}

		for (const auto& material : model.materials)
		{
			materials.emplace_back(CreateMaterial(device, material, model));
		}

		//for (const auto nodeIndex : scene.nodes)
		for (const auto& mesh : model.meshes)
		{
			//const auto& node = model.nodes[nodeIndex];

			// Process the node's mesh.
			//if (node.mesh >= 0)
			//model.meshes[node.mesh]

			// Process all the node's children.
			//for (const auto child : node.children)
			//model.nodes[child]

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