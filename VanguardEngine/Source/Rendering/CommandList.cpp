// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/CommandList.h>
#include <Rendering/Device.h>
#include <Rendering/PipelineState.h>

void CommandList::TransitionBarrierInternal(ID3D12Resource* Resource, D3D12_RESOURCE_STATES OldState, D3D12_RESOURCE_STATES NewState)
{
	if (OldState == NewState)
		return;

	D3D12_RESOURCE_BARRIER Barrier;
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	Barrier.Transition.pResource = Resource;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	Barrier.Transition.StateBefore = OldState;
	Barrier.Transition.StateAfter = NewState;

	PendingBarriers.emplace_back(std::move(Barrier));
}

void CommandList::Create(RenderDevice& Device, D3D12_COMMAND_LIST_TYPE Type)
{
	auto Result = Device.Native()->CreateCommandAllocator(Type, IID_PPV_ARGS(Allocator.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create command allocator: " << Result;
	}

	Result = Device.Native()->CreateCommandList(0, Type, Allocator.Get(), nullptr, IID_PPV_ARGS(List.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create command list: " << Result;
	}
}

void CommandList::SetName(std::wstring_view Name)
{
	Allocator->SetName(Name.data());
	List->SetName(Name.data());
}

void CommandList::FlushBarriers()
{
	if (!PendingBarriers.size())
		return;

	List->ResourceBarrier(static_cast<UINT>(PendingBarriers.size()), PendingBarriers.data());

	PendingBarriers.clear();
}

void CommandList::BindPipelineState(PipelineState& State)
{
	List->IASetPrimitiveTopology(State.Description.Topology);
	List->SetGraphicsRootSignature(State.RootSignature.Get());
	//List->SetGraphicsRootDescriptorTable()  // #TODO: Set the descriptor table?
	List->SetPipelineState(State.Native());
}

HRESULT CommandList::Close()
{
	return List->Close();
}

HRESULT CommandList::Reset()
{
	auto Result = Allocator->Reset();
	if (FAILED(Result))
	{
		return Result;
	}

	return List->Reset(Allocator.Get(), nullptr);
}