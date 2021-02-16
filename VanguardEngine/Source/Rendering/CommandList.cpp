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

	boundPipelineReflection = state.GetReflectionData();

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

void CommandList::BindConstants(const std::string& bindName, std::vector<uint32_t> data, size_t offset)
{
	VGAssert(boundPipelineReflection, "Attempted to bind resource without first binding a pipeline.");

	const auto& bindMetadata = boundPipelineReflection->resourceIndexMap.at(bindName);  // Can't use operator[] due to lack of const-ness.
	switch (bindMetadata.type)
	{
	case PipelineStateReflection::ResourceBindType::RootConstants:
		list->SetGraphicsRoot32BitConstants(bindMetadata.signatureIndex, data.size(), data.data(), offset);
		break;
	default:
		VGAssert(false, "Invalid binding, attempting to bind constants to binding '%s', where the bind type is '%i'.", bindName, bindMetadata.type);
		break;
	}
}

void CommandList::BindResource(const std::string& bindName, BufferHandle handle, size_t offset)
{
	VGAssert(boundPipelineReflection, "Attempted to bind resource without first binding a pipeline.");

	auto& bufferComponent = device->GetResourceManager().Get(handle);

	const auto& bindMetadata = boundPipelineReflection->resourceIndexMap.at(bindName);  // Can't use operator[] due to lack of const-ness.
	switch (bindMetadata.type)
	{
	case PipelineStateReflection::ResourceBindType::ConstantBuffer:
		list->SetGraphicsRootConstantBufferView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		break;
	case PipelineStateReflection::ResourceBindType::ShaderResource:
		list->SetGraphicsRootShaderResourceView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		break;
	case PipelineStateReflection::ResourceBindType::UnorderedAccess:
		list->SetGraphicsRootUnorderedAccessView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		break;
	default:
		VGAssert(false, "Invalid binding, attempting to bind buffer to binding '%s', where the bind type is '%i'.", bindName, bindMetadata.type);
		break;
	}
}

void CommandList::BindResourceTable(const std::string& bindName, D3D12_GPU_DESCRIPTOR_HANDLE descriptor)
{
	VGAssert(boundPipelineReflection, "Attempted to bind resource without first binding a pipeline.");

	const auto& bindMetadata = boundPipelineReflection->resourceIndexMap.at(bindName);  // Can't use operator[] due to lack of const-ness.
	list->SetGraphicsRootDescriptorTable(bindMetadata.signatureIndex, descriptor);
}

void CommandList::DrawFullscreenQuad()
{
	list->DrawInstanced(3, 1, 0, 0);
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