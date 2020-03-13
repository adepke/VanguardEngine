// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Device.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderGraph.h>

//#include <entt/entt.hpp>  // #TODO: Include from here instead of in the header.

#include <vector>
#include <utility>

// Per-entity data, passed through input assembler.
struct EntityInstance
{
	// #TODO: Create from some form of shader interop.

	TransformComponent Transform;
};

void Renderer::SetDescriptorHeaps(CommandList& List)
{
	VGScopedCPUStat("Set Descriptor Heaps");

	std::vector<ID3D12DescriptorHeap*> Heaps;

	Heaps.push_back(Device->GetResourceHeap().Native());
	Heaps.push_back(Device->GetSamplerHeap().Native());

	List.Native()->SetDescriptorHeaps(static_cast<UINT>(Heaps.size()), Heaps.data());
}

void Renderer::Initialize(std::unique_ptr<RenderDevice>&& InDevice)
{
	VGScopedCPUStat("Renderer Initialize");

	Device = std::move(InDevice);

	Device->CheckFeatureSupport();
	Materials = std::move(Device->ReloadMaterials());
}

void Renderer::Render(entt::registry& Registry)
{
	VGScopedCPUStat("Render");

	Device->GetCopyList().Close();
	
	ID3D12CommandList* CopyLists[] = { Device->GetCopyList().Native() };

	{
		VGScopedCPUStat("Execute Copy");

		Device->GetCopyQueue()->ExecuteCommandLists(1, CopyLists);
	}

	// #TODO: Culling.

	// #TODO: Sort by material.
	
	// Number of entities with MeshComponent and TransformComponent
	const auto RenderCount = 10;

	std::pair<std::shared_ptr<Buffer>, size_t> InstanceBuffer;

	{
		VGScopedCPUStat("Generate Instance Buffer");

		InstanceBuffer = std::move(Device->FrameAllocate(sizeof(EntityInstance) * RenderCount));

		size_t Index = 0;
		Registry.view<const TransformComponent, const MeshComponent>().each([this, &InstanceBuffer, &Index](auto Entity, const auto& Transform, const auto&)
			{
				EntityInstance Instance{ Transform };

				std::vector<uint8_t> InstanceData{};
				InstanceData.resize(sizeof(EntityInstance));
				std::memcpy(InstanceData.data(), &Instance, InstanceData.size());

				Device->WriteResource(InstanceBuffer.first, InstanceData, InstanceBuffer.second + (Index * sizeof(EntityInstance)));

				++Index;
			});
	}

	// Global to all render passes.
	SetDescriptorHeaps(Device->GetDirectList());

	// #TEMP: Testing render graph API.
	std::shared_ptr<Texture> DepthStencil;

	RenderGraph Graph{ Device.get() };

	const size_t BackBufferTag = Graph.ImportResource(Device->GetBackBuffer());
	const size_t DepthStencilTag = Graph.ImportResource(DepthStencil);

	auto& MainPass = Graph.AddPass(VGText("Main Pass"));
	MainPass.ReadResource(DepthStencilTag, RGUsage::DepthStencil);
	MainPass.WriteResource(BackBufferTag, RGUsage::SwapChain);

	MainPass.Bind(
		[BackBufferTag, DepthStencilTag](RGResolver& Resolver, CommandList& List)
		{
			auto& BackBuffer = Resolver.Get<Texture>(BackBufferTag);
			auto& DepthStencil = Resolver.Get<Texture>(DepthStencilTag);

			// #TODO: Add some draw commands to the list.
		}
	);

	Graph.Build();

	// Sync the copy engine so we're sure that all the resources are ready on the GPU. In the future this can be split up into separate sync groups (pre, main, post, etc.) to reduce idle time.
	Device->Sync(SyncType::Copy);

	Graph.Execute();

	{
		VGScopedCPUStat("Present");

		Device->GetSwapChain()->Present(Device->VSync, 0);  // #TODO: This is probably presenting the wrong frame!
	}
}