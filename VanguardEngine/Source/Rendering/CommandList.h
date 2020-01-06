// Copyright (c) 2019-2020 Andrew Depke

#pragma once

struct CommandList
{
protected:
	ResourcePtr<ID3D12CommandAllocator> Allocator;
	ResourcePtr<ID3D12GraphicsCommandList5> List;

public:
	auto* Native() const noexcept { return List.Get(); };

	void Create(D3D12_COMMAND_LIST_TYPE Type);

	void AddResourceBarrier(const std::shared_ptr<GPUBuffer>& Resource, TransitionBarrier Barrier);
};