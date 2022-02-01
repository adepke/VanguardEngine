// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/CommandList.h>
#include <Rendering/Device.h>
#include <Rendering/PipelineState.h>

void ValidateTransition(const BufferDescription& description, D3D12_RESOURCE_STATES newState)
{
#if !BUILD_RELEASE
	if (description.updateRate == ResourceFrequency::Dynamic)
	{
		// Render graph can attempt to transition to nonpixel/pixel, which just gets discarded since generic read already
		// covers that, so just make sure we're transitioning to a state that's covered by generic read.
		VGAssert(newState & D3D12_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers must always be in generic read state.");
	}
	else
	{
		VGAssert(newState != D3D12_RESOURCE_STATE_DEPTH_READ &&
			newState != D3D12_RESOURCE_STATE_DEPTH_WRITE &&
			newState != D3D12_RESOURCE_STATE_RENDER_TARGET,
			"Incorrect state transition for a buffer.");
	}
#endif
}

void ValidateTransition(const TextureDescription& description, D3D12_RESOURCE_STATES newState)
{
#if !BUILD_RELEASE
	VGAssert(newState != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER &&
		newState != D3D12_RESOURCE_STATE_INDEX_BUFFER &&
		newState != D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT &&
		newState != D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE &&
		newState != D3D12_RESOURCE_STATE_GENERIC_READ,
		"Incorrect state transition for a texture.");

#endif
}

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

void CommandList::BindResourceInternal(const std::string& bindName, BufferHandle handle, size_t offset, bool optional)
{
	VGAssert(boundPipeline, "Attempted to bind resource without first binding a pipeline.");

	const auto hasBinding = boundPipeline->GetReflectionData()->resourceIndexMap.contains(bindName);
	if (optional && !hasBinding)
	{
		return;
	}

	else if (!optional)
	{
		VGAssert(boundPipeline->GetReflectionData()->resourceIndexMap.contains(bindName), "Shader does not contain resource bind '%s'", bindName.c_str());
	}

	auto& bufferComponent = device->GetResourceManager().Get(handle);

	const auto& bindMetadata = boundPipeline->GetReflectionData()->resourceIndexMap.at(bindName);  // Can't use operator[] due to lack of const-ness.
	switch (bindMetadata.type)
	{
	case PipelineStateReflection::ResourceBindType::ConstantBuffer:
		if (boundPipeline->vertexShader)
		{
			list->SetGraphicsRootConstantBufferView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		}

		else
		{
			list->SetComputeRootConstantBufferView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		}

		break;
	case PipelineStateReflection::ResourceBindType::ShaderResource:
		if (boundPipeline->vertexShader)
		{
			list->SetGraphicsRootShaderResourceView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		}

		else
		{
			list->SetComputeRootShaderResourceView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		}

		break;
	case PipelineStateReflection::ResourceBindType::UnorderedAccess:
		if (boundPipeline->vertexShader)
		{
			list->SetGraphicsRootUnorderedAccessView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		}

		else
		{
			list->SetComputeRootUnorderedAccessView(bindMetadata.signatureIndex, bufferComponent.Native()->GetGPUVirtualAddress() + offset);
		}

		break;
	default:
		VGAssert(false, "Invalid binding, attempting to bind buffer to binding '%s', where the bind type is '%i'.", bindName.c_str(), bindMetadata.type);
		break;
	}
}

void CommandList::Create(RenderDevice* inDevice, D3D12_COMMAND_LIST_TYPE type)
{
	VGScopedCPUStat("Command List Create");

	device = inDevice;

	auto result = device->Native()->CreateCommandAllocator(type, IID_PPV_ARGS(allocator.Indirect()));
	if (FAILED(result))
	{
		VGLogCritical(logRendering, "Failed to create command allocator: {}", result);
	}

	result = device->Native()->CreateCommandList(0, type, allocator.Get(), nullptr, IID_PPV_ARGS(list.Indirect()));
	if (FAILED(result))
	{
		VGLogCritical(logRendering, "Failed to create command list: {}", result);
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

	ValidateTransition(component.description, state);
	TransitionBarrierInternal(component.Native(), component.state, state);
	component.state = state;
}

void CommandList::TransitionBarrier(TextureHandle resource, D3D12_RESOURCE_STATES state)
{
	auto& component = device->GetResourceManager().Get(resource);

	ValidateTransition(component.description, state);
	TransitionBarrierInternal(component.Native(), component.state, state);
	component.state = state;
}

void CommandList::UAVBarrier(BufferHandle resource)
{
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.UAV.pResource = device->GetResourceManager().Get(resource).Native();

	pendingBarriers.emplace_back(std::move(barrier));
}

void CommandList::UAVBarrier(TextureHandle resource)
{
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.UAV.pResource = device->GetResourceManager().Get(resource).Native();

	pendingBarriers.emplace_back(std::move(barrier));
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

	boundPipeline = &state;

	if (state.vertexShader)
	{
		list->IASetPrimitiveTopology(state.graphicsDescription.topology);
	}

	if (state.vertexShader)
	{
		list->SetGraphicsRootSignature(state.rootSignature.Get());
	}

	else
	{
		list->SetComputeRootSignature(state.rootSignature.Get());
	}

	list->SetPipelineState(state.Native());
}

void CommandList::BindDescriptorAllocator(DescriptorAllocator& allocator)
{
	VGScopedCPUStat("Bind Descriptor Allocator");

	auto* descriptorHeap = allocator.defaultHeap.Native();
	list->SetDescriptorHeaps(1, &descriptorHeap);
}

void CommandList::BindConstants(const std::string& bindName, const std::vector<uint32_t>& data, size_t offset)
{
	VGAssert(boundPipeline, "Attempted to bind resource without first binding a pipeline.");
	VGAssert(boundPipeline->GetReflectionData()->resourceIndexMap.contains(bindName), "Shader does not contain constant bind '%s'", bindName.c_str());

	const auto& bindMetadata = boundPipeline->GetReflectionData()->resourceIndexMap.at(bindName);  // Can't use operator[] due to lack of const-ness.
	switch (bindMetadata.type)
	{
	case PipelineStateReflection::ResourceBindType::RootConstants:
		if (boundPipeline->vertexShader)
		{
			list->SetGraphicsRoot32BitConstants(bindMetadata.signatureIndex, data.size(), data.data(), offset);
		}

		else
		{
			list->SetComputeRoot32BitConstants(bindMetadata.signatureIndex, data.size(), data.data(), offset);
		}

		break;
	default:
		VGAssert(false, "Invalid binding, attempting to bind constants to binding '%s', where the bind type is '%i'.", bindName.c_str(), bindMetadata.type);
		break;
	}
}

void CommandList::BindResourceTable(const std::string& bindName, D3D12_GPU_DESCRIPTOR_HANDLE descriptor)
{
	VGAssert(boundPipeline, "Attempted to bind resource without first binding a pipeline.");
	VGAssert(boundPipeline->GetReflectionData()->resourceIndexMap.contains(bindName), "Shader does not contain resource table bind '%s'", bindName.c_str());

	const auto& bindMetadata = boundPipeline->GetReflectionData()->resourceIndexMap.at(bindName);  // Can't use operator[] due to lack of const-ness.

	if (boundPipeline->vertexShader)
	{
		list->SetGraphicsRootDescriptorTable(bindMetadata.signatureIndex, descriptor);
	}

	else
	{
		list->SetComputeRootDescriptorTable(bindMetadata.signatureIndex, descriptor);
	}
}

void CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	list->Dispatch(x, y, z);
}

void CommandList::DrawFullscreenQuad()
{
	list->DrawInstanced(3, 1, 0, 0);
}

void CommandList::Copy(BufferHandle destination, BufferHandle source)
{
	TransitionBarrier(destination, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
	FlushBarriers();

	auto& destinationComponent = device->GetResourceManager().Get(destination);
	auto& sourceComponent = device->GetResourceManager().Get(source);

	list->CopyResource(destinationComponent.Native(), sourceComponent.Native());
}

void CommandList::Copy(TextureHandle destination, TextureHandle source)
{
	TransitionBarrier(destination, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
	FlushBarriers();

	auto& destinationComponent = device->GetResourceManager().Get(destination);
	auto& sourceComponent = device->GetResourceManager().Get(source);

	list->CopyResource(destinationComponent.Native(), sourceComponent.Native());
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