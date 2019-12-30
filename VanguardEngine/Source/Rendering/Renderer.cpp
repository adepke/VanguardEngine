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

void Renderer::Initialize(std::unique_ptr<RenderDevice>&& InDevice)
{
	VGScopedCPUStat("Renderer Initialize");

	Device = std::move(InDevice);
}

void Renderer::Render(entt::registry& Registry)
{
	VGScopedCPUStat("Render");

	static_cast<ID3D12GraphicsCommandList*>(Device->CopyCommandList[Device->Frame % RenderDevice::FrameCount].Get())->Close();
	
	ID3D12CommandList* CopyLists[] = { Device->CopyCommandList[Device->Frame % RenderDevice::FrameCount].Get() };

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

	// Sync the copy engine so we're sure that all the resources are ready on the GPU. In the future this can be split up into separate sync groups (pre, main, post, etc.) to reduce idle time.
	Device->Sync(SyncType::Copy, Device->Frame);
	
	auto* DrawList = Device->DirectCommandList[Device->Frame % RenderDevice::FrameCount].Get();
	/*
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
			VertexViews[1].BufferLocation = InstanceBuffer->Resource->GetResource()->GetGPUVirtualAddress();
			VertexViews[1].SizeInBytes = InstanceBuffer->Description.Size;
			VertexViews[1].StrideInBytes = InstanceBuffer->Description.Stride;

			DrawList->IASetVertexBuffers(0, VertexViews.size(), VertexViews.data());  // #TODO: Slot?

			D3D12_INDEX_BUFFER_VIEW IndexView{};
			IndexView.BufferLocation = Mesh.IndexBuffer->Resource->GetResource()->GetGPUVirtualAddress();
			IndexView.SizeInBytes = Mesh.IndexBuffer->Description.Size;
			IndexView.Format = DXGI_FORMAT_R32_UINT;

			DrawList->IASetIndexBuffer(&IndexView);

			//DrawList->OMSetStencilRef();  // #TODO: Stencil ref.

			//DrawList->SetPipelineState();  // #TODO: Pipeline state.
			//DrawList->IASetPrimitiveTopology();  // #TODO: Primitive topology.

			//DrawList->DrawIndexedInstanced();  // #TODO: Draw.
		});
	*/

	DrawList->Close();

	ID3D12CommandList* DirectLists[] = { DrawList };

	Device->DirectCommandQueue->ExecuteCommandLists(1, DirectLists);
}