// Copyright (c) 2019-2022 Andrew Depke

#include <Asset/AssetLoader.h>
#include <Asset/TextureLoader.h>
#include <Asset/AssetManager.h>
#include <Asset/GltfAccessor.h>
#include <Asset/MeshGeometryUtils.h>
#include <Rendering/Device.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/PrimitiveAssembly.h>
#include <Rendering/MeshFactory.h>
#include <Rendering/Resource.h>
#include <Rendering/ShaderStructs.h>
#include <Utility/StringTools.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <vector>
#include <list>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <cstdint>

namespace AssetLoader
{
	namespace
	{
		// List of attributes to load that we care about.
		constexpr const char* attributePosition = "POSITION";
		constexpr const char* attributeNormal = "NORMAL";
		constexpr const char* attributeTexcoord = "TEXCOORD_0";
		constexpr const char* attributeTangent = "TANGENT";
		constexpr const char* attributeColor = "COLOR_0";

		// Owning storage for a single primitive's data. PrimitiveAssembly has views into this storage.
		struct PrimitiveData
		{
			MeshGeometry::MeshData mesh;
			MeshGeometry::BoundingSphere bounds;
		};

		// Note: GLTF stores column-major, this converts to row-major.
		XMMATRIX ReadNodeMatrix(const tinygltf::Node& node)
		{
			if (node.matrix.size() == 16)
			{
				return XMMATRIX(
					(float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
					(float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
					(float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
					(float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]);
			}

			auto result = XMMatrixIdentity();

			if (node.scale.size() == 3)
			{
				result = XMMatrixMultiply(result, XMMatrixScaling((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
			}

			if (node.rotation.size() == 4)
			{
				// GLTF quaternions are (x, y, z, w).
				const auto quaternion = XMVectorSet((float)node.rotation[0], (float)node.rotation[1],
					(float)node.rotation[2], (float)node.rotation[3]);
				result = XMMatrixMultiply(result, XMMatrixRotationQuaternion(XMQuaternionNormalize(quaternion)));
			}

			if (node.translation.size() == 3)
			{
				result = XMMatrixMultiply(result, XMMatrixTranslation((float)node.translation[0],
					(float)node.translation[1], (float)node.translation[2]));
			}

			return result;
		}

		// A single drawable, with a transform relative to the asset space.
		struct FlattenedInstance
		{
			size_t primitive = 0;
			int material = -1;
			XMFLOAT4X4 transform;
		};

		// Recursively parse a node graph into a collection of instances, in asset local space.
		void FlattenNode(const tinygltf::Model& model, int nodeIndex, const XMMATRIX& parentTransform,
			const std::unordered_map<uint64_t, size_t>& primitiveLookup, std::vector<FlattenedInstance>& instances,
			std::vector<bool>& visited)
		{
			if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size())
				return;

			// Spec cannot contain cycles, but safeguard against bad files.
			if (visited[nodeIndex])
			{
				VGLogWarning(logAsset, "Node graph contains a cycle at node {}, skipping.", nodeIndex);
				return;
			}
			visited[nodeIndex] = true;

			const auto& node = model.nodes[nodeIndex];
			const auto transform = XMMatrixMultiply(ReadNodeMatrix(node), parentTransform);

			if (node.mesh >= 0 && node.mesh < (int)model.meshes.size())
			{
				const auto& mesh = model.meshes[node.mesh];
				for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
				{
					const uint64_t key = ((uint64_t)node.mesh << 32) | (uint32_t)primitiveIndex;
					if (const auto it = primitiveLookup.find(key); it != primitiveLookup.end())
					{
						FlattenedInstance instance;
						instance.primitive = it->second;
						instance.material = mesh.primitives[primitiveIndex].material;
						XMStoreFloat4x4(&instance.transform, transform);

						instances.emplace_back(instance);
					}
				}
			}

			for (const auto child : node.children)
			{
				FlattenNode(model, child, transform, primitiveLookup, instances, visited);
			}
		}
	}

	size_t CreateMaterial(RenderDevice& device, const tinygltf::Material& material, const tinygltf::Model& model)
	{
		size_t result = AssetManager::Get().EnqueueMaterialLoad(material);
		return result;
	}

	MeshComponent LoadMesh(RenderDevice& device, MeshFactory& factory, const std::filesystem::path& path)
	{
		VGScopedCPUStat("Load Mesh");

		if (!std::filesystem::exists(path))
		{
			VGLogError(logAsset, "Asset '{}' does not exist in the filesystem.", path.filename().generic_wstring());
			return {};
		}

		std::string error;
		std::string warning;

		tinygltf::Model& model = AssetManager::Get().BeginModel();
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
			return {};
		}

		VGLog(logAsset, "Loaded asset '{}'.", path.filename().generic_wstring());

		for (const auto& extension : model.extensionsRequired)
		{
			VGLogWarning(logAsset, "Asset '{}' requires unsupported extension '{}', results may be incorrect.",
				path.filename().generic_wstring(), Str2WideStr(extension));
		}

		std::vector<size_t> materials;
		materials.reserve(model.materials.size());
		for (const auto& material : model.materials)
		{
			materials.emplace_back(CreateMaterial(device, material, model));
		}

		// Decode every primitive once, keyed by (mesh, primitive) so that instanced meshes
		// share vertex data.
		std::list<PrimitiveData> primitiveStorage;
		std::vector<PrimitiveData*> primitives;
		std::unordered_map<uint64_t, size_t> primitiveLookup;
		std::unordered_set<std::string> droppedAttributes;

		// True if any of the primitives have these attributes. Allows for properly supporting
		// mixed attribute models.
		bool anyNormals = false;
		bool anyTexcoords = false;
		bool anyTangents = false;
		bool anyColors = false;

		{
			VGScopedCPUStat("Decode Primitives");

			for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
			{
				const auto& mesh = model.meshes[meshIndex];

				for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
				{
					const auto& primitive = mesh.primitives[primitiveIndex];

					const auto positionIt = primitive.attributes.find(attributePosition);
					if (positionIt == primitive.attributes.end())
					{
						VGLogWarning(logAsset, "Primitive {} of mesh {} has no POSITION attribute, skipping.", primitiveIndex, meshIndex);
						continue;
					}

					PrimitiveData data;
					auto& mesh = data.mesh;

					const auto positionFloats = GltfAccessor::ReadFloats(model, positionIt->second, 3);
					if (positionFloats.empty())
					{
						VGLogWarning(logAsset, "Primitive {} of mesh {} has an unreadable POSITION accessor, skipping.", primitiveIndex, meshIndex);
						continue;
					}

					const size_t vertexCount = positionFloats.size() / 3;
					mesh.vertices.resize(vertexCount);
					for (size_t i = 0; i < vertexCount; ++i)
					{
						std::memcpy(&mesh.vertices[i].position, positionFloats.data() + i * 3, sizeof(XMFLOAT3));
					}

					if (primitive.indices >= 0)
					{
						mesh.indices = GltfAccessor::ReadIndices(model, primitive.indices);
					}

					else
					{
						// Mesh isn't indexed, just use sequential indices then.
						mesh.indices.resize(vertexCount);
						for (size_t i = 0; i < vertexCount; ++i)
							mesh.indices[i] = (uint32_t)i;
					}

					if (mesh.indices.empty())
					{
						VGLogWarning(logAsset, "Primitive {} of mesh {} has no usable indices, skipping.", primitiveIndex, meshIndex);
						continue;
					}

					if (!MeshGeometry::NormalizeTopology(primitive.mode, mesh.indices))
					{
						VGLogWarning(logAsset, "Primitive {} of mesh {} uses unsupported topology mode {}, skipping.",
							primitiveIndex, meshIndex, primitive.mode);
						continue;
					}

					if (mesh.indices.size() < 3)
						continue;

					// Check for invalid indices.
					const auto invalid = std::find_if(mesh.indices.begin(), mesh.indices.end(),
						[vertexCount](uint32_t index) { return index >= vertexCount; });
					if (invalid != mesh.indices.end())
					{
						VGLogWarning(logAsset, "Primitive {} of mesh {} contains out of range indices, skipping.", primitiveIndex, meshIndex);
						continue;
					}

					const auto ReadAttribute = [&](const char* name, size_t components, const float* defaults) -> std::vector<float>
					{
						const auto it = primitive.attributes.find(name);
						if (it == primitive.attributes.end())
							return {};

						auto values = GltfAccessor::ReadFloats(model, it->second, components, defaults);
						// A mismatched element count means the file is inconsistent; treating the
						// attribute as absent is safer than interleaving misaligned data.
						if (values.size() / components != vertexCount)
						{
							VGLogWarning(logAsset, "Attribute '{}' on primitive {} of mesh {} has {} elements but POSITION has {}, ignoring.",
								Str2WideStr(name), primitiveIndex, meshIndex, values.size() / components, vertexCount);
							return {};
						}

						return values;
					};

					// Loads an attribute into the respective slot on the interleaved vertex.
					const auto CopyAttribute = [&](auto MeshGeometry::MeshVertex::* member, const char* name,
						size_t components, const float* defaults, uint32_t attribute)
					{
						const auto values = ReadAttribute(name, components, defaults);
						if (values.empty())
							return;

						for (size_t i = 0; i < vertexCount; ++i)
						{
							std::memcpy(&(mesh.vertices[i].*member), values.data() + i * components, components * sizeof(float));
						}

						mesh.Add(attribute);
					};

					CopyAttribute(&MeshGeometry::MeshVertex::normal, attributeNormal, 3, nullptr, MeshGeometry::meshAttributeNormal);
					CopyAttribute(&MeshGeometry::MeshVertex::texcoord, attributeTexcoord, 2, nullptr, MeshGeometry::meshAttributeTexcoord);
					CopyAttribute(&MeshGeometry::MeshVertex::tangent, attributeTangent, 4, nullptr, MeshGeometry::meshAttributeTangent);

					// COLOR_0 is allowed to be VEC3, in which case alpha defaults to fully opaque.
					static constexpr float colorDefaults[4] = { 0.f, 0.f, 0.f, 1.f };
					CopyAttribute(&MeshGeometry::MeshVertex::color, attributeColor, 4, colorDefaults, MeshGeometry::meshAttributeColor);

					for (const auto& [name, accessor] : primitive.attributes)
					{
						if (name != attributePosition && name != attributeNormal && name != attributeTexcoord &&
							name != attributeTangent && name != attributeColor)
						{
							droppedAttributes.insert(name);
						}
					}

					anyNormals |= mesh.Has(MeshGeometry::meshAttributeNormal);
					anyTexcoords |= mesh.Has(MeshGeometry::meshAttributeTexcoord);
					anyTangents |= mesh.Has(MeshGeometry::meshAttributeTangent);
					anyColors |= mesh.Has(MeshGeometry::meshAttributeColor);

					primitiveLookup.emplace(((uint64_t)meshIndex << 32) | (uint32_t)primitiveIndex, primitives.size());
					primitives.emplace_back(&primitiveStorage.emplace_back(std::move(data)));
				}
			}
		}

		for (const auto& name : droppedAttributes)
		{
			VGLogWarning(logAsset, "Asset '{}' uses vertex attribute '{}', which has no vertex channel and was dropped.",
				path.filename().generic_wstring(), Str2WideStr(name));
		}

		if (primitives.empty())
		{
			VGLogWarning(logAsset, "Asset '{}' produced no renderable primitives.", path.filename().generic_wstring());
			return {};
		}

		// If any primitive has a material with a normal map, every primitive needs tangents,
		// since the vertex layout is shared.
		const bool needsTangents = anyTangents || std::any_of(model.materials.begin(), model.materials.end(),
			[](const tinygltf::Material& material) { return material.normalTexture.index >= 0; });

		// Tangent generation reads normals and texture coordinates, so requiring tangents pulls
		// both into the layout even if the asset provided neither.
		anyNormals |= needsTangents;
		anyTexcoords |= needsTangents;

		{
			VGScopedCPUStat("Complete Vertex Layout");

			for (auto* primitive : primitives)
			{
				auto& mesh = primitive->mesh;

				if (anyNormals && !mesh.Has(MeshGeometry::meshAttributeNormal))
				{
					MeshGeometry::GenerateNormals(mesh);
				}

				// Declare the channels even if they're unpopulated and zeroed. #TOOD: think about
				// if it makes sense to do this.
				if (anyTexcoords)
				{
					mesh.Add(MeshGeometry::meshAttributeTexcoord);
				}
				if (anyColors)
				{
					mesh.Add(MeshGeometry::meshAttributeColor);
				}

				// Must run before winding flip.
				if (needsTangents && !mesh.Has(MeshGeometry::meshAttributeTangent))
				{
					MeshGeometry::GenerateTangents(mesh);
				}
			}
		}

		{
			VGScopedCPUStat("Optimize Primitives");

			for (auto* primitive : primitives)
			{
				MeshGeometry::Optimize(primitive->mesh);

				// Previous work could change the bounds of the mesh so compute bounds last.
				primitive->bounds = MeshGeometry::ComputeBoundingSphere(primitive->mesh);
			}
		}

		// Reverse the winding order for all primitives.
		for (auto* primitive : primitives)
		{
			auto& indices = primitive->mesh.indices;
			for (size_t i = 0; i + 2 < indices.size(); i += 3)
			{
				std::swap(indices[i], indices[i + 2]);
			}
		}

		std::vector<PrimitiveAssembly> assemblies;
		assemblies.reserve(primitives.size());

		// Setup the primitive assembly views into the data.
		for (auto* primitive : primitives)
		{
			auto& mesh = primitive->mesh;
			const auto* vertices = mesh.vertices.data();
			const size_t count = mesh.vertices.size();
			constexpr size_t stride = sizeof(MeshGeometry::MeshVertex);

			PrimitiveAssembly assembly;
			assembly.AddIndexStream(std::span{ mesh.indices });
			assembly.AddVertexStream(attributePosition, &vertices->position, count, stride);

			if (mesh.Has(MeshGeometry::meshAttributeNormal))
				assembly.AddVertexStream(attributeNormal, &vertices->normal, count, stride);
			if (mesh.Has(MeshGeometry::meshAttributeTexcoord))
				assembly.AddVertexStream(attributeTexcoord, &vertices->texcoord, count, stride);
			if (mesh.Has(MeshGeometry::meshAttributeTangent))
				assembly.AddVertexStream(attributeTangent, &vertices->tangent, count, stride);
			if (mesh.Has(MeshGeometry::meshAttributeColor))
				assembly.AddVertexStream(attributeColor, &vertices->color, count, stride);

			assemblies.emplace_back(std::move(assembly));
		}

		// Flatten the node graph into a collection of local space subsets.
		std::vector<FlattenedInstance> flattened;
		std::vector<bool> visited(model.nodes.size(), false);

		int sceneIndex = model.defaultScene;
		if (sceneIndex < 0 || sceneIndex >= (int)model.scenes.size())
		{
			sceneIndex = 0;
		}

		if (model.scenes.size() > 1)
		{
			VGLogWarning(logAsset, "Asset '{}' contains {} scenes, using scene {}.",
				path.filename().generic_wstring(), model.scenes.size(), sceneIndex);
		}

		if (!model.scenes.empty() && !model.scenes[sceneIndex].nodes.empty())
		{
			for (const auto nodeIndex : model.scenes[sceneIndex].nodes)
			{
				FlattenNode(model, nodeIndex, XMMatrixIdentity(), primitiveLookup, flattened, visited);
			}
		}

		// If there's no scene graph, then fall back to just loading all the meshes individually.
		if (flattened.empty())
		{
			VGLogWarning(logAsset, "Asset '{}' has no usable scene graph, falling back to untransformed primitives.",
				path.filename().generic_wstring());

			for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
			{
				const auto& mesh = model.meshes[meshIndex];
				for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
				{
					const uint64_t key = ((uint64_t)meshIndex << 32) | (uint32_t)primitiveIndex;
					if (const auto it = primitiveLookup.find(key); it != primitiveLookup.end())
					{
						FlattenedInstance instance;
						instance.primitive = it->second;
						instance.material = mesh.primitives[primitiveIndex].material;
						XMStoreFloat4x4(&instance.transform, XMMatrixIdentity());

						flattened.emplace_back(instance);
					}
				}
			}
		}

		std::vector<MeshInstance> instances;
		instances.reserve(flattened.size());

		for (const auto& instance : flattened)
		{
			const auto& bounds = primitives[instance.primitive]->bounds;

			MeshInstance output;
			output.assembly = instance.primitive;
			output.transform = instance.transform;
			output.boundingSphereCenter = bounds.center;
			output.boundingSphereRadius = bounds.radius;

			if (instance.material >= 0 && instance.material < (int)materials.size())
				output.material = materials[instance.material];
			else
				output.material = 0;

			instances.emplace_back(output);
		}

		VGLog(logAsset, "Asset '{}': {} primitives, {} instances, {} materials.",
			path.filename().generic_wstring(), primitives.size(), instances.size(), materials.size());

		return factory.CreateMeshComponent(assemblies, instances);
	}
}
