// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;
class PipelineState;
class DescriptorAllocator;
struct PipelineStateReflection;

class CommandList
{
protected:
	ResourcePtr<ID3D12CommandAllocator> allocator;  // #TODO: Potentially share allocators? Something to look into in the future.
	ResourcePtr<ID3D12GraphicsCommandList5> list;
	RenderDevice* device;

	// Stateful tracking of the bound pipeline's reflection data for binding.
	const PipelineStateReflection* boundPipelineReflection = nullptr;

	std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers;

private:
	void TransitionBarrierInternal(ID3D12Resource* resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState);

public:
	auto* Native() const noexcept { return list.Get(); }

	void Create(RenderDevice* inDevice, D3D12_COMMAND_LIST_TYPE type);
	void SetName(std::wstring_view name);

	// #TODO: Support split barriers.
	void TransitionBarrier(BufferHandle resource, D3D12_RESOURCE_STATES state);
	void TransitionBarrier(TextureHandle resource, D3D12_RESOURCE_STATES state);

	// Batch submits all pending barriers to the driver.
	void FlushBarriers();

	void BindPipelineState(const PipelineState& state);
	void BindDescriptorAllocator(DescriptorAllocator& allocator);
	void BindConstants(const std::string& bindName, const std::vector<uint32_t>& data, size_t offset = 0);
	void BindResource(const std::string& bindName, BufferHandle handle, size_t offset = 0);
	void BindResourceTable(const std::string& bindName, D3D12_GPU_DESCRIPTOR_HANDLE descriptor);

	void DrawFullscreenQuad();

	HRESULT Close();
	HRESULT Reset();
};