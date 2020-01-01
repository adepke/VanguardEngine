// Copyright (c) 2019 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Device.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>

//#include <entt/entt.hpp>  // #TODO: Include from here instead of in the header.

#include <vector>
#include <utility>

// Per-entity data, passed through input assembler.
struct EntityInstance
{
	// #TODO: Create from some form of shader interop.

	TransformComponent Transform;
};

void Renderer::SetRenderTargets(ID3D12GraphicsCommandList* CommandList, size_t Frame)
{
	D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetHandle{};

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> RenderTargets;

	// #TEMP
	D3D12_CPU_DESCRIPTOR_HANDLE RtvHeapHandle;
	size_t RenderTargetHeapSize;

	// Final render target (bound to swap chain).
	RenderTargets.push_back({});
	RenderTargets[0] = { RtvHeapHandle.ptr + Frame * RenderTargetHeapSize };

	CommandList->OMSetRenderTargets(RenderTargets.size(), RenderTargets.data(), false, nullptr);

	constexpr float ClearColor[] = { 0.f, 0.f, 0.f, 1.f };
	CommandList->ClearRenderTargetView(RenderTargets[0], ClearColor, 0, nullptr);
}

void Renderer::Initialize(std::unique_ptr<RenderDevice>&& InDevice)
{
	VGScopedCPUStat("Renderer Initialize");

	Device = std::move(InDevice);

	Device->ReloadShaders();
}

void Renderer::Render(entt::registry& Registry)
{
	VGScopedCPUStat("Render");

	const auto FrameIndex = Device->Frame % RenderDevice::FrameCount;

	static_cast<ID3D12GraphicsCommandList*>(Device->CopyCommandList[FrameIndex].Get())->Close();
	
	ID3D12CommandList* CopyLists[] = { Device->CopyCommandList[FrameIndex].Get() };

	Device->CopyCommandQueue->ExecuteCommandLists(1, CopyLists);

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

	auto* DrawList = Device->DirectCommandList[FrameIndex].Get();

	SetRenderTargets(DrawList, FrameIndex);

	// Sync the copy engine so we're sure that all the resources are ready on the GPU. In the future this can be split up into separate sync groups (pre, main, post, etc.) to reduce idle time.
	Device->Sync(SyncType::Copy, Device->Frame);

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

				//DrawList->SetPipelineState();  // #TODO: Pipeline state.
				DrawList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				DrawList->DrawIndexedInstanced(Mesh.IndexBuffer->Description.Size / sizeof(uint32_t), 1, 0, 0, 0);
			});

		DrawList->Close();
	}

	ID3D12CommandList* DirectLists[] = { DrawList };

	Device->DirectCommandQueue->ExecuteCommandLists(1, DirectLists);
}