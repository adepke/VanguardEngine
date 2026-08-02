// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <tiny_gltf.h>

#include <vector>
#include <cstring>
#include <cstdint>
#include <limits>
#include <algorithm>

// GLTF decoding utilities.
// #TOOD: Consider using a different library from tinygtlf that provides this functionality
// so I don't need to roll my own.
namespace GltfAccessor
{
	inline size_t ComponentByteSize(int componentType)
	{
		const int32_t size = tinygltf::GetComponentSizeInBytes((uint32_t)componentType);
		return size > 0 ? (size_t)size : 0;
	}

	inline size_t ComponentCount(int type)
	{
		const int32_t count = tinygltf::GetNumComponentsInType((uint32_t)type);
		return count > 0 ? (size_t)count : 0;
	}

	// Decodes a single component, applying the glTF normalized-integer conventions.
	inline float ReadComponentFloat(const uint8_t* source, int componentType, bool normalized)
	{
		switch (componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
		{
			float value;
			std::memcpy(&value, source, sizeof(value));
			return value;
		}
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		{
			double value;
			std::memcpy(&value, source, sizeof(value));
			return static_cast<float>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		{
			int8_t value;
			std::memcpy(&value, source, sizeof(value));
			return normalized ? std::max(value / 127.f, -1.f) : static_cast<float>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		{
			uint8_t value;
			std::memcpy(&value, source, sizeof(value));
			return normalized ? value / 255.f : static_cast<float>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			int16_t value;
			std::memcpy(&value, source, sizeof(value));
			return normalized ? std::max(value / 32767.f, -1.f) : static_cast<float>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			uint16_t value;
			std::memcpy(&value, source, sizeof(value));
			return normalized ? value / 65535.f : static_cast<float>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_INT:
		{
			int32_t value;
			std::memcpy(&value, source, sizeof(value));
			return static_cast<float>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		{
			uint32_t value;
			std::memcpy(&value, source, sizeof(value));
			return static_cast<float>(value);
		}
		default: return 0.f;
		}
	}

	inline uint32_t ReadComponentUint(const uint8_t* source, int componentType)
	{
		switch (componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		{
			uint8_t value;
			std::memcpy(&value, source, sizeof(value));
			return value;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			uint16_t value;
			std::memcpy(&value, source, sizeof(value));
			return value;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		{
			uint32_t value;
			std::memcpy(&value, source, sizeof(value));
			return value;
		}
		// The spec forbids signed index component types, but tolerate them rather than
		// producing garbage geometry.
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		{
			int8_t value;
			std::memcpy(&value, source, sizeof(value));
			return static_cast<uint32_t>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			int16_t value;
			std::memcpy(&value, source, sizeof(value));
			return static_cast<uint32_t>(value);
		}
		case TINYGLTF_COMPONENT_TYPE_INT:
		{
			int32_t value;
			std::memcpy(&value, source, sizeof(value));
			return static_cast<uint32_t>(value);
		}
		default: return 0;
		}
	}

	inline const uint8_t* ResolveView(const tinygltf::Model& model, int bufferViewIndex, size_t byteOffset,
		size_t count, size_t elementSize, size_t stride)
	{
		if (bufferViewIndex < 0 || bufferViewIndex >= (int)model.bufferViews.size())
			return nullptr;

		const auto& bufferView = model.bufferViews[bufferViewIndex];
		if (bufferView.buffer < 0 || bufferView.buffer >= (int)model.buffers.size())
			return nullptr;

		const auto& buffer = model.buffers[bufferView.buffer];

		if (count == 0 || elementSize == 0 || stride == 0)
			return nullptr;

		// The last element must fit entirely within the view, and the view within the buffer.
		const size_t span = (count - 1) * stride + elementSize;
		if (byteOffset > bufferView.byteLength || span > bufferView.byteLength - byteOffset)
			return nullptr;

		const size_t absolute = bufferView.byteOffset + byteOffset;
		if (absolute > buffer.data.size() || span > buffer.data.size() - absolute)
			return nullptr;

		return buffer.data.data() + absolute;
	}

	inline const uint8_t* ResolveAccessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
		size_t elementSize, size_t& outStride)
	{
		outStride = elementSize;

		if (accessor.bufferView < 0 || accessor.bufferView >= (int)model.bufferViews.size())
			return nullptr;

		const int stride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
		if (stride <= 0)
			return nullptr;

		outStride = (size_t)stride;
		return ResolveView(model, accessor.bufferView, accessor.byteOffset, accessor.count, elementSize, outStride);
	}

	// Reads an accessor into a float array of outComponents per element. Missing components filled with defaults.
	inline std::vector<float> ReadFloats(const tinygltf::Model& model, int accessorIndex, size_t outComponents,
		const float* defaults = nullptr)
	{
		std::vector<float> result;

		if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size())
			return result;

		const auto& accessor = model.accessors[accessorIndex];
		const size_t inComponents = ComponentCount(accessor.type);
		const size_t componentSize = ComponentByteSize(accessor.componentType);
		if (inComponents == 0 || componentSize == 0 || accessor.count == 0)
			return result;

		const size_t elementSize = inComponents * componentSize;
		size_t stride = elementSize;
		const uint8_t* base = ResolveAccessor(model, accessor, elementSize, stride);

		result.resize(accessor.count * outComponents, 0.f);

		// An accessor with no buffer view is defined to be all zeros, which sparse values then patch.
		if (base)
		{
			for (size_t i = 0; i < accessor.count; ++i)
			{
				const uint8_t* element = base + i * stride;
				for (size_t c = 0; c < outComponents; ++c)
				{
					result[i * outComponents + c] = c < inComponents
						? ReadComponentFloat(element + c * componentSize, accessor.componentType, accessor.normalized)
						: (defaults ? defaults[c] : 0.f);
				}
			}
		}

		else if (defaults)
		{
			for (size_t i = 0; i < accessor.count; ++i)
				for (size_t c = inComponents; c < outComponents; ++c)
					result[i * outComponents + c] = defaults[c];
		}

		// Sparse accessors override a subset of elements.
		if (accessor.sparse.isSparse && accessor.sparse.count > 0)
		{
			// Sparse index and value views are defined by the spec to be tightly packed.
			const size_t indexComponentSize = ComponentByteSize(accessor.sparse.indices.componentType);

			const uint8_t* indexBase = ResolveView(model, accessor.sparse.indices.bufferView,
				accessor.sparse.indices.byteOffset, accessor.sparse.count, indexComponentSize, indexComponentSize);
			const uint8_t* valueBase = ResolveView(model, accessor.sparse.values.bufferView,
				accessor.sparse.values.byteOffset, accessor.sparse.count, elementSize, elementSize);

			if (indexBase && valueBase)
			{
				for (size_t i = 0; i < (size_t)accessor.sparse.count; ++i)
				{
					const uint32_t target = ReadComponentUint(indexBase + i * indexComponentSize, accessor.sparse.indices.componentType);
					if (target >= accessor.count)
						continue;

					const uint8_t* element = valueBase + i * elementSize;
					for (size_t c = 0; c < outComponents; ++c)
					{
						result[target * outComponents + c] = c < inComponents
							? ReadComponentFloat(element + c * componentSize, accessor.componentType, accessor.normalized)
							: (defaults ? defaults[c] : 0.f);
					}
				}
			}
		}

		return result;
	}

	// Reads a scalar accessor as 32 bit indices, widening 8 and 16 bit index buffers.
	inline std::vector<uint32_t> ReadIndices(const tinygltf::Model& model, int accessorIndex)
	{
		std::vector<uint32_t> result;

		if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size())
			return result;

		const auto& accessor = model.accessors[accessorIndex];
		const size_t componentSize = ComponentByteSize(accessor.componentType);
		if (componentSize == 0 || accessor.count == 0 || accessor.type != TINYGLTF_TYPE_SCALAR)
			return result;

		size_t stride = componentSize;
		const uint8_t* base = ResolveAccessor(model, accessor, componentSize, stride);

		result.resize(accessor.count, 0);

		if (base)
		{
			for (size_t i = 0; i < accessor.count; ++i)
			{
				result[i] = ReadComponentUint(base + i * stride, accessor.componentType);
			}
		}

		if (accessor.sparse.isSparse && accessor.sparse.count > 0)
		{
			// Sparse index and value views are defined by the spec to be tightly packed.
			const size_t indexComponentSize = ComponentByteSize(accessor.sparse.indices.componentType);

			const uint8_t* indexBase = ResolveView(model, accessor.sparse.indices.bufferView,
				accessor.sparse.indices.byteOffset, accessor.sparse.count, indexComponentSize, indexComponentSize);
			const uint8_t* valueBase = ResolveView(model, accessor.sparse.values.bufferView,
				accessor.sparse.values.byteOffset, accessor.sparse.count, componentSize, componentSize);

			if (indexBase && valueBase)
			{
				for (size_t i = 0; i < (size_t)accessor.sparse.count; ++i)
				{
					const uint32_t target = ReadComponentUint(indexBase + i * indexComponentSize, accessor.sparse.indices.componentType);
					if (target < accessor.count)
					{
						result[target] = ReadComponentUint(valueBase + i * componentSize, accessor.componentType);
					}
				}
			}
		}

		return result;
	}
}
