// Copyright (c) 2019-2021 Andrew Depke

#include <Editor/EditorUI.h>
#include <Rendering/Device.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Editor/EntityReflection.h>
#include <Editor/ImGuiExtensions.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <numeric>

void EditorUI::DrawMenu()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Entity Hierarchy", nullptr, &entityHierarchyOpen);
			ImGui::MenuItem("Entity Properties", nullptr, &entityPropertyViewerOpen);
			ImGui::MenuItem("Performance Metrics", nullptr, &performanceMetricsOpen);
			ImGui::MenuItem("Render Graph", nullptr, &renderGraphOpen);

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void EditorUI::DrawFrameTimeHistory()
{
	if (ImGui::Begin("Performance Metrics", &performanceMetricsOpen))
	{
		// Compute statistics.
		const auto [min, max] = std::minmax_element(frameTimes.begin(), frameTimes.end());
		const auto mean = std::accumulate(frameTimes.begin(), frameTimes.end(), 0) / static_cast<float>(frameTimes.size());

		auto* window = ImGui::GetCurrentWindow();
		auto& style = ImGui::GetStyle();

		const auto frameWidth = ImGui::GetContentRegionAvailWidth() - window->WindowPadding.x - ImGui::CalcTextSize("Mean: 00.000").x;
		const auto frameHeight = (ImGui::GetTextLineHeight() + style.ItemSpacing.y) * 3.f + 10.f;  // Max, mean, min.

		const ImRect frameBoundingBox = { window->DC.CursorPos, window->DC.CursorPos + ImVec2{ frameWidth, frameHeight } };

		ImGui::ItemSize(frameBoundingBox, style.FramePadding.y);
		if (!ImGui::ItemAdd(frameBoundingBox, 0))  // Don't support navigation to the frame.
		{
			ImGui::End();
			return;
		}

		ImGui::RenderFrame(frameBoundingBox.Min, frameBoundingBox.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

		// Internal region for rendering the plot lines.
		const ImRect frameRenderSpace = { frameBoundingBox.Min + style.FramePadding, frameBoundingBox.Max - style.FramePadding };

		// Adaptively update the sample count.
		frameTimeHistoryCount = frameRenderSpace.GetWidth() / 2.f;

		if (frameTimes.size() > 1)
		{
			const ImVec2 lineSize = { frameRenderSpace.GetWidth() / (frameTimes.size() - 1), frameRenderSpace.GetHeight() / (*max - *min) };
			const auto lineColor = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_PlotLines]);

			for (int i = 0; i < frameTimes.size() - 1; ++i)  // Don't draw the final point.
			{
				window->DrawList->AddLine(
					{ frameRenderSpace.Min.x + (lineSize.x * i), frameRenderSpace.Min.y + frameRenderSpace.GetHeight() - (lineSize.y * (frameTimes[i] - *min)) },
					{ frameRenderSpace.Min.x + (lineSize.x * (i + 1)), frameRenderSpace.Min.y + frameRenderSpace.GetHeight() - (lineSize.y * (frameTimes[i + 1] - *min)) },
					lineColor);
			}
		}

		if (min != frameTimes.end() && max != frameTimes.end())
		{
			ImGui::SameLine();
			ImGui::BeginGroup();

			ImGui::Text("Max:  %.3f", *max / 1000.f);
			ImGui::Text("Mean: %.3f", mean / 1000.f);
			ImGui::Text("Min:  %.3f", *min / 1000.f);

			ImGui::EndGroup();
		}
	}

	ImGui::End();
}

void EditorUI::DrawLayout()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });

	auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetWorkPos());
	ImGui::SetNextWindowSize(viewport->GetWorkSize());
	ImGui::SetNextWindowViewport(viewport->ID);

	// Always draw the dock space.
	ImGui::Begin("Dock Space", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking);

	const auto dockSpaceId = ImGui::GetID("DockSpace");
	ImGui::DockSpace(dockSpaceId, { 0.f, 0.f });

	// Draw the menu in the dock space window.
	DrawMenu();

	ImGui::End();

	ImGui::PopStyleVar(3);
}

void EditorUI::DrawDemoWindow()
{
	static bool demoWindowOpen = true;

	ImGui::ShowDemoWindow(&demoWindowOpen);
}

void EditorUI::DrawScene(RenderDevice* device, TextureHandle sceneTexture)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });  // Remove window padding.

	if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::Image(device, sceneTexture);
	}

	ImGui::End();

	ImGui::PopStyleVar();
}

void EditorUI::DrawEntityHierarchy(entt::registry& registry)
{
	if (entityHierarchyOpen)
	{
		entt::entity selectedEntity = entt::null;

		if (ImGui::Begin("Entity Hierarchy", &entityHierarchyOpen))
		{
			ImGui::Text("%i Entities", registry.size());
			ImGui::Separator();

			registry.each([this, &registry, &selectedEntity](auto entity)
			{
				ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_None;

				if (entity == hierarchySelectedEntity)
					nodeFlags |= ImGuiTreeNodeFlags_Selected;

				bool nodeOpen = false;

				ImGui::PushID(static_cast<int32_t>(entity));  // Use the entity as the ID.

				if (registry.has<NameComponent>(entity))
				{
					nodeOpen = ImGui::TreeNodeEx("EntityTreeNode", nodeFlags, registry.get<NameComponent>(entity).name.c_str());
				}

				else
				{
					// Strip the version info from the entity, we only care about the actual ID.
					nodeOpen = ImGui::TreeNodeEx("EntityTreeNode", nodeFlags, "Entity_%i", registry.entity(entity));
				}

				if (ImGui::IsItemClicked())
				{
					selectedEntity = entity;
				}

				if (nodeOpen)
				{
					// #TODO: Draw entity children.

					ImGui::TreePop();
				}

				ImGui::PopID();
			});
		}

		ImGui::End();

		// Check if it's valid first, otherwise deselecting will remove the property viewer.
		if (registry.valid(selectedEntity))
		{
			hierarchySelectedEntity = selectedEntity;
		}
	}
}

void EditorUI::DrawEntityPropertyViewer(entt::registry& registry)
{
	if (entityPropertyViewerOpen)
	{
		if (ImGui::Begin("Property Viewer", &entityPropertyViewerOpen))
		{
			if (registry.valid(hierarchySelectedEntity))
			{
				uint32_t componentCount = 0;

				for (auto& [metaID, renderFunction] : EntityReflection::componentMap)
				{
					entt::id_type metaList[] = { metaID };

					if (registry.runtime_view(std::cbegin(metaList), std::cend(metaList)).contains(hierarchySelectedEntity))
					{
						++componentCount;

						renderFunction(registry, hierarchySelectedEntity);

						ImGui::Separator();
					}
				}

				if (componentCount == 0)
				{
					ImGui::Text("No components.");
				}
			}
		}

		ImGui::End();
	}
}

void EditorUI::DrawPerformanceMetrics(float frameTimeMs)
{
	frameTimes.push_back(frameTimeMs);

	while (frameTimes.size() > frameTimeHistoryCount)
	{
		frameTimes.pop_front();
	}

	if (performanceMetricsOpen)
	{
		DrawFrameTimeHistory();
	}
}

void EditorUI::DrawRenderGraph(RenderDevice* device, TextureHandle depthStencil, TextureHandle scene)
{
	if (renderGraphOpen)
	{
		if (ImGui::Begin("Render Graph", &renderGraphOpen))
		{
			ImGui::Image(device, depthStencil, 0.25f);
			ImGui::Image(device, scene, 0.25f);
		}

		ImGui::End();
	}
}