// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Core/Logging.h>
#include <Core/Windows/DirectX12Minimal.h>

#include <sstream>
#include <unordered_map>
#include <utility>

auto* DredBreadcrumbOpName(D3D12_AUTO_BREADCRUMB_OP op)
{
	switch (op)
	{
	case D3D12_AUTO_BREADCRUMB_OP_SETMARKER: return VGText("Set marker");
	case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT: return VGText("Begin event");
	case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT: return VGText("End event");
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED: return VGText("Draw instanced");
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED: return VGText("Draw indexed instanced");
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT: return VGText("Execute indirect");
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCH: return VGText("Dispatch");
	case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION: return VGText("Copy buffer region");
	case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION: return VGText("Copy texture region");
	case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE: return VGText("Copy resource");
	case D3D12_AUTO_BREADCRUMB_OP_COPYTILES: return VGText("Copy tiles");
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE: return VGText("Resolve subresource");
	case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW: return VGText("Clear render target view");
	case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW: return VGText("Clear unordered access view");
	case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW: return VGText("Clear depth stencil view");
	case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER: return VGText("Resource barrier");
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE: return VGText("Execute bundle");
	case D3D12_AUTO_BREADCRUMB_OP_PRESENT: return VGText("Present");
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA: return VGText("Resolve query data");
	case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION: return VGText("Begin submission");
	case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION: return VGText("End submission");
	case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME: return VGText("Decode frame");
	case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES: return VGText("Process frames");
	case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT: return VGText("Atomic copy buffer uint");
	case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64: return VGText("Atomic copy buffer uint64");
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION: return VGText("Resolve subresource region");
	case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE: return VGText("Write buffer immediate");
	case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1: return VGText("Decode frame 1");
	case D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION: return VGText("Set protected resource session");
	case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2: return VGText("Decode frame 2");
	case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1: return VGText("Process frames 1");
	case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE: return VGText("Build raytracing acceleration structure");
	case D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO: return VGText("Emit raytracing acceleration structure post build info");
	case D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE: return VGText("Copy raytracing acceleration structure");
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS: return VGText("Dispatch rays");
	case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND: return VGText("Initialize meta command");
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND: return VGText("Execute meta command");
	case D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION: return VGText("Estimate motion");
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP: return VGText("Resolve motion vector heap");
	case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1: return VGText("Set pipeline state 1");
	case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND: return VGText("Initialize extension command");
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND: return VGText("Execute extension command");
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH: return VGText("Dispatch mesh");
	default: return VGText("Unknown");
	}
}

auto* DredAllocationName(D3D12_DRED_ALLOCATION_TYPE type)
{
	switch (type)
	{
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE: return VGText("Command queue");
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR: return VGText("Command allocator");
	case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE: return VGText("Pipeline state");
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST: return VGText("Command list");
	case D3D12_DRED_ALLOCATION_TYPE_FENCE: return VGText("Fence");
	case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP: return VGText("Descriptor heap");
	case D3D12_DRED_ALLOCATION_TYPE_HEAP: return VGText("Heap");
	case D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP: return VGText("Query heap");
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE: return VGText("Command signature");
	case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY: return VGText("Pipeline library");
	case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER: return VGText("Video decoder");
	case D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR: return VGText("Video processor");
	case D3D12_DRED_ALLOCATION_TYPE_RESOURCE: return VGText("Resource");
	case D3D12_DRED_ALLOCATION_TYPE_PASS: return VGText("Pass");
	case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION: return VGText("Crypto session");
	case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY: return VGText("Crypto session policy");
	case D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION: return VGText("Protected resource session");
	case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP: return VGText("Video decoder heap");
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL: return VGText("Command pool");
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER: return VGText("Command recorder");
	case D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT: return VGText("State object");
	case D3D12_DRED_ALLOCATION_TYPE_METACOMMAND: return VGText("Meta command");
	case D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP: return VGText("Scheduling group");
	case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR: return VGText("Video motion estimator");
	case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP: return VGText("Video motion vector heap");
	case D3D12_DRED_ALLOCATION_TYPE_INVALID: return VGText("Invalid");
	default: return VGText("Unknown");
	}
}

void LogDredInfo(ID3D12Device5* device, ID3D12DeviceRemovedExtendedData1* dred)
{
	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs;
	D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault;

	std::vector<std::pair<int, const D3D12_AUTO_BREADCRUMB_NODE1*>> badNodes;

	auto result = dred->GetAutoBreadcrumbsOutput1(&breadcrumbs);
	if (FAILED(result))
	{
		VGLogWarning(logRendering, "Failed to get DRED breadcrumbs: {}", result);
	}

	else
	{
		auto* node = breadcrumbs.pHeadAutoBreadcrumbNode;
		if (node)
		{
			VGLog(logRendering, "DRED breadcrumb node(s):");
		}

		else
		{
			VGLog(logRendering, "No DRED breadcrumbs available.");
		}

		int nodeId = 0;
		while (node)
		{
			auto* commandList = node->pCommandListDebugNameW;
			auto* commandQueue = node->pCommandQueueDebugNameW;
			auto count = node->BreadcrumbCount;
			auto countCompleted = *node->pLastBreadcrumbValue;

			if (countCompleted > 0 && countCompleted < count)
			{
				badNodes.emplace_back(nodeId, node);
			}

			VGLog(logRendering, "\tNode {} from list \"{}\", queue \"{}\" executed {}/{} breadcrumbs", nodeId, commandList ? commandList : VGText("Unknown"), commandQueue ? commandQueue : VGText("Unknown"), countCompleted, count);
			
			std::unordered_map<uint32_t, const wchar_t*> contextMap;
			contextMap.reserve(node->BreadcrumbContextsCount);

			for (int i = 0; i < node->BreadcrumbContextsCount; ++i)
			{
				contextMap[node->pBreadcrumbContexts[i].BreadcrumbIndex] = node->pBreadcrumbContexts[i].pContextString;
			}

			int stack = 0;
			for (int i = 0; i < count; ++i)
			{
				auto command = node->pCommandHistory[i];

				std::wstringstream nodeStream;
				nodeStream << VGText("\t\t[") << i << VGText("]: ");

				if (i < 10)
					nodeStream << VGText("  ");
				else if (i < 100)
					nodeStream << VGText(" ");

				if (command == D3D12_AUTO_BREADCRUMB_OP_ENDEVENT)
					--stack;

				for (int j = 0; j < stack; ++j)
				{
					nodeStream << "  ";
				}

				nodeStream << DredBreadcrumbOpName(command);
				if (contextMap.contains(i))
				{
					nodeStream << VGText(": \"") << contextMap[i] << VGText("\"");
				}

				if (command == D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT)
					++stack;

				VGLog(logRendering, "{}", nodeStream.str());
			}

			++nodeId;
			node = node->pNext;
		}
	}

	VGLog(logRendering, "");

	result = dred->GetPageFaultAllocationOutput1(&pageFault);
	if (FAILED(result))
	{
		VGLogWarning(logRendering, "Failed to get DRED page fault: {}", result);
	}

	else
	{
		VGLog(logRendering, "GPU page fault virtual address: {:#x}", pageFault.PageFaultVA);

		const auto printAllocation = [](auto* node, auto id)
		{
			std::wstringstream nodeStream;

			if (id < 10)
				nodeStream << VGText("  ");
			else if (id < 100)
				nodeStream << VGText(" ");

			nodeStream << VGText("\t[") << id << VGText("]: \"") <<
				(node->ObjectNameW ? node->ObjectNameW : VGText("Unnamed")) <<
				VGText("\": (") << DredAllocationName(node->AllocationType) <<
				VGText(") ptr: 0x") << std::hex << node->pObject << VGText("\n");

			VGLog(logRendering, "{}", nodeStream.str());
		};

		VGLog(logRendering, "Relevant existing runtime objects:");
		auto* node = pageFault.pHeadExistingAllocationNode;
		if (!node)
		{
			VGLog(logRendering, "No DRED page fault existing objects available.");
		}

		int nodeId = 0;
		while (node)
		{
			printAllocation(node, nodeId);
			++nodeId;
			node = node->pNext;
		}

		VGLog(logRendering, "");
		VGLog(logRendering, "Relevant recently freed runtime objects:");
		node = pageFault.pHeadRecentFreedAllocationNode;
		if (!node)
		{
			VGLog(logRendering, "No DRED page fault recently freed objects available.");
		}

		nodeId = 0;
		while (node)
		{
			printAllocation(node, nodeId);
			++nodeId;
			node = node->pNext;
		}
	}

	VGLog(logRendering, "");
	VGLog(logRendering, "======== DRED SUMMARY ========");
	if (badNodes.size() > 0)
	{
		int i = 0;
		VGLog(logRendering, "Potential culprits:");
		for (const auto [nodeId, node] : badNodes)
		{
			auto* commandList = node->pCommandListDebugNameW;
			auto* commandQueue = node->pCommandQueueDebugNameW;
			auto count = node->BreadcrumbCount;
			auto countCompleted = *node->pLastBreadcrumbValue;

			VGLog(logRendering, "\t[{}] Node {} from list \"{}\", queue \"{}\" did not finish the following execution (completed {}/{}):", i, nodeId, commandList ? commandList : VGText("Unknown"), commandQueue ? commandQueue : VGText("Unknown"), countCompleted, count);
			
			std::unordered_map<uint32_t, const wchar_t*> contextMap;
			contextMap.reserve(node->BreadcrumbContextsCount);

			for (int i = 0; i < node->BreadcrumbContextsCount; ++i)
			{
				contextMap[node->pBreadcrumbContexts[i].BreadcrumbIndex] = node->pBreadcrumbContexts[i].pContextString;
			}

			const auto badCommand = countCompleted;  // 0-indexed, so no +1.

			std::wstringstream nodeStream;
			nodeStream << DredBreadcrumbOpName(node->pCommandHistory[badCommand]);
			if (contextMap.contains(badCommand))
			{
				nodeStream << VGText(": \"") << contextMap[badCommand] << VGText("\"");
			}

			VGLog(logRendering, "\t\t  {}", nodeStream.str());
			++i;
		}
	}
	
	else
	{
		VGLog(logRendering, "Found no potential culprits. All nodes either ran to completion or did not run at all.");
	}
}