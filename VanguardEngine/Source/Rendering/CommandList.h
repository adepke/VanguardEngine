// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>

#include <Core/Windows/DirectX12Minimal.h>

#include <cstring>

class RenderDevice;
class RenderGraph;
class PipelineState;
class RenderPipelineLayout;
class DescriptorAllocator;
struct PipelineStateReflection;

class CommandList
{
protected:
	ResourcePtr<ID3D12CommandAllocator> allocator;  // #TODO: Potentially share allocators? Something to look into in the future.
	ResourcePtr<ID3D12GraphicsCommandList5> list;
	RenderDevice* device;
	RenderGraph* graph;
	size_t passIndex;

	// Stateful tracking of the bound pipeline.
	const PipelineState* boundPipeline = nullptr;

	std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers;

private:
	void TransitionBarrierInternal(ID3D12Resource* resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState);
	void BindResourceInternal(const std::string& bindName, BufferHandle handle, size_t offset, bool optional);

public:
	auto* Native() const noexcept { return list.Get(); }

	void Create(RenderDevice* inDevice, RenderGraph* inGraph, D3D12_COMMAND_LIST_TYPE type, size_t pass);
	void SetName(std::wstring_view name);

	// #TODO: Support split barriers.
	void TransitionBarrier(BufferHandle resource, D3D12_RESOURCE_STATES state);
	void TransitionBarrier(TextureHandle resource, D3D12_RESOURCE_STATES state);
	void UAVBarrier(BufferHandle resource);
	void UAVBarrier(TextureHandle resource);

	// Batch submits all pending barriers to the driver.
	void FlushBarriers();

	void BindPipelineState(const PipelineState& state);
	void BindPipeline(const RenderPipelineLayout& layout);
	void BindDescriptorAllocator(DescriptorAllocator& allocator, bool visibleHeap = true);
	void BindConstants(const std::string& bindName, const std::vector<uint32_t>& data, size_t offset = 0);
	template <typename T>
	void BindConstants(const std::string& bindName, const T& data, size_t offset = 0);
	void BindResource(const std::string& bindName, BufferHandle handle, size_t offset = 0);
	void BindResourceOptional(const std::string& bindName, BufferHandle handle, size_t offset = 0);
	void BindResourceTable(const std::string& bindName, D3D12_GPU_DESCRIPTOR_HANDLE descriptor);

	void Dispatch(uint32_t x, uint32_t y, uint32_t z);
	void DrawFullscreenQuad();

	void Copy(BufferHandle destination, BufferHandle source);
	void Copy(TextureHandle destination, TextureHandle source);

	HRESULT Close();
	HRESULT Reset();
};

template <typename T>
inline void CommandList::BindConstants(const std::string& bindName, const T& data, size_t offset)
{
	std::vector<uint32_t> constants;
	constants.resize(sizeof(T) / sizeof(uint32_t));
	std::memcpy(constants.data(), &data, constants.size() * sizeof(uint32_t));

	BindConstants(bindName, constants, offset);
}

inline void CommandList::BindResource(const std::string& bindName, BufferHandle handle, size_t offset)
{
	BindResourceInternal(bindName, handle, offset, false);
}

inline void CommandList::BindResourceOptional(const std::string& bindName, BufferHandle handle, size_t offset)
{
	BindResourceInternal(bindName, handle, offset, true);
}