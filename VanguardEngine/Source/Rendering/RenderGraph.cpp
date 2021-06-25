// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/Device.h>

#include <algorithm>

void RenderGraph::BuildAdjacencyList()
{
	for (int i = 0; i < passes.size(); ++i)
	{
		const auto& outer = passes[i];

		for (int j = 0; j < passes.size(); ++j)
		{
			if (i == j)
				continue;

			const auto& inner = passes[j];

			std::vector<RenderResource> intersection;
			// #TODO: We can stop as soon as there's a single intersection, std::set_intersection will find all intersections.
			std::set_intersection(outer->writes.cbegin(), outer->writes.cend(), inner->reads.cbegin(), inner->reads.cend(), std::back_inserter(intersection));
			
			// If there's a write-to-read dependency, create an edge.
			if (intersection.size() > 0)
			{
				adjacencyList[i].emplace_back(j);
			}
		}
	}
}

void RenderGraph::DepthFirstSearch(size_t node, std::vector<bool>& visited, std::stack<bool>& stack)
{
	if (visited[node])
		return;

	visited[node] = true;

	for (const auto adjacentNode : adjacencyList[node])
	{
		DepthFirstSearch(adjacentNode, visited, stack);
	}

	stack.push(node);
}

void RenderGraph::TopologicalSort()
{
	sorted.reserve(passes.size());  // Most passes are unlikely to be trimmed.

	std::vector<bool> visited(passes.size(), false);
	std::stack<bool> stack;

	for (int i = 0; i < passes.size(); ++i)
	{
		if (!visited[i])
		{
			DepthFirstSearch(i, visited, stack);
		}
	}

	while (stack.size() > 0)
	{
		sorted.push_back(stack.top());
		stack.pop();
	}
}

void RenderGraph::BuildDepthMap()
{
	for (const auto pass : sorted)
	{
		for (const auto adjacentPass : adjacencyList[pass])
		{
			if (depthMap[adjacentPass] < depthMap[pass] + 1)
			{
				depthMap[adjacentPass] = depthMap[pass] + 1;
			}
		}
	}
}

void RenderGraph::InjectStateBarriers(RenderDevice* device)
{
	for (int i = 0; i < passes.size(); ++i)
	{
		auto& pass = passes[i];
		auto& list = passLists[i];

		for (const auto resource : pass->reads)
		{
			auto buffer = resourceManager->GetOptionalBuffer(resource);
			auto texture = resourceManager->GetOptionalTexture(resource);

			const auto Transition = [&](D3D12_RESOURCE_STATES state)
			{
				if (buffer)
				{
					list->TransitionBarrier(*buffer, state);
				}

				else if (texture)
				{
					list->TransitionBarrier(*texture, state);
				}
			};

			switch (pass->bindInfo[resource])
			{
			case ResourceBind::CBV: Transition(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER); break;
			case ResourceBind::SRV: Transition(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); break;
			case ResourceBind::UAV: Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS); break;
			case ResourceBind::DSV: Transition(D3D12_RESOURCE_STATE_DEPTH_READ); break;
			}
		}

		for (const auto resource : pass->writes)
		{
			auto buffer = resourceManager->GetOptionalBuffer(resource);
			auto texture = resourceManager->GetOptionalTexture(resource);

			const auto Transition = [&](D3D12_RESOURCE_STATES state)
			{
				if (buffer)
				{
					list->TransitionBarrier(*buffer, state);

					if (state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					{
						// If we have a counter buffer, we need to make sure it's in the proper state.
						auto& bufferComponent = device->GetResourceManager().Get(*buffer);
						if (bufferComponent.description.uavCounter)
						{
							list->TransitionBarrier(bufferComponent.counterBuffer, state);
						}
					}
				}

				else if (texture)
				{
					list->TransitionBarrier(*texture, state);
				}
			};

			if (pass->outputBindInfo.contains(resource))
			{
				switch (pass->outputBindInfo[resource].first)
				{
				case OutputBind::RTV: Transition(D3D12_RESOURCE_STATE_RENDER_TARGET); break;
				case OutputBind::DSV: Transition(D3D12_RESOURCE_STATE_DEPTH_WRITE); break;
				}
			}

			else
			{
				switch (pass->bindInfo[resource])
				{
				case ResourceBind::UAV: Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS); break;
				}
			}
		}
	}

	for (auto& list : passLists)
	{
		list->FlushBarriers();
	}
}

std::pair<uint32_t, uint32_t> RenderGraph::GetBackBufferResolution(RenderDevice* device)
{
	VGAssert(taggedResources.contains(ResourceTag::BackBuffer), "Render graph doesn't have tagged back buffer resource.");

	const auto backBuffer = resourceManager->GetTexture(taggedResources[ResourceTag::BackBuffer]);
	const auto& buffer = device->GetResourceManager().Get(backBuffer);
	return std::make_pair(buffer.description.width, buffer.description.height);
}

RenderPass& RenderGraph::AddPass(std::string_view stableName, ExecutionQueue execution)
{
	return *passes.emplace_back(std::make_unique<RenderPass>(resourceManager, stableName));
}

void RenderGraph::Build()
{
	for (const auto& pass : passes)
	{
		pass->Validate();
	}

	BuildAdjacencyList();
	TopologicalSort();
}

void RenderGraph::Execute(RenderDevice* device)
{
	resourceManager->BuildTransients(device, this);

	passLists.reserve(passes.size());

	for (int i = 0; i < passes.size(); ++i)
	{
		passLists.emplace_back(std::move(device->AllocateFrameCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT)));
	}

	InjectStateBarriers(device);

	for (int i = 0; i < passLists.size(); ++i)
	{
		auto& pass = passes[i];
		auto& list = passLists[i];

		VGScopedCPUTransientStat(pass->stableName.data());
		VGScopedGPUTransientStat(pass->stableName.data(), device->GetDirectContext(), list->Native());

		// If graphics pass...

		list->BindDescriptorAllocator(device->GetDescriptorAllocator());

		D3D12_VIEWPORT viewport{
			.TopLeftX = 0.f,
			.TopLeftY = 0.f,
			.Width = static_cast<float>(device->renderWidth),
			.Height = static_cast<float>(device->renderHeight),
			.MinDepth = 0.f,
			.MaxDepth = 1.f
		};

		list->Native()->RSSetViewports(1, &viewport);

		D3D12_RECT scissor{
			.left = 0,
			.top = 0,
			.right = static_cast<LONG>(device->renderWidth),
			.bottom = static_cast<LONG>(device->renderHeight)
		};

		list->Native()->RSSetScissorRects(1, &scissor);

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargets;
		renderTargets.reserve(pass->outputBindInfo.size());
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencil;
		bool hasDepthStencil = false;

		for (const auto& [resource, info] : pass->outputBindInfo)
		{
			const auto texture = resourceManager->GetTexture(resource);
			auto& component = device->GetResourceManager().Get(texture);

			if (info.first == OutputBind::RTV)
			{
				renderTargets.emplace_back(*component.RTV);
			}

			else if (info.first == OutputBind::DSV)
			{
				hasDepthStencil = true;
				depthStencil = *component.DSV;
			}
		}

		// If we don't have a depth stencil output, we might still have one as an input.
		if (!hasDepthStencil)
		{
			for (const auto [resource, bind] : pass->bindInfo)
			{
				if (bind == ResourceBind::DSV)
				{
					const auto texture = resourceManager->GetTexture(resource);
					auto& component = device->GetResourceManager().Get(texture);

					hasDepthStencil = true;
					depthStencil = *component.DSV;

					break;
				}
			}
		}

		// #TODO: Replace with render passes.
		list->Native()->OMSetRenderTargets(renderTargets.size(), renderTargets.size() > 0 ? renderTargets.data() : nullptr, false, hasDepthStencil ? &depthStencil : nullptr);

		// #TODO: This should be the same as the color given during resource creation. Only store this value in one place.
		const float ClearColor[] = { 0.f, 0.f, 0.f, 1.f };

		for (const auto& [resource, info] : pass->outputBindInfo)
		{
			if (info.second)
			{
				const auto texture = resourceManager->GetTexture(resource);
				auto& component = device->GetResourceManager().Get(texture);

				if (info.first == OutputBind::RTV)
				{
					list->Native()->ClearRenderTargetView(*component.RTV, ClearColor, 0, nullptr);
				}

				else if (info.first == OutputBind::DSV)
				{
					// #TODO: Stencil clearing.
					// #TODO: Retrieve clear color from the resource description.
					list->Native()->ClearDepthStencilView(*component.DSV, D3D12_CLEAR_FLAG_DEPTH/* | D3D12_CLEAR_FLAG_STENCIL*/, 0.f, 0, 0, nullptr);  // Inverse Z.
				}
			}
		}

		list->Native()->OMSetStencilRef(0);

		pass->Execute(*list, *resourceManager);

		// #TODO: End render pass.
	}

	// Present the back buffer. #TODO: Implement as a present pass.
	const auto backBuffer = resourceManager->GetTexture(taggedResources[ResourceTag::BackBuffer]);
	passLists[passLists.size() - 1]->TransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);

	// Close and submit the command lists.

	std::vector<ID3D12CommandList*> commandLists;
	commandLists.reserve(passLists.size() + 1);

	device->GetDirectList().FlushBarriers();
	device->GetDirectList().Close();
	commandLists.emplace_back(device->GetDirectList().Native());

	for (auto& list : passLists)
	{
		list->FlushBarriers();
		list->Close();
		commandLists.emplace_back(list->Native());
	}

	device->GetDirectQueue()->ExecuteCommandLists(commandLists.size(), commandLists.data());
}