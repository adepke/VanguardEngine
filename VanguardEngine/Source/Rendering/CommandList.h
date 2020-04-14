// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;
class PipelineState;
class DescriptorAllocator;

struct CommandList
{
protected:
	ResourcePtr<ID3D12CommandAllocator> Allocator;  // #TODO: Potentially share allocators? Something to look into in the future.
	ResourcePtr<ID3D12GraphicsCommandList5> List;
	RenderDevice* Device;

	std::vector<D3D12_RESOURCE_BARRIER> PendingBarriers;

private:
	void TransitionBarrierInternal(ID3D12Resource* Resource, D3D12_RESOURCE_STATES OldState, D3D12_RESOURCE_STATES NewState);

public:
	auto* Native() const noexcept { return List.Get(); }

	void Create(RenderDevice* InDevice, D3D12_COMMAND_LIST_TYPE Type);
	void SetName(std::wstring_view Name);

	// #TODO: Support split barriers.
	void TransitionBarrier(std::shared_ptr<Buffer>& Resource, D3D12_RESOURCE_STATES State) { TransitionBarrierInternal(Resource->Native(), Resource->State, State); Resource->State = State; }
	void TransitionBarrier(std::shared_ptr<Texture>& Resource, D3D12_RESOURCE_STATES State) { TransitionBarrierInternal(Resource->Native(), Resource->State, State); Resource->State = State; }

	// Batch submits all pending barriers to the driver.
	void FlushBarriers();

	void BindPipelineState(PipelineState& State);
	void BindDescriptorAllocator(DescriptorAllocator& Allocator);

	HRESULT Close();
	HRESULT Reset();
};