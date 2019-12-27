// Copyright (c) 2019 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Device.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>

//#include <entt/entt.hpp>

#include <vector>

void Renderer::Initialize(std::unique_ptr<RenderDevice>&& InDevice)
{
	VGScopedCPUStat("Renderer Initialize");

	Device = std::move(InDevice);
}

void Renderer::Render(entt::registry& Registry)
{
	VGScopedCPUStat("Render");

	ID3D12CommandList* CopyLists[] = { Device->CopyCommandList.Get() };

	Device->CopyCommandQueue->ExecuteCommandLists(1, CopyLists);

	// #TODO: Culling.

	// #TODO: Sort by material.

	// Number of entities with MeshComponent and TransformComponent
	const auto RenderCount = 0;

	std::shared_ptr<GPUBuffer> InstanceBuffer;

	{
		VGScopedCPUStat("Generate Instance Buffer");

		// #TODO: Allocate GPU buffer for instance data (transform).
		ResourceDescription Description;
		//Description.Size = RenderCount * sizeof(?);
		//Description.Stride = sizeof(?);
		Description.UpdateRate = ResourceFrequency::Dynamic;  // Don't bother sending to the default heap.
		Description.BindFlags = BindFlag::ConstantBuffer;  // #TODO: Correct?
		Description.AccessFlags = AccessFlag::CPUWrite;

		InstanceBuffer = std::move(Device->Allocate(Description, VGText("Frame Instance Buffer")));

		Registry.view<const TransformComponent, const MeshComponent>().each([](auto Entity, const auto& Transform, const auto&)
			{
				// #TODO: Write transform to GPU buffer. View requires MeshComponent to exclude non-rendered entities.
			});
	}

	auto* CommandList = Device->DirectCommandList.Get();

	Registry.view<const TransformComponent, const MeshComponent>().each([&InstanceBuffer, CommandList](auto Entity, const auto&, const auto& Mesh)
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

			CommandList->IASetVertexBuffers(0, VertexViews.size(), VertexViews.data());  // #TODO: Slot?

			D3D12_INDEX_BUFFER_VIEW IndexView{};
			IndexView.BufferLocation = Mesh.IndexBuffer->Resource->GetResource()->GetGPUVirtualAddress();
			IndexView.SizeInBytes = Mesh.IndexBuffer->Description.Size;
			IndexView.Format = DXGI_FORMAT_R32_UINT;

			CommandList->IASetIndexBuffer(&IndexView);

			//CommandList->OMSetStencilRef();  // #TODO: Stencil ref.

			//CommandList->SetPipelineState();  // #TODO: Pipeline state.
			//CommandList->IASetPrimitiveTopology();  // #TODO: Primitive topology.

			//CommandList->DrawIndexedInstanced();  // #TODO: Draw.
		});
}