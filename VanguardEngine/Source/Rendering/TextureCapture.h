// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/ResourceHandle.h>

#include <filesystem>
#include <vector>
#include <cstdint>

class RenderDevice;
class CommandList;

// Utilities for CPU readback of a GPU texture.
namespace TextureCapture
{
	struct PendingReadback
	{
		BufferHandle buffer;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t rowPitch = 0;  // GPU row pitch of the readback buffer (>= width * 4, 256-aligned).
		bool valid = false;
	};

	// Enqueues a RGBA8 texture into a readback buffer. Resolve must be called after the GPU executed the list.
	PendingReadback Enqueue(RenderDevice& device, CommandList& list, TextureHandle source);

	// Does a full GPU sync and resolves the readback into tight packed PNG bytes.
	std::vector<uint8_t> Resolve(RenderDevice& device, PendingReadback& pending);

	// Writes resolved PNG bytes to a file.
	bool WritePngFile(const std::filesystem::path& path, const std::vector<uint8_t>& pngBytes);
}
