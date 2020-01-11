// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/CommandList.h>
#include <Rendering/Device.h>

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

void CommandList::AddResourceBarrier(TransitionBarrier Barrier)
{
	TransitionBarriers.push_back(std::move(Barrier));
}

HRESULT CommandList::Close()
{
	return List->Close();
}

HRESULT CommandList::Reset()
{
	TransitionBarriers.clear();

	auto Result = Allocator->Reset();
	if (FAILED(Result))
	{
		return Result;
	}

	return List->Reset(Allocator.Get(), nullptr);
}