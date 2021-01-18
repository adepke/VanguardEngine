// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/CommandList.h>
#include <Rendering/Device.h>
#include <Rendering/PipelineState.h>

void CommandList::TransitionBarrierInternal(ID3D12Resource* resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState)
{
	VGScopedCPUStat("Transition Barrier");

	// #TODO: Validation, either ensure we never transition from a read only state to another read only state,
	// or combine these read states before a flush.

	// Make sure we don't discard transitions to common. Special case since it's 0.
	if (newState == D3D12_RESOURCE_STATE_COMMON && oldState == D3D12_RESOURCE_STATE_COMMON)
		return;

	// No need to transition if we're in a state that covers the new state.
	else if ((newState & oldState) != 0)
		return;

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = oldState;
	barrier.Transition.StateAfter = newState;

	pendingBarriers.emplace_back(std::move(barrier));
}

void CommandList::Create(RenderDevice* inDevice, D3D12_COMMAND_LIST_TYPE type)
{
	VGScopedCPUStat("Command List Create");

	device = inDevice;

	auto result = device->Native()->CreateCommandAllocator(type, IID_PPV_ARGS(allocator.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create command allocator: " << result;
	}

	result = device->Native()->CreateCommandList(0, type, allocator.Get(), nullptr, IID_PPV_ARGS(list.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create command list: " << result;
	}
}

void CommandList::SetName(std::wstring_view name)
{
	allocator->SetName(name.data());
	list->SetName(name.data());
}

void CommandList::TransitionBarrier(BufferHandle resource, D3D12_RESOURCE_STATES state)
{
	auto& component = device->GetResourceManager().Get(resource);

	TransitionBarrierInternal(component.Native(), component.state, state);
	component.state = state;
}

void CommandList::TransitionBarrier(TextureHandle resource, D3D12_RESOURCE_STATES state)
{
	auto& component = device->GetResourceManager().Get(resource);

	TransitionBarrierInternal(component.Native(), component.state, state);
	component.state = state;
}

void CommandList::FlushBarriers()
{
	VGScopedCPUStat("Command List Barrier Flush");

	if (!pendingBarriers.size())
		return;

	list->ResourceBarrier(static_cast<UINT>(pendingBarriers.size()), pendingBarriers.data());

	pendingBarriers.clear();
}

void CommandList::BindPipelineState(const PipelineState& state)
{
	VGScopedCPUStat("Bind Pipeline");

	list->IASetPrimitiveTopology(state.description.topology);
	list->SetGraphicsRootSignature(state.rootSignature.Get());
	list->SetPipelineState(state.Native());
}

void CommandList::BindDescriptorAllocator(DescriptorAllocator& allocator)
{
	VGScopedCPUStat("Bind Descriptor Allocator");

	auto* descriptorHeap = allocator.defaultHeap.Native();
	list->SetDescriptorHeaps(1, &descriptorHeap);
}

HRESULT CommandList::Close()
{
	return list->Close();
}

HRESULT CommandList::Reset()
{
	auto result = allocator->Reset();
	if (FAILED(result))
	{
		return result;
	}

	return list->Reset(allocator.Get(), nullptr);
}