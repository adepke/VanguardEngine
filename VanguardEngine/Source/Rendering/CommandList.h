// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;
class PipelineState;
class DescriptorAllocator;

class CommandList
{
protected:
	ResourcePtr<ID3D12CommandAllocator> allocator;  // #TODO: Potentially share allocators? Something to look into in the future.
	ResourcePtr<ID3D12GraphicsCommandList5> list;
	RenderDevice* device;

	std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers;

private:
	void TransitionBarrierInternal(ID3D12Resource* resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState);

public:
	auto* Native() const noexcept { return list.Get(); }

	void Create(RenderDevice* inDevice, D3D12_COMMAND_LIST_TYPE type);
	void SetName(std::wstring_view name);

	// #TODO: Support split barriers.
	void TransitionBarrier(std::shared_ptr<Buffer>& resource, D3D12_RESOURCE_STATES state) { TransitionBarrierInternal(resource->Native(), resource->state, state); resource->state = state; }
	void TransitionBarrier(std::shared_ptr<Texture>& resource, D3D12_RESOURCE_STATES state) { TransitionBarrierInternal(resource->Native(), resource->state, state); resource->state = state; }

	// Batch submits all pending barriers to the driver.
	void FlushBarriers();

	void BindPipelineState(PipelineState& state);
	void BindDescriptorAllocator(DescriptorAllocator& allocator);

	HRESULT Close();
	HRESULT Reset();
};