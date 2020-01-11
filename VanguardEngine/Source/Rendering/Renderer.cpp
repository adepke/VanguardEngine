// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Device.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Rendering/CommandList.h>

//#include <entt/entt.hpp>  // #TODO: Include from here instead of in the header.

#include <vector>
#include <utility>

// Per-entity data, passed through input assembler.
struct EntityInstance
{
	// #TODO: Create from some form of shader interop.

	TransformComponent Transform;
};

auto Renderer::GetPassRenderTargets(RenderPass Pass)
{
	VGScopedCPUStat("Get Render Pass Targets");

	// #TODO: Replace with render graph.

	// Split into two vectors so that we can trivially pass to the command list.
	std::vector<ID3D12Resource*> RenderTargets;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> RenderTargetViews;

	switch (Pass)
	{
	case RenderPass::Main:
		RenderTargets.push_back({});
		RenderTargetViews.push_back({});
		RenderTargets[0] = Device->GetBackBuffer()->Resource->GetResource();
		RenderTargetViews[0] = { std::static_pointer_cast<GPUTexture>(Device->GetBackBuffer())->RTV.ptr };
		break;
	}

	VGAssert(RenderTargets.size() == RenderTargetViews.size(), "Size mismatch in render targets and render target views");

	return std::move(std::pair{ std::move(RenderTargets), std::move(RenderTargetViews) });
}

void Renderer::BeginRenderPass(RenderPass Pass)
{
	VGScopedCPUStat("Begin Render Pass");

	auto [RenderTargets, RenderTargetViews] = GetPassRenderTargets(Pass);

	std::vector<D3D12_RESOURCE_BARRIER> Barriers;
	Barriers.reserve(RenderTargets.size());

	for (const auto& Target : RenderTargets)
	{
		D3D12_RESOURCE_BARRIER Barrier;
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.pResource = Target;
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		Barriers.push_back(std::move(Barrier));
	}

	Device->GetDirectList()->ResourceBarrier(Barriers.size(), Barriers.data());

	std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> RenderPassTargets;
	RenderTargets.reserve(RenderTargetViews.size());

	for (const auto& View : RenderTargetViews)
	{
		D3D12_RENDER_PASS_RENDER_TARGET_DESC PassDesc{};
		PassDesc.cpuDescriptor = View;
		PassDesc.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		PassDesc.BeginningAccess.Clear.ClearValue.Color[0] = 0.f;
		PassDesc.BeginningAccess.Clear.ClearValue.Color[1] = 0.f;
		PassDesc.BeginningAccess.Clear.ClearValue.Color[2] = 0.f;
		PassDesc.BeginningAccess.Clear.ClearValue.Color[3] = 1.f;
		PassDesc.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		RenderPassTargets.push_back(std::move(PassDesc));
	}

	Device->GetDirectList()->BeginRenderPass(RenderTargets.size(), RenderPassTargets.data(), nullptr, D3D12_RENDER_PASS_FLAG_NONE);  // #TODO: Some passes need D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES.
}

void Renderer::EndRenderPass(RenderPass Pass)
{
	VGScopedCPUStat("End Render Pass");

	Device->GetDirectList()->EndRenderPass();

	auto [RenderTargets, RenderTargetViews] = GetPassRenderTargets(Pass);

	std::vector<D3D12_RESOURCE_BARRIER> Barriers;
	Barriers.reserve(RenderTargets.size());

	for (const auto& Target : RenderTargets)
	{
		D3D12_RESOURCE_BARRIER Barrier;
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.pResource = Target;
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

		Barriers.push_back(std::move(Barrier));
	}

	Device->GetDirectList()->ResourceBarrier(Barriers.size(), Barriers.data());
}

void Renderer::SetDescriptorHeaps(CommandList& List)
{
	VGScopedCPUStat("Set Descriptor Heaps");

	std::vector<ID3D12DescriptorHeap*> Heaps;

	Heaps.push_back(Device->GetResourceHeap().Native());
	Heaps.push_back(Device->GetSamplerHeap().Native());

	List.Native()->SetDescriptorHeaps(Heaps.size(), Heaps.data());
}

void Renderer::Initialize(std::unique_ptr<RenderDevice>&& InDevice)
{
	VGScopedCPUStat("Renderer Initialize");

	Device = std::move(InDevice);

	Device->CheckFeatureSupport();
	Device->ReloadShaders();
}

void Renderer::Render(entt::registry& Registry)
{
	VGScopedCPUStat("Render");

	Device->GetCopyList()->Close();
	
	ID3D12CommandList* CopyLists[] = { Device->GetCopyList() };

	{
		VGScopedCPUStat("Execute Copy");

		Device->GetCopyQueue()->ExecuteCommandLists(1, CopyLists);
	}

	// #TODO: Culling.

	// #TODO: Sort by material.
	
	// Number of entities with MeshComponent and TransformComponent
	const auto RenderCount = 0;

	std::pair<std::shared_ptr<GPUBuffer>, size_t> InstanceBuffer;

	{
		VGScopedCPUStat("Generate Instance Buffer");

		InstanceBuffer = std::move(Device->FrameAllocate(sizeof(EntityInstance) * RenderCount));

		Registry.view<const TransformComponent, const MeshComponent>().each([this, &InstanceBuffer](auto Entity, const auto& Transform, const auto&)
			{
				EntityInstance Instance{ Transform };

				std::vector<uint8_t> InstanceData{};
				InstanceData.resize(sizeof(EntityInstance));
				std::memcpy(InstanceData.data(), &Instance, InstanceData.size());

				Device->Write(InstanceBuffer.first, InstanceData, InstanceBuffer.second);
			});
	}

	// Global to all render passes.
	SetDescriptorHeaps(Device->GetDirectList());

	BeginRenderPass(RenderPass::Main);

	auto* DrawList = Device->GetDirectList();

	{
		VGScopedCPUStat("Main Pass");

		Registry.view<const TransformComponent, const MeshComponent>().each([&InstanceBuffer, DrawList](auto Entity, const auto&, const auto& Mesh)
			{
				std::vector<D3D12_VERTEX_BUFFER_VIEW> VertexViews;

				// Vertex Buffer.
				VertexViews.push_back({});
				VertexViews[0].BufferLocation = Mesh.VertexBuffer->Resource->GetResource()->GetGPUVirtualAddress();
				VertexViews[0].SizeInBytes = Mesh.VertexBuffer->Description.Size;
				VertexViews[0].StrideInBytes = Mesh.VertexBuffer->Description.Stride;

				// Instance Buffer.
				VertexViews.push_back({});
				VertexViews[1].BufferLocation = InstanceBuffer.first->Resource->GetResource()->GetGPUVirtualAddress() + InstanceBuffer.second;
				VertexViews[1].SizeInBytes = InstanceBuffer.first->Description.Size;
				VertexViews[1].StrideInBytes = InstanceBuffer.first->Description.Stride;

				DrawList->IASetVertexBuffers(0, VertexViews.size(), VertexViews.data());  // #TODO: Slot?

				D3D12_INDEX_BUFFER_VIEW IndexView{};
				IndexView.BufferLocation = Mesh.IndexBuffer->Resource->GetResource()->GetGPUVirtualAddress();
				IndexView.SizeInBytes = Mesh.IndexBuffer->Description.Size;
				IndexView.Format = DXGI_FORMAT_R32_UINT;

				DrawList->IASetIndexBuffer(&IndexView);

				//DrawList->OMSetStencilRef();  // #TODO: Stencil ref.

				// #TODO: Bind the pipeline.

				DrawList->DrawIndexedInstanced(Mesh.IndexBuffer->Description.Size / sizeof(uint32_t), 1, 0, 0, 0);
			});
	}

	EndRenderPass(RenderPass::Main);

	// Sync the copy engine so we're sure that all the resources are ready on the GPU. In the future this can be split up into separate sync groups (pre, main, post, etc.) to reduce idle time.
	Device->Sync(SyncType::Copy);

	DrawList->Close();

	ID3D12CommandList* DirectLists[] = { DrawList };

	{
		VGScopedCPUStat("Execute Direct");

		Device->GetDirectQueue()->ExecuteCommandLists(1, DirectLists);
	}

	{
		VGScopedCPUStat("Present");

		Device->GetSwapChain()->Present(Device->VSync, 0);  // #TODO: This is probably presenting the wrong frame!
	}
}