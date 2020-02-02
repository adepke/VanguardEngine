// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Barrier.h>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;
class PipelineState;

struct CommandList
{
protected:
	ResourcePtr<ID3D12CommandAllocator> Allocator;  // #TODO: Potentially share allocators? Something to look into in the future.
	ResourcePtr<ID3D12GraphicsCommandList5> List;

	std::vector<TransitionBarrier> TransitionBarriers;

public:
	auto* Native() const noexcept { return List.Get(); }

	void Create(RenderDevice& Device, D3D12_COMMAND_LIST_TYPE Type);
	void SetName(std::wstring_view Name);

	void BindPipelineState(PipelineState& State);
	void AddResourceBarrier(TransitionBarrier Barrier);

	HRESULT Close();
	HRESULT Reset();
};