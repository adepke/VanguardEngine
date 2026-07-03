// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/TextureCapture.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/Resource.h>
#include <Rendering/Base.h>
#include <Core/Logging.h>

#include <stb_image_write.h>

#include <cstring>
#include <fstream>

namespace TextureCapture
{
	PendingReadback Enqueue(RenderDevice& device, CommandList& list, TextureHandle source)
	{
		PendingReadback pending;

		auto& resourceManager = device.GetResourceManager();
		if (!resourceManager.Valid(source))
		{
			return pending;
		}

		auto& sourceComponent = resourceManager.Get(source);

		const auto resourceDesc = sourceComponent.Native()->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		uint64_t requiredSize = 0;
		device.Native()->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &footprint, nullptr, nullptr, &requiredSize);

		pending.width = sourceComponent.description.width;
		pending.height = sourceComponent.description.height;
		pending.rowPitch = footprint.Footprint.RowPitch;

		// #TODO: consider reusing this buffer.
		BufferDescription readbackDesc{};
		readbackDesc.updateRate = ResourceFrequency::Readback;
		readbackDesc.bindFlags = 0;
		readbackDesc.accessFlags = AccessFlag::CPURead;
		readbackDesc.size = static_cast<size_t>(requiredSize);
		readbackDesc.stride = 1;
		pending.buffer = resourceManager.Create(readbackDesc, VGText("Texture Capture Readback"));

		list.TransitionBarrier(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
		list.FlushBarriers();

		auto& readbackComponent = resourceManager.Get(pending.buffer);

		D3D12_TEXTURE_COPY_LOCATION sourceCopy{};
		sourceCopy.pResource = sourceComponent.Native();
		sourceCopy.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		sourceCopy.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION destCopy{};
		destCopy.pResource = readbackComponent.Native();
		destCopy.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		destCopy.PlacedFootprint = footprint;

		list.Native()->CopyTextureRegion(&destCopy, 0, 0, 0, &sourceCopy, nullptr);

		pending.valid = true;
		return pending;
	}

	std::vector<uint8_t> Resolve(RenderDevice& device, PendingReadback& pending)
	{
		std::vector<uint8_t> pngBytes;

		if (!pending.valid)
		{
			return pngBytes;
		}

		auto& resourceManager = device.GetResourceManager();

		// #TODO: full sync is excessive, consider integrating with render graph to make this smarter.
		device.Synchronize();

		std::vector<uint8_t> rawBytes;
		resourceManager.Read(pending.buffer, rawBytes);

		std::vector<uint8_t> packed;
		if (!rawBytes.empty() && pending.width > 0 && pending.height > 0)
		{
			// Strip the GPU row-pitch padding so we hand stb_image_write a tightly packed RGBA8
			// buffer matching the texture's logical width.
			const size_t tightRowBytes = static_cast<size_t>(pending.width) * 4;
			packed.resize(tightRowBytes * pending.height);
			for (uint32_t row = 0; row < pending.height; ++row)
			{
				std::memcpy(
					packed.data() + row * tightRowBytes,
					rawBytes.data() + static_cast<size_t>(row) * pending.rowPitch,
					tightRowBytes);
			}
		}

		if (!packed.empty())
		{
			const auto writeFunction = [](void* context, void* data, int size)
			{
				auto* output = static_cast<std::vector<uint8_t>*>(context);
				const auto* bytes = static_cast<const uint8_t*>(data);
				output->insert(output->end(), bytes, bytes + size);
			};

			stbi_write_png_to_func(
				writeFunction, &pngBytes,
				static_cast<int>(pending.width),
				static_cast<int>(pending.height),
				4,
				packed.data(),
				static_cast<int>(pending.width * 4));
		}

		// Tear down the readback buffer and invalidate the pending state.
		resourceManager.Destroy(pending.buffer);
		pending = PendingReadback{};

		return pngBytes;
	}

	bool WritePngFile(const std::filesystem::path& path, const std::vector<uint8_t>& pngBytes)
	{
		if (pngBytes.empty())
		{
			return false;
		}

		std::error_code ec;
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path(), ec);
		}

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return false;
		}

		file.write(reinterpret_cast<const char*>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
		return static_cast<bool>(file);
	}
}
